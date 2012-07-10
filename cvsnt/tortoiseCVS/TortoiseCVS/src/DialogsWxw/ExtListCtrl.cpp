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

#include "StdAfx.h"
#include "ExtListCtrl.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseUtils.h"
#include <wx/msw/private.h>

// This should not be needed, but...
#ifdef UNICODE
#define DrawText  DrawTextW
#else
#define DrawText  DrawTextA
#endif // !UNICODE

#ifndef HDF_SORTUP
#define HDF_SORTUP 0x0400
#endif
#ifndef HDF_SORTDOWN
#define HDF_SORTDOWN 0x0200
#endif


class ExtHeaderCtrl : public wxControl
{
   DECLARE_DYNAMIC_CLASS(ExtHeaderCtrl)
public:
   // Constructor
   ExtHeaderCtrl();

   // Custom draw handler
   bool MSWOnDraw(WXDRAWITEMSTRUCT *item);

   // Set sort indicator
   void SetSortIndicator(int column, bool sortAsc);

protected:
   // Sort column
   int mySortColumn;
   // Sort direction
   bool mySortAsc;
};

IMPLEMENT_DYNAMIC_CLASS(ExtHeaderCtrl, wxControl)



//////////////////////////
// ExtListCtrl



BEGIN_EVENT_TABLE(ExtListCtrl, wxListCtrl)
   EVT_KEY_DOWN(ExtListCtrl::OnKeyDown)
   EVT_COMMAND_LEFT_CLICK(-1, ExtListCtrl::OnLeftClick)
END_EVENT_TABLE()


DEFINE_EVENT_TYPE( wxEVT_COMMAND_LIST_ITEM_CHECKED )
DEFINE_EVENT_TYPE( wxEVT_COMMAND_LIST_ITEM_UNCHECKED )



// Constructor
bool ExtListCtrl::Create(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
                         const wxSize& size, long style, const wxValidator& validator,
                         const wxString& name)

{
   if (style & wxLC_VIRTUAL)
   {
      SendMessage(GetHwnd(), LVM_SETCALLBACKMASK, LVIS_STATEIMAGEMASK | LVIS_OVERLAYMASK, 0);
   }

   return TRUE;
}



// Destructor
ExtListCtrl::~ExtListCtrl()
{
   if (myHeaderCtrl)
   {
      RemoveChild(myHeaderCtrl);
      myHeaderCtrl->UnsubclassWin();
      myHeaderCtrl->SetHWND(0);
      delete myHeaderCtrl;
   }
}



