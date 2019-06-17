
/***************************************************************************
 *  pddl_diagnosis_thread.cpp - pddl_diagnosis
 *
 *  Plugin created: Mon Apr 1 13:34:05 2019

 *  Copyright  2019  Daniel Habering
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

#include "pddl_diagnosis_thread.h"
#include <fstream>
#include <utils/misc/string_conversions.h>
#include <utils/misc/string_split.h>

using namespace fawkes;

/** @class PddlDiagnosisThread 'pddl_robot_memory_thread.h' 
 * Generate PDDL files from the robot memory
 *
 * This plugin uses the template engine ctemplate to generate a pddl
 * from a template file and the robot memory.
 *
 * The template file can use the following templates to generate some output
 * for each document returned by a query:
 *
 * Example: \c "<<#ONTABLE|{relation:'on-table'}>> (on-table <<object>>) <</ONTABLE>>"
 * Yields: (on-table a) (on-table b)
 * When these documents are in the database:
 *  {relation:'on-table', object:'a'}, {relation:'on-table', object:'b'}
 *
 * The selection template \c "<<#UNIQUENAME|query>> something <</UNIQUENAME>>"
 *  queries the robot memory and inserts 'something' for each returned document.
 *
 * Variable templates \c "<<key>>" inside the selection are substituted by the values
 *  of that key in the document returned by the query. You can also access subdocuments
 *  and arrays as follows:
 *  (e.g. \c "<<position_translation_0>>" for a document {position:{translation:[0,1,2], orientation:[0,1,2]}})
 *
 * @author Frederik Zwilling
 */

PddlDiagnosisThread::PddlDiagnosisThread()
 : Thread("PddlDiagnosisThread", Thread::OPMODE_WAITFORWAKEUP),
   BlackBoardInterfaceListener("PddlDiagnosisThread")
{
}

void
PddlDiagnosisThread::init()
{
  //read config values
  world_model_dump_prefix_ = StringConversions::resolve_path("@BASEDIR@/" + config->get_string("plugins/pddl-diagnosis/world-model-dump-prefix"));

  input_path_domain_ = StringConversions::resolve_path("@BASEDIR@/src/clips-specs/" +
    config->get_string("plugins/pddl-diagnosis/input-diagnosis-domain"));
  input_path_desc_ = StringConversions::resolve_path("@BASEDIR@/src/clips-specs/" +
        config->get_string("plugins/pddl-diagnosis/input-diagnosis-description"));
  output_path_domain_ = StringConversions::resolve_path("@BASEDIR@/src/clips-specs/" +
    config->get_string("plugins/pddl-diagnosis/output-diagnosis-domain"));
  output_path_desc_ = StringConversions::resolve_path("@BASEDIR@/src/clips-specs/" +
    config->get_string("plugins/pddl-diagnosis/output-diagnosis-description"));

  if(config->exists("plugin/pddl-diagnosis/plan_id"))
  plan_ = config->get_string("plugins/pddl-diagnosis/plan_id");

  if(config->exists("plugins/pddl-diagnosis/goal"))
    goal_ = config->get_string("plugins/pddl-diagnosis/goal");

  //setup interface
  gen_if_ = blackboard->open_for_writing<PddlDiagInterface>(config->get_string("plugins/pddl-diagnosis/interface-name").c_str());
  gen_if_->set_msg_id(0);
  gen_if_->set_final(false);
  gen_if_->write();

  //setup interface listener
  bbil_add_message_interface(gen_if_);
  blackboard->register_listener(this, BlackBoard::BBIL_FLAG_MESSAGES);

  if(config->get_bool("plugins/pddl-diagnosis/generate-on-init"))
  {
    wakeup(); //activates loop where the generation is done
  }
}

/**
 * @brief Seaches the input string for queries of the form <<#templateName|mongodb-query>>
 * eg <<#DOMAINOBJECTS|domain/object>>
 * 
 * @param input Input string
 * @return std::map<std::string, std::string> Return a map if template names to mongodb queries
 */
