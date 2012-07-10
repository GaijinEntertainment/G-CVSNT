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
#include "ShellExt.h"
#include <string>
#include "../CVSGlue/CVSStatus.h"
#include "../Utils/StringUtils.h"
#include "../Utils/TimeUtils.h"
#include "../Utils/Translate.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/Preference.h"


const static int ColumnFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;

#define GET_ITEM_DATA_CACHE_SIZE 10
#define GET_ITEM_DATA_CACHE_TIMEOUT 1000

TortoiseMemoryCache<std::string, CShellExt::GetItemDataInfo> 
   CShellExt::ourGetItemDataCache(GET_ITEM_DATA_CACHE_SIZE);
CriticalSection CShellExt::ourGetItemDataCS;


// IColumnProvider members
STDMETHODIMP CShellExt::GetColumnInfo(DWORD dwIndex, SHCOLUMNINFO *psci)
{
   TDEBUG_ENTER("CShellExt::GetColumnInfo");
   TDEBUG_TRACE("dwIndex: " << dwIndex);
   TDEBUG_TRACE("psci: " << psci);
   if (dwIndex > 3)
      return S_FALSE;

   CheckLanguage();

   psci->scid.fmtid = CLSID_TortoiseCVS0;
   psci->scid.pid = dwIndex;
   psci->vt = VT_BSTR;
   psci->fmt = LVCFMT_LEFT;
   psci->cChars = 15;
   psci->csFlags = ColumnFlags;
   std::wstring title;
   std::wstring desc;

   switch (dwIndex)
   {
   case 0:
      title = UnicodeTranslate("CVS Revision");
      desc = UnicodeTranslate("Version of the file in CVS");
      break;

   case 1:
      title = UnicodeTranslate("CVS Sticky Tag/Date");
      desc = UnicodeTranslate("Tag/date of the file in CVS");
      break;

   case 2:
      title = UnicodeTranslate("CVS Status");
      desc = UnicodeTranslate("Status of the file in CVS");
      break;

   case 3:
      title = UnicodeTranslate("CVS File Format");
      desc = UnicodeTranslate("Format of the file in CVS");
      break;
   }

   wcsncpy(psci->wszTitle, title.c_str(), MAX_COLUMN_NAME_LEN);
   psci->wszTitle[MAX_COLUMN_NAME_LEN - 1] = 0;
   wcsncpy(psci->wszDescription, desc.c_str(), MAX_COLUMN_DESC_LEN);
   psci->wszDescription[MAX_COLUMN_DESC_LEN - 1] = 0;

   return S_OK;
}


