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

#ifndef TORTOISE_INFOPANEL__H
#define TORTOISE_INFOPANEL__H

class CInfoPanel;

class tcvsInfoPanel : public wxPanel
{
public:
	tcvsInfoPanel(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition);
	virtual ~tcvsInfoPanel();

protected:
	bool m_bCreated;
	CInfoPanel *m_panel;
	static const wxSize defaultSize;

	void OnCreate(wxWindowCreateEvent& event);
	void OnSize(wxSizeEvent& event);

	DECLARE_EVENT_TABLE() ;
};

#endif
