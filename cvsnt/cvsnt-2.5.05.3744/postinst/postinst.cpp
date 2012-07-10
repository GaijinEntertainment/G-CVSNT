/*	cvsnt postinstallation
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
// postinst.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

static void MigrateCvsPass();
static void MigrateRepositories();
static void MigrateDomainSettings();
static void MigrateLegacySupport();

static HKEY hServerKey;

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	MigrateCvsPass();

	if(!RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\CVS\\PServer",0,KEY_READ|KEY_WRITE,&hServerKey))
	{
		MigrateRepositories();
		MigrateDomainSettings();
		MigrateLegacySupport();
		RegCloseKey(hServerKey);
	}
	return 0;
}

static std::string GetHomeDirectory()
{
	std::string path;
    char *hd, *hp;

    if ((hd = getenv ("HOME")))
		path=hd;
    else if ((hd = getenv ("HOMEDRIVE")) && (hp = getenv ("HOMEPATH")))
	{
		path=hd;
		path+=hp;
	}
    else
		path="";
	
	if(path.size() && (path[path.size()-1]=='\\' || path[path.size()-1]=='//'))
		path.resize(path.size()-1);
	return path;
}

static bool WriteRegistryKey(LPCTSTR key, LPCTSTR value, LPCTSTR buffer)
{
	HKEY hKey,hSubKey;
	DWORD dwLen;

	if(RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Cvsnt",0,KEY_READ,&hKey) &&
	   RegCreateKeyEx(HKEY_CURRENT_USER,"Software\\Cvsnt",0,NULL,0,KEY_READ,NULL,&hKey,NULL))
	{
		return false; // Couldn't open or create key
	}

	if(key)
	{
		if(RegOpenKeyEx(hKey,key,0,KEY_WRITE,&hSubKey) &&
		   RegCreateKeyEx(hKey,key,0,NULL,0,KEY_WRITE,NULL,&hSubKey,NULL))
		{
			RegCloseKey(hKey);
			return false; // Couldn't open or create key
		}
		RegCloseKey(hKey);
		hKey=hSubKey;
	}

	if(!buffer)
	{
		RegDeleteValue(hKey,value);
	}
	else
	{
		dwLen=strlen(buffer);
		if(RegSetValueEx(hKey,value,0,REG_SZ,(LPBYTE)buffer,dwLen+1))
		{
			RegCloseKey(hKey);
			return false;
		}
	}
	RegCloseKey(hKey);

	return true;
}

void MigrateCvsPass()
{
	std::string path = GetHomeDirectory();
	char line[1024],*key,*pw;

	if(!path.size())
		return; /* Nothing to migrate */

	path+="\\.cvspass";
	FILE *f = fopen(path.c_str(),"r");
	if(!f)
		return; /* No .cvspass file */

	while(fgets(line,1024,f)>0)
	{
		line[strlen(line)-1]='\0';
		key = strtok(line," ");
		if(key && key[0]=='/') /* This was added in 1.11.1.  Not sure why. */
			key=strtok(NULL," ");
		if(key)
			pw=key+strlen(key)+1;
		if(key && pw)
			WriteRegistryKey("cvspass",key,pw);
	}

	fclose(f);

	/* We might as well delete now.  cvsnt hasn't used this file for 3 years and
           the repeated migrations on upgrade will be confusing things. */
	DeleteFile(path.c_str());
}

#define MAX_REPOSITORIES 1024
struct RootStruct
{
	std::string root;
	std::string name;
	bool valid;
};
std::vector<RootStruct> Roots;

