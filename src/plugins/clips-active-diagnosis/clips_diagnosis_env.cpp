
/***************************************************************************
 *  clips_active_diagnosis_thread.cpp -  CLIPS active_diagnosis
 *
 *  Created: Tue Sep 19 12:00:06 2017
 *  Copyright  2006-2017  Tim Niemueller [www.niemueller.de]
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
using namespace mongo;
/** @class ClipsDiagnosisEnvThread "clips_active_diagnosis_thread.h"
 * Main thread of CLIPS-based active_diagnosis.
 *
 * @author Tim Niemueller
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

void
ClipsDiagnosisEnvThread::add_plan_action(CLIPS::Fact::pointer pa_fact,float diag_id)
{
	MutexLocker lock(clips.objmutex_ptr());

	CLIPS::Template::pointer plan_action = clips->get_template("plan-action");
	if (plan_action) {
		CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,plan_action);
		for (std::string slot : tmp->slot_names()) {
			if (pa_fact->slot_value(slot).empty()) {
				if (slot == "diag-id") {
					tmp->set_slot(slot,CLIPS::Value(std::to_string(diag_id),CLIPS::TYPE_SYMBOL));
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

void
ClipsDiagnosisEnvThread::setup_finished()
{
	MutexLocker lock(clips.objmutex_ptr());

	clips->refresh_agenda();
	clips->run();
	clips->assert_fact("(diagnosis-setup-finished)");
}

void
ClipsDiagnosisEnvThread::add_diagnosis_hypothesis(float hypo_id) 
{
	MutexLocker lock(clips.objmutex_ptr());

	if (!clips){
		logger->log_error(name(),"Clips Pointer invalid");
		return;
	}

	CLIPS::Template::pointer diag_hypothesis = clips->get_template("diagnosis-hypothesis");
	if (diag_hypothesis) {
		CLIPS::Fact::pointer fact = CLIPS::Fact::create(**clips,diag_hypothesis);
		fact->set_slot("id",CLIPS::Value(std::to_string(hypo_id),CLIPS::TYPE_SYMBOL));
		fact->set_slot("state",CLIPS::Value("INIT",CLIPS::TYPE_SYMBOL));
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

std::vector<std::pair<std::string,std::string>>
ClipsDiagnosisEnvThread::get_fact_strings()
{
	MutexLocker lock(clips.objmutex_ptr());
	std::vector<std::pair<std::string,std::string>> ret;
	
	CLIPS::Fact::pointer fact_ptr = clips->get_facts();
	while (fact_ptr) {
		CLIPS::Template::pointer tmpl = fact_ptr->get_template();
		if (tmpl->name() == "wm-fact") {
			ret.push_back(std::make_pair(wm_fact_to_string(fact_ptr),fact_ptr->slot_value("env")[0].as_string()));
		}
		fact_ptr = fact_ptr->next();
	}
	return ret;
}

int
ClipsDiagnosisEnvThread::sensing_result(bool positive, std::string predicate, CLIPS::Values param_names, CLIPS::Values param_values)
{
//
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



	return 2;
}

void
ClipsDiagnosisEnvThread::finalize()
{
	MutexLocker lock(clips.objmutex_ptr());
/*	CLIPS::Fact::pointer ret = clips->get_facts();
  while(ret) {
		std::vector<std::string> slot_names = ret->slot_names();
		std::string slot_str = "(" + ret->get_template()->name() + " ";
		for (std::string slot : slot_names) {
			slot_str += "(" + slot + " ";
			for (CLIPS::Value val : ret->slot_value(slot)) {
				if(val.type() == CLIPS::TYPE_STRING || val.type() == CLIPS::TYPE_SYMBOL) {
					slot_str += val.as_string() + " ";
				}
				else{
					slot_str += std::to_string(val.as_float()) + " ";
				}
			}
			slot_str += ") ";
		}
		logger->log_info(name(),slot_str.c_str());
		ret = ret->next();
	}*/
	logger->log_info(name(),"Killed diagnosis environment: %s", diag_id_.c_str());
	clips->clear();
	clips->refresh_agenda();
	clips->run();
}


void
ClipsDiagnosisEnvThread::loop()
{
	MutexLocker lock(clips.objmutex_ptr());

	clips->refresh_agenda();
	clips->run();
}

std::string wm_fact_to_string(CLIPS::Fact::pointer fact)
{
  CLIPS::Template::pointer tmpl = fact->get_template();
	CLIPS::Values values = fact->slot_value("key");
	std::string ret = clips_value_to_string(values[0]);
	for (size_t i = 1; i < values.size(); ++i){
		ret += " " + clips_value_to_string(values[i]);
	}
  return ret;
}

std::string clips_value_to_string(CLIPS::Value val)
{
  switch (val.type()){
    case CLIPS::TYPE_STRING:
      return val.as_string();
      break;
    case CLIPS::TYPE_SYMBOL:
      return val.as_string();
      break;
    case CLIPS::TYPE_INSTANCE_NAME:
      return val.as_string(); 
      break;
    case CLIPS::TYPE_FLOAT:
      return std::to_string(val.as_float());
      break;
    case CLIPS::TYPE_INTEGER:
      return std::to_string(val.as_float());
      break;
    default:
      return "";
  }
}
