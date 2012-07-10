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
#include "CVSStatus.h"
#include "CVSRoot.h"
#include <Utils/PathUtils.h>
#include <Utils/TortoiseDebug.h>

#include <fstream>


std::string CVSStatus::CVSRootForPath(std::string path)   
{   
    TDEBUG_ENTER("CVSStatus::CVSRootForPath");   
    TDEBUG_TRACE("path: " << path);     
    
    // We must be a directory with a CVS dir     
    path = GetDirectoryPart(path);   
    path = EnsureTrailingDelimiter(path);     
    if (!CVSDirectoryHere(path))     
    {     
        TDEBUG_TRACE("No CVS directory found in " << path);     
        return "";    
    }     
    
    std::string root;    
    std::string rootFile = path + "CVS/Root";    
    std::ifstream in(rootFile.c_str(), std::ios::in);     
    if (!in.good())   
    {     
        TDEBUG_TRACE("Error opening " << rootFile);    
        return "";    
    }     
    std::getline(in, root);    
    
    return root;   
}   
    
std::string CVSStatus::CVSRepositoryForPath(std::string path)   
{   
    TDEBUG_ENTER("CVSStatus::CVSRepositoryForPath");   
    TDEBUG_TRACE("  path: '" << path << "')");   
    
    // We must be a directory with a CVS dir     
    path = GetDirectoryPart(path);   
    path = EnsureTrailingDelimiter(path);     
    if (!CVSDirectoryHere(path))     
        return "";    
    
    std::string rootFile = path + "CVS/Repository";    
    std::ifstream in(rootFile.c_str(), std::ios::in);     
    if (!in.good())   
        return "";    
    std::string repository;    
    std::getline(in, repository);    
    
    // Apparently, the path in CVS/Repository may be either absolute or relative.    
    // So if it is absolute, we hack it to be relative.   
    TDEBUG_TRACE("  Repository is '" << repository << "')");    
    if (repository[0] == '/' || (repository.length() > 2 && repository[1] == ':'))   
    {     
        CVSRoot cvsroot(CVSRootForPath(path));   
        std::string root = cvsroot.GetDirectory();     
        TDEBUG_TRACE("  Root is '" << root << "')");   
        unsigned int i = 0;    
        // Find out how many leading chars match    
        while (i < root.length() && i < repository.length())    
        {    
            if (root[i] == repository[i]   
                || (root[i] == '\\' && repository[i] == '/')    
                || (root[i] == '/' && repository[i] == '\\'))   
            {   
                i++;     
                continue;   
            }   
            else   
            {   
                break;   
            }   
        }    
        // Also kill final slash     
        if (repository[i] == '/' || repository[i] == '\\')   
            ++i;   
        repository = repository.substr(i);    
    }     
    return repository;   
}   
