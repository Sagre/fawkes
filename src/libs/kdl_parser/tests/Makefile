#*****************************************************************************
#               Makefile Build System for Fawkes: KDL Parser Unit Test
#                            -------------------
#   Created on Mon Mar 24 15:46:30 2014
#   Copyright (C) 2014
#
#*****************************************************************************
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#*****************************************************************************

BASEDIR = ../../../..
include $(BASEDIR)/etc/buildsys/config.mk
include $(BASEDIR)/etc/buildsys/gtest.mk
include $(BASEDIR)/src/libs/kdl_parser/kdl_parser.mk

LIBS_gtest_kdlparser += stdc++ fawkeskdl_parser

OBJS_gtest_kdlparser += test_kdlparser.o
OBJS_all = $(OBJS_gtest_kdlparser)
BINS_all = $(BINDIR)/gtest_kdlparser
ifeq ($(HAVE_KDLPARSER),1)
  CFLAGS  += $(CFLAGS_KDLPARSER)  $(CFLAGS_GTEST)
  LDFLAGS += $(LDFLAGS_KDLPARSER) $(LDFLAGS_GTEST)
  BINS_test = $(BINS_all)
endif

include $(BUILDSYSDIR)/base.mk
