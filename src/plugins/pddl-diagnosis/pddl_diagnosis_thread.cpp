
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
using namespace mongo;

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
  world_model_ = config->get_string("plugins/pddl-diagnosis/world-model");
  if(config->exists("plugins/pddl-diagnosis/goal"))
    goal_ = config->get_string("plugins/pddl-diagnosis/goal");
  if(config->exists("plugins/pddl-diagnosis/world-model-dump"))
    plan_ = config->get_string("plugins/pddl-diagnosis/world-model-dump");


  logger->log_info(name(),"Started Interface on %s",config->get_string("plugins/pddl-diagnosis/interface-name").c_str());
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

void
PddlDiagnosisThread::fill_template_desc(BSONObjBuilder *facets, std::string input) {

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

    try {
      //fill dictionary to expand query template:
	    /*
	    QResCursor cursor = robot_memory->query(fromjson(query_str), collection);
      while(cursor->more())
      {
        BSONObj obj = cursor->next();
        //dictionary for one entry
        ctemplate::TemplateDictionary *entry_dict = dict.AddSectionDictionary(template_name);
        fill_dict_from_document(entry_dict, obj);
      }
	    */
	    mongo::BufBuilder &bb = facets->subarrayStart(template_name);
	    mongo::BSONArrayBuilder *arrb = new mongo::BSONArrayBuilder(bb);
	    BSONObjBuilder query;
	    query.append("$match", fromjson(query_str));
	    arrb->append(query.obj());
	    delete arrb;
    #ifdef HAVE_MONGODB_VERSION_H
    } catch (mongo::MsgAssertionException &e) {
    #else
    } catch (mongo::AssertionException &e) {
    #endif
      logger->log_error("PddlDiagnosis", "Template query failed: %s\n%s",
          e.what(), query_str.c_str());
    }
  }
}

/*
 * Fills problem file template with domain-facts from the world model before executing the plan
 * Adds goal description
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

  BSONObjBuilder facets;
  fill_template_desc(&facets,input_desc);
/*
  BSONObjBuilder aggregate_query;
  aggregate_query.append("$facet", facets.obj());
  BSONObj aggregate_query_obj(aggregate_query.obj());
  std::vector<mongo::BSONObj> aggregate_pipeline{aggregate_query_obj};
  std::string world_model_path = StringConversions::resolve_path(plan_prefix_ + "/" + plan_);
  if (robot_memory->restore_collection(collection_,world_model_path,"diagnosis.worldmodel") == 0)
  {
    logger->log_error(name(),"Failed to restore collection from %s",world_model_path.c_str());
    return -1;
  }
  robot_memory->drop_collection("diagnosis.worldmodel");
  return 0;
  //TODO Fix aggregate
  BSONObj res = robot_memory->aggregate(aggregate_pipeline, "diagnosis.collection");
  BSONObj result;

  //TODO parse return value of aggregate correctly
  if (res.hasField("result")) {
    result = res.getField("result").Obj()["0"].Obj();
  } else {
    logger->log_error(name(),"Failed to restore world model: \n %s",res.toString().c_str());
    return -1;
  }
  for( BSONObj::iterator i = result.begin(); i.more(); ) {
	  BSONElement e = i.next();
    for( BSONObj::iterator j = e.Obj().begin(); j.more(); ) {
	    BSONElement f = j.next();
	    ctemplate::TemplateDictionary *entry_dict = dict.AddSectionDictionary(e.fieldName());
	    fill_dict_from_document(entry_dict, f.Obj());
    }
  }
*/
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
  logger->log_info(name(), "Output:\n%s", output.c_str());
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

  BSONObjBuilder query_b;
  query_b << "_id" << BSONRegEx("^/diagnosis/plan-action");

  BSONObj q = query_b.done();
  logger->log_error(name(),"Query syncedrobmem.worldmodel for %s",q.toString().c_str());

  std::unique_ptr<mongo::DBClientCursor> c = robot_memory->query(q,"syncedrobmem.worldmodel");
  std::vector<PlanAction> history;
  if (c) {
    while(c->more()){
      BSONObj obj = c->next();
      logger->log_info(name(),"%s",obj.toString().c_str());
      PlanAction pa = bson_to_plan_action(obj);
      if (pa.plan == plan_) {
        history.push_back(pa);
      }
    }
  } else {
    logger->log_error(name(),"Failed to query for diagnosis/plan-action facts");
    return -1;
  }

  BSONObjBuilder query_hardware_edge;
  query_hardware_edge << "_id" << BSONRegEx("^/hardware/edge");
  q = query_hardware_edge.done();

  c = robot_memory->query(q,"syncedrobmem.worldmodel");
  std::map<std::string,std::vector<ComponentTransition>> comp_transitions;
  if (c) {
    while(c->more()) {
      BSONObj obj = c->next();
      logger->log_info(name(),"%s",obj.toString().c_str());
      ComponentTransition trans = bson_to_comp_trans(obj);
      if (!trans.executable) {

        comp_transitions[trans.name].push_back(trans);
      }
    }
  } else {
    logger->log_error(name(),"Failed to query for hardware component edges (%s) facts",q.toString().c_str());
    return -1;
  }

  std::vector<PlanAction> history_sorted;
  int history_length = history.size(); 
  for (int i = 1; i <= history_length; ++i) {
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
        break;
      }
    }
  }
  PlanAction last;
  last.id = history_sorted.size()+1;
  last.name = "FINISH";
  history_sorted.push_back(last);

  size_t cur_pos = input_domain.find("<<#",0);
  while(cur_pos != std::string::npos)
  {
    size_t tpl_end_pos =  input_domain.find(">>", cur_pos);
    std::string template_name = input_domain.substr(cur_pos + 3, tpl_end_pos - cur_pos - 3);
    logger->log_info(name(),"Tempname: %s", template_name.c_str());
    if (template_name == "exog-actions") {
      input_domain.erase(cur_pos,tpl_end_pos - cur_pos + 2);
  
      std::map<std::string,std::vector<ComponentTransition>>::iterator it = comp_transitions.begin();
      while( it != comp_transitions.end()) {
          std::string exog_template = "(:action <<#name>>\n :parameters ()\n :precondition (or <<#comps-from>>)\n :effect (and <<#comps-when>> (increase (total-cost) 1))\n)\n\n";
          ComponentTransition trans = it->second[0];
          std::string exog_replaced = find_and_replace(exog_template,"<<#name>>",trans.name);

          std::string comps_from = "";
          for (ComponentTransition trans : it->second) {
            comps_from += "(comp-state " + trans.component + " " + trans.from + ") ";
          }
          exog_replaced = find_and_replace(exog_replaced,"<<#comps-from>>",comps_from);

          std::string comps_when = "";
          for (ComponentTransition trans : it->second) {
            comps_when += "\n (when (comp-state " + trans.component + " " + trans.from + ")\n";
            comps_when += "  (and (not (comp-state " + trans.component + " " + trans.from + ")) (comp-state " + trans.component + " " + trans.to + "))\n )";
          }
          exog_replaced = find_and_replace(exog_replaced,"<<#comps-when>>",comps_when);
          input_domain.insert(cur_pos,exog_replaced);
          cur_pos = cur_pos + exog_replaced.length();

          logger->log_info(name(),"%s",exog_replaced.c_str());
      
        it++;
      }
    }
    if (template_name == "order-actions"){
      input_domain.erase(cur_pos,tpl_end_pos - cur_pos + 2);
      std::string order_template = "(:action order_<<#id>>\n :parameters ()\n :precondition (and (last-<<#lastname>> <<#lastvalues>>))\n :effect (and (not (last-<<#lastname>> <<#lastvalues>>)) (next-<<#name>> <<#values>>))\n)\n\n";
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

        logger->log_info(name(),"%s",order_replaced.c_str());
      }
    }
    cur_pos = input_domain.find("<<#",tpl_end_pos);
  }

  std::string output = input_domain;
  //generate output
  std::ofstream ostream(output_path_domain_);
  logger->log_info(name(),"%s",output_path_domain_.c_str());
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
      goal_ = msg->goal();
    if(std::string(msg->plan()) != "")
      plan_ = std::string(msg->plan());// StringConversions::resolve_path(world_model_dump_prefix_ + "/" + std::string(msg->world_model_dump()));
    if(std::string(msg->collection()) != "" && std::string(msg->collection()).find(".") != std::string::npos)
      collection_ = msg->collection();
    wakeup(); //activates loop where the generation is done
  } else {
    logger->log_error(name(), "Received unknown message of type %s, ignoring",
        message->type());
  }
  return false;
}

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

