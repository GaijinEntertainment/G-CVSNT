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

#ifndef EXT_SPLITTER_WINDOW_H
#define EXT_SPLITTER_WINDOW_H

#include <wx/splitter.h>


// Flags for resizing
#define SPLITTER_RESIZE_PANE_1 0
#define SPLITTER_RESIZE_PANE_2 1


class ExtSplitterWindow : public wxSplitterWindow
{
public:
   // Constructor
   ExtSplitterWindow(wxWindow *parent, wxWindowID id,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxSP_3D,
                     const wxString& name = wxT("extSplitterWindow"));

              
   // Destructor
   ~ExtSplitterWindow();

   // Split vertically
   virtual bool SplitVertically(wxWindow *window1,
                                wxWindow *window2,
                                int sashPosition = 0);

   
   // Split horizontally
   virtual bool SplitHorizontally(wxWindow *window1,
                                  wxWindow *window2,
                                  int sashPosition = 0);

   // Set right sash position
   void SetRightSashPosition(int position, const bool redraw = TRUE);

   // Get right sash position
   int GetRightSashPosition();

protected:
   
private:
   // Sash pos changed event handler
   void OnSashPosChanged(wxSplitterEvent& e);
   // Sash pos changing event handler
   void OnSashPosChanging(wxSplitterEvent& e);
   // Size event handler
   void OnSize(wxSizeEvent& e);
   // Update right sash position
   void UpdateRightSashPosition();

   
   int m_RightSashPos;

   DECLARE_EVENT_TABLE()
};


#endif /* EXT_SPLITTER_WINDOW_H */
