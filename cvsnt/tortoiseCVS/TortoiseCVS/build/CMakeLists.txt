# TortoiseCVS - a Windows shell extension for easy version control
#
# Copyright (C) 2008 - Torsten Martinsen
# <torsten@bullestock.net> - June 2008
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

PROJECT(TortoiseCVS)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6.0)

CMAKE_POLICY(SET CMP0001 NEW)
CMAKE_POLICY(SET CMP0002 NEW)
CMAKE_POLICY(SET CMP0003 NEW)
CMAKE_POLICY(SET CMP0004 NEW)
CMAKE_POLICY(SET CMP0005 NEW)

ENABLE_TESTING()

OPTION(OPT_MARCH_HARE_BUILD "March Hare build" OFF)

#################################################
#
#                 UTILITY MACROS
#
#################################################

MACRO(PRINT_G STR)
    MESSAGE(STATUS "(*) ${STR}")
ENDMACRO(PRINT_G)

# Something was enabled.
MACRO(PRINT_E STR)
    MESSAGE(STATUS "(+) ${STR} enabled.")
ENDMACRO(PRINT_E)

# Something was disabled.
MACRO(PRINT_D STR)
    MESSAGE(STATUS "(-) ${STR} disabled.")
ENDMACRO(PRINT_D)

MACRO(ADD_FLEX_FILES)
    FOREACH(_current_FILE ${ARGN})
	GET_FILENAME_COMPONENT(_in ${_current_FILE} ABSOLUTE)
	GET_FILENAME_COMPONENT(_basename ${_current_FILE} NAME_WE)
	GET_DIRECTORY_PROPERTY(_top ${_cmakeparentdir} PARENT_DIRECTORY)

	SET(_out ${CMAKE_CURRENT_BINARY_DIR}/flex_${_basename}.cpp)

	ADD_CUSTOM_COMMAND(
            OUTPUT ${_out}
            COMMAND D:/CVSNT/GnuWin32/bin/flex++${CMAKE_EXECUTABLE_SUFFIX}
            ARGS
	    -+ -d -S${CMAKE_CURRENT_SOURCE_DIR}/flex.skl
            -o${_out}
            ${_in}
	    WORKING_DIRECTORY "D:/CVSNT/GnuWin32/bin"
            DEPENDS ${_in})

	LIST(APPEND CPP_SOURCE ${_out})
    ENDFOREACH(_current_FILE)
ENDMACRO(ADD_FLEX_FILES)

MACRO(ADD_BISON_FILES)
    FOREACH(_current_FILE ${ARGN})
	GET_FILENAME_COMPONENT(_in ${_current_FILE} ABSOLUTE)
	GET_FILENAME_COMPONENT(_basename ${_current_FILE} NAME_WE)
	GET_DIRECTORY_PROPERTY(_top ${_cmakeparentdir} PARENT_DIRECTORY)

	SET(_out ${CMAKE_CURRENT_BINARY_DIR}/bison_${_basename}.cpp)

    ADD_CUSTOM_COMMAND(
            OUTPUT ${_out}
            COMMAND D:/CVSNT/GnuWin32/bin/bison${CMAKE_EXECUTABLE_SUFFIX}
            ARGS
            --name-prefix=getdate_ -o ${_out} -d ${_in} -S yacc.c
	    WORKING_DIRECTORY "D:/CVSNT/GnuWin32/bin"
            DEPENDS ${_in})

	LIST(APPEND CPP_SOURCE ${_out})
    ENDFOREACH(_current_FILE)
ENDMACRO(ADD_BISON_FILES)


