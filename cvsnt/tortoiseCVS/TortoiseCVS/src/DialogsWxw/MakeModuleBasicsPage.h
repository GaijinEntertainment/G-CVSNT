// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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

#ifndef MAKEMODULE_BASICS_PAGE_H
#define MAKEMODULE_BASICS_PAGE_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/valtext.h>

#include <string>
#include <vector>

#include "../CVSGlue/CVSRoot.h"
#include "RevOptions.h"

#define MB_CHECKOUT_MODULE 0
#define MB_MAKE_MODULE 1

class wxInitialTipTextCtrl;
class CvsRootListCtrl;

// This class implements the layout for the first page
// of the checkout dialog.

class MakeModuleBasicsPage : public wxPanel
{
public:
   MakeModuleBasicsPage(wxWindow* parent);

   // For make new module (module is already choosen by dir name)
   void SetModule(const std::string& module);
   
   CVSRoot GetCVSRoot() const { return myCVSRoot; }

   void SetCVSRoot(const CVSRoot& root);

   std::string GetModule() const { return myModule; }
   
   void OKClicked();

   // Annoyingly: To ghost the OK button of the dialog above, 
   // it needs to have been attached so we can't do it in this class's
   // constructor.  Call this after attaching the page to the dialog;
   void UpdateSensitivity();
   
private:
   wxChoice* myProtocolMenu;
   wxComboBox* myProtocolParametersCombo;
   wxComboBox* myUsernameCombo;
   wxComboBox* myServerCombo;
   wxComboBox* myPortCombo;
   wxComboBox* myDirectoryCombo;
   wxComboBox* myModuleCombo;
   wxButton* myFetchModuleList;
   wxTextValidator m_PortValidator;
   wxInitialTipTextCtrl* myCVSROOTText;
   CvsRootListCtrl* myCVSROOTList;
   
   CVSRoot myCVSRoot;
   std::string myModule;
   
   void ProtocolChanged(wxCommandEvent& event);
   void ModuleChanged(wxCommandEvent& event);
   void RootChanged(wxCommandEvent& event);
   void FetchModuleList(wxCommandEvent& event);
   void ListSelected(wxCommandEvent& event);

   void HistoryRemove(wxCommandEvent& event);
   void HistoryRemoveAll(wxCommandEvent& event);

   void SelectInListBox();
   wxString ListSelectedText();
   
   void FillInModuleList(std::vector<std::string>& modules);
   void FetchCachedModuleList();
   void UpdateCVSRoot(wxWindow* source);
   void UpdateModule(wxWindow* source);

   bool m_Updating;
   
   DECLARE_EVENT_TABLE()
};

#endif // MAKEMODULE_BASICS_PAGE_H


