// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Alan Dabiri
// <killjoy@users.sourceforge.net> - March 2004

// Copyright (C) 2005 - Hartmut Honisch
// <Hartmut_Honisch@web.de>

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - March 2005

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

#ifndef ANNOTATE_LISTCTRL_H
#define ANNOTATE_LISTCTRL_H

#include "ExtListCtrl.h"


class AnnotateDialog;
class AnnotateCanvas;


// Virtual list control for Annotate dialog
class AnnotateListCtrl : public ExtListCtrl
{
public:
   // Constructor
   AnnotateListCtrl(AnnotateDialog* parent, wxWindowID id, AnnotateCanvas* canvas);

   virtual void Refresh();

   void SetHighlighting();

protected:
   // Get item text
   wxString OnGetItemText(long item, long column) const;
   // Get item format
   wxListItemAttr *OnGetItemAttr(long item) const;

   void ContextMenu(wxContextMenuEvent& event);

   void OnMenuDiff(wxCommandEvent&);
   
   void OnMenuView(wxCommandEvent&);

   void OnMenuAnnotate(wxCommandEvent&);

   void OnMenuGet(wxCommandEvent&);

   void OnMenuSaveAs(wxCommandEvent&);

   void OnMenuViewLogMessage(wxCommandEvent&);
   
   void OnHighlightNone(wxCommandEvent&);

   void OnHighlightAuthor(wxCommandEvent&);
   
   void OnHighlightRev(wxCommandEvent&);
   
   void OnHighlightDate(wxCommandEvent&);
   
   void OnHighlightAgeFixed(wxCommandEvent&);
   
   void OnHighlightAgeScaled(wxCommandEvent&);

    void OnHighlightBugNumber(wxCommandEvent&);
    
    void DoHighlightAuthor(const std::string& author);
    void DoHighlightRev(const std::string& rev);
    void DoHighlightDate(const tm& date);
   
private:
   AnnotateDialog*      myParent;
   AnnotateCanvas*      myCanvas;

   wxColour GetAgeColor(unsigned int ageInDays, unsigned int intervals, unsigned int period);

   DECLARE_EVENT_TABLE()
};

#endif
