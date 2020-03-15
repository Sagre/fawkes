
/***************************************************************************
 *  pcl_db_roscomm_thread.h - ROS communication for pcl-db-merge
 *
 *  Created: Thu Dec 06 13:52:27 2012
 *  Copyright  2012-2013  Tim Niemueller [www.niemueller.de]
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

#ifndef _PLUGINS_PERCEPTION_PCL_DB_ROS_PCL_DB_ROSCOMM_THREAD_H_
#define _PLUGINS_PERCEPTION_PCL_DB_ROS_PCL_DB_ROSCOMM_THREAD_H_

#include <aspect/blackboard.h>
#include <aspect/blocked_timing.h>
#include <aspect/configurable.h>
#include <aspect/logging.h>
#include <aspect/pointcloud.h>
#include <core/threading/thread.h>
#include <fawkes_msgs/MergePointClouds.h>
#include <fawkes_msgs/RecordData.h>
#include <fawkes_msgs/RetrievePointCloud.h>
#include <fawkes_msgs/StorePointCloud.h>
#include <plugins/ros/aspect/ros.h>

namespace fawkes {
class PclDatabaseMergeInterface;
class PclDatabaseRetrieveInterface;
class PclDatabaseStoreInterface;
class BlackBoardOnUpdateWaker;
class WaitCondition;
} // namespace fawkes
namespace ros {
class ServiceServer;
}

class PointCloudDBROSCommThread : public fawkes::Thread,
                                  public fawkes::LoggingAspect,
                                  public fawkes::ConfigurableAspect,
                                  public fawkes::BlackBoardAspect,
                                  public fawkes::BlockedTimingAspect,
                                  public fawkes::ROSAspect,
                                  public fawkes::PointCloudAspect
{
public:
	PointCloudDBROSCommThread();
	virtual ~PointCloudDBROSCommThread();

	virtual void init();
	virtual void loop();
	virtual void finalize();

private:
	bool merge_cb(fawkes_msgs::MergePointClouds::Request & req,
	              fawkes_msgs::MergePointClouds::Response &resp);
	bool retrieve_cb(fawkes_msgs::RetrievePointCloud::Request & req,
	                 fawkes_msgs::RetrievePointCloud::Response &resp);
	bool store_cb(fawkes_msgs::StorePointCloud::Request & req,
	              fawkes_msgs::StorePointCloud::Response &resp);
	bool record_cb(fawkes_msgs::RecordData::Request &req, fawkes_msgs::RecordData::Response &resp);

	/** Stub to see name in backtrace for easier debugging. @see Thread::run() */
protected:
	virtual void
	run()
	{
		Thread::run();
	}

private: // members
	fawkes::PclDatabaseMergeInterface *   merge_if_;
	fawkes::PclDatabaseRetrieveInterface *retrieve_if_;
	fawkes::PclDatabaseStoreInterface *   store_if_;

	fawkes::BlackBoardOnUpdateWaker *merge_update_waker_;
	fawkes::WaitCondition *          merge_waitcond_;

	fawkes::BlackBoardOnUpdateWaker *retrieve_update_waker_;
	fawkes::WaitCondition *          retrieve_waitcond_;

	fawkes::BlackBoardOnUpdateWaker *store_update_waker_;
	fawkes::WaitCondition *          store_waitcond_;

	ros::ServiceServer *srv_merge_;
	ros::ServiceServer *srv_retrieve_;
	ros::ServiceServer *srv_store_;
	ros::ServiceServer *srv_record_;

	unsigned int merge_msg_id_;
	unsigned int retrieve_msg_id_;
	unsigned int store_msg_id_;

	std::string cfg_store_pcl_id_;
};

#endif
