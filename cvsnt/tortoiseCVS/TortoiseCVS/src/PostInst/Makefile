##########################################################################
# Variables

# Target name
TARGET=PostInst

# Binary file
BIN=PostInst.exe

# Source files
SOURCE=\
	PostInst.cpp				\
	StdAfx.cpp				\
	../Utils/PathUtils.cpp			\
	../Utils/ShellUtils.cpp			\
	../Utils/StringUtils.cpp		\
	../Utils/SyncUtils.cpp			\
	../Utils/TortoiseException.cpp		\
	../Utils/TortoiseRegistry.cpp		\
	../Utils/TortoiseUtils.cpp		\
	../Utils/OsVersion.cpp
ifeq ($(DEBUG),1)
SOURCE+=../Utils/TortoiseDebug.cpp
endif

# Compiler flags
CXXFLAGS=$(STLP_CFLAGS) -I . -Wall -fno-rtti -DWIN32 -D_MBCS -O2 -DNOTRANSLATE 
ifeq ($(DEBUG),1)
CXXFLAGS+=-D_DEBUG 
else
CXXFLAGS+=-DNDEBUG 
endif

# Linker flags
LDFLAGS=$(STLP_LDFLAGS) -lkernel32 -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -luuid -lshlwapi -lcomctl32 \
	-Wl,--subsystem,windows -mwindows
ifneq ($(DEBUG),1)
LDFLAGS+=-s
endif

# TortoiseCVS base dir
TORTOISE_SRC=..

# Include Make environment
include ../Makeenv

# Include Make program
include ../Makeprog