bool GetRootList()
{
	TCHAR buf[MAX_PATH],buf2[MAX_PATH],tmp[MAX_PATH];
	std::string prefix;
	DWORD bufLen;
	DWORD dwType;
	int drive;
	bool bModified = false;

	bufLen=sizeof(buf);
	if(!RegQueryValueEx(hServerKey,_T("RepositoryPrefix"),NULL,&dwType,(BYTE*)buf,&bufLen))
	{
		TCHAR *p = buf;
		while((p=_tcschr(p,'\\'))!=NULL)
			*p='/';
		p=buf+_tcslen(buf)-1;
		if(*p=='/')
			*p='\0';
		prefix = buf;
		bModified = true; /* Save will delete this value */
	}

	drive = _getdrive() + 'A' - 1;

	for(int n=0; n<MAX_REPOSITORIES; n++)
	{
		_sntprintf(tmp,sizeof(tmp),_T("Repository%d"),n);
		bufLen=sizeof(buf);
		if(RegQueryValueEx(hServerKey,tmp,NULL,&dwType,(BYTE*)buf,&bufLen))
			continue;
		if(dwType!=REG_SZ)
			continue;

		TCHAR *p = buf;
		while((p=_tcschr(p,'\\'))!=NULL)
			*p='/';

		_sntprintf(tmp,sizeof(tmp),_T("Repository%dName"),n);
		bufLen=sizeof(buf2);
		if(RegQueryValueEx(hServerKey,tmp,NULL,&dwType,(BYTE*)buf2,&bufLen))
		{
			_tcscpy(buf2,buf);
			if(prefix.size() && !_tcsnicmp(prefix.c_str(),buf,prefix.size()))
				_tcscpy(buf2,&buf[prefix.size()]);
			else
				_tcscpy(buf2,buf);
			if(buf[1]!=':')
				_sntprintf(buf,sizeof(buf),_T("%c:%s"),drive,buf2);

			p=buf2+_tcslen(buf2)-1;
			if(*p=='/')
				*p='\0';

			bModified = true;
		}
		else if(dwType!=REG_SZ)
			continue;

		RootStruct r;
		r.root = buf;
		r.name = buf2;
		r.valid = true;

		Roots.push_back(r);
	}
	return bModified;
}

void RebuildRootList()
{
	std::string path,desc;
	TCHAR tmp[64];
	int j;
	size_t n;

	for(n=0; n<MAX_REPOSITORIES; n++)
	{
		_sntprintf(tmp,sizeof(tmp),_T("Repository%d"),n);
		RegDeleteValue(hServerKey,tmp);
	}

	for(n=0,j=0; n<Roots.size(); n++)
	{
		path=Roots[n].root;
		desc=Roots[n].name;
		if(Roots[n].valid)
		{
			_sntprintf(tmp,sizeof(tmp),_T("Repository%d"),j);
			RegSetValueEx(hServerKey,tmp,NULL,REG_SZ,(BYTE*)path.c_str(),(path.length()+1)*sizeof(TCHAR));
			_sntprintf(tmp,sizeof(tmp),_T("Repository%dName"),j);
			RegSetValueEx(hServerKey,tmp,NULL,REG_SZ,(BYTE*)desc.c_str(),(desc.length()+1)*sizeof(TCHAR));
			j++;
		}
	}

	RegDeleteValue(hServerKey,_T("RepositoryPrefix"));
}

void MigrateRepositories()
{
	if(GetRootList())
		RebuildRootList();
}

void MigrateDomainSettings()
{
	DWORD dwVal,dwLen=sizeof(DWORD),dwType;
	char buf[256];
	if(!RegQueryValueEx(hServerKey,"DontUseDomain",NULL,&dwType,(BYTE*)&dwVal,&dwLen))
	{
		if(dwVal)
		{
			/* If dont use domain is set, force domain to computer name */
			/* The server will automatically pick up the domain otherwise */
			dwLen=sizeof(buf);
			GetComputerName(buf,&dwLen);
			RegSetValueEx(hServerKey,"DefaultDomain",0,REG_SZ,(BYTE*)buf,strlen(buf));
		}
		RegDeleteValue(hServerKey,"DontUseDomain");
	}
}

void MigrateLegacySupport()
{
	DWORD dwVal,dwLen=sizeof(DWORD),dwType;
	if(!RegQueryValueEx(hServerKey,"FakeUnixCvs",NULL,&dwType,(BYTE*)&dwVal,&dwLen))
	{
		if(dwVal)
		{
			RegSetValueEx(hServerKey,"Compat0_OldVersion",NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(dwVal));
			RegSetValueEx(hServerKey,"Compat0_OldCheckout",NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(dwVal));
			RegSetValueEx(hServerKey,"Compat0_HideStatus",NULL,REG_DWORD,(BYTE*)&dwVal,sizeof(dwVal));
			RegDeleteValue(hServerKey,"FakeUnixCvs");
		}
	}
}
