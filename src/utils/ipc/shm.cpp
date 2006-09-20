
/***************************************************************************
 *  shm.cpp - shared memory image buffer
 *
 *  Generated: Thu Jan 12 14:10:43 2006
 *  Copyright  2005-2006  Tim Niemueller [www.niemueller.de]
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/** @defgroup IPC Interprocess Communication (IPC)
 * Utilties for interprocess communication like shared memory segments,
 * semaphore sets and message queues.
 */

#include <utils/ipc/shm.h>
#include <utils/ipc/shm_exceptions.h>
#include <utils/ipc/shm_lister.h>

#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <iostream>

/** @class SharedMemoryHeader utils/ipc/shm.h
 * Interface for shared memory header.
 * This class has to be implemented to be able to use shared memory segments.
 * It defines a set of properties for the shared memory segment that can be
 * searched for and printed out by an appropriate lister.
 *
 * @see SharedMemory
 * @see SharedMemoryLister
 * @ingroup IPC
 * @author Tim Niemueller
 *
 *
 * @fn SharedMemoryHeader::~SharedMemoryHeader()
 * Virtual destructor
 *
 * @fn bool SharedMemoryHeader::matches(unsigned char *buffer)
 * Method to check if the given buffer matches this header.
 * This method is called when searching for a shared memory segment to
 * open, list or erase it.
 * Implement this to distuinguish several shared memory segments that share
 * the same magic token.
 * @param buffer The memory buffer in the shared memory segment where to start
 * checking.
 * @return true, if the given data in the buffer matches this header, false
 * otherwise.
 *
 * @fn unsigned int SharedMemoryHeader::size()
 * Size of the header.
 * The size that is needed in the shared memory buffer to accomodate the
 * header data. This size has to fit all the data that will be stored in the
 * header. It must return the same size every time.
 *
 * @fn void SharedMemoryHeader::initialize(unsigned char *buffer)
 * Initialize the header.
 * This should initialize the header data in the given buffer from the
 * data of this SharedMemoryHeader derivate instance. It has to write out
 * all state information that is needed to identify the shared memory
 * segment later on.
 * @param buffer the buffer where the header data shall be written to.
 *
 * @fn void SharedMemoryHeader::set(unsigned char *buffer)
 * Set information from buffer.
 * Set the information stored in this SharedMemoryHeader derivate instance
 * from the data stored in the given buffer.
 * @param buffer The buffer where to copy data from.
 *
 * @fn unsigned int SharedMemoryHeader::dataSize()
 * Return the size of the data.
 * The size of the data that will be stored in the shared memory segment.
 * This method has to return the same value everytime and may only depend
 * on the other data set in the header and written to the shared memory
 * segment.
 * @return the size of the data segment
 */


/** @class SharedMemory utils/ipc/shm.h
 * Shared memory segment.
 * This class gives access to shared memory segment to store arbitrary data.
 * With shared memory data can be shared between several applications. Special
 * means like semaphores have to be used to control access to the storage
 * to prevent data corruption.
 *
 * The shared memory segment is divided into three parts.
 * 1. General shared memory header
 * 2. Data-specific header
 * 3. Data
 *
 * The general header consists of a magic token of MagicTokenSize that is used
 * to find the basically compatible shared memory segments out of all existing
 * shared memory segments. This is done for convenience. Although in general
 * shared memory is accessed via keys or IDs it is easier from the maintenance
 * side to just scan the segments to find the correct one, especially if there
 * may be more than just one segment for the same application.
 * The header also includes a semaphore ID which is unused at the moment.
 *
 * The data-specific header is generated from a given SharedMemoryHeader
 * implementation. It can be used to store any information that is needed to
 * identify a specific shared memory segment and to store management data for
 * the data segment. It should always contain enough information to derive
 * the data segment size or if needed an explicit information about the memory
 * size.
 *
 * The data segment can be filled with any data you like. The memory is not
 * protected by any means. You have to do this by yourself using IPC semaphores.
 *
 * This class provides utilities to list, erase and check existence of given
 * shared memory segments. For this often a SharedMemoryLister is used that
 * takes care of formatting the output of the specific information about the
 * shared memory segment.
 *
 * @see SharedMemoryHeader
 * @see SharedMemorySegment
 * @see qa_shmem.cpp
 * @ingroup IPC
 *
 * @author Tim Niemueller
 */