std::map<std::string, std::string>
PddlDiagnosisThread::fill_template_desc(std::string &input) { 
  //find queries in template
  size_t cur_pos = 0;
  std::map<std::string, std::string> templates;
  while(input.find("<<#", cur_pos) != std::string::npos)
  {
    cur_pos = input.find("<<#", cur_pos) + 3;
    size_t tpl_end_pos =  input.find(">>", cur_pos);
    //is a query in the template? (indicated by '|')
    size_t q_del_pos = input.find("|", cur_pos);
    if(q_del_pos == std::string::npos || q_del_pos > tpl_end_pos)
      continue; //no query to execute
    //parse: template name | query
    std::string template_name = input.substr(cur_pos, q_del_pos - cur_pos);
    std::string query_str =  input.substr(q_del_pos + 1, tpl_end_pos - (q_del_pos + 1));
    if (templates.find(template_name) != templates.end()) {
      if (templates[template_name] != query_str) {
        logger->log_error(name(),
            "Template with same name '%s' but different query '%s' vs '%s'!",
            template_name.c_str(), query_str.c_str(),
            templates[template_name].c_str());
      } else {
        input.erase(q_del_pos, tpl_end_pos - q_del_pos);
        continue;
      }
    }
    templates[template_name] = query_str;
    //remove query stuff from input (its not part of the ctemplate features)
    input.erase(q_del_pos, tpl_end_pos - q_del_pos);
  }
  return templates;
}

/*
 * Fills problem file template with domain-facts from the world model before executing the plan
 * Adds goal description
 * Return 0 if successful, -1 otherwise
 */
int
PddlDiagnosisThread::create_problem_file()
{
  //read input template of problem description
  std::string input_desc;
  std::ifstream istream_desc(input_path_desc_);

  if(istream_desc.is_open())
  {
    input_desc = std::string((std::istreambuf_iterator<char>(istream_desc)), std::istreambuf_iterator<char>());
    istream_desc.close();
  }
  else
  {
    logger->log_error(name(), "Could not open %s", input_path_desc_.c_str());
    return -1;
  }

  //set template delimeters to << >>
  input_desc = "{{=<< >>=}}" +  input_desc;
  ctemplate::TemplateDictionary dict("pddl-rm");
  //Dictionary how to fill the templates
  
  std::map<std::string, std::string> queries = fill_template_desc(input_desc);

  // Restore world model that was true at the beginning of plan execution
  std::string world_model_path = StringConversions::resolve_path(world_model_dump_prefix_ + "/" + plan_);
  if (robot_memory->restore_collection(collection_,world_model_path,"diagnosis.worldmodel") == 0)
  {
    logger->log_error(name(),"Failed to restore collection from %s",world_model_path.c_str());
    return -1;
  }

  logger->log_info(name(),"Starting diagnosis pddl file generation");

  // For each query found in the template file, query the restored worldmodel and fill
  // the ctemplate dictionary
  using namespace bsoncxx::builder;

  std::map<std::string, std::string>::iterator it = queries.begin();
  for (;it != queries.end(); it++) {
    basic::document query_b;
    query_b.append(basic::kvp("_id",bsoncxx::types::b_regex{it->second}));

    auto c = robot_memory->query(query_b.view(),"diagnosis.worldmodel");
    for (auto doc : c) {
      ctemplate::TemplateDictionary *entry_dict = dict.AddSectionDictionary(it->first);
	    fill_dict_from_document(entry_dict, doc);
    }
  }
  logger->log_info(name(),"Finished template filling");
  robot_memory->drop_collection("diagnosis.worldmodel");

  //Add goal to dictionary
  dict.SetValue("GOAL", goal_);

  //prepare template expanding
  ctemplate::StringToTemplateCache("tpl-cache", input_desc, ctemplate::DO_NOT_STRIP);
  if(!ctemplate::TemplateNamelist::IsAllSyntaxOkay(ctemplate::DO_NOT_STRIP))
  {
    logger->log_error(name(), "Syntax error in template %s:", input_path_desc_.c_str());
    std::vector<std::string> error_list = ctemplate::TemplateNamelist::GetBadSyntaxList(false, ctemplate::DO_NOT_STRIP);
    for(std::string error : error_list)
    {
      logger->log_error(name(), "%s", error.c_str());
    }
    return -1;
  }
  //Let ctemplate expand the input
  std::string output;
  ctemplate::ExpandTemplate("tpl-cache", ctemplate::DO_NOT_STRIP, &dict, &output);

  //generate output
  std::ofstream ostream(output_path_desc_);
  if(ostream.is_open())
  {
    ostream << output.c_str();
    ostream.close();
  }
  else
  {
    logger->log_error(name(), "Could not open %s", output_path_desc_.c_str());
    return -1;
  }

  return 0;
}

