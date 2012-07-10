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


// Initialize GUIDs (should be done only and at-least once per DLL/EXE)
#define INITGUID

#include "StdAfx.h"

// This header is not part of the official wxWidgets distribution, but is needed to work around locale problems
#include <wx/msgcatalog.h>

#include <Utils/Translate.h>
#include <Utils/ShellUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/LangUtils.h>
#include <Utils/Preference.h>
#include <locale.h>
#include <string>

#include <initguid.h>
#include <shlguid.h>

#include "ShellExt.h"

// Reference count of this DLL, must be aligned because of InterlockedIncrement
#if defined(_MSC_VER) && (_MSC_VER >= 1300)
// Only for VS .NET - otherwise just hope for the best
__declspec(align(4))
#endif
UINT g_cRefThisDll = 0;     
// Handle to this DLL itself.
HINSTANCE g_hInstance = 0;                   

wxMsgCatalog*   g_msgCatalog = 0;
wxLogGui        g_Log;           
int             g_Language = -1;
CriticalSection g_csLocale;
DWORD           g_dwLangTimestamp = 0;

// Timeout for language cache
#define LANG_TIMEOUT 1000

// static CShellExt members
CriticalSection CShellExt::ourShellExtCS;   // Critical section for thread safety
ShellExtMap CShellExt::ourShellExtMap;
LPITEMIDLIST CShellExt::ourMyDocumentsPIDL = 0;
std::vector<MenuDescription> CShellExt::ourMenus;
bool CShellExt::ourContextMenuInitialised = false;


#if defined(_DEBUG) && !defined(__GNUWIN32__) && (_MSC_VER >= 1300)
// Autocreate mini dumps on crash
MiniDumper *g_MiniDumper;
#endif


extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /* lpReserved */)
{
    TDEBUG_ENTER("DllMain");
   if (dwReason == DLL_PROCESS_ATTACH)
   {
      // Extension DLL one-time initialization
      g_hInstance = hInstance;
#if defined(_DEBUG) && !defined(__GNUWIN32__) && (_MSC_VER >= 1300)
      g_MiniDumper = new MiniDumper(wxT("TortoiseCVS"), "TortoiseShell");
#endif
      CShellExt::StaticInit();
   }
   else if (dwReason == DLL_PROCESS_DETACH)
   {
      // Delete shell extensions (Explorer doesn't?)
      DeleteShellExtensions();
      // Release overlay semaphore
      CShellExt::StaticRelease();
#if defined(_DEBUG) && !defined(__GNUWIN32__) && (_MSC_VER >= 1300)
      delete g_MiniDumper;
#endif
   }
   
   return 1;   // ok
}

STDAPI DllCanUnloadNow(void)
{
   return (g_cRefThisDll == 0 ? S_OK : S_FALSE);
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvOut)
{
    *ppvOut = 0;
   
    TortoiseOLEClass whichClass = TORTOISE_OLE_INVALID;
    if (IsEqualIID(rclsid, CLSID_TortoiseCVS0))
        whichClass = TORTOISE_OLE_INCVS;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS1))
        whichClass = TORTOISE_OLE_CHANGED;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS2))
        whichClass = TORTOISE_OLE_NOTINCVS;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS3))
        whichClass = TORTOISE_OLE_CONFLICT;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS4))
        whichClass = TORTOISE_OLE_INCVSREADONLY;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS5))
        whichClass = TORTOISE_OLE_IGNORED;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS6))
        whichClass = TORTOISE_OLE_ADDED;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS7))
        whichClass = TORTOISE_OLE_DELETED;
    else if (IsEqualIID(rclsid, CLSID_TortoiseCVS8))
        whichClass = TORTOISE_OLE_LOCKED;
   
    if (whichClass != TORTOISE_OLE_INVALID)
    {
        CShellExtClassFactory *pcf = new CShellExtClassFactory(whichClass);
      
        return pcf->QueryInterface(riid, ppvOut);
    }
   
    return CLASS_E_CLASSNOTAVAILABLE;
}

