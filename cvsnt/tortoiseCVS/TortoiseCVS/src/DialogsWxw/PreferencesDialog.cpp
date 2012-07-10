// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - October 2000

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

#include <wx/notebook.h>
#include <wx/colordlg.h>
#include <wx/ffile.h>

#include "PreferencesDialog.h"
#include "MessageDialog.h"
#include "YesNoDialog.h"
#include <TortoiseAct/TortoiseActVerbs.h>


#include "WxwHelpers.h"
#include "ExtStaticText.h"
#include <CVSGlue/CVSRoot.h>
#include <CVSGlue/CVSStatus.h>
#include <Utils/Preference.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/Translate.h>
#include <Utils/PathUtils.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/ShellUtils.h>
#include <Utils/ProcessUtils.h>
#include <Utils/TortoiseException.h>


enum
{
   PREFDLG_ID_ICONSET_REBUILD,
   PREFDLG_ID_ICONSET_CHANGED,
   // Checkbox on Scope page
   PREFDLG_ID_SCOPE_CHECKBOX,
   // IDs for other controls (buttons, checkboxes, text controls) start from here and go upwards
   PREFDLG_ID_PREF_CTRL_FIRST
};

class FilePickEvtHandler : public wxEvtHandler
{
public:
   FilePickEvtHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnBrowse(wxCommandEvent& event)
   {
      int ix = event.GetId();
   
      wxTextCtrl* text = static_cast<wxTextCtrl*>(myParent->GetPrefControl(ix).myWidget);
      const Preference& pref = myParent->myPrefs.Lookup(myParent->GetPrefControlIndex(ix));
      wxString description = pref.myDescription.c_str();
      wxString wildCard = pref.myFilePickWildCard;

      // Remove ampersands in 'description'
      wxString dialogtitle;
      for (unsigned int i = 0; i < description.size(); ++i)
         if (description[i] != '&')
            dialogtitle += description[i];

      // Do sanity check on path to avoid FNERR_INVALIDFILENAME
      std::string path = wxAscii(text->GetValue().c_str());
      if (!FileExists(path.c_str()))
         path = GetTortoiseDirectory();
      path = RemoveTrailingDelimiter(path);
      wxFileDialog* dlg = new wxFileDialog(0, dialogtitle,
                                           wxText(path),
                                           wxT(""),
                                           wildCard, wxOPEN);

      if (dlg->ShowModal() == wxID_OK)
         text->SetValue(dlg->GetPath());

      DestroyDialog(dlg);
   }

private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

class DirPickEvtHandler : public wxEvtHandler
{
public:
   DirPickEvtHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnBrowse(wxCommandEvent& event)
   {
      int ix = event.GetId();
   
      wxTextCtrl* text = static_cast<wxTextCtrl*>(myParent->GetPrefControl(ix).myWidget);
      const Preference& pref = myParent->myPrefs.Lookup(myParent->GetPrefControlIndex(ix));
      wxString description = pref.myDescription.c_str();
      wxString wildCard = pref.myFilePickWildCard;

      // Remove ampersands in 'description'
      wxString dialogtitle;
      for (unsigned int i = 0; i < description.size(); ++i)
         if (description[i] != '&')
            dialogtitle += description[i];

      wxDirDialog* dlg = new wxDirDialog(0, dialogtitle, text->GetValue().c_str());

      if (dlg->ShowModal() == wxID_OK)
         text->SetValue(dlg->GetPath());

      DestroyDialog(dlg);
   }

private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

class BooleanEventHandler : public wxEvtHandler
{
public:
   BooleanEventHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnCheckboxClicked(wxCommandEvent&)
   {
      myParent->RefreshGhosting();
   }

private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

class ScopeCheckboxEventHandler : public wxEvtHandler
{
public:
   ScopeCheckboxEventHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnCheckboxClicked(wxCommandEvent&)
   {
      wxString title = _("TortoiseCVS - Preferences");
      title += wxT(" [");
      if (myParent->myScopeCheckbox->GetValue())
      {
         title += _("Global");
         myParent->myPrefs.SetScope("");
      }
      else
      {
         title += wxText(myParent->myModule);
         myParent->myPrefs.SetScope(myParent->myRoot + "/" + myParent->myModule);
      }
      for (unsigned int i = 0; i < myParent->myPrefs.GetCount(); ++i)
      {
         myParent->RefreshPreference(i, myParent->myPrefs.Lookup(i));
      }
      title += wxT("]");
      myParent->SetTitle(title);
   }
private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

class EnumerationEventHandler : public wxEvtHandler
{
public:
   EnumerationEventHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnSelect(wxCommandEvent&)
   {
      myParent->RefreshGhosting();
   }

private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

class ColorPickEventHandler : public wxEvtHandler
{
public:
   ColorPickEventHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnClick(wxCommandEvent& event)
   {
      int ix = event.GetId();
   
      wxWindow* win = static_cast<wxWindow*>(myParent->GetPrefControl(ix).myWidget);

      wxColor colour = win->GetForegroundColour();

      wxColourData data;
      data.SetChooseFull(TRUE);
      data.SetColour(colour);

      // Read custom colours
      for (int i = 0; i < 16; i++)
      {
         std::string s = PrintfA("Colours\\Custom %d", i); 
         data.SetCustomColour(i, ColorRefToWxColour(GetIntegerPreference(s)));
      }
   
      wxColourDialog dialog(myParent, &data);
      if (dialog.ShowModal() == wxID_OK)
      {
         win->SetForegroundColour(dialog.GetColourData().GetColour());
         win->Refresh();
      }

      // Store custom colours
      for (int i = 0; i < 16; i++)
      {
         std::string s = PrintfA("Prefs\\Colours\\Custom %d\\", i); 
         TortoiseRegistry::WriteInteger(s,
                                        WxColourToColorRef(dialog.GetColourData().GetCustomColour(i)));
      }
   }

private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};

class CacheEventHandler : public wxEvtHandler
{
public:
   CacheEventHandler(PreferencesDialog* parent)
      : myParent(parent)
   {
   }

