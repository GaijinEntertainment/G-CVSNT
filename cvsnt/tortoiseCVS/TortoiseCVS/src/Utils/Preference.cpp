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
#include "Preference.h"
#include "TortoiseRegistry.h"
#include "StringUtils.h"
#include "Translate.h"
#include <CVSGlue/CVSStatus.h>
#include <Utils/PathUtils.h>


// Preference storage in registry:
//
// Global prefs are stored in
// HKCU/Software/TortoiseCVS/Prefs/<name>/_
//
// Per-module prefs are stored in
// HKCU/Software/TortoiseCVS/Prefs/<name>/<root-and-module>
//
// where <root-and-module> is
//
// <CVSROOT> '/' <module>


Preference::Preference(const wxString& page,
                       const wxString& description, 
                       const wxString& tooltip,
                       const std::string& registryKey,
                       bool globalOnly)
   : myDescription(description),
     myTooltip(tooltip),
     myRegistryKey(registryKey),
     myPage(page),
     myGlobalOnly(globalOnly),
     myGhostPreference(-1),
     myGhostType(STRING),
     myGhostValue(0)
{
}

void Preference::SetString(const std::string& defaultValue)
{
   myType = STRING;
   myStringData = myDefaultStringData = defaultValue;
}

void Preference::SetDirectory(const std::string& defaultValue)
{
   myType = DIRPICK;
   myStringData = defaultValue;
}

void Preference::SetFile(const std::string& defaultValue, const wxString& wildCard)
{
    myType = FILEPICK;
    myStringData = myDefaultStringData = defaultValue;
    myFilePickWildCard = wildCard;
}

void Preference::SetIntEnumeration(const std::vector<wxString>& possibilities,
                                   int defaultValue)
{
   myType = ENUMERATION_INT;
   myEnumerationPossibilities = possibilities;
   myIntData = myDefaultIntData = defaultValue;
}


void Preference::SetStrKeyEnumeration(const std::vector<wxString>& possibilities,
                                      const std::vector<std::string>& keys,
                                      const std::string& defaultValue)
{
   myType = ENUMERATION_STR_KEY;
   myEnumerationPossibilities = possibilities;
   myEnumerationStrKeys = keys;
   myStringData = defaultValue;
   myDefaultStringData = defaultValue;
}



void Preference::SetIconSetEnumeration(const std::vector<std::string>& possibilities,
                                       const std::string& defaultValue)
{
   myType = ENUMERATION_ICONSET;
   myEnumerationStrKeys = possibilities;
   myStringData = defaultValue;
   myDefaultStringData = defaultValue;
}

void Preference::SetBoolean(bool defaultValue, const wxString& checkBoxText)
{
   myType = BOOLEAN;
   myBooleanData = myDefaultBooleanData = defaultValue;
   myWideStringData = checkBoxText;
}


void Preference::SetMessage(const wxString& message)
{
   myMessage = message;
}



void Preference::SetPath(const std::string& defaultValue)
{
   myType = PATH;
   myStringData = defaultValue;
}



void Preference::SetColor(COLORREF color, const wxString& colorText)
{
   myType = COLOR;
   myColor = myDefaultColor = color;
   myWideStringData = colorText;
}



void Preference::SetCache(const wxString& caption)
{
   myType = CACHE;
   myWideStringData = caption;
}


void Preference::ReadFromRegistry(const std::string* cvsroot)
{
    std::string globalKey = "Prefs\\";
    globalKey += EnsureTrailingDelimiter(myRegistryKey);
    std::string key(globalKey);
    globalKey += "_";
    if (!myGlobalOnly && cvsroot)
        key += *cvsroot;
    else
        key += "_";
   
   switch(myType)
   {
   case STRING:
   case DIRPICK:
   case FILEPICK:
       myStringData = TortoiseRegistry::ReadString(key, myDefaultStringData);
       break;
   case ENUMERATION_INT:
       if (!TortoiseRegistry::ReadInteger(key, myIntData))
           myIntData = myDefaultIntData;
       break;
   case ENUMERATION_STR_KEY:
   {
       myStringData = TortoiseRegistry::ReadString(key, myDefaultStringData);
       std::vector<std::string>::iterator it = myEnumerationStrKeys.begin();
       bool bFound = false;
       while (it != myEnumerationStrKeys.end())
       {
           if (*it == myStringData)
           {
               bFound = true;
               break;
           }
           it++;
       }
       if (!bFound)
           myStringData = myDefaultStringData;;
       break;
   }
   case ENUMERATION_ICONSET:
   {
       myStringData = TortoiseRegistry::ReadString(globalKey, myDefaultStringData);
       // Make sure the icon set exists
       std::vector<std::string>::iterator it = myEnumerationStrKeys.begin();
       bool bFound = false;
       while (it != myEnumerationStrKeys.end())
       {
           if (*it == myStringData)
           {
               bFound = true;
               break;
           }
           it++;
       }
       if (!bFound)
           myStringData = myDefaultStringData;
       break;
   }
   case BOOLEAN:
       myBooleanData = TortoiseRegistry::ReadBoolean(key, myDefaultBooleanData);
       break;

   case PATH:
   {
       std::vector<std::string> v;
       myStringData = "";
       TortoiseRegistry::ReadVector(key, v);
       std::vector<std::string>::iterator it = v.begin();
       while (it != v.end())
       {
           if (!myStringData.empty())
               myStringData += ";";
           myStringData += Trim(*it);
           it++;
       }
       break;
   }
   case COLOR:
      {
         // Colours are always global
         int value;
         if (!TortoiseRegistry::ReadInteger(globalKey, value))
             value = myDefaultColor;
         myColor = value;
         break;
      }

   case CACHE:
      break;
   }
}

