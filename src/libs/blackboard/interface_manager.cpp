 
/***************************************************************************
 *  interface_manager.cpp - BlackBoard interface manager
 *
 *  Generated: Mon Oct 09 19:08:29 2006
 *  Copyright  2006  Tim Niemueller [www.niemueller.de]
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

#include <blackboard/interface_manager.h>

#include <blackboard/blackboard.h>
#include <blackboard/memory_manager.h>
#include <blackboard/message_manager.h>
#include <blackboard/exceptions.h>
#include <blackboard/interface_mem_header.h>
#include <blackboard/interface_listener.h>
#include <blackboard/interface_observer.h>

#include <interface/interface.h>

#include <core/threading/mutex.h>
#include <core/threading/refc_rwlock.h>
#include <core/exceptions/system.h>
#include <utils/system/dynamic_module/module_dl.h>
#include <utils/logging/liblogger.h>

#include <stdlib.h>

using namespace std;

/** @class BlackBoardInterfaceManager <blackboard/interface_manager.h>
 * BlackBoard interface manager.
 * This class is used by the BlackBoard to manage interfaces stored in the
 * shared memory.
 *
 * @author Tim Niemueller
 */


/** Constructor.
 * The shared memory segment is created with data from bbconfig.h.
 * @param bb_memmgr BlackBoard memory manager to use
 * @param bb_msgmgr BlackBoard message manager to use
 * @see bbconfig.h
 */
BlackBoardInterfaceManager::BlackBoardInterfaceManager(BlackBoardMemoryManager *bb_memmgr,
						       BlackBoardMessageManager *bb_msgmgr)
{
  memmgr = bb_memmgr;
  msgmgr = bb_msgmgr;

  instance_serial = 1;
  mutex = new Mutex();
  try {
    iface_module = new ModuleDL( LIBDIR"/libinterfaces.so" );
    iface_module->open();
  } catch (Exception &e) {
    e.append("BlackBoardInterfaceManager cannot open interface module");
    delete mutex;
    delete iface_module;
    throw;
  }

  writer_interfaces.clear();
  rwlocks.clear();
}


/** Destructor */
BlackBoardInterfaceManager::~BlackBoardInterfaceManager()
{
  delete mutex;
  delete iface_module;
}


/** Creates a new interface instance.
 * This method will look in the libinterfaces shared object for a factory function
 * for the interface of the given type. If this was found a new instance of the
 * interface is returned.
 * @param type type of the interface
 * @param identifier identifier of the interface
 * @return a new instance of the requested interface type
 * @exception BlackBoardInterfaceNotFoundException thrown if the factory function
 * for the given interface type could not be found
 */
Interface *
BlackBoardInterfaceManager::new_interface_instance(const char *type, const char *identifier)
{
  char *generator_name = (char *)malloc(strlen("new") + strlen(type) + 1);
  sprintf(generator_name, "new%s", type);
  if ( ! iface_module->hasSymbol(generator_name) ) {
    free(generator_name);
    throw BlackBoardInterfaceNotFoundException(type);
  }

  InterfaceFactoryFunc iff = (InterfaceFactoryFunc)iface_module->getSymbol(generator_name);

  Interface *iface = iff();

  iface->__instance_serial = next_instance_serial();
  strncpy(iface->__type, type, __INTERFACE_TYPE_SIZE);
  strncpy(iface->__id, identifier, __INTERFACE_ID_SIZE);
  snprintf(iface->__uid, __INTERFACE_UID_SIZE, "%s::%s", type, identifier);
  iface->__interface_mediator = this;
  iface->__message_mediator   = msgmgr;

  free(generator_name);
  return iface;
}


/** Destroy an interface instance.
 * The destroyer function for the given interface is called to destroy the given
 * interface instance.
 * @param interface to destroy
 * @exception BlackBoardInterfaceNotFoundException thrown if the destroyer function
 * for the given interface could not be found. The interface will not be freed.
 */
