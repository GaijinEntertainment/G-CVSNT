// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2003

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
#include "TortoiseStatus.h"
#include "TortoiseDebug.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include <fstream>

//////////////////////////////////////////////////////////////////////////////
// TortoiseDirectoryStatus

// Constructor
TortoiseDirectoryStatus::TortoiseDirectoryStatus(const std::string& dirname)
   : m_Dirname(EnsureTrailingDelimiter(dirname)), m_DirStatus(CVSStatus::STATUS_INCVS_RW),
     m_DirStatusRec(CVSStatus::STATUS_INCVS_RW)

{
}



// Load the status file
void TortoiseDirectoryStatus::Load()
{
   TDEBUG_ENTER("TortoiseDirectoryStatus::Load");
   std::string filename = m_Dirname + "CVS\\TortoiseCVS.Status";
   std::string sLine;
   std::string sName;
   std::string sValue;
   std::string::size_type p;
   std::ifstream in(filename.c_str(), std::ios::in);

   if (!in.good())
   {
      TDEBUG_TRACE("Error opening file " << filename);
      return;
   }

   while (std::getline(in, sLine))
   {
      TDEBUG_TRACE("Line: " << sLine);
      sLine = Trim(sLine);
      // Skip empty lines
      if (sLine.empty())
         continue;
      // Skip comments
      if (sLine.substr(0, 1) == "#")
         continue;

      // Look for '=' delimiter
      p = sLine.find_first_of("=");
      sName = Trim(sLine.substr(0, p)); 
      TDEBUG_TRACE("Name: " << sName);
      sValue = Trim(sLine.substr(p + 1));
      TDEBUG_TRACE("Value: " << sValue);

      if (stricmp(sName.c_str(), "DirStatus") == 0)
      {
         // Set directory status
         m_DirStatus = TextToFileStatus(sValue.c_str(), m_DirStatus);
         continue;
      }
      if (stricmp(sName.c_str(), "DirStatusRecursive") == 0)
      {
         // Set directory status recursive
         m_DirStatusRec = TextToFileStatus(sValue.c_str(), m_DirStatusRec);
         continue;
      }
   }
}


// Save the status file
void TortoiseDirectoryStatus::Save()
{
   TDEBUG_ENTER("TortoiseDirectoryStatus::Save");
   std::string filename = m_Dirname + "CVS\\TortoiseCVS.Status";
   std::ofstream out(filename.c_str(), std::ios::out);

   if (!out.good())
   {
      TDEBUG_TRACE("Error creating file " << filename);
      return;
   }

   // Write DirStatus
   out << "DirStatus=" << FileStatusToText(m_DirStatus) << std::endl;
   TDEBUG_TRACE("DirStatus: " << m_DirStatus);

   // Write DirStatusRec
   out << "DirStatusRecursive=" << FileStatusToText(m_DirStatusRec) << std::endl;
   TDEBUG_TRACE("DirStatusRecursive: " << m_DirStatusRec);
}


// Convert text to filestatus
CVSStatus::FileStatus TortoiseDirectoryStatus::TextToFileStatus(const char* text, 
   CVSStatus::FileStatus defStatus)
{
   TDEBUG_ENTER("TortoiseDirectoryStatus::TextToFileStatus");
   TDEBUG_TRACE("Text: " << text);
   CVSStatus::FileStatus res;

   if (stricmp(text, "Added") == 0)
   {
      res = CVSStatus::STATUS_ADDED;
   }
   else if (stricmp(text, "Changed") == 0)
   {
      res = CVSStatus::STATUS_CHANGED;
   }
   else if (stricmp(text, "Missing") == 0)
   {
      res = CVSStatus::STATUS_MISSING;
   }
   else if (stricmp(text, "Conflict") == 0)
   {
      res = CVSStatus::STATUS_CONFLICT;
   }
   else if (stricmp(text, "Removed") == 0)
   {
      res = CVSStatus::STATUS_REMOVED;
   }
   else if (stricmp(text, "In CVS") == 0)
   {
      res = CVSStatus::STATUS_INCVS_RW;
   }
   else if (stricmp(text, "In CVS Readonly") == 0)
   {
      res = CVSStatus::STATUS_INCVS_RO;
   }
   else if (stricmp(text, "Not in CVS") == 0)
   {
      res = CVSStatus::STATUS_NOTINCVS;
   }
   else if (stricmp(text, "Ignored") == 0)
   {
      res = CVSStatus::STATUS_IGNORED;
   }
   else
   {
      res = defStatus;
   }
   TDEBUG_TRACE("Result " << res);
   return res;
}


