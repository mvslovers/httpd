# Root of the repository (auto-detected)
ROOT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Tools path (mvsasm etc.)
export PATH := $(ROOT_DIR)scripts:$(PATH)

# Cross-compiler
CC       := c2asm370
CFLAGS   := -fverbose-asm -S -O1

# Defines and include paths
DEFS     := -DLUA_USE_C89 -DLUA_USE_JUMPTABLE=0
INC_DIR  := $(ROOT_DIR)include
INC1     := $(ROOT_DIR)credentials/include
INC2     := $(ROOT_DIR)contrib/crent370_sdk/inc
INC3     := $(ROOT_DIR)contrib/ufs_sdk/inc
INC4     := $(ROOT_DIR)contrib/lua370_sdk/inc
INC5     := $(ROOT_DIR)contrib/mqtt370_sdk/inc
INCS     := -I$(INC_DIR) -I$(INC1) -I$(INC2) -I$(INC3) -I$(INC4) -I$(INC5)

CFLAGS   += $(DEFS) $(INCS)

# export MACn variables for mvsasm->jobasm script
export MAC1=MDR.CRENT370.MACLIB

# export dataset names used by mvsasm script
export MVSASM_PUNCH=MDR.HTTPD.OBJECT
export MVSASM_SYSLMOD=MDR.HTTPD.NCALIB

# Warning collection file
export BUILD_WARNINGS := $(ROOT_DIR).build-warnings