/** @var SharedMemory::buffer
 * Pointer to the data segment.
 */
/** @var SharedMemory::mem_size
 * Total size of the segment, including headers
 */
/** @fn SharedMemory::data_size
 * Size of the data segment only
 */
/** @var SharedMemory::header
 * Data-specific header
 */
/** @var SharedMemory::is_read_only
 * Read-only.
 * if true before attach() open segment read-only
 */
/** @var SharedMemory::destroy_on_delete
 * destroy on delete.
 * If true before free() segment is destroyed.
 */
/** @var SharedMemory::should_create
 * Create shared memory segment.
 * If true before attach shared memory segment is created if it does
 * not exist.
 */
/** @var SharedMemory::magic_token
 * Magic token
 */
/** @var SharedMemory::shm_magic_token
 * Magic token as stored in the shared memory segment
 */
/** @var SharedMemory::shm_header
 * general header as stored in the shared memory segment
 */

/** The magic token size.
 * Your magic token identifier may have an arbitrary size. It is truncated
 * at MagicTokenSize bytes or filled with zeros up to a length of
 * MagicTokenSize bytes.
 */
const unsigned int SharedMemory::MagicTokenSize = 16;


/** Constructor for derivates.
 * This constructor may only be used by derivatives. It can be used to delay
 * the call to attach() to do other preparations like creating a
 * SharedMemoryHeader object.
 * @param magic_token magic token of the shared memory segment
 * @param is_read_only if true the shared memory segment is opened in
 *                     read-only mode
 * @param create       if true the shared memory segment is created if
 *                     no one matching the headers was found
 * @param destroy_on_delete if true the shared memory segment is destroyed
 *                          when this SharedMemory instance is deleted.
 */
SharedMemory::SharedMemory(char *magic_token,
			   bool is_read_only,
			   bool create,
			   bool destroy_on_delete)
{
  this->magic_token = new char[MagicTokenSize];
  memset(this->magic_token, 0, MagicTokenSize);
  strncpy(this->magic_token, magic_token, MagicTokenSize);

  this->is_read_only      = is_read_only;
  this->destroy_on_delete = destroy_on_delete;
  this->should_create     = create;

  buffer          = NULL;
  shm_magic_token = NULL;
  shm_header      = NULL;
  header          = NULL;
  data_size       = 0;
}


/** Create a new shared memory segment.
 * This will open a shared memory segment that exactly fits the given
 * SharedMemoryHeader. It the segment does not exist and create is assured
 * the segment is created from the given data, otherwise the SharedMemory
 * instance remains in an invalid state and an exception is thrown.
 * The segment can be destroyed automatically if the instance is destroyed.
 * Shared memory segments can be opened read-only.
 * @param magic_token This is the magic token discussed above that is used
 *                    to identify the shared memory segment. The magic_token
 *                    can be of arbitrary size but at most MagicTokenSize
 *                    bytes are used.
 * @param header      The data-sepcific header used for this shared memory
 *                    segment
 * @param is_read_only if true the shared memory segment is opened in
 *                     read-only mode
 * @param create       if true the shared memory segment is created if
 *                     no one matching the headers was found
 * @param destroy_on_delete if true the shared memory segment is destroyed
 *                          when this SharedMemory instance is deleted.
 * @exception ShmNoHeaderException No header has been set
 * @exception ShmInconsistentSegmentSizeException The memory size is not the
 *                                                expected memory size
 * @exception ShmCouldNotAttachException Could not attach to shared
 *                                       memory segment
 */
