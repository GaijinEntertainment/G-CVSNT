/*  Call Control Panel at its installed location with modified path
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#define WIN32_LEAN_AND_MEAN
#define STRICT
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#include <windows.h>
#include <cpl.h>
#include <tchar.h>

#define PATH_SIZE 10240

extern "C" LONG WINAPI CPlApplet(HWND hwndCPl, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
	static LONG (WINAPI* pCPlApplet)(HWND, UINT, LPARAM, LPARAM);
	static HMODULE hCpl;
	static CPLINFO info = {0};
	long ret;

	if(uMsg == CPL_INIT)
	{
		HKEY hKeyLocal,hKeyGlobal;
		TCHAR InstallPath[1024],SearchPath[2048],Env[10240];
		DWORD dwLen,dwType;

		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"Software\\CVS\\PServer",0,KEY_QUERY_VALUE,&hKeyGlobal))
			hKeyGlobal = NULL;

		if(RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\Cvsnt\\PServer",0,KEY_QUERY_VALUE,&hKeyLocal))
			hKeyLocal = NULL;

		dwLen=sizeof(InstallPath);
		if(RegQueryValueEx(hKeyGlobal,L"InstallPath",NULL,&dwType,(LPBYTE)InstallPath,&dwLen) &&
			RegQueryValueEx(hKeyLocal,L"InstallPath",NULL,&dwType,(LPBYTE)InstallPath,&dwLen))
		{
			InstallPath[0]='\0';
		}

		dwLen=sizeof(SearchPath);
		if(RegQueryValueEx(hKeyGlobal,L"SearchPath",NULL,&dwType,(LPBYTE)SearchPath,&dwLen) &&
			RegQueryValueEx(hKeyLocal,L"SearchPath",NULL,&dwType,(LPBYTE)SearchPath,&dwLen))
		{
			SearchPath[0]='\0';
		}
		RegCloseKey(hKeyGlobal);
		RegCloseKey(hKeyLocal);
		
		_tcscpy(Env,InstallPath);
		_tcscat(Env,_T(";"));
		_tcscat(Env,SearchPath);
		_tcscat(Env,_T(";"));

		GetEnvironmentVariable(_T("Path"),Env+_tcslen(Env),sizeof(Env));
		SetEnvironmentVariable(_T("Path"),Env);

		_tcscpy(InstallPath,_T("cvsntcpl.cpl"));
		//GetModuleFileName(NULL,InstallPath,sizeof(InstallPath));
		if(!_tcsicmp(InstallPath+_tcslen(InstallPath)-4,_T(".cpl")))
			_tcscpy(InstallPath+_tcslen(InstallPath)-4,_T(".dll"));
		else
			_tcscat(InstallPath,_T(".dll"));

		hCpl = LoadLibraryEx(InstallPath,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);

		if(!hCpl)
		{
			swprintf(Env,1024,L"Couldn't load control panel %s",InstallPath);
			MessageBox(NULL,Env,L"Control Panel",MB_ICONSTOP|MB_OK);
			return -1;
		}

		pCPlApplet=(LONG (WINAPI*)(HWND, UINT, LPARAM, LPARAM))GetProcAddress(hCpl,"CPlApplet");

		if(!pCPlApplet)
		{
			swprintf(Env,1024,L"Couldn't find CPLApplet in control panel %s",InstallPath);
			MessageBox(NULL,Env,L"Control Panel",MB_ICONSTOP|MB_OK);
			return -1;
		}
	}

	if(uMsg == CPL_NEWINQUIRE && (info.idIcon || info.idInfo || info.idName))
	{
		LPNEWCPLINFO cpl = (LPNEWCPLINFO)lParam2;

		cpl->dwSize = sizeof(NEWCPLINFO);
		cpl->lData = info.lData;
		cpl->hIcon = LoadIcon(hCpl,MAKEINTRESOURCE(info.idIcon));
		LoadString(hCpl,info.idInfo,cpl->szInfo,sizeof(cpl->szInfo)/sizeof(cpl->szInfo[0]));
		LoadString(hCpl,info.idName,cpl->szName,sizeof(cpl->szName)/sizeof(cpl->szName[0]));

		ret = 0;
	}
	else
	{
		ret = pCPlApplet(hwndCPl,uMsg,lParam1,lParam2);

		if(uMsg == CPL_INQUIRE && !ret)
		{
			LPCPLINFO cpl = (LPCPLINFO)lParam2;
			info = *cpl;
			cpl->idIcon = CPL_DYNAMIC_RES;
			cpl->idInfo = CPL_DYNAMIC_RES;
			cpl->idName = CPL_DYNAMIC_RES;
		}

		if(uMsg == CPL_EXIT)
		{
			FreeLibrary(hCpl);
		}
	}
	return ret;
}
