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
#include "ExtTextCtrl.h"
#include <wx/msw/private.h>
#include <map>
#include <richedit.h>
#include <wx/textfile.h>

// Texts used by wxWindows in wxTextCtrl's context menu
#define WXW_TEXTS _("&Undo") _("&Redo") _("Cu&t") _("&Copy") _("&Paste") _("&Delete") _("Select &All")


ExtTextCtrl::ExtTextCtrl(wxWindow *parent, wxWindowID id,
                         const wxString& value,
                         const wxPoint& pos,
                         const wxSize& size,
                         long style,
                         const wxValidator& validator,
                         const wxString& name)
   : wxTextCtrl(parent, id, value, pos, size, style, validator, name),
     m_PlainText(false), m_SingleCodepage(false), myKeyHandler(0)
{
   // Set default char format
   CHARFORMAT charformat;
   charformat.cbSize = sizeof(charformat);
   charformat.dwMask = CFM_CHARSET;
   charformat.bCharSet = DEFAULT_CHARSET;

   SendMessage(GetHwnd(), EM_SETCHARFORMAT, 0, (LPARAM) &charformat);
}


ExtTextCtrl::~ExtTextCtrl()
{
   delete myKeyHandler;
}

void ExtTextCtrl::SetKeyHandler(KeyHandler* handler)
{
   myKeyHandler = handler;
}

// Window procedure
WXLRESULT ExtTextCtrl::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
   // if we're richtext and read-only, block ctrl-l, -e, -r key strokes
   if (IsRich())
   {
      if (nMsg == WM_KEYDOWN)
      {
         char c = LOWORD(MapVirtualKey(wParam, 2));
         if (c == 'R' || c == 'L' || c == 'E')
         {
            if (GetKeyState(VK_CONTROL) < 0)
               return 0;
         }
         if (GetKeyState(VK_CONTROL) < 0)
         {
            if (wParam == VK_UP)
            {
               return myKeyHandler ? myKeyHandler->OnArrowUpDown(true) : 0;
            }
            else if (wParam == VK_DOWN)
            {
               return myKeyHandler ? myKeyHandler->OnArrowUpDown(false) : 0;
            }
         }
         else if (wParam == ' ')
            if (myKeyHandler)
               return myKeyHandler->OnSpace();
      }
   }

   if (!IsRich())
   {
      if (nMsg == WM_CHAR)
      {
         if (wParam == 1)
         {
            SetSelection(-1, -1);
            return 0;
         }
      }
   }

   // inherited behaviour
   return wxTextCtrl::MSWWindowProc(nMsg, wParam, lParam);
}


// Process WM_NOTIFY
bool ExtTextCtrl::MSWOnNotify(int idCtr,
                              WXLPARAM lParam,
                              WXLPARAM* result)
{
   // Handle EN_REQUESTRESIZE
   REQRESIZE *req = (REQRESIZE *) lParam;
   if (req->nmhdr.code == EN_REQUESTRESIZE)
   {
      m_ReqSize.SetWidth(req->rc.right - req->rc.left);
      m_ReqSize.SetHeight(req->rc.bottom - req->rc.top);
   }

   return wxTextCtrl::MSWOnNotify(idCtr, lParam, result);
}


// Request size
wxSize ExtTextCtrl::RequestSize() const
{
   // Get event mask
   int mask = SendMessage(GetHwnd(), EM_GETEVENTMASK, 0, 0);

   // Enable resize requests
   if ((mask & ENM_REQUESTRESIZE) == 0)
   {
      SendMessage(GetHwnd(), EM_SETEVENTMASK, 0, mask | ENM_REQUESTRESIZE);
   }

   // request size
   SendMessage(GetHwnd(), EM_REQUESTRESIZE, 0, 0);

   // Disable resize requests
   if ((mask & ENM_REQUESTRESIZE) == 0)
   {
      SendMessage(GetHwnd(), EM_SETEVENTMASK, 0, mask);
   }

   return m_ReqSize;
}



