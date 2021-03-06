
/***************************************************************************
 *  daemonize.h - Fawkes daemonization functions
 *
 *  Created: Wed May 04 23:32:25 2011
 *  Copyright  2006-2011  Tim Niemueller [www.niemueller.de]
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

#ifndef _LIBS_BASEAPP_DAEMONIZE_H_
#define _LIBS_BASEAPP_DAEMONIZE_H_


namespace fawkes {
  namespace daemon {

void init(const char *pidfile, const char *progname);
bool start();
bool running();
void kill();
void cleanup();

} // end namespace fawkes::daemon
} // end namespace fawkes

#endif
