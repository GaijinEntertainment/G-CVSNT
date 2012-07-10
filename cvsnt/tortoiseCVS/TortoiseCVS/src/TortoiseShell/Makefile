##########################################################################
# Variables

# Taget name
TARGET=TortoiseShell

# Binary file
BIN=TrtseShl.dll

# Source files
SOURCE=\
	../ContextMenus/DefaultMenus.cpp \
	../ContextMenus/HasMenu.cpp \
	../ContextMenus/MenuDescription.cpp \
	ColumnProvider.cpp \
	ContextMenu.cpp \
	IconOverlay.cpp \
	PropSheet.cpp \
	QueryInfo.cpp \
	ShellExt.cpp \
	StdAfx.cpp \
	../CVSGlue/CVSEntries.cpp \
	../CVSGlue/CVSIgnore.cpp \
	../CVSGlue/CVSRoot.cpp \
	../CVSGlue/CVSStatus.cpp \
	../CVSGlue/getline.cpp \
	../CVSGlue/Stat.cpp \
	../Utils/ClockRackCon.cpp \
	../Utils/CrapLexer.cpp \
	../Utils/Keyboard.cpp \
	../Utils/OsVersion.cpp \
	../Utils/LangUtils.cpp \
	../Utils/PathUtils.cpp \
	../Utils/ShellUtils.cpp \
	../Utils/StringUtils.cpp \
	../Utils/SyncUtils.cpp \
	../Utils/TimeUtils.cpp \
	../Utils/TortoiseUtils.cpp \
	../Utils/TortoiseRegistry.cpp \
	../Utils/TortoiseException.cpp \
	../Utils/TortoiseStatus.cpp \
	../Utils/Wildcard.cpp \
	../TortoiseAct/TortoiseActVerbs.cpp
ifeq ($(DEBUG),1)
SOURCE+=../Utils/TortoiseDebug.cpp
endif


# Resource script
RESOURCE=TortoiseShell.rc 


# Compiler flags
CXXFLAGS=$(STLP_CFLAGS) $(WX_CFLAGS) -I . -Wall -fno-rtti -DWIN32 -D_MBCS \
	-DTORTOISESHELL -D_USRDLL -D_WIN32_IE=0x0500 -O2 
ifeq ($(DEBUG),1)
CXXFLAGS+=-D_DEBUG 
else
CXXFLAGS+=-DNDEBUG 
endif


# Linker flags
LDFLAGS=$(STLP_LDFLAGS) $(WX_LDFLAGS) \
	-lole32 -lkernel32 -luser32 -lgdi32 -lshlwapi -lwininet -lwinmm \
	-Wl,--subsystem,windows,--enable-stdcall-fixup,ShellExt.def -mwindows

ifneq ($(DEBUG),1)
LDFLAGS+=-s
endif


# Resource compiler flags
RCFLAGS=--include-dir "$(WXDIR)/include" --define __WIN32__ \
	--define __WIN95__ --define __GNUWIN32__ --define _WINRESRC_

# dlltool flags
DLLTOOLFLAGS=-d ShellExt.def

# TortoiseCVS base dir
TORTOISE_SRC=..

# Include Make environment
include ../Makeenv

# Include Make program
include ../Makeprog
