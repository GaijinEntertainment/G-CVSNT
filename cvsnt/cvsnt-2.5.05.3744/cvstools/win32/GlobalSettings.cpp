/*
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* Win32 specific */
#include <cvsapi.h>
#include "../GlobalSettings.h"

#include <lm.h>

namespace
{
	static char cvs_config_dir[_MAX_PATH] = {0};
	static char cvs_library_dir[_MAX_PATH] = {0};
	static char cvs_command[_MAX_PATH] = {0};

	const char *GetCvsDir(char *dir, int size)
	{
		if(!*dir) /* Only do this once */
		{
			if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","LibraryPath",dir,size) &&
			   CGlobalSettings::GetUserValue("cvsnt","PServer","LibraryPath",dir,size))
			{
				if (GetModuleFileNameA(GetModuleHandleA("cvstools.dll"), dir, size))
				{
					char *p;

					p = strrchr(dir, '\\');
					if(p)
						*p = '\0';
					else
						dir[0] = '\0';
				}
			}
			GetShortPathNameA(dir,dir,size);
		}
		return dir;
	}

	int GetCachedPassword(const char *key, char *buffer, int buffer_len)
	{
		CSocketIO sock;
#ifdef _WIN32
		struct _str
		{
			int state;
			char key[256];
			char rslt[256];
		};
		COPYDATASTRUCT cds;

		HWND hWnd=FindWindow(L"CvsAgent",NULL);
		if(!hWnd)
			return -1;
		cvs::string name;
		cvs::sprintf(name,32,"%08x:%08x",GetCurrentProcessId(),GetTickCount());
		HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(_str),name.c_str());
		_str* pMap = (_str*)MapViewOfFile(hMap,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(_str));
		memset(pMap,0,sizeof(_str));
		strcpy(pMap->key,key);
		pMap->state=1;
		cds.lpData=(PVOID)name.c_str();
		cds.cbData=(DWORD)name.size()+1;
		SendMessage(hWnd,WM_COPYDATA,NULL,(LPARAM)&cds);
		if(pMap->state==0)
			strncpy(buffer,pMap->rslt,buffer_len);
		int ret = pMap->state==0?0:-1;
		UnmapViewOfFile(pMap);
		CloseHandle(hMap);
		return ret;
#else
		return 0;
#endif
	}
	int SetCachedPassword(const char *key, const char *buffer)
	{
		CSocketIO sock;
#ifdef _WIN32
		struct _str
		{
			int state;
			char key[256];
			char rslt[256];
		};
		COPYDATASTRUCT cds;

		HWND hWnd=FindWindow(L"CvsAgent",NULL);
		if(!hWnd)
			return -1;
		cvs::string name;
		cvs::sprintf(name,32,"%08x:%08x",GetCurrentProcessId(),GetTickCount());
		HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(_str),name.c_str());
		_str* pMap = (_str*)MapViewOfFile(hMap,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(_str));
		memset(pMap,0,sizeof(_str));
		strcpy(pMap->key,key);
		if(buffer)
		{
			strcpy(pMap->rslt,buffer);
			pMap->state=2;
		}
		else
			pMap->state=3;
		cds.lpData=(PVOID)name.c_str();
		cds.cbData=(DWORD)name.size()+1;
		SendMessage(hWnd,WM_COPYDATA,NULL,(LPARAM)&cds);
		int state = pMap->state;
		UnmapViewOfFile(pMap);
		CloseHandle(hMap);
		return state==0?0:-1;
#else
		return 0;
#endif
	}
};

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len)
{
	/* Special case for the 'cvspass' key */
	if((!product || !strcmp(product,"cvsnt")) && key && !strcmp(key,"cvspass") && !GetCachedPassword(value,buffer,buffer_len))
		return 0;

	return _GetUserValue(product,key,value,buffer,buffer_len);
}

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, int& ival)
{
	char tmp[32];
	if(_GetUserValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	ival = atoi(tmp);
	return 0;
}

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, cvs::string& sval)
{
	char tmp[512];
	if(_GetUserValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	sval = tmp;
	return 0;
}

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, LONGINT& lival)
{
	char tmp[512];
	if(_GetUserValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	if(!sscanf(tmp,"%I64d",&lival))
		lival=0;
	return 0;
}

int CGlobalSettings::_GetUserValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len)
{
	HKEY hKey,hSubKey;
	DWORD dwType,dwLen,dw;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\Cvsnt";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,KEY_READ,&hKey))
		return -1; // Couldn't open or create key

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_READ,&hSubKey))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	dwType=REG_SZ;
	dwLen=buffer_len;
	if((dw=RegQueryValueExA(hKey,value,NULL,&dwType,(LPBYTE)buffer,&dwLen))!=0)
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);
	if(dwType==REG_DWORD && buffer)
		sprintf(buffer,"%u",*(DWORD*)buffer);

	return 0;
}

