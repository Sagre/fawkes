
/***************************************************************************
 *  deadspots.h - Laser data dead spots filter
 *
 *  Created: Wed Jun 24 22:39:19 2009
 *  Copyright  2006-2009  Tim Niemueller [www.niemueller.de]
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

#ifndef _PLUGINS_LASER_FILTER_FILTERS_DEADSPOTS_H_
#define _PLUGINS_LASER_FILTER_FILTERS_DEADSPOTS_H_

#include "filter.h"

#include <string>
#include <utility>
#include <vector>

namespace fawkes {
class Configuration;
class Logger;
} // namespace fawkes

class LaserDeadSpotsDataFilter : public LaserDataFilter
{
public:
	LaserDeadSpotsDataFilter(const std::string &                     filter_name,
	                         fawkes::Configuration *                 config,
	                         fawkes::Logger *                        logger,
	                         const std::string &                     prefix,
	                         unsigned int                            data_size,
	                         std::vector<LaserDataFilter::Buffer *> &in);
	LaserDeadSpotsDataFilter(const LaserDeadSpotsDataFilter &other);
	~LaserDeadSpotsDataFilter();

	LaserDeadSpotsDataFilter &operator=(const LaserDeadSpotsDataFilter &other);

	void filter();

private:
	void calc_spots();
	void set_out_vector(std::vector<LaserDataFilter::Buffer *> &out);

private:
	fawkes::Logger *logger_;

	unsigned int                         num_spots_;
	unsigned int *                       dead_spots_;
	unsigned int                         dead_spots_size_;
	std::vector<std::pair<float, float>> cfg_dead_spots_;
};

#endif
