#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR licensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
  OCR_TYPE=tg-xe
else
  ifneq (${OCR_TYPE}, tg-xe)
    $(error OCR_TYPE is set to ${OCR_TYPE} but expected tg-xe)
  endif
endif
#
# Tool-chain to be used for the build
#

DEFAULT_CONFIG = xe-default.cfg
APPS_LIBS_INSTALL_ROOT ?= ../../../../apps/apps/libs/install
ARCH ?= tg-xe
APPS_LIBS_INSTALL := ${APPS_LIBS_INSTALL_ROOT}/${ARCH}

# Check that this path exists, but only if we are building something
_T := $(APPS_LIBS_INSTALL)
APPS_LIBS_INSTALL := $(realpath $(_T))
ifeq (,$(APPS_LIBS_INSTALL))
  $(info Creating APPS_LIBS_INSTALL: $(_T))
  $(shell mkdir -p "$(_T)")
  APPS_LIBS_INSTALL := $(realpath $(_T))
endif

TG_INSTALL ?= ../../../../tg/tg/install

# Check that this path exists
_T := $(TG_INSTALL)
TG_INSTALL := $(realpath $(_T))
ifeq (,$(TG_INSTALL))
  $(error TG_INSTALL is not a valid path: $(_T))
endif

PREFIX  := $(TG_INSTALL)/bin/xstg-linux-elf-
CC      := $(PREFIX)clang
CFLAGS  := -fxstg-extensions -fno-builtin -Wno-typedef-redefinition -I $(TG_INSTALL)/include -I $(APPS_LIBS_INSTALL)/include/ -Wno-visibility

# Uncomment the following line to move the strings used in DPRINTF to another section
# which can then be moved to L2 or DRAM
# CFLAGS  += -DOCR_DPRINTF_SECTION=.dprintf_str

RM      := rm
RMFLAGS := -rf

CP      := cp
MKDIR   := mkdir
LN      := ln

# Shared libraries specific builds
LDFLAGS :=

# CFLAGS_SHARED will be concatenated with any
# common CFLAGS options
CFLAGS_SHARED :=

# Static libraries specific builds
# Same as for CFLAGS_SHARED
CFLAGS_STATIC := $(CFLAGS)
AR := $(PREFIX)ar
ARFLAGS := cru

RANLIB := $(PREFIX)ranlib

# Library supported
SUPPORTS_SHARED=no
SUPPORTS_STATIC=yes

.PHONY: all
all: static

.PHONY: debug
debug: debug-static

include ../common.mk