int CGlobalSettings::SetUserValue(const char *product, const char *key, const char *value, const char *buffer)
{
	if((!product || !strcmp(product,"cvsnt")) && key && !strcmp(key,"cvspass") && !SetCachedPassword(value,buffer) && buffer)
		return 0;
	return _SetUserValue(product,key,value,buffer);
}

int CGlobalSettings::SetUserValue(const char *product, const char *key, const char *value, LONGINT lival)
{
	char tmp[128];
	sprintf(tmp,"%I64d",lival);
	return _SetUserValue(product,key,value,tmp);
}

int CGlobalSettings::_SetUserValue(const char *product, const char *key, const char *value, const char *buffer)
{
	HKEY hKey,hSubKey;
	DWORD dwLen;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\Cvsnt";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,KEY_READ|KEY_WRITE,&hKey) &&
	   RegCreateKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,NULL,0,KEY_READ|KEY_WRITE,NULL,&hKey,NULL))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_WRITE,&hSubKey) &&
		   RegCreateKeyExA(hKey,key,0,NULL,0,KEY_WRITE,NULL,&hSubKey,NULL))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	if(!buffer)
	{
		RegDeleteValueA(hKey,value);
	}
	else
	{
		dwLen=(DWORD)strlen(buffer);
		if(RegSetValueExA(hKey,value,0,REG_SZ,(LPBYTE)buffer,dwLen+1))
		{
			RegCloseKey(hKey);
			return -1;
		}
	}
	RegCloseKey(hKey);

	return 0;
}

int CGlobalSettings::SetUserValue(const char *product, const char *key, const char *value, int ival)
{
	HKEY hKey,hSubKey;
	DWORD dwLen;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\Cvsnt";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,KEY_READ|KEY_WRITE,&hKey) &&
	   RegCreateKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,NULL,0,KEY_READ|KEY_WRITE,NULL,&hKey,NULL))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_WRITE,&hSubKey) &&
		   RegCreateKeyExA(hKey,key,0,NULL,0,KEY_WRITE,NULL,&hSubKey,NULL))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	DWORD dw = ival;
	dwLen=(DWORD)sizeof(DWORD);
	if(RegSetValueExA(hKey,value,0,REG_DWORD,(LPBYTE)&dw,dwLen))
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);

	return 0;
}

int CGlobalSettings::EnumUserValues(const char *product, const char *key, int value_num, char *value, int value_len, char *buffer, int buffer_len)
{
	HKEY hKey,hSubKey;
	DWORD dwType,dwLen,dwValLen;
	DWORD dwRes;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\Cvsnt";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,KEY_READ,&hKey))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_READ,&hSubKey))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	dwLen=buffer_len;
	dwValLen=value_len;
	if((dwRes=RegEnumValueA(hKey,value_num,value,&dwValLen,NULL,&dwType,(LPBYTE)buffer,&dwLen))!=0 && dwRes!=234)
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);
	if(dwType==REG_DWORD)
	    sprintf(buffer,"%u",*(DWORD*)buffer);

	return 0;
}

int CGlobalSettings::EnumUserKeys(const char *product, const char *key, int value_num, char *value, int value_len)
{
	HKEY hKey,hSubKey;
	DWORD dwValLen;
	DWORD dwRes;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\Cvsnt";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,KEY_READ,&hKey))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_READ,&hSubKey))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	dwValLen=value_len;
	if((dwRes=RegEnumKeyExA(hKey,value_num,value,&dwValLen,NULL,NULL,NULL,NULL))!=0)
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);

	return 0;
}