PddlDiagnosisThread::PlanAction
PddlDiagnosisThread::bson_to_plan_action(BSONObj obj)
{
  PlanAction ret;
  std::string content = obj["_id"];
  content = content.substr(1,content.length() - 2);

  std::vector<std::string> splitted = str_split(content,"?");
  std::string id = splitted[0];
  std::string args = splitted[1];

  std::vector<std::string> id_splitted = str_split(id,"/");
  ret.name = id_splitted[3];

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


PddlDiagnosisThread::ComponentTransition
PddlDiagnosisThread::bson_to_comp_trans(BSONObj obj)
{
  ComponentTransition ret;
  std::string content = obj["_id"];
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
 * Fills a dictionary with key value pairs from a document. Recursive to handle subdocuments
 * @param dict Dictionary to fill
 * @param obj Document
 * @param prefix Prefix of previous super-documents keys
 */
void PddlDiagnosisThread::fill_dict_from_document(ctemplate::TemplateDictionary *dict, BSONObj obj, std::string prefix)
{
  for(BSONObjIterator it = obj.begin(); it.more();)
  {
    BSONElement elem = it.next();
    switch (elem.type()) {
      case mongo::NumberDouble:
        dict->SetValue(prefix + elem.fieldName(), std::to_string(elem.Double()));
        break;
      case mongo::String:
        dict->SetValue(prefix + elem.fieldName(), elem.String());
        break;
      case mongo::Bool:
        dict->SetValue(prefix + elem.fieldName(), std::to_string(elem.Bool()));
        break;
      case mongo::NumberInt:
        dict->SetIntValue(prefix + elem.fieldName(), elem.Int());
        break;
      case mongo::NumberLong:
        dict->SetValue(prefix + elem.fieldName(), std::to_string(elem.Long()));
        break;
      case mongo::Object:
        fill_dict_from_document(dict, elem.Obj(), prefix + elem.fieldName() + "_");
        break;
      case 7: //ObjectId
        dict->SetValue(prefix + elem.fieldName(), elem.OID().toString());
        break;
      case mongo::Array:
      {
        // access array elements as if they were a subdocument with key-value pairs
        // using the indices as keys
        BSONObjBuilder b;
        for(size_t i = 0; i < elem.Array().size(); i++)
        {
          b.append(elem.Array()[i]);
        }
        fill_dict_from_document(dict, b.obj(), prefix + elem.fieldName() + "_");
        // additionally feed the whole array as space-separated list
        std::string array_string;
        for (size_t i = 0; i < elem.Array().size(); i++)
        {
          // TODO: This only works for string arrays, adapt to other types.
          array_string += " " + elem.Array()[i].String();
        }
        dict->SetValue(prefix + elem.fieldName(), array_string);
        break;
      }
      default:
        dict->SetValue(prefix + elem.fieldName(), "INVALID_VALUE_TYPE");
    }
  }
}