/*
 * Takes given Plan id, grabs already executed plan-actions from robot-memory, creates order actions
 * Return 0 if successfull, -1 otherwise
 */
int
PddlDiagnosisThread::create_domain_file()
{
  std::string input_domain;
  std::ifstream istream_domain(input_path_domain_);
  if(istream_domain.is_open())
  {
    input_domain = std::string((std::istreambuf_iterator<char>(istream_domain)), std::istreambuf_iterator<char>());
    istream_domain.close();
  }
  else
  {
    logger->log_error(name(), "Could not open %s", input_path_domain_.c_str());
  }
	using namespace bsoncxx::builder;
  basic::document query_b;
  query_b.append(basic::kvp("_id",bsoncxx::types::b_regex{"^/diagnosis/plan-action"}));

    auto c = robot_memory->query(query_b.view(),"robmem.diagnosis");

  std::vector<PlanAction> history;
  for (auto doc: c) {
    PlanAction pa = bson_to_plan_action(doc);
      if (pa.plan == plan_) {
        history.push_back(pa);
      }
  }

  // Query the diagnosis collection for transitions of hardware components
  // These are used to create the exogenous actions representing state changes
  // Collect all existing states and components on the way to add them as objects and types

  std::map<std::string,std::vector<ComponentTransition>> comp_transitions;
  std::vector<std::string> components;
  std::vector<std::string> states;

  basic::document query_hardware_edge;
  query_hardware_edge.append(basic::kvp("_id",bsoncxx::types::b_regex{"^/hardware/edge"}));

  c = robot_memory->query(query_hardware_edge.view(),"robmem.diagnosis");

  for (auto doc : c) {
      ComponentTransition trans = bson_to_comp_trans(doc);
      if (!trans.executable) {
        //Transition is exogenous and has to be added as pddl action
        comp_transitions[trans.name].push_back(trans);
      }
      if (std::find(components.begin(), components.end(), trans.component) == components.end()) {
        //New component found
        components.push_back(trans.component);
      }
      if (std::find(states.begin(), states.end(), trans.from) == states.end()) {
        //New state found
        states.push_back(trans.from);
      }
      if (std::find(states.begin(), states.end(), trans.to) == states.end()) {
        //New state found
        states.push_back(trans.to);
      }
  }

  // Sort history according to their ids in a vector in order to correctly create the order actions
  std::vector<PlanAction> history_sorted;
  int history_length = history.size(); 
  for (int i = 1; i <= history_length; ++i) {
    bool found = false;
    for (std::vector<PlanAction>::iterator it = history.begin(); it != history.end(); ++it) {
      if (it->id == i) {
        PlanAction new_pa;

        new_pa.id = it->id;
        new_pa.name = it->name;
        new_pa.param_names = it->param_names;
        new_pa.param_values = it->param_values;
        new_pa.plan = it->plan;

        history.erase(it);
        history_sorted.push_back(new_pa);

        found = true;
        break;
      }
    }
    if (!found) {
      logger->log_error(name(),"Missing plan action with id %d in history stored in the diagnosis collection \n \t \
                                  This may result in an incomplete diagnosis generation.",i);
    }
  }
  //Dummy action to enforce the last order action to result in "next-FINISH"
  PlanAction last;
  last.id = history_sorted.size()+1;
  last.name = "FINISH";
  history_sorted.push_back(last);


  // Fill domain file template
  // Possible replacements: <<#constants>>, <<#exog-actions>>, <<#order-actions>>

  size_t cur_pos = input_domain.find("<<#",0);
  while(cur_pos != std::string::npos)
  {
    size_t tpl_end_pos =  input_domain.find(">>", cur_pos);
    std::string template_name = input_domain.substr(cur_pos + 3, tpl_end_pos - cur_pos - 3);

    if (template_name == "constants") {
      // Add all dynamic constants to the pddl domain file
      // Namely components and component states
      input_domain.erase(cur_pos,tpl_end_pos - cur_pos + 2);

      for (std::string state : states) {
        std::string pddl_state =  state + " - state\n";
        input_domain.insert(cur_pos,pddl_state);
        cur_pos = cur_pos + pddl_state.length();
      }

      for (std::string comp : components) {
        std::string pddl_comp = comp + " - object\n";
        input_domain.insert(cur_pos,pddl_comp);
        cur_pos = cur_pos + pddl_comp.length();
      }
    } else if (template_name == "exog-actions") {
      // Add all exogenous actions that represent component state changes
      // Since the same action can trigger different transitions, we have to use
      // conditional effects.
      input_domain.erase(cur_pos,tpl_end_pos - cur_pos + 2);
  
      std::map<std::string,std::vector<ComponentTransition>>::iterator it = comp_transitions.begin();
      while( it != comp_transitions.end()) {
          std::string exog_template = "(:action <<#name>>\n \
                                        :parameters ()\n \
                                        :precondition (and (exog-possible) (or <<#comps-from>>))\n \
                                        :effect (and <<#comps-when>>\n \
                                              \t\t(increase (total-cost) 1)\n \
                                                )\n \
                                        )\n";
          ComponentTransition trans = it->second[0];
          // Replace the name with the name of the transition actions
          std::string exog_replaced = find_and_replace(exog_template,"<<#name>>",trans.name);

          // Disjunctive precondition, enforcing the components being in one of the orgin states
          // from which the action can trigger a transition
          std::string comps_from = "";
          for (ComponentTransition trans : it->second) {
            comps_from += "(comp-state " + trans.component + " " + trans.from + ") ";
          }
          exog_replaced = find_and_replace(exog_replaced,"<<#comps-from>>",comps_from);

          // Conditional effect for each of the transitions
          std::string comps_when = "";
          for (ComponentTransition trans : it->second) {
            comps_when += "\n (when (comp-state " + trans.component + " " + trans.from + ")\n";
            comps_when += "  (and (not (comp-state " + trans.component + " " + trans.from + ")) (comp-state " + trans.component + " " + trans.to + ") )\n ) ";
          }

          exog_replaced = find_and_replace(exog_replaced,"<<#comps-when>>",comps_when);
          input_domain.insert(cur_pos,exog_replaced);
          cur_pos = cur_pos + exog_replaced.length();
          it++;
      }
    } else if (template_name == "order-actions"){
      // Insert all order actions that enforce the executed history
      // For each action in the history there is a order action enforcing 
      // one action of that type in the generated diagnosis hypothesis
      input_domain.erase(cur_pos,tpl_end_pos - cur_pos + 2);
      std::string order_template = "(:action order_<<#id>>\n \
                                      :parameters ()\n \
                                      :precondition (and (last-<<#lastname>> <<#lastvalues>>))\n \
                                      :effect (and (exog-possible) (not (last-<<#lastname>> <<#lastvalues>>)) (next-<<#name>> <<#values>>))\n \
                                      )\n\n";
      std::string last_name = "BEGIN";
      std::string last_values = "";
      for (PlanAction pa : history_sorted)
      {
        std::string values_str = "";
        for (std::string pv : pa.param_values)
        {
          values_str += pv + " ";
        }
        values_str = values_str.substr(0,values_str.length()-1);
        std::string order_replaced = find_and_replace(order_template,"<<#lastname>>",last_name);
        order_replaced = find_and_replace(order_replaced,"<<#id>>",std::to_string(pa.id));
        order_replaced = find_and_replace(order_replaced,"<<#lastvalues>>",last_values);
        order_replaced = find_and_replace(order_replaced,"<<#name>>",pa.name);
        order_replaced = find_and_replace(order_replaced,"<<#values>>", values_str);
        
        last_values = values_str;
        last_name = pa.name;

        input_domain.insert(cur_pos,order_replaced);
        cur_pos = cur_pos + order_replaced.length();

      }
    } else {
      logger->log_warn(name(),"Unknown template name %s found in diagnosis domain template file",template_name.c_str());
    }
    cur_pos = input_domain.find("<<#",tpl_end_pos);
  }

  std::string output = input_domain;
  //generate output
  std::ofstream ostream(output_path_domain_);
  if(ostream.is_open())
  {
    ostream << output.c_str();
    ostream.close();
  }
  else
  {
    logger->log_error(name(), "Could not open %s", output_path_domain_.c_str());
    return -1;
  }

  return 0;
}