void
BlackBoardInterfaceManager::delete_interface_instance(Interface *interface)
{
  char *destroyer_name = (char *)malloc(strlen("delete") + strlen(interface->__type) + 1);
  sprintf(destroyer_name, "delete%s", interface->__type);
  if ( ! iface_module->hasSymbol(destroyer_name) ) {
    free(destroyer_name);
    throw BlackBoardInterfaceNotFoundException(interface->__type);
  }

  InterfaceDestroyFunc idf = (InterfaceDestroyFunc)iface_module->getSymbol(destroyer_name);
  idf(interface);
  free(destroyer_name);
}


/** search memory chunks if the desired interface has been allocated already.
 * @param type type of the interface to look for
 * @param identifier identifier of the interface to look for
 * @return a pointer to the memory of the interface or NULL if not found
 */
void *
BlackBoardInterfaceManager::find_interface_in_memory(const char *type, const char *identifier)
{
  interface_header_t *ih;
  BlackBoardMemoryManager::ChunkIterator cit;
  for ( cit = memmgr->begin(); cit != memmgr->end(); ++cit ) {
    ih = (interface_header_t *)*cit;
    if ( (strncmp(ih->type, type, __INTERFACE_TYPE_SIZE) == 0) &&
	 (strncmp(ih->id, identifier, __INTERFACE_ID_SIZE) == 0 )
	 ) {
      // found it!
      return *cit;
    }
  }

  return NULL;
}


/** Get next mem serial.
 * @return next unique memory serial
 */
unsigned int
BlackBoardInterfaceManager::next_mem_serial()
{
  unsigned int serial = 1;
  interface_header_t *ih;
  BlackBoardMemoryManager::ChunkIterator cit;
  for ( cit = memmgr->begin(); cit != memmgr->end(); ++cit ) {
    ih = (interface_header_t *)*cit;
    if ( ih->serial >= serial ) {
      serial = ih->serial + 1;
    }
  }

  return serial;
}


/** Get next instance serial.
 * @return next unique instance serial
 */
unsigned int
BlackBoardInterfaceManager::next_instance_serial()
{
  if ( memmgr->is_master() ) {
    // simple, just increment value and return it
    return instance_serial++;
  } else {
    throw BBNotMasterException("Instance serial can only be requested by BB Master");
  }
}


/** Create an interface instance.
 * This will create a new interface instance. Storage in the shared memory
 * is allocated to hold the interface data.
 * @param type type of the interface
 * @param identifier identifier of the interface
 * @param interface reference to a pointer where the interface will be created
 * @param ptr reference to pointer of interface memory
 * @exception OutOfMemoryException thrown if there is not enough memory in the
 * BlackBoard to create the interface
 */
void
BlackBoardInterfaceManager::create_interface(const char *type, const char *identifier,
					     Interface* &interface, void* &ptr)
{
  interface_header_t *ih;

  // create new interface and allocate appropriate chunk
  interface = new_interface_instance(type, identifier);
  try {
    ptr = memmgr->alloc_nolock(interface->datasize() + sizeof(interface_header_t));
    ih  = (interface_header_t *)ptr;
  } catch (OutOfMemoryException &e) {
    e.append("BlackBoardInterfaceManager::createInterface: interface of type %s could not be created", type);
    memmgr->unlock();
    mutex->unlock();
    throw;
  }
  memset(ptr, 0, interface->datasize() + sizeof(interface_header_t));

  strncpy(ih->type, type, __INTERFACE_TYPE_SIZE);
  strncpy(ih->id, identifier, __INTERFACE_ID_SIZE);

  ih->refcount           = 0;
  ih->serial             = next_mem_serial();
  ih->flag_writer_active = 0;
  ih->num_readers        = 0;
  rwlocks[ih->serial] = new RefCountRWLock();

  interface->__mem_real_ptr  = ptr;
  interface->__mem_data_ptr  = (char *)ptr + sizeof(interface_header_t);
}


