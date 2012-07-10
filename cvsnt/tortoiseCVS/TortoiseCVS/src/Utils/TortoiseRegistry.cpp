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
#include "TortoiseRegistry.h"
#include "TortoiseException.h"
#include "TortoiseDebug.h"
#include "OsVersion.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include <windows.h>
#include "FixWinDefs.h"
#include <stdio.h>
#include <limits>
#include <sstream>
#include <iomanip>
#include <shlwapi.h>

// Note the lack of a trailing "\" - Windows 95 doesn't like one
const char tortoisePath[] = "Software\\TortoiseCVS";
const char wincvsPath[] = "Software\\WinCVS\\wincvs\\CVS settings";

static HKEY keys_uc[2] = { static_cast<HKEY>(HKEY_CURRENT_USER), static_cast<HKEY>(HKEY_LOCAL_MACHINE) };
static HKEY key_cr = static_cast<HKEY>(HKEY_CLASSES_ROOT);
static HKEY key_cu = static_cast<HKEY>(HKEY_CURRENT_USER);
static HKEY key_lm = static_cast<HKEY>(HKEY_LOCAL_MACHINE);

// static
DWORD TortoiseRegistry::ourFlag64 = 0;



void TortoiseRegistry::Init()
{
    if (WindowsVersionIsXPOrHigher())
        ourFlag64 = KEY_WOW64_32KEY;
}

std::string TortoiseRegistry::ReadString(const std::string& name,
                                         const std::string& defaultValue,
                                         bool* exists)
{
    if (exists)
        *exists = false;

    std::string sKey;
    std::string sName;
    HKEY *rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(name, rootKeys, &rootKeyCount, sKey, sName);

    DWORD len;
    DWORD type;
    for (int k = 0; k < rootKeyCount; k++)
    {
        HKEY key;
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) != ERROR_SUCCESS)
            continue;

        len = 2048;
        std::auto_ptr<unsigned char> buf(new unsigned char[len]);
        long res = RegQueryValueExA(key, sName.c_str(), NULL, &type, buf.get(), &len);
        if (res == ERROR_MORE_DATA)
        {
            buf.reset(new unsigned char[len]);
            res = RegQueryValueExA(key, sName.c_str(), NULL, &type, buf.get(), &len);
        }

        RegCloseKey(key);
        if (res != ERROR_SUCCESS || ((type != REG_SZ) && (type != REG_EXPAND_SZ)))
            continue;

        if (exists)
            *exists = true;

        std::string result(reinterpret_cast<char*>(buf.get()));
        if (type == REG_EXPAND_SZ)
            return ExpandEnvStrings(result);  
        return result;
    }
    return defaultValue;
}

#ifndef POSTINST

wxString TortoiseRegistry::ReadWxString(const std::string& name,
                                        const wxString& defaultValue,
                                        bool* exists)
{
#if wxUSE_UNICODE
    if (exists)
        *exists = false;

    std::string sKey;
    std::string sName;
    HKEY *rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(name, rootKeys, &rootKeyCount, sKey, sName);

    DWORD len;
    DWORD type;
    for (int k = 0; k < rootKeyCount; k++)
    {
        HKEY key;
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) != ERROR_SUCCESS)
            continue;

        wxString wideName(wxText(sName));
        len = 2048;
        std::auto_ptr<unsigned char> buf(new unsigned char[len]);
        long res = RegQueryValueExW(key, wideName.c_str(), NULL, &type, buf.get(), &len);
        if (res == ERROR_MORE_DATA)
        {
            buf.reset(new unsigned char[len]);
            res = RegQueryValueExW(key, wideName.c_str(), NULL, &type, buf.get(), &len);
        }

        RegCloseKey(key);
        if (res != ERROR_SUCCESS || ((type != REG_SZ) && (type != REG_EXPAND_SZ)))
            continue;

        if (exists)
            *exists = true;

        wxString result(reinterpret_cast<wxChar*>(buf.get()));
        if (type == REG_EXPAND_SZ)
            return ExpandEnvStrings(result);  
        return result;
    }
    return defaultValue;