SharedMemory::SharedMemory(char *magic_token,
			   SharedMemoryHeader *header,
			   bool is_read_only, bool create, bool destroy_on_delete)
{
  this->magic_token = new char[MagicTokenSize];
  memset(this->magic_token, 0, MagicTokenSize);
  strncpy(this->magic_token, magic_token, MagicTokenSize);

  this->header            = header;
  this->is_read_only      = is_read_only;
  this->destroy_on_delete = destroy_on_delete;
  this->should_create     = create;

  buffer          = NULL;
  shm_magic_token = NULL;
  shm_header      = NULL;
  data_size       = 0;

  try {
    attach();
  } catch (Exception &e) {
    e.append("SharedMemory public constructor");
    throw;
  }

  if (buffer == NULL) {
    throw ShmCouldNotAttachException("Could not attach to created shared memory segment");
  }
}


/** Destructor
 */
SharedMemory::~SharedMemory()
{
  delete[] magic_token;
  free();
}


/** Detach from and maybe destroy the shared memory segment.
 * This will detach from the shared memory segment. If destroy_on_delete is
 * true this will destroy the shared memory segment before detaching.
 */
void
SharedMemory::free()
{
  buffer = NULL;
  shm_header = NULL;
  shm_magic_token = NULL;

  if ((shared_mem_id != -1) && !is_read_only && destroy_on_delete ) {
    //std::cout << "Destroying shared memory segment at id "
    //          << shared_mem_id << std::endl;
    shmctl(shared_mem_id, IPC_RMID, NULL);
    shared_mem_id = -1;
  }
  if (shared_mem != NULL) {
    // std::cout << "Detaching from shared memory" << std::endl;
    shmdt(shared_mem);
    shared_mem = NULL;
  }
}


/** Attach to the shared memory segment.
 * This method will try to open and/or create the shared memory segment.
 * @exception ShmNoHeaderException No header has been set
 * @exception ShmInconsistentSegmentSizeException The memory size is not the
 *                                                expected memory size
 * @exception ShmCouldNotAttachException Could not attach to shared
 *                                       memory segment
 */
