
/***************************************************************************
 *  logreplay_thread.cpp - BB Log Replay Thread
 *
 *  Created: Mi Feb 17 01:53:00 2010
 *  Copyright  2010  Masrur Doostdar, Tim Niemueller [www.niemueller.de]
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

#include "logreplay_thread.h"
#include "file.h"

#include <blackboard/blackboard.h>
#include <utils/logging/logger.h>
#include <core/exceptions/system.h>
#include <utils/misc/autofree.h>

#include <blackboard/internal/instance_factory.h>


#include <memory>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <endian.h>
#include <arpa/inet.h>
#include <sys/mman.h>

using namespace fawkes;

/** @class BBLogReplayThread "logreplay_thread.h"
 * BlackBoard log Replay thread.
 * Writes the data of the logfile into a blackboard interface, considering the time-step differences between the data.
 * @author Masrur Doostdar
 * @author Tim Niemueller
 */

/** Constructor.
 * @param logfile_name filename of the log to be replayed
 * @param logdir directory containing the logfile
 * @param scenario ID of the log scenario
 * @param loop_replay specifies if the replay should be looped
 */
BBLogReplayThread::BBLogReplayThread(const char *logfile_name,
				     const char *logdir,
				     const char *scenario,
				     const bool loop_replay)
  : Thread("BBLogReplayThread", Thread::OPMODE_CONTINUOUS)
{
  //  set_coalesce_wakeups(true);
  set_name("BBLogReplayThread(%s)", logfile_name);

  __logfile_name= strdup(logfile_name);
  __logdir      = strdup(logdir);
  __scenario    = strdup(scenario); // dont need this!?
  __filename    = NULL;
  __loop_replay = loop_replay;
  //  __queue_mutex = new Mutex();
  //  __data_size   = 0;
  //  __now = NULL;
}


/** Destructor. */
BBLogReplayThread::~BBLogReplayThread()
{
  free(__logfile_name);
  free(__logdir);
  free(__scenario);
}



void
BBLogReplayThread::read_file_header(FILE *f, bblog_file_header *header)
{
  uint32_t magic;
  uint32_t version;
  if ((fread(&magic, sizeof(uint32_t), 1, f) == 1) &&
      (fread(&version, sizeof(uint32_t), 1, f) == 1) ) {
    if ( (ntohl(magic) == BBLOGGER_FILE_MAGIC) &&
	 (ntohl(version) == BBLOGGER_FILE_VERSION) ) {
      rewind(f);
      if (fread(header, sizeof(bblog_file_header), 1, f) != 1) {
	printf("Failed to read file header\n");
	throw Exception(errno, "Failed to read file header");
      }
    } else {
      printf("File magic/version %X/%u does not match (expected %X/%u)",
	     ntohl(magic), ntohl(version),
	     BBLOGGER_FILE_MAGIC, BBLOGGER_FILE_VERSION);
      throw Exception("File magic/version %X/%u does not match (expected %X/%u)",
		      ntohl(magic), ntohl(version),
		      BBLOGGER_FILE_VERSION, BBLOGGER_FILE_MAGIC);
    }
  } else {
    perror("Failed to read magic/version from file");
    throw Exception(errno, "Failed to read magic/version from file");
  }
}


void
BBLogReplayThread::sanity_check(FILE *f, bblog_file_header *header)
{
  if (header->num_data_items == 0) {
    printf("\nWARNING: file does not specify number of data items. This usually\n"
	   "         happens if the logger was stopped unexpectedly or is still\n"
	   "         running. Either stop the logger, use repair command to fix\n"
	   "         the file.\n");
    throw Exception("file does not specify number of data items");
  }

  struct stat fs;
  if (fstat(fileno(f), &fs) != 0) {
    throw Exception(errno, "Failed to get stat file");
  }

  off_t expected_size = sizeof(bblog_file_header)
    + header->num_data_items * header->data_size
    + header->num_data_items * sizeof(bblog_entry_header);
  if (expected_size != fs.st_size) {
    printf("\nWARNING: file size does not match expectation. Expected %li b,\n"
	   "         but file has %li b. The logger might still be running.\n"
	   "         Otherwise use repair command to fix the file.\n",
	   expected_size, fs.st_size);
    throw Exception("file size does not match expectation");
  }

#if __BYTE_ORDER == __LITTLE_ENDIAN
  if (header->endianess == 1)
#else
  if (header->endianess == 0)
#endif
  {
    printf("\nWARNING: file has incompatible endianess.\n");
    throw Exception("file size does not match expectation");
  }
  
}