/** Open interface for reading.
 * This will create a new interface instance of the given type. The result can be
 * casted to the appropriate type.
 * @param type type of the interface
 * @param identifier identifier of the interface
 * @return new fully initialized interface instance of requested type
 * @exception OutOfMemoryException thrown if there is not enough free space for
 * the requested interface.
 */
Interface *
BlackBoardInterfaceManager::open_for_reading(const char *type, const char *identifier)
{
  mutex->lock();
  Interface *iface = NULL;
  void *ptr = NULL;
  interface_header_t *ih;
  bool created = false;

  memmgr->lock();

  ptr = find_interface_in_memory(type, identifier);

  if ( ptr != NULL ) {
    // found, instantiate new interface for given memory chunk
    iface = new_interface_instance(type, identifier);
    iface->__mem_real_ptr = ptr;
    iface->__mem_data_ptr = (char *)ptr + sizeof(interface_header_t);
    ih  = (interface_header_t *)ptr;
    rwlocks[ih->serial]->ref();
  } else {
    created = true;
    create_interface(type, identifier, iface, ptr);
    ih  = (interface_header_t *)ptr;
  }

  iface->__write_access = false;
  iface->__rwlock = rwlocks[ih->serial];
  iface->__mem_serial = ih->serial;
  iface->__message_queue = new MessageQueue(iface->__mem_serial, iface->__instance_serial);
  ih->refcount++;
  ih->num_readers++;

  memmgr->unlock();
  mutex->unlock();

  if ( created ) {
    notify_of_interface_created(type, identifier);
  }
  notify_of_reader_added(iface->uid());

  return iface;
}


/** Open all interfaces of the given type for reading.
 * This will create interface instances for all currently registered interfaces of
 * the given type. The result can be casted to the appropriate type.
 * @param type type of the interface
 * @param id_prefix if set only interfaces whose ids have this prefix are returned
 * @return list of new fully initialized interface instances of requested type. The
 * is allocated using new and you have to free it using delete after you are done
 * with it!
 */
std::list<Interface *> *
BlackBoardInterfaceManager::open_all_of_type_for_reading(const char *type,
							 const char *id_prefix)
{
  mutex->lock();
  memmgr->lock();

  bool match = false;
  std::list<Interface *> *rv = new std::list<Interface *>();

  Interface *iface = NULL;
  interface_header_t *ih;
  BlackBoardMemoryManager::ChunkIterator cit;
  for ( cit = memmgr->begin(); cit != memmgr->end(); ++cit ) {
    ih = (interface_header_t *)*cit;

    if (NULL == id_prefix) {
      match = (strncmp(ih->type, type, __INTERFACE_TYPE_SIZE) == 0);
    } else {
      unsigned int len = (id_prefix != NULL) ? strlen(id_prefix) : 0;
      match = ((strncmp(ih->type, type, __INTERFACE_TYPE_SIZE) == 0) &&
	       (len <= strlen(ih->id)) &&
	       (strncmp(id_prefix, ih->id, len) == 0) );
    }

    if (match) {
      // found one!
      // open 
      void *ptr = *cit;
      iface = new_interface_instance(ih->type, ih->id);
      iface->__mem_real_ptr = ptr;
      iface->__mem_data_ptr = (char *)ptr + sizeof(interface_header_t);
      ih  = (interface_header_t *)ptr;
      rwlocks[ih->serial]->ref();

      iface->__write_access = false;
      iface->__rwlock = rwlocks[ih->serial];
      iface->__mem_serial = ih->serial;
      iface->__message_queue = new MessageQueue(iface->__mem_serial, iface->__instance_serial);
      ih->refcount++;
      ih->num_readers++;

      rv->push_back(iface);
    }
  }

  mutex->unlock();
  memmgr->unlock();

  for (std::list<Interface *>::iterator j = rv->begin(); j != rv->end(); ++j) {
    notify_of_reader_added((*j)->uid());
  }

  return rv;
}