int CGlobalSettings::DeleteUserKey(const char *product, const char *key)
{
	HKEY hKey;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\Cvsnt";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_CURRENT_USER,regkey.c_str(),0,KEY_READ|KEY_WRITE,&hKey))
		return -1; // Couldn't open or create key

	RegDeleteKeyA(hKey,key);

	RegCloseKey(hKey);

	return 0;
}


int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, int& ival)
{
	char tmp[32];
	if(GetGlobalValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	ival = atoi(tmp);
	return 0;
}

int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, cvs::string& sval)
{
	char tmp[512];
	if(GetGlobalValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	sval = tmp;
	return 0;
}

int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, LONGINT& lival)
{
	char tmp[512];
	if(GetGlobalValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	if(!sscanf(tmp,"%I64d",&lival))
		lival=0;
	return 0;
}

int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len)
{
	HKEY hKey,hSubKey;
	DWORD dwType,dwLen;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\CVS";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,KEY_READ,&hKey))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_READ,&hSubKey))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	dwType=REG_SZ;
	dwLen=buffer_len;
	if(RegQueryValueExA(hKey,value,NULL,&dwType,(LPBYTE)buffer,&dwLen))
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);
	if(dwType==REG_DWORD && buffer)
	    sprintf(buffer,"%u",*(DWORD*)buffer);

	return 0;
}

int CGlobalSettings::SetGlobalValue(const char *product, const char *key, const char *value, LONGINT lival)
{
	char tmp[128];
	sprintf(tmp,"%I64d",lival);
	return SetGlobalValue(product,key,value,tmp);
}

int CGlobalSettings::SetGlobalValue(const char *product, const char *key, const char *value, const char *buffer)
{
	HKEY hKey,hSubKey;
	DWORD dwLen;
	cvs::string regkey;

	if(!isAdmin())
		return -1;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\CVS";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,KEY_READ|KEY_WRITE,&hKey) &&
	   RegCreateKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,NULL,0,KEY_READ|KEY_WRITE,NULL,&hKey,NULL))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_WRITE,&hSubKey) &&
		   RegCreateKeyExA(hKey,key,0,NULL,0,KEY_WRITE,NULL,&hSubKey,NULL))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	if(!buffer)
	{
		RegDeleteValueA(hKey,value);
	}
	else
	{
		dwLen=(DWORD)strlen(buffer);
		if(RegSetValueExA(hKey,value,0,REG_SZ,(LPBYTE)buffer,dwLen+1))
		{
			RegCloseKey(hKey);
			return -1;
		}
	}
	RegCloseKey(hKey);

	return 0;
}

int CGlobalSettings::SetGlobalValue(const char *product, const char *key, const char *value, int ival)
{
	HKEY hKey,hSubKey;
	DWORD dwLen;
	cvs::string regkey;

	if(!isAdmin())
		return -1;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\CVS";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,KEY_READ|KEY_WRITE,&hKey) &&
	   RegCreateKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,NULL,0,KEY_READ|KEY_WRITE,NULL,&hKey,NULL))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_WRITE,&hSubKey) &&
		   RegCreateKeyExA(hKey,key,0,NULL,0,KEY_WRITE,NULL,&hSubKey,NULL))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	DWORD dw = (DWORD)ival,err;
	dwLen=(DWORD)sizeof(DWORD);
	err = RegSetValueExA(hKey,value,0,REG_DWORD,(LPBYTE)&dw,dwLen);
	if(err)
	{
		dw = GetLastError();
		RegCloseKey(hKey);
		return -1;
	}

	RegCloseKey(hKey);

	return 0;
}

int CGlobalSettings::EnumGlobalValues(const char *product, const char *key, int value_num, char *value, int value_len, char *buffer, int buffer_len)
{
	HKEY hKey,hSubKey;
	DWORD dwType,dwLen,dwValLen;
	DWORD dwRes;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\CVS";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,KEY_READ,&hKey))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_READ,&hSubKey))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	dwLen=buffer_len;
	dwValLen=value_len;
	if((dwRes=RegEnumValueA(hKey,value_num,value,&dwValLen,NULL,&dwType,(LPBYTE)buffer,&dwLen))!=0 && dwRes!=234)
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);
	if(dwType==REG_DWORD && buffer)
	    sprintf(buffer,"%u",*(DWORD*)buffer);

	return 0;
}

