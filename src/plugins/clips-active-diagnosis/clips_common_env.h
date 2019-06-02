/***************************************************************************
 *  clips_common_env.h - CLIPS-based active_diagnosis plugin
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

#ifndef _PLUGINS_CLIPS_COMMON_ENV_CLIPS_COMMON_THREAD_H_
#define _PLUGINS_CLIPS_COMMON_ENV_CLIPS_COMMON_THREAD_H_

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

class ClipsCommonEnvThread
: public fawkes::Thread,
	public fawkes::LoggingAspect,
	public fawkes::ConfigurableAspect,
	public fawkes::ClockAspect,
	public fawkes::CLIPSAspect,
	public fawkes::RobotMemoryAspect
{
 public:
	ClipsCommonEnvThread();
	virtual ~ClipsCommonEnvThread();

	virtual void init();
	virtual void loop();
	virtual void finalize();

	void add_wm_fact(std::string id);

	/** Stub to see name in backtrace for easier debugging. @see Thread::run() */
 protected: virtual void run() { Thread::run(); }

 private:
};

#endif
