
/***************************************************************************
 *  min_merge.cpp - Laser min merge data filter
 *
 *  Created: Wed Mar 16 21:46:36 2011
 *  Copyright  2006-2011  Tim Niemueller [www.niemueller.de]
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

#include "min_merge.h"

#include <core/exception.h>
#include <utils/time/time.h>
#include <logging/logger.h>
#include <cstring>

/** @class LaserMinMergeDataFilter "min_merge.h"
 * Merge multiple laser data arrays into one.
 * For each value in the output array takes the minimum value of all input
 * arrays.
 * @author Tim Niemueller
 */

/** Constructor.
 * @param filter_name name of this filter instance
 * @param logger logger
 * @param in_data_size number of entries input value arrays
 * @param in vector of input arrays
 */
LaserMinMergeDataFilter::LaserMinMergeDataFilter(const std::string filter_name,
                                                 fawkes::Logger *logger,
                                                 unsigned int in_data_size,
                                                 std::vector<LaserDataFilter::Buffer *> &in)
	: LaserDataFilter(filter_name, in_data_size, in, 1),
    logger(logger),
    timestamp_selection_method_(TIMESTAMP_LATEST)
{
}

/** Constructor.
 * @param filter_name name of this filter instance
 * @param logger logger
 * @param in_data_size number of entries input value arrays
 * @param in vector of input arrays
 * @param timestamp_selection_method method to use for timestamp selection
 * @param timestamp_index if timestamp selection method is TIMESTAMP_INDEX this
 * is the index of the input buffer to choose the timestamp from
 */
LaserMinMergeDataFilter::LaserMinMergeDataFilter(const std::string filter_name,
                                                 fawkes::Logger *logger,
                                                 unsigned int in_data_size,
                                                 std::vector<LaserDataFilter::Buffer *> &in,
                                                 TimestampSelectionMethod timestamp_selection_method,
                                                 unsigned int timestamp_index)
	: LaserDataFilter(filter_name, in_data_size, in, 1),
    logger(logger),
    timestamp_selection_method_(timestamp_selection_method),
    timestamp_index_(timestamp_index)
{
  if (timestamp_index_ >= in.size()) {
    throw fawkes::Exception("min_merge timestamp index larger than number of input buffers");
  }
}


void
LaserMinMergeDataFilter::filter()
{
  const unsigned int vecsize = in.size();
  if (vecsize == 0)  return;

  out[0]->frame = in[0]->frame;

  copy_to_outbuf(out[0], in[0]);
  float *outbuf = out[0]->values;

  for (unsigned int a = 1; a < vecsize; ++a) {
    if (in[a]->frame != out[0]->frame) {
      throw fawkes::Exception("MinMerge frame mismatch: two frames with different frame IDs "
                              "(first has %s but input buffer %u has %s)",
                              out[0]->frame.c_str(), a, in[a]->frame.c_str());
    }
    float *inbuf  = in[a]->values;
    for (unsigned int i = 0; i < (const unsigned int)out_data_size; ++i) {
      if ( (outbuf[i] == 0) || ((inbuf[i] != 0) && (inbuf[i] < outbuf[i])) ) {
	outbuf[i] = inbuf[i];
      }
    }
  }

  if (timestamp_selection_method_ == TIMESTAMP_FIRST) {
    fawkes::Time first(in[0]->timestamp);
    for (unsigned int a = 1; a < vecsize; ++a) {
      if (*in[a]->timestamp < first) {
	first = in[a]->timestamp;
      }
    }
    out[0]->timestamp->set_time(first);
  } else if (timestamp_selection_method_ == TIMESTAMP_INDEX) {
    out[0]->timestamp->set_time(in[timestamp_index_]->timestamp);
 } else { // TIMESTAMP_LATEST
    fawkes::Time latest(in[0]->timestamp);
    for (unsigned int a = 1; a < vecsize; ++a) {
      if (*in[a]->timestamp > latest) {
	latest = in[a]->timestamp;
      }
    }
    out[0]->timestamp->set_time(latest);
  }
}
