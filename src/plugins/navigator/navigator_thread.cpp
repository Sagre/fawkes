
/***************************************************************************
 *  navigator_thread.cpp - Navigator Thread
 *
 *  Created: Thu May 31 18:36:55 2007
 *  Copyright  2007  Martin Liebenberg
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

#include <plugins/navigator/navigator_thread.h>
#include <interfaces/navigator.h>
#include <interfaces/motor.h>
#include <interfaces/object.h>
#include <core/threading/mutex.h>

#include <cmath>
#include <unistd.h>

/** @class NavigatorThread <plugins/navigator/navigator_thread.h>
 * Navigator thread.
 * Navigator functional thread.
 * @author Martin Liebenberg
 */

/** Contructor. */
NavigatorThread::NavigatorThread()
    : Thread("NavigatorThread", Thread::OPMODE_WAITFORWAKEUP),
    BlockedTimingAspect(BlockedTimingAspect::WAKEUP_HOOK_ACT)
{
  logger_modulo_counter = 0;
  old_velocity_x = 0;
  old_velocity_y = 0;
  old_velocity_rotation = 0;
  ball_position_x = 100000.;
  ball_position_y = 100000.;
  ball_mutex = new Mutex();
}

/** Destructor. */
NavigatorThread::~NavigatorThread()
{
  delete ball_mutex;
}


void
NavigatorThread::finalize()
{
  try
    {
      interface_manager->close(navigator_interface);
      interface_manager->close(motor_interface);
      delete object_interface_list;
      //  interface_manager->close(object_interface);
    }
  catch (Exception& e)
    {
      logger->log_error("NavigatorThread", "Closing interface failed!");
      logger->log_error("NavigatorThread", e);
    }
}


void
NavigatorThread::init()
{
  try
    {
      navigator_interface = interface_manager->open_for_writing<NavigatorInterface>("Navigator");
    }
  catch (Exception& e)
    {
      e.append("%s initialization failed, could not open navigator interface for writing", name());
      logger->log_error("NavigatorThread", "Opening interface failed!");
      logger->log_error("NavigatorThread", e);
      throw;
    }

  try
    {
      motor_interface = interface_manager->open_for_reading<MotorInterface>("Motor");
    }
  catch (Exception& e)
    {
      e.append("%s initialization failed, could not open motor interface for reading", name());
      logger->log_error("NavigatorThread", "Opening interface for reading failed!");
      logger->log_error("NavigatorThread", e);
      throw;
    }

  try
    {
      object_interface_list = interface_manager->open_all_of_type_for_reading("ObjectPositionInterface");
    }
  catch (Exception& e)
    {
      e.append("%s initialization failed, could not open object interface for reading", name());
      logger->log_error("NavigatorThread", "Opening interface for reading failed!");
      logger->log_error("NavigatorThread", e);
      throw;
    }


  set_target_tolerance(config->get_float("/navigator/target_tolerance"));

}


void
NavigatorThread::once()
{
  MotorInterface::AcquireControlMessage *msg = new MotorInterface::AcquireControlMessage();
  motor_interface->msgq_enqueue(msg);
}


