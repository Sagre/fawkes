
/***************************************************************************
 *  pddl_diagnosis_thread.h - pddl_diagnosis
 *
 *  Plugin created: Mon Apr 1 13:34:05 2019

 *  Copyright  2019  Daniel Habering
 
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

#ifndef _PLUGINS_PDDL_DIAGNOSISTHREAD_H_
#define _PLUGINS_PDDL_DIAGNOSISTHREAD_H_

#include <core/threading/thread.h>
#include <aspect/logging.h>
#include <aspect/blackboard.h>
#include <aspect/configurable.h>
#include <plugins/robot-memory/aspect/robot_memory_aspect.h>
#include <blackboard/interface_listener.h>


#include <bsoncxx/document/view.hpp>
#include <string>

#include <ctemplate/template.h>
#include "interfaces/PddlDiagInterface.h"

namespace fawkes {
}
class PddlDiagnosisThread
: public fawkes::Thread,
  public fawkes::LoggingAspect,
  public fawkes::ConfigurableAspect,
  public fawkes::BlackBoardAspect,
  public fawkes::RobotMemoryAspect,
  public fawkes::BlackBoardInterfaceListener
{

 public:
  PddlDiagnosisThread();

  virtual void init();
  virtual void finalize();
  virtual void loop();

  /** Stub to see name in backtrace for easier debugging. @see Thread::run() */
  protected: virtual void run() { Thread::run(); }

 private:

  typedef struct {
    std::string              name;
    int                      id;
    std::string              plan;
    std::vector<std::string> param_names;
    std::vector<std::string> param_values;
  } PlanAction;

  typedef struct {
    std::string              name;
    std::string              from;
    std::string              to;
    std::string              component;
    bool                     executable;
    float                    prob;
  } ComponentTransition;

  fawkes::PddlDiagInterface *gen_if_;

  std::string collection_;
  std::string world_model_dump_prefix_;
  std::string plan_;
  std::string input_path_desc_;
  std::string input_path_domain_;
  std::string output_path_desc_;
  std::string output_path_domain_;
  std::string goal_;

  void fill_dict_from_document(ctemplate::TemplateDictionary *dict, const bsoncxx::document::view &obj, std::string prefix = "");
  void generate();

  int create_problem_file();
  int create_domain_file();
  
  std::map<std::string,std::string> fill_template_desc(std::string &input);
  std::string find_and_replace(const std::string &input, const std::string &find, const std::string &replace);

  virtual bool bb_interface_message_received(fawkes::Interface *interface,
                                             fawkes::Message *message) throw();

  PlanAction bson_to_plan_action(const bsoncxx::document::view &obj);
  ComponentTransition bson_to_comp_trans(const bsoncxx::document::view &obj);

  bool is_domain_fact(std::string key_str);
  bool is_domain_object(std::string key_str);

  std::string key_get_predicate_name(std::string key);
  std::string key_get_param_values(std::string key);
  std::string key_get_object_type(std::string key);

};


#endif
