/////////////////////////////////////////////////////////////////////////////
// Name:        wxInitialTipTextCtrl.h
// Purpose:     A text control with an embedded blue tip
//					  which goes away when you click on it
// Author:      Francis Irving
// Modified by:
// RCS-ID:      $Id: wxInitialTipTextCtrl.h,v 1.1 2012/03/04 01:06:37 aliot Exp $
// Copyright:   (c) Francis Irving
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

#ifndef INITIAL_TIP_TEXT_CTRL
#define INITIAL_TIP_TEXT_CTRL

#include <wx/wx.h>

class wxInitialTipTextCtrl : public wxTextCtrl
{
public:
	wxInitialTipTextCtrl(wxWindow* parent, wxWindowID id);

	void SetInitialTip(const wxString& initialTipText);
	void ClearInitialTip();

	virtual void SetValue(const wxString& value);
    virtual wxString GetValue() const;

protected:
    void OnFocusGot(wxFocusEvent& event);
	void OnTextChanged(wxCommandEvent& event);

	bool myInitialTipShowing;
	bool myDuringInitialTipChange;

private:
	DECLARE_EVENT_TABLE()
};

#endif