void
SharedMemory::attach()
{

  if (header == NULL) {
    // No shared memory header, needed!
    throw ShmNoHeaderException();
  }

  if ((buffer != NULL) && (shared_mem_id != -1)) {
    // a buffer has already been attached
    //cout << msg_prefix << cyellow << "Shared memory already attached" << cnormal << endl;
    return;
  }

  // based on code by ipcs and Philipp Vorst from allemaniACs3D
  int              max_id;
  int              shm_id;
  struct shmid_ds  shm_segment;
  void            *shm_buf;
  unsigned char   *shm_buffer;

  // Find out maximal number of existing SHM segments
  struct shm_info shm_info;
  max_id = shmctl( 0, SHM_INFO, (struct shmid_ds *)&shm_info );

  if (max_id >= 0) {
    for ( int i = 0; (buffer == NULL) && (i <= max_id); ++i ) {

      shm_id = shmctl( i, SHM_STAT, &shm_segment );
      if ( shm_id < 0 )  continue;

      shm_buf = shmat(shm_id, NULL, is_read_only ? SHM_RDONLY : 0);
      if (shm_buf != (void *)-1) {
	// Attached

	shm_magic_token = (char *)shm_buf;
	shm_header = (SharedMemory_header_t *)shm_buf + MagicTokenSize;

	if ( strncmp(shm_magic_token, magic_token, MagicTokenSize) == 0 ) {

	  shm_buffer = (unsigned char *)shm_buf + MagicTokenSize
                                                + sizeof(SharedMemory_header_t);

	  if ( header->matches( shm_buffer ) ) {
	    // Image found in memory

	    header->set( shm_buffer );
	    data_size = header->dataSize();
	    mem_size  = sizeof(SharedMemory_header_t) + header->size() + data_size;

	    if (mem_size != (unsigned int) shm_segment.shm_segsz) {
	      /*
	      cout << msg_prefix << cred << "Inconsistent image found in memory (mem, "
		   << mem_size << " vs. " << (unsigned int) shm_segment.shm_segsz
		   << " bytes)" << cnormal << endl;
	      */
	      throw ShmInconsistentSegmentSizeException(mem_size,
							(unsigned int) shm_segment.shm_segsz);
	    }

	    header->set( shm_buffer );
	    // header->printInfo();

	    // cout << msg_prefix << "Found matching FireVision shared memory segment" << endl;
	    shared_mem_id = shm_id;
	    shared_mem    = shm_buf;
	    buffer        = shm_buffer + header->size();

	  } else {
	    // not the wanted image
	    shmdt(shm_buf);
	  }
	} else {
	  // not our region of memory
	  shmdt(shm_buf);
	}
      } // else could not attach, ignore

    }
  }

  if (! is_read_only && should_create) {
    // try to create a new shared memory segment
    char proj     = 0;
    char max_proj = 127;
    data_size = header->dataSize();
    mem_size  = sizeof(SharedMemory_header_t) + header->size() + data_size;
    while ((buffer == NULL) && (proj < max_proj)) {
    // no shm segment found, create one
      key_t key = ftok(".", proj++);
      shared_mem_id = shmget(key, mem_size, IPC_CREAT | IPC_EXCL | 0666);
      if (shared_mem_id != -1) {
	shared_mem = shmat(shared_mem_id, NULL, 0);
	if (shared_mem != (void *)-1) {
	  // cout << msg_prefix << "Attached to shared mem" << endl;
	  shm_magic_token = (char *)shared_mem;
	  shm_header = (SharedMemory_header_t *)shared_mem + MagicTokenSize;
	  buffer = (unsigned char *)shared_mem + MagicTokenSize
	                                       + sizeof(SharedMemory_header_t)
                                               + header->size();

	  memset((void *)shared_mem, 0, mem_size);

	  strncpy(shm_magic_token, magic_token, MagicTokenSize);

	  header->initialize( (unsigned char*)shared_mem + MagicTokenSize
                                                         + sizeof(SharedMemory_header_t));

	} else {
	  // It didn't work out, destroy shared mem and try again
	  //cout << msg_prefix << flush;
          // perror("Could not attach to created shmem segment");
	  shmctl(shared_mem_id, IPC_RMID, NULL);
	  throw ShmCouldNotAttachException("Could not create shared memory segment");
	}
      } else {
        if (errno == EINVAL) {
	  throw ShmCouldNotAttachException("Could not attach, segment too small or too big");
	} else {
	  throw ShmCouldNotAttachException("Could not attach, shmget failed");
	}
      }
    }
    if (buffer == NULL) {
      throw ShmCouldNotAttachException("Could not attach, buffer still NULL");
    }
  }

}


/** Check for read-only mode
 * @return true, if the segment is opened in read-only mode, false otherwise
 */
bool
SharedMemory::isReadOnly()
{
  return is_read_only;
}


/** Get a pointer to the shared memory
 * This method returns a pointer to the data-segment of the shared memory
 * segment. It has the size stated as dataSize() from the header.
 * @return pointer to the data-segment
 * @see getDataSize()
 */
unsigned char *
SharedMemory::getBuffer()
{
  return buffer;
}


/** Get the size of the data-segment.
 * Use this method to get the size of the data segment. Calls dataSize() of
 * the data-specific header internally.
 * @return size of the data-segment in bytes
 */
unsigned int
SharedMemory::getDataSize()
{
  return data_size;
}


/** Copies data from the buffer to shared memory.
 * Use this method to copy data from the given external buffer to the
 * data segment of the shared memory.
 * @param buffer the buffer to copy from
 */
void
SharedMemory::set(unsigned char *buffer)
{
  memcpy(this->buffer, buffer, data_size);
}


/** Check if segment has been destroyed
 * This can be used if the segment has been destroyed. This means that no
 * other process can connect to the shared memory segment. As long as some
 * process is attached to the shared memory segment the segment will still
 * show up in the list
 * @return true, if this shared memory segment has been destroyed, false
 *         otherwise
 */
bool
SharedMemory::isDestroyed()
{
  return isDestroyed(shared_mem_id);
}


/** Check if memory can be swapped out.
 * This method can be used to check if the memory can be swapped.
 * @return true, if the memory cannot be swapped, false otherwise
 */
bool
SharedMemory::isLocked()
{
  return isLocked(shared_mem_id);
}


