/***************************************************************************
 *  navgraph_stconstr_thread.cpp - static constraints for navgraph
 *
 *  Created: Fri Jul 11 17:34:18 2014
 *  Copyright  2012-2014  Tim Niemueller [www.niemueller.de]
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

#include "navgraph_stconstr_thread.h"

#include <plugins/navgraph/constraints/static_list_node_constraint.h>
#include <plugins/navgraph/constraints/static_list_edge_constraint.h>
#include <utils/misc/string_split.h>

using namespace fawkes;

/** @class NavGraphStaticConstraintsThread "navgraph_stconstr_thread.h"
 * Thread to statically block certain nodes from config.
 * @author Tim Niemueller
 */

/** Constructor. */
NavGraphStaticConstraintsThread::NavGraphStaticConstraintsThread()
  : Thread("NavGraphStaticConstraintsThread", Thread::OPMODE_WAITFORWAKEUP)
{
}

/** Destructor. */
NavGraphStaticConstraintsThread::~NavGraphStaticConstraintsThread()
{
}

void
NavGraphStaticConstraintsThread::init()
{
  std::vector<std::string> nodes =
    config->get_strings("/plugins/navgraph/static-constraints/nodes");

  std::vector<std::string> c_edges =
    config->get_strings("/plugins/navgraph/static-constraints/edges");

  std::vector<std::pair<std::string, std::string>> edges;
  for (std::string ce : c_edges) {
    std::vector<std::string> node_names = str_split(ce, "--");
    if (node_names.size() == 2) {
      edges.push_back(std::make_pair(node_names[0], node_names[1]));
    }
  }
  
  node_constraint_ = new NavGraphStaticListNodeConstraint("static-nodes");
  edge_constraint_ = new NavGraphStaticListEdgeConstraint("static-edges");

  const std::vector<TopologicalMapNode> &graph_nodes = navgraph->nodes();

  std::list<std::string> missing_nodes;
  for (std::string node_name : nodes) {
    bool found = false;
    for (const TopologicalMapNode &gnode : graph_nodes) {
      if (gnode.name() == node_name) {
	node_constraint_->add_node(gnode);
	found = true;
	break;
      }
    }

    if (!found) {
      missing_nodes.push_back(node_name);
    }
  }

  if (! missing_nodes.empty()) {
    std::list<std::string>::iterator n = missing_nodes.begin();
    std::string err_str = *n++;
    for (;n != missing_nodes.end(); ++n) {
      err_str += ", " + *n;
    }

    delete node_constraint_;
    delete edge_constraint_;
    throw Exception("Some block nodes are not in graph: %s", err_str.c_str());
  }

  const std::vector<TopologicalMapEdge> &graph_edges = navgraph->edges();


  std::list<std::pair<std::string, std::string>> missing_edges;
  for (std::pair<std::string, std::string> edge : edges) {
    bool found = false;
    for (const TopologicalMapEdge &gedge : graph_edges) {
      if ((edge.first == gedge.from() && edge.second == gedge.to()) ||
	  (edge.first == gedge.to() && edge.second == gedge.from()))
      {
	edge_constraint_->add_edge(gedge);
	found = true;
	break;
      }
    }

    if (!found) {
      missing_edges.push_back(edge);
    }
  }

  if (! missing_edges.empty()) {
    std::list<std::pair<std::string, std::string>>::iterator n = missing_edges.begin();
    std::string err_str = n->first + "--" + n->second;
    for (++n ; n != missing_edges.end(); ++n) {
      err_str += ", " + n->first + "--" + n->second;
    }

    delete node_constraint_;
    delete edge_constraint_;
    throw Exception("Some block nodes are not in graph: %s", err_str.c_str());
  }

  constraint_repo->register_constraint(node_constraint_);
  constraint_repo->register_constraint(edge_constraint_);
}

void
NavGraphStaticConstraintsThread::finalize()
{
  constraint_repo->unregister_constraint(node_constraint_->name());
  constraint_repo->unregister_constraint(edge_constraint_->name());
  delete node_constraint_;
  delete edge_constraint_;
}

void
NavGraphStaticConstraintsThread::loop()
{
}
