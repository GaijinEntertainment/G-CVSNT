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

#ifndef MODULE_BASICS_PAGE_H
#define MODULE_BASICS_PAGE_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#include <wx/valtext.h>

#include <string>
#include <vector>

#include "../CVSGlue/CVSRoot.h"
#include "RevOptions.h"
#include "ExtSplitterWindow.h"
#include "../Utils/RepoUtils.h"
#include "../cvstree/common.h"
#include "ModuleBasicsPageEventHandler.h"

#define MB_CHECKOUT_MODULE 0
#define MB_MAKE_MODULE 1

class CVSRepoData;
class CVSRepoDataSortCriteria;
class wxInitialTipTextCtrl;
class CvsRootListCtrl;
class RepoTreeCtrl;
class ExtTextCtrl;

// This class implements the layout for the first page
// of the checkout dialog.

class ModuleBasicsPage : public wxPanel, public ModuleBasicsPageEvtHandlerParent
{
public:
    ModuleBasicsPage(wxWindow* parent, int mode, bool fixed = false);
    friend class ModuleBasicsPageEvtHandler;

   // Preset
//   ModuleBasicsPage(wxWindow* parent, int mode, const std::string& server);
   
    // For make new module (module is already choosen by dir name)
    void SetModule(const std::string& module);
   
    CVSRoot GetCVSRoot() const { return myCVSRoot; }

    void SetCVSRoot(const CVSRoot& root);

    std::string GetModule() const { return myModule; }

    bool GetEdit() const { return myEditCheckBox->GetValue() != 0; }

    std::string GetBugNumber() const;
    
    void OKClicked();

   // Set RevOptions pointer
    void SetRevOptions(RevOptions* revOptions);

    const RevOptions* GetRevOptions() const { return myRevOptions; }
   
    // Annoyingly: To ghost the OK button of the dialog above, 
    // it needs to have been attached so we can't do it in this class's
    // constructor.  Call this after attaching the page to the dialog;
    void UpdateSensitivity();
    void SetRepoGraph(wxWindow* parent);

    // Called by CheckoutOptionsPage to enable/disable Edit checkbox
    void ExportEnabled(bool enabled);

    virtual std::string& GetModule();

protected:
   wxWindow*        myParent;
   static wxColour* myModuleColor;
   static wxColour* myDirectoryColor;
   static int       myRefCount;

private:
   void ProcessNode(CRepoNode* node, wxTreeItemId rootId, bool onlychildren);
   void Refresh();
   void RefreshModuleList();
   void EditChanged(wxCommandEvent&);

   // Menus
   
   void OnMenuGet(wxCommandEvent& WXUNUSED(event));

   void OnMenuBrowse(wxCommandEvent& WXUNUSED(event));

   wxChoice*             myProtocolMenu;
   wxComboBox*           myProtocolParametersCombo;
   wxComboBox*           myUsernameCombo;
   wxComboBox*           myServerCombo;
   wxComboBox*           myPortCombo;
   wxComboBox*           myDirectoryCombo;
   wxComboBox*           myModuleCombo;
   wxCheckBox*           myUseBugCheckBox;
   wxCheckBox*           myEditCheckBox;
   ExtTextCtrl*          myBugNumber;
   wxButton*             myFetchModuleList;
   wxTextValidator       myPortValidator;
   wxInitialTipTextCtrl* myCVSROOTText;
   CvsRootListCtrl*      myCVSROOTList;
   RepoTreeCtrl*         myTreeCtrl;
   ExtSplitterWindow*    mySplitter;
   wxImageList*          myImageList;
   
   std::vector<int>      myColumnWidths;
   CRepoNode*            myTree;
   int                   mySortColumn;
   RepoParserData        myRepoParserData;
   
   CVSRoot               myCVSRoot;
   std::string           myModule;
   int                   myMode;
   RevOptions*           myRevOptions;
   bool                  myFixed;
   
   void ProtocolChanged(wxCommandEvent& event);
   void ModuleChanged(wxCommandEvent& event);
   void RootChanged(wxCommandEvent& event);
   void FetchModuleList(wxCommandEvent& event);
   void ListSelected(wxCommandEvent& event);

   void HistoryRemove(wxCommandEvent& event);
   void HistoryRemoveAll(wxCommandEvent& event);

   void SelectInListBox();
private:
   wxString ListSelectedText();
   wxString ListSelectedModule();
   
   void FillInModuleList(std::vector<std::string>& modules);
   void UpdateRevOptions();
   void FetchCachedModuleList();
   void UpdateCVSRoot(wxWindow* source);
   void UpdateModule(wxWindow* source);

    virtual class RepoTreeCtrl* GetTreeCtrl();

    virtual wxComboBox* GetModuleCombo();

    virtual void UpdateModule();

    virtual void GoBrowse(const wxTreeItemId& openingItem);

    virtual int& GetSortColumn();

    virtual void SortItems();

    bool myUpdating;
   
    friend class RepoTreeCtrl;

    DECLARE_EVENT_TABLE()
};

#endif // MODULE_BASICS_PAGE_H


