#*****************************************************************************
#                      Makefile Build System for Fawkes
#                            -------------------
#   Created on Fri 16 Feb 2018 17:33:33 CET
#   Copyright (C) 2018 by Till Hofmann <hofmann@kbsg.rwth-aachen.de>
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
include $(BUILDSYSDIR)/clips.mk
include $(BASEDIR)/src/plugins/robot-memory/robot_memory.mk

LIBS_clips_active_diagnosis = m fawkescore fawkesutils fawkesaspects fawkesbaseapp \
                          fawkesblackboard fawkesinterface fawkesclipsaspect \
                          fawkesrobotmemory
OBJS_clips_active_diagnosis = clips_active_diagnosis_plugin.o clips_active_diagnosis_thread.o \
												 clips_diagnosis_env.o \

OBJS_all    = $(OBJS_clips_active_diagnosis)
PLUGINS_all = $(PLUGINDIR)/clips-active-diagnosis.$(SOEXT)

ifeq ($(HAVE_CLIPS),1)
  CFLAGS += $(CFLAGS_CLIPS)
  LDFLAGS += $(LDFLAGS_CLIPS)

  PLUGINS_build = $(PLUGINS_all)
else
  WARN_TARGETS += warning_clips
endif
ifneq ($(HAVE_ROBOT_MEMORY),1)
    WARN_TARGETS = warning_robot_memory
endif

ifeq ($(OBJSSUBMAKE),1)
all: $(WARN_TARGETS)

.PHONY: warning_clips warning_robot_memory
warning_clips:
	$(SILENT)echo -e "$(INDENT_PRINT)--> $(TRED)Omitting clips-pddl-parser plugin$(TNORMAL) ($(CLIPS_ERROR))"
warning_robot_memory:
	$(SILENT)echo -e "$(INDENT_PRINT)--> $(TRED)Omitting clips-robot-memory plugin$(TNORMAL) (robot memory not available)"
endif

include $(BUILDSYSDIR)/base.mk
