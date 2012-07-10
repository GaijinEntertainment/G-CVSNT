/*
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef CVSTOOLS__H
#define CVSTOOLS__H

#ifndef CVSAPI__H
#include <cvsapi.h>
#endif
#include "export.h"

#ifndef PATCH_NULL
#ifdef	sun
/* solaris has a poor implementation of vsnprintf() which is not able to handle null pointers for %s */
# define PATCH_NULL(x) ((x)?(x):"<NULL>")
#else
# define PATCH_NULL(x) x
#endif
#endif

#include "GlobalSettings.h"
#include "ProtocolLibrary.h"
#include "TriggerLibrary.h"
#include "Cvsgui.h"
#include "Scramble.h"
#include "ServerInfo.h"
#include "RootSplitter.h"
#include "ServerConnection.h"
#include "EntriesParser.h"

#ifdef _WIN32
#include "win32/CvsCommonDialogs.h"
#include "win32/InfoPanel.h"
#endif

#endif

