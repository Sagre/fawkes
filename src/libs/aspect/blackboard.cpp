
/***************************************************************************
 *  blackboard.h - BlackBoard aspect for Fawkes
 *
 *  Created: Thu Jan 11 16:28:58 2007
 *  Copyright  2006-2007  Tim Niemueller [www.niemueller.de]
 *
 *  $Id$
 *
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307, USA.
 */

#include <aspect/blackboard.h>

/** @class BlackBoardAspect aspect/blackboard.h
 * Thread aspect to access to BlackBoard.
 * Give this aspect to your thread to gain access to the BlackBoard.
 * It is guaranteed that if used properly from within plugins that the
 * blackboard member has been initialized properly.
 * @ingroup Aspects
 * @author Tim Niemueller
 */


/** @var BlackBoard *  BlackBoardAspect::blackboard
 * This is the BlackBoard instance you can use to interact with the
 * BlackBoard. It is set when the thread starts.
 */

/** Virtual empty destructor. */
BlackBoardAspect::~BlackBoardAspect()
{
}


/** Init BlackBoard aspect.
 * This set the BlackBoard interface manager that can be used to access the
 * BB.
 * It is guaranteed that this is called for a BlackBoardThread before start
 * is called (when running regularly inside Fawkes).
 * @param bb BlackBoard to use
 */
void
BlackBoardAspect::init_BlackBoardAspect(BlackBoard *bb)
{
  blackboard = bb;
}