# Set EXTRA_CFLAGS for the project, in particular precompiled headers.
#
# Uses the following variables:
#    CPP_SOURCE     List of C++ source files.
#    C_SOURCE       List of C source files.
#    STDAFX_SOURCE  Precompiled header file, usually stdafx.cpp.
#
# Arguments:
#    1   Name of project (target)
MACRO(SET_CFLAGS project)

    IF(OPT_DEST_VISUALSTUDIO)
	# Precompiled headers
	IF(STDAFX_SOURCE)
            FOREACH(file ${PRECOMP_SOURCE})
		IF(${file} MATCHES ".*\\.c(pp)?")
		    GET_SOURCE_FILE_PROPERTY(
			props
			${file}
			COMPILE_FLAGS)
		    IF("${props}" STREQUAL "NOTFOUND")
			SET(props "")
		    ENDIF("${props}" STREQUAL "NOTFOUND")
		    # Append /Od to existing properties
		    SET(props "${props} /Fp${CMAKE_CFG_INTDIR}/${project}.pch /Yustdafx.h")
      		    SET_SOURCE_FILES_PROPERTIES(
			${file}
			PROPERTIES
			COMPILE_FLAGS ${props})
		ENDIF(${file} MATCHES ".*\\.c(pp)?")
            ENDFOREACH(file)

	    SET_SOURCE_FILES_PROPERTIES(
		${STDAFX_SOURCE}
		PROPERTIES COMPILE_FLAGS "${EXTRA_CFLAGS} /Fp${CMAKE_CFG_INTDIR}/${project}.pch /Ycstdafx.h")
	ENDIF(STDAFX_SOURCE)

	# Unicode
	IF(USE_UNICODE)
            ADD_DEFINITIONS(-D_UNICODE -DUNICODE)
	ELSE(USE_UNICODE)
            ADD_DEFINITIONS(-D_MBCS)
	ENDIF(USE_UNICODE)

	ADD_DEFINITIONS(-D_DLL)

        # 1400 is Microsoft Visual Studio 2005, Visual C++ 8.0
        # 1500 is Microsoft Visual C++ 2008, Visual C++ 9.0
        IF(MSVC_VERSION EQUAL 1400 OR MSVC_VERSION GREATER 1400)
            ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_NON_CONFORMING_SWPRINTFS)
        ENDIF(MSVC_VERSION EQUAL 1400 OR MSVC_VERSION GREATER 1400)
        IF(OPT_MARCH_HARE_BUILD)
           ADD_DEFINITIONS(-DMARCH_HARE_BUILD)
        ENDIF(OPT_MARCH_HARE_BUILD)
    ENDIF(OPT_DEST_VISUALSTUDIO)

ENDMACRO(SET_CFLAGS)


#################################################
#
#            COMPILER-SPECIFIC SETUP
#
#################################################
  	 
# MINGW specific options.
OPTION(OPT_DEST_MINGW "Enable mingw specific options" OFF)
IF(OPT_DEST_MINGW)
  PRINT_E("MINGW specific options")
ENDIF(OPT_DEST_MINGW)

# Visual Studio specific options.
OPTION(OPT_DEST_VISUALSTUDIO "Enable Visual Studio specific options" OFF)
IF(OPT_DEST_VISUALSTUDIO)
  PRINT_E("VC++ specific options")
ENDIF(OPT_DEST_VISUALSTUDIO)

# Generic compiler flags.
ADD_DEFINITIONS(
    # TODO
)

#
# Visual Studio .NET 2003
#

IF(OPT_DEST_VISUALSTUDIO)
#    SET(CMAKE_CXX_FLAGS_DEBUG "/wd4800 /D_DEBUG /Od /Gm /EHsc /RTC1 /MDd /W3 /nologo /ZI" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_CXX_FLAGS_RELEASE "/wd4800 /O2 /Ob2 /DNDEBUG /EHsc /MD /W3 /nologo" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_CXX_FLAGS_MINSIZEREL "/wd4800 /O1 /Ob1 /DNDEBUG /EHsc /MD /W3 /nologo" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/wd4800 /O2 /Ob1 /DNDEBUG /EHsc /MD /W3 /nologo /Zi" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_C_FLAGS_DEBUG "/wd4800 /D_DEBUG /Od /Gm /EHsc /RTC1 /MDd /W3 /nologo /ZI" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_C_FLAGS_RELEASE "/wd4800 /wd4244 /wd4305 /O2 /Ob2 /DNDEBUG /EHsc /MD /W3 /nologo" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_C_FLAGS_MINSIZEREL "/wd4800 /wd4244 /wd4305 /O1 /Ob1 /DNDEBUG /EHsc /MD /W3 /nologo" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "/wd4800 /wd4244 /wd4305 /O2 /Ob1 /DNDEBUG /EHsc /MD /W3 /nologo /Zi" 
#       CACHE STRING "MSVC MT flags " FORCE)
#    SET(CMAKE_CXX_STACK_SIZE 0)
ENDIF(OPT_DEST_VISUALSTUDIO)


