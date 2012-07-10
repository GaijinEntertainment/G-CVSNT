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

#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <string>
#include <vector>
#include <map>

#include "../TortoiseAct/TortoisePreferences.h"
#include <wx/wx.h>
#include "ExtDialog.h"

class Preference;
class wxNotebook;

class PreferencesDialog : public ExtDialog
{
public:
   // Allow the user to edit all the preferences passed in.
   // Changes their values if they click OK.
   friend void DoPreferencesDialog(wxWindow* parent, 
                                   const std::string& dir);
   
private:
   PreferencesDialog(wxWindow* parent,
                     const std::string& dir);
   ~PreferencesDialog();

   void AddPreference(const Preference& pref);
   void ExtractPreference(Preference& pref, wxWindow* widget);
   void RefreshPreference(int i, const Preference& pref);
   void AddScope();
   void AddCvsIgnore();

   void RefreshGhosting();
   void IconsRebuild(wxCommandEvent& event);
   void ClearCache(int ix);
   void SelectIconSet(const std::string& iconSet);

   struct PrefControl
   {
      PrefControl(int id, wxWindow* widget, wxWindow* extrawidget = 0)
         : myId(id), myWidget(widget), myWidgetExtra(extrawidget)
      {
      }

      bool CompareId(int id) const
      {
         return id == myId;
      }

      // ID
      int       myId;
      // Control
      wxWindow* myWidget;
      // Auxiliary control (optional)
      wxWindow* myWidgetExtra;
   };

   // Given a control ID, return the corresponding PrefControl
   PrefControl& GetPrefControl(int id);
   
   // Given a control ID, return the index of the corresponding PrefControl
   int GetPrefControlIndex(int id);

   TortoisePreferences          myPrefs;
   // List of controls
   std::vector<PrefControl>     myPrefControls;

   // Map of all notebook pages
   std::map<wxString, wxPanel*> myPages;
   std::map<wxString, wxSizer*> mySizers;
   wxNotebook *myBook;
   wxTextCtrl *myCvsIgnore;
   std::string myCvsIgnoreFile;
   wxCheckBox* myScopeCheckbox;
   wxChoice*   myIconSetCombo;

   // Next available ID for controls
   int          myNextId;

   std::string  myModule;
   std::string  myRoot;

   // Special variables for Icon Set
   std::string  myIconSet;
   std::vector<std::string> myIconSets;
   
   // Event handlers
   friend class FilePickEvtHandler;
   friend class DirPickEvtHandler;
   friend class BooleanEventHandler;
   friend class ScopeCheckboxEventHandler;
   friend class EnumerationEventHandler;
   friend class ColorPickEventHandler;
   friend class CacheEventHandler;

   DECLARE_EVENT_TABLE();
};

#endif

