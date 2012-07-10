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

#ifndef MAKE_ARGS_H
#define MAKE_ARGS_H

#include <windows.h>
#include <string>
#include <vector>

class MakeArgs
{
public:
   MakeArgs(const char* cmdname = 0);
    
   // Add a global CVS option, such as the '-x' in 'cvs -x update -d file'.
   void add_global_option(const std::string& option);

   // Add a local CVS option, such as the '-d' in 'cvs -x update -d file'.
   void add_option(const std::string& option);
    
   // Add a filename argument, such as the 'file' in 'cvs -x update -d file'.
   void add_arg(const std::string& arg);

   // Return the main CVS command, e.g. "update".
   std::string main_command() const;

   // Return specified argument.
   const std::string& arg(unsigned int i) const;

   // Return total number of arguments.
   int argc() const;

   // return arguments as vector
   void get_arguments(unsigned int& argc, char **&argv, std::string &arguments);
      
private:
   // escape an argument 
   std::string escape_argument(const std::string& s);
   // prepare for visual output
   std::string visual_prepare(const std::string& s);

   std::vector<std::string> myOptions;
   std::vector<std::string> myArgs;
   int myMainArg;
   unsigned int myIndex;
   HANDLE myInputHandle;
   HANDLE myOutputHandle;
};


#endif

