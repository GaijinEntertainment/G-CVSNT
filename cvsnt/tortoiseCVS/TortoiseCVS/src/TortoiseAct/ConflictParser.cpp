// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - January 2001

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
#include <fstream>
#include <iostream>

#include "ConflictParser.h"
#include <Utils/PathUtils.h>
#include <Utils/LineReader.h>
#include <Utils/SandboxUtils.h>

// Parse a file
bool ConflictParser::ParseFile(const std::string& sConflictFileName, 
                               const std::string& sWorkingCopyFileName, 
                               const std::string& sNewRevisionFileName,
                               bool& bNestedConflicts)
{
   std::ifstream ifConflictFile;
   std::ofstream ofWorkingCopy;
   std::ofstream ofNewRevision;
   std::string line;
   std::string::size_type pos;
   int state;
   int iNestingLevel = 0;
   bool bResult = false;
   bool bFirstLineNR = true;
   bool bFirstLineWC = true;
   std::string sRevision = "none";
   bNestedConflicts = false;


   // open input file
   ifConflictFile.open(sConflictFileName.c_str(), std::ios::binary);

   // Create output files
   ofWorkingCopy.open(sWorkingCopyFileName.c_str(), std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
   ofNewRevision.open(sNewRevisionFileName.c_str(), std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);

   state = 0;
   LineReader<std::string> lr(&ifConflictFile, LineReader<std::string>::CrlfStyleDos);
   std::string newline = "\r\n";
   if (IsUnixSandbox(StripLastPart(sConflictFileName)))
   {
      lr.SetCrlfStyle(LineReader<std::string>::CrlfStyleUnix);
      newline = "\n";
   }
   while(lr.ReadLine(line))
   {
      switch(state)
      {
      // in common section
      case 0:
         // search beginning of conflict section
         pos = line.find("<<<<<<< ");
         if(pos == 0)
         {
            // working copy section starts
            state = 1;
            bResult = true;
         }
         else
         {
            // we're in the common section, so write to both files
            if(!bFirstLineNR)
               ofNewRevision << newline;
            else
               bFirstLineNR = false;
            ofNewRevision << line;

            if(!bFirstLineWC)
               ofWorkingCopy << newline;
            else
               bFirstLineWC = false;
            ofWorkingCopy << line;
         }
         break;

      // in working copy section
      case 1:
         // search beginning of conflict section
         pos = line.find("<<<<<<< ");
         if(pos == 0)
         {
            // nested conflict section starts
            state = 3;
            if(!bFirstLineWC)
               ofWorkingCopy << newline;
            else
               bFirstLineWC = false;
            ofWorkingCopy << line;
         }
         else
         {
            pos = line.find("=======");
            if((pos != std::string::npos) && (pos == (line.length() - 7)))
            {
               line = line.substr(0, pos);
               if(!line.empty())
               {
                  if(!bFirstLineWC)
                     ofWorkingCopy << newline;
                  else
                     bFirstLineWC = false;
                  ofWorkingCopy << line;
               }

               //  new revision section
               state = 2;
            }
            else
            {
               if(!bFirstLineWC)
                  ofWorkingCopy << newline;
               else
                  bFirstLineWC = false;
               ofWorkingCopy << line;
            }
         }
         break;

      // in new revision section
      case 2:
         // search beginning of nested conflict section
         pos = line.find("<<<<<<< ");
         if(pos == 0)
         {
            // nested conflict section starts
            state = 4;
            if(!bFirstLineNR)
               ofNewRevision << newline;
            else
               bFirstLineNR = false;
            ofNewRevision << line;
         }
         else
         {
            pos = line.find(">>>>>>> ");
            if(pos != std::string::npos)
            {
               sRevision = line.substr(pos + 8);
               line = line.substr(0, pos);
               if(!line.empty())
               {
                  if(!bFirstLineNR)
                     ofNewRevision << newline;
                  else
                     bFirstLineNR = false;
                  ofNewRevision << line;
               }

               //  common section
               state = 0;
            }
            else
            {
               if(!bFirstLineNR)
                  ofNewRevision << newline;
               else
                  bFirstLineNR = false;
               ofNewRevision << line;
            }
         }
         break;


      // in nested section in working copy section
      case 3:
         // search beginning of nested conflict section
         bNestedConflicts = true;
         pos = line.find("<<<<<<< ");
         if(pos == 0)
         {
            iNestingLevel++;
         }
         else
         {
            pos = line.find(">>>>>>> ");
            if(pos != std::string::npos)
            {
               if(iNestingLevel == 0)
               {
                  state = 1;
               }
               else
               {
                  iNestingLevel--;
               }
            }
         }
         if(!bFirstLineWC)
            ofWorkingCopy << newline;
         else
            bFirstLineWC = false;
         ofWorkingCopy << line;
         break;

      // in nested section in new revision section
      case 4:
         // search beginning of nested conflict section
         pos = line.find("<<<<<<< ");
         if(pos == 0)
         {
            iNestingLevel++;
         }
         else
         {
            pos = line.find(">>>>>>> ");
            if(pos != std::string::npos)
            {
               if(iNestingLevel == 0)
               {
                  state = 2;
               }
               else
               {
                  iNestingLevel--;
               }
            }
         }
         if(!bFirstLineNR)
            ofNewRevision << newline;
         else
            bFirstLineNR = false;
         ofNewRevision << line;
         break;
      }
   }

   // Flush
   ofNewRevision.flush();
   ofWorkingCopy.flush();
   
   // Close
   ofNewRevision.close();
   ofWorkingCopy.close();
   
   return bResult;
}
