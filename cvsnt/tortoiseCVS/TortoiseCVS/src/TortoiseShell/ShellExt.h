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

//
// The class IDs of these Shell extension classs.
//
// class ids: 
//
// 5d1cb710-1c4b-11d4-bed5-005004b1f42f
// 5d1cb711-1c4b-11d4-bed5-005004b1f42f
// 5d1cb712-1c4b-11d4-bed5-005004b1f42f
// 5d1cb713-1c4b-11d4-bed5-005004b1f42f
// 5d1cb714-1c4b-11d4-bed5-005004b1f42f
// 5d1cb715-1c4b-11d4-bed5-005004b1f42f
// 5d1cb716-1c4b-11d4-bed5-005004b1f42f
// 5d1cb717-1c4b-11d4-bed5-005004b1f42f
// 5d1cb718-1c4b-11d4-bed5-005004b1f42f
//
//
// NOTE!!!  If you use this shell extension as a starting point, 
//          you MUST change the GUID below.  Simply run UUIDGEN.EXE
//          to generate a new GUID.
//

#ifndef SHELL_EXT_H
#define SHELL_EXT_H

#include "../Utils/FixCompilerBugs.h"

#include <vector>
#include <string>
#include <map>
#include <shlobj.h>
#include <wx/log.h>

// GdiPlus includes
#ifdef NOMINMAX
# define min(a, b) ((a) < (b) ? (a) : (b))
# define max(a, b) ((a) > (b) ? (a) : (b))
# include <Gdiplus.h>
# undef min
# undef max
#else
# include <Gdiplus.h>
#endif

#include "../ContextMenus/MenuDescription.h"
#include "../Utils/Translate.h"
#include "../Utils/Cache.h"
#include "../CVSGlue/CVSStatus.h"
#include "../Utils/FixCompilerBugs.h"


extern UINT             g_cRefThisDll; // Reference count of this DLL.
extern HINSTANCE        g_hInstance;   // Instance handle for this DLL
extern int              g_Language;    // Current language
extern wxLocale*        g_Locale;      // Locale object
extern wxLogGui         g_Log;         // Log object
extern CriticalSection  g_csLocale;
extern DWORD            g_dwLangTimestamp; // Timestamp for language check

class CShellExt;

typedef std::map<CShellExt*, int> ShellExtMap;

// Delete shell extensions
void DeleteShellExtensions();

// Check language
void CheckLanguage();


DEFINE_GUID(CLSID_TortoiseCVS0, 0x5d1cb710L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS1, 0x5d1cb711L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS2, 0x5d1cb712L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS3, 0x5d1cb713L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS4, 0x5d1cb714L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS5, 0x5d1cb715L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS6, 0x5d1cb716L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS7, 0x5d1cb717L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );
DEFINE_GUID(CLSID_TortoiseCVS8, 0x5d1cb718L, 0x1c4b, 0x11d4, 0xbe, 0xd5, 0x00, 0x50, 0x04, 0xb1, 0xf4, 0x2f );

enum TortoiseOLEClass
{
   TORTOISE_OLE_INVALID,
   TORTOISE_OLE_CHANGED,
   TORTOISE_OLE_CONFLICT,
   TORTOISE_OLE_NOTINCVS,
   TORTOISE_OLE_INCVS,
   TORTOISE_OLE_INCVSREADONLY,
   TORTOISE_OLE_IGNORED,
   TORTOISE_OLE_ADDED,
   TORTOISE_OLE_DELETED,
   TORTOISE_OLE_LOCKED
};


// this class factory object creates the main handlers -
// its constructor says which OLE class it is to make
class CShellExtClassFactory : public IClassFactory
{
protected:
    TortoiseOLEClass myclassToMake;
    ULONG mycRef;
   
public:
    CShellExtClassFactory(TortoiseOLEClass classToMake);
    virtual ~CShellExtClassFactory();
   
    //IUnknown members
    STDMETHODIMP         QueryInterface(REFIID, LPVOID FAR *);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    
    //IClassFactory members
    STDMETHODIMP      CreateInstance(LPUNKNOWN, REFIID, LPVOID FAR *);
    STDMETHODIMP      LockServer(BOOL);
};

// The actual OLE Shell context menu handler
class CShellExt : public IContextMenu3,
                  IPersistFile,
