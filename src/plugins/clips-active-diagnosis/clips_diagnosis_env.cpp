
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

#include "clips_active_diagnosis_thread.h"

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
ClipsDiagnosisEnvThread::ClipsDiagnosisEnvThread(std::string diag_id,float hypothesis_id)
	: Thread("ClipsDiagnosisEnvThread", Thread::OPMODE_CONTINUOUS),
	  CLIPSAspect(std::string(diag_id + "-" + std::to_string(hypothesis_id)).c_str(), std::string("CLIPS (" + diag_id + "-" + std::to_string(hypothesis_id) + ")").c_str())
{
    diag_id_ = diag_id;
		hypothesis_id_ = hypothesis_id;
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
ClipsDiagnosisEnvThread::add_plan_action(CLIPS::Fact::pointer pa_fact)
{
	MutexLocker lock(clips.objmutex_ptr());

	CLIPS::Template::pointer plan_action = clips->get_template("plan-action");
	if (plan_action) {
		CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,plan_action);
		for (std::string slot : tmp->slot_names()) {
			if (pa_fact->slot_value(slot).empty()) {
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
				if (v.size() > 0 && v[0].as_string() == "HISTORY-PROPAGATED") {
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

//TODO: Cleanup
void
ClipsDiagnosisEnvThread::add_wm_fact(std::string id)
{
	MutexLocker lock(clips.objmutex_ptr());

	if (!clips){
		logger->log_error(name(),"Clips pointer invalid");
	}
	CLIPS::Template::pointer wm_fact = clips->get_template("wm-fact");
	if (wm_fact) {
		CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,wm_fact);
		tmp->set_slot("id",CLIPS::Value(std::string("\"" + id + "\""),CLIPS::TYPE_SYMBOL));
		tmp->set_slot("key",CLIPS::Values());
		tmp->set_slot("type", CLIPS::Value("BOOL",CLIPS::TYPE_SYMBOL));
		tmp->set_slot("is-list", CLIPS::Value("FALSE",CLIPS::TYPE_SYMBOL));
		tmp->set_slot("value", CLIPS::Value("TRUE",CLIPS::TYPE_SYMBOL));
		tmp->set_slot("values",CLIPS::Values());

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
	logger->log_info(name(),"Killed diagnosis environment: %s %f", diag_id_.c_str(),hypothesis_id_);
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
