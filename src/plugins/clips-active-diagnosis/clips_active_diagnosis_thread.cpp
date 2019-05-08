
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

using namespace fawkes;

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
  return CLIPS::Value(true);
}

/*
  Integrates the "true" diagnosis hypothesis into the world model, if there is only one left
  Returns FALSE otherwise
*/
CLIPS::Value
ClipsActiveDiagnosisThread::finalize_diagnosis()
{
  return CLIPS::Value(false);
}

/*
  Integrates a sensor measurement by removing all diagnosis hypothesis that are contradicted by the measurement
*/
CLIPS::Value
ClipsActiveDiagnosisThread::integrate_measurement(std::string fact, std::string value)
{
  return CLIPS::Value(true);
}


/*
  Selects the next action according to the current state of each diagnosis environment
*/
CLIPS::Value
ClipsActiveDiagnosisThread::get_sensing_action() 
{
/*
    try {
      auto env_thread_ptr = std::make_shared<ClipsDiagnosisEnvThread>("test_diag");
      diag_envs_.push_back(env_thread_ptr);
      thread_collector->add(&(*env_thread_ptr));

    } catch (fawkes::CannotInitializeThreadException &e) {
      logger->log_error(name(),"Cannot start diagnosis environment: %s",e.what());
    }
*/
    return CLIPS::Value("test-action");
}