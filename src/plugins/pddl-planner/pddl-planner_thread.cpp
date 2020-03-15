
/***************************************************************************
 *  pddl-planner_thread.cpp - pddl-planner
 *
 *  Created: Wed Dec  7 19:09:44 2016
 *  Copyright  2016  Frederik Zwilling
 *             2017  Matthias Loebach
 *             2017  Till Hofmann
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

#include "pddl-planner_thread.h"

#include <utils/misc/string_conversions.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <utils/misc/string_conversions.h>
#include <unistd.h>
#include <ctime>
#include <cstdlib>

using namespace fawkes;
using namespace mongocxx;
using namespace bsoncxx;

/** @class PddlPlannerThread 'pddl-planner_thread.h' 
 * Starts a pddl planner and writes the resulting plan into the robot memory
 * @author Frederik Zwilling
 */

/** Constructor. */
PddlPlannerThread::PddlPlannerThread()
: Thread("PddlPlannerThread", Thread::OPMODE_WAITFORWAKEUP),
  BlackBoardInterfaceListener("PddlPlannerThread")
{
}

void
PddlPlannerThread::init()
{
	//read config
	std::string cfg_prefix = "plugins/pddl-planner/";
	cfg_descripton_path_ =
	  StringConversions::resolve_path(config->get_string((cfg_prefix + "description-folder")));
	cfg_result_path_  = cfg_descripton_path_ + config->get_string((cfg_prefix + "result-file"));
	cfg_domain_path_  = cfg_descripton_path_ + config->get_string(cfg_prefix + "domain-description");
	cfg_problem_path_ = cfg_descripton_path_ + config->get_string(cfg_prefix + "problem-description");
	cfg_fd_options_ = config->get_string(cfg_prefix + "fd-search-opts");
	cfg_kstar_options_ = config->get_string(cfg_prefix + "kstar-search-opts");
  cfg_collection_ = config->get_string(cfg_prefix + "collection");

	//set configured planner
	std::string planner_string = config->get_string((cfg_prefix + "planner").c_str());
	if (planner_string == "ff") {
		planner_ = std::bind(&PddlPlannerThread::ff_planner, this);
		logger->log_info(name(), "Fast-Forward planner selected.");
	} else if (planner_string == "fd") {
		planner_ = std::bind(&PddlPlannerThread::fd_planner, this);
		logger->log_info(name(), "Fast-Downward planner selected.");
	} else if (planner_string == "dbmp") {
		planner_ = std::bind(&PddlPlannerThread::dbmp_planner, this);
		logger->log_info(name(), "DBMP selected.");
	} else if ( planner_string == "kstar" ) {
    planner_ = std::bind(&PddlPlannerThread::kstar_planner, this);
    logger->log_info(name(), "KStar selected.");
  }	else {
		planner_ = std::bind(&PddlPlannerThread::ff_planner, this);
		logger->log_warn(name(), "No planner configured.\nDefaulting to ff.");
	}

	//setup interface
	plan_if_ = blackboard->open_for_writing<PddlPlannerInterface>(
	  config->get_string(cfg_prefix + "interface-name").c_str());
	plan_if_->set_active_planner(planner_string.c_str());
	plan_if_->set_msg_id(0);
	plan_if_->set_final(false);
	plan_if_->set_success(false);
	plan_if_->write();

	//setup interface listener
	bbil_add_message_interface(plan_if_);
	blackboard->register_listener(this, BlackBoard::BBIL_FLAG_MESSAGES);

	// If we receive multiple wakeup() calls during loop, only call the loop once afterwards
	// We want to only re-plan once after a loop run since multiple runs would plan on the same problem
	this->set_coalesce_wakeups(true);
}

/**
 * Thread is only waked up if there was a new interface message to plan
 */
