// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2002

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

#ifndef EXT_LISTCTRL_H
#define EXT_LISTCTRL_H


#include <wx/wx.h>
#include <wx/listctrl.h>
#include <windows.h>
#include <commctrl.h>

BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_LIST_ITEM_CHECKED, 731)
    DECLARE_EVENT_TYPE(wxEVT_COMMAND_LIST_ITEM_UNCHECKED, 732)
END_DECLARE_EVENT_TYPES()


#define EVT_LIST_ITEM_CHECKED(id, fn)                                                                   \
        DECLARE_EVENT_TABLE_ENTRY(wxEVT_COMMAND_LIST_ITEM_CHECKED, id, -1,                              \
                                  (wxObjectEventFunction) (wxEventFunction) (wxListEventFunction) &fn,  \
                                  (wxObject*) NULL),

#define EVT_LIST_ITEM_UNCHECKED(id, fn)                                                                 \
        DECLARE_EVENT_TABLE_ENTRY(wxEVT_COMMAND_LIST_ITEM_UNCHECKED, id, -1,                            \
                                  (wxObjectEventFunction) (wxEventFunction) (wxListEventFunction) &fn,  \
                                  (wxObject*) NULL),


class ExtHeaderCtrl;


class ExtListCtrl : public wxListCtrl
{
public:
   // Default constructor
   ExtListCtrl() : wxListCtrl() { Init(); }

   // Constructor
   ExtListCtrl(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
      const wxSize& size, long style)
      : wxListCtrl(parent, id, pos, size, style)
   {
      Init();
      Create(parent, id, pos, size, style);
   }

   // Destructor
   ~ExtListCtrl();

   bool Create(wxWindow *parent, wxWindowID id = -1, const wxPoint& pos = wxDefaultPosition, 
               const wxSize& size = wxDefaultSize, long style = wxLC_ICON, 
               const wxValidator& validator = wxDefaultValidator, const wxString& name = _T("ExtListCtrl"));

   bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

   bool IsChecked(int index);
   bool IsSelected(int index);
   void SetCheckboxes(bool enable);
   void SetChecked(int index, bool check);

   void SetBestSize(const wxSize max, const wxSize min);
   void SetBestSize(int maxWidth = -1, int maxHeight = -1, int minWidth = -1,
                    int minHeight = -1);
   void SetBestColumnWidth(int index);
   bool inline HasCheckBoxes() { return myHasCheckBoxes; };
   wxPoint GetContextMenuPos(long item);
   void SetSortIndicator(int column, bool sortAsc);

   // Show focus rect
   void ShowFocusRect();

protected:
   // common part of all ctors
   void Init();

   virtual int OnGetItemState(long item) const; 
   virtual int OnGetItemImage(long item) const;

   virtual int OnCustomDraw(WXLPARAM lParam);
   
   ExtHeaderCtrl* myHeaderCtrl;
private:
   void OnKeyDown(wxKeyEvent& e);
   void OnLeftClick(wxCommandEvent& e);
   bool myDisableCheckEvents;
   bool myHasCheckBoxes;

   DECLARE_EVENT_TABLE()
};

#endif
