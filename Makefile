# Copyright (C) 2004, 2005 Valery Reznic
# This file is part of the Elf Statifier project
# 
# This project is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License.
# See LICENSE file in the doc directory.

SUBDIRS  = src man rpm 

SOURCES =           \
   configure        \
   Makefile         \
   Makefile.common  \
   Makefile.include \
   Makefile.top     \
   VERSION          \
   RELEASE          \
   $(DOCS)          \
   $(CONFIGS)       \

DOCS =       \
   AUTHORS   \
   ChangeLog \
   FAQ       \
   INSTALL   \
   LICENSE   \
   NEWS      \
   README    \
   THANKS    \
   TODO      \
   $(addprefix doc/,$(DOC_DOCS))

DOC_DOCS =                                \
   README                                 \
   Background.txt                         \
   DataFlow.txt                           \
   Implementation.txt                     \
   MoreDetails.txt                        \
   MoreProblems.txt                       \
   Porting.txt                            \
   StatifiedLayout.txt                    \
   $(addprefix 1.5.0/, $(DOC_1_5_0_DOCS)) \

DOC_1_5_0_DOCS =             \
   StatifiedLayout.alpha.txt \
   StatifiedLayout.x86.txt   \
   StatifiedLayout.txt       \
   StarterLayout.txt         \


CONFIGS = $(addprefix configs/config.,$(SUPPORTED_CPU_LIST))

all: config

dist-list-for-tar: config

# It is simpler always re-make config and do not check dependencies.
# Configure care not change config's timestamp if content was not changed
.PHONY: config
config: configure
	/bin/sh ./configure

TOP_DIR := .
include Makefile.top
