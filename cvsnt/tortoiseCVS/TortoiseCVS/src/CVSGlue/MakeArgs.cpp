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
#include "MakeArgs.h"
#include <Utils/TortoiseRegistry.h>
#include <Utils/PathUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/TortoiseException.h>
#include <Utils/Translate.h>
#include <Utils/Preference.h>
#include <cvsgui/cvsgui_process.h>
#include <limits>

MakeArgs::MakeArgs(const char* cmdname)
{
    if (cmdname)
        myOptions.push_back(cmdname);
    else
    {
        std::string exe;
        HKEY key;
        LONG retval = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Cvs\\PServer", 0, KEY_QUERY_VALUE, &key);
        if (retval == ERROR_SUCCESS)
        {
            char data[MAX_PATH];
            DWORD datasize = MAX_PATH;
            DWORD type;
            retval = RegQueryValueExA(key, "InstallPath", 0, &type, reinterpret_cast<unsigned char*>(data), &datasize);
            RegCloseKey(key);
            if (retval == ERROR_SUCCESS)
            {
                exe = EnsureTrailingDelimiter(data);
            }
            exe += "cvs.exe";
            if (FileExists(exe.c_str()))
                myOptions.push_back(exe);
        }
    }
    if (myOptions.empty())
        myOptions.push_back("cvs.exe");

    // This could be a scoped preference, but I don't think anyone wants that.
    int quietness = GetIntegerPreference("Quietness");
    if (quietness == 1)
        myOptions.push_back("-q");
    else if (quietness == 2)
        myOptions.push_back("-Q");
    // otherwise, noisy

    // This could be a scoped preference, but I don't think anyone wants that.
    int compression = GetIntegerPreference("Compression Level");
    if (compression == 1)
        myOptions.push_back("-z3");
    else if (compression == 2)
        myOptions.push_back("-z6");
    else if (compression == 3)
        myOptions.push_back("-z9");
    // otherwise, no compression

    myMainArg = static_cast<int>(myOptions.size());
}

void MakeArgs::add_global_option(const std::string& arg)
{
    if (arg.empty())
        return;
    myOptions.insert(myOptions.begin() + myMainArg, arg);
    myMainArg++;
}

void MakeArgs::add_option(const std::string& arg)
{
    myOptions.push_back(arg);
}

void MakeArgs::add_arg(const std::string& arg)
{
    if (arg.empty())
        return;
    std::string myArg = arg;
    FindAndReplace<std::string>(myArg, "\\", "/");
    myArgs.push_back(myArg);
}


const std::string& MakeArgs::arg(unsigned int i) const
{
    if (i < myOptions.size())
        return myOptions[i];
    i -= static_cast<unsigned int>(myOptions.size());
    return myArgs[i];
}


int MakeArgs::argc() const
{
   return static_cast<int>(myOptions.size() + myArgs.size());
}


std::string MakeArgs::main_command() const
{
   return myOptions[myMainArg];
}


// return arguments as vector
void MakeArgs::get_arguments(unsigned int& argc, char **&argv, std::string &arguments)
{
   argc = 0;
   argv = 0;
   arguments = "";

   // allocate memory for strings
   std::vector< std::pair<std::string, std::string> > vTemp;

   // First add options.
   for (size_t i = 0; i < myOptions.size(); ++i)
   {
      std::string cvsgui_arg = myOptions[i];
      std::string visual_arg = visual_prepare(cvsgui_arg); 
      vTemp.push_back(std::pair<std::string, std::string>(cvsgui_arg, visual_arg));
   }

   // Now add arguments
   size_t n = myArgs.size();
   if (n)
   {
      for (size_t i = 0; i < n; ++i)
      {
         std::string cvsgui_arg = escape_argument(myArgs[i]);
         std::string visual_arg = visual_prepare(cvsgui_arg);
         vTemp.push_back(std::pair<std::string, std::string>(cvsgui_arg, visual_arg));
      }
   }

   // set argc
   argc = static_cast<unsigned int>(vTemp.size());
   // Fill argv
   argv = new char*[argc];
   bool argsTruncated = false;
   for (size_t i = 0; i < argc; i++)
   {
      argv[i] = new char[vTemp[i].first.size() + 1];
      strncpy(argv[i], vTemp[i].first.c_str(), vTemp[i].first.size() + 1);

      if (!argsTruncated)
      {
         if (i != 0)
            arguments += " ";
         arguments += vTemp[i].second;

         if (arguments.length() > 4096)
         {
            arguments = arguments.substr(0, 4096) + "...";
            argsTruncated = true;
         }
      }
   }
}

// escape an argument
std::string MakeArgs::escape_argument(const std::string& s)
{
   std::string result = s;
   if (s[0] == '-')
   {
      result = std::string("./") + result;
   }
   return result;
}

// prepare for visual output
std::string MakeArgs::visual_prepare(const std::string& s)
{
   std::string result = s;
   if (s.find(' ') != std::string::npos)
      result = std::string("\"") + s + std::string("\"");
   FindAndReplace<std::string>(result, "\n", "\\n");
   FindAndReplace<std::string>(result, "\t", "\\t");

   if (result.length() > 1024) 
   {
      result = result.substr(0, 1024) + "...";
   }

   return result;
}