/** Open interface for writing.
 * This will create a new interface instance of the given type. The result can be
 * casted to the appropriate type. This will only succeed if there is not already
 * a writer for the given interface type/id!
 * @param type type of the interface
 * @param identifier identifier of the interface
 * @return new fully initialized interface instance of requested type
 * @exception OutOfMemoryException thrown if there is not enough free space for
 * the requested interface.
 * @exception BlackBoardWriterActiveException thrown if there is already a writing
 * instance with the same type/id
 */
Interface *
BlackBoardInterfaceManager::open_for_writing(const char *type, const char *identifier)
{
  mutex->lock();
  memmgr->lock();

  Interface *iface = NULL;
  void *ptr = NULL;
  interface_header_t *ih;
  bool created = false;

  ptr = find_interface_in_memory(type, identifier);

  if ( ptr != NULL ) {
    // found, check if there is already a writer
    //instantiate new interface for given memory chunk
    ih  = (interface_header_t *)ptr;
    if ( ih->flag_writer_active ) {
      memmgr->unlock();
      mutex->unlock();
      throw BlackBoardWriterActiveException(identifier, type);
    }
    iface = new_interface_instance(type, identifier);
    iface->__mem_real_ptr = ptr;
    iface->__mem_data_ptr = (char *)ptr + sizeof(interface_header_t);
    rwlocks[ih->serial]->ref();
  } else {
    created = true;
    create_interface(type, identifier, iface, ptr);
    ih = (interface_header_t *)ptr;
  }

  iface->__write_access = true;
  iface->__rwlock  = rwlocks[ih->serial];
  iface->__mem_serial = ih->serial;
  iface->__message_queue = new MessageQueue(iface->__mem_serial, iface->__instance_serial);
  ih->flag_writer_active = 1;
  ih->refcount++;

  memmgr->unlock();
  writer_interfaces[iface->__mem_serial] = iface;

  mutex->unlock();

  if ( created ) {
    notify_of_interface_created(type, identifier);
  }
  notify_of_writer_added(iface->uid());

  return iface;
}


/** Close interface.
 * @param interface interface to close
 */
void
BlackBoardInterfaceManager::close(Interface *interface)
{
  if ( interface == NULL ) return;
  mutex->lock();
  bool destroyed = false;

  // reduce refcount and free memory if refcount is zero
  interface_header_t *ih = (interface_header_t *)interface->__mem_real_ptr;
  bool killed_writer = interface->__write_access;
  if ( --(ih->refcount) == 0 ) {
    // redeem from memory
    memmgr->free( interface->__mem_real_ptr );
    destroyed = true;
  } else {
    if ( interface->__write_access ) {
      ih->flag_writer_active = 0;
      writer_interfaces.erase( interface->__mem_serial );
    } else {
      ih->num_readers--;
    }
  }

  mutex->unlock();
  if (killed_writer) {
    notify_of_writer_removed(interface);
  } else {
    notify_of_reader_removed(interface);
  }
  if ( destroyed ) {
    notify_of_interface_destroyed(interface->__type, interface->__id);
  }

  mutex->lock();
  delete_interface_instance( interface );
  mutex->unlock();
}


/** Get the writer interface for the given mem serial.
 * @param mem_serial memory serial to get writer for
 * @return writer interface for given mem serial, or NULL if non exists
 * @exception BlackBoardNoWritingInstanceException thrown if no writer
 * was found for the given interface.
 */
Interface *
BlackBoardInterfaceManager::writer_for_mem_serial(unsigned int mem_serial)
{
  if ( writer_interfaces.find(mem_serial) != writer_interfaces.end() ) {
    return writer_interfaces[mem_serial];
  } else {
    throw BlackBoardNoWritingInstanceException();
  }
}


bool
BlackBoardInterfaceManager::exists_writer(const Interface *interface) const
{
  return (writer_interfaces.find(interface->__mem_serial) != writer_interfaces.end());
}


unsigned int
BlackBoardInterfaceManager::num_readers(const Interface *interface) const
{
  const interface_header_t *ih = (interface_header_t *)interface->__mem_real_ptr;
  return ih->num_readers;
}