int CGlobalSettings::EnumGlobalKeys(const char *product, const char *key, int value_num, char *value, int value_len)
{
	HKEY hKey,hSubKey;
	DWORD dwValLen;
	DWORD dwRes;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\CVS";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,KEY_READ,&hKey))
	{
		return -1; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyExA(hKey,key,0,KEY_READ,&hSubKey))
		{
			RegCloseKey(hKey);
			return -1; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	dwValLen=value_len;
	if((dwRes=RegEnumKeyExA(hKey,value_num,value,&dwValLen,NULL,NULL,NULL,NULL))!=0)
	{
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);

	return 0;
}

int CGlobalSettings::DeleteGlobalKey(const char *product, const char *key)
{
	HKEY hKey;
	cvs::string regkey;

	if(!product || !strcmp(product,"cvsnt"))
		regkey="Software\\CVS";
	else
		cvs::sprintf(regkey,64,"Software\\March Hare Software Ltd\\%s",product);

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,regkey.c_str(),0,KEY_READ|KEY_WRITE,&hKey))
	{
		return -1; // Couldn't open or create key
	}

	RegDeleteKeyA(hKey,key);

	RegCloseKey(hKey);

	return 0;
}

const char *CGlobalSettings::GetConfigDirectory()
{
	return GetCvsDir(cvs_config_dir,sizeof(cvs_config_dir));
}

const char *CGlobalSettings::GetLibraryDirectory(GLDType type)
{
	/* At some point replace this with something more complex.  For now
	   it serves the purpose */
	const char *cvsDir = GetCvsDir(cvs_library_dir,sizeof(cvs_library_dir));
	static const char *cvsDirProtocols = NULL;
	static const char *cvsDirTriggers = NULL;
	static const char *cvsDirXdiff = NULL;
	static const char *cvsDirMdns = NULL;
	static const char *cvsDirDatabase = NULL;
	switch(type)
	{
		case GLDLib:
			return cvsDir;
		case GLDProtocols:
			if(!cvsDirProtocols)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/protocols";
				cvsDirProtocols = strdup(cv.c_str());
			}
			return cvsDirProtocols;
		case GLDTriggers:
			if(!cvsDirTriggers)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/triggers";
				cvsDirTriggers = strdup(cv.c_str());
			}
			return cvsDirTriggers;
		case GLDXdiff:
			if(!cvsDirXdiff)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/xdiff";
				cvsDirXdiff = strdup(cv.c_str());
			}
			return cvsDirXdiff;
		case GLDMdns:
			if(!cvsDirMdns)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/mdns";
				cvsDirMdns = strdup(cv.c_str());
			}
			return cvsDirMdns;
		case GLDDatabase:
			if(!cvsDirDatabase)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/database";
				cvsDirDatabase = strdup(cv.c_str());
			}
			return cvsDirDatabase;
		default:
			return cvsDir;
	}
}

bool CGlobalSettings::SetConfigDirectory(const char *directory)
{
	if(CFileAccess::type(directory)!=CFileAccess::typeDirectory)
		return false;
	strcpy(cvs_config_dir,directory);
	return true;
}

bool CGlobalSettings::SetLibraryDirectory(const char *directory)
{
	if(CFileAccess::type(directory)!=CFileAccess::typeDirectory)
		return false;
	strcpy(cvs_library_dir,directory);
	return true;
}

const char *CGlobalSettings::GetCvsCommand()
{
	if(!*cvs_command) /* Only do this once */
	{
		HMODULE hModule = GetModuleHandleA("cvs.exe");
		if(!hModule) hModule = GetModuleHandleA("cvsnt.exe");
		if (hModule && GetModuleFileNameA(hModule, cvs_command, sizeof(cvs_command)))
		{
			GetShortPathNameA(cvs_command,cvs_command,sizeof(cvs_command));
		}
		else
		{
			hModule = GetModuleHandleA("cvstools.dll");
			if (hModule && GetModuleFileNameA(hModule, cvs_command, sizeof(cvs_command)))
			{
				char *p;

				p = strrchr(cvs_command, '\\');
				if(p)
					*p = '\0';
				else
					cvs_command[0] = '\0';
			}
			GetShortPathNameA(cvs_command,cvs_command,sizeof(cvs_command));
			strcat(cvs_command,"\\cvs.exe");
		}

		if(!CFileAccess::exists(cvs_command))
		{
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","InstallPath",cvs_command,sizeof(cvs_command)) ||
			   !CGlobalSettings::GetUserValue("cvsnt","PServer","InstallPath",cvs_command,sizeof(cvs_command)))
			{
				GetShortPathNameA(cvs_command,cvs_command,sizeof(cvs_command));
				strcat(cvs_command,"\\cvs.exe");
				if(!CFileAccess::exists(cvs_command))
					*cvs_command='\0';
			}
		}
		if(!*cvs_command)
			strcpy(cvs_command,"cvs.exe");
	}
	return cvs_command;
}