/**
 * Thread is only waked up if there is a new interface message to generate a pddl
 */
void
PddlDiagnosisThread::loop()
{

  if (create_problem_file() == -1) {
    gen_if_->set_final(true);
    gen_if_->set_success(false);
    gen_if_->write();
    logger->log_error(name(),"Failed to generate problem file");
    return;
  }
  if (create_domain_file() == -1) {
    gen_if_->set_final(true);
    gen_if_->set_success(false);
    gen_if_->write();
    return;
  }
  logger->log_info(name(), "Generation of PDDL problem description finished");
  gen_if_->set_final(true);
  gen_if_->set_success(true);
  gen_if_->write();
}

void
PddlDiagnosisThread::finalize()
{
  blackboard->close(gen_if_);
}

bool
PddlDiagnosisThread::bb_interface_message_received(Interface *interface, fawkes::Message *message) throw()
{
  if (message->is_of_type<PddlDiagInterface::GenerateMessage>()) {
    PddlDiagInterface::GenerateMessage* msg = (PddlDiagInterface::GenerateMessage*) message;
    gen_if_->set_msg_id(msg->id());
    gen_if_->set_final(false);
    gen_if_->write();
    if(std::string(msg->goal()) != "")
      // Fault for which an explanation is searched
      goal_ = msg->goal();
    if(std::string(msg->plan()) != "")
      // ID of the plan that lead to the fault
      plan_ = std::string(msg->plan());
    if(std::string(msg->collection()) != "" && std::string(msg->collection()).find(".") != std::string::npos)
      // Name of the world model dump collection: db.collection
      collection_ = msg->collection();
    wakeup(); //activates loop where the generation is done
  } else {
    logger->log_error(name(), "Received unknown message of type %s, ignoring",
        message->type());
  }
  return false;
}

