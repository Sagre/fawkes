
/***************************************************************************
 *  clips_diagnosis_env.cpp -  CLIPS active_diagnosis
 *
 *  Created: Tue May 19 12:00:06 2019
 *  Copyright  2019 Daniel Habering
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include "clips_diagnosis_env.h"

#include <utils/misc/string_conversions.h>
#include <utils/misc/string_split.h>
#include <utils/misc/map_skill.h>
#include <interfaces/SwitchInterface.h>
#include <core/threading/mutex_locker.h>

using namespace fawkes;
/** @class ClipsDiagnosisEnvThread "clips_diagnosis_env.h"
 * Thread maintaining the diagnosis environment
 *
 * @author Daniel Habering
 */

/** Constructor. */
ClipsDiagnosisEnvThread::ClipsDiagnosisEnvThread(std::string diag_id)
	: Thread("ClipsDiagnosisEnvThread", Thread::OPMODE_CONTINUOUS),
	  CLIPSAspect(std::string(diag_id).c_str(), std::string("CLIPS (" + diag_id + ")").c_str())
{
    diag_id_ = diag_id;
}

/** Destructor. */
ClipsDiagnosisEnvThread::~ClipsDiagnosisEnvThread()
{

}

void
ClipsDiagnosisEnvThread::init()
{
	std::vector<std::string> clips_dirs;

	try {
		clips_dirs = config->get_strings("/clips-executive/clips-dirs");
		for (size_t i = 0; i < clips_dirs.size(); ++i) {
			if (clips_dirs[i][clips_dirs[i].size()-1] != '/') {
				clips_dirs[i] += "/";
			}
			logger->log_debug(name(), "DIR: %s", clips_dirs[i].c_str());
		}
	} catch (Exception &e) {} // ignore, use default
	clips_dirs.insert(clips_dirs.begin(), std::string(SRCDIR) + "/clips/");

	MutexLocker lock(clips.objmutex_ptr());

	clips->evaluate(std::string("(path-add-subst \"@BASEDIR@\" \"") + BASEDIR + "\")");
	clips->evaluate(std::string("(path-add-subst \"@FAWKES_BASEDIR@\" \"") +
	                FAWKES_BASEDIR + "\")");
	clips->evaluate(std::string("(path-add-subst \"@RESDIR@\" \"") + RESDIR + "\")");
	clips->evaluate(std::string("(path-add-subst \"@CONFDIR@\" \"") + CONFDIR + "\")");

	clips->evaluate("(ff-feature-request \"config\")");

	for (size_t i = 0; i < clips_dirs.size(); ++i) {
		clips->evaluate("(path-add \"" + clips_dirs[i] + "\")");
	}

  std::vector<std::string> files{SRCDIR "/clips/saliences.clp", SRCDIR "/clips/init.clp"};
	for (const auto f : files) {
		if (!clips->batch_evaluate(f)) {
		  logger->log_error(name(), "Failed to initialize CLIPS environment, "
			                  "batch file '%s' failed.", f.c_str());
			throw Exception("Failed to initialize CLIPS environment, batch file '%s' failed.",
			                f.c_str());
	  }
  }
	clips->use_fact_duplication(false);
	clips->assert_fact("(active-diagnosis-init)");
	clips->refresh_agenda();
	clips->run();
	// Verify that initialization did not fail (yet)
	{
		CLIPS::Fact::pointer fact = clips->get_facts();
		while (fact) {
			CLIPS::Template::pointer tmpl = fact->get_template();
			if (tmpl->name() == "active-diagnosis-init-stage") {
				CLIPS::Values v = fact->slot_value("");
				if (v.size() > 0 && v[0].as_string() == "FAILED") {
					throw Exception("CLIPS Active Diagnosis initialization failed");
				}
			}

			fact = fact->next();
		}
	}
	clips->refresh_agenda();
	clips->run();
}

/**
 * @brief Add a plan-action to the diagnosis environment. The plan-action template differs from
 * the plan-action from the clips environment, since there is no goal/plan lifecycle, but
 * plan-actions belong to a diagnosis-hypothesis
 * 
 * @param pa_fact Pointer to the clips-executive style plan-action fact
 * @param hypo_id Id of the diagnosis candidate the plan-action belongs to
 */