/** Check validity of shared memory segment.
 * Use this to check if the shared memory segmentis valid. That means that
 * this instance is attached to the shared memory and data can be read from
 * or written to the buffer.
 * @return true, if the shared memory segment is valid and can be utilized,
 *         false otherwise
 */
bool
SharedMemory::isValid()
{
  return (buffer != NULL);
}


/** Set deletion behaviour.
 * This has the same effect as the destroy_on_delete parameter given to the
 * constructor.
 * @param destroy set to true to destroy the shared memory segment on
 *        deletion
 */
void
SharedMemory::setDestroyOnDelete(bool destroy)
{
  destroy_on_delete = destroy;
}


/* ==================================================================
 * STATICs
 */

/** Check if a segment has been destroyed.
 * Check for a shared memory segment of the given ID.
 * @param shm_id ID of the shared memory segment.
 * @return true, if the shared memory segment was destroyed, false otherwise.
 * @exception ShmDoesNotExistException No shared memory segment exists for
 *                                     the given ID.
 */
bool
SharedMemory::isDestroyed(int shm_id)
{
  struct shmid_ds  shm_segment;
  struct ipc_perm *perm = &shm_segment.shm_perm;

  if (shmctl(shm_id, IPC_STAT, &shm_segment ) == -1) {
    throw ShmDoesNotExistException();
    return false;
  } else {
    return (perm->mode & SHM_DEST);
  }
}


/** Check if memory can be swapped out.
 * This method can be used to check if the memory can be swapped.
 * @param shm_id ID of the shared memory segment.
 * @return true, if the memory cannot be swapped, false otherwise
 */
bool
SharedMemory::isLocked(int shm_id)
{
  struct shmid_ds  shm_segment;
  struct ipc_perm *perm = &shm_segment.shm_perm;

  if (shmctl(shm_id , IPC_STAT, &shm_segment ) < 0) {
    return false;
  } else {
    return (perm->mode & SHM_LOCKED);
  }
}


/** List shared memory segments of a given type.
 * This method lists all shared memory segments that match the given magic
 * token (first MagicTokenSize bytes, filled with zero) and the given
 * header. The lister is called to format the output.
 * @param magic_token Token to look for
 * @param header      header to identify interesting segments with matching
 *                    magic_token
 * @param lister      Lister used to format output
 */
void
SharedMemory::list(char *magic_token,
		   SharedMemoryHeader *header, SharedMemoryLister *lister)
{

  lister->printHeader();

  int              max_id;
  int              shm_id;
  struct shmid_ds  shm_segment;
  void            *shm_buf;

  char                  *shm_magic_token;
  SharedMemory_header_t *shm_header;

  unsigned int     num_segments = 0;

  // Find out maximal number of existing SHM segments
  struct shm_info shm_info;
  max_id = shmctl( 0, SHM_INFO, (struct shmid_ds *)&shm_info );
  if (max_id >= 0) {
    for ( int i = 0; i <= max_id; ++i ) {

      shm_id = shmctl( i, SHM_STAT, &shm_segment );
      if ( shm_id < 0 )  continue;

      shm_buf = shmat(shm_id, NULL, SHM_RDONLY);
      if (shm_buf != (void *)-1) {
	// Attached

	shm_magic_token = (char *)shm_buf;
	shm_header = (SharedMemory_header_t *)shm_buf + MagicTokenSize;

	if (strncmp(shm_magic_token, magic_token, MagicTokenSize) == 0) {
	  if ( header->matches( (unsigned char*)shm_buf + MagicTokenSize
                                                        + sizeof(SharedMemory_header_t)) ) {
	    header->set((unsigned char*)shm_buf + MagicTokenSize
                                                + sizeof(SharedMemory_header_t));
	    lister->printInfo(header, shm_id, shm_segment.shm_segsz,
			      (unsigned char*)shm_buf + MagicTokenSize
                                                      + sizeof(SharedMemory_header_t)
                                                      + header->size());
	    ++num_segments;
	  }
	}
	shmdt(shm_buf);
      }
    }
    if ( num_segments == 0 ) {
      lister->printNoSegments();
    }
  }

  lister->printFooter();
}


