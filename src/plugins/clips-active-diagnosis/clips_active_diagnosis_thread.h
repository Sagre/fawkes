
/***************************************************************************
 *  clips_active_diagnosis_thread.h - CLIPS feature for accessing the robot memory
 *
 *  Plugin created: Mon Aug 29 15:41:47 2016

 *  Copyright  2016  Frederik Zwilling
 *             2013  Tim Niemueller [www.niemueller.de]
 *
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

#ifndef _PLUGINS_CLIPS_ACTIVE_DIAGNOSISTHREAD_H_
#define _PLUGINS_CLIPS_ACTIVE_DIAGNOSISTHREAD_H_

#include <core/threading/thread.h>
#include <aspect/logging.h>
#include <aspect/configurable.h>
#include <plugins/clips/aspect/clips_feature.h>
#include <aspect/thread_producer.h>
#include <plugins/robot-memory/aspect/robot_memory_aspect.h>
#include "clips_diagnosis_env.h"
#include "clips_common_env.h"

#include <string>
#include <future>
#include <clipsmm.h>

namespace fawkes {
}

class ClipsActiveDiagnosisThread 
: public fawkes::Thread,
  public fawkes::LoggingAspect,
  public fawkes::ConfigurableAspect,
  public fawkes::CLIPSFeature,
  public fawkes::CLIPSFeatureAspect,
  public fawkes::RobotMemoryAspect,
  public fawkes::ThreadProducerAspect
{

 public:
  ClipsActiveDiagnosisThread();

  virtual void init();
  virtual void finalize();
  virtual void loop();

  // for CLIPSFeature
  virtual void clips_context_init(const std::string &env_name,
          fawkes::LockPtr<CLIPS::Environment> &clips);
  virtual void clips_context_destroyed(const std::string &env_name);

  std::vector<float> get_hypothesis_ids(const std::string &diag_id);
  std::string get_plan_id_from_diag_id(const std::string &diag_id);
  bool diag_env_initiate_wm_facts(const std::string &plan_id);
  bool diag_env_initiate_plan_actions(const std::string &diag_id);

  CLIPS::Value set_up_active_diagnosis(std::string diag_id);
  CLIPS::Value finalize_diagnosis();
  void delete_diagnosis();
  CLIPS::Value integrate_measurement(std::string fact, std::string value);
  CLIPS::Value get_sensing_action();
  
  std::string fact_to_string(CLIPS::Fact::pointer fact);
  std::string clips_value_to_string(CLIPS::Value val);

  /** Stub to see name in backtrace for easier debugging. @see Thread::run() */
  protected: virtual void run() { Thread::run(); }

 private:
  std::map<std::string, fawkes::LockPtr<CLIPS::Environment> >  envs_;
  std::shared_ptr<ClipsCommonEnvThread> common_env_;
  std::vector<std::shared_ptr<ClipsDiagnosisEnvThread>> diag_envs_;


 private:
 
};


#endif
