
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
ClipsDiagnosisEnvThread::ClipsDiagnosisEnvThread(std::string diag_id)
	: Thread("ClipsDiagnosisEnvThread", Thread::OPMODE_CONTINUOUS),
	  CLIPSAspect(diag_id.c_str(), std::string("CLIPS (" + diag_id + ")").c_str())
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
	logger->log_error(name(),"Init Diagnosis environment thread");
	try {
		std::string test_fact = std::string("(" + diag_id_ + ")");
    	clips->assert_fact(test_fact);
    	clips->refresh_agenda();
		clips->run();
	
		CLIPS::Values vfacts;
		try {
			vfacts = clips->evaluate("(get-fact-list)");
		} catch (std::exception &e) {
			logger->log_error(name(),"Cannot evaluate: %s",e.what());
		}
		
		

		clips->run();
		for (CLIPS::Value f : vfacts) {
			try {
					logger->log_error(name(),"%s",f.as_string().c_str());
			} catch (...) {}

		}
	} catch (Exception &e) {
		logger->log_error(name(),"Cannot initialize: %s",e.what());
	}

	std::string diag_query = std::string("goal-id=" + diag_id_);

	BSONObjBuilder query_b;
  query_b << "_id" << BSONRegEx(diag_query);
 	BSONObj q = query_b.done();
 	logger->log_error(name(),"Query robmem.diagnosis for %s",q.toString().c_str());

 	std::unique_ptr<mongo::DBClientCursor> c = robot_memory->query(q,"syncedrobmem.worldmodel");
 
 	if (c) {
   	while(c->more()){
     		BSONObj obj = c->next();
			logger->log_info(name(),obj.toString().c_str());
   	}
  } else {
   	logger->log_error(name(),"Failed to query for plan-action facts");
  	return;
  }
    
}


void
ClipsDiagnosisEnvThread::finalize()
{
	clips->assert_fact("(active_diagnosis-finalize)");
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
