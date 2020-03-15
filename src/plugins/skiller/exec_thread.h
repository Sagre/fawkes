
/***************************************************************************
 *  exec_thread.h - Fawkes Skiller: Execution Thread
 *
 *  Created: Mon Feb 18 10:28:38 2008
 *  Copyright  2006-2011  Tim Niemueller [www.niemueller.de]
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

#ifndef _PLUGINS_SKILLER_EXEC_THREAD_H_
#define _PLUGINS_SKILLER_EXEC_THREAD_H_

#include <aspect/blackboard.h>
#include <aspect/blocked_timing.h>
#include <aspect/clock.h>
#include <aspect/configurable.h>
#include <aspect/logging.h>
#include <core/threading/thread.h>
#ifdef HAVE_TF
#	include <aspect/tf.h>
#endif
#include <blackboard/interface_listener.h>
#include <blackboard/ownership.h>
#include <core/utils/lock_queue.h>
#include <lua/context_watcher.h>
#include <utils/system/fam.h>

#include <cstdlib>
#include <list>
#include <string>

namespace fawkes {
class ComponentLogger;
class Mutex;
class LuaContext;
class Interface;
class SkillerInterface;
class SkillerDebugInterface;
#ifdef SKILLER_TIMETRACKING
class TimeTracker;
#endif
} // namespace fawkes
class SkillerFeature;

class SkillerExecutionThread : public fawkes::Thread,
                               public fawkes::BlockedTimingAspect,
                               public fawkes::LoggingAspect,
                               public fawkes::BlackBoardAspect,
                               public fawkes::ConfigurableAspect,
                               public fawkes::ClockAspect,
#ifdef HAVE_TF
                               public fawkes::TransformAspect,
#endif
                               public fawkes::BlackBoardInterfaceListener,
                               public fawkes::LuaContextWatcher
{
public:
	SkillerExecutionThread();
	virtual ~SkillerExecutionThread();

	virtual void init();
	virtual void loop();
	virtual void finalize();

	void add_skiller_feature(SkillerFeature *feature);

	/* BlackBoardInterfaceListener */
	void bb_interface_reader_removed(fawkes::Interface *interface,
	                                 unsigned int       instance_serial) throw();

	// LuaContextWatcher
	void lua_restarted(fawkes::LuaContext *context);

	/** Stub to see name in backtrace for easier debugging. @see Thread::run() */
protected:
	virtual void
	run()
	{
		Thread::run();
	}

private: /* members */
	fawkes::ComponentLogger *        clog_;
	fawkes::BlackBoardWithOwnership *bbo_;

	// config values
	std::string cfg_skillspace_;
	bool        cfg_watch_files_;

	fawkes::LockQueue<unsigned int> skiller_if_removed_readers_;

	fawkes::SkillerInterface *skiller_if_;

	fawkes::LuaContext *lua_;

	std::list<SkillerFeature *> features_;
};

#endif
