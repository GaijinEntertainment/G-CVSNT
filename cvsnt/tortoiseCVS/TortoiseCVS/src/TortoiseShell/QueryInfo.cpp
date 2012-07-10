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

// Implementation of IQueryInfo interface

#include "StdAfx.h"
#include "ShellExt.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/StringUtils.h"
#include "../Utils/PathUtils.h"
#include "../CVSGlue/CVSStatus.h"
#include "../Utils/Translate.h"


STDMETHODIMP CShellExt::GetInfoTip(DWORD, WCHAR** ppwszTip)
{
   TDEBUG_ENTER("CShellExt::GetInfoTip");
   std::string sFile = WideToMultibyte(myFile, 0);

   // Check removable / network drives / in- and excluded paths
   if (CVSStatus::IsDisabledPath(sFile))
   {
      TDEBUG_TRACE("Disabled path");
      *ppwszTip = 0;
      return S_OK;
   }

   if (IsDirectory(sFile.c_str()))
   {
      TDEBUG_TRACE("Folder");
      *ppwszTip = 0;
      return S_OK;
   }

   CheckLanguage();

   // Retrieve information
   CSHelper csEntnode;
   EntnodeData* nodeData = CVSStatus::GetEntnodeData(sFile, csEntnode);
   CVSStatus::FileStatus status = CVSStatus::GetFileStatus(nodeData);
   if (!nodeData)
   {
      TDEBUG_TRACE("No data");
      *ppwszTip = 0;
      csEntnode.Leave();
      return S_OK;
   }

   bool hasRevisionNumber = CVSStatus::HasRevisionNumber(nodeData);
   wxString revisionNumber;
   if (hasRevisionNumber)
   {
      std::string stdRevisionNumber = CVSStatus::GetRevisionNumber(nodeData);
      revisionNumber = wxString::FromAscii(stdRevisionNumber.c_str());
   }

   bool hasStickyTag = CVSStatus::HasStickyTag(nodeData);
   std::string stdStickyTagOrDate;
   if (hasStickyTag)
      stdStickyTagOrDate = CVSStatus::GetStickyTag(nodeData);
   else
      stdStickyTagOrDate = CVSStatus::GetStickyDate(nodeData);
   wxString stickyTagOrDate = wxString::FromAscii(stdStickyTagOrDate.c_str());
   wxString fileFormat = CVSStatus::FileFormatString(CVSStatus::GetFileFormat(nodeData));
   csEntnode.Leave();
   
   // CVS Status
   wxString fileStatus;
   if ((status != CVSStatus::STATUS_NOTINCVS) && (status != CVSStatus::STATUS_NOWHERENEAR_CVS))
      fileStatus = CVSStatus::FileStatusString(status);

   if (!fileStatus.empty())
   {
#if wxUSE_UNICODE
      std::wstringstream ss;
#else
      std::ostringstream ss;
#endif
      ss << _("CVS Status") << ": " << fileStatus << "\n";
      if (!revisionNumber.empty())
         ss << _("CVS Revision") << ": " << revisionNumber << "\n";
      if (!stickyTagOrDate.empty())
         ss << _("CVS Sticky Tag") << ": " << stickyTagOrDate << "\n";
      if (status != CVSStatus::STATUS_IGNORED)
         ss << _("CVS File Format") << ": " << fileFormat;

#if wxUSE_UNICODE
      std::wstring ws = ss.str();
#else
      std::wstring ws = MultibyteToWide(ss.str(), 0);
#endif

      IMalloc* pMalloc = 0;
      if (SUCCEEDED(SHGetMalloc(&pMalloc)))
      {
         *ppwszTip = (LPWSTR) pMalloc->Alloc((ws.length() + 1) * sizeof(WCHAR));
         lstrcpyW(*ppwszTip, ws.c_str());
      }
      if (pMalloc)
         pMalloc->Release();
   }
   else
      *ppwszTip = 0;
      
   return S_OK;
}


STDMETHODIMP CShellExt::GetInfoFlags(DWORD *pdwFlags)
{
   TDEBUG_ENTER("CShellExt::GetInfoFlags");
   *pdwFlags = 0;
   return S_OK;
}
