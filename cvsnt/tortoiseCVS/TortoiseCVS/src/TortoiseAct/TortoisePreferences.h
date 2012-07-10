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

#ifndef TORTOISE_PREFERENCES_H
#define TORTOISE_PREFERENCES_H

#include "../Utils/FixCompilerBugs.h"

// A simple vector of preferences, which constructs itself with one
// for each preference in TortoiseCVS.

#include "../Utils/Preference.h"
#include <vector>

class TortoisePreferences
{
public:
   // Setting for "Edit policy"
   enum EditPolicySetting
   {
      EditPolicyNone,
      EditPolicyConcurrent,
      EditPolicyConcurrentExclusive,
      EditPolicyExclusive,
      EditPolicyExclusiveACL
   };

   TortoisePreferences();
   ~TortoisePreferences();

   void SetScope(const std::string& abbreviatedRoot);
   
   size_t GetCount();
   Preference& Lookup(int index);
   Preference& Lookup(const std::string& regKey);

   const std::vector<wxString>& GetPageOrder() const;
   void AddPage(const wxString& page);
      
private:
   void CreateMainPreferences();
   void CreateAdvancedPreferences();
   void CreateToolsPreferences();
   void CreatePolicyPreferences();
   void CreateEditPreferences();
   void CreateDebugPreferences();
   void CreateAppearancePreferences();
   void CreateCachePreferences();

   std::vector<Preference*>     myPrefs;
   // Determines order of notebook pages
   std::vector<wxString>        myPageOrder;
   std::string                  myAbbreviatedRoot;
};

#endif
