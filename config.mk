# Root of the repository (auto-detected)
ROOT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Load personalizable settings from .env
-include $(ROOT_DIR).env

export MVS_HOST MVS_PORT MVS_USER MVS_PASS
export MVS_JOBCLASS MVS_MSGCLASS

# Tools path (mvsasm etc.)
export PATH := $(ROOT_DIR)scripts:$(PATH)

# Cross-compiler
CC       := c2asm370
CFLAGS   := -fverbose-asm -S -O1

# Version (override on command line or in .env)
HTTPD_VERSION ?= 3.3.1-dev

# Defines and include paths
DEFS     := -DLUA_USE_C89 -DLUA_USE_JUMPTABLE=0 -DHTTPD_VERSION=\"$(HTTPD_VERSION)\"
INC_DIR  := $(ROOT_DIR)include
INC1     := $(ROOT_DIR)credentials/include
INC2     := $(ROOT_DIR)contrib/crent370_sdk/inc
INC3     := $(ROOT_DIR)contrib/ufs370_sdk/inc
INC4     := $(ROOT_DIR)contrib/lua370_sdk/inc
INC5     := $(ROOT_DIR)contrib/mqtt370_sdk/inc
INCS     := -I$(INC_DIR) -I$(INC1) -I$(INC2) -I$(INC3) -I$(INC4) -I$(INC5)

CFLAGS   += $(DEFS) $(INCS)

# Map .env variables to mvsasm exports
export MAC1=$(CRENT_MACLIB)
export MVSASM_PUNCH=$(HTTPD_PUNCH)
export MVSASM_SYSLMOD=$(HTTPD_SYSLMOD)

# Warning collection file
export BUILD_WARNINGS := $(ROOT_DIR).build-warnings