void
ClipsDiagnosisEnvThread::add_plan_action(CLIPS::Fact::pointer pa_fact,std::string hypo_id)
{
	MutexLocker lock(clips.objmutex_ptr());

	CLIPS::Template::pointer plan_action = clips->get_template("plan-action");
	if (plan_action) {
		CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,plan_action);
		// Copy all slots over to the new plan-action fact
		for (std::string slot : tmp->slot_names()) {
			if (pa_fact->slot_value(slot).empty()) {
				// Fill the diag-id slot with the hypothesis id
				if (slot == "diag-id") {
					tmp->set_slot(slot,CLIPS::Value(hypo_id,CLIPS::TYPE_SYMBOL));
					continue;
				}
				if (plan_action->slot_default_type(slot) == CLIPS::DefaultType::NO_DEFAULT) {
					logger->log_error(name(),"Slot %s of plan action is missing a value. This will lead to undefined behaviour of this environment",slot.c_str());
					return;
				}
				if (plan_action->is_multifield_slot(slot)){
					tmp->set_slot(slot,CLIPS::Values());
				} else {
					tmp->set_slot(slot,CLIPS::Value());
				}
			} else {
        if (slot == "state") {
          tmp->set_slot(slot,CLIPS::Value("FORMULATED",CLIPS::TYPE_SYMBOL));
          continue;
        }
				if (!plan_action->is_multifield_slot(slot)) {
					tmp->set_slot(slot,pa_fact->slot_value(slot)[0]);
				} else {
					tmp->set_slot(slot,pa_fact->slot_value(slot));
				}
			}
		}

		if (!clips->assert_fact(tmp)){
			logger->log_error(name(),"Failed to assert plan-action %s",tmp->slot_value("action-name")[0].as_string().c_str());
		}
	}
}

/**
 * @brief Check if the diagnosis initialization is finished. Say, that the different histories finished
 * propagating
 * 
 * @return true If the diagnosis-setup-stage of the diagnosis environment is in state HISTORY-PROPAGATED
 * @return false Otherwise
 */
bool
ClipsDiagnosisEnvThread::clips_init_finished()
{
	MutexLocker lock(clips.objmutex_ptr());
	clips->refresh_agenda();
	clips->run();
	// Verify that initialization did not fail (yet)
	{
		CLIPS::Fact::pointer fact = clips->get_facts();
		while (fact) {
			CLIPS::Template::pointer tmpl = fact->get_template();
			if (tmpl->name() == "diagnosis-setup-stage") {
				CLIPS::Values v = fact->slot_value("state");
				if (v.size() > 0 && (v[0].as_string() == "HISTORY-PROPAGATED" || v[0].as_string() == "FAILED")) {
					return true;
				}
			}
			fact = fact->next();
		}
	}
	return false;
}

/**
 * @brief Inform the diagnosis environment that the setup is finished. This means that all
 * plan-actions and all wm-facts are asserted and that history propagation can start
 * 
 */
void
ClipsDiagnosisEnvThread::setup_finished()
{
	MutexLocker lock(clips.objmutex_ptr());

	clips->refresh_agenda();
	clips->run();
	clips->assert_fact("(diagnosis-setup-finished)");
  clips->refresh_agenda();
	clips->run();
}

/**
 * @brief Assert a new diagnosis hypothesis to the diagnosis environment
 * 
 * @param hypo_id Id of the diagnosis hypothesis
 */
void
ClipsDiagnosisEnvThread::add_diagnosis_hypothesis(std::string hypo_id,bool diag_candidate) 
{
	MutexLocker lock(clips.objmutex_ptr());

	if (!clips){
		logger->log_error(name(),"Clips Pointer invalid");
		return;
	}
  logger->log_info(name(),"Diag ID: %s",hypo_id.c_str());
	CLIPS::Template::pointer diag_hypothesis = clips->get_template("diagnosis-hypothesis");
	if (diag_hypothesis) {
		CLIPS::Fact::pointer fact = CLIPS::Fact::create(**clips,diag_hypothesis);
		fact->set_slot("id",CLIPS::Value(hypo_id,CLIPS::TYPE_SYMBOL));
		fact->set_slot("state",CLIPS::Value("INIT",CLIPS::TYPE_SYMBOL));
		fact->set_slot("probability",CLIPS::Value(-1.0));
    if (diag_candidate) {
      fact->set_slot("candidate",CLIPS::Value("TRUE",CLIPS::TYPE_SYMBOL));
    } else
    {
      fact->set_slot("candidate",CLIPS::Value("FALSE",CLIPS::TYPE_SYMBOL));
    }
    
    
		try{
			auto ret = clips->assert_fact(fact);
			if (!ret) {
				logger->log_error(name(),"Failed to assert fact");
			} 
		} catch ( ... ) {
				logger->log_error(name(),"Failed to assert fact: Exception");
		}
	} else {
		logger->log_error(name(),"Unable to find template diagnosis-hypothesis");
	}
	
}

