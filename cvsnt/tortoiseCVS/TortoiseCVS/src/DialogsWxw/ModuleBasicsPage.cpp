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

#include "StdAfx.h"

#include <algorithm>
#include <memory>

#include <wx/statline.h>

#include "CvsRootListCtrl.h"
#include "ExtComboBox.h"
#include "ExtListCtrl.h"
#include "ExtTextCtrl.h"
#include "MessageDialog.h"
#include "ModuleBasicsPage.h"
#include "RepoTreeCtrl.h"
#include "WxwHelpers.h"
#include "WxwMain.h"

#include "wxInitialTipTextCtrl.h"

#include <Utils/PathUtils.h>
#include <Utils/ProcessUtils.h>
#include <Utils/RepoUtils.h>
#include <Utils/ShellUtils.h>
#include <Utils/TimeUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/Translate.h>
#include <CVSGlue/RemoteLists.h>

static const int ICONSIZE = 12;

enum
{
   MODBASICS_CONTEXT_ID_GET = 10001,
   MODBASICS_CONTEXT_ID_BROWSE,
   MODBASICS_ID_PROTOCOL,
   MODBASICS_ID_PROTPARMS,
   MODBASICS_ID_SERVER,
   MODBASICS_ID_PORT,
   MODBASICS_ID_DIRECTORY,
   MODBASICS_ID_MODULE,
   MODBASICS_ID_USERNAME,
   MODBASICS_ID_FETCHMODULELIST,
   MODBASICS_ID_CVSROOTTEXT,
   MODBASICS_ID_CVSROOTLIST,
   MODBASICS_ID_EDIT,

   MB_REMOVE,
   MB_REMOVE_ALL
};

static const int REPO_DEF_COLUMN_SIZE = 100;
static const int REPO_MAX_COLUMN_SIZE = 2000;

enum Protocol
{
    ProtPserver = 0,
    ProtSspi,
#ifndef MARCH_HARE_BUILD
    ProtExt,
#endif
    ProtSsh,
    ProtServer,
    ProtLocal,
    ProtGserver,
    ProtSserver
};

#define MODBASICS_MODULE_RGB            0xFF,0xFF,0xD0
#define MODBASICS_DIRECTORY_RGB         0xE0,0xFF,0xE0

int ModuleBasicsPage::myRefCount = 0;
wxColour* ModuleBasicsPage::myModuleColor = 0;
wxColour* ModuleBasicsPage::myDirectoryColor = 0;

enum
{
   IconFile,            // Image list offset 0
   IconDirectory,       // Image list offset 1
   IconModule           // Image list offset 2
};


BEGIN_EVENT_TABLE(ModuleBasicsPage, wxPanel)
   EVT_COMMAND(MODBASICS_ID_PROTOCOL,   wxEVT_COMMAND_CHOICE_SELECTED,          ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PROTPARMS,  wxEVT_COMMAND_TEXT_UPDATED,             ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PROTPARMS,  wxEVT_COMMAND_COMBOBOX_SELECTED,        ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_SERVER,     wxEVT_COMMAND_TEXT_UPDATED,             ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_SERVER,     wxEVT_COMMAND_COMBOBOX_SELECTED,        ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PORT,       wxEVT_COMMAND_TEXT_UPDATED,             ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PORT,       wxEVT_COMMAND_COMBOBOX_SELECTED,        ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_DIRECTORY,  wxEVT_COMMAND_TEXT_UPDATED,             ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_DIRECTORY,  wxEVT_COMMAND_COMBOBOX_SELECTED,        ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_MODULE,     wxEVT_COMMAND_TEXT_UPDATED,             ModuleBasicsPage::ModuleChanged)
   EVT_COMMAND(MODBASICS_ID_MODULE,     wxEVT_COMMAND_COMBOBOX_SELECTED,        ModuleBasicsPage::ModuleChanged)
   EVT_COMMAND(MODBASICS_ID_USERNAME,   wxEVT_COMMAND_TEXT_UPDATED,             ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_USERNAME,   wxEVT_COMMAND_COMBOBOX_SELECTED,        ModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_FETCHMODULELIST, wxEVT_COMMAND_BUTTON_CLICKED,      ModuleBasicsPage::FetchModuleList)
   EVT_COMMAND(MODBASICS_ID_CVSROOTTEXT, wxEVT_COMMAND_TEXT_UPDATED,            ModuleBasicsPage::RootChanged)
   EVT_COMMAND(MODBASICS_ID_CVSROOTLIST, wxEVT_COMMAND_LIST_ITEM_SELECTED,      ModuleBasicsPage::ListSelected)
   EVT_COMMAND(MODBASICS_ID_EDIT,       wxEVT_COMMAND_CHECKBOX_CLICKED,         ModuleBasicsPage::EditChanged)

   EVT_MENU(MODBASICS_CONTEXT_ID_BROWSE,        ModuleBasicsPage::OnMenuBrowse)
   EVT_MENU(MODBASICS_CONTEXT_ID_GET,           ModuleBasicsPage::OnMenuGet)
END_EVENT_TABLE()


