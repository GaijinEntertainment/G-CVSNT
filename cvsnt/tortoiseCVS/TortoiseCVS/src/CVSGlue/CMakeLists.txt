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
    CVSAction.cpp
    CVSFeatures.cpp
    CVSRoot.cpp
    CVSStatus.cpp
    CvsEntries.cpp
    CvsIgnore.cpp
    MakeArgs.cpp
    RemoteLists.cpp
    Stat.cpp
    getline.cpp
    ndir.cpp
    CVSStatusShared.cpp
    )

SET(PRECOMP_SOURCE ${CPP_SOURCE})

SET(STDAFX_SOURCE stdafx.cpp)

SET_CFLAGS(CVSGlue)

ADD_LIBRARY(CVSGlue STATIC ${STDAFX_SOURCE} ${CPP_SOURCE})