/**
 * @brief Replaces each occurance of the find string with the replace string in the input string
 * 
 * @param input String in which all instances of the find string should be relaced
 * @param find String to be replaced
 * @param replace String to replace
 * @return std::string Input string in which all instances of the find string is replaced
 */
std::string
PddlDiagnosisThread::find_and_replace(const std::string &input, const std::string &find, const std::string &replace)
{
  std::string ret = input;
  size_t pos = 0;
  while ((pos = ret.find(find, pos)) != std::string::npos) {
      ret.replace(pos, find.length(), replace);
      pos += replace.length();
    }
  return ret;

}

/**
 * @brief Creates a PlanAction object from a bson obj
 *        The bson obj should have an _id field that contains a wm-fact id (Refer to the clips-executive definition of wm-facts)
 *        eg _id : "/diagnosis/plan-action/move?plan=TEST-PLAN&id=1&r=R-1&to=C-CS1&to-side=INPUT"
 * 
 * @param obj 
 * @return PddlDiagnosisThread::PlanAction 
 */
PddlDiagnosisThread::PlanAction
PddlDiagnosisThread::bson_to_plan_action(const bsoncxx::document::view &obj)
{
  PlanAction ret;
  std::string content = obj["_id"].get_utf8().value.to_string();
  content = content.substr(1,content.length() - 1);
  std::vector<std::string> splitted = str_split(content,"?");
  std::string id = splitted[0];
  std::string args = splitted[1];

  std::vector<std::string> id_splitted = str_split(id,"/");
  ret.name = id_splitted[id_splitted.size() - 1];
  std::vector<std::string> args_splitted = str_split(args,"&");

  for (size_t i = 0; i < args_splitted.size(); ++i) {
    std::string name = str_split(args_splitted[i],"=")[0];
    std::string value = str_split(args_splitted[i],"=")[1];
    if (name == "plan"){
      ret.plan = value;
    } else if(name == "id") {
      ret.id = stoi(value);
    } else {
      ret.param_names.push_back(name);
      ret.param_values.push_back(value);
    }
  }
  return ret;
}

