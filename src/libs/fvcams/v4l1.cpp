
/***************************************************************************
 *  v4l1.cpp - Implementation to access V4L cam
 *
 *  Generated: Fri Mar 11 17:48:27 2005
 *  Copyright  2005  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

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
#include <core/exceptions/software.h>
#include <fvcams/v4l1.h>
#include <fvutils/color/colorspaces.h>
#include <fvutils/color/rgb.h>
#include <fvutils/system/camargp.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h> /* gettimeofday() */
#include <sys/types.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

using namespace std;
using namespace fawkes;

namespace firevision {

/// @cond INTERNALS

class V4L1CameraData
{
public:
	V4L1CameraData(const char *device_name)
	{
		this->device_name = strdup(device_name);
	}

	~V4L1CameraData()
	{
		free(device_name);
	}

public:
	char *device_name;

	/* V4L1 stuff */
	struct video_capability capabilities; // Device Capabilities: Can overlay, Number of channels, etc
	struct video_buffer     vbuffer;      // information about buffer
	struct video_window     window;       // Window Information: Size, Depth, etc
	struct video_channel
	  *channel; // Channels information: Channel[0] holds information for channel 0 and so on...
	struct video_picture picture; // Picture information: Palette, contrast, hue, etc
	struct video_tuner * tuner;   // Tuner Information: if the card has tuners...
	struct video_audio   audio;   // If the card has audio
	struct video_mbuf
	                   captured_frame_buffer; // Information for the frame to be captured: norm, palette, etc
	struct video_mmap *buf_v4l;               // mmap() buffer VIDIOCMCAPTURE
};

/// @endcond

/** @class V4L1Camera <fvcams/v4l1.h>
 * Video4Linux 1 camera implementation.
 */

/** Constructor.
 * @param device_name device file name (e.g. /dev/video0)
 */
V4L1Camera::V4L1Camera(const char *device_name)
{
	started = opened = false;
	data_            = new V4L1CameraData(device_name);
}

/** Constructor.
 * Initialize camera with parameters from camera argument parser.
 * Supported arguments:
 * - device=DEV, device file, for example /dev/video0
 * @param cap camera argument parser
 */
V4L1Camera::V4L1Camera(const CameraArgumentParser *cap)
{
	started = opened = false;
	if (cap->has("device")) {
		data_ = new V4L1CameraData(cap->get("device").c_str());
	} else {
		throw MissingParameterException("Missing device for V4L1Camera");
	}
}

/** Protected Constructor.
 * Gets called from V4LCamera, when the device has already been opened
 * and determined to be a V4L1 device.
 * @param device_name device file name (e.g. /dev/video0)
 * @param dev file descriptor of the opened device
 */
V4L1Camera::V4L1Camera(const char *device_name, int dev)
{
	started = opened = false;
	data_            = new V4L1CameraData(device_name);
	this->dev        = dev;

	// getting grabber info in capabilities struct
	if ((ioctl(dev, VIDIOCGCAP, &(data_->capabilities))) == -1) {
		throw Exception("V4L1Cam: Could not get capabilities");
	}

	post_open();
}

/** Destructor. */
V4L1Camera::~V4L1Camera()
{
	delete data_;
}

void
V4L1Camera::open()
{
	opened = false;

	dev = ::open(data_->device_name, O_RDWR);
	if (dev < 0) {
		throw Exception("V4L1Cam: Could not open device");
	}

	// getting grabber info in capabilities struct
	if ((ioctl(dev, VIDIOCGCAP, &(data_->capabilities))) == -1) {
		throw Exception("V4L1Cam: Could not get capabilities");
	}

	post_open();
}

/**
 * Post-open() operations
 * @param dev file descriptor of the opened device
 */
void
V4L1Camera::post_open()
{
	// Capture window information
	if ((ioctl(dev, VIDIOCGWIN, &data_->window)) == -1) {
		throw Exception("V4L1Cam: Could not get window information");
	}

	// Picture information
	if ((ioctl(dev, VIDIOCGPICT, &data_->picture)) == -1) {
		throw Exception("V4L1Cam: Could not get window information");
	}

	///Video Channel Information or Video Sources
	///Allocate space for each channel
	data_->channel = (struct video_channel *)malloc(sizeof(struct video_channel)
	                                                * (data_->capabilities.channels + 1));
	for (int ch = 0; ch < data_->capabilities.channels; ch++) {
		data_->channel[ch].norm = 0;
		if ((ioctl(dev, VIDIOCSCHAN, &data_->channel[ch])) == -1) {
			printf("V4L1Cam: Could not get channel information for channel %i: %s", ch, strerror(errno));
		}
	}

	///Trying to capture through read()
	if (ioctl(dev, VIDIOCGMBUF, data_->captured_frame_buffer) == -1) {
		capture_method = READ;
		frame_buffer =
		  (unsigned char *)malloc(data_->window.width * data_->window.height * RGB_PIXEL_SIZE);
	} else {
		capture_method = MMAP;
		frame_buffer   = (unsigned char *)
		  mmap(0, data_->captured_frame_buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, dev, 0);
		if ((unsigned char *)-1 == (unsigned char *)frame_buffer) {
			throw Exception("V4L1Cam: Cannot initialize mmap region");
		}
	}

	data_->buf_v4l = NULL;

	opened = true;
}

void
V4L1Camera::start()
{
	started = false;
	if (!opened) {
		throw Exception("V4L1Cam: Trying to start closed cam!");
	}

	started = true;
}

void
V4L1Camera::stop()
{
	started = false;
}

void
V4L1Camera::print_info()
{
	if (!opened)
		return;

	cout << endl
	     << "CAPABILITIES" << endl
	     << "===========================================================================" << endl;

	if (data_->capabilities.type & VID_TYPE_CAPTURE)
		cout << " + Can capture to memory" << endl;
	if (data_->capabilities.type & VID_TYPE_TUNER)
		cout << " + Has a tuner of some form" << endl;
	if (data_->capabilities.type & VID_TYPE_TELETEXT)
		cout << " + Has teletext capability" << endl;
	if (data_->capabilities.type & VID_TYPE_OVERLAY)
		cout << " + Can overlay its image onto the frame buffer" << endl;
	if (data_->capabilities.type & VID_TYPE_CHROMAKEY)
		cout << " + Overlay is Chromakeyed" << endl;
	if (data_->capabilities.type & VID_TYPE_CLIPPING)
		cout << " + Overlay clipping is supported" << endl;
	if (data_->capabilities.type & VID_TYPE_FRAMERAM)
		cout << " + Overlay overwrites frame buffer memory" << endl;
	if (data_->capabilities.type & VID_TYPE_SCALES)
		cout << " + The hardware supports image scaling" << endl;
	if (data_->capabilities.type & VID_TYPE_MONOCHROME)
		cout << " + Image capture is grey scale only" << endl;
	if (data_->capabilities.type & VID_TYPE_SUBCAPTURE)
		cout << " + Can subcapture" << endl;

	cout << endl;
	cout << " Number of Channels ='" << data_->capabilities.channels << "'" << endl;
	cout << " Number of Audio Devices ='" << data_->capabilities.audios << "'" << endl;
	cout << " Maximum Capture Width ='" << data_->capabilities.maxwidth << "'" << endl;
	cout << " Maximum Capture Height ='" << data_->capabilities.maxheight << "'" << endl;
	cout << " Minimum Capture Width ='" << data_->capabilities.minwidth << "'" << endl;
	cout << " Minimum Capture Height ='" << data_->capabilities.minheight << "'" << endl;

	cout << endl
	     << "CAPTURE WINDOW INFO" << endl
	     << "===========================================================================" << endl;

	cout << " X Coord in X window Format:  " << data_->window.x << endl;
	cout << " Y Coord in X window Format:  " << data_->window.y << endl;
	cout << " Width of the Image Capture:  " << data_->window.width << endl;
	cout << " Height of the Image Capture: " << data_->window.height << endl;
	cout << " ChromaKey:                   " << data_->window.chromakey << endl;

	cout << endl
	     << "DEVICE PICTURE INFO" << endl
	     << "===========================================================================" << endl;

	cout << " Picture Brightness: " << data_->picture.brightness << endl;
	cout << " Picture        Hue: " << data_->picture.hue << endl;
	cout << " Picture     Colour: " << data_->picture.colour << endl;
	cout << " Picture   Contrast: " << data_->picture.contrast << endl;
	cout << " Picture  Whiteness: " << data_->picture.whiteness << endl;
	cout << " Picture      Depth: " << data_->picture.depth << endl;
	cout << " Picture    Palette: " << data_->picture.palette << " (";

	if (data_->picture.palette == VIDEO_PALETTE_GREY)
		cout << "VIDEO_PALETTE_GRAY";
	if (data_->picture.palette == VIDEO_PALETTE_HI240)
		cout << "VIDEO_PALETTE_HI240";
	if (data_->picture.palette == VIDEO_PALETTE_RGB565)
		cout << "VIDEO_PALETTE_RGB565";
	if (data_->picture.palette == VIDEO_PALETTE_RGB555)
		cout << "VIDEO_PALETTE_RGB555";
	if (data_->picture.palette == VIDEO_PALETTE_RGB24)
		cout << "VIDEO_PALETTE_RGB24";
	if (data_->picture.palette == VIDEO_PALETTE_RGB32)
		cout << "VIDEO_PALETTE_RGB32";
	if (data_->picture.palette == VIDEO_PALETTE_YUV422)
		cout << "VIDEO_PALETTE_YUV422";
	if (data_->picture.palette == VIDEO_PALETTE_YUYV)
		cout << "VIDEO_PALETTE_YUYV";
	if (data_->picture.palette == VIDEO_PALETTE_UYVY)
		cout << "VIDEO_PALETTE_UYVY";
	if (data_->picture.palette == VIDEO_PALETTE_YUV420)
		cout << "VIDEO_PALETTE_YUV420";
	if (data_->picture.palette == VIDEO_PALETTE_YUV411)
		cout << "VIDEO_PALETTE_YUV411";
	if (data_->picture.palette == VIDEO_PALETTE_RAW)
		cout << "VIDEO_PALETTE_RAW";
	if (data_->picture.palette == VIDEO_PALETTE_YUV422P)
		cout << "VIDEO_PALETTE_YUV422P";
	if (data_->picture.palette == VIDEO_PALETTE_YUV411P)
		cout << "VIDEO_PALETTE_YUV411P";

	cout << ")" << endl;

	cout << endl
	     << "VIDEO SOURCE INFO" << endl
	     << "===========================================================================" << endl;

	cout << " Channel Number or Video Source Number: " << data_->channel->channel << endl;
	cout << " Channel Name:                          " << data_->channel->name << endl;
	cout << " Number of Tuners for this source:      " << data_->channel->tuners << endl;
	cout << " Channel Norm:                          " << data_->channel->norm << endl;
	if (data_->channel->flags & VIDEO_VC_TUNER)
		cout << " + This channel source has tuners" << endl;
	if (data_->channel->flags & VIDEO_VC_AUDIO)
		cout << " + This channel source has audio" << endl;
	if (data_->channel->type & VIDEO_TYPE_TV)
		cout << " + This channel source is a TV input" << endl;
	if (data_->channel->type & VIDEO_TYPE_CAMERA)
		cout << " + This channel source is a Camera input" << endl;

	cout << endl
	     << "FRAME BUFFER INFO" << endl
	     << "===========================================================================" << endl;

	cout << " Base Physical Address:  " << data_->vbuffer.base << endl;
	cout << " Height of Frame Buffer: " << data_->vbuffer.height << endl;
	cout << " Width of Frame Buffer:  " << data_->vbuffer.width << endl;
	cout << " Depth of Frame Buffer:  " << data_->vbuffer.depth << endl;
	cout << " Bytes Per Line:         " << data_->vbuffer.bytesperline << endl;

	/* Which channel!?
  cout << endl << "CHANNEL INFO" << endl
       << "===========================================================================" << endl;

  cout << " Channel:          " << ch << " - " << channel[ch].name << endl;
  cout << " Number of Tuners: " << channel[0].tuners << endl;
  cout << " Input Type:       " << channel[ch].type << endl;
  cout << " Flags: " << endl;
  if(channel[0].flags & VIDEO_VC_TUNER)
    cout << " + This Channel Source has Tuners" << endl;
  if(channel[0].flags & VIDEO_VC_AUDIO)
    cout << " + This Channel Source has Audio" << endl;
  //  if(channel[0].flags & VIDEO_VC_NORM)
  //cout << " \tThis Channel Source has Norm\n");
  cout << " Norm for Channel: '" << channel[0].norm << "'" << endl;
  */
}

void
V4L1Camera::capture()
{
	if (capture_method == READ) {
		int len = read(dev, frame_buffer, data_->window.width * data_->window.height * RGB_PIXEL_SIZE);
		if (len < 0) {
			throw Exception("V4L1Cam: Could not capture frame");
		}
	} else {
		data_->buf_v4l =
		  (struct video_mmap *)malloc(data_->captured_frame_buffer.frames * sizeof(struct video_mmap));

		///Setting up the palette, size of frame and which frame to capture
		data_->buf_v4l[0].format = data_->picture.palette;
		data_->buf_v4l[0].frame  = 0;
		data_->buf_v4l[0].width  = data_->window.width;
		data_->buf_v4l[0].height = data_->window.height;

		if (ioctl(dev, VIDIOCMCAPTURE, &(data_->buf_v4l[0])) == -1) {
			throw Exception("V4L1Cam: Could not capture frame (VIDIOCMCAPTURE)");
		}
		///Waiting for the frame to finish
		int Frame = 0;
		if (ioctl(dev, VIDIOCSYNC, &Frame) == -1) {
			throw Exception("V4L1Cam: Could not capture frame (VIDIOCSYNC)");
		}
	}
}

void
V4L1Camera::dispose_buffer()
{
	if (capture_method == MMAP) {
		if (data_->buf_v4l != NULL) {
			free(data_->buf_v4l);
			data_->buf_v4l = NULL;
		}
		munmap(frame_buffer, data_->captured_frame_buffer.size);
	}
}

unsigned char *
V4L1Camera::buffer()
{
	return frame_buffer;
}

unsigned int
V4L1Camera::buffer_size()
{
	return colorspace_buffer_size(RGB, data_->window.width, data_->window.height);
}

void
V4L1Camera::close()
{
	if (opened) {
		::close(dev);
	}
}

unsigned int
V4L1Camera::pixel_width()
{
	if (opened) {
		return data_->window.width;
	} else {
		throw Exception("V4L1Cam::pixel_width(): Camera not opened");
	}
}

unsigned int
V4L1Camera::pixel_height()
{
	if (opened) {
		return data_->window.height;
	} else {
		throw Exception("V4L1Cam::pixel_height(): Camera not opened");
	}
}

colorspace_t
V4L1Camera::colorspace()
{
	return BGR;
}

void
V4L1Camera::flush()
{
}

bool
V4L1Camera::ready()
{
	return started;
}

void
V4L1Camera::set_image_number(unsigned int n)
{
}

} // end namespace firevision
