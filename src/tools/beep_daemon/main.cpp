
/***************************************************************************
 *  main.cpp - Fawkes main application
 *
 *  Created: Sun Apr 11 19:34:09 2010
 *  Copyright  2006-2010  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

#include "beep.h"

#include <blackboard/remote.h>
#include <core/threading/thread.h>
#include <interfaces/SwitchInterface.h>
#include <utils/system/argparser.h>
#include <utils/system/signal.h>
#include <utils/time/time.h>

#include <cmath>
#include <cstdio>
#include <unistd.h>
#ifdef HAVE_LIBDAEMON
#	include <libdaemon/dfork.h>
#	include <libdaemon/dlog.h>
#	include <libdaemon/dpid.h>
#	include <sys/stat.h>
#	include <sys/wait.h>

#	include <cerrno>
#	include <cstring>
#endif

using namespace std;
using namespace fawkes;

/** Fawkes beep daemon.
 *
 * @author Tim Niemueller
 */
class FawkesBeepDaemon : public Thread, public SignalHandler
{
public:
	/** Constructor. */
	FawkesBeepDaemon() : Thread("FawkesBeepDaemon", Thread::OPMODE_CONTINUOUS)
	{
		until_     = NULL;
		bb_        = NULL;
		switch_if_ = NULL;
	}

	virtual void
	loop()
	{
		while (!(bb_ && bb_->is_alive() && switch_if_->is_valid())) {
			if (bb_) {
				printf("Lost connection to blackboard\n");
				bb_->close(switch_if_);
				delete bb_;
				bb_ = NULL;
			}
			try {
				printf("Trying to connect to remote BB...");
				bb_        = new RemoteBlackBoard("localhost", 1910);
				switch_if_ = bb_->open_for_writing<SwitchInterface>("Beep");
				printf("succeeded\n");
			} catch (Exception &e) {
				printf("failed\n");
				delete bb_;
				bb_ = NULL;
				sleep(5);
			}
		}

		if (until_) {
			Time now;
			if ((now - until_) >= 0) {
				beep_.beep_off();
				delete until_;
				until_ = NULL;
			}
		}

		while (!switch_if_->msgq_empty()) {
			if (switch_if_->msgq_first_is<SwitchInterface::SetMessage>()) {
				SwitchInterface::SetMessage *msg = switch_if_->msgq_first<SwitchInterface::SetMessage>();
				if (msg->value() > 0.0) {
					beep_.beep_on(msg->value());
				} else if (msg->is_enabled()) {
					beep_.beep_on();
				} else {
					beep_.beep_off();
				}

			} else if (switch_if_->msgq_first_is<SwitchInterface::EnableDurationMessage>()) {
				SwitchInterface::EnableDurationMessage *msg =
				  switch_if_->msgq_first<SwitchInterface::EnableDurationMessage>();
				float duration = fabs(msg->duration());
				float value    = fabs(msg->value());

				delete until_;
				until_ = new Time();
				*until_ += duration;
				beep_.beep_on(value);
			} else if (switch_if_->msgq_first_is<SwitchInterface::EnableSwitchMessage>()) {
				beep_.beep_on();
			} else if (switch_if_->msgq_first_is<SwitchInterface::DisableSwitchMessage>()) {
				beep_.beep_off();
			}

			switch_if_->msgq_pop();
		}

		usleep(10000);
	}

	/** Handle signals.
   * @param signum signal number
   */
	void
	handle_signal(int signum)
	{
		this->cancel();
	}

private:
	BeepController   beep_;
	BlackBoard *     bb_;
	SwitchInterface *switch_if_;

	Time *until_;
};

void
usage(const char *progname)
{
}

#ifdef HAVE_LIBDAEMON
void
daemonize_cleanup()
{
	daemon_retval_send(-1);
	daemon_retval_done();
	daemon_pid_file_remove();
}