/**
 * @brief Creates a ComponentTransition object from a bson obj.
 *        The bson obj is assumed to have an _id field that contains a wm-fact id. Refer to the clips-executive definition of wm-facts
 *        eg. _id : "/hardware/edge?comp=gripper&from=CALIBRATED&to=FINGERS-BROKEN&trans=break_gripper_fingers&prob=0.5&exec=nil"
 * 
 * @param obj 
 * @return PddlDiagnosisThread::ComponentTransition 
 */
PddlDiagnosisThread::ComponentTransition
PddlDiagnosisThread::bson_to_comp_trans(const bsoncxx::document::view &obj)
{
  ComponentTransition ret;
  std::string content = obj["_id"].get_utf8().value.to_string();
  content = content.substr(1,content.length() - 2);

  std::vector<std::string> splitted = str_split(content,"?");
  std::string id = splitted[0];
  std::string args = splitted[1];

  std::vector<std::string> args_splitted = str_split(args,"&");
  for (size_t i = 0; i < args_splitted.size(); ++i) {
    std::string name = str_split(args_splitted[i],"=")[0];
    std::string value = str_split(args_splitted[i],"=")[1];
    if (name == "from"){
      ret.from = value;
    } else if (name == "comp") {
      ret.component = value;
    } else if (name == "to") {
      ret.to = value;
    } else if (name == "trans") {
      ret.name = value;      
    } else if (name == "exec") {
      if (value == "true") {
        ret.executable = true; 
      } else {
        ret.executable = false;
      }
    } else if (name == "prob") {
      ret.prob = std::stof(value);
    }
  }
  return ret;
}

/**
 * @brief Checks if a wm-fact id refers to a domain fact. 
 * 
 * @param key_str 
 * @return true 
 * @return false 
 */
bool PddlDiagnosisThread::is_domain_fact(std::string key_str)
{
  std::string domain_fact_prefix = "/domain/fact";
  auto res = std::mismatch(domain_fact_prefix.begin(), domain_fact_prefix.end(), key_str.begin());

  if (res.first == domain_fact_prefix.end())
  {
    return true;
  }
  return false;
}

/**
 * @brief Checks if a wm-fact id refers to a domain-object
 * 
 * @param key_str 
 * @return true 
 * @return false 
 */
bool PddlDiagnosisThread::is_domain_object(std::string key_str)
{
  std::string domain_fact_prefix = "/domain/object";
  auto res = std::mismatch(domain_fact_prefix.begin(), domain_fact_prefix.end(), key_str.begin());

  if (res.first == domain_fact_prefix.end())
  {
    return true;
  }
  return false;
}