CShellExtClassFactory::CShellExtClassFactory(TortoiseOLEClass classToMake)
{
   InterlockedIncrement((long*) &g_cRefThisDll);
   myclassToMake = classToMake;
   mycRef = 0L;
}

CShellExtClassFactory::~CShellExtClassFactory()          
{
   InterlockedDecrement((long*) &g_cRefThisDll);
}

STDMETHODIMP CShellExtClassFactory::QueryInterface(REFIID riid,
                                                   LPVOID FAR *ppv)
{
   *ppv = 0;
   
   // Any interface on this object is the object pointer
   
   if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
   {
      *ppv = (LPCLASSFACTORY)this;
      
      AddRef();
      
      return NOERROR;
   }
   
   return E_NOINTERFACE;
}  

STDMETHODIMP_(ULONG) CShellExtClassFactory::AddRef()
{
    return ++mycRef;
}

STDMETHODIMP_(ULONG) CShellExtClassFactory::Release()
{
    if (--mycRef)
        return mycRef;
   
    delete this;
   
    return 0L;
}

STDMETHODIMP CShellExtClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
                                                   REFIID riid,
                                                   LPVOID *ppvObj)
{
    *ppvObj = 0;
   
    // Shell extensions typically don't support aggregation (inheritance)
   
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;
   
    // Create the main shell extension object.  The shell will then call
    // QueryInterface with IID_IShellExtInit--this is how shell extensions are
    // initialized.
   
    CShellExt* pShellExt = new CShellExt(myclassToMake);  //Create the CShellExt object
   
    if (0 == pShellExt)
        return E_OUTOFMEMORY;
   
    return pShellExt->QueryInterface(riid, ppvObj);
}


STDMETHODIMP CShellExtClassFactory::LockServer(BOOL /*fLock*/)
{
    return NOERROR;
}

// *********************** CShellExt *************************
CShellExt::CShellExt(TortoiseOLEClass tortoiseClass)
{
    TDEBUG_ENTER("CShellExt ctor");
    CSHelper cs(ourShellExtCS, true);
    ourShellExtMap[this] = 1;
    cs.Leave();
    InterlockedIncrement((long*) &g_cRefThisDll);

    myTortoiseClass = tortoiseClass;
    mycRef = 0L;
    myGdiplusToken = 0;
    if (WindowsVersionIsVistaOrHigher())
    {
        TDEBUG_TRACE("Init GDI+");
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&myGdiplusToken, &gdiplusStartupInput, NULL);
    }
}

CShellExt::~CShellExt()
{
    TDEBUG_ENTER("CShellExt dtor");
    ContextMenuFinalise();

    ClearMenuBitmaps();
   
    if (myGdiplusToken)
    {
        TDEBUG_TRACE("Uninit GDI+");
        Gdiplus::GdiplusShutdown(myGdiplusToken);
        myGdiplusToken = 0;
    }
    CSHelper cs(ourShellExtCS, true);
    ourShellExtMap.erase(this);
    cs.Leave();
    InterlockedDecrement((long*) &g_cRefThisDll);
}


// Is PIDL ignored
bool CShellExt::IsIgnoredIDList(LPCITEMIDLIST pidl)
{
   TDEBUG_ENTER("CShellExt::IsIgnoredIDList");

   // Start menu
   if (IsSpecialFolder(pidl, CSIDL_STARTMENU))
   {
      TDEBUG_TRACE("return true");
      return true;
   }

   TDEBUG_TRACE("return false");
   return false;
}


// Initialize static members
void CShellExt::StaticInit()
{
   wxLog::SetActiveTarget(&g_Log);
   wxLocale::CreateLanguagesDB();
   CheckLanguage();

   if (WindowsVersionIs2K())
   {
      HRESULT hr;
      IShellFolder* psfDeskTop = 0;
      hr = SHGetDesktopFolder(&psfDeskTop);
      if (SUCCEEDED(hr))
      {
         hr = psfDeskTop->ParseDisplayName(0, 0, 
                                           L"::{450d8fba-ad25-11d0-98a8-0800361b1103}", 0, 
                                           &ourMyDocumentsPIDL, 0);
         psfDeskTop->Release();
      }
   }
}