/** Register BB event listener.
 * @param listener BlackBoard event listener to register
 * @param flags an or'ed combination of BBIL_FLAG_DATA, BBIL_FLAG_READER, BBIL_FLAG_WRITER
 * and BBIL_FLAG_INTERFACE. Only for the given types the event listener is registered.
 * BBIL_FLAG_ALL can be supplied to register for all events.
 */
void
BlackBoardInterfaceManager::register_listener(BlackBoardInterfaceListener *listener,
					      unsigned int flags)
{
  if ( flags & BlackBoard::BBIL_FLAG_DATA ) {
    BlackBoardInterfaceListener::InterfaceLockHashMapIterator i;
    BlackBoardInterfaceListener::InterfaceLockHashMap *im = listener->bbil_data_interfaces();
    __bbil_data.lock();
    for (i = im->begin(); i != im->end(); ++i) {
      __bbil_data[(*i).first].push_back(listener);
    }
    __bbil_data.unlock();
  }
  if ( flags & BlackBoard::BBIL_FLAG_READER ) {
    BlackBoardInterfaceListener::InterfaceLockHashMapIterator i;
    BlackBoardInterfaceListener::InterfaceLockHashMap *im = listener->bbil_reader_interfaces();
    __bbil_reader.lock();
    for (i = im->begin(); i != im->end(); ++i) {
      __bbil_reader[(*i).first].push_back(listener);
    }
    __bbil_reader.unlock();
  }
  if ( flags & BlackBoard::BBIL_FLAG_WRITER ) {
    BlackBoardInterfaceListener::InterfaceLockHashMapIterator i;
    BlackBoardInterfaceListener::InterfaceLockHashMap *im = listener->bbil_writer_interfaces();
    __bbil_writer.lock();
    for (i = im->begin(); i != im->end(); ++i) {
      __bbil_writer[(*i).first].push_back(listener);
    }
    __bbil_writer.unlock();
  }
}

/** Unregister BB interface listener.
 * This will remove the given BlackBoard interface listener from any event that it was
 * previously registered for.
 * @param listener BlackBoard event listener to remove
 */
void
BlackBoardInterfaceManager::unregister_listener(BlackBoardInterfaceListener *listener)
{
  for (BBilLockHashMapIterator i = __bbil_data.begin(); i != __bbil_data.end(); ++i) {
    BBilListIterator j = (*i).second.begin();
    while (j != (*i).second.end()) {
      if ( *j == listener ) {
	j = (*i).second.erase(j);
      } else {
	++j;
      }
    }
  }
  for (BBilLockHashMapIterator i = __bbil_reader.begin(); i != __bbil_reader.end(); ++i) {
    BBilListIterator j = (*i).second.begin();
    while (j != (*i).second.end()) {
      if ( *j == listener ) {
	j = (*i).second.erase(j);
      } else {
	++j;
      }
    }
  }
  for (BBilLockHashMapIterator i = __bbil_writer.begin(); i != __bbil_writer.end(); ++i) {
    BBilListIterator j = (*i).second.begin();
    while (j != (*i).second.end()) {
      if ( *j == listener ) {
	j = (*i).second.erase(j);
      } else {
	++j;
      }
    }
  }
}


/** Register BB interface observer.
 * @param observer BlackBoard interface observer to register
 * @param flags an or'ed combination of BBIO_FLAG_CREATED, BBIO_FLAG_DESTROYED
 */
void
BlackBoardInterfaceManager::register_observer(BlackBoardInterfaceObserver *observer,
					      unsigned int flags)
{
  if ( flags & BlackBoard::BBIO_FLAG_CREATED ) {
    BlackBoardInterfaceObserver::InterfaceTypeLockHashSetIterator i;
    BlackBoardInterfaceObserver::InterfaceTypeLockHashSet *its = observer->bbio_interface_create_types();
    __bbio_created.lock();
    for (i = its->begin(); i != its->end(); ++i) {
      __bbio_created[*i].push_back(observer);
    }
    __bbio_created.unlock();
  }

  if ( flags & BlackBoard::BBIO_FLAG_DESTROYED ) {
    BlackBoardInterfaceObserver::InterfaceTypeLockHashSetIterator i;
    BlackBoardInterfaceObserver::InterfaceTypeLockHashSet *its = observer->bbio_interface_destroy_types();
    __bbio_destroyed.lock();
    for (i = its->begin(); i != its->end(); ++i) {
      __bbio_destroyed[*i].push_back(observer);
    }
    __bbio_destroyed.unlock();
  }
}


