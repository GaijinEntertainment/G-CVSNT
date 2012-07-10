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

PROJECT(TortoiseSetupHelper)

SET(USE_UNICODE 1)

INCLUDE_DIRECTORIES(
   ${WX_INCLUDE_PATH}
   
)

ADD_DEFINITIONS(
    -DNOTRANSLATE=1
)

SET(CPP_SOURCE
   ../Utils/StringUtils.cpp
   StdAfx.cpp
   TortoiseSetupHelper.cpp
)

SET(PRECOMP_SOURCE ${CPP_SOURCE})

LINK_LIBRARIES(
    ${WX_LIBS}
    shlwapi.lib comctl32.lib rpcrt4.lib
)

SET_CFLAGS(TortoiseSetupHelper)
ADD_EXECUTABLE(TortoiseSetupHelper WIN32 ${STDAFX_SOURCE} ${C_SOURCE} ${CPP_SOURCE})

SET_TARGET_PROPERTIES(TortoiseSetupHelper PROPERTIES LINK_FLAGS_RELEASE "/NODEFAULTLIB:wxbase28u_net.lib /NODEFAULTLIB:wxbase28u_xml.lib /NODEFAULTLIB:wxexpat.lib /NODEFAULTLIB:wxjpeg.lib /NODEFAULTLIB:wxmsw28u_aui.lib /NODEFAULTLIB:wxmsw28u_html.lib /NODEFAULTLIB:wxmsw28u_media.lib /NODEFAULTLIB:wxmsw28u_qa.lib /NODEFAULTLIB:wxmsw28u_richtext.lib /NODEFAULTLIB:wxmsw28u_xrc.lib /NODEFAULTLIB:wxpng.lib /NODEFAULTLIB:wxregexu.lib /NODEFAULTLIB:wxtiff.lib /NODEFAULTLIB:wxzlib.lib")
SET_TARGET_PROPERTIES(TortoiseSetupHelper PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:wxbase28ud_net.lib /NODEFAULTLIB:wxbase28ud_xml.lib /NODEFAULTLIB:wxexpatd.lib /NODEFAULTLIB:wxjpegd.lib /NODEFAULTLIB:wxmsw28ud_aui.lib /NODEFAULTLIB:wxmsw28ud_html.lib /NODEFAULTLIB:wxmsw28ud_media.lib /NODEFAULTLIB:wxmsw28ud_qa.lib /NODEFAULTLIB:wxmsw28ud_richtext.lib /NODEFAULTLIB:wxmsw28ud_xrc.lib /NODEFAULTLIB:wxpngd.lib /NODEFAULTLIB:wxregexud.lib /NODEFAULTLIB:wxtiffd.lib /NODEFAULTLIB:wxzlibd.lib")