STDMETHODIMP CShellExt::GetItemData(LPCSHCOLUMNID pscid, LPCSHCOLUMNDATA pscd, VARIANT *pvarData)
{
   TDEBUG_ENTER("CShellExt::GetItemData");
   if (pscid->fmtid == CLSID_TortoiseCVS0 && pscid->pid < 4)
   {
      CheckLanguage();

      std::string path = WideToMultibyte(std::wstring(pscd->wszFile));
      TDEBUG_TRACE("File: " << path);

      wxString info;
      CVSStatus::FileStatus status = CVSStatus::STATUS_INCVS_RO;
      
      bool hasStickyTag = false; 
      bool hasStickyDate = false;
      bool hasRevisionNumber = false;
      std::string stickyTagOrDate;
      std::string revisionNumber;
      wxString fileFormat;

      // Check cache
      CSHelper cs(ourGetItemDataCS, true);
      GetItemDataInfo* data = 0;
      DWORD tc = GetTickCount();
      bool bGetUncached = false;
      TDEBUG_TRACE("Search cache for " << path);
      data = ourGetItemDataCache.Find(path);
      if (data)
      {
         TDEBUG_TRACE("Found " << path << " in cache");
         if (tc - data->myTimeStamp < GET_ITEM_DATA_CACHE_TIMEOUT)
         {
            TDEBUG_TRACE("Not outdated :)");
            status = data->myStatus;
            hasStickyDate = data->myHasStickyDate;
            hasStickyTag = data->myHasStickyTag;
            hasRevisionNumber = data->myHasRevisionNumber;
            stickyTagOrDate = data->myStickyTagOrDate;
            revisionNumber = data->myRevisionNumber;
            fileFormat = data->myFileFormat;
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
         data = ourGetItemDataCache.Insert(path);
         bGetUncached = true;
      }

      if (bGetUncached)
      {
         data->myTimeStamp = tc;
         bool done = false;
         TDEBUG_TRACE("Get data uncached");

         if (!done)
         {
            // Check removable / network drives / in- and excluded paths
            if (CVSStatus::IsDisabledPath(path))
            {
               TDEBUG_TRACE("Disabled path");
               return S_FALSE;
            }
         }

         if (!done)
         {
            if (CVSStatus::IsOuterDirectory(path))
            {
               data->myStatus = CVSStatus::STATUS_OUTERDIRECTORY;
               status = data->myStatus;
               done = true;
            }
         }

         if (!done)
         {
            CSHelper csEntnode;
            EntnodeData* nodeData = CVSStatus::GetEntnodeData(path, csEntnode);
            status = CVSStatus::GetFileStatus(nodeData, CVSStatus::GetEditStatus(path));
            data->myStatus = status;
            if (nodeData)
            {
               hasRevisionNumber = CVSStatus::HasRevisionNumber(nodeData);
               if (hasRevisionNumber)
               {
                  revisionNumber = CVSStatus::GetRevisionNumber(nodeData);
               }

               hasStickyTag = CVSStatus::HasStickyTag(nodeData);
               if (!hasStickyTag)
               {
                  hasStickyDate = CVSStatus::HasStickyDate(nodeData);
               }
               else
               {
                  hasStickyDate = false;
               }
               if (hasStickyTag)
               {
                  stickyTagOrDate = CVSStatus::GetStickyTag(nodeData);
               }
               else
               {
                  // TODO: Convert from CVS "free-format" date to localized date
                  stickyTagOrDate = CVSStatus::GetStickyDate(nodeData);
               }
               if (IsDirectory(path.c_str()))
               {
                  fileFormat = _("Folder");
               }
               else
               {
                  fileFormat = CVSStatus::FileFormatString(CVSStatus::GetFileFormat(nodeData));
               }

               data->myHasStickyTag = hasStickyTag;
               data->myHasStickyDate = hasStickyDate;
               data->myHasRevisionNumber = hasRevisionNumber;
               data->myStickyTagOrDate = stickyTagOrDate;
               data->myRevisionNumber = revisionNumber;
               data->myFileFormat = fileFormat;
            }
         }
      }
      cs.Leave();

      switch (pscid->pid)
      {
      case 0:
          TDEBUG_TRACE("Get revision: Status: " << status << " hasRevisionNumber: " << hasRevisionNumber);
         // CVS Revision
         if ((status == CVSStatus::STATUS_INCVS_RO || 
              status == CVSStatus::STATUS_INCVS_RW ||
              status == CVSStatus::STATUS_CHANGED ||
              status == CVSStatus::STATUS_ADDED || 
              status == CVSStatus::STATUS_CONFLICT) &&
             hasRevisionNumber)
         {
             info = wxText(revisionNumber);
             if (info == wxT("0"))
                 info = _("Added");
         }
         else
             info = wxT("-");
         break;
      case 1:
         TDEBUG_TRACE("Get sticky tag");
         // CVS Sticky Tag
         if (status == CVSStatus::STATUS_INCVS_RO || 
             status == CVSStatus::STATUS_INCVS_RW ||
             status == CVSStatus::STATUS_CHANGED ||
             status == CVSStatus::STATUS_ADDED || 
             status == CVSStatus::STATUS_CONFLICT)
         {
            if (hasStickyTag | hasStickyDate)
            {
               if (stickyTagOrDate.empty())
                  info = wxT("-");
               else
                  info = wxText(stickyTagOrDate);
            }
         }
         else
            info = wxT("-");
         break;
      case 2:
         TDEBUG_TRACE("Get status");
         // CVS Status
         if (status == CVSStatus::STATUS_NOTINCVS || status == CVSStatus::STATUS_NOWHERENEAR_CVS)
            info = wxT("-");
         else
            info = CVSStatus::FileStatusString(status);
         break;
      case 3:
         // CVS file format
         TDEBUG_TRACE("Get format");
         if (status == CVSStatus::STATUS_INCVS_RO || 
             status == CVSStatus::STATUS_INCVS_RW ||
             status == CVSStatus::STATUS_CHANGED ||
             status == CVSStatus::STATUS_ADDED || 
             status == CVSStatus::STATUS_CONFLICT)
         {
            info = fileFormat;
         }
         else
            info = wxT("-");
         break;
      }

      V_VT(pvarData) = VT_BSTR;
#if wxUSE_UNICODE
      V_BSTR(pvarData) = SysAllocString(info.c_str());
#else
      V_BSTR(pvarData) = SysAllocString(MultibyteToWide(info).c_str());
#endif
      return S_OK;
   }

   return S_FALSE;
}

STDMETHODIMP CShellExt::Initialize(LPCSHCOLUMNINIT psci)
{
   TDEBUG_ENTER("CShellExt::Initialize");
   CheckLanguage();
   if (GetBooleanPreference("Hide Columns"))
   {
      // psci->wszFolder (WCHAR) holds the path to the folder to be displayed
      // Should check to see if its a "CVS" dir and if not return E_FAIL

      // The only problem with doing this is that we can't add this column in non-CVS dirs.
      // In fact it shows up in non-CVS dirs as some random other column).
      // Thats why this is turned off for !COLUMN_ALWAYS_SHOW
      std::string path = WideToMultibyte(std::wstring(psci->wszFolder));
      TDEBUG_TRACE(path);
      CVSStatus::FileStatus status = CVSStatus::GetFileStatus(path);
      if (status == CVSStatus::STATUS_NOWHERENEAR_CVS || 
          status == CVSStatus::STATUS_IGNORED || 
          status == CVSStatus::STATUS_NOTINCVS)
      {
         TDEBUG_TRACE("No CVS folder");
         return E_FAIL;
      }
      return S_OK;
   }
   else
   {
      TDEBUG_TRACE("Initialize entered with COLUMN_ALWAYS_SHOW not set");
      return S_OK;
   }
}