#else
    return ReadString(name, defaultValue, exists);
#endif
}

#endif

void TortoiseRegistry::WriteString(const std::string& name, const std::string& value)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(name, rootKey, 0, sKey, sName);
    HKEY key;
    if (RegCreateKeyExA(*rootKey, sKey.c_str(), 0, 0, 0, KEY_WRITE | ourFlag64, 0, &key, 0) != ERROR_SUCCESS)
        return;

    RegSetValueExA(key, sName.c_str(), 0,
                   REG_SZ, reinterpret_cast<const unsigned char*>(value.c_str()),
                   static_cast<DWORD>(value.size() + 1)); // include NUL in length for WinNT
    RegCloseKey(key);
}

#ifndef POSTINST

#if wxUSE_UNICODE

void TortoiseRegistry::WriteString(const std::string& name, const wxString& value)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(name, rootKey, 0, sKey, sName);
    HKEY key;
    if (RegCreateKeyExA(*rootKey, sKey.c_str(), 0, 0, 0, KEY_WRITE | ourFlag64, 0, &key, 0) != ERROR_SUCCESS)
        return;

    RegSetValueExW(key, wxText(sName).c_str(), 0,
                   REG_SZ, reinterpret_cast<const unsigned char*>(value.c_str()),
                   static_cast<DWORD>(sizeof(wchar_t)*(value.size() + 1))); // include NUL in length for WinNT
    RegCloseKey(key);
}

#endif

#endif


bool TortoiseRegistry::ReadInteger(const std::string& name, int& value)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(name, rootKeys, &rootKeyCount, sKey, sName);
   
    DWORD dwvalue;
    DWORD len;
    DWORD type;
    for (int k = 0; k < rootKeyCount; k++)
    {
        HKEY key;
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) != ERROR_SUCCESS)
            continue;

        len = sizeof(dwvalue);
        DWORD res = RegQueryValueExA(key, sName.c_str(), NULL, &type, reinterpret_cast<unsigned char*>(&dwvalue), &len);
        RegCloseKey(key);
        if ((res != ERROR_SUCCESS) || (type != REG_DWORD))
            continue;

        value = static_cast<int>(dwvalue);
        return true;
    }
    return false;
}
   

void TortoiseRegistry::WriteInteger(const std::string& name, int value)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(name, rootKey, 0, sKey, sName);
    HKEY key;
    if (RegCreateKeyExA(*rootKey, sKey.c_str(), 0, 0, 0, KEY_WRITE | ourFlag64, NULL, &key, NULL) != ERROR_SUCCESS)
        return;

    RegSetValueExA(key, sName.c_str(), 0, REG_DWORD, (unsigned char*) &value, sizeof(value));

    RegCloseKey(key);
}

bool TortoiseRegistry::ReadBoolean(const std::string& name, bool defaultValue)
{
    bool exists;
    return ReadBoolean(name, defaultValue, exists);
}

bool TortoiseRegistry::ReadBoolean(const std::string& name, bool defaultValue, bool& exists)
{
    int value = 0;
    exists = ReadInteger(name, value);
    return (exists ? value : defaultValue) ? true : false;
}

void TortoiseRegistry::WriteBoolean(const std::string& name, bool value)
{
    WriteInteger(name, value ? 1 : 0);
}