void
PddlPlannerThread::loop()
{
  logger->log_info(name(), "Starting PDDL Planning...");

  //writes plan into action_list_ or plan_list_ if it is a diagnose

  // Check if domain and problem file exist
  if (FILE *file = fopen(cfg_domain_path_.c_str(),"r")) {
    fclose(file);
  }
  else {
    logger->log_error(name(),"Can not find %s",cfg_domain_path_.c_str());
    return;
  }

  if (FILE *file = fopen(cfg_problem_path_.c_str(),"r")) {
    fclose(file);
  } else {
    logger->log_error(name(),"Can not find %s",cfg_problem_path_.c_str());
    return;
  }

  planner_();

  // Generated a diagnose
  // Have to add multiple plans (possible diagnosises) to robot memory
  if ( !plan_list_.empty() ) {
    int id = 0;
    for(std::vector<Action> plan : plan_list_){
      std::string matching = std::string(" { plan :" + std::to_string(id) + " } ");
      auto bson_plan = BSONFromActionList(plan,id++);
      if(robot_memory->update(from_json(matching).view(), bson_plan, cfg_collection_,true) == 0 ){
        logger->log_error(name(),"Failed to update plan");
      }
    }
    plan_if_->set_success(true);
  } else {
    // Standard plan
    if ( !action_list_.empty() ) {
  		auto plan = BSONFromActionList(action_list_,0);
  		robot_memory->update(from_json("{plan:{$exists:true}}").view(), plan, cfg_collection_, true);
  		print_action_list();
  		plan_if_->set_success(true);
  	} else {
  		logger->log_error(name(), "Updating plan failed, action list empty!");
  		robot_memory->update(from_json("{plan:{$exists:true}}").view(),
		                     from_json("{plan:0}").view(),
		                     cfg_collection_,
		                     true);
  		plan_if_->set_success(false);
    }
  }

  plan_if_->set_final(true);
  plan_if_->write();
}

void
PddlPlannerThread::finalize()
{
	blackboard->close(plan_if_);
}

void
PddlPlannerThread::ff_planner()
{
	logger->log_info(name(), "Starting PDDL Planning with Fast-Forward...");

	std::string command = "ff -o " + cfg_domain_path_ + " -f " + cfg_problem_path_;
	logger->log_info(name(), "Calling %s", command.c_str());
	std::string result = run_planner(command);

	//Parse Result and write it into the robot memory
	logger->log_info(name(), "Parsing result");

	action_list_.clear();

	size_t cur_pos = 0;
	if (result.find("found legal plan as follows", cur_pos) == std::string::npos) {
		logger->log_error(name(), "Planning Failed: %s", result.c_str());
		robot_memory->update(from_json("{plan:{$exists:true}}").view(),
		                     from_json("{plan:1,fail:1,steps:[]}").view(),
		                     cfg_collection_,
		                     true);
		return;
	}
	//remove stuff that could confuse us later
	result.erase(result.find("time spent:", cur_pos));

	cur_pos = result.find("step", cur_pos) + 4;
	while (result.find(": ", cur_pos) != std::string::npos) {
		cur_pos         = result.find(": ", cur_pos) + 2;
		size_t line_end = result.find("\n", cur_pos);
		logger->log_info(name(),
		                 "line:%s (%zu-%zu)",
		                 result.substr(cur_pos, line_end - cur_pos).c_str(),
		                 cur_pos,
		                 line_end);
		Action a;
		if (line_end < result.find(" ", cur_pos)) {
			a.name = result.substr(cur_pos, line_end - cur_pos);
		} else {
			size_t action_end = result.find(" ", cur_pos);
			a.name            = StringConversions::to_lower(result.substr(cur_pos, action_end - cur_pos));
			cur_pos           = action_end + 1;
			while (cur_pos < line_end) {
				size_t arg_end = result.find(" ", cur_pos);
				if (arg_end > line_end) {
					arg_end = line_end;
				}
				a.args.push_back(result.substr(cur_pos, arg_end - cur_pos));
				cur_pos = arg_end + 1;
			}
		}
		action_list_.push_back(a);
	}
}