   void OnClick(wxCommandEvent& event)
   {
      myParent->ClearCache(event.GetId());
   }

private:
   PreferencesDialog* myParent;

   DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(PreferencesDialog, ExtDialog)
   EVT_BUTTON(PREFDLG_ID_ICONSET_REBUILD, PreferencesDialog::IconsRebuild)
END_EVENT_TABLE()


BEGIN_EVENT_TABLE(FilePickEvtHandler, wxEvtHandler)
   EVT_BUTTON(-1, FilePickEvtHandler::OnBrowse)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(DirPickEvtHandler, wxEvtHandler)
   EVT_BUTTON(-1, DirPickEvtHandler::OnBrowse)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(BooleanEventHandler, wxEvtHandler)
   EVT_COMMAND(-1, wxEVT_COMMAND_CHECKBOX_CLICKED, BooleanEventHandler::OnCheckboxClicked)
END_EVENT_TABLE()
   
BEGIN_EVENT_TABLE(EnumerationEventHandler, wxEvtHandler)
   EVT_COMMAND(-1, wxEVT_COMMAND_CHOICE_SELECTED, EnumerationEventHandler::OnSelect)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(ColorPickEventHandler, wxEvtHandler)
   EVT_BUTTON(-1, ColorPickEventHandler::OnClick)
END_EVENT_TABLE()
   
BEGIN_EVENT_TABLE(CacheEventHandler, wxEvtHandler)
   EVT_BUTTON(-1, CacheEventHandler::OnClick)
END_EVENT_TABLE()
   
BEGIN_EVENT_TABLE(ScopeCheckboxEventHandler, wxEvtHandler)
   EVT_COMMAND(-1, wxEVT_COMMAND_CHECKBOX_CLICKED, ScopeCheckboxEventHandler::OnCheckboxClicked)
END_EVENT_TABLE()


//static
void DoPreferencesDialog(wxWindow* parent, const std::string& dir)
{
   PreferencesDialog* dlg = new PreferencesDialog(parent, dir);

   // Store current icon set selection
   std::string oldIconSet = dlg->myPrefs.Lookup("Icons").myStringData;

   bool ret = dlg->ShowModal() == wxID_OK;

   if (ret)
   {
      for (unsigned int i = 0; i < dlg->myPrefs.GetCount(); ++i)
         dlg->ExtractPreference(dlg->myPrefs.Lookup(i), dlg->myPrefControls[i].myWidget);

      // Store cvsignore content
      if (dlg->myCvsIgnore->IsEnabled() && dlg->myCvsIgnore->IsModified())
      {
         if (dlg->myCvsIgnore->GetValue().Trim().IsEmpty())
            DeleteFileA(dlg->myCvsIgnoreFile.c_str());
         else
         {
             wxFFile file(wxTextCStr(dlg->myCvsIgnoreFile), _T("w"));
             wxCSConv converter(wxT("iso8859-1"));
             if (file.IsOpened())
                 file.Write(dlg->myCvsIgnore->GetValue(), converter);
         }
      }

      // Rebuild icon cache if set has changed
      std::string newIconSet(dlg->myPrefs.Lookup("Icons").myStringData);
      if (oldIconSet != newIconSet)
      {
         dlg->SelectIconSet(newIconSet);
         if (DoYesNoDialog(parent, _(
"You have selected a different icon set - it won't take effect until Windows Explorer is restarted.\n\
You can log off or reboot the system, or TortoiseCVS can simply restart the Explorer process for you.\n\
Do you want to restart the Explorer process?"),
                           true))
            RestartExplorer();
      }
   }

   DestroyDialog(dlg);
}

PreferencesDialog::PreferencesDialog(wxWindow* parent,
                                     const std::string& dir)
   : ExtDialog(parent, -1, _("TortoiseCVS - Preferences"),
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
     myNextId(PREFDLG_ID_PREF_CTRL_FIRST),
     myIconSetCombo(0)
{
    SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

    if (!dir.empty())
    {
       myModule = CVSStatus::CVSRepositoryForPath(dir);
       std::string::size_type slashPos = myModule.find("/");
       if (slashPos != std::string::npos)
          myModule = myModule.substr(0, slashPos);
       myRoot = CVSStatus::CVSRootForPath(dir);
       SetTitle(GetTitle() + wxT(" [") + _("Global") + wxT("]"));
    }
    // OK/Cancel button
    wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
    wxButton* ok = new wxButton(this, wxID_OK, _("OK"));
    ok->SetDefault();
    wxButton* cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
    sizerConfirm->Add(ok, 0, wxGROW | wxALL, 5); 
    sizerConfirm->Add(cancel, 0, wxGROW | wxALL, 5);

    // Make a listbook, and add preferences to pages
    myBook = new wxNotebook(this, -1);
    for (unsigned int i = 0; i < myPrefs.GetCount(); ++i)
    {
        AddPreference(myPrefs.Lookup(i));
    }

    // Add all the pages to the notebook
    std::vector<wxString>::const_iterator it;
    for (it = myPrefs.GetPageOrder().begin(); it != myPrefs.GetPageOrder().end(); ++it)
    {
       std::map<wxString, wxPanel*>::iterator it2;
       for (it2 = myPages.begin(); it2 != myPages.end(); ++it2)
       {
          if (it2->first == *it)
             myBook->AddPage(it2->second, it2->first);
       }
    }

    if (!myModule.empty())
       AddScope();
    AddCvsIgnore();
    
    // Main vertical with controls and buttons in it
    wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
    sizerTop->Add(myBook, 1, wxGROW | wxALL, 8);
    sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxALL, 0);

    // Overall dialog layout settings
    SetAutoLayout(TRUE);
    SetSizer(sizerTop);
    sizerTop->SetSizeHints(this);
    sizerTop->Fit(this);

    RefreshGhosting();

    SetTortoiseDialogPos(this, GetRemoteHandle());

    myIconSet = myPrefs.Lookup("Icons").myStringData.c_str();
}


PreferencesDialog::~PreferencesDialog()
{
}



void PreferencesDialog::AddPreference(const Preference& pref)
{
   // See if page already exists
   wxString pageName = pref.myPage;
   std::map<wxString, wxPanel*>::iterator it = myPages.find(pageName);
   if (it == myPages.end())
   {
      // Page doesn't exist, so add it in
      wxPanel* page = new wxPanel(myBook);
      wxFlexGridSizer* sizer = new wxFlexGridSizer(2, 5, 5);
      page->SetAutoLayout(TRUE);
      page->SetSizer(sizer);
      sizer->AddGrowableCol(1);
      myPages[pageName] = page;
      mySizers[pageName] = sizer;
   }
   // We have the sizer for this page
   wxPanel* page = myPages.find(pageName)->second;
   wxSizer* gridSizer = mySizers.find(pageName)->second;

   int iAlign = wxALL;
   if (!pref.myMessage.empty())
   {
      iAlign = wxLEFT | wxRIGHT | wxTOP;
   }

   wxWindow* widget = 0;
   wxStaticText* label = new wxStaticText(page, -1, ((pref.myDescription + _(":")).c_str()));
   
   gridSizer->Add(label, 1, iAlign | wxALIGN_CENTER_VERTICAL, 5);
   
   switch (pref.myType)
   {
      case Preference::STRING:
      case Preference::PATH:
      {
         wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
         widget = new wxTextCtrl(page, myNextId, wxText(pref.myStringData));
         sizer->Add(widget, 2, wxEXPAND | wxLEFT | wxRIGHT, 0);
         myPrefControls.push_back(PrefControl(myNextId, widget));
         ++myNextId;
         gridSizer->Add(sizer, 1, wxGROW | iAlign, 5);
         break;
      }

      case Preference::FILEPICK:
      {
         wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
         widget = new wxTextCtrl(page, myNextId, wxText(pref.myStringData));
         // We cannot use an accelerator here because there may be more than one
         // of these buttons on a page.
         wxButton* button = new wxButton(page, myNextId+1, _("Browse..."));
         button->SetEventHandler(new FilePickEvtHandler(this));
         sizer->Add(widget, 2, wxEXPAND | wxLEFT | wxRIGHT, 0);
         sizer->Add(button, 0, wxLEFT, 5);

         // Need to use the ID of the button in the PrefControl entry
         myPrefControls.push_back(PrefControl(myNextId+1, widget, button));
         myNextId += 2;

         gridSizer->Add(sizer, 1, wxGROW | iAlign, 5);
         button->SetToolTip(pref.myTooltip.c_str());
         break;
      }

      case Preference::DIRPICK:
      {
         wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
         widget = new wxTextCtrl(page, myNextId, wxText(pref.myStringData));
         wxButton* button = new wxButton(page, myNextId+1, _("Browse..."));
         button->SetEventHandler(new DirPickEvtHandler(this));
         sizer->Add(widget, 2, wxEXPAND | wxLEFT | wxRIGHT, 0);
         sizer->Add(button, 0, wxLEFT, 5);
   
         // Need to use the ID of the button in the PrefControl entry
         myPrefControls.push_back(PrefControl(myNextId+1, widget, button));
         myNextId += 2;

         gridSizer->Add(sizer, 1, wxGROW | iAlign, 5);
         button->SetToolTip(pref.myTooltip.c_str());
         break;
      }
      case Preference::BOOLEAN:
      {
         wxCheckBox* check = new wxCheckBox(page, myNextId, pref.myWideStringData);
         check->SetValue(pref.myBooleanData);
         check->SetEventHandler(new BooleanEventHandler(this));
         widget = check;

         myPrefControls.push_back(PrefControl(myNextId, check));
         ++myNextId;

         gridSizer->Add(check, 1, wxGROW | iAlign, 5);
         break;
      }
      case Preference::ENUMERATION_INT:
      {
         // Add each possibility in
         std::vector<wxString> choices;
         for (unsigned int i = 0; i < pref.myEnumerationPossibilities.size(); ++i)
            choices.push_back(pref.myEnumerationPossibilities[i]);

         // Make widget
         wxChoice* choice = new wxChoice(page, myNextId,
                                         wxDefaultPosition, wxDefaultSize, static_cast<int>(choices.size()), &choices[0]);
         choice->SetEventHandler(new EnumerationEventHandler(this));
         widget = choice;
         myPrefControls.push_back(PrefControl(myNextId, choice));
         ++myNextId;

         // Set default
         choice->SetSelection(pref.myIntData);

         gridSizer->Add(widget, 1, wxGROW | iAlign, 5);
         break;
      }
      case Preference::ENUMERATION_STR_KEY:
      {
         // Add each possibility in
         std::vector<wxString> choices;
         int sel = 0;
         for (unsigned int i = 0; i < pref.myEnumerationPossibilities.size(); ++i)
         {
            choices.push_back(pref.myEnumerationPossibilities[i]);
            if (pref.myEnumerationStrKeys[i] == pref.myStringData)
               sel = i;
         }

         // Make widget
         wxChoice* choice = new wxChoice(page, myNextId,
                                         wxDefaultPosition, wxDefaultSize, static_cast<int>(choices.size()), &choices[0]);
         widget = choice;
         myPrefControls.push_back(PrefControl(myNextId, choice));
         ++myNextId;

         // Set default
         choice->SetSelection(sel);

         gridSizer->Add(widget, 1, wxGROW | iAlign, 5);
         break;
      }
      case Preference::ENUMERATION_ICONSET:
      {
         // Add each possibility in
         myIconSets.clear();
         std::vector<wxString> choices;
         int sel = 0;
         for (unsigned int i = 0; i < pref.myEnumerationStrKeys.size(); ++i)
         {
             std::string s = pref.myEnumerationStrKeys[i];
             myIconSets.push_back(s);
             wxString ws;
             if (s.empty())
                 ws = _("No icons");
             else
                 ws = wxGetTranslation(GetIconSetName(s));
             choices.push_back(ws);
             if (s == pref.myStringData)
                sel = i;
         }
         // Make widget
         wxChoice* widget = new wxChoice(page, PREFDLG_ID_ICONSET_CHANGED,
                                       wxDefaultPosition, wxDefaultSize, static_cast<int>(choices.size()), &choices[0]);
         widget->SetSelection(sel);
         myPrefControls.push_back(PrefControl(myNextId, widget));
         ++myNextId;

         gridSizer->Add(widget, 1, wxGROW | iAlign, 5);
         break;
      }

      case Preference::COLOR:
      {
         wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
         ExtStaticText* text = new ExtStaticText(page, myNextId, _("Sample text"), wxDefaultPosition, wxDefaultSize);
         widget = text;
         text->SetForegroundColour(ColorRefToWxColour(pref.myColor)); 
         text->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
         text->SetThemeSupport(false);
         text->SetAutosize(false);

         wxButton* button = new wxButton(page, myNextId+1, _("Select colour..."));
         button->SetEventHandler(new ColorPickEventHandler(this));
         sizer->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);
         sizer->Add(button, 0, wxLEFT, 5);
   
         // Need to use the ID of the button in the PrefControl entry
         myPrefControls.push_back(PrefControl(myNextId+1, text, button));
         myNextId += 2;

         gridSizer->Add(sizer, 1, wxGROW | iAlign, 5);
         button->SetToolTip(pref.myTooltip.c_str());
         break;
      }

      case Preference::CACHE:
      {
         wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
         widget = new wxButton(page, myNextId, pref.myWideStringData);
         widget->SetEventHandler(new CacheEventHandler(this));
         sizer->Add(widget, 0, wxLEFT, 5);
   
         myPrefControls.push_back(PrefControl(myNextId, widget));
         ++myNextId;

         gridSizer->Add(sizer, 1, wxGROW | iAlign, 5);
         break;
      }
    }

