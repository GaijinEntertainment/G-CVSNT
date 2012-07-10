// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - January 2004

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
#include <Utils/TortoiseDebug.h>
#include "MakeArgs.h"
#include "CVSFeatures.h"



// Constructor
CVSServerFeatures::CVSServerFeatures()
   : myInitialized(false)
{
}



// Initialize
bool CVSServerFeatures::Initialize(CVSAction* glue)
{
   bool result = true;
   if (!myInitialized)
   {
      result = false;
      myVersionInfo = CVSStatus::GetServerCVSVersion(myCVSRoot, glue);
      if (myVersionInfo.IsValid())
      {
         myInitialized = true;
         result = true;
      }
   }
   return result;
}



// Does this release support file options
bool CVSServerFeatures::SupportsFileOptions() const
{
   ASSERT(myInitialized);
   return myVersionInfo.IsCVSNT() && 
      myVersionInfo >= CVSStatus::CVSVersionInfo("2.0.18");
}



// Does this release support unicode
bool CVSServerFeatures::SupportsUnicode() const
{
   ASSERT(myInitialized);
   return myVersionInfo.IsCVSNT() && 
      myVersionInfo >= CVSStatus::CVSVersionInfo("2.0.11");
}


// Does this release support 3-way diffs
bool CVSServerFeatures::SupportsThreeWayDiffs() const
{
   ASSERT(myInitialized);
   return myVersionInfo.IsCVSNT() && 
      myVersionInfo >= CVSStatus::CVSVersionInfo("2.0.18");
}


bool CVSServerFeatures::SupportsExclusiveEdit() const
{
   ASSERT(myInitialized);
   return myVersionInfo.IsCVSNT() && 
      myVersionInfo >= CVSStatus::CVSVersionInfo("2.0.58");
}


bool CVSServerFeatures::SupportsEditComment() const
{
   ASSERT(myInitialized);
   return myVersionInfo.IsCVSNT() && 
      myVersionInfo >= CVSStatus::CVSVersionInfo("2.0.52");
}


bool CVSServerFeatures::SupportsVerboseEditors() const
{
    ASSERT(myInitialized);
    return myVersionInfo.IsCVSNT();
}


void CVSServerFeatures::AddUnixLineEndingsFlag(MakeArgs& args) const
{
   ASSERT(myInitialized);
   if (myVersionInfo.IsCVSNT())
       args.add_option("-k+L");
   else
       args.add_global_option("--lf");
}


////////////////


// Constructor
CVSClientFeatures::CVSClientFeatures()
   : myInitialized(false)
{
}



// Initialize
bool CVSClientFeatures::Initialize(CVSAction* glue)
{
   bool result = true;
   if (!myInitialized)
   {
      result = false;
      myVersionInfo = CVSStatus::GetClientCVSVersion(myCVSRoot, glue);
      if (myVersionInfo.IsValid())
      {
         myInitialized = true;
         result = true;
      }
   }
   return result;
}



void CVSClientFeatures::AddOptionM(MakeArgs& args) const
{
   ASSERT(myInitialized);
   if (myVersionInfo.IsCVSNT() && (myVersionInfo > CVSStatus::CVSVersionInfo("2.5.04.2471")))
       args.add_option("-M");
   else
       args.add_option("-m");
}
