
/***************************************************************************
 *  node_thread.cpp - Gazebo node handle providing Thread
 *
 *  Created: Fri Aug 24 11:04:04 2012
 *  Author  Bastian Klingen, Frederik Zwilling (2013)
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

#include "node_thread.h"

// from Gazebo
#include <gazebo/transport/TransportIface.hh>
#include <gazebo/transport/TransportTypes.hh>
#include <gazebo/transport/Node.hh>
#include <gazebo/gazebo_config.h>
#include <google/protobuf/message.h>
#include <gazebo/msgs/msgs.hh>


using namespace fawkes;

/** @class GazeboNodeThread "node_thread.h"
 * Gazebo node handle thread.
 * This thread maintains a Gazebo node which can be used by other
 * threads and is provided via the GazeboAspect.
 *
 * @author Bastian Klingen, Frederik Zwilling
 */

/** Constructor. */
GazeboNodeThread::GazeboNodeThread()
  : Thread("GazeboNodeThread", Thread::OPMODE_WAITFORWAKEUP),
    BlockedTimingAspect(BlockedTimingAspect::WAKEUP_HOOK_POST_LOOP),
    AspectProviderAspect(&gazebo_aspect_inifin_)
{
}


/** Destructor. */
GazeboNodeThread::~GazeboNodeThread()
{
}


void
GazeboNodeThread::init()
{
  //read config values
  robot_channel = config->get_string("/gazsim/world-name")
    + "/" + config->get_string("/gazsim/robot-name");
  
  world_name = config->get_string("/gazsim/world-name");

  if(gazebo::transport::is_stopped()) {
    gazebo::transport::init();
    gazebo::transport::run();
  }
  else {
    logger->log_warn(name(), "Gazebo already running ");
  }

  //Initialize Communication nodes:
  //the common one for the robot
  gazebo::transport::NodePtr node(new gazebo::transport::Node());
  gazebonode_ = node;
  //initialize node (this node only communicates with nodes that were initialized with the same string)
  gazebonode_->Init(robot_channel.c_str());
  gazebo_aspect_inifin_.set_gazebonode(gazebonode_);
  
  //and the node for world change messages
  gazebo_world_node_ = gazebo::transport::NodePtr(new gazebo::transport::Node());
  gazebo_world_node_->Init(world_name.c_str());
  gazebo_aspect_inifin_.set_gazebo_world_node(gazebo_world_node_);
}


void
GazeboNodeThread::finalize()
{
  gazebonode_->Fini();
  gazebonode_.reset();
  gazebo_aspect_inifin_.set_gazebonode(gazebonode_);
  gazebo_world_node_->Fini();
  gazebo_world_node_.reset();
  gazebo_aspect_inifin_.set_gazebonode(gazebo_world_node_);
}


void
GazeboNodeThread::loop()
{
}
