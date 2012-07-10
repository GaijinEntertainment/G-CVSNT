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

#ifndef CVS_ROOT_H
#define CVS_ROOT_H

#include "../Utils/FixCompilerBugs.h"
#include <string>
#include <sstream>
#include "../Utils/FixWinDefs.h"

// Provides a holder for a CVSROOT variable, storing each
// individual component separately.  You can retrieve the
// or set using the complete CVSROOT.

class CVSRoot
{
public:
   CVSRoot(const std::string& cvsroot);
   CVSRoot();

   void SetCVSROOT(const std::string& cvsroot);
   std::string GetCVSROOT() const;
   void Clear();

   bool myIsModified;

   // Are parameters allowed for this protocol?
   bool AllowProtocolParameters() const;

   // Is a user name allowed for this protocol?
   bool AllowUser() const;

   // Is a port number allowed for this protocol?
   bool AllowPort() const;
   
   // Is a server name mandatory for this protocol?
   bool NeedServer() const;

   // Is a server name allowed for this protocol?
   bool AllowServer() const;
   
   bool Valid() const;

   // Set protocol
   void SetProtocol(const std::string& value); 

   // Set protocol parameters
   void SetProtocolParameters(const std::string& value); 

   // Set user
   void SetUser(const std::string& value); 

   // Set server
   void SetServer(const std::string& value); 

   // Set directory
   void SetDirectory(const std::string& value); 

   // Set port
   void SetPort(const std::string& value); 

   // Get protocol
   std::string GetProtocol() const;

   // Get protocol parameters
   std::string GetProtocolParameters() const;

   // Get user
   std::string GetUser() const;

   // Get server
   std::string GetServer() const;

   // Get directory
   std::string GetDirectory() const;

   // Get port
   std::string GetPort() const;

   // Remove trailing delimiter
   void RemoveTrailingDelimiter();

private:
   std::string myProtocol;
   std::string myProtocolParameters;
   std::string myUser;
   std::string myServer;
   std::string myDirectory;
   std::string myPort;
   std::string myCvsRootString;
};

#endif // CVS_ROOT_H