// Get text size
wxSize ExtTextCtrl::GetTextSize() const
{
    wxString text(this->GetValue());

    int widthTextMax = 0, heightTextTotal = 0, heightLineDefault = 0;
    int heightLine = 0;
    bool lastWasAmpersand = false;

    wxString curLine;
    for (const wxChar* pc = text; ; pc++)
    {
        if (*pc == '\n' || *pc == '\0')
        {
         if ( !curLine )
         {
            // we can't use GetTextExtent - it will return 0 for both width
            // and height and an empty line should count in height
            // calculation
            if (!heightLineDefault )
               heightLineDefault = heightLine;
            if (!heightLineDefault)
               GetTextExtent(wxT("W"), 0, &heightLineDefault);

            heightTextTotal += heightLineDefault;
         }
         else
         {
             int widthLine;
             GetTextExtent(curLine, &widthLine, &heightLine);
             if (widthLine > widthTextMax)
                 widthTextMax = widthLine;
             heightTextTotal += heightLine;
         }

         if (*pc == '\n')
         {
            curLine.Empty();
         }
         else
         {
            // the end of string
            break;
         }
      }
      else
      {
         // we shouldn't take into account the '&' which just introduces the
         // mnemonic characters and so are not shown on the screen -- except
         // when it is preceded by another '&' in which case it stands for a
         // literal ampersand
         if (*pc == '&')
         {
            if (!lastWasAmpersand)
            {
               lastWasAmpersand = true;

               // skip the statement adding pc to curLine below
               continue;
            }

            // it is a literal ampersand
            lastWasAmpersand = false;
         }
         curLine += *pc;
      }
   }

   return wxSize(widthTextMax, heightTextTotal);
}



WXDWORD ExtTextCtrl::MSWGetStyle(long flags, WXDWORD *exstyle) const
{
   return wxTextCtrl::MSWGetStyle(flags, exstyle) | WS_TABSTOP;
}


// Set plaintext mode
void ExtTextCtrl::SetPlainText(bool bPlainText)
{
   m_PlainText = bPlainText;
   SetTextMode();
}


// Get plaintext mode
bool ExtTextCtrl::GetPlainText() const
{
   return m_PlainText;
}


// Set single codepage
void ExtTextCtrl::SetSingleCodepage(bool bSingleCodepage)
{
   m_SingleCodepage = bSingleCodepage;
   SetTextMode();
}


// Get single codepage
bool ExtTextCtrl::GetSingleCodepage() const
{
   return m_SingleCodepage;
}


// Set textmode
void ExtTextCtrl::SetTextMode()
{
   wxString s = GetValue();
   SetValue(wxT(""));
   WPARAM mode = 0;
   if (m_PlainText)
   {
      mode |= TM_PLAINTEXT;
   }
   else
   {
      mode |= TM_RICHTEXT;
   }
   if (m_SingleCodepage)
   {
      mode |= TM_SINGLECODEPAGE;
   }
   else
   {
      mode |= TM_MULTICODEPAGE;
   }

   SendMessage(GetHwnd(), EM_SETTEXTMODE, mode, 0);
   SetValue(s);
}


// Get range
wxString ExtTextCtrl::GetRange(long from, long to) const
{
    wxString str;

    if ( from >= to && to != -1 )
    {
        // nothing to retrieve
        return str;
    }
    // retrieve all text
    str = wxGetWindowText(GetHWND());

    // need only a range?
    if ( from < to )
    {
        str = str.Mid(from, to - from);
    }

    // WM_GETTEXT uses standard DOS CR+LF (\r\n) convention - convert to the
    // canonical one (same one as above) for consistency with the other kinds
    // of controls and, more importantly, with the other ports
    str = wxTextFile::Translate(str, wxTextFileType_Unix);

    return str;
}