bool CGlobalSettings::SetCvsCommand(const char *command)
{
	return false; // Not on win32
}

// Basically Microsoft 118626
// Needed for vista as it fakes the admin rights on the registry and screws everything up
bool CGlobalSettings::isAdmin()
{
	static int isAd = 0;
	bool   fReturn         = false;
	DWORD  dwStatus;
	DWORD  dwAccessMask;
	DWORD  dwAccessDesired;
	DWORD  dwACLSize;
	DWORD  dwStructureSize = sizeof(PRIVILEGE_SET);
	PACL   pACL            = NULL;
	PSID   psidAdmin       = NULL;

	HANDLE hToken              = NULL;
	HANDLE hImpersonationToken = NULL;

	PRIVILEGE_SET   ps;
	GENERIC_MAPPING GenericMapping;

	PSECURITY_DESCRIPTOR     psdAdmin           = NULL;
	SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;

	if(isAd)
		return isAd>0?true:false;

	__try
	{
		if (!OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE|TOKEN_QUERY, TRUE, &hToken))
		{
			if (GetLastError() != ERROR_NO_TOKEN)
				__leave;

			if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE|TOKEN_QUERY, &hToken))
				__leave;
		}

		if (!DuplicateToken (hToken, SecurityImpersonation, &hImpersonationToken))
			__leave;


		if (!AllocateAndInitializeSid(&SystemSidAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0, 0, 0, 0, 0, 0, &psidAdmin))
			__leave;

		psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		if (psdAdmin == NULL)
			__leave;

		if (!InitializeSecurityDescriptor(psdAdmin, SECURITY_DESCRIPTOR_REVISION))
			__leave;

		// Compute size needed for the ACL.
		dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psidAdmin) - sizeof(DWORD);

		pACL = (PACL)LocalAlloc(LPTR, dwACLSize);
		if (pACL == NULL)
			__leave;

		if (!InitializeAcl(pACL, dwACLSize, ACL_REVISION2))
			__leave;

		dwAccessMask = ACCESS_READ | ACCESS_WRITE;

		if (!AddAccessAllowedAce(pACL, ACL_REVISION2, dwAccessMask, psidAdmin))
			__leave;

		if (!SetSecurityDescriptorDacl(psdAdmin, TRUE, pACL, FALSE))
			__leave;

		SetSecurityDescriptorGroup(psdAdmin, psidAdmin, FALSE);
		SetSecurityDescriptorOwner(psdAdmin, psidAdmin, FALSE);

		if (!IsValidSecurityDescriptor(psdAdmin))
			__leave;

		dwAccessDesired = ACCESS_READ;

		GenericMapping.GenericRead    = ACCESS_READ;
		GenericMapping.GenericWrite   = ACCESS_WRITE;
		GenericMapping.GenericExecute = 0;
		GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

		BOOL bRet;
		if (!AccessCheck(psdAdmin, hImpersonationToken, dwAccessDesired,
						&GenericMapping, &ps, &dwStructureSize, &dwStatus,
						&bRet))
			__leave;
		fReturn = bRet?true:false;
	}
	__finally
	{
		// Clean up.
		if (pACL) LocalFree(pACL);
		if (psdAdmin) LocalFree(psdAdmin);
		if (psidAdmin) FreeSid(psidAdmin);
		if (hImpersonationToken) CloseHandle (hImpersonationToken);
		if (hToken) CloseHandle (hToken);
	}

	isAd=fReturn?1:-1;

	return fReturn;
}
