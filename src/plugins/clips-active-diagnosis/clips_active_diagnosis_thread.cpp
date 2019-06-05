
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
  clips->add_function("active-diagnosis-integrate-measurement", sigc::slot<CLIPS::Value,int, std::string, CLIPS::Values, CLIPS::Values>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::integrate_measurement)));
  clips->add_function("active-diagnosis-update-common-knowledge", sigc::slot<CLIPS::Value>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::update_common_knowledge)));
  clips->add_function("active-diagnosis-information-gain", sigc::slot<CLIPS::Value, std::string>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::information_gain)));
  clips.unlock();
}

void
ClipsActiveDiagnosisThread::clips_context_destroyed(const std::string &env_name)
{
  envs_.erase(env_name);
  logger->log_error(name(), "Removing environment--- %s", env_name.c_str());
}

bool
ClipsActiveDiagnosisThread::diag_env_initiate_wm_facts(const std::string &plan_id)
{ 
  //TODO: worldmodel dump path parsen
  std::string world_model_path = StringConversions::resolve_path(std::string("/home/sagre/uni/fawkes-robotino/cfg/robot-memory/" + plan_id));
  if (robot_memory->restore_collection("pddl.worldmodel",world_model_path,"diagnosis.worldmodel") == 0)
  {
    logger->log_error(name(),"Failed to restore collection from %s",world_model_path.c_str());
    return false;
  }

  BSONObjBuilder query_b;
  query_b << "_id" << BSONRegEx("^/domain/fact");

  BSONObj q = query_b.done();
  try{
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
    return false;
  }
  } catch (mongo::UserException &e) {
    logger->log_error(name(),"Exception while filling wm-facts: %s", e.toString().c_str());
    return false;
  }
  return true;
}

std::string
ClipsActiveDiagnosisThread::get_plan_id_from_diag_id(const std::string &diag_id)
{
  std::map<std::string,fawkes::LockPtr<CLIPS::Environment>>::iterator it;
  for (it = envs_.begin(); it != envs_.end(); it++) {
    CLIPS::Fact::pointer ret = it->second->get_facts();
    while(ret) {
      CLIPS::Template::pointer tmpl = ret->get_template();
      if (tmpl->name() == "diagnosis") {
        try {
          CLIPS::Value fact_diag_id = ret->slot_value("id")[0];
          if (clips_value_to_string(fact_diag_id) == diag_id) {
            CLIPS::Values fact_plan_id = ret->slot_value("plan-id");
            if (fact_plan_id.empty()) {
              logger->log_error(name(), "Slot id empty");
            } else {
              return fact_plan_id[0].as_string();
            } 
          }        
       } catch (Exception &e) {
          logger->log_error(name(), "Exception while iterating facts: %s",e.what());
       }
      }
      ret = ret->next();
    }
  }
  return "";
}

std::vector<float>
ClipsActiveDiagnosisThread::get_hypothesis_ids(const std::string &diag_id)
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
          if (clips_value_to_string(fact_diag_id) == diag_id) {
            CLIPS::Values fact_id = ret->slot_value("id");
            if (fact_id.empty()) {
              logger->log_error(name(), "Slot id empty");
            } else {
              hypothesis_ids.push_back(fact_id[0].as_float());
            } 
          }        
       } catch (Exception &e) {
          logger->log_error(name(), "Exception while iterating facts: %s",e.what());
       }
      }
      ret = ret->next();
    }
  }
  return hypothesis_ids;
}

bool
ClipsActiveDiagnosisThread::diag_env_initiate_plan_actions(const std::string &diag_id)
{
  std::map<std::string,fawkes::LockPtr<CLIPS::Environment>>::iterator it;
  for (it = envs_.begin(); it != envs_.end(); it++) {
    MutexLocker lock(it->second.objmutex_ptr());
    CLIPS::Fact::pointer ret = it->second->get_facts();
    while(ret) {
      CLIPS::Template::pointer tmpl = ret->get_template();
      if (tmpl->name() == "plan-action") {
        try {
          CLIPS::Value fact_goal_id = ret->slot_value("goal-id")[0];
          if (clips_value_to_string(fact_goal_id) == diag_id) {
            for (auto diag_env : diag_envs_) {
              if (diag_env->get_hypothesis_id() == ret->slot_value("plan-id")[0].as_float()){
                diag_env->add_plan_action(ret);
              }
            }
          }        
       } catch (Exception &e) {
         logger->log_error(name(), "Exception while iterating facts: %s",e.what());
       }
      }
      ret = ret->next();
    }
  }
  return true;
}

