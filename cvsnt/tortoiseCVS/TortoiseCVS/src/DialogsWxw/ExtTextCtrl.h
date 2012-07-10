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

#ifndef EXT_TEXTCTRL_H
#define EXT_TEXTCTRL_H


#include <wx/wx.h>
#include <windows.h>



class ExtTextCtrl : public wxTextCtrl
{
public:
   class KeyHandler
   {
   public:
      virtual bool OnArrowUpDown(bool up) = 0;
      virtual bool OnSpace() = 0;
   };
   
   // Constructor
   ExtTextCtrl(wxWindow *parent, wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxTextCtrlNameStr);
              
   // Destructor
   ~ExtTextCtrl();

   // Window procedure
   virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);

   // Process WM_NOTIFY
   bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM* result);

   // Request size
   wxSize RequestSize() const;

   // Get text size
   wxSize GetTextSize() const;

   // Set plaintext mode
   void SetPlainText(bool bPlainText);

   // Get plaintext mode
   bool GetPlainText() const;

   // Set single codepage
   void SetSingleCodepage(bool bSingleCodepage);

   // Get single codepage
   bool GetSingleCodepage() const;

   // Get range
   wxString GetRange(long from, long to) const;

   // Set key handler instance
   void SetKeyHandler(KeyHandler* handler);

protected:
   virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;
   
private:
   // requested size
   wxSize m_ReqSize;

   // disable formatting
   bool m_PlainText;

   // Use single codepage only
   bool m_SingleCodepage;

   // Set textmode
   void SetTextMode();

   KeyHandler*  myKeyHandler;
};


#endif