/**
 * @brief Assert a wm-fact to the diagnosis environment for a given wm-fact id
 * 
 * @param pos Flag if the value of the fact should be true or false
 * @param id wm-fact id string
 */
void
ClipsDiagnosisEnvThread::add_wm_fact_from_id(bool pos, std::string id)
{
	MutexLocker lock(clips.objmutex_ptr());

	if (!clips){
		return logger->log_error(name(),"Clips pointer invalid");
	}
	CLIPS::Template::pointer wm_fact = clips->get_template("wm-fact");
	if (wm_fact) {
		CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,wm_fact);
		tmp->set_slot("id",CLIPS::Value(id,CLIPS::TYPE_STRING));
		tmp->set_slot("key",CLIPS::Values());
		tmp->set_slot("type", CLIPS::Value("BOOL",CLIPS::TYPE_SYMBOL));
		tmp->set_slot("is-list", CLIPS::Value("FALSE",CLIPS::TYPE_SYMBOL));
		if(pos) {
			tmp->set_slot("value", CLIPS::Value("TRUE",CLIPS::TYPE_SYMBOL));
		} else {
			tmp->set_slot("value", CLIPS::Value("FALSE",CLIPS::TYPE_SYMBOL));
		}

		tmp->set_slot("values",CLIPS::Values());

		tmp->set_slot("env",CLIPS::Value("DEFAULT",CLIPS::TYPE_SYMBOL));

		try{
			auto ret = clips->assert_fact(tmp);
			if (!ret) {
				logger->log_error(name(),"Failed to assert fact");
			} 
		} catch ( ... ) {
			logger->log_error(name(),"Failed to assert fact: Exception");
		}
	} else {
		logger->log_error(name(),"Cant find wm-fact template");
	}
}

/**
 * @brief Check the information gain of a grounded sensed predicate onto the hypotheses set.
 * Refer to Mühlbacher, Clemens, and Gerald Steinbauer. "Active Diagnosis for Agents with Belief Management."
 * 
 * @param predicate Predicate name
 * @param key_args Grounded parameters of the predicate. It is assumed that the key_args vector contains parameter names
 * and values in an alternating fashion. Eg m C-CS1 side INPUt
 * @return float A value between 0 and 1 representing the mutual information gain.
 */
float
ClipsDiagnosisEnvThread::information_gain(std::string predicate, std::vector<std::string> key_args)
{
	MutexLocker lock(clips.objmutex_ptr());

	std::string arguments = predicate;
	for (std::string key_arg : key_args)
	{
		arguments += " " + key_arg;
	}

	CLIPS::Values ret = clips->function("diagnosis-information-gain",arguments);
	if (ret.size() == 0)
	{
		logger->log_error(name(),"Failed to evaluate clips function (diagnosis-information-gain %s",arguments.c_str());
		return 0.0;
	}
	if (ret.size() > 1)
	{
		logger->log_error(name(),"Unexpected multifield returned by (diagnosis-information-gain %s",arguments.c_str());
		return 0.0;
	}
  logger->log_info(name(),"Predicate %s has information gain %f",predicate.c_str(), ret[0].as_float());
	return ret[0].as_float();
}

/**
 * @brief Insert a domain-sensing-result fact to the diagnosis environment, informing it about a new sensing result
 * The diagnosis environment then excludes all contradicting hypotheses.
 * 
 * @param pos True if the predicate is sensed as true, false otherwise
 * @param predicate Name of the sensed predicate
 * @param key_args Parameters of the sensed predicate. Assumed to have parameter names and parameter values in an alternating fashion
 */