bool TortoiseRegistry::ReadVector(const std::string& name,
                                  std::vector<std::string>& strings)
{
    std::string sKey;
    std::string sName;
    HKEY* rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(name, rootKeys, &rootKeyCount, sKey, sName);

    std::auto_ptr<unsigned char> buf;

    DWORD len = 0;
    DWORD type;
    for (int k = 0; k < rootKeyCount; k++)
    {
        HKEY key;
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) != ERROR_SUCCESS)
            continue;

        len = 2048;
        buf.reset(new unsigned char[len]);
        long res = RegQueryValueExA(key, sName.c_str(), NULL, &type, buf.get(), &len);
        if (res == ERROR_MORE_DATA)
        {
            buf.reset(new unsigned char[len]);
            res = RegQueryValueExA(key, sName.c_str(), NULL, &type, buf.get(), &len);
        }

        RegCloseKey(key);
        if ((res != ERROR_SUCCESS) || (type != REG_MULTI_SZ))
        {
            buf.reset(0);
            continue;
        }
        break;
    }

    if (!buf.get())
        return false;

    const char* p = reinterpret_cast<const char*>(buf.get());
    int i = 0;
    while (i < MAX_TORTOISE_REGISTRY_VECTOR_SIZE)
    {
        size_t thislen = strlen(p);
        if (!thislen && !*(p+1))        // List is empty
            break;
        strings.push_back(std::string(p, thislen));
        p += thislen+1;
        len -= static_cast<DWORD>(thislen+1);
        if (len <= 1)
            break;
        ++i;
    }
    return true;
}


bool TortoiseRegistry::ReadVector(std::string const& name,
                                  std::vector<int>& ints)
{
    std::string sKey;
    std::string sName;
    HKEY* rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(name, rootKeys, &rootKeyCount, sKey, sName);

    std::auto_ptr<unsigned char> buf;

    DWORD len = 0;
    DWORD type;
    for (int k = 0; k < rootKeyCount; k++)
    {
        HKEY key;
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) != ERROR_SUCCESS)
            continue;

        len = 2048;
        buf.reset(new unsigned char[len]);
        long res = RegQueryValueExA(key, sName.c_str(), NULL, &type, buf.get(), &len);
        if (res == ERROR_MORE_DATA)
        {
            buf.reset(new unsigned char[len]);
            res = RegQueryValueExA(key, sName.c_str(), NULL, &type, buf.get(), &len);
        }

        RegCloseKey(key);
        if ((res != ERROR_SUCCESS) || (type != REG_BINARY))
        {
            buf.reset(0);
            continue;
        }
        break;
    }

    if (!buf.get())
        return false;

    const int* p = reinterpret_cast<const int*>(buf.get());
    int i = 0;
    while (len && (i < MAX_TORTOISE_REGISTRY_VECTOR_SIZE))
    {
        ints.push_back(*p++);
        len -= sizeof(int);
        ++i;
    }
    return true;
}


void TortoiseRegistry::WriteVector(std::string const& name,
                                   std::vector<std::string> const& strings,
                                   int maxsize)
{
    std::vector<std::string>::const_iterator it;
    int i = 0;
    std::string value;
    for (it = strings.begin(); it != strings.end() && i < maxsize; ++it)
    {
        value += *it;
        value.append(1, '\0');
        ++i;
    }
    value.append(1, '\0');

    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(name, rootKey, 0, sKey, sName);
    HKEY key;
    if (RegCreateKeyExA(*rootKey, sKey.c_str(), 0, 0, 0, KEY_WRITE | ourFlag64, 0, &key, 0) != ERROR_SUCCESS)
        return;

    RegSetValueExA(key, sName.c_str(), 0,
                   REG_MULTI_SZ, reinterpret_cast<const unsigned char*>(value.c_str()),
                   static_cast<DWORD>(value.size()));

    RegCloseKey(key);
}


void TortoiseRegistry::PushBackVector(std::string const& basekey,
                                      const std::string& value)
{
    // Read existing values
    std::vector<std::string> values;
    ReadVector(basekey, values);

    // Remove existing values from vector
    std::vector<std::string>::iterator it = values.begin();
    while (it != values.end())
    {
        if (*it == value)
            it = values.erase(it);
        else
            ++it;
    }

    // Add new value
    values.push_back(value);
    // Remove first (oldest) value if necessary
    while (values.size() > MAX_TORTOISE_REGISTRY_VECTOR_SIZE)
        values.erase(values.begin());
    // Write back
    WriteVector(basekey, values);
}


