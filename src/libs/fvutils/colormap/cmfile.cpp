
/**************************************************************************
 *  cmfile.cpp - FVFF Colormap File Format
 *
 *  Created: Mon Mar 31 14:11:01 2008
 *  Copyright  2005-2008  Tim Niemueller  [www.niemueller.de]
 *
 ***************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. A runtime exception applies to
 *  this software (see LICENSE.GPL_WRE file mentioned below for details).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL_WRE file in the doc directory.
 */

#include <core/exception.h>
#include <fvutils/colormap/cmfile.h>
#include <fvutils/colormap/cmfile_yuvblock.h>
#include <fvutils/colormap/colormap.h>
#include <fvutils/colormap/yuvcm.h>
#include <sys/utsname.h>

#include <cstdio>

namespace firevision {

/** @class ColormapFile::ColormapBlockVector <fvutils/colormap/cmfile.h>
 * Vector of colormap blocks.
 * @author Tim Niemueller
 */

/** Destructor.
 * Deletes all hold colormap blocks.
 */
ColormapFile::ColormapBlockVector::~ColormapBlockVector()
{
	for (iterator i = begin(); i != end(); ++i) {
		delete *i;
	}
}

/** @class ColormapFile <fvutils/colormap/cmfile.h>
 * Colormap file.
 * This class implements a FireVision data file format for colormaps.
 * @author Tim Niemueller
 */

/** Constructor.
 * Creates a plain empty colormap file with given dimensions.
 * @param depth depth of colormap
 * @param width width of colormap
 * @param height height of colormap
 */
ColormapFile::ColormapFile(uint16_t depth, uint16_t width, uint16_t height)
: FireVisionDataFile(CMFILE_MAGIC_TOKEN, CMFILE_CUR_VERSION)
{
	_spec_header      = calloc(1, sizeof(cmfile_header_t));
	_spec_header_size = sizeof(cmfile_header_t);
	header_           = (cmfile_header_t *)_spec_header;
	header_->depth    = depth;
	header_->width    = width;
	header_->height   = height;
}

/** Constructor.
 * Creates a plain empty colormap file.
 */
ColormapFile::ColormapFile() : FireVisionDataFile(CMFILE_MAGIC_TOKEN, CMFILE_CUR_VERSION)
{
	header_ = NULL;
}

/** Add colormap.
 * This will add the given colormap to this file. It will query the colormap for
 * a number of blocks that shall be added to the file.
 * Note that for now only a single colormap per file is supported, though not
 * enforced.
 * @param colormap colormap to add
 */
void
ColormapFile::add_colormap(Colormap *colormap)
{
	if (!header_) {
		if (_spec_header) {
			header_ = (cmfile_header_t *)_spec_header;
		} else {
			_spec_header      = calloc(1, sizeof(cmfile_header_t));
			_spec_header_size = sizeof(cmfile_header_t);
			header_           = (cmfile_header_t *)_spec_header;
			header_->depth    = colormap->depth();
			header_->width    = colormap->width();
			header_->height   = colormap->height();
		}
	}

	if ((colormap->depth() != header_->depth) || (colormap->width() != header_->width)
	    || (colormap->height() != header_->height)) {
		throw fawkes::Exception(
		  "Colormap dimensions %dx%dx%d do not match expected dimensions %dx%dx%d",
		  colormap->depth(),
		  colormap->width(),
		  colormap->height(),
		  header_->depth,
		  header_->width,
		  header_->height);
	}

	printf("Adding colormap with dimensions %dx%dx%d\n",
	       colormap->width(),
	       colormap->height(),
	       colormap->depth());

	std::list<ColormapFileBlock *> blocks = colormap->get_blocks();
	for (std::list<ColormapFileBlock *>::iterator i = blocks.begin(); i != blocks.end(); ++i) {
		add_block(*i);
	}
}

/** Get colormap blocks.
 * @return vector of colormap blocks
 */
ColormapFile::ColormapBlockVector *
ColormapFile::colormap_blocks()
{
	FireVisionDataFile::BlockList &b  = blocks();
	ColormapBlockVector *          rv = new ColormapBlockVector();
	for (std::list<FireVisionDataFileBlock *>::iterator i = b.begin(); i != b.end(); ++i) {
		if ((*i)->type() == CMFILE_TYPE_YUV) {
			ColormapFileYuvBlock *yuvb = new ColormapFileYuvBlock(*i);
			rv->push_back(yuvb);
		}
	}

	return rv;
}

void
ColormapFile::assert_header()
{
	if (!header_) {
		if (!_spec_header) {
			throw fawkes::Exception("Cannot get header information, invalid ctor used or file not read?");
		}
		header_ = (cmfile_header_t *)_spec_header;
	}
}

/** Get a freshly generated colormap based on current file content.
 * This returns an instance of a colormap that uses all current blocks of this instance.
 * Currently it only supports file which contain a valid YuvColormap. This means that it
 * has d blocks of YUV type. d is the depth and must fulfill d=2^n with n from [1,8].
 * It can throw any exception that the YuvColormap ctor can throw.
 * @return instance of colormap. You must delete it after you are done with it.
 */
Colormap *
ColormapFile::get_colormap()
{
	// Make sure we only have YUV blocks
	BlockList &  bl = blocks();
	YuvColormap *cm = NULL;

	for (BlockList::iterator b = bl.begin(); b != bl.end(); ++b) {
		if ((*b)->type() != CMFILE_TYPE_YUV) {
			throw fawkes::Exception("Colormap file contains block of unknown type");
		}
	}

	assert_header();

	// create colormap, throws an exception is depth/num_blocks is invalid
	//printf("File header dimensions: %dx%dx%d\n",
	//	 header_->depth, header_->width, header_->height);
	cm = new YuvColormap(header_->depth, header_->width, header_->height);

	unsigned int level = 0;
	for (BlockList::iterator b = bl.begin(); b != bl.end(); ++b) {
		if ((*b)->data_size() != cm->plane_size()) {
			// invalid size, for a YUV colormap we must have this for one plane!
			delete cm;
			throw fawkes::Exception("Invalid data size for a YUV block");
		}

		cm->copy_uvplane((unsigned char *)(*b)->data_ptr(), level++);
	}

	return cm;
}

/** Check if given file is a colormap file.
 * @param filename name of file to check
 * @return true if file is a colormap file, false otherwise
 */
bool
ColormapFile::is_colormap_file(const char *filename)
{
	return FireVisionDataFile::has_magic_token(filename, CMFILE_MAGIC_TOKEN);
}

/** Compose filename.
 * In the format %g is replaced with the hostname.
 * @param format format for the filename
 * @return filename
 */
std::string
ColormapFile::compose_filename(const std::string format)
{
	std::string rv = format;

	struct utsname uname_info;
	uname(&uname_info);

	size_t loc = rv.find("%h");
	while (loc != std::string::npos) {
		rv.replace(loc, 2, uname_info.nodename);
		loc = rv.find("%h");
	}

	return rv;
}

void
ColormapFile::clear()
{
	FireVisionDataFile::clear();
	header_ = NULL;
}

/** Get depth of colormap.
 * @return depth
 */
uint16_t
ColormapFile::get_depth()
{
	assert_header();
	return header_->depth;
}

/** Get width of colormap.
 * @return width
 */
uint16_t
ColormapFile::get_width()
{
	assert_header();
	return header_->width;
}

/** Get height of colormap.
 * @return height
 */
uint16_t
ColormapFile::get_height()
{
	assert_header();
	return header_->height;
}

} // end namespace firevision
