
/***************************************************************************
 *  fawkes_network.h - Fawkes Fawkes Network Aspect initializer/finalizer
 *
 *  Created: Tue Nov 23 23:43:03 2010
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

#ifndef _ASPECT_INIFINS_FAWKES_NETWORK_H_
#define _ASPECT_INIFINS_FAWKES_NETWORK_H_

#include <aspect/inifins/inifin.h>

namespace fawkes {

class FawkesNetworkHub;

class FawkesNetworkAspectIniFin : public AspectIniFin
{
public:
	FawkesNetworkAspectIniFin(FawkesNetworkHub *hub);

	virtual void init(Thread *thread);
	virtual void finalize(Thread *thread);

private:
	FawkesNetworkHub *hub_;
};

} // end namespace fawkes

#endif
