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
#include "../Utils/TortoiseDebug.h"
#include "CVSRoot.h"
#include "../Utils/PathUtils.h"


static std::string RootProtocol(std::string& cvsroot)
{
   if (!cvsroot.empty())
   {
      // if it starts with a slash or with a drive letter or UNC path, protocol is "local"
      if ((cvsroot[0] == '/') || (cvsroot[0] == '\\'))
      {
         return "local";
      }
      if ((cvsroot.length() >= 3) && isalpha(cvsroot[0]) && (cvsroot[1] == ':') && 
          ((cvsroot[2] == '/') || (cvsroot[2] == '\\')))
      {
         return "local";
      }
      // if it starts with a colon, read the protocol name
      else if (cvsroot[0] == ':')
      {
         std::string::size_type pos = cvsroot.find_first_of(":", 2);
         if (pos != std::string::npos)
         {
            std::string protocol = cvsroot.substr(1, pos - 1);
            cvsroot = cvsroot.substr(pos + 1);
            return protocol;
         }
      }
      else if (cvsroot.find_first_of(":") != std::string::npos)
      {
         return "ext";
      }
   }

   return "";
}

static void CrackProtocol(const std::string& wholeProtocol, std::string& protocol, std::string& protocolParameters)
{
   protocolParameters = "";
   std::string::size_type pos = wholeProtocol.find_first_of(';');
   if (std::string::npos == pos)
   {
      protocol = wholeProtocol;
   }
   else
   {
      protocol = wholeProtocol.substr(0, pos);
      if (pos + 1 < wholeProtocol.length())
      {
         protocolParameters = wholeProtocol.substr(pos + 1);
      }
   }
}

static std::string RootUser(std::string& cvsroot)
{
   std::string::size_type pos = cvsroot.find_first_of("@");
   if (pos == std::string::npos)
   {
      return "";
   }

   std::string user = cvsroot.substr(0, pos);
   cvsroot = cvsroot.substr(pos + 1);
   
   return user;
}

static std::string RootServer(std::string& cvsroot)
{
   std::string::size_type pos = cvsroot.find_first_of(":/");
   if (pos == std::string::npos)
   {
      cvsroot = "";
      return "";
   }

   std::string server = cvsroot.substr(0, pos);
   if (cvsroot[pos] == ':')
      pos++;
   cvsroot = cvsroot.substr(pos);
   
   return server;
}

static std::string RootPort(std::string& cvsroot)
{
   // Port may be separated by a colon
   if (!isdigit(cvsroot[0]) && (cvsroot[0] != ':'))
      return "";

   std::string::size_type last = cvsroot.find_first_not_of("0123456789");
   std::string port;
   if (last != std::string::npos)
   {
      port = cvsroot.substr(0, last);
      if (cvsroot[last] == ':')
         last++;
      cvsroot = cvsroot.substr(last);
   }
   return port;
}

static std::string RootDirectory(std::string& cvsroot)
{
   return cvsroot;
}



CVSRoot::CVSRoot(const std::string& cvsroot) 
{ 
   SetCVSROOT(cvsroot); 
}


CVSRoot::CVSRoot() 
{ 
   myIsModified = false;
}


void CVSRoot::SetCVSROOT(const std::string& cvsroot)
{
   myCvsRootString = cvsroot;
   if (cvsroot.empty())
   {
      Clear();
      return;
   }
   std::string chop(cvsroot);
   std::string wholeProtocol = RootProtocol(chop);
   CrackProtocol(wholeProtocol, myProtocol, myProtocolParameters);
   myUser = RootUser(chop);
   myServer = NeedServer() ? RootServer(chop) : std::string("");
   myPort = RootPort(chop);
   myDirectory = RootDirectory(chop);
   myIsModified = false;
}

std::string CVSRoot::GetCVSROOT() const
{
   if (myIsModified)
   {
      std::ostringstream sout;
      sout << ":" << myProtocol;
      if (AllowProtocolParameters() && !myProtocolParameters.empty())
      {
         sout << ";" << myProtocolParameters;
      }
      sout << ":";

      if (AllowUser() && !myUser.empty())
      {
         sout << myUser << "@";
      }

      if (NeedServer() || (AllowServer() && !myServer.empty()))
      {
         sout << myServer << ":";
      }

      if (AllowPort() && !myPort.empty())
      {
         sout << myPort;
         if (!myDirectory.empty() && myDirectory[0] != '/')
         {
            sout << ":";
         }
      }

      sout << myDirectory;

      return sout.str();
   }
   else
   {
      return myCvsRootString;
   }
}

void CVSRoot::Clear()
{
   myProtocol = "";
   myUser = "";
   myServer = "";
   myDirectory = "";
   myPort = "";
}

bool CVSRoot::AllowProtocolParameters() const
{
   // Nowadays, all protocols accept keywords except the special cases :local: and :ext:
   return (myProtocol != "local") && (myProtocol != "ext");
}

bool CVSRoot::AllowUser() const
{
   return (myProtocol == "pserver" ||
           myProtocol == "ext" ||
           myProtocol == "ssh" ||
           myProtocol == "server" ||
           myProtocol == "sserver" ||
           myProtocol == "sspi");
}

bool CVSRoot::AllowPort() const
{
   return (myProtocol == "pserver" || myProtocol == "sspi" || myProtocol == "gserver"
           || myProtocol == "sserver" || myProtocol == "ssh");
}

bool CVSRoot::NeedServer() const
{
   return (myProtocol != "local");
}

bool CVSRoot::AllowServer() const
{
   return (myProtocol != "local");
}

bool CVSRoot::Valid() const
{
   if (myProtocol.empty())
      return false;
   if (NeedServer() && myServer.empty())
      return false;
   if (myDirectory.empty())
      return false;
   if (!myProtocolParameters.empty() && !AllowProtocolParameters())
      return false;

   return true;
}

// Set protocol
void CVSRoot::SetProtocol(const std::string& value)
{
   myProtocol = value;
   myIsModified = true;
}

// Set protocol parameters
void CVSRoot::SetProtocolParameters(const std::string& value)
{
   myProtocolParameters = value;
   myIsModified = true;
}

// Set user
void CVSRoot::SetUser(const std::string& value)
{
   myUser = value;
   myIsModified = true;
}

// Set server
void CVSRoot::SetServer(const std::string& value)
{
   myServer = value;
   myIsModified = true;
}

// Set directory
void CVSRoot::SetDirectory(const std::string& value)
{
   myDirectory = value;
   myIsModified = true;
}

// Set port
void CVSRoot::SetPort(const std::string& value)
{
   myPort = value;
   myIsModified = true;
}

// Get protocol
std::string CVSRoot::GetProtocol() const
{
   return myProtocol;
}

// Get protocol parameters
std::string CVSRoot::GetProtocolParameters() const
{
   return myProtocolParameters;
}

// Get user
std::string CVSRoot::GetUser() const
{
   return myUser;
}

// Get server
std::string CVSRoot::GetServer() const
{
   return myServer;
}

// Get directory
std::string CVSRoot::GetDirectory() const
{
   return myDirectory;
}

// Get port
std::string CVSRoot::GetPort() const
{
   return myPort;
}


// Remove trailing delimiter
void CVSRoot::RemoveTrailingDelimiter()
{
   ::RemoveTrailingDelimiter(myDirectory);
   ::RemoveTrailingDelimiter(myCvsRootString);
}
