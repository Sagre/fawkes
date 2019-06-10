/***************************************************************************
 *  clips_active_diagnosis_thread.cpp - CLIPS feature to access the robot memory
 *
 *  Created: Mon June 5 15:41:47 2019
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

#include "clips_active_diagnosis_thread.h"
#include <core/threading/mutex_locker.h>

#include <utils/misc/string_conversions.h>
#include <utils/misc/string_split.h>

using namespace fawkes;
using namespace mongo;

/** @class ClipsActiveDiagnosisThread 'clips_active_diagnosis_thread.h' 
 * CLIPS Feature to perform active diagnosis. Refer to:
 * Mühlbacher, Clemens, and Gerald Steinbauer. "Active Diagnosis for Agents with Belief Management."
 * @author Daniel Habering
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
}

/**
 * @brief Initialize the active diagnosis feature for a given clips environment
 * 
 * @param env_name Name of the environment
 * @param clips Pointer to the clips environment
 */
void
ClipsActiveDiagnosisThread::clips_context_init(const std::string &env_name,
          LockPtr<CLIPS::Environment> &clips)
{
  envs_[env_name] = clips;
  logger->log_debug(name(), "Called to initialize environment %s", env_name.c_str());

  world_model_dump_prefix_ = StringConversions::resolve_path("@BASEDIR@/" + config->get_string("plugins/pddl-diagnosis/world-model-dump-prefix"));
  collection_ = config->get_string("plugins/pddl-diagnosis/collection");

  clips.lock();
  clips->add_function("active-diagnosis-set-up", sigc::slot<CLIPS::Value, std::string>(sigc::bind<0>(
                                                                                sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::set_up_active_diagnosis),
                                                                                env_name)));
  clips->add_function("active-diagnosis-delete", sigc::slot<void>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::delete_diagnosis)));
  clips->add_function("active-diagnosis-integrate-measurement", sigc::slot<CLIPS::Value,int, std::string, CLIPS::Values, CLIPS::Values>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::integrate_measurement)));
  clips->add_function("active-diagnosis-update-common-knowledge", sigc::slot<CLIPS::Value>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::update_common_knowledge)));
  clips->add_function("active-diagnosis-information-gain", sigc::slot<CLIPS::Value, std::string>(sigc::mem_fun(*this, &ClipsActiveDiagnosisThread::information_gain)));
  clips.unlock();
}

/**
 * @brief Remove a destroyed clips environment from list
 * 
 * @param env_name Name of the destroyed environment
 */
void
ClipsActiveDiagnosisThread::clips_context_destroyed(const std::string &env_name)
{
  envs_.erase(env_name);
}

/**
 * @brief Restore the worldmodel dump of a given plan id and query it for wm-facts
 *        Assert all wm-facts to the diagnosis clips environment thread
 * @param plan_id ID of the plan for which a world model dump was created
 * @return true On success
 * @return false On failure
 */
bool
ClipsActiveDiagnosisThread::diag_env_initiate_wm_facts(const std::string &plan_id)
{ 
  //TODO: worldmodel dump path parsen
  std::string world_model_path = StringConversions::resolve_path(std::string(world_model_dump_prefix_ + "/" + plan_id));
  if (robot_memory->restore_collection(collection_,world_model_path,"diagnosis.worldmodel") == 0)
  {
    logger->log_error(name(),"Failed to restore collection from %s",world_model_path.c_str());
    return false;
  }

  BSONObjBuilder query_b;

  //Query for domain facts and hardware facts. Hardware facts are later used to determine 
  //probabilities of diagnosis candidates
  query_b << "_id" << BSONRegEx("^/domain/fact|^/hardware");

  BSONObj q = query_b.done();
  try{
    std::unique_ptr<mongo::DBClientCursor> c = robot_memory->query(q,"diagnosis.worldmodel");
    if (c) {
      while(c->more()){
        BSONObj obj = c->next();
        if (obj.hasField("_id")) {
          std::string id = obj.getFieldDotted("_id").String();  
          diag_env_->add_wm_fact_from_id(true,id);
        }
      }
    } else {
      logger->log_error(name(),"Failed to query for worldmodel facts from %s",collection_.c_str());
      return false;
    }
  } catch (mongo::UserException &e) {
    logger->log_error(name(),"Exception while filling wm-facts: %s", e.toString().c_str());
    return false;
  }
  return true;
}

/**
 * @brief For a given diagnosis id, determine the id of the plan that has to be diagnosis
 * 
 * @param diag_id ID of the current diagnosis
 * @return std::string The id of the plan that has to be diagnosed
 */