#################################################
#
#               INCLUDE DIRECTORIES
#
#################################################

SET(WX_INCLUDE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/include/msvc
    /cvsbin/inc
)

SET(WX_SUBDIR_NAME "vc9")
# 1310 is Microsoft Visual Studio 2003 .NET, Visual C++ 7.x
# 1400 is Microsoft Visual Studio 2005, Visual C++ 8.0
# 1500 is Microsoft Visual C++ 2008, Visual C++ 9.0
IF(MSVC_VERSION EQUAL 1500 OR MSVC_VERSION GREATER 1500)
  SET(WX_SUBDIR_NAME "vc9")
ELSE(MSVC_VERSION EQUAL 1500 OR MSVC_VERSION GREATER 1500)
  IF(MSVC_VERSION EQUAL 1400 OR MSVC_VERSION GREATER 1400)
    SET(WX_SUBDIR_NAME "vc8")
  ENDIF(MSVC_VERSION EQUAL 1400 OR MSVC_VERSION GREATER 1400)
ENDIF(MSVC_VERSION EQUAL 1500 OR MSVC_VERSION GREATER 1500)

IF(CMAKE_GENERATOR MATCHES Win64)
    SET(WX_LIBS
        debug ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxbase28ud64.lib optimized ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxbase28u64.lib
        debug ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28ud_adv64.lib optimized ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28u_adv64.lib
        debug ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28ud_core64.lib optimized ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28u_core64.lib
    )
ELSE(CMAKE_GENERATOR MATCHES Win64)
    SET(WX_LIBS
        debug ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxbase28ud.lib optimized ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxbase28u.lib
        debug ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28ud_adv.lib optimized ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28u_adv.lib
        debug ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28ud_core.lib optimized ${CMAKE_CURRENT_SOURCE_DIR}/../wxw/lib/${WX_SUBDIR_NAME}/wxmsw28u_core.lib
    )
ENDIF(CMAKE_GENERATOR MATCHES Win64)

IF(OPT_MARCH_HARE_BUILD)
   SET(CVSAPILIB cvsapi.lib)
ELSE(OPT_MARCH_HARE_BUILD)
   SET(CVSAPILIB "")
ENDIF(OPT_MARCH_HARE_BUILD)

#################################################
#
#                SUBDIRECTORIES
#
#################################################
  	 
# Static libraries
ADD_SUBDIRECTORY(../src/CVSGlue CVSGlue)
ADD_SUBDIRECTORY(../src/cvstree cvstree)
ADD_SUBDIRECTORY(../src/Utils Utils)
ADD_SUBDIRECTORY(../tools/cppunit/src/cppunit CppUnit)

# DLL's
ADD_SUBDIRECTORY(../src/TortoiseShell TortoiseShell)

# Applications
ADD_SUBDIRECTORY(../src/PostInst PostInst)
ADD_SUBDIRECTORY(../src/RunTimeInstaller RunTimeInstaller)
ADD_SUBDIRECTORY(../src/TortoiseAct TortoiseAct)
ADD_SUBDIRECTORY(../src/TortoisePlink TortoisePlink)
ADD_SUBDIRECTORY(../src/TortoiseSetupHelper TortoiseSetupHelper)
ADD_SUBDIRECTORY(../src/TranslateIss TranslateIss)

# Unit tests
ADD_SUBDIRECTORY(../src/Test/UnitTests UnitTest)
