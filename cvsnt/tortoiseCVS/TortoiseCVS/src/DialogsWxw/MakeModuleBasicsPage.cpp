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

#include "MakeModuleBasicsPage.h"
#include "MessageDialog.h"
#include "ExtComboBox.h"
#include "CvsRootListCtrl.h"
#include <wx/statline.h>
#include "WxwHelpers.h"

#include "wxInitialTipTextCtrl.h"

#include "../Utils/ProcessUtils.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/PathUtils.h"
#include "../Utils/Translate.h"
#include "../CVSGlue/RemoteLists.h"


enum
{
   MODBASICS_ID_PROTOCOL = 10001,
   MODBASICS_ID_PROTPARMS,
   MODBASICS_ID_SERVER,
   MODBASICS_ID_PORT,
   MODBASICS_ID_DIRECTORY,
   MODBASICS_ID_MODULE,
   MODBASICS_ID_USERNAME,
   MODBASICS_ID_FETCHMODULELIST,
   MODBASICS_ID_CVSROOTTEXT,
   MODBASICS_ID_CVSROOTLIST,
};


BEGIN_EVENT_TABLE(MakeModuleBasicsPage, wxPanel)
   EVT_COMMAND(MODBASICS_ID_PROTOCOL,   wxEVT_COMMAND_CHOICE_SELECTED,          MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PROTPARMS,  wxEVT_COMMAND_TEXT_UPDATED,             MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PROTPARMS,  wxEVT_COMMAND_COMBOBOX_SELECTED,        MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_SERVER,     wxEVT_COMMAND_TEXT_UPDATED,             MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_SERVER,     wxEVT_COMMAND_COMBOBOX_SELECTED,        MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PORT,       wxEVT_COMMAND_TEXT_UPDATED,             MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_PORT,       wxEVT_COMMAND_COMBOBOX_SELECTED,        MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_DIRECTORY,  wxEVT_COMMAND_TEXT_UPDATED,             MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_DIRECTORY,  wxEVT_COMMAND_COMBOBOX_SELECTED,        MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_MODULE,     wxEVT_COMMAND_TEXT_UPDATED,             MakeModuleBasicsPage::ModuleChanged)
   EVT_COMMAND(MODBASICS_ID_MODULE,     wxEVT_COMMAND_COMBOBOX_SELECTED,        MakeModuleBasicsPage::ModuleChanged)
   EVT_COMMAND(MODBASICS_ID_USERNAME,   wxEVT_COMMAND_TEXT_UPDATED,             MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_USERNAME,   wxEVT_COMMAND_COMBOBOX_SELECTED,        MakeModuleBasicsPage::ProtocolChanged)
   EVT_COMMAND(MODBASICS_ID_FETCHMODULELIST, wxEVT_COMMAND_BUTTON_CLICKED,      MakeModuleBasicsPage::FetchModuleList)
   EVT_COMMAND(MODBASICS_ID_CVSROOTTEXT, wxEVT_COMMAND_TEXT_UPDATED,            MakeModuleBasicsPage::RootChanged)
   EVT_COMMAND(MODBASICS_ID_CVSROOTLIST, wxEVT_COMMAND_LIST_ITEM_SELECTED,      MakeModuleBasicsPage::ListSelected)
END_EVENT_TABLE()