// Release static members
void CShellExt::StaticRelease()
{
    ItemListFree(ourMyDocumentsPIDL);
    ourMyDocumentsPIDL = 0;
    wxLocale::DestroyLanguagesDB();
    if (g_msgCatalog)
        delete g_msgCatalog;
    for (std::map<std::string, HBITMAP>::iterator it = ourBitmaps.begin(); it != ourBitmaps.end(); ++it)
        ::DeleteObject(it->second);
    ourBitmaps.clear();
    for (std::set<HICON>::iterator it = ourIcons.begin(); it != ourIcons.end(); ++it)
        ::DeleteObject(*it);
    ourIcons.clear();
}



STDMETHODIMP CShellExt::QueryInterface(REFIID riid, LPVOID FAR *ppv)
{
   *ppv = 0;
   
   if (IsEqualIID(riid, IID_IShellExtInit) || IsEqualIID(riid, IID_IUnknown))
   {
      *ppv = (IShellExtInit *) this;
   }
   else if (IsEqualIID(riid, IID_IContextMenu))
   {
      *ppv = (IContextMenu *) this;
   }
   else if (IsEqualIID(riid, IID_IContextMenu2))
   {
      *ppv = (IContextMenu2 *) this;
   }
   else if (IsEqualIID(riid, IID_IContextMenu3))
   {
      *ppv = (IContextMenu3 *) this;
   }
   else if (IsEqualIID(riid, IID_IShellIconOverlayIdentifier))
   {
      *ppv = (IShellIconOverlayIdentifier *) this;
   }
   else if (IsEqualIID(riid, IID_IShellPropSheetExt))
   {
      *ppv = (IShellPropSheetExt *) this;
   }
   else if (IsEqualIID(riid, IID_IColumnProvider))
   { 
      *ppv = (IColumnProvider *) this;
   } 
   else if (IsEqualIID(riid, IID_IQueryInfo))
   { 
      *ppv = (IQueryInfo *) this;
   } 
   else if (IsEqualIID(riid, IID_IPersistFile))
   { 
      *ppv = (IPersistFile *) this;
   }
   if (*ppv)
   {
      AddRef();

      return NOERROR;
   }

   return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CShellExt::AddRef()
{
    return ++mycRef;
}

STDMETHODIMP_(ULONG) CShellExt::Release()
{
    if (--mycRef)
        return mycRef;
   
    delete this;
   
    return 0L;
}

int g_shellidlist = RegisterClipboardFormat(CFSTR_SHELLIDLIST);

STDMETHODIMP CShellExt::Initialize(LPCITEMIDLIST pIDFolder,
                                   LPDATAOBJECT pDataObj,
                                   HKEY /* hRegKey */)
{
   TDEBUG_ENTER("Initialize");
   TDEBUG_TRACE("pIDFolder: " << pIDFolder);
   TDEBUG_TRACE("pDataObj: " << pDataObj);
   CheckLanguage();
   // signal that there are no files to work on
   myFiles.clear();

   if (pDataObj)
   {
      STGMEDIUM medium;
      FORMATETC fmte =
      {
         g_shellidlist, (DVTARGETDEVICE FAR *) 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL
      };
      HRESULT hres = pDataObj->GetData(&fmte, &medium);
      
      if (SUCCEEDED(hres) && medium.hGlobal)
      {
         // Enumerate PIDLs which the user has selected
         CIDA* cida = (CIDA*) GlobalLock(medium.hGlobal);
         LPCITEMIDLIST parentFolder = GetPIDLFolder(cida);
         TDEBUG_TRACE("Parent folder: " << GetPathFromIDList(parentFolder));
         int count = cida->cidl;
         TDEBUG_TRACE("Selected items: " << count);
         for (int i = 0; i < count; ++i)
         {
            LPCITEMIDLIST child = GetPIDLItem(cida, i);
            LPITEMIDLIST absolute = AppendPIDL(parentFolder, child);
            std::string name = GetPathFromIDList(absolute);
            TDEBUG_TRACE("Processing " << GetPathFromIDList(absolute));
            if (!IsIgnoredIDList(absolute))
            {
               if (IsShortcut(absolute))
               {
                  // Don't follow shortcuts in CVS sandboxes
                  if (CVSStatus::GetFileStatus(name) == 
                      CVSStatus::STATUS_NOWHERENEAR_CVS)
                  {
                     LPITEMIDLIST target = GetShortcutTarget(absolute);
                     ItemListFree(absolute);
                     absolute = target;
                     name = GetPathFromIDList(target);
                  }
               }

               std::string name = GetPathFromIDList(absolute);
               myFiles.push_back(name);
            }
            else
            {
               myFiles.push_back("");
            }

            ItemListFree(absolute);
         }

         GlobalUnlock(medium.hGlobal);
         if (medium.pUnkForRelease)
         {
            IUnknown* relInterface = (IUnknown*) medium.pUnkForRelease;
            relInterface->Release();
         }
      }
   }

   // if a directory background
   if (pIDFolder) 
   {
      TDEBUG_TRACE("Folder " << GetPathFromIDList(pIDFolder));
      if (!(IsShortcut(pIDFolder) || IsIgnoredIDList(pIDFolder)))
      {
         std::string dirname = GetPathFromIDList(pIDFolder);
         myFiles.push_back(dirname);
      }
      else
      {
         myFiles.push_back("");
      }
   }
   
   return NOERROR;
}


// IPersistFile members
STDMETHODIMP CShellExt::GetClassID(CLSID *pclsid) 
{
   *pclsid = CLSID_TortoiseCVS0;
   return S_OK;
}


STDMETHODIMP CShellExt::Load(LPCOLESTR pszFileName, DWORD /* dwMode */)
{
   TDEBUG_ENTER("CShellExt::Load");
   TDEBUG_TRACE(pszFileName);
   myFile = pszFileName;
   return S_OK;
}


// Delete shell extensions
void DeleteShellExtensions()
{
   TDEBUG_ENTER("DeleteShellExtensions");
   CSHelper cs(CShellExt::ourShellExtCS, true);
   ShellExtMap::iterator it = CShellExt::ourShellExtMap.begin();
   while (it != CShellExt::ourShellExtMap.end())
   {
      delete it->first;
      it = CShellExt::ourShellExtMap.begin();
   }
   CShellExt::ourShellExtMap.clear();
}


// Get translated string
const wxChar* GetString(const wxChar* str)
{
   g_csLocale.Enter();
   ASSERT(g_msgCatalog);
   const wxChar* result = g_msgCatalog->GetString(str);
   g_csLocale.Leave();
   if (result)
      return result;
   return str;
}



void CheckLanguage()
{
   TDEBUG_ENTER("CheckLanguage");
   CSHelper csh(g_csLocale, true);
   if (GetTickCount() - g_dwLangTimestamp > LANG_TIMEOUT)
   {
      int language = GetLangFromCanonicalName(GetStringPreference("LanguageIso"));
      if (language < 0)
         language = wxLANGUAGE_DEFAULT;
      TDEBUG_TRACE("CheckLanguage: language " << language << " g_Language " << g_Language);
      if (language != g_Language)
      {
         // Language setting has been changed
         g_Language = language;
         const wxLanguageInfo* info = wxLocale::GetLanguageInfo(language);
         if (info)
         {
             delete g_msgCatalog;
             g_msgCatalog = new wxMsgCatalog;
             // Get TortoiseCVS installation directory
             std::string localedir = EnsureTrailingDelimiter(GetTortoiseDirectory());
             localedir += "locale/";
             // Add "locale" subdir to catalog lookup path
             wxLocale::AddCatalogLookupPathPrefix(MultibyteToWide(localedir).c_str());
             // Load catalog
             if (!g_msgCatalog->Load(info->CanonicalName, wxT("TortoiseCVS"), 0, true))
             {
                 TDEBUG_TRACE("Error loading message catalog for locale "
                              << info->Language
                              << ". Messages will not be translated.");
             }
         }
         else
         {
            TDEBUG_TRACE("Failed initializing localization framework. Messages will not be translated.");
         }
      }
      g_dwLangTimestamp = GetTickCount();
   }
}