   // Tooltips
   label->SetToolTip(pref.myTooltip.c_str());
   if (widget)
      widget->SetToolTip(pref.myTooltip.c_str());

   // Messages
   if (pref.myMessage.length() > 0)
   {
      ExtStaticText* messageLabel = new ExtStaticText(page, -1, pref.myMessage.c_str());

      messageLabel->SetSize(300, -1);
      gridSizer->Add(0, 1, wxALL | wxALIGN_CENTER_VERTICAL, 0);
      gridSizer->Add(messageLabel, 1,
                     wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_CENTER_VERTICAL | wxGROW, 5);
   }
}


void PreferencesDialog::ExtractPreference(Preference& pref, wxWindow* widget)
{
   switch (pref.myType)
   {
      case Preference::STRING:
      case Preference::DIRPICK:
      case Preference::FILEPICK:
      case Preference::PATH:
      {
         pref.myStringData = wxAscii(static_cast<wxTextCtrl*>(widget)->GetValue().c_str());
         break;
      }
      case Preference::BOOLEAN:
      {
         pref.myBooleanData = static_cast<wxCheckBox*>(widget)->GetValue();
         break;
      }
      case Preference::ENUMERATION_INT:
      {
         pref.myIntData = static_cast<wxChoice*>(widget)->GetSelection();
         break;
      }
      case Preference::ENUMERATION_STR_KEY:
      {
         int sel = static_cast<wxChoice*>(widget)->GetSelection();
         pref.myStringData = pref.myEnumerationStrKeys[sel];
         break;
      }
      case Preference::ENUMERATION_ICONSET:
      {
         pref.myStringData = myIconSets[static_cast<wxChoice*>(widget)->GetSelection()];
         break;
      }
      case Preference::COLOR:
      {
         pref.myColor = WxColourToColorRef(static_cast<wxControl*>(widget)->GetForegroundColour());
         break;
      }
      default:
      {
         // Do nothing
      }
   }
}


void PreferencesDialog::RefreshPreference(int i, const Preference& pref)
{
   switch (pref.myType)
   {
      case Preference::STRING:
      case Preference::PATH:
      case Preference::FILEPICK:
      case Preference::DIRPICK:
      {
         wxTextCtrl* textctrl = dynamic_cast<wxTextCtrl*>(myPrefControls[i].myWidget);
         textctrl->SetValue(wxText(pref.myStringData));
         break;
      }

      case Preference::BOOLEAN:
      {
         wxCheckBox* check = dynamic_cast<wxCheckBox*>(myPrefControls[i].myWidget);
         check->SetValue(pref.myBooleanData);
         break;
      }

      case Preference::ENUMERATION_INT:
      {
         wxChoice* choice = dynamic_cast<wxChoice*>(myPrefControls[i].myWidget);
         choice->SetSelection(pref.myIntData);
      }

      case Preference::ENUMERATION_STR_KEY:
      {
         wxChoice* choice = dynamic_cast<wxChoice*>(myPrefControls[i].myWidget);
         choice->SetStringSelection(wxText(pref.myStringData));
         break;
      }

   default:
       // N/A
       break;
   }
}

void PreferencesDialog::RefreshGhosting()
{
   for (unsigned int i = 0; i < myPrefs.GetCount(); ++i)
   {
      Preference& pref = myPrefs.Lookup(i);
      if (pref.myGhostPreference >= 0)
      {
         ASSERT(myPrefs.Lookup(pref.myGhostPreference).myType == pref.myGhostType);
         switch (pref.myGhostType)
         {
         case Preference::BOOLEAN:
            {
               bool value = static_cast<wxCheckBox*>(myPrefControls[pref.myGhostPreference].myWidget)->GetValue();
               if (!pref.myGhostValue)
                  value = !value;
               myPrefControls[i].myWidget->Enable(value);
               if (myPrefControls[i].myWidgetExtra)
                  myPrefControls[i].myWidgetExtra->Enable(value);
               break;
            }

         case Preference::ENUMERATION_INT:
            {
               int value = static_cast<wxChoice*>(myPrefControls[pref.myGhostPreference].myWidget)->GetSelection();
               bool enabled = (value > pref.myGhostValue);
               myPrefControls[i].myWidget->Enable(enabled);
               if (myPrefControls[i].myWidgetExtra)
                  myPrefControls[i].myWidgetExtra->Enable(enabled);
               break;
            }

         default:
            ASSERT(false);
            break;
         }
      }
   }
}

void PreferencesDialog::IconsRebuild(wxCommandEvent&)
{
   if (!RebuildIcons())
   {
      DoMessageDialog(0, _("Error rebuilding icons. You may not have the required permissions."));
   }
}


// Clear cache
void PreferencesDialog::ClearCache(int ix)
{
   std::string regKey = myPrefs.Lookup(GetPrefControlIndex(ix)).myRegistryKey;
   TortoiseRegistry::EraseKey(regKey);

   DoMessageDialog(this, _("Cache has been cleared"));
}



// Add cvsignore page
void PreferencesDialog::AddCvsIgnore()
{
   wxPanel* page = new wxPanel(myBook);
   wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
   page->SetAutoLayout(TRUE);
   page->SetSizer(sizer);
   myBook->AddPage(page, _("Ignored files"));

   wxString s = _("Per-user list of cvsignore file name patterns");
   s += wxT(":");
   wxStaticText* label = new wxStaticText(page, -1, s);
   sizer->Add(label, 0, wxALIGN_LEFT | wxALL, 5);

   myCvsIgnore = new wxTextCtrl(page, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
   sizer->Add(myCvsIgnore, 1, wxGROW | wxLEFT | wxRIGHT, 5);

   s = _("Hint: To reset the list and include all files, enter a single exclamation mark (\"!\").");
   wxStaticText* label2 = new wxStaticText(page, -1, s.c_str());
   label2->SetForegroundColour(SetForegroundColour(ColorRefToWxColour(GetIntegerPreference("Colour Tip Text"))));
   sizer->Add(label2, 0, wxALIGN_LEFT | wxALL, 5);

   s = _(
"A list of files (or file name patterns like \"*.exe\") that CVS ignores when running certain operations. \
The entries must be separated by whitespace and are appended to the default built-in ignored list. \
A single exclamation mark (\"!\") resets the list. For details refer to the original CVS documentation.");
   myCvsIgnore->SetToolTip(s.c_str());

   // read .cvsignore from homedir
   std::string homeDir;
   bool GotHomeDir = GetHomeDirectory(homeDir);

   if (GotHomeDir)
   {
      // Read .cvsignore file from HOME directory.
      myCvsIgnoreFile = EnsureTrailingDelimiter(homeDir) + ".cvsignore";
      if (FileExists(myCvsIgnoreFile.c_str()))
      {
          wxFFile file(wxTextCStr(myCvsIgnoreFile));
          if (file.IsOpened())
          {
              wxString text;
              wxCSConv converter(wxT("iso8859-1"));
              if (file.ReadAll(&text, converter))
              {
                  myCvsIgnore->SetValue(text);
                  myCvsIgnore->DiscardEdits();
              }
          }
      }
   }
   else
      myCvsIgnore->Enable(FALSE);
}

// Add Scope page
void PreferencesDialog::AddScope()
{
   wxPanel* page = new wxPanel(myBook);
   wxFlexGridSizer* sizer = new wxFlexGridSizer(2, 5, 5);
   page->SetAutoLayout(TRUE);
   page->SetSizer(sizer);
   sizer->AddGrowableCol(1);
   myBook->AddPage(page, _("Scope"));

   wxString msg(_("Preference &scope"));
   msg += _(":");
   wxStaticText* label = new wxStaticText(page, -1, msg);
   const wxChar* tip = _("Whether to set preferences globally or for the current module");
   label->SetToolTip(tip);
   sizer->Add(label, 1, wxLEFT | wxRIGHT | wxTOP | wxALIGN_CENTER_VERTICAL, 5);

   myScopeCheckbox = new wxCheckBox(page, PREFDLG_ID_SCOPE_CHECKBOX, _("Set global preferences"));
   myScopeCheckbox->SetToolTip(tip);
   myScopeCheckbox->SetValue(true);
   myScopeCheckbox->SetEventHandler(new ScopeCheckboxEventHandler(this));
   sizer->Add(myScopeCheckbox, 1, wxGROW | wxLEFT | wxRIGHT | wxTOP, 5);
}
               

PreferencesDialog::PrefControl& PreferencesDialog::GetPrefControl(int id)
{
   std::vector<PrefControl>::iterator it =
      std::find_if(myPrefControls.begin(),
                   myPrefControls.end(),
                   std::bind2nd(std::mem_fun_ref(&PrefControl::CompareId), id));
   if (it == myPrefControls.end())
      TortoiseFatalError(Printf(_("PrefControl index %d not found"), id));
   return *it;
}

int PreferencesDialog::GetPrefControlIndex(int id)
{
   std::vector<PrefControl>::iterator it =
      std::find_if(myPrefControls.begin(),
                   myPrefControls.end(),
                   std::bind2nd(std::mem_fun_ref(&PrefControl::CompareId), id));
   if (it == myPrefControls.end())
      TortoiseFatalError(Printf(_("PrefControl index %d not found"), id));
   return it - myPrefControls.begin();
}

void PreferencesDialog::SelectIconSet(const std::string& iconSet)
{
   static const char* keys[] =
   {
      "NormalIcon",
      "ModifiedIcon",
      "ConflictIcon",
      "DeletedIcon",
      "ReadOnlyIcon",
      "LockedIcon",
      "AddedIcon",
      "IgnoredIcon",
      "UnversionedIcon",
      0
   };
   std::string regBasePath("\\HKEY_CURRENT_USER\\Software\\TortoiseOverlays\\");
   std::string fileBasePath(GetSpecialFolder(CSIDL_PROGRAM_FILES_COMMON));
   fileBasePath = EnsureTrailingDelimiter(fileBasePath) + "TortoiseOverlays\\icons\\";
   fileBasePath += EnsureTrailingDelimiter(iconSet);
   for (int i = 0; keys[i]; ++i)
   {
      std::string key(regBasePath);
      key += keys[i];
      std::string iconPath(fileBasePath);
      iconPath += keys[i];
      iconPath += ".ico";
      TortoiseRegistry::WriteString(key, iconPath);
   }
}