void
BBLogReplayThread::read_entry(FILE *f, bblog_file_header *header, bblog_entry_header *entryh,
	   Interface *iface, unsigned int index, bool do_seek )
{
  if (do_seek) {
    long offset = sizeof(bblog_file_header)
      + (sizeof(bblog_entry_header) + header->data_size) * index;

    if (fseek(f, offset, SEEK_SET) != 0) {
      throw Exception(errno, "Cannot seek to index %u", index);
    }
  }

  void *data = malloc(header->data_size);
  if ( (fread(entryh, sizeof(bblog_entry_header), 1, f) == 1) &&
       (fread(data, header->data_size, 1, f) == 1) ) {
    iface->set_from_chunk(data);
  } else {
    free(data);
    throw Exception("Cannot read interface data");
  }
  free(data);
}




void
BBLogReplayThread::init()
{

  if (asprintf(&__filename, "%s/%s", __logdir, __logfile_name) == -1) {
    throw OutOfMemoryException("Cannot re-generate logfile-path");
  }
  __f_data = fopen(__filename, "r");
  if (!__f_data) {
    throw Exception("Failed to open log file %s",__filename);
  }

  logger->log_info(name(), "Replaying from %s:", __filename);
}



void
BBLogReplayThread::once()
{
  do{
    try {
      bblog_file_header header;
      read_file_header(__f_data, &header);
      sanity_check(__f_data, &header);
      
      char interface_type[BBLOG_INTERFACE_TYPE_SIZE + 1];
      char interface_id[BBLOG_INTERFACE_ID_SIZE + 1];
      
      strncpy(interface_type, header.interface_type, BBLOG_INTERFACE_TYPE_SIZE);
      strncpy(interface_id, header.interface_id, BBLOG_INTERFACE_ID_SIZE);
      
      
      Interface *iface = blackboard->open_for_writing(interface_type, interface_id);
      
      if (memcmp(header.interface_hash, iface->hash(), BBLOG_INTERFACE_HASH_SIZE) != 0) {
	printf("Cannot read data. Hash mismatch between local interface and\n"
	       "log data.\n\n");
	blackboard->close(iface);
	throw Exception("Interface hash mismatch\n");
      }
      
      bblog_entry_header entryh;
      timeval last = {0, 0};
      for (unsigned int i = 0; i < header.num_data_items; ++i) {
	read_entry(__f_data, &header, &entryh, iface, i);
	timeval next = {entryh.rel_time_sec, entryh.rel_time_usec};
	usleep(time_diff_usec(next, last));
	//	logger->log_info(name(), "timediff:%f,timeall%f",(double)time_diff_usec(next, last)/1000000,entryh.rel_time_sec + ((double) entryh.rel_time_usec/1000000));            
	last = next;
	
	iface->write();
	
	//      print_entry(&entryh, iface);
	//      printf("\n");
	//      logger->log_info(name(), ".");
      }
      
      blackboard->close(iface);
      
    } catch (Exception &e) {
      printf("Failed to read log file: %s\n", e.what());
    }
    if(!__loop_replay){
      logger->log_info(name(), "\nReplay finished for file\n: %s", __filename);
      fclose(__f_data);
      free(__filename);
    }
    else{
      logger->log_info(name(), "\nLooping file\n: %s", __filename);
      rewind(__f_data);
    }
  }
  while (__loop_replay);
}




void
BBLogReplayThread::finalize()   // wont be called??
{
  logger->log_info(name(), "Replay fineshed for file\n: %s", __filename);
  fclose(__f_data);
  free(__filename);
}




