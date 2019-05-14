
/***************************************************************************
 *  clips_active_diagnosis_thread.cpp - CLIPS feature to access the robot memory
 *
 *  Created: Mon Aug 29 15:41:47 2016
 *  Copyright  2016       Frederik Zwilling
 *             2013-2018  Tim Niemueller [www.niemueller.de]
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
#include <core/threading/mutex_locker.h>
#include <utils/misc/string_conversions.h>
#include <utils/misc/string_split.h>

using namespace fawkes;
using namespace mongo;

/** @class ClipsActiveDiagnosisThread 'clips_active_diagnosis_thread.h' 
 * CLIPS feature to access the robot memory.
 * MongoDB access through CLIPS first appeared in the RCLL referee box.
 * @author Tim Niemueller
 * @author Frederik Zwilling
 */

ClipsActiveDiagnosisThread::ClipsActiveDiagnosisThread()
: Thread("ClipsActiveDiagnosisThread", Thread::OPMODE_WAITFORWAKEUP),
  CLIPSFeature("active-diagnosis"), CLIPSFeatureAspect(this)
{
}

void
ClipsActiveDiagnosisThread::init()
{
}

void
ClipsActiveDiagnosisThread::loop()
{
}

void
ClipsActiveDiagnosisThread::finalize()
{
  envs_.clear();
  diag_envs_.clear();
}

void
ClipsActiveDiagnosisThread::clips_context_init(const std::string &env_name,
          LockPtr<CLIPS::Environment> &clips)
{
  envs_[env_name] = clips;
  logger->log_debug(name(), "Called to initialize environment %s", env_name.c_str());

  clips.lock();
  clips->add_function("active-diagnosis-set-up", sigc::slot<CLIPS::Value, std::string>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::set_up_active_diagnosis)));
  clips->add_function("active-diagnosis-final", sigc::slot<CLIPS::Value>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::finalize_diagnosis)));
  clips->add_function("active-diagnosis-delete", sigc::slot<void>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::delete_diagnosis)));
  clips->add_function("active-diagnosis-integrate-measurement", sigc::slot<CLIPS::Value,std::string, std::string>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::integrate_measurement)));
  clips->add_function("active-diagnosis-get-sensing-action", sigc::slot<CLIPS::Value>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::get_sensing_action)));
  clips.unlock();
}

void
ClipsActiveDiagnosisThread::clips_context_destroyed(const std::string &env_name)
{
  envs_.erase(env_name);
  logger->log_debug(name(), "Removing environment %s", env_name.c_str());
}

/*
  Sets up a clips environment for each diagnosis hypothesis
*/
CLIPS::Value
ClipsActiveDiagnosisThread::set_up_active_diagnosis(std::string diag_id)
{
  std::vector<float> hypothesis_ids;
  std::map<std::string,fawkes::LockPtr<CLIPS::Environment>>::iterator it;
  for (it = envs_.begin(); it != envs_.end(); it++) {
    CLIPS::Fact::pointer ret = it->second->get_facts();
    while(ret) {
      CLIPS::Template::pointer tmpl = ret->get_template();
      if (tmpl->name() == "diagnosis-hypothesis") {
       try {
         CLIPS::Value fact_diag_id = ret->slot_value("diag-id")[0];
            if (fact_diag_id.as_string() == diag_id) {
              CLIPS::Values fact_id = ret->slot_value("id");
              if (fact_id.empty()) {
                  logger->log_error(name(), "Slot id empty");
              } else {
                  hypothesis_ids.push_back(fact_id[0].as_float());
              }
             
            }        
       } catch (Exception &e) {}
     }
     ret = ret->next();
    }
  }

  for (float hypo_id : hypothesis_ids) {
    try {
      auto env_thread_ptr = std::make_shared<ClipsDiagnosisEnvThread>(diag_id,hypo_id);
      diag_envs_.push_back(env_thread_ptr);
      thread_collector->add(&(*env_thread_ptr));
    } catch (fawkes::CannotInitializeThreadException &e) {
      logger->log_error(name(),"Cannot start diagnosis environment: %s",e.what());
      return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
    }
  }
 
  //TODO: worldmodel dump path parsen
  std::string world_model_path = StringConversions::resolve_path("/home/sagre/uni/fawkes-robotino/cfg/robot-memory/TEST-PLAN");
  if (robot_memory->restore_collection("syncedrobmem.worldmodel",world_model_path,"diagnosis.worldmodel") == 0)
  {
    logger->log_error(name(),"Failed to restore collection from %s",world_model_path.c_str());
    return -1;
  }

  BSONObjBuilder query_b;
  query_b << "_id" << BSONRegEx("^/domain/fact");

  BSONObj q = query_b.done();

  std::unique_ptr<mongo::DBClientCursor> c = robot_memory->query(q,"diagnosis.worldmodel");
  if (c) {
    while(c->more()){
      BSONObj obj = c->next();
      if (obj.hasField("_id")) {
        std::string id = obj.getFieldDotted("_id").String();
        for (auto diag_env : diag_envs_) {
          diag_env->add_wm_fact(id);
        }
      }
    }
  } else {
    logger->log_error(name(),"Failed to query for worldmodel facts");
    return -1;
  }

  return CLIPS::Value("TRUE", CLIPS::TYPE_SYMBOL);
}

/*
  Integrates the "true" diagnosis hypothesis into the world model, if there is only one left
  Returns FALSE otherwise
*/
CLIPS::Value
ClipsActiveDiagnosisThread::finalize_diagnosis()
{
  delete_diagnosis();
  return CLIPS::Value("TRUE", CLIPS::TYPE_SYMBOL);
}

/*
  Clears all created diagnosis environments
*/
void
ClipsActiveDiagnosisThread::delete_diagnosis()
{
  for (auto diag_env : diag_envs_) {
    thread_collector->remove(&*diag_env);
  }
  logger->log_info(name(),"Fake delete");
  diag_envs_.clear();

}

/*
  Integrates a sensor measurement by removing all diagnosis hypothesis that are contradicted by the measurement
*/
CLIPS::Value
ClipsActiveDiagnosisThread::integrate_measurement(std::string fact, std::string value)
{
  return CLIPS::Value("TRUE", CLIPS::TYPE_SYMBOL);
}


/*
  Selects the next action according to the current state of each diagnosis environment
*/
CLIPS::Value
ClipsActiveDiagnosisThread::get_sensing_action() 
{
/*

*/
    return CLIPS::Value("test-action");
}

std::string
ClipsActiveDiagnosisThread::clips_value_to_string(CLIPS::Value val)
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