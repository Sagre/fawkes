
/***************************************************************************
 *  visualization_thread.h - Visualization for pathplan via rviz
 *
 *  Created: Fri Nov 11 21:17:24 2011
 *  Copyright  2011-2012  Tim Niemueller [www.niemueller.de]
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

#ifndef __PLUGINS_VISPATHPLAN_VISPATHPLAN_THREAD_H_
#define __PLUGINS_VISPATHPLAN_VISPATHPLAN_THREAD_H_

#include <core/threading/thread.h>
#include <core/threading/mutex.h>
#include <core/utils/lockptr.h>
#include <aspect/configurable.h>
#include <aspect/logging.h>
#include <plugins/ros/aspect/ros.h>
#include <utils/graph/topological_map_node.h>

#include <ros/publisher.h>
#include <visualization_msgs/MarkerArray.h>

namespace fawkes {
  class TopologicalMapGraph;
  class ConstraintRepo;
}


class NavGraphVisualizationThread
: public fawkes::Thread,
  public fawkes::ConfigurableAspect,
  public fawkes::LoggingAspect,
  public fawkes::ROSAspect
{
 public:
  NavGraphVisualizationThread();

  virtual void init();
  virtual void loop();
  virtual void finalize();

  void set_graph(fawkes::LockPtr<fawkes::TopologicalMapGraph> &graph);
  void set_constraint_repo(fawkes::LockPtr<fawkes::ConstraintRepo> &crepo);

  void set_plan(std::vector<fawkes::TopologicalMapNode> plan);
  void set_current_edge(std::string from, std::string to);
  void reset_plan();

 private:
  void publish();
  void add_circle_markers(visualization_msgs::MarkerArray &m, size_t &id_num,
			  float center_x, float center_y, float radius, unsigned int arc_length,
			  float r, float g, float b, float alpha, float line_width = 0.03);

 private:
  size_t last_id_num_;
  size_t constraints_last_id_num_;
  ros::Publisher vispub_;

  std::vector<fawkes::TopologicalMapNode> plan_;
  std::string plan_to_;
  std::string plan_from_;

  fawkes::LockPtr<fawkes::TopologicalMapGraph> graph_;
  fawkes::LockPtr<fawkes::ConstraintRepo>      crepo_;
};


#endif