std::string
ClipsActiveDiagnosisThread::get_plan_id_from_diag_id()
{
  LockPtr<CLIPS::Environment> clips = envs_[env_name_];

  CLIPS::Fact::pointer ret = clips->get_facts();
  while(ret) {
    CLIPS::Template::pointer tmpl = ret->get_template();
    if (tmpl->name() == "diagnosis") {
      try {
        CLIPS::Value fact_diag_id = ret->slot_value("id")[0];
        if (fact_diag_id.as_string() == diag_id_) {
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
  
  return "";
}

/**
 * @brief Get all ids of possible hypotheses for a given diagnosis id
 * 
 * @return std::vector<float> A vector containing all hypothesis ids
 */
std::vector<float>
ClipsActiveDiagnosisThread::get_hypothesis_ids()
{
  std::vector<float> hypothesis_ids;

  LockPtr<CLIPS::Environment> clips = envs_[env_name_];

  CLIPS::Fact::pointer ret = clips->get_facts();
  while(ret) {
   CLIPS::Template::pointer tmpl = ret->get_template();
   if (tmpl->name() == "diagnosis-hypothesis") {
      try {
        CLIPS::Value fact_diag_id = ret->slot_value("diag-id")[0];
        if (fact_diag_id.as_string() == diag_id_) {
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

  return hypothesis_ids;
}

/**
 * @brief Query the current clips environment for plan-actions that refer to the current
 *        diagnosis. A plan action is part of a diagnosis candidate if the goal-id is
 *        equal to the diagnosis id. The plan-id refers to the hypothesis the plan-action
 *        belongs to.
 * 
 * @return true On success
 * @return false On failure
 */
bool
ClipsActiveDiagnosisThread::diag_env_initiate_plan_actions()
{
  LockPtr<CLIPS::Environment> clips = envs_[env_name_];

  MutexLocker lock(clips.objmutex_ptr());
  CLIPS::Fact::pointer ret = clips->get_facts();
  while(ret) {
    CLIPS::Template::pointer tmpl = ret->get_template();
    if (tmpl->name() == "plan-action") {
      try {
        CLIPS::Value fact_goal_id = ret->slot_value("goal-id")[0];
        if (fact_goal_id.as_string() == diag_id_) {
          diag_env_->add_plan_action(ret,ret->slot_value("plan-id")[0].as_float());
        }        
     } catch (Exception &e) {
       logger->log_error(name(), "Exception while iterating facts: %s",e.what());
     }
    }
    ret = ret->next();
  }
  return true;
}

/**
 * @brief Set up the plugin for a new diagnosis for a given environment and a given diagnosis id
 * 
 * @param env_name Name of the clips environment
 * @param diag_id ID of the diagnosis
 * @return CLIPS::Value TRUE if set up was successfull, FALSE otherwise
 */
CLIPS::Value
ClipsActiveDiagnosisThread::set_up_active_diagnosis(std::string env_name, std::string diag_id)
{
  //TODO Check if there is a diagnosis running

  diag_id_ = diag_id;
  env_name_ = env_name;

  logger->log_info(name(),"Starting to setup diagnosis environment for %s",diag_id.c_str());

  std::vector<float> hypothesis_ids = get_hypothesis_ids();
  if (hypothesis_ids.empty()) {
    logger->log_error(name(),"Failed to get hypothesis ids for diagnosis %s from cx environment", diag_id.c_str());
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished getting hypothesis ids");

  try {
    diag_env_ = std::make_shared<ClipsDiagnosisEnvThread>(diag_id_);
    thread_collector->add(&(*diag_env_));
  } catch (fawkes::CannotInitializeThreadException &e) {
    logger->log_error(name(),"Cannot start diagnosis environment: %s",e.what());
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished starting diagnosis environments");

  std::string plan_id = get_plan_id_from_diag_id();
  if (plan_id == "") {
    logger->log_error(name(),"Failed to get plan-id for diagnosis %s from cx environment",diag_id.c_str());
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished getting plan_id");

  if (!diag_env_initiate_wm_facts(plan_id)) {
    logger->log_error(name(),"Failed to initiate worldmodel for diagnosis environments");
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished initializing wm-facts");

  for (float hypo_id : hypothesis_ids) {
    diag_env_->add_diagnosis_hypothesis(hypo_id);
  }

  if (!diag_env_initiate_plan_actions()) {
    logger->log_error(name(),"Failed to initiate plan-actions for diagnosis environments");
    delete_diagnosis();
    return CLIPS::Value("FALSE", CLIPS::TYPE_SYMBOL);
  }
  logger->log_info(name(),"Finished initializing plan-actions");

  diag_env_->setup_finished();

  //TODO: Thats a bad way to wait for the environments to finish...
  while (!diag_env_->clips_init_finished()) {
  }

  return CLIPS::Value("TRUE", CLIPS::TYPE_SYMBOL);
}

/*
  Stop the diagnosis environment thread
*/
void
ClipsActiveDiagnosisThread::delete_diagnosis()
{
    diag_env_->wait_loop_done();
    thread_collector->remove(&*diag_env_);
}

/*
* Returns information gain for a given predicate. Predicate string is expected to have the
* format: predicate-name param-name param-value ....
*/
CLIPS::Value
ClipsActiveDiagnosisThread::information_gain(std::string grounded_predicate)
{
  std::vector<std::string> pred_splitted = str_split(grounded_predicate, " ");
  std::string predicate_name = pred_splitted[0];
  std::vector<std::string> key_args;
  for (size_t i = 1; i < pred_splitted.size(); ++i)
  {
    key_args.push_back(pred_splitted[i]);
  }
  return CLIPS::Value(diag_env_->information_gain(predicate_name,key_args));
}


/*
  Integrates a sensor measurement by removing all diagnosis hypothesis that are contradicted by the measurement
*/
CLIPS::Value
ClipsActiveDiagnosisThread::integrate_measurement(int pos, std::string predicate, CLIPS::Values param_names, CLIPS::Values param_values)
{
  int valid_diags = diag_env_->integrate_sensing_result((bool)pos,predicate,param_names,param_values);
  logger->log_info(name(),"Still %d hypotheses left after integrating %s",valid_diags,predicate.c_str());
  return CLIPS::Value(valid_diags);
}

/*
  Calculates all wm-facts that are true in all hypotheses. Retracts all wm-facts from the executive environment which are not
  part of these.
*/
CLIPS::Value
ClipsActiveDiagnosisThread::update_common_knowledge()
{ 
  auto clips = envs_[env_name_];
  fact_occurences_.clear();

  // World model facts of diagnosis environment with the "sub environment" as second pair member
  std::vector<std::pair<std::string,std::string>> diag_facts = diag_env_->get_fact_strings();

  //Number of valid diagnosis candidates
  int max = 0;

  for (std::pair<std::string,std::string> diag_fact : diag_facts)
  {
    if (diag_fact.second == "DEFAULT") {
      continue;
    }
    if (fact_occurences_.find(diag_fact.first) == fact_occurences_.end()) {
      fact_occurences_[diag_fact.first] = 1;
    } else {
      fact_occurences_[diag_fact.first]++;
    }
    if (fact_occurences_[diag_fact.first] > max) max = fact_occurences_[diag_fact.first];
  }


  // Remove all domain facts from the current environment that are not true in all diagnosis hypotheses
  CLIPS::Fact::pointer cx_fact_ptr = clips->get_facts();
  std::vector<std::string> cx_facts;
  while (cx_fact_ptr)
  {   
    if (cx_fact_ptr->get_template()->name() == "wm-fact")
    {
      std::string cx_fact_string = cx_fact_ptr->slot_value("id")[0].as_string();
      cx_facts.push_back(cx_fact_string);
      if (cx_fact_string.find("/domain/fact") != std::string::npos && 
              (fact_occurences_.find(cx_fact_string) == fact_occurences_.end() ||
              fact_occurences_[cx_fact_string] != max))
      {
        logger->log_debug(name(),"Retract: %s",cx_fact_string.c_str());
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

  //Assert all facts that are true in all diagnosis hypotheses and not already in the current environment
  std::map<std::string,int>::iterator it_count = fact_occurences_.begin();
  for (;it_count != fact_occurences_.end(); it_count++)
  {
    if (it_count->second == max && 
            std::find(cx_facts.begin(), cx_facts.end(), it_count->first) == cx_facts.end())
    {
      logger->log_debug(name(),"Assert: %s",it_count->first.c_str());

      CLIPS::Template::pointer wm_fact = clips->get_template("wm-fact");
	    if (wm_fact) {
		    CLIPS::Fact::pointer tmp = CLIPS::Fact::create(**clips,wm_fact);

	    	tmp->set_slot("id",CLIPS::Value(it_count->first,CLIPS::TYPE_STRING));
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
  }
  logger->log_info(name(),"Updating done"); 
  return CLIPS::Value("TRUE",CLIPS::TYPE_SYMBOL);
}