std::string PddlDiagnosisThread::key_get_predicate_name(std::string key)
{
  std::vector<std::string> splitted = str_split(key,"?");
  std::string id = splitted[0];
  std::string args = splitted[1];

  std::vector<std::string> id_splitted = str_split(id,"/");
  return id_splitted[3];
}

std::string PddlDiagnosisThread::key_get_param_values(std::string key)
{
  std::vector<std::string> splitted = str_split(key,"?");
  std::string id = splitted[0];
  std::string args = splitted[1];

  std::vector<std::string> args_splitted = str_split(args,"&");
  std::string output;
  for (std::string arg : args_splitted)
  {
    std::vector<std::string> equal_string = str_split(arg,"=");
    output += " " + equal_string[1];
  }
  return output;
}

std::string PddlDiagnosisThread::key_get_object_type(std::string key)
{
  std::vector<std::string> id_splitted = str_split(key,"/");
  return id_splitted[3];
}


/**
 * Fills a dictionary with key value pairs from a document. Recursive to handle subdocuments
 * @param dict Dictionary to fill
 * @param obj Document
 * @param prefix Prefix of previous super-documents keys
 */
void PddlDiagnosisThread::fill_dict_from_document(ctemplate::TemplateDictionary *dict, const bsoncxx::document::view &obj, std::string prefix)
{
  	for (auto elem : obj) {
		switch (elem.type()) {
		case bsoncxx::type::k_double:
			dict->SetValue(prefix + std::string(elem.key()), std::to_string(elem.get_double()));
			break;
		case bsoncxx::type::k_utf8:
        if (is_domain_fact(elem.get_utf8().value.to_string())) {
           dict->SetValue("name",key_get_predicate_name(elem.get_utf8().value.to_string()));
           dict->SetValue("param_values",key_get_param_values(elem.get_utf8().value.to_string()));
        }
        if (is_domain_object(elem.get_utf8().value.to_string())) {
          dict->SetValue("object_type",key_get_object_type(elem.get_utf8().value.to_string()));
        }
			dict->SetValue(prefix + std::string(elem.key()), elem.get_utf8().value.to_string());
			break;
		case bsoncxx::type::k_bool:
			dict->SetValue(prefix + std::string(elem.key()), std::to_string(elem.get_bool()));
			break;
		case bsoncxx::type::k_int32:
			dict->SetIntValue(prefix + std::string(elem.key()), elem.get_int32());
			break;
		case bsoncxx::type::k_int64:
			dict->SetIntValue(prefix + std::string(elem.key()), elem.get_int64());
			break;
		case bsoncxx::type::k_document:
			fill_dict_from_document(dict,
			                        elem.get_document().view(),
			                        prefix + std::string(elem.key()) + "_");
			break;
		case bsoncxx::type::k_oid: //ObjectId
			dict->SetValue(prefix + std::string(elem.key()), elem.get_oid().value.to_string());
			break;
		case bsoncxx::type::k_array: {
			// access array elements as if they were a subdocument with key-value pairs
			// using the indices as keys
			bsoncxx::builder::basic::document b;
			bsoncxx::array::view     array{elem.get_array().value};
			uint            i     = 0;
			for (auto e : array) {
        if (e.type() == bsoncxx::type::k_document) {
	        b.append(bsoncxx::builder::basic::kvp(std::to_string(i++), e.get_document()));
        } else {
          logger->log_debug(name(),"Element is not a document but %d" , (int)e.type());
        }
       
			}
			fill_dict_from_document(dict, b.view(), prefix + std::string(elem.key()) + "_");
			// additionally feed the whole array as space-separated list
			std::string array_string;
			for (auto e : array) {
				// TODO: This only works for string arrays, adapt to other types.
				array_string += " " + e.get_utf8().value.to_string();
			}
			dict->SetValue(prefix + std::string(elem.key()), array_string);
			break;
		}
		default: dict->SetValue(prefix + std::string(elem.key()), "INVALID_VALUE_TYPE");
		}
	}
  
}