ModuleBasicsPage::ModuleBasicsPage(wxWindow* parent, int mode, bool fixed)
   : wxPanel(parent),
     myParent(parent),
     myFetchModuleList(0),
     myPortValidator(wxFILTER_NUMERIC), 
     myCVSROOTList(0),
     myTree(0),
     mySortColumn(0),
     myMode(mode),
     myRevOptions(0),
     myFixed(fixed),
     myUpdating(false)
{
   // Tooltips
   const wxChar* tip1 = _("How your machine should connect to the CVS repository");
   const wxChar* tip2 = _("Parameters associated with this protocol");
   const wxChar* tip3 = _("The computer that the CVS repository is on");
   const wxChar* tip4 = _("Location of the repository on the server");
   const wxChar* tip5 = _("The user name that CVS should use");
   const wxChar* tip6 = _("The name of the module, or full path of any file or folder in the module");
   const wxChar* tip7 = _("Technical details of the CVSROOT which TortoiseCVS is going to use");
   const wxChar* tip8 = _("Retrieves a list of modules from the server, and puts them in the dropdown box");
   const wxChar* tip9 = _("The TCP port of your CVS server process (optional)");
   const wxChar* tip10 = _("Do a CVS Edit on the file/folder after checkout");
   const wxChar* tip11 = _("Mark edit with bug number");

   // Create splitter
   mySplitter = new ExtSplitterWindow(this, -1, wxDefaultPosition, 
                                      wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
   wxPanel* rightPanel = new wxPanel(mySplitter, -1);
   wxPanel* leftPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxDOUBLE_BORDER | wxTAB_TRAVERSAL);
   mySplitter->SplitVertically(leftPanel, rightPanel, 0);
   mySplitter->SetMinimumPaneSize(wxDLG_UNIT_Y(this, 200));
   mySplitter->SetSize(wxDLG_UNIT(this, wxSize(150, 90)));

   // Default
   myCVSRoot.SetProtocol("pserver");
   
   // Two vertical columns
   wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer* boxSizerCvsroot = new wxBoxSizer(wxHORIZONTAL);
   wxBoxSizer* boxSizerModule = new wxBoxSizer(wxHORIZONTAL);
   wxBoxSizer* boxSizerBug = new wxBoxSizer(wxHORIZONTAL);
   wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2);
   gridSizer->AddGrowableCol(1);
   SetAutoLayout(TRUE);
   SetSizer(boxSizer);
   
   // Labels
   wxStaticText* label1 = new wxStaticText(leftPanel, -1, wxString(_("Protocol")) + _(":"));
   wxStaticText* label2 = new wxStaticText(leftPanel, -1, wxString(_("Protocol parameters")) + _(":"));
   wxStaticText* label3 = new wxStaticText(leftPanel, -1, wxString(_("Server")) + _(":"));
   wxStaticText* label4 = new wxStaticText(leftPanel, -1, wxString(_("Repository folder")) + _(":"));
   wxStaticText* label5 = new wxStaticText(leftPanel, -1, wxString(_("User name")) + _(":"));
   wxStaticText* label6 = new wxStaticText(rightPanel, -1, wxString(_("Module")) + _(":"));
   wxStaticText* label7 = new wxStaticText(leftPanel, -1, wxString(_("CVSROOT")) + _(":"));
   wxStaticText* label8 = new wxStaticText(leftPanel, -1, wxString(_("Port")) + _(":"));
   
   // CVSROOT stuff
   if (!fixed)
   {
      myCVSROOTList = new CvsRootListCtrl(leftPanel, MODBASICS_ID_CVSROOTLIST);
      myCVSROOTList->InsertColumn(0, _("Previous CVSROOTs"), wxLIST_FORMAT_LEFT, wxDLG_UNIT_X(leftPanel, 200));
      if (myMode == MB_CHECKOUT_MODULE)
      {
         myCVSROOTList->InsertColumn(1, _("Module"), wxLIST_FORMAT_LEFT);
      }

      // CVSROOT environment variable
      bool envExists = false;
      const char* env = getenv("CVSROOT");
   
      // Fill in CVSROOT history list
      std::vector< std::string > history, historyM;
      TortoiseRegistry::ReadVector("History\\Checkout CVSROOT", history);
      
      unsigned int itemIndex = 0;
      if (myMode == MB_CHECKOUT_MODULE)
      {
         TortoiseRegistry::ReadVector("History\\Checkout CVSROOT Module", historyM);
         for (unsigned int i = 0; i < std::min(history.size(), historyM.size()); ++i)
         {
            if (env && history[i] == env && historyM[i].empty())
               envExists = true;
            myCVSROOTList->InsertItem(itemIndex, wxText(history[i]), 0);
            myCVSROOTList->SetItem(itemIndex++, 1, wxText(historyM[i]), 0);
         }
      }
      else
      {
         // Remove duplicates
         std::sort(history.begin(), history.end());
         history.erase(std::unique(history.begin(), history.end()), history.end());
         for (unsigned int i = 0; i < history.size(); ++i)
         {
            if (env && history[i] == env)
               envExists = true;
            myCVSROOTList->InsertItem(itemIndex, wxText(history[i]), 0);
         }
      }

      if (env && !envExists)
         myCVSROOTList->InsertItem(0, wxText(env), 0);
   
      if (myCVSROOTList->GetItemCount() == 0)
         myCVSROOTList->Show(FALSE);
   
      if (myMode == MB_CHECKOUT_MODULE)
      {
         myCVSROOTList->SetBestColumnWidth(0);
         myCVSROOTList->SetBestColumnWidth(1);
         int colwidth = myCVSROOTList->GetColumnWidth(0) + myCVSROOTList->GetColumnWidth(1);
         myCVSROOTList->SetSize(colwidth + GetSystemMetrics(SM_CXVSCROLL), 
                                wxDLG_UNIT_Y(leftPanel, 60));
      }
      else
      {
         myCVSROOTList->SetBestColumnWidth(0);
         int colwidth = myCVSROOTList->GetColumnWidth(0);
         myCVSROOTList->SetSize(colwidth + GetSystemMetrics(SM_CXVSCROLL), 
                                wxDLG_UNIT_X(leftPanel, 60));
      }
   }
   
   myCVSROOTText = new wxInitialTipTextCtrl(leftPanel, MODBASICS_ID_CVSROOTTEXT);
   boxSizerCvsroot->Add(label7, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   boxSizerCvsroot->Add(myCVSROOTText, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   // Protocols
   wxString protocols[] = 
   {
      _("Password server (:pserver:)"),
      _("Windows authentication (:sspi:)"),
#ifndef MARCH_HARE_BUILD
      _("Secure shell (:ext:)"),
#endif
      _("Secure shell (:ssh:)"),
      _("Remote shell (:server:)"),
#ifndef MARCH_HARE_BUILD
      _("Locally mounted folder (:local:)"),
#endif
      _("Kerberos (:gserver:)"),
      _("SSL (:sserver:)")
   };
   const int protocolCount = sizeof(protocols)/sizeof(wxString);
   myProtocolMenu = new wxChoice(leftPanel, MODBASICS_ID_PROTOCOL,
                                 wxDefaultPosition, wxDefaultSize, protocolCount, protocols);
   gridSizer->Add(label1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myProtocolMenu, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   ReadOptionMenu(myProtocolMenu, "Checkout Protocol Default");

   myProtocolParametersCombo = MakeComboList(leftPanel, MODBASICS_ID_PROTPARMS,
                                             "History\\Checkout Protocol Parameters", "",
                                             COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label2, 0, wxALL| wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myProtocolParametersCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   myServerCombo = MakeComboList(leftPanel, MODBASICS_ID_SERVER,
                                 "History\\Checkout Server", "",
                                 COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label3, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myServerCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   myPortCombo = MakeComboList(leftPanel, MODBASICS_ID_PORT,
                               "History\\Checkout Port", "",
                               COMBO_FLAG_NODEFAULT, 0, &myPortValidator);
   gridSizer->Add(label8, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myPortCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   myDirectoryCombo = MakeComboList(leftPanel, MODBASICS_ID_DIRECTORY,
                                    "History\\Checkout Directory", "",
                                    COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label4, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myDirectoryCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   // Use their login name under Windows as a sensible default
   myUsernameCombo = MakeComboList(leftPanel, MODBASICS_ID_USERNAME,
                                   "History\\Checkout Username", GetUserName(),
                                   COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label5, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myUsernameCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   wxStaticText* label9 = new wxStaticText(leftPanel, -1, wxString(_("Bug number")) + _(":"));
   // Bug box
   wxBoxSizer* bugSizer = new wxBoxSizer(wxHORIZONTAL);
   myEditCheckBox = new wxCheckBox(leftPanel, MODBASICS_ID_EDIT, _("&Edit"));
   myEditCheckBox->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\Checkout\\Edit", false));
   myEditCheckBox->SetToolTip(tip10);
   myUseBugCheckBox = new wxCheckBox(leftPanel, -1, _("&Use Bug"));
   myUseBugCheckBox->SetValue(TortoiseRegistry::ReadBoolean("Dialogs\\Checkout\\Use Bug", false));
   myUseBugCheckBox->SetToolTip(tip11);
   bugSizer->Add(myEditCheckBox, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   bugSizer->Add(myUseBugCheckBox, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   myBugNumber = new ExtTextCtrl(leftPanel, -1);
   myBugNumber->SetPlainText(true);
   myBugNumber->SetMaxLength(100);
   myBugNumber->SetFocus();
   myBugNumber->SetSelection(-1, -1);
   std::string defaultbugnumber = TortoiseRegistry::ReadString("Dialogs\\Checkout\\Bug Number");
   myBugNumber->SetValue(wxTextCStr(defaultbugnumber));
   bugSizer->Add(label9, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   bugSizer->Add(myBugNumber, 0, 0, 0);

   if (!myEditCheckBox->GetValue())
   {
      myBugNumber->Disable();
      myUseBugCheckBox->Disable();
   }

   boxSizerBug->Add(bugSizer, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   // Module box
   wxBoxSizer* moduleSizer = new wxBoxSizer(wxHORIZONTAL);
   myModuleCombo = new ExtComboBox(rightPanel, MODBASICS_ID_MODULE);
   moduleSizer->Add(myModuleCombo, 2, wxALIGN_CENTER_VERTICAL, 0);
   if (!fixed)
   {
      myFetchModuleList = new wxButton(rightPanel, MODBASICS_ID_FETCHMODULELIST, _("&Fetch list..."));
      moduleSizer->Add(myFetchModuleList, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
   }
   boxSizerModule->Add(label6, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   boxSizerModule->Add(moduleSizer, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   myImageList = new wxImageList(ICONSIZE, ICONSIZE);
    
   myImageList->Add(wxIcon(wxT("WXICON_SMALL_FILE"), wxBITMAP_TYPE_ICO_RESOURCE));
   myImageList->Add(wxIcon(wxT("WXICON_SMALL_CLOSED_FOLDER"), wxBITMAP_TYPE_ICO_RESOURCE));
   myImageList->Add(wxIcon(wxT("WXICON_SMALL_DRIVE"), wxBITMAP_TYPE_ICO_RESOURCE));

   // List control with all modules/directories/files    
   myTreeCtrl = new RepoTreeCtrl(rightPanel, MODBASICS_ID_TREECTRL, wxDefaultPosition, 
                                 wxDLG_UNIT(this, wxSize(250, 100)),
                                 wxTR_HIDE_ROOT |
                                 wxTR_HAS_BUTTONS | wxTR_MULTIPLE | wxTR_EXTENDED | wxBORDER_NONE);
   myTreeCtrl->AddColumn(wxString(_("File")));
   myTreeCtrl->AddColumn(wxString(_("Revision")));
   myTreeCtrl->AddColumn(wxString(_("Date")));
   // Image list
   myTreeCtrl->AssignImageList(myImageList);

   // Sizer for right panel
   wxBoxSizer* sizerRightPanel = new wxBoxSizer(wxVERTICAL);
   sizerRightPanel->Add(myTreeCtrl, 3, wxGROW | wxALL, 0);
   // Sizer for left panel
   wxBoxSizer* sizerLeftPanel = new wxBoxSizer(wxVERTICAL);

   // String them all together:
   boxSizer->Add(mySplitter, 1, wxGROW);
   if (!fixed && myCVSROOTList->IsShown())
      sizerLeftPanel->Add(myCVSROOTList, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   sizerLeftPanel->Add(boxSizerCvsroot, 0, wxGROW | wxALL, 0);
   sizerLeftPanel->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 7);
   sizerLeftPanel->Add(gridSizer, 0, wxGROW | wxALL, 0);
   sizerLeftPanel->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 7);
   sizerLeftPanel->Add(boxSizerBug, 0, wxGROW | wxALL, 0);
   sizerRightPanel->Add(boxSizerModule, 0, wxGROW | wxALL, 0);
   leftPanel->SetSizer(sizerLeftPanel);
   rightPanel->SetSizer(sizerRightPanel);

   myTreeCtrl->PushEventHandler(new ModuleBasicsPageEvtHandler(this));

   myProtocolMenu->SetToolTip(tip1);
   myProtocolParametersCombo->SetToolTip(tip2);
   myServerCombo->SetToolTip(tip3);
   myDirectoryCombo->SetToolTip(tip4);
   myUsernameCombo->SetToolTip(tip5);
   myModuleCombo->SetToolTip(tip6);
   myCVSROOTText->SetToolTip(tip7);

   if (myFetchModuleList)
      myFetchModuleList->SetToolTip(tip8);

   myPortCombo->SetToolTip(tip9);
   
   label1->SetToolTip(tip1);
   label2->SetToolTip(tip2);
   label3->SetToolTip(tip3);
   label4->SetToolTip(tip4);
   label5->SetToolTip(tip5);
   label6->SetToolTip(tip6);
   myCVSROOTText->SetToolTip(tip7);
   label8->SetToolTip(tip9);

   if (!fixed)
   {
      if (myCVSROOTList->GetItemCount() > 0)
         myCVSROOTList->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      else
         myCVSROOTText->SetInitialTip(_("Click here and paste in the CVSROOT"));
   }
   boxSizer->SetSizeHints(this);
   boxSizer->Fit(this);

   // Apply column widths
   unsigned int numColumns = std::min(size_t(REPO_NUM_COLUMNS), myColumnWidths.size());
   for (unsigned int i = 0; i < numColumns; ++i)
   {
      int colWidth = myColumnWidths[i];
      if (colWidth <= 0 || colWidth > REPO_MAX_COLUMN_SIZE)
          colWidth = REPO_DEF_COLUMN_SIZE;

      myTreeCtrl->SetColumnWidth(i, wxDLG_UNIT_X(this, colWidth));
   }

   if (fixed)
   {
      myCVSROOTText->Disable();
      myProtocolMenu->Disable();
      myProtocolParametersCombo->Disable();
      myServerCombo->Disable();
      myPortCombo->Disable();
      myDirectoryCombo->Disable();
      myUsernameCombo->Disable();
      myModuleCombo->Disable();
   }
}

void ModuleBasicsPage::ProcessNode(CRepoNode* node, wxTreeItemId rootId, bool onlychildren)
{
   wxTreeItemId id, subid;
   if (!node)
       return;

   do
   {
       if (!onlychildren)
           switch(node->GetType())
           {
           case kNodeModule:
           {
               wxString module = wxText(((CRepoNodeDirectory&) *node).c_str());
               id = myTreeCtrl->AppendItem(rootId, module.c_str(), IconModule, -1,
                                           new ModuleBasicsPageTreeItemData(node), false);
               myTreeCtrl->Expand(rootId);
               break;
           }

           case kNodeDirectory:
           {
               CRepoNodeDirectory* directoryNode = reinterpret_cast<CRepoNodeDirectory*>(node);
               
               std::string fullName;
               if (node->Root()->GetType() == kNodeDirectory)
                   fullName = reinterpret_cast<CRepoNodeDirectory*>(node->Root())->c_str();
               else
                   fullName = reinterpret_cast<CRepoNodeModule*>(node->Root())->c_str();
               fullName += "/";
               fullName += directoryNode->name_c_str();
               directoryNode->SetFullname(fullName);
               std::string directory = directoryNode->name_c_str();
               id = myTreeCtrl->AppendItem(rootId, wxText(directory), IconDirectory, -1,
                                           new ModuleBasicsPageTreeItemData(directoryNode), false);
               CRepoNodeRubbish* rubbishnode = new CRepoNodeRubbish(directoryNode);
               subid = myTreeCtrl->AppendItem(id, wxT("rubbish"), IconFile, -1,
                                              new ModuleBasicsPageTreeItemData(rubbishnode), false);
               myTreeCtrl->Collapse(id);

               //myTreeCtrl->SetItemBackgroundColour(id, *myDirectoryColor);
               //myTreeCtrl->Expand(rootId);
               rootId = id;
               break;
           }

           case kNodeFile:
           {
               CRepoNodeFile& filenode = (CRepoNodeFile&) *node;
               EntnodeFile& item = *filenode;
               ENTNODE anode(item);

               std::string lookupName = anode.Data()->GetName();
               std::string ts = anode.Data()->GetTS();
               std::string rev1 = anode.Data()->GetVN();

               std::string fullName;
               if (node->Root()->GetType() == kNodeDirectory)
                   fullName = reinterpret_cast<CRepoNodeDirectory*>(node->Root())->str();
               else
                   fullName = reinterpret_cast<CRepoNodeModule*>(node->Root())->c_str();
               fullName += "/";
               fullName += lookupName;
               filenode.SetFullname(fullName);
               wxString wlookupName = wxText(lookupName);
               id = myTreeCtrl->AppendItem(rootId, wlookupName, IconFile, -1, 
                                           new ModuleBasicsPageTreeItemData(((CRepoNodeFile*) node)));
               myTreeCtrl->SetItemText(id, REPO_COL_FILE, wlookupName);
               myTreeCtrl->SetItemText(id, REPO_COL_REVISION, wxText(rev1));
               wxChar date[40];
               asctime_to_local_DateTimeFormatted(ts.c_str(), date, sizeof(date), false);
               myTreeCtrl->SetItemText(id, REPO_COL_DATE, date);
               break;
           }

           default:
               break;
           }
       else
       {
           id = myTreeCtrl->GetSelection();
           myTreeCtrl->Expand(rootId);
       }

       if (node->GetType() == kNodeModule)
       {
           // Process directory first
           std::vector<CRepoNode*>::iterator it = node->Childs().begin();
           while (it != node->Childs().end())
           {
               if ((*it)->GetType() == kNodeDirectory)
                   ProcessNode(*it, id, false);
               it++;
           }
           // Process rest next
           it = node->Childs().begin();
           while (it != node->Childs().end())
           {
               if ((*it)->GetType() != kNodeDirectory)
                   ProcessNode(*it, id, false);
               it++;
           }
       }
       else
       {
           std::vector<CRepoNode*>::iterator it = node->Childs().begin();
           while (it != node->Childs().end())
           {
               ProcessNode(*it, id, false);
               it++;
           }
       }
       node = node->Next();
   } while (node);
}


void ModuleBasicsPage::SetRepoGraph(wxWindow* parent)
{
   myTree = GetRepoGraph(wxT(""), parent, "", myRepoParserData);
   if (myTree)
      Refresh();
}


void ModuleBasicsPage::Refresh() 
{
   TDEBUG_ENTER("ModuleBasicsPage::Refresh");
   myTreeCtrl->DeleteAllItems();
   RefreshModuleList();
   // Dummy root, not displayed
   myTreeCtrl->AddRoot(wxT("root"));
   ProcessNode(myTree, myTreeCtrl->GetRootItem(), false);
}

void ModuleBasicsPage::EditChanged(wxCommandEvent&)
{
   myBugNumber->Enable(myEditCheckBox->GetValue());
   myUseBugCheckBox->Enable(myEditCheckBox->GetValue());
}

void ModuleBasicsPage::RefreshModuleList()
{
   // Information about modules
}

void ModuleBasicsPage::OnMenuGet(wxCommandEvent&)
{
   //LaunchTortoiseAct(true, CvsLsVerb, myFilename, 
   //                  "-r " + Quote(myRevision1), GetHwndOf(this)); 
   //CVSStatus::InvalidateCache();
   //Refresh();
}

void ModuleBasicsPage::GoBrowse(const wxTreeItemId& selone)
{
   wxTreeItemData* thisdata = myTreeCtrl->GetItemData(selone);
   if (thisdata)
   {
      CRepoNode* thisnode = static_cast<ModuleBasicsPageTreeItemData*>(thisdata)->m_Node;
      std::string ofwhat;
      if (thisnode->GetType() == kNodeModule)
      {
         // Need to get a "tree" by doing a cvs ls of this module
         CRepoNodeModule* modulenode = static_cast<CRepoNodeModule*>(thisnode);
         ofwhat = modulenode->c_str();
      }
      else if (thisnode->GetType() == kNodeDirectory)
      {
         // need to get a "tree" by doing a cvs ls of this directory
         CRepoNodeDirectory* directorynode = static_cast<CRepoNodeDirectory*>(thisnode);
         ofwhat = directorynode->c_str();
      }
      if (!ofwhat.empty())
      {
         myRepoParserData.Clear();
         RemoteLists::GetEntriesList(myParent,
                                     myRepoParserData.entries,
                                     myCVSRoot.GetCVSROOT(),
                                     ofwhat);
         CRepoNode* node = CvsRepoGraph(myRepoParserData, 0, thisnode);
         // then need to pass the new tree for parsing to ProcessNode
         ProcessNode(node, selone, true);
      }
   }
}

void ModuleBasicsPage::OnMenuBrowse(wxCommandEvent&)
{
   wxArrayTreeItemIds sel;
   int selCount = static_cast<int>(myTreeCtrl->GetSelections(sel));
   if (selCount == 1)
   {
      myTreeCtrl->DeleteChildren(sel[0]);
      ModuleBasicsPage::GoBrowse(sel[0]);
   }
}

void ModuleBasicsPage::ModuleChanged(wxCommandEvent&)
{
   myModule = wxAscii(myModuleCombo->GetValue().c_str());
   UpdateModule(myModuleCombo);
}

void ModuleBasicsPage::ProtocolChanged(wxCommandEvent&)
{
   TDEBUG_ENTER("ModuleBasicsPage::ProtocolChanged");

   int protocolValue = myProtocolMenu->GetSelection();
   if (protocolValue == ProtSspi)
      myCVSRoot.SetProtocol("sspi");
#ifndef MARCH_HARE_BUILD
   else if (protocolValue == ProtExt)
      myCVSRoot.SetProtocol("ext");
#endif
   else if (protocolValue == ProtSsh)
      myCVSRoot.SetProtocol("ssh");
   else if (protocolValue == ProtServer)
      myCVSRoot.SetProtocol("server");
#ifndef MARCH_HARE_BUILD
   else if (protocolValue == ProtLocal)
      myCVSRoot.SetProtocol("local");
#endif
   else if (protocolValue == ProtGserver)
      myCVSRoot.SetProtocol("gserver");
   else if (protocolValue == ProtSserver)
      myCVSRoot.SetProtocol("sserver");
   else
      myCVSRoot.SetProtocol("pserver");
   
   myCVSRoot.SetProtocolParameters(myCVSRoot.AllowProtocolParameters() ?
                                   wxAscii(myProtocolParametersCombo->GetValue().c_str()) : "");
   myCVSRoot.SetUser(myCVSRoot.AllowUser() ? wxAscii(myUsernameCombo->GetValue().c_str()) : "");
   if (myCVSRoot.AllowServer())
   {
       std::string server = wxAscii(myServerCombo->GetValue().c_str());
       std::string newServer = server;
       newServer = Trim(newServer);
       if (newServer != server)
       {
           server = newServer;
           size_t pos = myDirectoryCombo->GetInsertionPoint();
           myServerCombo->SetValue(wxText(server));
           if (pos < server.size())
               myServerCombo->SetInsertionPoint(pos);
       }
       myCVSRoot.SetServer(server);
   }
   myCVSRoot.SetPort(myCVSRoot.AllowPort() ? wxAscii(myPortCombo->GetValue().c_str()) : "");

   // Let's be careful with the directory value to prevent common mistakes
   std::string directory = wxAscii(myDirectoryCombo->GetValue().c_str());
   std::string newdirectory = directory;
   // Silently replace backslashes with slashes
   FindAndReplace<std::string>(newdirectory, "\\", "/");
   // Don't move the insertion point
   long pos = myDirectoryCombo->GetInsertionPoint();
   if (directory.size() > 1)
   {
      // Neither leading slash nor drive letter
      if ((directory[0] != '/') && (directory[1] != ':'))
      {
         // Add leading slash
         newdirectory = '/' + directory;
         // Keep track
         ++pos;
      }
   }
   if (newdirectory != directory)
   {
      // Update combo box
      directory = newdirectory;
      myDirectoryCombo->SetValue(wxText(directory));
      myDirectoryCombo->SetInsertionPoint(pos);
   }
   if (directory != myCVSRoot.GetDirectory())
   {
      myModule = "";
      UpdateModule(0);
   }
   myCVSRoot.SetDirectory(directory.c_str());

   UpdateCVSRoot(myProtocolMenu);
}

void ModuleBasicsPage::RootChanged(wxCommandEvent&)
{
   std::string cvsroot = wxAscii(myCVSROOTText->GetValue().c_str());
   
   // strip common extraneous data from either end
   while (cvsroot.size() > 0 && cvsroot[cvsroot.size() - 1] == ' ')
      cvsroot = cvsroot.substr(0, cvsroot.size() - 1);
   // these happen when copying (part of) the "-d" switch
   if (cvsroot.size() > 0 && cvsroot.substr(0, 1) == "-")
      cvsroot = cvsroot.substr(1);
   if (cvsroot.size() > 0 && cvsroot.substr(0, 1) == "d")
      cvsroot = cvsroot.substr(1);
   if (cvsroot.size() > 7 && cvsroot.substr(0, 7) == "CVSROOT")
      cvsroot = cvsroot.substr(7);
   if (cvsroot.size() > 1 && cvsroot.substr(0, 2) == ": ")
      cvsroot = cvsroot.substr(2);
   while (cvsroot.size() > 0 &&
      (cvsroot.substr(0, 1) == " " || cvsroot.substr(0, 1) == "="))
      cvsroot = cvsroot.substr(1);
   
   myCVSRoot.SetCVSROOT(cvsroot.c_str());
   UpdateCVSRoot(myCVSROOTText);
}

void ModuleBasicsPage::OKClicked()
{
   // Sanitize CVSROOT
   myCVSRoot.SetCVSROOT(RemoveTrailingDelimiter(myCVSRoot.GetCVSROOT()));

   WriteComboList(myProtocolParametersCombo, "History\\Checkout Protocol Parameters");
   WriteComboList(myServerCombo, "History\\Checkout Server");
   WriteComboList(myPortCombo, "History\\Checkout Port");
   WriteComboList(myDirectoryCombo, "History\\Checkout Directory");
   WriteComboList(myUsernameCombo, "History\\Checkout Username");
   WriteOptionMenu(myProtocolMenu, "History\\Checkout Protocol");

   if (!myFixed)
   {
      // Read in old CVSROOT history from list
      std::vector< std::string > history, historyM;
      for (int j = 0; j < myCVSROOTList->GetItemCount(); ++j)
      {
         history.push_back(wxAscii(myCVSROOTList->GetItemText(j).c_str()));
         
         wxListItem info;
         info.m_mask = wxLIST_MASK_TEXT;
         info.m_itemId = j;
         info.m_col = 1;
         myCVSROOTList->GetItem(info);
         historyM.push_back(wxAscii(info.m_text.c_str()));
      }
      // Erase entry if already there
      int found = -1;

      for (unsigned int i = 0; i < std::min(history.size(), historyM.size()); ++i)
      {
         if (history[i].c_str() == myCVSRoot.GetCVSROOT()
             && historyM[i].c_str() == myModule)
         {
            found = i;
         }
      }
      if (found >= 0)
      {
         history.erase(history.begin() + found);
         historyM.erase(historyM.begin() + found);
      }
      // Add ourselves to front
      if (!myModule.empty() && !myCVSRoot.GetCVSROOT().empty())
      {
         history.insert(history.begin(), myCVSRoot.GetCVSROOT());
         historyM.insert(historyM.begin(), myModule);
      }
      
      // Write out again
      TortoiseRegistry::WriteVector("History\\Checkout CVSROOT", history, 10);
      TortoiseRegistry::WriteVector("History\\Checkout CVSROOT Module", historyM, 10);
      TortoiseRegistry::WriteBoolean("Dialogs\\Checkout\\Use Bug", myUseBugCheckBox->GetValue());
      TortoiseRegistry::WriteBoolean("Dialogs\\Checkout\\Edit", myEditCheckBox->GetValue());
      TortoiseRegistry::WriteString("Dialogs\\Checkout\\Bug Number", myBugNumber->GetValue().c_str());
   }
}


// Set RevOptions pointer
void ModuleBasicsPage::SetRevOptions(RevOptions* revOptions)
{
   myRevOptions = revOptions;
   UpdateRevOptions();
}


void ModuleBasicsPage::UpdateSensitivity()
{
   if (!myFixed)
   {
      myProtocolParametersCombo->Enable(myCVSRoot.AllowProtocolParameters());
      myUsernameCombo->Enable(myCVSRoot.AllowUser());
      myPortCombo->Enable(myCVSRoot.AllowPort());
      myServerCombo->Enable(myCVSRoot.AllowServer());
   }
   
   // Find OK button on parent, and ghost it appropriately
   wxWindow* grandparent = GetGrandParent();
   wxDialog* dialog = wxDynamicCast(grandparent, wxDialog);
   if (dialog)
   {
      wxWindow* OK = dialog->FindWindow(wxID_OK);
      if (OK)
          OK->Enable(myCVSRoot.Valid() && !myModule.empty());
   }
   
   // Fetch modules list widget
   if (myFetchModuleList)
      myFetchModuleList->Enable(myCVSRoot.Valid());
}

void ModuleBasicsPage::SelectInListBox()
{
   std::string cvsroot = myCVSRoot.GetCVSROOT().c_str();
   
   // select in list box if available 
   std::string curSel = wxAscii(ListSelectedText().c_str());
   std::string curModule = wxAscii(ListSelectedModule().c_str());
   if (curSel != cvsroot.c_str() 
       || ((myMode == MB_CHECKOUT_MODULE) && (curModule != myModule.c_str())))
   {
      // clear selection
      long sel = myCVSROOTList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (sel != -1)
         myCVSROOTList->SetItemState(sel, 0, wxLIST_STATE_SELECTED);
      // find and set list box to cvsroot/module pair if already there
      if (!cvsroot.empty())
      {
         for (int i = 0; i < myCVSROOTList->GetItemCount(); ++i)
         {               
            wxListItem info;
            if (myMode == MB_CHECKOUT_MODULE)
            {
               info.m_mask = wxLIST_MASK_TEXT;
               info.m_itemId = i;
               info.m_col = 1;
               myCVSROOTList->GetItem(info);
            }
            if (wxAscii(myCVSROOTList->GetItemText(i).c_str()) == cvsroot
                && (myMode != MB_CHECKOUT_MODULE || wxAscii(info.m_text.c_str()) == myModule))
            {
               myCVSROOTList->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            }
         }
      }
   }
}

void ModuleBasicsPage::SetModule(const std::string& module)
{
   myModule = module;
   UpdateModule(0);
}


void ModuleBasicsPage::FillInModuleList(std::vector<std::string>& modules)
{
    TDEBUG_ENTER("FillInModuleList");
    if (modules.size() > 0)
    {
        // Move CVSROOT to the end
        std::vector<std::string>::iterator it = std::find(modules.begin(), modules.end(), "CVSROOT");
        if (it != modules.end())
            modules.erase(it);
        modules.push_back("CVSROOT");
      
        myModuleCombo->Clear();
        myTreeCtrl->DeleteAllItems();
        myTreeCtrl->AddRoot(wxT("root"));
        for (unsigned int j = 0; j < modules.size(); ++j)
        {
            TDEBUG_TRACE("Item " << modules[j]);
            wxTreeItemId modid, subid;
            EntnodeDir* modent = new EntnodeDir(modules[j].c_str(), modules[j].c_str());
            CRepoNodeModule* mynode = new CRepoNodeModule(*modent, 0);
            CRepoNodeRubbish* rubbishnode = new CRepoNodeRubbish(mynode);
            myModuleCombo->Append(wxText(modules[j]));
            modid = myTreeCtrl->AppendItem(myTreeCtrl->GetRootItem(), wxText(modules[j]), IconModule, -1,
                                           new ModuleBasicsPageTreeItemData(mynode), false);
            subid = myTreeCtrl->AppendItem(modid, wxT("rubbish"), IconFile, -1,
                                           new ModuleBasicsPageTreeItemData(rubbishnode), false);
            myTreeCtrl->Collapse(modid);

            modent->UnRef();
        }
        myModuleCombo->SetValue(wxText(modules[0]));
        myModule = modules[0].c_str();
        UpdateRevOptions();
    }
}

// Use various heuristics to get a list of modules on the server
void ModuleBasicsPage::FetchModuleList(wxCommandEvent&)
{
   if (myFetchModuleList)
      myFetchModuleList->Enable(false);
   
   // Get module list and put it in the dialog
   std::vector<std::string> modules;
   CVSRoot cvsRoot = myCVSRoot;
   cvsRoot.RemoveTrailingDelimiter();
   RemoteLists::GetModuleList(GetParent(), modules, cvsRoot);
   wxString module = myModuleCombo->GetValue().c_str();
   FillInModuleList(modules);
   myModuleCombo->SetValue(module);
   
   if (modules.size() <= 1) // CVSROOT
   {
       wxString msg(_("Module list not available for server"));
       msg += wxT(" ");
       msg += wxText(myCVSRoot.GetServer());
       msg += wxT(".\n\n");
       msg += _(
"Ask your administrator to either install a web browser interface on the server (CVSweb or ViewCVS) or \
add the modules to the CVSROOT/modules file.\n\nMeanwhile, you'll have to find the module name elsewhere.");
      DoMessageDialog(GetParent(), msg);
   }
   
   myModule = wxAscii(myModuleCombo->GetValue().c_str());
   UpdateSensitivity();
   SelectInListBox();
   UpdateRevOptions();
}

wxString ModuleBasicsPage::ListSelectedText()
{
   long sel = myCVSROOTList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (sel == -1)
      return wxT("");
   else
      return myCVSROOTList->GetItemText(sel);
}

wxString ModuleBasicsPage::ListSelectedModule()
{
   if (myMode == MB_CHECKOUT_MODULE)
   {
      long sel = myCVSROOTList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (sel == -1)
         return wxT("");

      wxListItem info;
      info.m_mask = wxLIST_MASK_TEXT;
      info.m_itemId = sel;
      info.m_col = 1;
      myCVSROOTList->GetItem(info);
      return info.m_text;
   }
   else
   {
      return wxT("");
   }
}

void ModuleBasicsPage::ListSelected(wxCommandEvent&)
{
   std::string sel = wxAscii(ListSelectedText().c_str());
   std::string module = wxAscii(ListSelectedModule().c_str());

   if (!sel.empty())
   {
      myCVSRoot.SetCVSROOT(sel.c_str());
      UpdateCVSRoot(myCVSROOTList);
   }
   if (!module.empty())
   {
      myModule = module;
      UpdateModule(myCVSROOTList);
   }
}

void ModuleBasicsPage::SetCVSRoot(const CVSRoot& root)
{
   myCVSROOTText->SetValue(wxText(root.GetCVSROOT()));
   wxCommandEvent event;
   RootChanged(event);
}

void ModuleBasicsPage::UpdateRevOptions()
{
   // Set CVSROOT for Revision page
   if (myRevOptions)
   {
      myRevOptions->SetCVSRoot(myCVSRoot);
      myRevOptions->SetModule(myModule);
   }
}



void ModuleBasicsPage::FetchCachedModuleList()
{
   if (myFetchModuleList)
      myFetchModuleList->Enable(false);
   
   // Get module list and put it in the dialog
   std::vector<std::string> modules;
   RemoteLists::GetCachedModuleList(modules, myCVSRoot);
   wxString module = myModuleCombo->GetValue();
   myModuleCombo->Clear();
   FillInModuleList(modules);
   myModuleCombo->SetValue(module);
   
   myModule = wxAscii(myModuleCombo->GetValue().c_str());
}


void ModuleBasicsPage::SortItems()
{
   myTreeCtrl->SetSortParams(true, mySortColumn);
   long c = 0; // Cookie
   wxTreeItemId rootItem = myTreeCtrl->GetRootItem();
   if (rootItem.IsOk())
       myTreeCtrl->SortChildren(myTreeCtrl->GetFirstChild(rootItem, c));
}



void ModuleBasicsPage::UpdateCVSRoot(wxWindow* source)
{
   if (myUpdating)
      return;

   myUpdating = true;
   if (!myFixed && (source != myCVSROOTList))
   {
      SelectInListBox();
   }
   if (source != myCVSROOTText)
   {
      myCVSROOTText->SetValue(wxText(myCVSRoot.GetCVSROOT()));
   }
   if (source != myProtocolMenu)
   {
      int protocolValue;
      if (myCVSRoot.GetProtocol() == "sspi")
         protocolValue = 1;
      else if (myCVSRoot.GetProtocol() == "ext")
         protocolValue = 2;
      else if (myCVSRoot.GetProtocol() == "ssh")
         protocolValue = 3;
      else if (myCVSRoot.GetProtocol() == "server")
         protocolValue = 4;
      else if (myCVSRoot.GetProtocol() == "local")
         protocolValue = 5;
      else if (myCVSRoot.GetProtocol() == "gserver")
         protocolValue = 6;
      else if (myCVSRoot.GetProtocol() == "sserver")
         protocolValue = 7;
      else if (myCVSRoot.GetProtocol() == "ntserver")
         protocolValue = 8;
      else
         protocolValue = 0;
      myProtocolMenu->SetSelection(protocolValue);
   
      if ((wxAscii(myProtocolParametersCombo->GetValue().c_str()) != myCVSRoot.GetProtocolParameters()) &&
          myCVSRoot.AllowProtocolParameters())
      {
         myProtocolParametersCombo->SetValue(wxText(myCVSRoot.GetProtocolParameters()));
      }
      if (wxAscii(myUsernameCombo->GetValue().c_str()) != myCVSRoot.GetUser() 
          && myCVSRoot.AllowUser())
      {
         myUsernameCombo->SetValue(wxText(myCVSRoot.GetUser()));
      }
      if (wxAscii(myServerCombo->GetValue().c_str()) != myCVSRoot.GetServer() 
          && myCVSRoot.AllowServer())
      {
         myServerCombo->SetValue(wxText(myCVSRoot.GetServer()));
      }
      if (wxAscii(myPortCombo->GetValue().c_str()) != myCVSRoot.GetPort()
         && myCVSRoot.AllowPort())
      {
         myPortCombo->SetValue(wxText(myCVSRoot.GetPort()));
      }
      if (wxAscii(myDirectoryCombo->GetValue().c_str()) != myCVSRoot.GetDirectory())
      {
         myDirectoryCombo->SetValue(wxText(myCVSRoot.GetDirectory()));
      }
   }

   if (!myFixed)
   {
      if (myCVSRoot.Valid())
         FetchCachedModuleList();

      UpdateSensitivity();
   }
   UpdateRevOptions();
   myUpdating = false;
}


void ModuleBasicsPage::UpdateModule(wxWindow* source)
{
   if (myUpdating)
      return;

   myUpdating = true;

   if (source != myModuleCombo)
   {
      myModuleCombo->SetValue(wxText(myModule));
      myTreeCtrl->DeleteAllItems();
   }
   if (source != myCVSROOTList)
   {
      SelectInListBox();
   }

   UpdateSensitivity();
   UpdateRevOptions();
   myUpdating = false;
}


std::string ModuleBasicsPage::GetBugNumber() const
{
    return myUseBugCheckBox->GetValue() ? wxAscii(myBugNumber->GetValue()) : "";
}


void ModuleBasicsPage::ExportEnabled(bool enabled)
{
    if (enabled)
        myEditCheckBox->SetValue(0);
    myEditCheckBox->Enable(!enabled);
    wxCommandEvent event;
    EditChanged(event);
}


RepoTreeCtrl* ModuleBasicsPage::GetTreeCtrl()
{
    return myTreeCtrl;
}

wxComboBox* ModuleBasicsPage::GetModuleCombo()
{
    return myModuleCombo;
}

std::string& ModuleBasicsPage::GetModule()
{
    return myModule;
}

void ModuleBasicsPage::UpdateModule()
{
    UpdateModule(myModuleCombo);
}

int& ModuleBasicsPage::GetSortColumn()
{
    return mySortColumn;
}
