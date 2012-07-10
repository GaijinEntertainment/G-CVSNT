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

#ifndef PREFERENCE_H
#define PREFERENCE_H

#include "FixCompilerBugs.h"

#include <vector>
#include <string>
#include <windows.h>
#include "FixWinDefs.h"

// Represents a single option for the user
class Preference
{
public:
   // Construction
   Preference()
   {
   }
   
   Preference(const wxString& page,
              const wxString& description,
              const wxString& tooltip,
              const std::string& registryKey,
              bool globalOnly = false);
   void SetString(const std::string& defaultValue);
   void SetDirectory(const std::string& defaultValue);
   void SetFile(const std::string& defaultValue, const wxString& wildCard);
   void SetIntEnumeration(const std::vector<wxString>& possibilities, int defaultValue);
   void SetStrKeyEnumeration(const std::vector<wxString>& possibilities, 
                             const std::vector<std::string>& keys,
                             const std::string& defaultValue);
   void SetIconSetEnumeration(const std::vector<std::string>& possibilities, const std::string& defaultValue);
   void SetBoolean(bool defaultValue, const wxString& checkBoxText);
   void SetMessage(const wxString& message);
   void SetPath(const std::string& defaultValue);
   void SetColor(COLORREF color, const wxString& colorText);
   void SetCache(const wxString& caption);

   // The sort of data that the preference is
   enum DATA_TYPE
   {
       STRING,
       DIRPICK, // Type of STRING
       FILEPICK, // Type of STRING
       ENUMERATION_INT,
       ENUMERATION_STR_KEY,
       ENUMERATION_ICONSET,
       BOOLEAN,
       PATH,
       COLOR,
       CACHE
   };

   // Ghosting

   // This preference will only be shown if the specified boolean preference is enabled
   void SetGhostPositive(int ghostPreference);
   // This preference will only be shown if the specified boolean preference is disabled
   void SetGhostNegative(int ghostPreference);
   // This preference will only be shown if the specified enumerated preference is
   // set to a value larger than the one specified
   void SetGhostIfLarger(int ghostPreference, int enumValue);

    // Global properties
    DATA_TYPE    myType;
    wxString     myDescription;
    wxString     myTooltip;
    std::string  myRegistryKey;
    wxString     myPage;
    bool         myGlobalOnly;
   wxString     myMessage;
   int          myGhostPreference;
   DATA_TYPE    myGhostType;
   int          myGhostValue;

   // Data specific to the type
   COLORREF     myColor;
   COLORREF     myDefaultColor;
   std::string  myStringData;
   std::string  myDefaultStringData;
   wxString     myWideStringData;
   wxString     myDefaultWideStringData;
   bool         myBooleanData;
   bool         myDefaultBooleanData;
   int          myIntData;
   int          myDefaultIntData;
   
   std::vector<wxString> myEnumerationPossibilities;
   std::vector<std::string> myEnumerationStrKeys;

   wxString myFilePickWildCard;

   // Storage
   void ReadFromRegistry(const std::string* cvsroot = 0);
   void WriteToRegistry(const std::string* cvsroot = 0);
};

bool GetBooleanPreference(const std::string& name, const std::string& dir = "");

int GetIntegerPreference(const std::string& name, const std::string& dir = "");

std::string GetStringPreference(const std::string& name, const std::string& dir = "");

std::vector<std::string> GetStringListPreference(const std::string& name, const std::string& dir = "");

std::string GetStringPreferenceRootModule(const std::string& name,
                                          const std::string& rootModule);

std::vector<std::string> GetStringListPreferenceRootModule(const std::string& name,
                                                           const std::string& rootModule);

void SetStringPreferenceRootModule(const std::string& name,
                                   const std::string& value,
                                   const std::string& rootModule);

int GetIntegerPreferenceRootModule(const std::string& name,
                                   const std::string& rootModule);


#endif
