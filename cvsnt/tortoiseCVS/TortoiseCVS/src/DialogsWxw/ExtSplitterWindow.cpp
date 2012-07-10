// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - December 2003

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


#include "StdAfx.h"
#include "ExtSplitterWindow.h"
#include "../Utils/TortoiseDebug.h"

// Events
BEGIN_EVENT_TABLE (ExtSplitterWindow, wxSplitterWindow)
   EVT_SPLITTER_SASH_POS_CHANGED(-1, ExtSplitterWindow::OnSashPosChanged) 
   EVT_SPLITTER_SASH_POS_CHANGING(-1, ExtSplitterWindow::OnSashPosChanging) 
   EVT_SIZE(ExtSplitterWindow::OnSize)
END_EVENT_TABLE()


// Constructor
ExtSplitterWindow::ExtSplitterWindow(wxWindow *parent, wxWindowID id,
                                     const wxPoint& pos,
                                     const wxSize& size,
                                     long style,
                                     const wxString& name)
   : wxSplitterWindow(parent, id, pos, size, style, name)
{
}



// Destructor
ExtSplitterWindow::~ExtSplitterWindow()
{
}



// Split vertically
bool ExtSplitterWindow::SplitVertically(wxWindow *window1,
                                        wxWindow *window2,
                                        int sashPosition)
{
   bool b = wxSplitterWindow::SplitVertically(window1, window2, sashPosition);
   if (sashPosition < 0)
      m_RightSashPos = -sashPosition;
   else
      m_RightSashPos = -1;

   return b;
}



// Split horizontally
bool ExtSplitterWindow::SplitHorizontally(wxWindow *window1,
                                          wxWindow *window2,
                                          int sashPosition)
{
   TDEBUG_ENTER("ExtSplitterWindow::SplitHorizontally");
   bool b = wxSplitterWindow::SplitHorizontally(window1, window2, sashPosition);
   if (sashPosition < 0)
      m_RightSashPos = -sashPosition;
   else
      m_RightSashPos = -1;
   return b;
}



// Set right sash position
void ExtSplitterWindow::SetRightSashPosition(int position, const bool redraw)
{
   SetSashPosition(GetWindowSize() - position, redraw);
   m_RightSashPos = position;
}



// Get right sash position
int ExtSplitterWindow::GetRightSashPosition()
{
   return GetWindowSize() - GetSashPosition();
}



// Sash pos changed event handler
void ExtSplitterWindow::OnSashPosChanged(wxSplitterEvent& WXUNUSED(event))
{
   TDEBUG_ENTER("ExtSplitterWindow::OnSashPosChanged");
   UpdateRightSashPosition();
}



// Sash pos changing event handler
void ExtSplitterWindow::OnSashPosChanging(wxSplitterEvent& WXUNUSED(event))
{
   TDEBUG_ENTER("ExtSplitterWindow::OnSashPosChanged");
   UpdateRightSashPosition();
}



// Size event handler
void ExtSplitterWindow::OnSize(wxSizeEvent& e)
{
   TDEBUG_ENTER("ExtSplitterWindow::OnSize");
   if (m_RightSashPos >= 0)
   {
      SetSashPosition(e.GetSize().GetHeight() - m_RightSashPos);
   }
   e.Skip();
}



// Update right sash position
void ExtSplitterWindow::UpdateRightSashPosition()
{
   if (m_RightSashPos >= 0)
   {
      m_RightSashPos = GetWindowSize() - GetSashPosition();
   }
}
