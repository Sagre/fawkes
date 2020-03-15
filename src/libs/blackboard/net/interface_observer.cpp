
/***************************************************************************
 *  interface_observer.cpp - BlackBoard interface observer for net handler
 *
 *  Created: Wed Mar 02 17:05:29 2011
 *  Copyright  2007-2011  Tim Niemueller [www.niemueller.de]
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

#include <blackboard/blackboard.h>
#include <blackboard/net/interface_observer.h>
#include <blackboard/net/messages.h>
#include <logging/liblogger.h>
#include <netcomm/fawkes/component_ids.h>
#include <netcomm/fawkes/hub.h>

#include <cstdlib>
#include <cstring>

namespace fawkes {

/** @class BlackBoardNetHandlerInterfaceObserver <blackboard/net/interface_observer.h>
 * Interface observer for blackboard network handler.
 * This class is used by the BlackBoardNetworkHandler to track interface events (creation
 * and destruction) and broadcast them to everybody listening.
 * @author Tim Niemueller
 */

/** Constructor.
 * @param blackboard local BlackBoard
 * @param hub Fawkes network hub to use to send messages
 */
BlackBoardNetHandlerInterfaceObserver::BlackBoardNetHandlerInterfaceObserver(BlackBoard *blackboard,
                                                                             FawkesNetworkHub *hub)
{
	blackboard_ = blackboard;
	fnh_        = hub;

	bbio_add_observed_create("*", "*");
	bbio_add_observed_destroy("*", "*");

	blackboard_->register_observer(this);
}

/** Destructor. */
BlackBoardNetHandlerInterfaceObserver::~BlackBoardNetHandlerInterfaceObserver()
{
	blackboard_->unregister_observer(this);
}

/** Broadcast event.
 * @param msg_id message ID to use
 * @param type interface type
 * @param id interface ID
 */
void
BlackBoardNetHandlerInterfaceObserver::send_event(unsigned int msg_id,
                                                  const char * type,
                                                  const char * id)
{
	bb_ievent_msg_t *esm = (bb_ievent_msg_t *)malloc(sizeof(bb_ievent_msg_t));
	strncpy(esm->type, type, INTERFACE_TYPE_SIZE_ - 1);
	strncpy(esm->id, id, INTERFACE_ID_SIZE_ - 1);

	try {
		fnh_->broadcast(FAWKES_CID_BLACKBOARD, msg_id, esm, sizeof(bb_ievent_msg_t));
	} catch (Exception &e) {
		LibLogger::log_warn("BlackBoardNetHandlerInterfaceObserver",
		                    "Failed to send BlackBoard event (%s), exception follows",
		                    (msg_id == MSG_BB_INTERFACE_CREATED) ? "create" : "destroy");
		LibLogger::log_warn("BlackBoardNetHandlerInterfaceObserver", e);
	}
}

void
BlackBoardNetHandlerInterfaceObserver::bb_interface_created(const char *type,
                                                            const char *id) throw()
{
	send_event(MSG_BB_INTERFACE_CREATED, type, id);
}

void
BlackBoardNetHandlerInterfaceObserver::bb_interface_destroyed(const char *type,
                                                              const char *id) throw()
{
	send_event(MSG_BB_INTERFACE_DESTROYED, type, id);
}

} // end namespace fawkes