void
ClipsDiagnosisEnvThread::add_sensing_result_from_key(bool pos, std::string predicate, std::vector<std::string> key_args)
{
	MutexLocker lock(clips.objmutex_ptr());

	if (!clips){
		logger->log_error(name(),"Clips pointer invalid");
	}
	CLIPS::Template::pointer sens_result = clips->get_template("diagnosis-sensing-result");
	if (sens_result) {
		CLIPS::Values clips_key_args;
		for (std::string key_arg: key_args) {
			clips_key_args.push_back(CLIPS::Value(key_arg,CLIPS::TYPE_SYMBOL));
		}

		CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,sens_result);
		tmp->set_slot("predicate",CLIPS::Value(predicate,CLIPS::TYPE_SYMBOL));
		tmp->set_slot("args",clips_key_args);
		if (pos) {
			tmp->set_slot("value", CLIPS::Value("TRUE",CLIPS::TYPE_SYMBOL));
		} else {
			tmp->set_slot("value", CLIPS::Value("FALSE",CLIPS::TYPE_SYMBOL));
		}

		try{
			auto ret = clips->assert_fact(tmp);
			if (!ret) {
				logger->log_error(name(),"Failed to assert fact");
			} 
		} catch ( ... ) {
			logger->log_error(name(),"Failed to assert fact: Exception");
		}
	} else {
		logger->log_error(name(),"Cant find diagnosis-sensing-result template");
	}
}

/**
 * @brief Return a vector of pairs where the first entry of a pair is the id string of a wm-fact
 * and the second entry is the id of the diagnosis in which this wm-fact is true.
 * 
 * @return std::vector<std::pair<std::string,std::string>> vector of wm-fact id strings and hypothesis ids
 */
std::vector<std::pair<std::string,std::string>>
ClipsDiagnosisEnvThread::get_fact_strings()
{
	MutexLocker lock(clips.objmutex_ptr());
	std::vector<std::pair<std::string,std::string>> ret;
	
	CLIPS::Fact::pointer fact_ptr = clips->get_facts();
	while (fact_ptr) {
		CLIPS::Template::pointer tmpl = fact_ptr->get_template();
		if (tmpl->name() == "wm-fact") {
     // logger->log_info(name(),"%s : %s",fact_ptr->slot_value("env")[0].as_string().c_str(), fact_ptr->slot_value("id")[0].as_string().c_str());
			ret.push_back(std::make_pair(fact_ptr->slot_value("id")[0].as_string(),
																	 fact_ptr->slot_value("env")[0].as_string()));
		}
		fact_ptr = fact_ptr->next();
	}
	return ret;
}

/**
 * @brief Integrate a sensing result to the diagnosis environment and check the amount of diagnosis
 * hypotheses that are still valid after integrating the sensing result
 * 
 * @param positive True if the predicated was sensed as true
 * @param predicate Name of the predicate
 * @param param_names Vector of parameter names
 * @param param_values Vector of parameter values, where the ith value refer to the ith parameter of param_names
 * @return int The number of diagnosis hypotheses still valid after integrating the sensing result
 */
int
ClipsDiagnosisEnvThread::integrate_sensing_result(bool positive, std::string predicate, CLIPS::Values param_names, CLIPS::Values param_values)
{
	MutexLocker lock(clips.objmutex_ptr());

	std::vector<std::string> key_list;

	if (param_names.size() != param_values.size()) {
		logger->log_error(name(),"Invalid param-names and param values of %s",predicate.c_str());
		return false;
	}

	for (size_t i = 0; i < param_names.size(); ++i)
	{
		key_list.push_back(param_names[i].as_string());
		key_list.push_back(param_values[i].as_string());
	}

	add_sensing_result_from_key(positive,predicate,key_list);

	
	clips->refresh_agenda();
	clips->run();

	int diag_count;
 	CLIPS::Values clips_diag_count = clips->function("diagnosis-hypothesis-count");
	if (clips_diag_count.size() != 1) {
		logger->log_error(name(),"Failed to count diagnosis hypotheses");
		return 0;
	} else {
		diag_count = (int)clips_diag_count[0].as_float();
	}

	return diag_count;
}

void
ClipsDiagnosisEnvThread::finalize()
{
	MutexLocker lock(clips.objmutex_ptr());
	logger->log_info(name(),"Killed diagnosis environment: %s", diag_id_.c_str());
	clips->clear();
	//clips->refresh_agenda();
	//clips->run();
}


void
ClipsDiagnosisEnvThread::loop()
{
	MutexLocker lock(clips.objmutex_ptr());
	clips->refresh_agenda();
	clips->run();
}