void
NavigatorThread::loop()
{
  motor_interface->read();

  if ( navigator_interface->msgq_first_is<NavigatorInterface::TargetMessage>() )
    {
      NavigatorInterface::TargetMessage* msg = navigator_interface->msgq_first<NavigatorInterface::TargetMessage>();

      logger->log_info("NavigatorThread", "target message received %f, %f", msg->x(), msg->y());

      if(motor_interface->controller_thread_id() == current_thread_id())
        {
          goTo_cartesian_ori(msg->x(), msg->y(), msg->orientation());
        }
      navigator_interface->msgq_pop();

    }
  else if ( navigator_interface->msgq_first_is<NavigatorInterface::MaxVelocityMessage>() )
    {
      NavigatorInterface::MaxVelocityMessage* msg = navigator_interface->msgq_first<NavigatorInterface::MaxVelocityMessage>();

      logger->log_info("NavigatorThread", "velocity message received %f", msg->velocity());
      // logger->log_info("NavigatorThread", "motor_interface->controller_thread_id() %i == %i current_thread_id()", motor_interface->controller_thread_id() ,  current_thread_id() );

      if(motor_interface->controller_thread_id() == current_thread_id())
        {
          //logger->log_info("NavigatorThread", "set velocity %f", msg->velocity());
          setVelocity(msg->velocity());
        }
      navigator_interface->msgq_pop();
    }
  else if ( navigator_interface->msgq_first_is<NavigatorInterface::ObstacleMessage>() )
    {
      NavigatorInterface::ObstacleMessage* msg = navigator_interface->msgq_first<NavigatorInterface::ObstacleMessage>();

      logger->log_info("NavigatorThread", "obstacle message received");

      if(motor_interface->controller_thread_id() == current_thread_id())
        {
          Obstacle o(msg->width(), msg->x(), msg->y(), 0.);
          add_obstacle(o);
        }
      navigator_interface->msgq_pop();
    }

  std::list<Interface *>::iterator i;
  for ( i = object_interface_list->begin(); i != object_interface_list->end(); ++i )
    {
      ObjectPositionInterface *object_interface = (ObjectPositionInterface *) *i;
      object_interface->read();

      //  logger->log_info("NavigatorThread", "Ball object_interface->is_visible() %i",object_interface->is_visible());
      if(object_interface->object_type() == ObjectPositionInterface::BALL && object_interface->is_visible())
        {
          double direction = atan2(object_interface->relative_y(), object_interface->relative_x());
          double before_ball_x = object_interface->relative_x() + 0.1 * cos(direction + M_PI);
          double before_ball_y = object_interface->relative_y() + 0.1 * sin(direction + M_PI);

          goTo_cartesian_ori(before_ball_x, before_ball_y, direction);

          ball_mutex->lock();
          ball_position_x = object_interface->relative_x();
          ball_position_y = object_interface->relative_y();
          ball_mutex->unlock();
          //  logger->log_info("NavigatorThread", "Ball at  %f, %f", object_interface->relative_x(), object_interface->relative_y());
        }
      /*   else
           {
             float distance = object_interface->distance();
             float yaw = object_interface->yaw();
             float width = object_interface->extent();
             logger->log_info("NavigatorThread", "Object at distance = %f, yaw = %f, width = %f", distance, yaw, width);
             std::vector<Obstacle> obstacle_list;
             obstacle_list.push_back(*(new Obstacle(width, distance * cos(yaw), distance * sin(yaw), 0)));
             setObstacles(obstacle_list);
           }*/
    }
  // logger->log_info("NavigatorThread", "motor_interface->v  %f, %f", motor_interface->vx(), motor_interface->vy());

  set_odometry_velocity_x(motor_interface->vx());
  set_odometry_velocity_y(motor_interface->vy());
  set_odometry_velocity_rotation(motor_interface->omega());


  //from navigator
  mainLoop();


  if(motor_interface->controller_thread_id() == current_thread_id())
    {
      double vx = getVelocityX();
      double vy = getVelocityY();
      double rotation = getVelocityRotation();

      if(old_velocity_x != vx || old_velocity_y != vy
          || old_velocity_rotation != rotation)
        {
          old_velocity_x = vx;
          old_velocity_y = vy;
          old_velocity_rotation = rotation;
          MotorInterface::LinTransRotMessage* motor_msg = new  MotorInterface::LinTransRotMessage(vx, vy, rotation);
          //          MotorInterface::TransMessage* motor_msg = new  MotorInterface::TransMessage(getVelocityX(), getVelocityY());
          motor_interface->msgq_enqueue(motor_msg);

          //  logger->log_info("NavigatorThread", "send x = %f, y = %f", getVelocityX(), getVelocityY());
        }
    }
  /*
  if((++logger_modulo_counter %= 10) == 0)
    {
      logger->log_info("NavigatorThread", "NavigatorThread called: %lu, %lu", motor_interface->getControllerID(), this->current_thread_id());
    }
  */
  //usleep(100000);
}

/** Returns the x coordinate of the relative ball position.
 * Is needed by NavigatorNetworkThread, which sends the position to the GUI.
 * @return x coordinate of the relative ball position
 */
double NavigatorThread::get_ball_position_x()
{
  double x = 0;
  ball_mutex->lock();
  x = ball_position_x;
  ball_mutex->unlock();
  return x;
}

/** Returns the y coordinate of the relative ball position.
 * Is needed by NavigatorNetworkThread, which sends the position to the GUI.
 * @return y coordinate of the relative ball position
 */
double NavigatorThread::get_ball_position_y()
{
  double y = 0;
  ball_mutex->lock();
  y = ball_position_y;
  ball_mutex->unlock();
  return y;
}