/** Erase shared memory segments of a given type.
 * This method erases (destroys) all shared memory segments that match the
 * given magic token (first MagicTokenSize bytes, filled with zero) and the
 * given header. The lister is called to format the output.
 * @param magic_token Token to look for
 * @param header      header to identify interesting segments with matching
 *                    magic_token
 * @param lister      Lister used to format output, maybe NULL (default)
 */
void
SharedMemory::erase(char *magic_token,
		    SharedMemoryHeader *header, SharedMemoryLister *lister)
{

  if (lister != NULL) lister->printHeader();

  int              max_id;
  int              shm_id;
  struct shmid_ds  shm_segment;
  void            *shm_buf;

  char                  *shm_magic_token;
  SharedMemory_header_t *shm_header;
  unsigned int     num_segments = 0;

  // Find out maximal number of existing SHM segments
  struct shm_info shm_info;
  max_id = shmctl( 0, SHM_INFO, (struct shmid_ds *)&shm_info );
  if (max_id >= 0) {
    for ( int i = 0; i <= max_id; ++i ) {

      shm_id = shmctl( i, SHM_STAT, &shm_segment );
      if ( shm_id < 0 )  continue;

      shm_buf = shmat(shm_id, NULL, SHM_RDONLY);
      if (shm_buf != (void *)-1) {
	// Attached

	shm_magic_token = (char *)shm_buf;
	shm_header = (SharedMemory_header_t *)shm_buf + MagicTokenSize;

	if (strncmp(shm_magic_token, magic_token, MagicTokenSize) == 0) {
	  if ( header->matches( (unsigned char*)shm_buf + MagicTokenSize
                                                        + sizeof(SharedMemory_header_t)) ) {
	    header->set((unsigned char*)shm_buf + MagicTokenSize
                                                + sizeof(SharedMemory_header_t));
	    shmctl(shm_id, IPC_RMID, NULL);
	    if ( lister != NULL)
	      lister->printInfo(header, shm_id, shm_segment.shm_segsz,
				(unsigned char*)shm_buf + MagicTokenSize
				                        + sizeof(SharedMemory_header_t)
						        + header->size());
	    ++num_segments;
	  }
	}
	shmdt(shm_buf);
      }
    }
    if ( num_segments == 0 ) {
      if (lister != NULL) lister->printNoSegments();
    }
  }

  if (lister != NULL) lister->printFooter();
}


/** Check if a specific shared memory segment exists.
 * This method will search for a memory chunk that matches the given magic
 * token and header.
 * @param magic_token Token to look for
 * @param header      header to identify interesting segments with matching
 *                    magic_token
 * @return true, if a matching shared memory segment was found, else
 * otherwise
 */
bool
SharedMemory::exists(char *magic_token,
		     SharedMemoryHeader *header)
{
  int              max_id;
  int              shm_id;
  struct shmid_ds  shm_segment;
  void            *shm_buf;

  char                  *shm_magic_token;
  SharedMemory_header_t *shm_header;

  // Find out maximal number of existing SHM segments
  struct shm_info shm_info;
  bool found = false;
  max_id = shmctl( 0, SHM_INFO, (struct shmid_ds *)&shm_info );
  if (max_id >= 0) {
    for ( int i = 0; (! false) && (i <= max_id); ++i ) {

      shm_id = shmctl( i, SHM_STAT, &shm_segment );
      if ( shm_id < 0 )  continue;

      shm_buf = shmat(shm_id, NULL, SHM_RDONLY);
      if (shm_buf != (void *)-1) {
	// Attached

	shm_magic_token = (char *)shm_buf;
	shm_header = (SharedMemory_header_t *)shm_buf + MagicTokenSize;

	if (strncmp(shm_magic_token, magic_token, MagicTokenSize) == 0) {
	  if ( header->matches((unsigned char *)shm_buf + MagicTokenSize
                                                        + sizeof(SharedMemory_header_t)) ) {
	    found = true;
	  }
	}
	shmdt(shm_buf);
      }
    }
  }

  return found;
}
