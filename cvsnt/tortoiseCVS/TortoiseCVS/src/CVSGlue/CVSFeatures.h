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

#ifndef CVS_FEATURES_H
#define CVS_FEATURES_H

#include "CVSStatus.h"

class CVSAction;
class MakeArgs;


// Defines the features available for a given CVSROOT

class CVSServerFeatures
{
public:
    // Constructor
    CVSServerFeatures();

    // Set CVSROOT
    inline void SetCVSRoot(const std::string& cvsRoot)
    {
        if (cvsRoot != myCVSRoot)
        {
            myCVSRoot = cvsRoot;
            myInitialized = false;
        }
    }

    // Get CVSRoot
    inline std::string GetCVSRoot() const
    {
        return myCVSRoot;
    }

   
    // Initialize
    bool Initialize(CVSAction* glue = 0);

    // Does this release support file options
    bool SupportsFileOptions() const;

   // Does this release support unicode
   bool SupportsUnicode() const;

   // Does this release support 3-way diffs
   bool SupportsThreeWayDiffs() const;

   // Does this release support -kx
   bool SupportsExclusiveEdit() const;

    // Does this release support 'edit -m'
    bool SupportsEditComment() const;

    // Does this release support 'editors -v'
    bool SupportsVerboseEditors() const;

    // Add flag (--lf or -k+L) for UNIX line endings
    void AddUnixLineEndingsFlag(MakeArgs& args) const;

private:
   std::string myCVSRoot;
   bool myInitialized;
   CVSStatus::CVSVersionInfo myVersionInfo;
};

class CVSClientFeatures
{
public:
    // Constructor
    CVSClientFeatures();

    // Set CVSROOT
    inline void SetCVSRoot(const std::string& cvsRoot)
    {
        if (cvsRoot != myCVSRoot)
        {
            myCVSRoot = cvsRoot;
            myInitialized = false;
        }
    }

    // Get CVSRoot
    inline std::string GetCVSRoot() const
    {
        return myCVSRoot;
    }

   
    // Initialize
    bool Initialize(CVSAction* glue = 0);

   // Add -m or -M option depending on server version
   void AddOptionM(MakeArgs& args) const;
   
private:
   std::string myCVSRoot;
   bool myInitialized;
   CVSStatus::CVSVersionInfo myVersionInfo;
};

#endif // CVS_FEATURES_H
