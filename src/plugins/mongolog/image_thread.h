
/***************************************************************************
 *  image_thread.h - Thread to log images to MongoDB
 *
 *  adapted from ros/image_thread.h
 *
 *  Created: Tue Apr 10 22:12:27 2012
 *  Copyright  2011-2012  Tim Niemueller [www.niemueller.de]
 *  Modified: Thu Jul 12 10:00:00 2012 by Bastian Klingen
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

#ifndef __PLUGINS_MONGOLOG_IMAGE_THREAD_H_
#define __PLUGINS_MONGOLOG_IMAGE_THREAD_H_

#include <core/threading/thread.h>
#include <aspect/blocked_timing.h>
#include <aspect/clock.h>
#include <aspect/configurable.h>
#include <aspect/logging.h>
#include <aspect/pointcloud.h>
#include <plugins/mongodb/aspect/mongodb.h>
#include <core/threading/mutex.h>

#include <list>
#include <queue>
#include <set>
#include <string>

namespace firevision {
  class SharedMemoryImageBuffer;
}

class MongoLogImagesThread
: public fawkes::Thread,
  public fawkes::ClockAspect,
  public fawkes::LoggingAspect,
  public fawkes::ConfigurableAspect,
  public fawkes::BlockedTimingAspect,
  public fawkes::MongoDBAspect
{
 public:
  MongoLogImagesThread();
  virtual ~MongoLogImagesThread();

  virtual void init();
  virtual void loop();
  virtual void finalize();

 /** Stub to see name in backtrace for easier debugging. @see Thread::run() */
 protected: virtual void run() { Thread::run(); }

 private:
  void update_images();
  void get_sets(std::set<std::string> &missing_images,
                std::set<std::string> &unbacked_images);

 private:
  /// @cond INTERNALS
  typedef struct {
    fawkes::Time                         last_sent;
    firevision::SharedMemoryImageBuffer *img;
  } ImageInfo;
  /// @endcond
  std::map<std::string, ImageInfo> imgs_;

  fawkes::Time *last_update_;
  fawkes::Time *now_;
  mongo::DBClientBase *__mongodb;
  std::string          __collection;
  std::string          __database;
};

#endif
