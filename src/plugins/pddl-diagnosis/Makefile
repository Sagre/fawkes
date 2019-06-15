#*****************************************************************************
#         Makefile Build System for Fawkes: pddl_diagnosis Plugin
#                            -------------------
#   Created on Mon Apr 1 13:34:05 2019
 
#   Copyright (C) 2019 by Daniel Habering
#
#*****************************************************************************
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#*****************************************************************************

BASEDIR = ../../..
include $(BASEDIR)/etc/buildsys/config.mk
include $(BASEDIR)/src/plugins/robot-memory/robot_memory.mk
include $(BASEDIR)/src/plugins/pddl-diagnosis/ctemplate.mk

PRESUBDIRS = interfaces

LIBS_pddl_diagnosis = m fawkescore fawkesutils fawkesaspects fawkesbaseapp \
                         fawkesblackboard fawkesinterface fawkesrobotmemory \
                         PddlDiagInterface

OBJS_pddl_diagnosis = pddl_diagnosis_plugin.o pddl_diagnosis_thread.o

OBJS_all   = $(OBJS_pddl_diagnosis)
PLUGINS_all = $(PLUGINDIR)/pddl-diagnosis.$(SOEXT)	  

ifeq ($(HAVE_CPP11)$(HAVE_ROBOT_MEMORY)$(HAVE_CTEMPLATE),111)
  CFLAGS  += $(CFLAGS_CPP11) $(CFLAGS_CLIPS) $(CFLAGS_ROBOT_MEMORY)
  LDFLAGS += $(LDFLAGS_CTEMPLATE) $(LDFLAGS_CLIPS) $(LDFLAGS_ROBOT_MEMORY)

  PLUGINS_build = $(PLUGINS_all)
else
  ifneq ($(HAVE_CPP11),1)
    WARN_TARGETS += warning_cpp11
  endif
  ifneq ($(HAVE_ROBOT_MEMORY),1)
    WARN_TARGETS += warning_robot_memory
  endif
  ifneq ($(HAVE_CTEMPLATE),1)
    WARN_TARGETS += warning_ctemplate
  endif
endif

ifeq ($(OBJSSUBMAKE),1)
all: $(WARN_TARGETS)

.PHONY: warning_robot_memory warning_ctemplate warning_cpp11
warning_robot_memory:
	$(SILENT)echo -e "$(INDENT_PRINT)--> $(TRED)Omitting pddl-diagnosis plugin$(TNORMAL) (robot memory not available)"
warning_cpp11:
	$(SILENT)echo -e "$(INDENT_PRINT)--> $(TRED)Cannot build pddl-diagnosis plugin$(TNORMAL) (C++11 not supported)"
warning_ctemplate:
	$(SILENT)echo -e "$(INDENT_PRINT)--> $(TRED)Cannot build pddl-diagnosis plugin$(TNORMAL) (ctemplate-devel not installed)"
endif

include $(BUILDSYSDIR)/base.mk