/*
  Sets up a clips environment for each diagnosis hypothesis
*/
CLIPS::Value
ClipsActiveDiagnosisThread::set_up_active_diagnosis(std::string diag_id)
{
  logger->log_info(name(),"Starting to setup diagnosis environment for %s",diag_id.c_str());
  std::vector<float> hypothesis_ids = get_hypothesis_ids(diag_id);
  if (hypothesis_ids.empty()) {
    logger->log_error(name(),"Failed to get hypothesis ids for diagnosis %s from cx environment", diag_id.c_str());
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished getting hypothesis ids");
  int count = 0;
  for (float hypo_id : hypothesis_ids) {

    try {
      count++;
      //if (count != 1 && count != (int)hypothesis_ids.size()) continue;
      auto env_thread_ptr = std::make_shared<ClipsDiagnosisEnvThread>(diag_id,hypo_id);
      diag_envs_.push_back(env_thread_ptr);
      thread_collector->add(&(*env_thread_ptr));

    } catch (fawkes::CannotInitializeThreadException &e) {
      logger->log_error(name(),"Cannot start diagnosis environment: %s",e.what());
      return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
    }
  }

  logger->log_info(name(),"Finished starting diagnosis environments");
  std::string plan_id = get_plan_id_from_diag_id(diag_id);
  if (plan_id == "") {
    logger->log_error(name(),"Failed to get plan-id for diagnosis %s from cx environment",diag_id.c_str());
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished getting plan_id");
  if (!diag_env_initiate_wm_facts(plan_id)) {
    logger->log_error(name(),"Failed to initiate worldmodel for diagnosis environments");
    delete_diagnosis();
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished initializing wm-facts");

  if (!diag_env_initiate_plan_actions(diag_id)) {
    logger->log_error(name(),"Failed to initiate plan-actions for diagnosis environments");
    delete_diagnosis();
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished initializing plan-actions");

  for (auto diag_env : diag_envs_) {
    diag_env->setup_finished();
  }
   //TODO: Thats a bad way to wait for the environments to finish...
  bool clips_init_finished = false;
  while (!clips_init_finished) {
    clips_init_finished = true;
    for (auto diag_env : diag_envs_) {
      if (!diag_env->clips_init_finished()) {
        clips_init_finished = false;
        break;
      }
    }
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
  MutexLocker lock(envs_["executive"].objmutex_ptr());

  std::map<std::string,int>::iterator it = fact_occurences_.begin();
  for (;it != fact_occurences_.end();it++)
  {
    if (it->second < (int)diag_envs_.size()) {
      return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
    }
  }
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
    diag_env->wait_loop_done();
    thread_collector->remove(&*diag_env);
  }
  diag_envs_.clear();

}

/*
* Returns information gain for a given predicate. Predicate string is expected to have the
* format: predicate-name param-name param-value ....
*/
CLIPS::Value
ClipsActiveDiagnosisThread::information_gain(std::string grounded_predicate)
{
  std::vector<std::string> pred_splitted = str_split(grounded_predicate, " ");
  std::string fact_string = "domain fact";
  fact_string += " " + pred_splitted[0] + " args?";

  for (size_t i = 1; i < pred_splitted.size(); ++i)
  {
    fact_string += " " + pred_splitted[i];
  }

  logger->log_info(name(),"Check gain of %s, %s", fact_string.c_str(), grounded_predicate.c_str());

  if (fact_occurences_.find(fact_string) == fact_occurences_.end()) {
    logger->log_info(name(),"Not found: %s \n Not a predicate that the hyopthesises disagree on",fact_string.c_str());
    return CLIPS::Value(0.0);
  } else {
    float valid = fact_occurences_[fact_string];
    float total = (int)diag_envs_.size();
    float entropy = -(1.0/log(2)) * ( (valid/total) * log((valid/total)) 
                                      + ((total-valid)/total) * log((total-valid)/total) );
    logger->log_info(name(),"Gain: %f",entropy);
    return CLIPS::Value(entropy);
  }
  
}


/*
  Integrates a sensor measurement by removing all diagnosis hypothesis that are contradicted by the measurement
*/
CLIPS::Value
ClipsActiveDiagnosisThread::integrate_measurement(int pos, std::string predicate, CLIPS::Values param_names, CLIPS::Values param_values)
{
  std::string fact_string;
  std::vector<std::shared_ptr<ClipsDiagnosisEnvThread>>::iterator it = diag_envs_.begin();
  while(it != diag_envs_.end())
  {
    if ((*it)->valid_measurement_result(predicate,param_names,param_values) != pos){
      logger->log_error(name(),"Removing: %f - %s",(*it)->get_hypothesis_id(),(*it)->get_diag_id().c_str());
      (*it)->wait_loop_done();
      thread_collector->remove(&*(*it));
      it = diag_envs_.erase(it);
      //it++;
    } else {
      it++;
    }
  }
  logger->log_info(name(),"Still %d hypothesis left after integrating knowledge about %s",(int)diag_envs_.size(),predicate.c_str());
  return CLIPS::Value("TRUE", CLIPS::TYPE_SYMBOL);
}

CLIPS::Value
ClipsActiveDiagnosisThread::update_common_knowledge()
{
  auto clips = envs_["executive"];
  fact_occurences_.clear();
  for (auto diag_env : diag_envs_) {
    std::vector<std::string> diag_facts = diag_env->get_fact_strings();
    for (std::string fact_string : diag_facts) {
      if (fact_occurences_.find(fact_string) == fact_occurences_.end()) {
        fact_occurences_[fact_string] = 1;
      } else {
        fact_occurences_[fact_string]++;
      }
    }
  }
  logger->log_info(name(),"Start matching fact bases");
  CLIPS::Fact::pointer cx_fact_ptr = clips->get_facts();
  while (cx_fact_ptr)
  {   
    if (cx_fact_ptr->get_template()->name() == "wm-fact")
    {
      std::string cx_fact_string = wm_fact_to_string(cx_fact_ptr);
      if (fact_occurences_.find(cx_fact_string) != fact_occurences_.end() &&
          fact_occurences_[cx_fact_string] != (int)diag_envs_.size())
      {
        logger->log_error(name(),"Retract: %s",cx_fact_string.c_str());
        CLIPS::Fact::pointer tmp(cx_fact_ptr->next());
        cx_fact_ptr->retract();
        cx_fact_ptr = tmp;
      } else {
        cx_fact_ptr = cx_fact_ptr->next();
      }
    } else {
      cx_fact_ptr = cx_fact_ptr->next();
    }
    
  }

  std::map<std::string,int>::iterator it = fact_occurences_.begin();
  for (;it != fact_occurences_.end(); it++)
  {
    if (it->second == (int)diag_envs_.size())
    {
      //logger->log_error(name(),"Assert: %s",it->first.c_str());
      CLIPS::Template::pointer wm_fact = clips->get_template("wm-fact");
	    if (wm_fact) {
		    CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,wm_fact);
        CLIPS::Values key_values;
        std::vector<std::string> key_string_splitted = str_split(it->first," ");

        for (auto key_value : key_string_splitted) {
          key_values.push_back(CLIPS::Value(key_value,CLIPS::TYPE_SYMBOL));
        }

	    	tmp->set_slot("id",CLIPS::Value("",CLIPS::TYPE_STRING));
		    tmp->set_slot("key",key_values);
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
  }
  logger->log_info(name(),"Updating done");
  return CLIPS::Value("TRUE",CLIPS::TYPE_SYMBOL);
}