// Convert filestatus to text
const char* TortoiseDirectoryStatus::FileStatusToText(CVSStatus::FileStatus status)
{
   TDEBUG_ENTER("TortoiseDirectoryStatus::TextToFileStatus");
   TDEBUG_TRACE("Status: " << status);
   char* res;
   if (status == CVSStatus::STATUS_ADDED)
   {
      res = "Added";
   }
   else if (status == CVSStatus::STATUS_CHANGED)
   {
      res = "Changed";
   }
   else if (status == CVSStatus::STATUS_MISSING)
   {
      res = "Missing";
   }
   else if (status == CVSStatus::STATUS_CONFLICT)
   {
      res = "Conflict";
   }
   else if (status == CVSStatus::STATUS_REMOVED)
   {
      res = "Removed";
   }
   else if (status == CVSStatus::STATUS_INCVS_RW)
   {
      res = "In CVS";
   }
   else if (status == CVSStatus::STATUS_INCVS_RO)
   {
      res = "In CVS Readonly";
   }
   else if (status == CVSStatus::STATUS_NOTINCVS)
   {
      res = "Not in CVS";
   }
   else if (status == CVSStatus::STATUS_IGNORED)
   {
      res = "Ignored";
   }
   else
   {
      res = "In CVS";
   }

   TDEBUG_TRACE(res);
   return res;
}



//////////////////////////////////////////////////////////////////////////////
// TortoiseCommitStatus


// Constructor
TortoiseCommitStatus::TortoiseCommitStatus(const std::string& dirname)
   : m_Dirname(EnsureTrailingDelimiter(dirname))
{
}



// Load the status file
void TortoiseCommitStatus::Load()
{
   TDEBUG_ENTER("TortoiseCommitStatus::Load");
   std::string filename = m_Dirname + "CVS\\TortoiseCVS.CommitStatus";
   std::string sLine;
   std::string slcLine;
   std::ifstream in(filename.c_str(), std::ios::in);
   m_DeselectedFiles.clear();

   if (!in.good())
   {
      TDEBUG_TRACE("Error opening file " << filename);
      return;
   }

   while (std::getline(in, sLine))
   {
      TDEBUG_TRACE("Line: " << sLine);
      sLine = Trim(sLine);
      // Skip empty lines
      if (sLine.empty())
         continue;
      // Skip comments
      if (sLine.substr(0, 1) == "#")
         continue;

      // Check if file still exists in CVS
      CVSStatus::FileStatus status = CVSStatus::GetFileStatus(sLine);

      if (status != CVSStatus::STATUS_NOWHERENEAR_CVS && status != CVSStatus::STATUS_IGNORED
         && status != CVSStatus::STATUS_NOTINCVS)
      {
         slcLine = sLine;
         MakeLowerCase(slcLine);
         m_DeselectedFiles.insert(std::pair<std::string, std::string>(slcLine, sLine));
      }
   }
}


// Save the status file
void TortoiseCommitStatus::Save()
{
   TDEBUG_ENTER("TortoiseCommitStatus::Save");
   std::string filename = m_Dirname + "CVS\\TortoiseCVS.CommitStatus";
   std::ofstream out(filename.c_str(), std::ios::out);

   if (!out.good())
   {
      TDEBUG_TRACE("Error creating file " << filename);
      return;
   }

   std::map<std::string, std::string>::const_iterator it = m_DeselectedFiles.begin();
   while (it != m_DeselectedFiles.end())
   {
      out << it->second << std::endl;
      TDEBUG_TRACE(it->second);
      it++;
   }
}


// Add deselected file
void TortoiseCommitStatus::AddDeselectedFile(const char* filename)
{
   TDEBUG_ENTER("TortoiseCommitStatus::AddDeselectedFile");
   std::string sFile = filename;
   std::string slcFile = filename;
   MakeLowerCase(slcFile);
   m_DeselectedFiles.insert(std::pair<std::string, std::string>(slcFile, sFile));
}



// Check if file is deselected
bool TortoiseCommitStatus::IsFileDeselected(const char* filename)
{
   TDEBUG_ENTER("TortoiseCommitStatus::IsFileDeselected");
   std::string sFile = filename;
   MakeLowerCase(sFile);
   return (m_DeselectedFiles.find(sFile) != m_DeselectedFiles.end());
}
