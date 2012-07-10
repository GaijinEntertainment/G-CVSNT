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

PROJECT(TortoiseShell)

SET(USE_UNICODE 1)

INCLUDE_DIRECTORIES(
   ${WX_INCLUDE_PATH}
   ..
   .
)

ADD_DEFINITIONS(
    -D_USRDLL
    -D__WXMSW__
    -DTORTOISESHELL
    -DWINVER=0x0500
    -DSTRICT
    -DHAVE_ERRNO_H
)

SET(CPP_SOURCE
   ../CVSGlue/CVSRoot.cpp
   ../CVSGlue/CVSStatus.cpp
   ../CVSGlue/CVSStatusShared.cpp
   ../CVSGlue/CvsEntries.cpp
   ../CVSGlue/CvsIgnore.cpp
   ../CVSGlue/Stat.cpp
   ../CVSGlue/getline.cpp
   ../ContextMenus/DefaultMenus.cpp
   ../ContextMenus/HasMenu.cpp
   ../ContextMenus/MenuDescription.cpp
   ../TortoiseAct/TortoiseActVerbs.cpp
   ../Utils/CrapLexer.cpp
   ColumnProvider.cpp
   ContextMenu.cpp
   IconOverlay.cpp
   PropSheet.cpp
   QueryInfo.cpp
   ShellExt.cpp
)

SET(PRECOMP_SOURCE ${CPP_SOURCE})

SET(STDAFX_SOURCE StdAfx.cpp)

SET_CFLAGS(TortoiseShell)

LINK_LIBRARIES(
    CVSGlue
    Utils
    ${WX_LIBS}
    comctl32.lib
    gdiplus.lib
    rpcrt4.lib
    shlwapi.lib
    ws2_32.lib
)

ADD_LIBRARY(TortoiseShell SHARED ${STDAFX_SOURCE} ${CPP_SOURCE} ShellExt.def TortoiseShell.rc)
