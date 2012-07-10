// TortoiseCVS - a Windows shell extension for easy version control
//
// Interface to CInfoPanel

// Copyright 2007 - Tony Hoyle and March Hare Software Ltd

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// InfoPanel - contributed by March Hare Software Ltd 2009

// PCH is disabled, because if we use it visual studio ignores all text before the "stdafx.h" line,
// which includes winsock 1.. which in turn we don't want (Windows 3.1 compatibility not being a 
// priority at this point)

#include "StdAfx.h"

#ifdef MARCH_HARE_BUILD

#include <winsock2.h>

#undef HAVE_ERRNO_H
#undef SIZEOF_INT
#undef SIZEOF_LONG
#undef SIZEOF_SHORT

// This lot should probably be in the include search path
#include "/cvsbin/inc/cvsapi.h"
#include "/cvsbin/inc/cvstools.h"

#pragma comment(lib,"/cvsbin/lib/cvsapi.lib")
#pragma comment(lib,"/cvsbin/lib/cvstools.lib")

#include "InfoPanel.h"

BEGIN_EVENT_TABLE(tcvsInfoPanel, wxPanel)
	EVT_WINDOW_CREATE(OnCreate)
	EVT_SIZE(OnSize)
END_EVENT_TABLE()

const wxSize tcvsInfoPanel::defaultSize(-1,100);

tcvsInfoPanel::tcvsInfoPanel(wxWindow* parent, wxWindowID id /*= wxID_ANY*/, const wxPoint& pos /*= wxDefaultPosition*/)
	: wxPanel(parent, id, pos, defaultSize, wxSUNKEN_BORDER)
{
	m_panel = new CInfoPanel;
	m_bCreated = false;
}

tcvsInfoPanel::~tcvsInfoPanel()
{
	if(m_panel) delete m_panel;
}

void tcvsInfoPanel::OnCreate(wxWindowCreateEvent& event)
{
	// Doesn't get triggered.. no idea why
	m_panel->Create((HWND)GetHWND());
	m_bCreated = true;
}

void tcvsInfoPanel::OnSize(wxSizeEvent& event)
{
	if(!m_bCreated)
	{
		// Hack - see OnCreate
		m_panel->Create((HWND)GetHWND());
		m_bCreated = true;
	}	
	m_panel->ResizeWindowToParent();
}

#endif // MARCH_HARE_BUILD