MakeModuleBasicsPage::MakeModuleBasicsPage(wxWindow* parent)
   : wxPanel(parent),
     myFetchModuleList(0),
     m_PortValidator(wxFILTER_NUMERIC), 
     myCVSROOTList(0),
     m_Updating(false)
{
   myCVSRoot.SetProtocol("pserver");
   
   // Two vertical columns
   wxBoxSizer *boxSizer = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer *boxSizerCvsroot = new wxBoxSizer(wxHORIZONTAL);
   wxBoxSizer *boxSizerModule = new wxBoxSizer(wxHORIZONTAL);
   wxFlexGridSizer *gridSizer = new wxFlexGridSizer(2);
   gridSizer->AddGrowableCol(1);
   SetAutoLayout(TRUE);
   SetSizer(boxSizer);
   
   // Labels
   wxStaticText* label1 = new wxStaticText(this, -1, wxString(_("Protocol")) + _(":"));
   wxStaticText* label2 = new wxStaticText(this, -1, wxString(_("Protocol parameters")) + _(":"));
   wxStaticText* label3 = new wxStaticText(this, -1, wxString(_("Server")) + _(":"));
   wxStaticText* label4 = new wxStaticText(this, -1, wxString(_("Repository folder")) + _(":"));
   wxStaticText* label5 = new wxStaticText(this, -1, wxString(_("User name")) + _(":"));
   wxStaticText* label6 = new wxStaticText(this, -1, wxString(_("Module")) + _(":"));
   wxStaticText* label7 = new wxStaticText(this, -1, wxString(_("CVSROOT")) + _(":"));
   wxStaticText* label8 = new wxStaticText(this, -1, wxString(_("Port")) + _(":"));
   
   // CVSROOT stuff
   myCVSROOTList = new CvsRootListCtrl(this, MODBASICS_ID_CVSROOTLIST);
   myCVSROOTList->InsertColumn(0, _("Previous CVSROOTs"), wxLIST_FORMAT_LEFT, wxDLG_UNIT_X(this, 200));

   // CVSROOT environment variable
   bool envExists = false;
   const char* env = getenv("CVSROOT");
   
   // Fill in CVSROOT history list
   std::vector< std::string > history, historyM;
   TortoiseRegistry::ReadVector("History\\Checkout CVSROOT", history);
      
   unsigned int itemIndex = 0;
   // Remove duplicates
   std::sort(history.begin(), history.end());
   history.erase(std::unique(history.begin(), history.end()), history.end());
   for (unsigned int i = 0; i < history.size(); ++i)
   {
      if (env && history[i] == env)
         envExists = true;
      myCVSROOTList->InsertItem(itemIndex, wxText(history[i]), 0);
   }

   if (env && !envExists)
      myCVSROOTList->InsertItem(0, wxText(env), 0);
   
   if (myCVSROOTList->GetItemCount() == 0)
      myCVSROOTList->Show(FALSE);
   
   myCVSROOTList->SetBestColumnWidth(0);
   int colwidth = myCVSROOTList->GetColumnWidth(0);
   myCVSROOTList->SetSize(colwidth + GetSystemMetrics(SM_CXVSCROLL), 
                          wxDLG_UNIT_X(this, 60));
   
   myCVSROOTText = new wxInitialTipTextCtrl(this, MODBASICS_ID_CVSROOTTEXT);
   boxSizerCvsroot->Add(label7, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   boxSizerCvsroot->Add(myCVSROOTText, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   // Protocols
   const int protocolCount = 9;
   wxString protocols[protocolCount] = 
   {
      _("Password server (:pserver:)"),
      _("Windows authentication (:sspi:)"),
      _("Secure shell (:ext:)"),
      _("Secure shell (:ssh:)"),
      _("Remote shell (:server:)"),
      _("Locally mounted folder (:local:)"),
      _("Kerberos (:gserver:)"),
      _("SSL (:sserver:)"),
      _("Named pipe (:ntserver:)")
   };
   myProtocolMenu = new wxChoice(this, MODBASICS_ID_PROTOCOL,
                                 wxDefaultPosition, wxDefaultSize, protocolCount, protocols);
   gridSizer->Add(label1, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myProtocolMenu, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   ReadOptionMenu(myProtocolMenu, "Checkout Protocol Default");

   myProtocolParametersCombo = MakeComboList(this, MODBASICS_ID_PROTPARMS,
                                             "History\\Checkout Protocol Parameters", "",
                                             COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label2, 0, wxALL| wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myProtocolParametersCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   myServerCombo = MakeComboList(this, MODBASICS_ID_SERVER,
                                 "History\\Checkout Server", "",
                                 COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label3, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myServerCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);

   myPortCombo = MakeComboList(this, MODBASICS_ID_PORT,
                               "History\\Checkout Port", "",
                               COMBO_FLAG_NODEFAULT, 0, &m_PortValidator);
   gridSizer->Add(label8, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myPortCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   myDirectoryCombo = MakeComboList(this, MODBASICS_ID_DIRECTORY,
                                    "History\\Checkout Directory", "",
                                    COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label4, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myDirectoryCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   // Use their login name under Windows as a sensible default
   myUsernameCombo = MakeComboList(this, MODBASICS_ID_USERNAME,
                                   "History\\Checkout Username", GetUserName(),
                                   COMBO_FLAG_NODEFAULT);
   gridSizer->Add(label5, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   gridSizer->Add(myUsernameCombo, 0, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   // Module box
   wxBoxSizer *moduleSizer = new wxBoxSizer(wxHORIZONTAL);
   myModuleCombo = new ExtComboBox(this, MODBASICS_ID_MODULE);
   moduleSizer->Add(myModuleCombo, 2, wxALIGN_CENTER_VERTICAL, 0);
   myFetchModuleList = new wxButton(this, MODBASICS_ID_FETCHMODULELIST, _("&Fetch list..."));
   moduleSizer->Add(myFetchModuleList, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
   boxSizerModule->Add(label6, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
   boxSizerModule->Add(moduleSizer, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   
   // String them all together:
   if (myCVSROOTList->IsShown())
   {
      boxSizer->Add(myCVSROOTList, 1, wxGROW | wxALL | wxALIGN_CENTER_VERTICAL, 5);
   }
   boxSizer->Add(boxSizerCvsroot, 0, wxGROW | wxALL, 0);
   boxSizer->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 7);
   boxSizer->Add(gridSizer, 0, wxGROW | wxALL, 0);
   boxSizer->Add(new wxStaticLine( this, -1 ), 0, wxEXPAND | wxALL, 7);
   boxSizer->Add(boxSizerModule, 0, wxGROW | wxALL, 0);
   
   // Tooltips
   const wxChar* tip1 = _("How your machine should connect to the CVS repository");
   const wxChar* tip2 = _("Parameters associated with this protocol");
   const wxChar* tip3 = _("The computer that the CVS repository is on");
   const wxChar* tip4 = _("Location of the repository on the server");
   const wxChar* tip5 = _("The user name that CVS should use");
   const wxChar* tip6 = _("The name of the module");
   const wxChar* tip7 = _("Technical details of the CVSROOT which TortoiseCVS is going to use");
   const wxChar* tip8 = _("Retrieves a list of modules from the server, and puts them in the dropdown box");
   const wxChar* tip9 = _("The TCP port of your CVS server process");
   
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

   if (myCVSROOTList->GetItemCount() > 0)
   {
      myCVSROOTList->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
   }
   else
   {
      myCVSROOTText->SetInitialTip(_("Click here and paste in the CVSROOT"));
   }
   boxSizer->SetSizeHints(this);
   boxSizer->Fit(this);
}

void MakeModuleBasicsPage::ModuleChanged(wxCommandEvent&)
{
   myModule = wxAscii(myModuleCombo->GetValue().c_str());
   UpdateModule(myModuleCombo);
}

void MakeModuleBasicsPage::ProtocolChanged(wxCommandEvent&)
{
   TDEBUG_ENTER("MakeModuleBasicsPage::ProtocolChanged");

   int protocolValue = myProtocolMenu->GetSelection();
   if (protocolValue == 1)
      myCVSRoot.SetProtocol("sspi");
   else if (protocolValue == 2)
      myCVSRoot.SetProtocol("ext");
   else if (protocolValue == 3)
      myCVSRoot.SetProtocol("ssh");
   else if (protocolValue == 4)
      myCVSRoot.SetProtocol("server");
   else if (protocolValue == 5)
      myCVSRoot.SetProtocol("local");
   else if (protocolValue == 6)
      myCVSRoot.SetProtocol("gserver");
   else if (protocolValue == 7)
      myCVSRoot.SetProtocol("sserver");
   else if (protocolValue == 8)
      myCVSRoot.SetProtocol("ntserver");
   else
      myCVSRoot.SetProtocol("pserver");
   
   myCVSRoot.SetProtocolParameters(myCVSRoot.AllowProtocolParameters() ?
                                   wxAscii(myProtocolParametersCombo->GetValue().c_str()) : "");
   myCVSRoot.SetUser(myCVSRoot.AllowUser() ? wxAscii(myUsernameCombo->GetValue().c_str()) : "");
   myCVSRoot.SetServer(myCVSRoot.AllowServer() ? wxAscii(myServerCombo->GetValue().c_str()) : "");
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
   myCVSRoot.SetDirectory(directory.c_str());

   UpdateCVSRoot(myProtocolMenu);
}

void MakeModuleBasicsPage::RootChanged(wxCommandEvent&)
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

void MakeModuleBasicsPage::OKClicked()
{
   // Sanitize CVSROOT
   myCVSRoot.SetCVSROOT(RemoveTrailingDelimiter(myCVSRoot.GetCVSROOT()));

   WriteComboList(myProtocolParametersCombo, "History\\Checkout Protocol Parameters");
   WriteComboList(myServerCombo, "History\\Checkout Server");
   WriteComboList(myPortCombo, "History\\Checkout Port");
   WriteComboList(myDirectoryCombo, "History\\Checkout Directory");
   WriteComboList(myUsernameCombo, "History\\Checkout Username");
   WriteOptionMenu(myProtocolMenu, "History\\Checkout Protocol");

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
}


void MakeModuleBasicsPage::UpdateSensitivity()
{
   myProtocolParametersCombo->Enable(myCVSRoot.AllowProtocolParameters());
   myUsernameCombo->Enable(myCVSRoot.AllowUser());
   myPortCombo->Enable(myCVSRoot.AllowPort());
   myServerCombo->Enable(myCVSRoot.AllowServer());
   
   // Find OK button on parent, and ghost it appropriately
   wxWindow* grandparent = GetGrandParent();
   wxDialog* dialog = wxDynamicCast(grandparent, wxDialog);
   if (dialog)
   {
      wxWindow* OK = dialog->FindWindow(wxID_OK);
      if (OK)
         OK->Enable(myCVSRoot.Valid() && (myModule != ""));
   }
   
   // Fetch modules list widget
   if (myFetchModuleList)
      myFetchModuleList->Enable(myCVSRoot.Valid());
}

void MakeModuleBasicsPage::SelectInListBox()
{
   std::string cvsroot = myCVSRoot.GetCVSROOT();
   
   // select in list box if available 
   std::string curSel = wxAscii(ListSelectedText().c_str());
   if (curSel != cvsroot)
   {
      // clear selection
      long sel = myCVSROOTList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (sel != -1)
         myCVSROOTList->SetItemState(sel, 0, wxLIST_STATE_SELECTED);
      // find and set list box to cvsroot/module pair if already there
      if (!cvsroot.empty())
      {
         for (int i = 0; i < myCVSROOTList->GetItemCount(); ++i)
            if (wxAscii(myCVSROOTList->GetItemText(i).c_str()) == cvsroot)
               myCVSROOTList->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      }
   }
}

void MakeModuleBasicsPage::SetModule(const std::string& module)
{
   myModule = module;
   UpdateModule(0);
}


void MakeModuleBasicsPage::FillInModuleList(std::vector<std::string>& modules)
{
   if (modules.size() > 0)
   {
      // move CVSROOT to the end
      std::vector<std::string>::iterator it = std::find(modules.begin(), modules.end(), "CVSROOT");
      if (it != modules.end())
         modules.erase(it);
      modules.push_back("CVSROOT");
      
      myModuleCombo->Clear();
      for (unsigned int j = 0; j < modules.size(); ++j)
         myModuleCombo->Append(wxText(modules[j]));
      myModuleCombo->SetValue(wxText(modules[0]));
      myModule = modules[0];
   }
}

// Use various heuristics to get a list of modules on the server
void MakeModuleBasicsPage::FetchModuleList(wxCommandEvent&)
{
   if (myFetchModuleList)
      myFetchModuleList->Enable(false);
   
   // Get module list and put it in the dialog
   std::vector<std::string> modules;
   CVSRoot cvsRoot = myCVSRoot;
   cvsRoot.RemoveTrailingDelimiter();
   RemoteLists::GetModuleList(GetParent(), modules, cvsRoot);
   wxString module = myModuleCombo->GetValue();
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
add the modules to the CVSROOT/modules file.\n\n\
Meanwhile, you'll have to find the module name elsewhere.");
      DoMessageDialog(GetParent(), msg);
   }
   
   myModule = wxAscii(myModuleCombo->GetValue().c_str());
   UpdateSensitivity();
   SelectInListBox();
}

wxString MakeModuleBasicsPage::ListSelectedText()
{
   long sel = myCVSROOTList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (sel == -1)
      return wxT("");
   else
      return myCVSROOTList->GetItemText(sel);
}

void MakeModuleBasicsPage::ListSelected(wxCommandEvent&)
{
   wxString sel = ListSelectedText();

   if (!sel.empty())
   {
      myCVSRoot.SetCVSROOT(wxAscii(sel.c_str()));
      UpdateCVSRoot(myCVSROOTList);
   }
}

void MakeModuleBasicsPage::SetCVSRoot(const CVSRoot& root)
{
   myCVSROOTText->SetValue(wxText(root.GetCVSROOT()));
   wxCommandEvent e;
   RootChanged(e);
}

void MakeModuleBasicsPage::FetchCachedModuleList()
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


void MakeModuleBasicsPage::UpdateCVSRoot(wxWindow* source)
{
   if (m_Updating)
      return;

   m_Updating = true;
   if (source != myCVSROOTList)
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

   if (myCVSRoot.Valid())
      FetchCachedModuleList();

   UpdateSensitivity();
   m_Updating = false;
}


void MakeModuleBasicsPage::UpdateModule(wxWindow* source)
{
   if (m_Updating)
      return;

   m_Updating = true;

   if (source != myModuleCombo)
   {
      myModuleCombo->SetValue(wxText(myModule));
   }
   if (source != myCVSROOTList)
   {
      SelectInListBox();
   }

   UpdateSensitivity();
   m_Updating = false;
}