pid_t
daemonize(int argc, char **argv)
{
	pid_t  pid;
	mode_t old_umask = umask(0);

	// Prepare for return value passing
	daemon_retval_init();

	// Do the fork
	if ((pid = daemon_fork()) < 0) {
		return -1;

	} else if (pid) { // the parent
		int ret;

		// Wait for 20 seconds for the return value passed from the daemon process
		if ((ret = daemon_retval_wait(20)) < 0) {
			daemon_log(LOG_ERR, "Could not recieve return value from daemon process.");
			return -1;
		}

		if (ret != 0) {
			daemon_log(LOG_ERR, "*** Daemon startup failed, see syslog for details. ***");
			switch (ret) {
			case 1: daemon_log(LOG_ERR, "Daemon failed to close file descriptors"); break;
			case 2: daemon_log(LOG_ERR, "Daemon failed to create PID file"); break;
			}
			return -1;
		} else {
			return pid;
		}

	} else { // the daemon
#	ifdef DAEMON_CLOSE_ALL_AVAILABLE
		if (daemon_close_all(-1) < 0) {
			daemon_log(LOG_ERR, "Failed to close all file descriptors: %s", strerror(errno));
			// Send the error condition to the parent process
			daemon_retval_send(1);
			return -1;
		}
#	endif

		// Create the PID file
		if (daemon_pid_file_create() < 0) {
			printf("Could not create PID file (%s).", strerror(errno));
			daemon_log(LOG_ERR, "Could not create PID file (%s).", strerror(errno));

			// Send the error condition to the parent process
			daemon_retval_send(2);
			return -1;
		}

		// Send OK to parent process
		daemon_retval_send(0);

		daemon_log(LOG_INFO, "Sucessfully started");

		umask(old_umask);
		return 0;
	}
}

/** Global variable containing the path to the PID file.
 * unfortunately needed for libdaemon */
const char *fawkes_pid_file;

/** Function that returns the PID file name.
 * @return PID file name
 */
const char *
fawkes_daemon_pid_file_proc()
{
	return fawkes_pid_file;
}
#endif // HAVE_LIBDAEMON

/** Fawkes application.
 * @param argc argument count
 * @param argv array of arguments
 */
int
main(int argc, char **argv)
{
	ArgumentParser *argp = new ArgumentParser(argc, argv, "hD::ks");

#ifdef HAVE_LIBDAEMON
	pid_t pid;

	if (argp->has_arg("D")) {
		// Set identification string for the daemon for both syslog and PID file
		daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);
		if (argp->arg("D") != NULL) {
			fawkes_pid_file      = argp->arg("D");
			daemon_pid_file_proc = fawkes_daemon_pid_file_proc;
		}

		// We should daemonize, check if we were called to kill a daemonized copy
		if (argp->has_arg("k")) {
			// Check that the daemon is not run twice a the same time
			if ((pid = daemon_pid_file_is_running()) < 0) {
				daemon_log(LOG_ERR, "Fawkes daemon not running.");
				return 1;
			}

			// Kill daemon with SIGINT
			int ret;
			if ((ret = daemon_pid_file_kill_wait(SIGINT, 5)) < 0) {
				daemon_log(LOG_WARNING, "Failed to kill daemon");
			}
			return (ret < 0) ? 1 : 0;
		}

		if (argp->has_arg("s")) {
			// Check daemon status
			return (daemon_pid_file_is_running() < 0);
		}

		// Check that the daemon is not run twice a the same time
		if ((pid = daemon_pid_file_is_running()) >= 0) {
			daemon_log(LOG_ERR, "Daemon already running on (PID %u)", pid);
			return 201;
		}

		pid = daemonize(argc, argv);
		if (pid < 0) {
			daemonize_cleanup();
			return 201;
		} else if (pid) {
			// parent
			return 0;
		} // else child, continue as usual
	}
#else
	if (argp->has_arg("D")) {
		printf("Daemonizing support is not available.\n"
		       "(libdaemon[-devel] was not available at compile time)\n");
		return 202;
	}
#endif

	Thread::init_main();

	if (argp->has_arg("h")) {
		usage(argv[0]);
		delete argp;
		return 0;
	}

	FawkesBeepDaemon beepd;
	SignalManager::register_handler(SIGINT, &beepd);
	SignalManager::register_handler(SIGTERM, &beepd);

	beepd.start();
	beepd.join();

	Thread::destroy_main();

#ifdef HAVE_LIBDAEMON
	if (argp->has_arg("D")) {
		daemonize_cleanup();
	}
#endif

	delete argp;
	return 0;
}