/** Unregister BB interface observer.
 * This will remove the given BlackBoard event listener from any event that it was
 * previously registered for.
 * @param observer BlackBoard event listener to remove
 */
void
BlackBoardInterfaceManager::unregister_observer(BlackBoardInterfaceObserver *observer)
{
  for (BBioLockHashMapIterator i = __bbio_created.begin(); i != __bbio_created.end(); ++i) {
    BBioListIterator j = (*i).second.begin();
    while (j != (*i).second.end()) {
      if ( *j == observer ) {
	j = (*i).second.erase(j);
      } else {
	++j;
      }
    }
  }

  for (BBioLockHashMapIterator i = __bbio_destroyed.begin(); i != __bbio_destroyed.end(); ++i) {
    BBioListIterator j = (*i).second.begin();
    while (j != (*i).second.end()) {
      if ( *j == observer ) {
	j = (*i).second.erase(j);
      } else {
	++j;
      }
    }
  }
}

/** Notify that an interface has been created.
 * @param type type of the interface
 * @param id ID of the interface
 */
void
BlackBoardInterfaceManager::notify_of_interface_created(const char *type, const char *id) throw()
{
  BBioLockHashMapIterator lhmi;
  BBioListIterator i, l;
  __bbio_created.lock();
  if ( (lhmi = __bbio_created.find(type)) != __bbio_created.end() ) {
    BBioList &list = (*lhmi).second;
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceObserver *bbio = (*i);
      bbio->bb_interface_created(type, id);
    }
  }
  __bbio_created.unlock();
}


/** Notify that an interface has been destroyed.
 * @param type type of the interface
 * @param id ID of the interface
 */
void
BlackBoardInterfaceManager::notify_of_interface_destroyed(const char *type, const char *id) throw()
{
  BBioLockHashMapIterator lhmi;
  BBioListIterator i, l;
  __bbio_destroyed.lock();
  if ( (lhmi = __bbio_destroyed.find(type)) != __bbio_destroyed.end() ) {
    BBioList &list = (*lhmi).second;
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceObserver *bbio = (*i);
      bbio->bb_interface_destroyed(type, id);
    }
  }
  __bbio_destroyed.unlock();
}


/** Notify that writer has been added.
 * @param uid UID of interface
 */
void
BlackBoardInterfaceManager::notify_of_writer_added(const char *uid) throw()
{
  BBilLockHashMapIterator lhmi;
  BBilListIterator i, l;
  if ( (lhmi = __bbil_writer.find(uid)) != __bbil_writer.end() ) {
    BBilList &list = (*lhmi).second;
    __bbil_writer.lock();
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceListener *bbil = (*i);
      Interface *bbil_iface = bbil->bbil_writer_interface(uid);
      if (bbil_iface != NULL ) {
	bbil->bb_interface_writer_added(bbil_iface);
      } else {
	LibLogger::log_warn("BlackBoardInterfaceManager", "BBIL registered for writer "
			    "events (open) for '%s' but has no such interface", uid);
      }
    }
    __bbil_writer.unlock();
  }
}


/** Notify that writer has been removed.
 * @param uid UID of interface
 */
