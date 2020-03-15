
/***************************************************************************
 *  mongorrd_thread.cpp - MongoDB RRD Thread
 *
 *  Created: Sat Jan 15 18:42:39 2011
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

#include "mongorrd_thread.h"

#include <utils/time/wait.h>

// from MongoDB
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/exception.hpp>

using namespace mongocxx;
using namespace fawkes;

#define DB_CONF_PREFIX "/plugins/mongorrd/databases/"

/** @class MongoRRDThread "mongorrd_thread.h"
 * MongoDB RRD Thread.
 * This thread queries performance data from MongoDB every 10 seconds and
 * writes it to RRD databases.
 *
 * @author Tim Niemueller
 */

/** Constructor. */
MongoRRDThread::MongoRRDThread()
: Thread("MongoRRDThread", Thread::OPMODE_CONTINUOUS),
  MongoDBAspect("default"),
  ConfigurationChangeHandler(DB_CONF_PREFIX)
{
	set_prepfin_conc_loop(true);
}

/** Destructor. */
MongoRRDThread::~MongoRRDThread()
{
}

void
MongoRRDThread::init()
{
	timewait_ = new TimeWait(clock, 10 * 1000000);

	opcounters_graph_ = NULL;
	memory_graph_     = NULL;
	indexes_graph_    = NULL;

	std::vector<RRDDataSource> rrds;
	rrds.push_back(RRDDataSource("insert", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("query", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("update", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("delete", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("getmore", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("command", RRDDataSource::COUNTER));
	opcounters_rrd_ = new RRDDefinition("opcounters", rrds);

	rrds.clear();
	rrds.push_back(RRDDataSource("resident", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("virtual", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("mapped", RRDDataSource::GAUGE));
	memory_rrd_ = new RRDDefinition("memory", rrds);

	rrds.clear();
	rrds.push_back(RRDDataSource("accesses", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("hits", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("misses", RRDDataSource::COUNTER));
	rrds.push_back(RRDDataSource("resets", RRDDataSource::COUNTER));
	indexes_rrd_ = new RRDDefinition("indexes", rrds);

	rrds.clear();
	rrds.push_back(RRDDataSource("locktime", RRDDataSource::COUNTER));
	locks_rrd_ = new RRDDefinition("locks", rrds);

	try {
		rrd_manager->add_rrd(opcounters_rrd_);
		rrd_manager->add_rrd(memory_rrd_);
		rrd_manager->add_rrd(indexes_rrd_);
		rrd_manager->add_rrd(locks_rrd_);
	} catch (Exception &e) {
		finalize();
		throw;
	}

	std::vector<RRDGraphDataDefinition> defs;
	std::vector<RRDGraphElement *>      els;

	defs.push_back(RRDGraphDataDefinition("insert", RRDArchive::AVERAGE, opcounters_rrd_));
	defs.push_back(RRDGraphDataDefinition("query", RRDArchive::AVERAGE, opcounters_rrd_));
	defs.push_back(RRDGraphDataDefinition("update", RRDArchive::AVERAGE, opcounters_rrd_));
	defs.push_back(RRDGraphDataDefinition("delete", RRDArchive::AVERAGE, opcounters_rrd_));
	defs.push_back(RRDGraphDataDefinition("getmore", RRDArchive::AVERAGE, opcounters_rrd_));
	defs.push_back(RRDGraphDataDefinition("command", RRDArchive::AVERAGE, opcounters_rrd_));

	els.push_back(new RRDGraphLine("insert", 1, "FF7200", "Inserts"));
	els.push_back(new RRDGraphGPrint("insert", RRDArchive::LAST, " Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("insert", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("insert", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("query", 1, "503001", "Queries"));
	els.push_back(new RRDGraphGPrint("query", RRDArchive::LAST, " Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("query", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("query", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("update", 1, "EDAC00", "Updates"));
	els.push_back(new RRDGraphGPrint("update", RRDArchive::LAST, " Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("update", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("update", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("delete", 1, "506101", "Deletes"));
	els.push_back(new RRDGraphGPrint("delete", RRDArchive::LAST, " Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("delete", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("delete", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("getmore", 1, "0CCCCC", "Getmores"));
	els.push_back(new RRDGraphGPrint("getmore", RRDArchive::LAST, "Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("getmore", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("getmore", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("command", 1, "53CA05", "Commands"));
	els.push_back(new RRDGraphGPrint("command", RRDArchive::LAST, "Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("command", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("command", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	opcounters_graph_ = new RRDGraphDefinition(
	  "opcounters", opcounters_rrd_, "MongoDB Op Counters", "Ops/sec", defs, els);

	defs.clear();
	els.clear();
	defs.push_back(
	  RRDGraphDataDefinition("rawresident", RRDArchive::AVERAGE, memory_rrd_, "resident"));
	defs.push_back(RRDGraphDataDefinition("rawvirtual", RRDArchive::AVERAGE, memory_rrd_, "virtual"));
	defs.push_back(RRDGraphDataDefinition("rawmapped", RRDArchive::AVERAGE, memory_rrd_, "mapped"));
	defs.push_back(RRDGraphDataDefinition("resident", "rawresident,1048576,*"));
	defs.push_back(RRDGraphDataDefinition("virtual", "rawvirtual,1048576,*"));
	defs.push_back(RRDGraphDataDefinition("mapped", "rawmapped,1048576,*"));

	els.push_back(new RRDGraphArea("virtual", "3B7AD9", "Virtual"));
	els.push_back(new RRDGraphGPrint("virtual", RRDArchive::LAST, " Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("virtual", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("virtual", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphArea("mapped", "6FD1BF", "Mapped"));
	els.push_back(new RRDGraphGPrint("mapped", RRDArchive::LAST, "  Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("mapped", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("mapped", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphArea("resident", "0E6E5C", "Resident"));
	els.push_back(new RRDGraphGPrint("resident", RRDArchive::LAST, "Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("resident", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("resident", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	memory_graph_ =
	  new RRDGraphDefinition("memory", memory_rrd_, "MongoDB Memory Usage", "MB", defs, els);

	defs.clear();
	els.clear();
	defs.push_back(RRDGraphDataDefinition("accesses", RRDArchive::AVERAGE, indexes_rrd_));
	defs.push_back(RRDGraphDataDefinition("hits", RRDArchive::AVERAGE, indexes_rrd_));
	defs.push_back(RRDGraphDataDefinition("misses", RRDArchive::AVERAGE, indexes_rrd_));
	defs.push_back(RRDGraphDataDefinition("resets", RRDArchive::AVERAGE, indexes_rrd_));

	els.push_back(new RRDGraphLine("accesses", 1, "FF7200", "Accesses"));
	els.push_back(new RRDGraphGPrint("accesses", RRDArchive::LAST, "Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("accesses", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("accesses", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("hits", 1, "503001", "Hits"));
	els.push_back(new RRDGraphGPrint("hits", RRDArchive::LAST, "    Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("hits", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("hits", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("misses", 1, "EDAC00", "Misses"));
	els.push_back(new RRDGraphGPrint("misses", RRDArchive::LAST, "  Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("misses", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("misses", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("resets", 1, "506101", "Resets"));
	els.push_back(new RRDGraphGPrint("resets", RRDArchive::LAST, "  Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("resets", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("resets", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	indexes_graph_ =
	  new RRDGraphDefinition("indexes", indexes_rrd_, "MongoDB Indexes", "", defs, els);

	try {
		rrd_manager->add_graph(opcounters_graph_);
		rrd_manager->add_graph(memory_graph_);
		rrd_manager->add_graph(indexes_graph_);
	} catch (Exception &e) {
		finalize();
		throw;
	}

	// Add DB Stats
	std::string dbprefix = DB_CONF_PREFIX;

	Configuration::ValueIterator *i = config->search(dbprefix.c_str());
	while (i->next()) {
		if (!i->is_string()) {
			logger->log_warn(name(),
			                 "Entry %s is not a string, but of type %s, "
			                 "ignoring",
			                 i->path(),
			                 i->type());
			continue;
		}

		std::string dbname = i->get_string();
		if (dbname.find(".") != std::string::npos) {
			logger->log_warn(name(), "Database name %s contains dot, ignoring", dbname.c_str());
			continue;
		}

		try {
			add_dbstats(i->path(), dbname);
		} catch (Exception &e) {
			finalize();
			throw;
		}
	}

	config->add_change_handler(this);
}

void
MongoRRDThread::finalize()
{
	config->rem_change_handler(this);
	delete timewait_;

	rrd_manager->remove_rrd(opcounters_rrd_);
	rrd_manager->remove_rrd(memory_rrd_);
	rrd_manager->remove_rrd(indexes_rrd_);
	rrd_manager->remove_rrd(locks_rrd_);

	for (DbStatsMap::iterator i = dbstats_.begin(); i != dbstats_.end(); ++i) {
		DbStatsInfo &info = i->second;
		rrd_manager->remove_rrd(info.rrd);
		delete info.graph1;
		delete info.graph2;
		delete info.graph3;
		delete info.rrd;
	}
	dbstats_.clear();

	delete opcounters_graph_;
	delete memory_graph_;
	delete indexes_graph_;

	delete opcounters_rrd_;
	delete memory_rrd_;
	delete indexes_rrd_;
	delete locks_rrd_;
}

void
MongoRRDThread::add_dbstats(const char *path, std::string dbname)
{
	if (dbstats_.find(path) != dbstats_.end()) {
		throw Exception("Database stats for config %s already monitored", path);
	}

	DbStatsInfo info;

	std::vector<RRDDataSource> rrds;
	rrds.push_back(RRDDataSource("collections", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("objects", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("avgObjSize", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("dataSize", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("storageSize", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("numExtents", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("indexes", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("indexSize", RRDDataSource::GAUGE));
	rrds.push_back(RRDDataSource("fileSize", RRDDataSource::GAUGE));

	info.db_name  = dbname;
	info.rrd_name = std::string("dbstats_") + dbname;
	info.rrd      = new RRDDefinition(info.rrd_name.c_str(), rrds);

	std::vector<RRDGraphDataDefinition> defs;
	std::vector<RRDGraphElement *>      els;

	defs.push_back(RRDGraphDataDefinition("collections", RRDArchive::AVERAGE, info.rrd));
	defs.push_back(RRDGraphDataDefinition("indexes", RRDArchive::AVERAGE, info.rrd));
	defs.push_back(RRDGraphDataDefinition("numExtents", RRDArchive::AVERAGE, info.rrd));

	els.push_back(new RRDGraphLine("collections", 1, "FF7200", "Collections"));
	els.push_back(new RRDGraphGPrint("collections", RRDArchive::LAST, "Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("collections", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("collections", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("indexes", 1, "EDAC00", "Indexes"));
	els.push_back(new RRDGraphGPrint("indexes", RRDArchive::LAST, "    Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("indexes", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("indexes", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("numExtents", 1, "506101", "Extents"));
	els.push_back(new RRDGraphGPrint("numExtents", RRDArchive::LAST, "    Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("numExtents", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("numExtents", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	std::string g1name  = info.rrd_name + "_collindext";
	std::string g1title = std::string("MongoDB Collections, Indexes, Extents for ") + dbname;
	info.graph1 = new RRDGraphDefinition(g1name.c_str(), info.rrd, g1title.c_str(), "", defs, els);

	defs.clear();
	els.clear();
	defs.push_back(RRDGraphDataDefinition("objects", RRDArchive::AVERAGE, info.rrd));

	els.push_back(new RRDGraphLine("objects", 1, "FF7200", "Objects"));
	els.push_back(new RRDGraphGPrint("objects", RRDArchive::LAST, " Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("objects", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("objects", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	std::string g2name  = info.rrd_name + "_objects";
	std::string g2title = std::string("MongoDB Objects for ") + dbname;
	info.graph2 = new RRDGraphDefinition(g2name.c_str(), info.rrd, g2title.c_str(), "", defs, els);

	defs.clear();
	els.clear();
	defs.push_back(RRDGraphDataDefinition("avgObjSize", RRDArchive::AVERAGE, info.rrd));
	defs.push_back(RRDGraphDataDefinition("dataSize", RRDArchive::AVERAGE, info.rrd));
	defs.push_back(RRDGraphDataDefinition("storageSize", RRDArchive::AVERAGE, info.rrd));
	defs.push_back(RRDGraphDataDefinition("indexSize", RRDArchive::AVERAGE, info.rrd));
	defs.push_back(RRDGraphDataDefinition("fileSize", RRDArchive::AVERAGE, info.rrd));

	els.push_back(new RRDGraphLine("avgObjSize", 1, "FF7200", "Avg Obj Sz"));
	els.push_back(new RRDGraphGPrint("avgObjSize", RRDArchive::LAST, "Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("avgObjSize", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("avgObjSize", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("dataSize", 1, "503001", "Data"));
	els.push_back(new RRDGraphGPrint("dataSize", RRDArchive::LAST, "      Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("dataSize", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("dataSize", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("storageSize", 1, "EDAC00", "Storage"));
	els.push_back(new RRDGraphGPrint("storageSize", RRDArchive::LAST, "   Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("storageSize", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("storageSize", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("indexSize", 1, "506101", "Index"));
	els.push_back(new RRDGraphGPrint("indexSize", RRDArchive::LAST, "     Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("indexSize", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("indexSize", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	els.push_back(new RRDGraphLine("fileSize", 1, "0CCCCC", "File"));
	els.push_back(new RRDGraphGPrint("fileSize", RRDArchive::LAST, "      Current\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("fileSize", RRDArchive::AVERAGE, "Average\\:%8.2lf %s"));
	els.push_back(new RRDGraphGPrint("fileSize", RRDArchive::MAX, "Maximum\\:%8.2lf %s\\n"));

	std::string g3name  = info.rrd_name + "_sizes";
	std::string g3title = std::string("MongoDB Sizes for ") + dbname;
	info.graph3 = new RRDGraphDefinition(g3name.c_str(), info.rrd, g3title.c_str(), "Mem", defs, els);

	rrd_manager->add_rrd(info.rrd);
	try {
		rrd_manager->add_graph(info.graph1);
		rrd_manager->add_graph(info.graph2);
		rrd_manager->add_graph(info.graph3);

		dbstats_[dbname] = info;
		logger->log_info(name(), "Started monitoring MongoDB %s", info.db_name.c_str());
	} catch (Exception &e) {
		rrd_manager->remove_rrd(info.rrd);
		delete info.graph1;
		delete info.graph2;
		delete info.graph3;
		delete info.rrd;
		throw;
	}
}

void
MongoRRDThread::remove_dbstats(const char *path)
{
	if (dbstats_.find(path) != dbstats_.end()) {
		DbStatsInfo &info = dbstats_[path];
		rrd_manager->remove_rrd(info.rrd);
		delete info.graph1;
		delete info.graph2;
		delete info.graph3;
		delete info.rrd;

		logger->log_info(name(), "Stopped monitoring MongoDB %s", info.db_name.c_str());
		dbstats_.erase(path);
	}
}

void
MongoRRDThread::loop()
{
	timewait_->mark_start();
	using namespace bsoncxx::builder;

	try {
		auto reply = mongodb_client->database("admin").run_command(
		  basic::make_document(basic::kvp("serverStatus", 1)));
		if (int(reply.view()["ok"].get_double()) == 1) {
			auto    opcounters = reply.view()["opcounters"].get_document().view();
			int64_t insert, query, update, del, getmore, command;
			insert  = opcounters["insert"].get_int64();
			query   = opcounters["query"].get_int64();
			update  = opcounters["update"].get_int64();
			del     = opcounters["delete"].get_int64();
			getmore = opcounters["getmore"].get_int64();
			command = opcounters["command"].get_int64();

			try {
				rrd_manager->add_data(
				  "opcounters", "N:%i:%i:%i:%i:%i:%i", insert, query, update, del, getmore, command);
			} catch (Exception &e) {
				logger->log_warn(name(),
				                 "Failed to update opcounters RRD, "
				                 "exception follows");
				logger->log_warn(name(), e);
			}

			auto    mem = reply.view()["mem"].get_document().view();
			int64_t resident, virtmem, mapped;
			resident = mem["resident"].get_int64();
			virtmem  = mem["virtual"].get_int64();
			mapped   = mem["mapped"].get_int64();

			try {
				rrd_manager->add_data("memory", "N:%i:%i:%i", resident, virtmem, mapped);
			} catch (Exception &e) {
				logger->log_warn(name(), "Failed to update memory RRD, exception follows");
				logger->log_warn(name(), e);
			}

			auto indexc = reply.view()["indexCounters"]["btree"].get_document().view();
			int  accesses, hits, misses, resets;
			accesses = indexc["accesses"].get_int64();
			hits     = indexc["hits"].get_int64();
			misses   = indexc["misses"].get_int64();
			resets   = indexc["resets"].get_int64();

			try {
				rrd_manager->add_data("indexes", "N:%i:%i:%i:%i", accesses, hits, misses, resets);
			} catch (Exception &e) {
				logger->log_warn(name(),
				                 "Failed to update indexes RRD, "
				                 "exception follows");
				logger->log_warn(name(), e);
			}

			for (DbStatsMap::iterator i = dbstats_.begin(); i != dbstats_.end(); ++i) {
				auto dbstats = mongodb_client->database(i->second.db_name)
				                 .run_command(basic::make_document(basic::kvp("dbStats", 1)));
				if (int(dbstats.view()["ok"].get_double()) == 1) {
					try {
						int64_t collections, objects, numExtents, indexes, dataSize, storageSize, indexSize,
						  fileSize;
						double avgObjSize;

						collections = dbstats.view()["collections"].get_int64();
						objects     = dbstats.view()["objects"].get_int64();
						avgObjSize  = dbstats.view()["avgObjSize"].get_double();
						dataSize    = dbstats.view()["dataSize"].get_int64();
						storageSize = dbstats.view()["storageSize"].get_int64();
						numExtents  = dbstats.view()["numExtents"].get_int64();
						indexes     = dbstats.view()["indexes"].get_int64();
						indexSize   = dbstats.view()["indexSize"].get_int64();
						fileSize    = dbstats.view()["fileSize"].get_int64();

						try {
							rrd_manager->add_data(i->second.rrd_name.c_str(),
							                      "N:%li:%li:%f:%li:%li:%li:%li:%li:%li",
							                      collections,
							                      objects,
							                      avgObjSize,
							                      dataSize,
							                      storageSize,
							                      numExtents,
							                      indexes,
							                      indexSize,
							                      fileSize);
						} catch (Exception &e) {
							logger->log_warn(name(),
							                 "Failed to update dbstates RRD for "
							                 "%s exception follows",
							                 i->second.db_name.c_str());
							logger->log_warn(name(), e);
						}

					} catch (mongocxx::exception &e) {
						logger->log_warn(name(),
						                 "Failed to update MongoDB RRD for database "
						                 "%s: %s",
						                 i->second.db_name.c_str(),
						                 e.what());
					}
				} else {
					logger->log_warn(name(),
					                 "Failed to retrieve db stats for %s, reply: %s",
					                 i->second.db_name.c_str(),
					                 bsoncxx::to_json(dbstats.view()).c_str());
				}
			}

			//double locktime = reply.view()["globalLock"].get_document().view()["lockTime"].get_double();

		} else {
			logger->log_warn(name(),
			                 "Failed to retrieve server status, reply: %s",
			                 bsoncxx::to_json(reply.view()).c_str());
		}

	} catch (mongocxx::exception &e) {
		logger->log_warn(name(), "Failed to update MongoDB RRD: %s", e.what());
	}

	timewait_->wait_systime();
}

void
MongoRRDThread::config_tag_changed(const char *new_tag)
{
	// ignored
}

void
MongoRRDThread::config_value_changed(const Configuration::ValueIterator *v)
{
	if (v->is_string()) {
		remove_dbstats(v->path());
		add_dbstats(v->path(), v->get_string());
	} else {
		logger->log_warn(name(), "Non-string value at %s, ignoring", v->path());
	}
}

void
MongoRRDThread::config_comment_changed(const Configuration::ValueIterator *v)
{
}

void
MongoRRDThread::config_value_erased(const char *path)
{
	remove_dbstats(path);
}
