
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

#include "clips_common_env.h"

#include <utils/misc/string_conversions.h>
#include <utils/misc/string_split.h>
#include <utils/misc/map_skill.h>
#include <interfaces/SwitchInterface.h>
#include <core/threading/mutex_locker.h>

using namespace fawkes;
/** @class ClipsCommonEnvThread 
 * Clips Environment containing common knowledge of all diagnosis hypothesises
 *
 * @author Daniel Habering
 */

/** Constructor. */
ClipsCommonEnvThread::ClipsCommonEnvThread()
	: Thread("ClipsCommonEnvThread", Thread::OPMODE_CONTINUOUS),
	  CLIPSAspect("common-diagnosis", "CLIPS (common-diagnosis)")
{
}

/** Destructor. */
ClipsCommonEnvThread::~ClipsCommonEnvThread()
{

}

void
ClipsCommonEnvThread::init()
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

//TODO: Cleanup
void
ClipsCommonEnvThread::add_wm_fact(std::string id)
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
ClipsCommonEnvThread::finalize()
{
	MutexLocker lock(clips.objmutex_ptr());
	logger->log_info(name(),"Killed diagnosis environment: common-diagnosis");
	clips->clear();
	clips->refresh_agenda();
	clips->run();
}


void
ClipsCommonEnvThread::loop()
{
	MutexLocker lock(clips.objmutex_ptr());

	clips->refresh_agenda();
	clips->run();
}