void TortoiseRegistry::InsertFrontVector(std::string const& basekey,
                                         const std::string& value,
                                         unsigned int maxsize)
{
    // Read existing values
    std::vector<std::string> values;
    ReadVector(basekey, values);

    // Remove existing values from vector
    std::vector<std::string>::iterator it = values.begin();
    while (it != values.end())
    {
        if (*it == value)
            it = values.erase(it);
        else
            ++it;
    }

    // Add new value
    values.insert(values.begin(), value);
    // Remove last (oldest) value if necessary
    while (values.size() > maxsize)
        values.pop_back();
    // Write back
    WriteVector(basekey, values);
}


void TortoiseRegistry::WriteVector(std::string const& name,
                                   std::vector<int> const& ints)
{
    std::auto_ptr<unsigned char> buf(new unsigned char[ints.size()*sizeof(int)]);
    int* p = reinterpret_cast<int*>(buf.get());
    for (size_t i = 0;
         i < ints.size() && i < MAX_TORTOISE_REGISTRY_VECTOR_SIZE;
         ++i)
        *p++ = ints[i];

    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(name, rootKey, 0, sKey, sName);
    HKEY key;
    if (RegCreateKeyExA(*rootKey, sKey.c_str(), 0, 0, 0, KEY_WRITE | ourFlag64, 0, &key, 0) != ERROR_SUCCESS)
        return;

    RegSetValueExA(key, sName.c_str(), 0,
                   REG_BINARY, buf.get(),
                   static_cast<DWORD>(ints.size()*sizeof(int)));

    RegCloseKey(key);
}


bool TortoiseRegistry::ReadSize(std::string const& name, int* width, int* height) 
{
    if (!width || !height)
        return false;

    std::string value = TortoiseRegistry::ReadString(name);
    if (value.empty())
        return false;

    std::string::size_type pos = value.find(",");
    if (pos == std::string::npos)
        return false;

    *width = atoi(value.substr(0, pos).c_str());
    *height = atoi(value.substr(pos+1).c_str());
    return true;
}



void TortoiseRegistry::WriteSize(std::string const& name, int width, int height)
{
    std::stringstream ss;
    ss << width << ',' << height << std::ends;

    TortoiseRegistry::WriteString(name, ss.str());
}

std::string TortoiseRegistry::ReadWinCVSString(const std::string& name, const std::string& defaultValue)
{
    HKEY key;
    LONG retval = RegOpenKeyExA(HKEY_CURRENT_USER, wincvsPath, 0, KEY_QUERY_VALUE, &key);
    if (retval != ERROR_SUCCESS)
        return defaultValue;

    char data[MAX_PATH];
    DWORD datasize = MAX_PATH;
    DWORD type;
    DWORD res = RegQueryValueExA(key, name.c_str(), NULL, &type, (unsigned char*)data, &datasize);
    RegCloseKey(key);
    if (res != ERROR_SUCCESS || type != REG_BINARY)
        return defaultValue;

    return std::string(data);
}

void TortoiseRegistry::EraseValue(const std::string& name)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(name, rootKey, 0, sKey, sName);
    HKEY key;
    if (RegOpenKeyExA(*rootKey, sKey.c_str(), 0, KEY_WRITE | ourFlag64, &key) != ERROR_SUCCESS)
        return;

    // Ignore return value
    RegDeleteValueA(key, name.c_str());

    RegCloseKey(key);
}



void TortoiseRegistry::ReadValueNames(const std::string& basekey, std::vector<std::string>& names)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(EnsureTrailingDelimiter(basekey), rootKeys, &rootKeyCount, sKey, sName);
    HKEY key = 0;

    // Open key
    for (int k = 0; k < rootKeyCount; k++)
    {
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) == ERROR_SUCCESS)
            break;
        key = 0;
    }

    if (!key)
        return;

    DWORD dwIndex = 0;
    char buf[2048];
    while (1)
    {
        DWORD len = sizeof(buf);
        if (RegEnumValueA(key, dwIndex, buf, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            names.push_back(buf);
            dwIndex++;
        }
        else
            break;
    }
      
    RegCloseKey(key);
}

#ifndef POSTINST