// Common part of all constructors
void ExtListCtrl::Init()
{
   myDisableCheckEvents = false;
   myHasCheckBoxes = false;
   myHeaderCtrl = 0;
}


  
bool ExtListCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result)
{
    NMHDR* nmhdr = (NMHDR*) lParam;
    NM_LISTVIEW* nmLV = (NM_LISTVIEW*) nmhdr;
    wxListEvent event(wxEVT_NULL, m_windowId);
    wxEventType eventType = wxEVT_NULL;
    event.m_item.m_data = nmLV->lParam;
    bool sendEvent = false;
    bool retValue = false;

    switch (nmhdr->code)
    {
    case LVN_ITEMCHANGED:

        // Uncheck item
        if (((nmLV->uOldState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2))
            && ((nmLV->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(1))
            && !myDisableCheckEvents)
        {
            eventType = wxEVT_COMMAND_LIST_ITEM_UNCHECKED;
            event.m_itemIndex = nmLV->iItem;
            event.m_item.m_text = GetItemText(nmLV->iItem);
            event.m_item.m_data = GetItemData(nmLV->iItem);
            sendEvent = true;
        }
        // Check item
        else if (((nmLV->uOldState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(1))
                 && ((nmLV->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2))
                 && !myDisableCheckEvents)
        {
            eventType = wxEVT_COMMAND_LIST_ITEM_CHECKED;
            event.m_itemIndex = nmLV->iItem;
            event.m_item.m_text = GetItemText(nmLV->iItem);
            event.m_item.m_data = GetItemData(nmLV->iItem);
            sendEvent = true;
        }
        retValue = wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
        break;

    case LVN_GETDISPINFO:
        if (IsVirtual())
        {
            LV_DISPINFO* info = (LV_DISPINFO*) lParam;

            LV_ITEM& lvi = info->item;

            if (lvi.mask & LVIF_STATE)
            {
                // Pass to wxWindows handler without LVIF_STATE flag
                lvi.mask &= ~LVIF_STATE; 
                retValue = wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
                lvi.mask |= LVIF_STATE; 
                if (!lvi.iSubItem)
                    lvi.state = OnGetItemState(lvi.iItem);
            }
            else
                retValue = wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
        }
        else
            retValue = wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
        break;

    case NM_CUSTOMDRAW:
    {
        int res = OnCustomDraw(lParam);
        if (res >= 0)
        {
            *result = res;
            return TRUE;
        }
        return wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
    }

    default:
        return wxListCtrl::MSWOnNotify(idCtrl, lParam, result);
    }

    if (!sendEvent)
        return retValue;

    event.SetEventObject( this );
    event.SetEventType(eventType);

    if (!GetEventHandler()->ProcessEvent(event))
        return FALSE;

    *result = !event.IsAllowed();

    return true;
}

int ExtListCtrl::OnCustomDraw(WXLPARAM lParam)
{
   return -1;
}

void ExtListCtrl::OnKeyDown(wxKeyEvent& e)
{
   // Ctrl-A selects all items
   if (e.ControlDown() && (e.GetKeyCode() == 'A'))
   {
      int nItems = GetItemCount();

      for (int i = 0; i < nItems; i++)
      {
         SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      }
   }
   // Space toggles active items
   else if (!e.ControlDown() && (e.GetKeyCode() == ' '))
   {
      int nItems = GetItemCount();
      bool bStateDetermined = false;
      bool bNewState = false;

      // Disable check events (we want to send one single event instead)
      myDisableCheckEvents = true;
      for (int i = 0; i < nItems; i++)
      {
         // Process selected items
         if (GetItemState(i, wxLIST_STATE_SELECTED) & wxLIST_STATE_SELECTED)
         {
            // Determine new state: Contrary of first item 
            if (!bStateDetermined)
            {
               bNewState = !ListView_GetCheckState(GetHwnd(), i);
               bStateDetermined = true;
            }
            ListView_SetCheckState(GetHwnd(), i, bNewState);
         }
      }
      // Create and send the event
      wxListEvent event(wxEVT_NULL, m_windowId);
      event.SetEventObject(this);
      event.m_itemIndex = -1;
      
      if (bNewState == true)
         event.SetEventType(wxEVT_COMMAND_LIST_ITEM_CHECKED);
      else
         event.SetEventType(wxEVT_COMMAND_LIST_ITEM_UNCHECKED);
      GetEventHandler()->ProcessEvent(event);

      // Enable check events 
      myDisableCheckEvents = false;
   }
   else
   {
      e.Skip();
   }
}


void ExtListCtrl::OnLeftClick(wxCommandEvent& WXUNUSED(event))
{
   if (IsVirtual() && HasCheckBoxes())
   {
      // Get cursor position
      wxPoint point = wxGetMousePosition();
      point = ScreenToClient(point);
      int flags;
      int item = HitTest(point, flags);
      if ((flags & wxLIST_HITTEST_ONITEM) == wxLIST_HITTEST_ONITEMSTATEICON)
      {
         wxListEvent event(wxEVT_NULL, m_windowId);
         event.SetEventObject(this);
         event.m_itemIndex = item;
         bool bNewState = !ListView_GetCheckState(GetHwnd(), item);
         if (bNewState == true)
            event.SetEventType(wxEVT_COMMAND_LIST_ITEM_CHECKED);
         else
            event.SetEventType(wxEVT_COMMAND_LIST_ITEM_UNCHECKED);
   
         GetEventHandler()->ProcessEvent(event);
      }
   }

}


bool ExtListCtrl::IsChecked(int index)
{
   if (ListView_GetCheckState(GetHwnd(), index))
      return true;
   else
      return false;
}


bool ExtListCtrl::IsSelected(int index)
{
   if (GetItemState(index, wxLIST_STATE_SELECTED) & wxLIST_STATE_SELECTED)
      return true;
   else
      return false;
}


void ExtListCtrl::SetCheckboxes(bool enable)
{
   DWORD dwStyle = ListView_GetExtendedListViewStyle(GetHwnd());
   if (enable)
   {
      dwStyle |= LVS_EX_CHECKBOXES;
   }
   else
   {
      dwStyle &= ~LVS_EX_CHECKBOXES;
   }
   ListView_SetExtendedListViewStyle(GetHwnd(), dwStyle);
   myHasCheckBoxes = enable;
}


void ExtListCtrl::SetChecked(int index, bool check)
{
   ListView_SetCheckState(GetHwnd(), index, check);
}


void ExtListCtrl::SetBestSize(const wxSize max, const wxSize min)
{
   SetBestSize(max.GetWidth(), max.GetHeight(), min.GetWidth(), min.GetHeight());
}


void ExtListCtrl::SetBestSize(int maxWidth, int maxHeight, int WXUNUSED(minWidth), 
                              int WXUNUSED(minHeight))
{
   int i;
   int width = GetSystemMetrics(SM_CXVSCROLL) + 2;
   for(i = 0; i < GetColumnCount(); i++)
   {
       width += GetColumnWidth(i) + 2;
   }
   if (maxWidth >= 0)
      width = std::min(width, maxWidth);
   
   RECT r;
   int height = 0;
   HWND hwndHeader = ListView_GetHeader(GetHwnd());
   GetWindowRect(hwndHeader, &r);
   height = (r.bottom - r.top) + 8 + GetSystemMetrics(SM_CYHSCROLL);

   if (GetItemCount() > 0)
   {
      ListView_GetItemRect(GetHwnd(), 0, &r, LVIR_ICON);
      height += (r.bottom - r.top) * GetItemCount();
   }

   if (maxHeight >= 0)
      height = std::min(height, maxHeight);

   SetSize(width, height);
}


void ExtListCtrl::SetBestColumnWidth(int index)
{
   int w1, w2;
   SetColumnWidth(index, wxLIST_AUTOSIZE_USEHEADER);
   w1 = GetColumnWidth(index);
   SetColumnWidth(index, wxLIST_AUTOSIZE);
   w2 = GetColumnWidth(index);
   if (w1 > w2)
   {
      SetColumnWidth(index, w1);
   }
}



wxPoint ExtListCtrl::GetContextMenuPos(long item)
{
   wxRect rect;
   wxPoint result(0, 0);
   if (item >= 0 && GetItemRect(item, rect))
   {
      int d = rect.GetHeight() / 2;
      result.x = rect.GetLeft() + d;
      result.y = rect.GetTop() + d;
   }
   return result;
}


void ExtListCtrl::SetSortIndicator(int column, bool sortAsc)
{
   if (myHeaderCtrl)
   {
       RemoveChild(myHeaderCtrl);
       myHeaderCtrl->UnsubclassWin();
       myHeaderCtrl->SetHWND(0);
       delete myHeaderCtrl;
   }

   // XP's listview can draw a sort indicator by itself
   if (WindowsVersionIsXPOrHigher())
   {
      HWND hWnd = ListView_GetHeader(GetHwnd());
      int c = Header_GetItemCount((HWND) hWnd);
      int i;
      for (i = 0; i < c; i++)
      {
         HDITEM hditem;
         memset(&hditem, 0, sizeof(hditem));
         hditem.mask = HDI_FORMAT;
         Header_GetItem((HWND) hWnd, i, &hditem);
         hditem.fmt &= ~HDF_SORTUP;
         hditem.fmt &= ~HDF_SORTDOWN;
         if (i == column)
         {
            if (sortAsc)
               hditem.fmt |= HDF_SORTUP;
            else
               hditem.fmt |= HDF_SORTDOWN;
         }
         Header_SetItem((HWND) hWnd, i, &hditem);
      }
   }
   // for older Windows versions, we have to draw the sort indicator manually
   else
   {
      WXHWND hWnd = (WXHWND) ListView_GetHeader(GetHwnd());
      wxClassInfo* headerControlClass = CLASSINFO(ExtHeaderCtrl);
      myHeaderCtrl = (ExtHeaderCtrl*) headerControlClass->CreateObject();
      myHeaderCtrl->SetHWND(hWnd);
      myHeaderCtrl->SubclassWin(hWnd);
      myHeaderCtrl->SetParent(this);
      myHeaderCtrl->SetId(GetDlgCtrlID((HWND) hWnd));
      myHeaderCtrl->SetSortIndicator(column, sortAsc);
      AddChild(myHeaderCtrl);


      // Enable custom draw
      int c = Header_GetItemCount((HWND) hWnd);
      int i;
      for (i = 0; i < c; i++)
      {
         HDITEM hditem;
         memset(&hditem, 0, sizeof(hditem));
         hditem.mask = HDI_FORMAT;
         Header_GetItem((HWND) hWnd, i, &hditem);
         hditem.fmt |= HDF_OWNERDRAW;
         Header_SetItem((HWND) hWnd, i, &hditem);
      }
   }
}



// Show focus rect
void ExtListCtrl::ShowFocusRect()
{
   if (GetItemCount() > 0)
   {
      SetItemState(0, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
   }
}



int ExtListCtrl::OnGetItemState(long WXUNUSED(item)) const
{
   return 0;
}


int ExtListCtrl::OnGetItemImage(long WXUNUSED(item)) const
{
   return 0;
}



//////////////////////////
// ExtHeaderCtrl


// Constructor
ExtHeaderCtrl::ExtHeaderCtrl()
   : wxControl()
{
   mySortColumn = -1;
   mySortAsc = true;
}


bool ExtHeaderCtrl::MSWOnDraw(WXDRAWITEMSTRUCT *item)
{
   DRAWITEMSTRUCT* lpDrawItemStruct = (DRAWITEMSTRUCT *) item;

   HDC hdc = lpDrawItemStruct->hDC;

   // Get the column rect
   RECT rcLabel = lpDrawItemStruct->rcItem;

   // Save DC
   int nSavedDC = SaveDC(hdc);

   // Set clipping region to limit drawing within column
   HRGN rgn = CreateRectRgnIndirect(&rcLabel);
   SelectObject(hdc, rgn);
   DeleteObject(rgn);

   // Draw the background
   FillRect(hdc, &rcLabel, GetSysColorBrush(COLOR_3DFACE));

   // Labels are offset by a certain amount  
   // This offset is related to the width of a space character
   SIZE size;
   GetTextExtentPoint(hdc, _T(" "), 1, &size);
   int offset = size.cx * 2;


   // Get the column text and format
   TCHAR buf[256];
   HD_ITEM hditem;

   hditem.mask = HDI_TEXT | HDI_FORMAT;
   hditem.pszText = buf;
   hditem.cchTextMax = 255;

   Header_GetItem(GetHwnd(), lpDrawItemStruct->itemID, &hditem);

   // Determine format for drawing column label
   UINT uFormat = DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER;

   if (hditem.fmt & HDF_CENTER)
      uFormat |= DT_CENTER;
   else if (hditem.fmt & HDF_RIGHT)
      uFormat |= DT_RIGHT;
   else
      uFormat |= DT_LEFT;

   // Adjust the rect if the mouse button is pressed on it
   if (lpDrawItemStruct->itemState == ODS_SELECTED)
   {
      rcLabel.left++;
      rcLabel.top += 2;
      rcLabel.right++;
   }

   // Adjust the rect further if Sort arrow is to be displayed
   if (lpDrawItemStruct->itemID == (UINT) mySortColumn)
   {
      rcLabel.right -= 2 * offset - 1;
   }

   rcLabel.left += offset;
   rcLabel.right -= offset;

   // Draw column label
   int textWidth = 0;
   bool hasEllipsis = true;

   if (rcLabel.left > rcLabel.right)
   {
      rcLabel.right = rcLabel.left;
   }

   RECT rc = rcLabel;
   DrawText(hdc, buf, -1, &rc, uFormat | DT_CALCRECT);
   int w = rc.right - rc.left;
   rc = rcLabel;
   DrawText(hdc, buf, -1, &rc, uFormat | DT_CALCRECT | DT_END_ELLIPSIS | DT_MODIFYSTRING);
   int w1 = rc.right - rc.left;
   hasEllipsis = (w1 != w);
   if (lpDrawItemStruct->itemID == (UINT) mySortColumn)
   {
      rcLabel.right = lpDrawItemStruct->rcItem.right - 2 * offset;
      DrawText(hdc, buf, -1, &rcLabel, uFormat);
   }
   else
   {
      rcLabel.right = lpDrawItemStruct->rcItem.right;
      DrawText(hdc, buf, -1, &rcLabel, uFormat);
   }
   textWidth = w;

   // Draw the Sort arrow
   if (lpDrawItemStruct->itemID == (UINT) mySortColumn)
   {
      RECT rcIcon = lpDrawItemStruct->rcItem;
      rcIcon.right = rcIcon.left + 4 * offset + textWidth - 1;
      if (rcIcon.right > lpDrawItemStruct->rcItem.right)
      {
         rcIcon.right = lpDrawItemStruct->rcItem.right;
         rcIcon.left = rcIcon.right - 2 * offset;
         if (rcIcon.left < lpDrawItemStruct->rcItem.left) 
         {
            rcIcon.left = lpDrawItemStruct->rcItem.left;
            rcIcon.right = rcIcon.left + 2 * offset;
         }
      }
       if (lpDrawItemStruct->itemState == ODS_SELECTED)
       {
          rcIcon.left++;
          rcIcon.right++;
       }

      // Set up pens to use for drawing the triangle
      HPEN penLight = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
      HPEN penShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
      HPEN pOldPen = (HPEN) SelectObject(hdc, penLight);

      if (mySortAsc)
      {
         // Draw triangle pointing upwards
         MoveToEx(hdc, rcIcon.right - offset, offset - 1, 0);
         LineTo(hdc, rcIcon.right - offset / 2, rcIcon.bottom - offset);
         LineTo(hdc, rcIcon.right - 3 * offset / 2 - 2, rcIcon.bottom - offset);
         MoveToEx(hdc, rcIcon.right - 3 * offset / 2 - 1, rcIcon.bottom - offset - 1, 0);

         SelectObject(hdc, penShadow);
         LineTo(hdc, rcIcon.right - offset, offset - 2);
      }
      else
      {
         // Draw triangle pointing downwords
         MoveToEx(hdc, rcIcon.right - offset / 2, offset - 1, 0);
         LineTo(hdc, rcIcon.right - offset - 1, rcIcon.bottom - offset + 1);
         MoveToEx(hdc, rcIcon.right - offset - 1, rcIcon.bottom - offset, 0);

         SelectObject(hdc, penShadow);
         LineTo(hdc, rcIcon.right - 3 * offset / 2 - 1, offset - 1);
         LineTo(hdc, rcIcon.right - offset / 2, offset - 1);
      }

      // Restore the pen
      SelectObject(hdc, pOldPen);

      // Delete pens
      DeleteObject(penLight);
      DeleteObject(penShadow);
   }

   // Restore dc
   RestoreDC(hdc, nSavedDC);

   return true;
}


// Set sort indicator
void ExtHeaderCtrl::SetSortIndicator(int column, bool sortAsc)
{
   mySortColumn = column;
   mySortAsc = sortAsc;
}


