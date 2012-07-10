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

// Implementation of IShellIconOverlayIdentifier interface

#include "StdAfx.h"
#include "ShellExt.h"
#include <Utils/PathUtils.h>
#include <Utils/Preference.h>
#include <Utils/ShellUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/Translate.h>
#include <Utils/UnicodeStrings.h>


// "The Shell calls IShellIconOverlayIdentifier::GetOverlayInfo to request the
//  location of the handler's icon overlay. The icon overlay handler returns
//  the name of the file containing the overlay image, and its index within
//  that file. The Shell then adds the icon overlay to the system image list."


STDMETHODIMP CShellExt::GetOverlayInfo(LPWSTR, int cchMax, int*, DWORD *pdwFlags)
{
   // Nothing to do here - TortoiseOverlays does the work

   CheckLanguage();

   return S_OK;
};

STDMETHODIMP CShellExt::GetPriority(int *pPriority)
{
   TDEBUG_ENTER("CShellExt::GetPriority");
   switch (myTortoiseClass)
   {
   case TORTOISE_OLE_CONFLICT:
      *pPriority = 60;
      break;
   case TORTOISE_OLE_CHANGED:
      *pPriority = 30;
      break;
   case TORTOISE_OLE_IGNORED:
      *pPriority = 20;
      break;
   case TORTOISE_OLE_NOTINCVS:
      *pPriority = 90;
      break;
   case TORTOISE_OLE_INCVSREADONLY:
      *pPriority = 50;
      break;
   case TORTOISE_OLE_INCVS:
      *pPriority = 10;
      break;
   case TORTOISE_OLE_ADDED:
      *pPriority = 25;
      break;
   case TORTOISE_OLE_DELETED:
      *pPriority = 70;
      break;
   case TORTOISE_OLE_LOCKED:
      *pPriority = 27;
      break;
   default:
      return S_FALSE;
   }

   TDEBUG_TRACE("Priority: " << *pPriority);
   return S_OK;
}

// "Before painting an object's icon, the Shell passes the object's name to
//  each icon overlay handler's IShellIconOverlayIdentifier::IsMemberOf
//  method. An icon overlay handler is normally associated with a particular
//  group of files. For example, the icon overlay handler might request an
//  overlay for all members of a file class, such as all files with an .myp
//  file name extension. If a handler wants to have its icon overlay displayed,
//  it returns S_OK. The Shell then calls the handler's
//  IShellIconOverlayIdentifier::GetOverlayInfo method to determine which icon
//  to display."

#define IS_MEMBER_OF_CACHE_SIZE 10
#define IS_MEMBER_OF_CACHE_TIMEOUT 1000

// static
TortoiseMemoryCache<std::string, CShellExt::IsMemberOfInfo> CShellExt::ourIsMemberOfCache(IS_MEMBER_OF_CACHE_SIZE);
CriticalSection CShellExt::ourIsMemberOfCS;

STDMETHODIMP CShellExt::IsMemberOf(LPCWSTR pwszPath, DWORD /* dwAttrib */)
{
   TDEBUG_ENTER("CShellExt::IsMemberOf");
   CheckLanguage();

   CVSStatus::FileStatus status = CVSStatus::STATUS_INCVS_RO;
   std::string path = WideToMultibyte(std::wstring(pwszPath));
   TDEBUG_TRACE("IsMemberOf(" << path << ")");

   // Check cache
   CSHelper cs(ourIsMemberOfCS, true);
   IsMemberOfInfo *data = 0;
   DWORD tc = GetTickCount();
   bool bGetUncached = false;
   TDEBUG_TRACE("Search cache for " << path);
   data = ourIsMemberOfCache.Find(path);
   if (data)
   {
      TDEBUG_TRACE("Found " << path << " in cache");
      if (tc - data->myTimeStamp < IS_MEMBER_OF_CACHE_TIMEOUT)
      {
         TDEBUG_TRACE("Not outdated :)");
         if (data->myIgnore)
         {
            return S_FALSE;
         }
         else
         {
            status = data->myStatus;
         }
      }
      else
      {
         TDEBUG_TRACE("Outdated :(");
         bGetUncached = true;
      }
   }

   if (!data)
   {
      TDEBUG_TRACE("Create new entry");
      data = ourIsMemberOfCache.Insert(path);
      bGetUncached = true;
   }

   if (bGetUncached)
   {
      data->myTimeStamp = tc;
      TDEBUG_TRACE("Get data uncached");

      // Check removable / network drives / in- and excluded paths
      if (CVSStatus::IsDisabledPath(path))
      {
         TDEBUG_TRACE("Disabled path");
         data->myIgnore = true;
         return S_FALSE;
      }

      status = CVSStatus::GetFileStatusRecursive(path, -1, -1);
      data->myStatus = status;
   }
   cs.Leave();

   TDEBUG_TRACE("FileStatus(" << path << "): " << status);

   if (status == CVSStatus::STATUS_NOWHERENEAR_CVS)
      return S_FALSE;

   switch(myTortoiseClass)
   {
      case TORTOISE_OLE_IGNORED:
         if (status == CVSStatus::STATUS_IGNORED)
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_IGNORED");
            return S_OK;
         }
         break;
      
      case TORTOISE_OLE_CONFLICT:
         if (status == CVSStatus::STATUS_CONFLICT)
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_CONFLICT");
            return S_OK;
         }
         break;

      case TORTOISE_OLE_INCVSREADONLY:
         if ((status == CVSStatus::STATUS_INCVS_RO) ||
             (status == CVSStatus::STATUS_LOCKED_RO))
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_INCVSREADONLY");
            return S_OK;
         }
         break;

      case TORTOISE_OLE_ADDED:
         if (status == CVSStatus::STATUS_ADDED)
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_ADDED");
            return S_OK;
         }
         break;

      case TORTOISE_OLE_CHANGED:
         if (status == CVSStatus::STATUS_CHANGED ||
             status == CVSStatus::STATUS_REMOVED ||
             status == CVSStatus::STATUS_RENAMED)
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_CHANGED");
            return S_OK;
         }
         break;

      case TORTOISE_OLE_NOTINCVS:
         if (status == CVSStatus::STATUS_NOTINCVS)
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_NOTINCVS");
            return S_OK;
         }
         break;

      case TORTOISE_OLE_INCVS:
         if ((status == CVSStatus::STATUS_INCVS_RW) || 
             (status == CVSStatus::STATUS_OUTERDIRECTORY) ||
             (status == CVSStatus::STATUS_LOCKED) ||
             // Treat static files as unmodified
             (status == CVSStatus::STATUS_STATIC))
         {
            TDEBUG_TRACE("Member of TORTOISE_OLE_INCVS");
            return S_OK;
         }
         break;

      case TORTOISE_OLE_DELETED:
          if (status == CVSStatus::STATUS_REMOVED ||
              status == CVSStatus::STATUS_MISSING)
          {
              TDEBUG_TRACE("Member of TORTOISE_OLE_DELETED");
              return S_OK;
          }
          break;

      case TORTOISE_OLE_LOCKED:
          if (status == CVSStatus::STATUS_LOCKED)
          {
              TDEBUG_TRACE("Member of TORTOISE_OLE_LOCKED");
              return S_OK;
          }
          break;

      default:
         return S_FALSE;
   }

   return S_FALSE;
}
