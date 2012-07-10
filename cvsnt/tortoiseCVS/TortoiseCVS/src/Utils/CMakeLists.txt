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

SET(USE_UNICODE 1)

INCLUDE_DIRECTORIES(
   ${WX_INCLUDE_PATH}
   ..
)

ADD_DEFINITIONS(
   
)

SET(CPP_SOURCE
    Cache.cpp
    ClockRackCon.cpp
    FileTree.cpp
    HttpSnaffle.cpp
    Keyboard.cpp
    LangUtils.cpp
    LineReader.cpp
    LogUtils.cpp
    MultiMonUtils.cpp
    OsVersion.cpp
    PathUtils.cpp
    Preference.cpp
    ProcessUtils.cpp
    RepoUtils.cpp
    SandboxUtils.cpp
    Serialize.cpp
    ShellUtils.cpp
    SoundUtils.cpp
    StringUtils.cpp
    SyncUtils.cpp
    TimeUtils.cpp
    TortoiseDebug.cpp
    TortoiseException.cpp
    TortoiseRegistry.cpp
    TortoiseStatus.cpp
    TortoiseUtils.cpp
    UnicodeStrings.cpp
    Wildcard.cpp
    GetHomeDirectory.cpp
    LaunchTortoiseAct.cpp
    ShellUtils2.cpp
)

SET(PRECOMP_SOURCE ${CPP_SOURCE})

ADD_BISON_FILES(getdate.y)

SET_CFLAGS(Utils)

ADD_LIBRARY(Utils STATIC ${STDAFX_SOURCE} ${CPP_SOURCE})
