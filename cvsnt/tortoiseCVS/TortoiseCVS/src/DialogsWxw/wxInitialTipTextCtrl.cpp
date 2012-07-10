/////////////////////////////////////////////////////////////////////////////
// Name:        wxInitialTipTextCtrl.cpp
// Purpose:     A text control with an embedded blue tip
//               which goes away when you click on it
// Author:      Francis Irving
// Modified by:
// RCS-ID:      $Id: wxInitialTipTextCtrl.cpp,v 1.1 2012/03/04 01:06:37 aliot Exp $
// Copyright:   (c) Francis Irving
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "wxInitialTipTextCtrl.h"
#include "WxwHelpers.h"
#include "../Utils/TortoiseRegistry.h"
#include <wx/defs.h>


BEGIN_EVENT_TABLE(wxInitialTipTextCtrl, wxTextCtrl)
   EVT_SET_FOCUS(wxInitialTipTextCtrl::OnFocusGot)
   EVT_TEXT(-1, wxInitialTipTextCtrl::OnTextChanged)
END_EVENT_TABLE()

wxInitialTipTextCtrl::wxInitialTipTextCtrl(wxWindow* parent, wxWindowID id)
   : wxTextCtrl(parent, id, wxT("")),
     myInitialTipShowing(false),
     myDuringInitialTipChange(false)
{
   SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
}

void wxInitialTipTextCtrl::SetInitialTip(const wxString& initialTipText)
{
   // myDuringInitialTipChange is a hack to stop EVT_TEXT events
   // getting called when we are merely setting the initial tip text - that
   // text won't be relevant to any dialog hooking that command event.
   myDuringInitialTipChange = true; 

   SetValue(initialTipText);
   myInitialTipShowing = true;

   int fgColour = RGB(0x00, 0x00, 0xFF);
   TortoiseRegistry::ReadInteger("Colour Tip Text", fgColour);
   SetForegroundColour(ColorRefToWxColour(fgColour));
   myDuringInitialTipChange = false;
}

void wxInitialTipTextCtrl::OnTextChanged(wxCommandEvent& event)
{
   if (myDuringInitialTipChange)
      return;
   event.Skip();
}

void wxInitialTipTextCtrl::ClearInitialTip()
{
   if (myInitialTipShowing)
   {
      myInitialTipShowing = false;
      SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

      // again, we call SetValue when we aren't really changing the value of 
      // the control
      myDuringInitialTipChange = true; 
      SetValue(wxT(""));
      myDuringInitialTipChange = false;
   }
}

void wxInitialTipTextCtrl::SetValue(const wxString& value)
{
   ClearInitialTip();
   wxTextCtrl::SetValue(value);
}

wxString wxInitialTipTextCtrl::GetValue() const
{
   if (myInitialTipShowing)
      return wxT("");
   return wxTextCtrl::GetValue();
}

void wxInitialTipTextCtrl::OnFocusGot(wxFocusEvent& WXUNUSED(event))
{
   ClearInitialTip();
}