// COMPILER ERROR? You need the latest version of the
// platform SDK which has references to IColumnProvider 
// in the header files.  Download it here:
// http://www.microsoft.com/msdownload/platformsdk/sdkupdate/
                  IColumnProvider,
                  IShellExtInit,
                  IShellIconOverlayIdentifier,
                  IShellPropSheetExt,
                  IQueryInfo
{
protected:
    class IsMemberOfInfo 
    {
    public:
        IsMemberOfInfo()
            : myIgnore(false),
              myTimeStamp(0)
        {
        }
        bool                    myIgnore;
        CVSStatus::FileStatus   myStatus;
        DWORD                   myTimeStamp;
    };
   
    class GetItemDataInfo 
    {
    public:
        GetItemDataInfo()
            : myHasStickyTag(false),
              myHasStickyDate(false),
              myHasRevisionNumber(false),
              myTimeStamp(0),
              myStatus(-1)
        {
        }
        bool            myHasStickyTag;
        bool            myHasStickyDate;
        bool            myHasRevisionNumber;
        std::string     myStickyTagOrDate;
        std::string     myRevisionNumber;
        wxString        myFileFormat;
        DWORD           myTimeStamp;
        CVSStatus::FileStatus myStatus;
    };
   
    TortoiseOLEClass                    myTortoiseClass;
    // Reference count
    ULONG                               mycRef;
    std::map<std::string, HBITMAP>      myMenuBitmapMap;
    std::vector<std::string>            myFiles;
    char                                myszFileUserClickedOn[MAX_PATH];
    std::wstring                        myFile;

    std::map<std::string, int>          myVerbMap;
    std::map<unsigned int, int>         myIDMap;
    std::map<unsigned int, bool>        myShortcutMap;
    static std::vector<MenuDescription> ourMenus;
    static bool                         ourContextMenuInitialised;
    static TortoiseMemoryCache<std::string, 
                               IsMemberOfInfo> ourIsMemberOfCache;
    static TortoiseMemoryCache<std::string, 
                               GetItemDataInfo> ourGetItemDataCache;

    // Used for Windows 2000 background folder (W2k bug)
    static HBITMAP IconToBitmap(const std::string& sIcon);

    // Used for Vista icons but using GdiPlus rather than WIC so it can be compiled without Vista SDK
    // http://shellrevealed.com/blogs/shellblog/archive/2007/02/06/Vista-Style-Menus_2C00_-Part-1-_2D00_-Adding-icons-to-standard-menus.aspx
    HBITMAP IconToBitmapPARGB32(const std::string& sIcon);
    ULONG_PTR myGdiplusToken;

public:
    static CriticalSection              ourIsMemberOfCS;
    static CriticalSection              ourGetItemDataCS;
    static CriticalSection              ourShellExtCS;
    static ShellExtMap                  ourShellExtMap;
    static LPITEMIDLIST                 ourMyDocumentsPIDL;
    static std::map<std::string, HBITMAP> ourBitmaps;
    static std::set<HICON>              ourIcons;

    // Is PIDL ignored
    static bool IsIgnoredIDList(LPCITEMIDLIST pidl);
    // Initialize static members
    static void StaticInit();
    // Release static members
    static void StaticRelease();


    CShellExt(TortoiseOLEClass tortoiseClass);
    virtual ~CShellExt();
   
    // IUnknown members
    STDMETHODIMP         QueryInterface(REFIID, LPVOID FAR *);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
   
    // IContextMenu3 members
    STDMETHODIMP         QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
    STDMETHODIMP         InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi);
    STDMETHODIMP         GetCommandString(UINT_PTR idCmd, UINT uFlags, UINT FAR *reserved, LPSTR pszName, UINT cchMax);
    STDMETHODIMP         HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);
    STDMETHODIMP         HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult);
    void ContextMenuInitialise();
    void ContextMenuFinalise();

    void ClearMenuBitmaps();

    // IPersistFile - required by IColumnProvider
    STDMETHODIMP GetClassID(CLSID *pclsid);
    STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode);
    STDMETHODIMP IsDirty(void) { return S_OK; }
    STDMETHODIMP Save(LPCOLESTR /* pszFileName */, BOOL /* fRemember */) { return S_OK; }
    STDMETHODIMP SaveCompleted(LPCOLESTR /* pszFileName */) { return S_OK; }
    STDMETHODIMP GetCurFile(LPOLESTR * /* ppszFileName */) { return S_OK; }
   
    // IColumnProvider members
    STDMETHODIMP GetColumnInfo(DWORD dwIndex, SHCOLUMNINFO *psci);
    STDMETHODIMP GetItemData(LPCSHCOLUMNID pscid, LPCSHCOLUMNDATA pscd, VARIANT *pvarData);
    STDMETHODIMP Initialize(LPCSHCOLUMNINIT psci);
   
    // IShellExtInit methods and related functions
    STDMETHODIMP Initialize(LPCITEMIDLIST pIDFolder, LPDATAOBJECT pDataObj, HKEY hKeyID);
    BOOL AreShellExtTargetsAvail() const { return !myFiles.empty(); }
   
    // IExtractIcon methods
    STDMETHODIMP GetIconLocation(UINT uFlags, LPSTR szIconFile, UINT cchMax, int *piIndex, UINT *pwFlags);
    STDMETHODIMP Extract(LPCSTR pszFile, UINT nIconIndex, HICON *phiconLarge, HICON *phiconSmall, UINT nIconSize);
   
    // IShellIconOverlayIdentifier methods
    STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, int cchMax, int *pIndex, DWORD *pdwFlags);
    STDMETHODIMP GetPriority(int *pPriority); 
    STDMETHODIMP IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);
   
    // IShellPropSheetExt methods
    STDMETHODIMP AddPages(LPFNADDPROPSHEETPAGE lpfnAddPage, LPARAM lParam);
    STDMETHODIMP ReplacePage (UINT, LPFNADDPROPSHEETPAGE, LPARAM);

    // IQueryInfo methods 
    STDMETHOD(GetInfoTip)(DWORD dwFlags, WCHAR **ppwszTip);
    STDMETHOD(GetInfoFlags)(DWORD *pdwFlags);
};


#endif // SHELL_EXT_H
