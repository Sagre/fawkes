/***************************************************************************
 *  clips_active_diagnosis_thread.h - CLIPS-based active_diagnosis plugin
 *
 *  Created: Tue Sep 19 11:59:44 2017
 *  Copyright  2006-2017  Tim Niemueller [www.niemueller.de]
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

#ifndef _PLUGINS_CLIPS_ACTIVE_DIAGNOSIS_ENV_CLIPS_DIAGNOSIS_ENV_THREAD_H_
#define _PLUGINS_CLIPS_ACTIVE_DIAGNOSIS_CLIPS_DIAGNOSIS_ENV_THREAD_H_

#include <core/threading/thread.h>
#include <aspect/clock.h>
#include <aspect/logging.h>
#include <aspect/configurable.h>
#include <plugins/clips/aspect/clips.h>
#include <plugins/robot-memory/aspect/robot_memory_aspect.h>
#include <utils/time/time.h>

#include <clipsmm.h>
#include <memory>

namespace fawkes {
	class ActionSkillMapping;
}

class ClipsDiagnosisEnvThread
: public fawkes::Thread,
	public fawkes::LoggingAspect,
	public fawkes::ConfigurableAspect,
	public fawkes::ClockAspect,
	public fawkes::CLIPSAspect,
	public fawkes::RobotMemoryAspect
{
 public:
	ClipsDiagnosisEnvThread(std::string diag_id,float hypothesis_id);
	virtual ~ClipsDiagnosisEnvThread();

	virtual void init();
	virtual void loop();
	virtual void finalize();

	float get_hypothesis_id() {
		return hypothesis_id_;
	}
	std::string get_diag_id() {
		return diag_id_;
	}
	std::vector<std::string> get_fact_strings();

	bool clips_init_finished();
	void setup_finished();
	void add_wm_fact(std::string id);
	void add_plan_action(CLIPS::Fact::pointer pa_fact);

	bool vector_equal_to_wm_fact(std::vector<std::string> vec, CLIPS::Fact::pointer fact);
	bool valid_measurement_result(std::string predicate, CLIPS::Values param_names, CLIPS::Values param_values);

	/** Stub to see name in backtrace for easier debugging. @see Thread::run() */
 protected: virtual void run() { Thread::run(); }

 private:
    std::string diag_id_;
	float hypothesis_id_;
};

std::string wm_fact_to_string(CLIPS::Fact::pointer fact);
std::string clips_value_to_string(CLIPS::Value val);
#endif