void
PddlPlannerThread::dbmp_planner()
{
	logger->log_info(name(), "Starting PDDL Planning with DBMP...");

	std::string command =
	  "dbmp.py -p ff --output plan.pddl " + cfg_domain_path_ + " " + cfg_problem_path_;
	logger->log_info(name(), "Calling %s", command.c_str());
	std::string result = run_planner(command);

	//Parse Result and write it into the robot memory
	logger->log_info(name(), "Parsing result");

	size_t cur_pos = 0;
	if (result.find("Planner failed", cur_pos) != std::string::npos) {
		logger->log_error(name(), "Planning Failed: %s", result.c_str());
		robot_memory->update(from_json("{plan:{$exists:true}}").view(),
		                     from_json("{plan:1,fail:1,steps:[]}").view(),
		                     cfg_collection_,
		                     true);
		return;
	}
	std::ifstream planfile("plan.pddl");
	std::string   line;
	action_list_.clear();
	while (std::getline(planfile, line)) {
		std::string time_string = "Time";
		if (line.compare(0, time_string.size(), time_string) == 0) {
			// makespan, skip
			continue;
		}
		if (line[0] != '(' || line[line.size() - 1] != ')') {
			logger->log_error(name(), "Expected parantheses in line '%s'!", line.c_str());
			return;
		}
		// remove parantheses
		std::string action_str = line.substr(1, line.size() - 2);
		Action      a;
		cur_pos = action_str.find(" ", cur_pos + 1);
		a.name  = StringConversions::to_lower(action_str.substr(0, cur_pos));
		while (cur_pos != std::string::npos) {
			size_t word_start = cur_pos + 1;
			cur_pos           = action_str.find(" ", word_start);
			a.args.push_back(action_str.substr(word_start, cur_pos - word_start));
		}
		action_list_.push_back(a);
	}
}

/**
 * @brief Use Kstar planner for generating the top k plans for the given pddl planning problem
 * 
 */
void
PddlPlannerThread::kstar_planner()
{
  logger->log_info(name(), "Starting PDDL Planning with Fast-Downward: KStar...");

	std::string command = "fast-downward.py"
		+ std::string(" ") + cfg_domain_path_
		+ std::string(" ") + cfg_problem_path_;

	if ( !cfg_fd_options_.empty() ) {
		command += std::string(" ") + cfg_kstar_options_;
	}

	std::string result = run_planner(command);

  logger->log_info(name(),"Removing temporary planner output.");
  std::remove("output");
  std::remove("output.sas");

  size_t cur_pos = 0;
  if ( result.find("Plan id:", cur_pos) == std::string::npos) {
    logger->log_error(name(), "Planning Failed: %s", result.c_str());
    throw Exception("No solution found");
  } else {
    cur_pos = result.find("Plan id:", cur_pos);
  }

  result.erase(0,cur_pos);
  size_t end_pos = result.find("Actual search time: ");
  if (end_pos == std::string::npos) {
    logger->log_error(name(),"Expected \"Actual search time: \" at the end of planner output but did not found");
    throw Exception("Unexpected planner output");
  }
  result.erase(end_pos, result.size()-1);

  std::istringstream iss(result);
  std::string line;

  // remove surplus line
  std::vector<Action> curr_plan;
  while ( getline(iss, line) ) {
    // Begin of a new plan. Push current plan to plan list
    if (line.find("Plan id:") != std::string::npos){
      if (!curr_plan.empty()) {
        plan_list_.push_back(curr_plan);
        curr_plan.clear();
      }
      continue;
    }
    if (line.find("Plan length") != std::string::npos ||
        line.find("Plan cost") != std::string::npos ||
        line.find("order") != std::string::npos ||
        line == ""){
      continue;
    }

    Action a;
    if (line.find("cost") != std::string::npos) {
      // Update cost of last action in plan
      if (!curr_plan.empty() && curr_plan.back().cost == 0) {
        curr_plan.back().cost = std::stof(line.substr(line.find("cost")+5, line.length() - 1));
      } 
      
    } else {
      // current line is assumed to contain an action
      // Expected format eg. move-stuck r-1 c-cs2 wait c-cs2 input:
      a.name = line.substr(0,find_nth_space(line, 1)-1);
      if ( find_nth_space(line, 1) != line.find(':') + 1 ) {
        std::stringstream ss(line.substr(find_nth_space(line, 1),
              line.find(':') - find_nth_space(line,1)));
        std::string item;
        while (getline(ss, item, ' ')) {
          a.args.push_back(item);
        }
      }
      curr_plan.push_back(a);
    }
   
  }

}


