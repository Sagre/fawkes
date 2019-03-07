
/***************************************************************************
 *  remote.h - Remote BlackBoard using the Fawkes network protocol
 *
 *  Created: Mon Mar 03 10:52:28 2008
 *  Copyright  2006-2015  Tim Niemueller [www.niemueller.de]
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

#ifndef _BLACKBOARD_REMOTE_H_
#define _BLACKBOARD_REMOTE_H_

#include <blackboard/blackboard.h>
#include <core/exceptions/software.h>
#include <core/utils/lock_map.h>
#include <netcomm/fawkes/client_handler.h>

#include <list>

namespace fawkes {

class FawkesNetworkClient;
class FawkesNetworkMessage;
class Mutex;
class WaitCondition;
class Interface;
class InterfaceInfoList;

class BlackBoardInstanceFactory;
class BlackBoardNotifier;
class BlackBoardInterfaceProxy;
class BlackBoardInterfaceListener;
class BlackBoardInterfaceObserver;

class RemoteBlackBoard : public BlackBoard, public FawkesNetworkClientHandler
{
public:
	RemoteBlackBoard(FawkesNetworkClient *client);
	RemoteBlackBoard(const char *hostname, unsigned short int port);
	virtual ~RemoteBlackBoard();

	virtual Interface *
	open_for_reading(const char *interface_type, const char *identifier, const char *owner = NULL);
	virtual Interface *
	             open_for_writing(const char *interface_type, const char *identifier, const char *owner = NULL);
	virtual void close(Interface *interface);

	virtual InterfaceInfoList *list_all();
	virtual InterfaceInfoList *list(const char *type_pattern, const char *id_pattern);
	virtual bool               is_alive() const throw();
	virtual bool               try_aliveness_restore() throw();

	std::list<Interface *> open_multiple_for_reading(const char *interface_type,
	                                                 const char *id_pattern = "*",
	                                                 const char *owner      = NULL);

	/* for FawkesNetworkClientHandler */
	virtual void deregistered(unsigned int id) throw();
	virtual void inbound_received(FawkesNetworkMessage *msg, unsigned int id) throw();
	virtual void connection_died(unsigned int id) throw();
	virtual void connection_established(unsigned int id) throw();

	/* extensions for RemoteBlackBoard */

private: /* methods */
	void open_interface(const char *type,
	                    const char *identifier,
	                    const char *owner,
	                    bool        writer,
	                    Interface * iface);
	Interface *
	     open_interface(const char *type, const char *identifier, const char *owner, bool writer);
	void reopen_interfaces();

private: /* members */
	Mutex *                                                     mutex_;
	FawkesNetworkClient *                                       fnc_;
	bool                                                        fnc_owner_;
	FawkesNetworkMessage *                                      m_;
	BlackBoardInstanceFactory *                                 instance_factory_;
	LockMap<unsigned int, BlackBoardInterfaceProxy *>           proxies_;
	LockMap<unsigned int, BlackBoardInterfaceProxy *>::iterator pit_;
	std::list<BlackBoardInterfaceProxy *>                       invalid_proxies_;
	std::list<BlackBoardInterfaceProxy *>::iterator             ipit_;

	Mutex *        wait_mutex_;
	WaitCondition *wait_cond_;

	const char *inbound_thread_;
};

} // end namespace fawkes

#endif