void Preference::WriteToRegistry(const std::string* cvsroot)
{
    std::string globalKey = "Prefs\\";
    globalKey += EnsureTrailingDelimiter(myRegistryKey);
    std::string key(globalKey);
    globalKey += "_";
    if (!myGlobalOnly && cvsroot)
        key += *cvsroot;
    else
        key += "_";

    switch(myType)
    {
    case STRING:
    case DIRPICK:
    case FILEPICK:
        TortoiseRegistry::WriteString(key, myStringData);
        break;
    case ENUMERATION_INT:
        TortoiseRegistry::WriteInteger(key, myIntData);
        break;
    case ENUMERATION_STR_KEY:
        TortoiseRegistry::WriteString(key, myStringData);
        break;
    case ENUMERATION_ICONSET:
        TortoiseRegistry::WriteString(globalKey, myStringData);
        break;
    case BOOLEAN:
        TortoiseRegistry::WriteBoolean(key, myBooleanData);
        break;
    case PATH:
    {
        std::vector<std::string> v;
        std::string s = myStringData;
        while (1)
        {
            std::string s1 = Trim(CutFirstToken(s, ";"));
            if (s1.empty())
                break;
            v.push_back(s1);
        }
        TortoiseRegistry::WriteVector(key, v);
        break;
    }
    case COLOR:
        // Colours are always global
        TortoiseRegistry::WriteInteger(globalKey, myColor);
        break;

    case CACHE:
        break;
    }
}

void Preference::SetGhostPositive(int ghostPreference)
{
   myGhostPreference = ghostPreference;
   myGhostType = BOOLEAN;
   myGhostValue = 1;
}

void Preference::SetGhostNegative(int ghostPreference)
{
   myGhostPreference = ghostPreference;
   myGhostType = BOOLEAN;
   myGhostValue = 0;
}

void Preference::SetGhostIfLarger(int ghostPreference, int enumValue)
{
   myGhostPreference = ghostPreference;
   myGhostType = ENUMERATION_INT;
   myGhostValue = enumValue;
}


bool GetBooleanPreference(const std::string& name,
                          const std::string& dir)
{
   return GetIntegerPreference(name, dir) ? true : false;
}

int GetIntegerPreference(const std::string& name,
                         const std::string& dir)
{
    std::string rootModule;
    if (!dir.empty())
    {
        std::string root = CVSStatus::CVSRootForPath(dir);
        std::string module = CVSStatus::CVSRepositoryForPath(dir);
        std::string::size_type slashPos = module.find("/");
        if (slashPos != std::string::npos)
            module = module.substr(0, slashPos);
        rootModule = root + "/" + module;
    }
    return GetIntegerPreferenceRootModule(name, rootModule);
}

std::string GetStringPreference(const std::string& name,
                                const std::string& dir)
{
    std::string rootModule;
    if (!dir.empty())
    {
        std::string root = CVSStatus::CVSRootForPath(dir);
        std::string module = CVSStatus::CVSRepositoryForPath(dir);
        std::string::size_type slashPos = module.find("/");
        if (slashPos != std::string::npos)
            module = module.substr(0, slashPos);
        rootModule = root + "/" + module;
    }
    return GetStringPreferenceRootModule(name, rootModule);
}

std::vector<std::string> GetStringListPreference(const std::string& name, const std::string& dir)
{
    std::string rootModule;
    if (!dir.empty())
    {
        std::string root = CVSStatus::CVSRootForPath(dir);
        std::string module = CVSStatus::CVSRepositoryForPath(dir);
        std::string::size_type slashPos = module.find("/");
        if (slashPos != std::string::npos)
            module = module.substr(0, slashPos);
        rootModule = root + "/" + module;
    }
    return GetStringListPreferenceRootModule(name, rootModule);
}


int GetIntegerPreferenceRootModule(const std::string& name,
                                   const std::string& rootModule)
{
    std::string globalKey("Prefs\\");
    globalKey += EnsureTrailingDelimiter(name);
    std::string localKey(globalKey);
    globalKey += "_";
    localKey += rootModule;
    int value = 0;
    if (rootModule.empty() || !TortoiseRegistry::ReadInteger(localKey, value))
        TortoiseRegistry::ReadInteger(globalKey, value);
    return value;
}

std::string GetStringPreferenceRootModule(const std::string& name,
                                          const std::string& rootModule)
{
    std::string globalKey("Prefs\\");
    globalKey += EnsureTrailingDelimiter(name);
    std::string localKey(globalKey);
    globalKey += "_";
    if (!rootModule.empty())
    {
        localKey += rootModule;
        std::string localValue = TortoiseRegistry::ReadString(localKey);
        if (!localValue.empty())
            return localValue;
    }
    return TortoiseRegistry::ReadString(globalKey);
}

std::vector<std::string> GetStringListPreferenceRootModule(const std::string& name,
                                                           const std::string& rootModule)
{
    std::string globalKey("Prefs\\");
    globalKey += EnsureTrailingDelimiter(name);
    std::string localKey(globalKey);
    globalKey += "_";
    std::vector<std::string> value;
    if (!rootModule.empty())
    {
        localKey += rootModule;
        if (TortoiseRegistry::ReadVector(globalKey, value))
            return value;
    }
    TortoiseRegistry::ReadVector(globalKey, value);
    return value;
}

void SetStringPreferenceRootModule(const std::string& name,
                                   const std::string& value,
                                   const std::string& rootModule)
{
    std::string globalKey("Prefs\\");
    globalKey += EnsureTrailingDelimiter(name);
    std::string localKey(globalKey);
    globalKey += "_";
    localKey += rootModule;
    if (rootModule.empty())
        TortoiseRegistry::WriteString(globalKey, value);
    else
        TortoiseRegistry::WriteString(localKey, value);
}