void
BlackBoardInterfaceManager::notify_of_writer_removed(const Interface *interface) throw()
{
  BBilLockHashMapIterator lhmi;
  BBilListIterator i, l;
  __bbil_writer.lock();
  const char *uid = interface->uid();
  if ( (lhmi = __bbil_writer.find(uid)) != __bbil_writer.end() ) {
    BBilList &list = (*lhmi).second;
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceListener *bbil = (*i);
      Interface *bbil_iface = bbil->bbil_writer_interface(uid);
      if (bbil_iface != NULL ) {
	if ( bbil_iface->serial() == interface->serial() ) {
	  LibLogger::log_warn("BlackBoardInterfaceManager", "Interface instance (writing) "
			      "for %s removed, but interface instance still in BBIL, this "
			      "will lead to a fatal problem shortly", uid);
	} else {
	  bbil->bb_interface_writer_removed(bbil_iface);
	}
      } else {
	LibLogger::log_warn("BlackBoardInterfaceManager", "BBIL registered for writer "
			    "events (close) for '%s' but has no such interface", uid);
      }
    }
  }
  __bbil_writer.unlock();
}


/** Notify that reader has been added.
 * @param uid UID of interface
 */
void
BlackBoardInterfaceManager::notify_of_reader_added(const char *uid) throw()
{
  BBilLockHashMapIterator lhmi;
  BBilListIterator i, l;
  __bbil_reader.lock();
  if ( (lhmi = __bbil_reader.find(uid)) != __bbil_reader.end() ) {
    BBilList &list = (*lhmi).second;
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceListener *bbil = (*i);
      Interface *bbil_iface = bbil->bbil_reader_interface(uid);
      if (bbil_iface != NULL ) {
	bbil->bb_interface_reader_added(bbil_iface);
      } else {
	LibLogger::log_warn("BlackBoardInterfaceManager", "BBIL registered for reader "
			    "events (open) for '%s' but has no such interface", uid);
      }
    }
  }
  __bbil_reader.unlock();
}


/** Notify that reader has been removed.
 * @param uid UID of interface
 */
void
BlackBoardInterfaceManager::notify_of_reader_removed(const Interface *interface) throw()
{
  BBilLockHashMapIterator lhmi;
  BBilListIterator i, l;
  const char *uid = interface->uid();
  if ( (lhmi = __bbil_reader.find(uid)) != __bbil_reader.end() ) {
    BBilList &list = (*lhmi).second;
    __bbil_reader.lock();
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceListener *bbil = (*i);
      Interface *bbil_iface = bbil->bbil_reader_interface(uid);
      if (bbil_iface != NULL ) {
	if ( bbil_iface->serial() == interface->serial() ) {
	  LibLogger::log_warn("BlackBoardInterfaceManager", "Interface instance (reading) "
			      "for %s removed, but interface instance still in BBIL, this "
			      "will lead to a fatal problem shortly", uid);
	} else {
	  bbil->bb_interface_reader_removed(bbil_iface);
	}
      } else {
	LibLogger::log_warn("BlackBoardInterfaceManager", "BBIL registered for reader "
			    "events (close) for '%s' but has no such interface", uid);
      }
    }
    __bbil_reader.unlock();
  }
}


/** Notify of data change.
 * Notify all subscribers of the given interface of a data change.
 * This also influences logging and sending data over the network so it is
 * mandatory to call this function! The interface base class write method does
 * that for you.
 * @param interface interface whose subscribers to notify
 * @see Interface::write()
 */
void
BlackBoardInterfaceManager::notify_of_data_change(const Interface *interface)
{
  BBilLockHashMapIterator lhmi;
  BBilListIterator i, l;
  const char *uid = interface->uid();
  if ( (lhmi = __bbil_data.find(uid)) != __bbil_data.end() ) {
    BBilList &list = (*lhmi).second;
    __bbil_data.lock();
    for (i = list.begin(); i != list.end(); ++i) {
      BlackBoardInterfaceListener *bbil = (*i);
      Interface *bbil_iface = bbil->bbil_data_interface(uid);
      if (bbil_iface != NULL ) {
	bbil->bb_interface_data_changed(bbil_iface);
      } else {
	LibLogger::log_warn("BlackBoardInterfaceManager", "BBIL registered for data change "
			    "events for '%s' but has no such interface", uid);
      }
    }
    __bbil_data.unlock();
  }
}