void TortoiseRegistry::ReadValueNames(const std::string& basekey, std::vector<wxString>& names)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(EnsureTrailingDelimiter(basekey), rootKeys, &rootKeyCount, sKey, sName);
    HKEY key = 0;

    // Open key
    for (int k = 0; k < rootKeyCount; k++)
    {
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) == ERROR_SUCCESS)
            break;
        key = 0;
    }

    if (!key)
        return;

    DWORD dwIndex = 0;
    while (1)
    {
        wxChar buf[2048];
        DWORD len = sizeof(buf);
        if (RegEnumValue(key, dwIndex, buf, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            names.push_back(buf);
            dwIndex++;
        }
        else
            break;
    }
      
    RegCloseKey(key);
}

#endif

void TortoiseRegistry::ReadKeys(const std::string& basekey, std::vector<std::string>& keys)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKeys = 0;
    int rootKeyCount = 0;
    ParseRegistryPath(EnsureTrailingDelimiter(basekey), rootKeys, &rootKeyCount, sKey, sName);
    HKEY key = 0;

    // Open key
    for (int k = 0; k < rootKeyCount; k++)
    {
        if (RegOpenKeyExA(rootKeys[k], sKey.c_str(), 0, KEY_READ | ourFlag64, &key) == ERROR_SUCCESS)
            break;
        key = 0;
    }

    if (!key)
        return;

    DWORD dwIndex = 0;
    while (1)
    {
        char buf[2048];
        DWORD len = sizeof(buf);
        if (RegEnumKeyExA(key, dwIndex, buf, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        {
            keys.push_back(buf);
            dwIndex++;
        }
        else
            break;
    }
      
    RegCloseKey(key);
}



void TortoiseRegistry::EraseKey(const std::string& name)
{
    std::string sKey;
    std::string sName;
    HKEY *rootKey = 0;
    ParseRegistryPath(EnsureTrailingDelimiter(name), rootKey, 0, sKey, sName);
    HKEY key;

    if (RegOpenKeyExA(*rootKey, sKey.c_str(), 0, KEY_WRITE | ourFlag64, &key) != ERROR_SUCCESS)
        return;

    // Ignore return value
    SHDeleteKeyA(key, sName.c_str());

    RegCloseKey(key);
}


bool TortoiseRegistry::ParseRegistryPath(const std::string& path,
                                         HKEY*& rootkeys, int* count, 
                                         std::string& key, 
                                         std::string& name)
{
    std::string sPath = path;
    std::string::size_type p;

    // Complete relative path
    if (sPath.substr(0, 1) != "\\")
    {
        sPath = EnsureTrailingDelimiter(tortoisePath) + sPath;
        if (count)
        {
            rootkeys = keys_uc;
            *count = 2;
        }
        else
            rootkeys = &key_cu;
    }
    else
    {
        sPath = sPath.substr(1);
        // Extract root key
        p = sPath.find_first_of("\\");
        if (p == std::string::npos)
            return false;

        std::string sRootKey = sPath.substr(0, p);
        sPath = sPath.substr(p + 1);

        if (sRootKey == "HKEY_USER_CONFIG")
        {
            if (count)
            {
                rootkeys = keys_uc;
                *count = 2;
            }
            else
                rootkeys = &key_cu;
        }
        else if (sRootKey == "HKEY_CLASSES_ROOT")
        {
            rootkeys = &key_cr;
            if (count)
                *count = 1;
        }
        else if (sRootKey == "HKEY_CURRENT_USER")
        {
            rootkeys = &key_cu;
            if (count)
                *count = 1;
        }
        else if (sRootKey == "HKEY_LOCAL_MACHINE")
        {
            rootkeys = &key_lm;
            if (count)
                *count = 1;
        }
        else
            return false;
    }

    p = sPath.find_last_of("\\");
    if (p == std::string::npos)
    {
        key = sPath;
        name = "";
    }
    else
    {
        key = sPath.substr(0, p);
        name = sPath.substr(p + 1);
    }
    return true;
}