void
PddlPlannerThread::fd_planner()
{
	logger->log_info(name(), "Starting PDDL Planning with Fast-Downward...");

	std::string command = "fast-downward.py"
		+ std::string(" ") + cfg_domain_path_
		+ std::string(" ") + cfg_problem_path_;

	if (!cfg_fd_options_.empty()) {
		command += std::string(" ") + cfg_fd_options_;
	}

	std::string result = run_planner(command);

	logger->log_info(name(), "Removing temporary planner output.");
	std::remove("output");
	std::remove("output.sas");

	size_t cur_pos = 0;
	if (result.find("Solution found!", cur_pos) == std::string::npos) {
		logger->log_error(name(), "Planning Failed: %s", result.c_str());
		throw Exception("No solution found");
	} else {
		cur_pos = result.find("Solution found!", cur_pos);
		cur_pos = result.find("\n", cur_pos);
		cur_pos = result.find("\n", cur_pos + 1);
		logger->log_info(name(), "Planner found solution.");
	}
	result.erase(0, cur_pos);
	size_t end_pos = result.find("Plan length: ");
	result.erase(end_pos, result.size() - 1);

	std::istringstream iss(result);
	std::string        line;
	// remove surplus line
	getline(iss, line);
	while (getline(iss, line)) {
		Action a;
		a.name = line.substr(0, find_nth_space(line, 1));
		if (find_nth_space(line, 2) != line.rfind(' ') + 1) {
			std::stringstream ss(
			  line.substr(find_nth_space(line, 2), line.rfind(' ') - find_nth_space(line, 2)));
			std::string item;
			while (getline(ss, item, ' ')) {
				a.args.push_back(item);
			}
		}
		action_list_.push_back(a);
	}
}

document::value
PddlPlannerThread::BSONFromActionList(const std::vector<action>& action_list, int plan_id)
{
  using namespace bsoncxx::builder;
  basic::document plan;
  float cost = 0;
  plan.append(basic::kvp("plan",static_cast<int64_t>(plan_id)));
  plan.append(basic::kvp("msg_id", static_cast<int64_t>(plan_if_->msg_id())));
	plan.append(basic::kvp("actions", [&](basic::sub_array actions) {
		for (Action a : action_list) {
		  cost += a.cost;
			basic::document action;
			action.append(basic::kvp("name", a.name));
			action.append(basic::kvp("args", [a](basic::sub_array args) {
				for (std::string arg : a.args) {
					args.append(arg);
				}
			}));
		}
	}));
  plan.append(basic::kvp("cost",cost));
  return plan.extract();
}

/**
 * @brief Searches the position of the nth space in a string.
 *        If there are less than n spaces in the string, the position of the last space is returned
 * 
 * @param s String that should be searched for the position of the nth space
 * @param nth 
 * @return size_t Position of the nth space in string s
 */
size_t
PddlPlannerThread::find_nth_space(const std::string &s, size_t nth)
{
	size_t   pos        = 0;
	unsigned occurrence = 0;

	while (occurrence != nth && (pos = s.find(' ', pos + 1)) != std::string::npos) {
		++occurrence;
	}

	return pos + 1;
}

void
PddlPlannerThread::print_action_list()
{
  unsigned int count = 0;
  for ( Action a : action_list_ ) {
    count++;
    std::string args;
    for ( std::string arg : a.args ) {
      args += arg + " ";
    }
    logger->log_info(name(),"Action %d %s with args %s", count, a.name.c_str(), args.c_str());
  }
}

std::string
PddlPlannerThread::run_planner(std::string command)
{
	logger->log_info(name(), "Running planner with command: %s", command.c_str());
	std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
	if (!pipe)
		throw std::runtime_error("popen() failed!");
	char        buffer[128];
	std::string result;
	while (!feof(pipe.get())) {
		if (fgets(buffer, 128, pipe.get()) != NULL)
			result += buffer;
	}
	logger->log_info(name(), "Planner finished run.");

	return result;
}

bool
PddlPlannerThread::bb_interface_message_received(Interface *      interface,
                                                 fawkes::Message *message) throw()
{
	if (message->is_of_type<PddlPlannerInterface::PlanMessage>()) {
		PddlPlannerInterface::PlanMessage *msg = (PddlPlannerInterface::PlanMessage *)message;
		plan_if_->set_msg_id(msg->id());
		plan_if_->set_success(false);
		plan_if_->set_final(false);
		plan_if_->write();
		wakeup(); //activates loop where the generation is done
	} else {
		logger->log_error(name(), "Received unknown message of type %s, ignoring", message->type());
	}
	return false;
}
