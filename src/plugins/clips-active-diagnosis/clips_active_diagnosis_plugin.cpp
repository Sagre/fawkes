/***************************************************************************
 *  clips_active_diagnosis_plugin.cpp - clips_active_diagnosis
 *
 *  Plugin created: Mon Apr 19 15:41:47 2019

 *  Copyright  2019 Daniel Habering
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

#include <core/plugin.h>

#include "clips_active_diagnosis_thread.h"

using namespace fawkes;

/** @class ClipsActiveDiagnosisPlugin "clips_active_diagnosis_plugin.cpp"
 * CLIPS feature for active diagnosis
 * @author Daniel Habering
 */
class ClipsActiveDiagnosisPlugin : public fawkes::Plugin
{
 public:
  /** Constructor
   * @param config Fakwes configuration
   */
  explicit ClipsActiveDiagnosisPlugin(Configuration *config)
     : Plugin(config)
  {
     thread_list.push_back(new ClipsActiveDiagnosisThread());
  }
};

PLUGIN_DESCRIPTION("CLIPS feature to perform active diagnosis")
EXPORT_PLUGIN(ClipsActiveDiagnosisPlugin)
