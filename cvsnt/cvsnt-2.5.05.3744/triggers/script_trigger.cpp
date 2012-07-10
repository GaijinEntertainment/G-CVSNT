/*
	CVSNT Windows Scripting trigger handler
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

#include <config.h>
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500
#include <windows.h>
#include <comutil.h>
#include <comdef.h>
#include <atlbase.h>
#include <atlcom.h>
#include <shlwapi.h>
#include <activscp.h>

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <ctype.h>

#define MODULE script

#include <map>
#include <cvstools.h>
#include "../version.h"

#include "Server.h"

int server_codepage;
IActiveScript *g_pEngine;

#ifndef SCRIPT_E_REPORTED
// KB: 247784
#define SCRIPT_E_REPORTED 0x80020101
#endif

#ifdef _WIN32
HMODULE g_hInst;

BOOL CALLBACK DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hInst = hModule;
	return TRUE;
}

#include "script_resource.h"

static int win32config(const struct plugin_interface *ui, void *wnd);

#endif

namespace {

static int CallDispatch(const wchar_t *name,variant_t* args, size_t nargs);
static _variant_t CallDispatchVariant(const wchar_t *name,variant_t* args, size_t nargs);
static IDispatch* GetItemList(long count, const char **names, const char **values);

class CMyModule : public CAtlModuleT< CMyModule >{};
CMyModule _AtlModule;

class ATL_NO_VTABLE CActiveScriptSite : 
	   public CComObjectRootEx<CComMultiThreadModel>,
	   public CComCoClass<IActiveScriptSite>,
	   public IActiveScriptSite
{
    BEGIN_COM_MAP(CActiveScriptSite)
     COM_INTERFACE_ENTRY(IActiveScriptSite)
    END_COM_MAP()
public:
	STDMETHOD(GetLCID)(LCID *plcid) { *plcid=GetThreadLocale(); return S_OK; }
	STDMETHOD(GetItemInfo)(LPCOLESTR pstrName, DWORD dwReturnMask, IUnknown **ppunkItem, ITypeInfo **ppTypeInfo)
	{
		if(!wcscmp(pstrName,L"Server"))
		{
			CServer *pServer = new CComObject<CServer>;
			if(dwReturnMask&SCRIPTINFO_IUNKNOWN)
				pServer->QueryInterface(IID_IUnknown,(void**)ppunkItem);
			if(dwReturnMask&SCRIPTINFO_ITYPEINFO)
				pServer->GetTypeInfo(0,0,ppTypeInfo);
			return S_OK;
		}
		return TYPE_E_ELEMENTNOTFOUND;
	}
	STDMETHOD(GetDocVersionString)(BSTR *pbstrVersionString) { return E_NOTIMPL; }
	STDMETHOD(OnScriptTerminate)(const VARIANT *pvarResult, const EXCEPINFO *pexcepinfo) { return S_OK; }
	STDMETHOD(OnStateChange)(SCRIPTSTATE ssScriptState) { return S_OK; }
	STDMETHOD(OnScriptError)(IActiveScriptError *pase)
	{
		EXCEPINFO excep;
		pase->GetExceptionInfo(&excep);
		if(excep.pfnDeferredFillIn)
			excep.pfnDeferredFillIn(&excep);
		cvs::wstring strText;
		if(excep.bstrDescription)
			cvs::swprintf(strText,128,L"Script error: %s",excep.bstrDescription);
		else
			cvs::swprintf(strText,128,L"Script error: %08x",excep.scode);
		CServerIo::error("%S\n",strText.c_str());
		LONG lColumn = 0;
		ULONG uLine = 0;
		DWORD dwDummyContext = 0;
		
		pase->GetSourcePosition(&dwDummyContext, &uLine, &lColumn);
		CServerIo::error("Line %u, Column %d.\n", uLine, lColumn);
		
		BSTR bstrLine=0;
		pase->GetSourceLineText(&bstrLine);
		if (NULL != bstrLine)
			CServerIo::error("%S\n", bstrLine);

		return S_OK; 
	}
	STDMETHOD(OnEnterScript)() { return S_OK; }
	STDMETHOD(OnLeaveScript)() { return S_OK; }
};


class _ubstr_t
{
public:
	_ubstr_t(const char *str)
	{
		if(str)
		{
			int l = strlen(str)*2+2;
			wchar_t *wstr = new wchar_t[l];
			MultiByteToWideChar(server_codepage,0,str,-1,wstr,l);
			bStr = SysAllocString(wstr);
			delete[] wstr;
		}
		else
			bStr=NULL;
	}
	~_ubstr_t()
	{
		if(bStr)
			SysFreeString(bStr);
	}
	operator BSTR() { return bStr; }
	BSTR Detach() { BSTR b = bStr; bStr=NULL; return b; }
protected:
	BSTR bStr;
};

int init(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
{
	const wchar_t *szLang;
	DWORD dwLang = sizeof(szLang)/sizeof(szLang[0]);
	cvs::string dstr;
	cvs::string str;
	char value[256];
	int val = 0;
	int globalval = 0;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","ScriptTrigger",value,sizeof(value)))
		val = atoi(value);
	if(!val)
	{
		CServerIo::trace(3,"Script trigger not enabled.");
		return -1;
	}
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","GlobalScriptTrigger",value,sizeof(value)))
		globalval = atoi(value);

        cvs::sprintf(str,80,"%s/CVSROOT/script",physical_repository);
	cvs::sprintf(dstr,80,"%s/CVSROOT/script",PATCH_NULL(virtual_repository));
	if((globalval!=0)&&(CFileAccess::exists((str+".name").c_str())))
	{ 
	        // potential security risks about - but if the user has enabled ...
	        CFileAccess ac;
	        if(!ac.open((str+".name").c_str(),"r"))
	        {
		        CServerIo::error("Couldn't open script name file: %s\n",strerror(errno));
		        return -1;
	        }

	        size_t length = (size_t)ac.length();
	        char *buf = new char[length];

	        length = ac.read(buf,length);
	        ac.close();
                // First line is file name; clip it there.
                char *c = strchr (buf, '\n');
                if (NULL != c)
                        *c = '\0';

                c = strchr (buf, '\r');
                if (NULL != c)
                        *c = '\0';
                
	        str = buf;
	        dstr = buf;
	        delete[] buf;
	        CServerIo::trace (2, "Found script name: %s\n",dstr.c_str());
        }

	if(CFileAccess::exists((str+".vbs").c_str()))
		{ szLang=L"VBScript"; str+=".vbs"; }
	else if(CFileAccess::exists((str+".js").c_str()))
		{ szLang=L"JScript"; str+=".js"; }
	else if(CFileAccess::exists((str+".pl").c_str()))
		{ szLang=L"PerlScript"; str+=".pl"; }
	else if(CFileAccess::exists((str+".py").c_str()))
		{ szLang=L"PythonScript"; str+=".py"; }
	else if(CFileAccess::exists((str+".rb").c_str()))
		{ szLang=L"RubyScript"; str+=".rb"; }
	else
	{
		g_pEngine=NULL;
		return 0;
	}

	CServerIo::trace (2, "Opening script file: %s\n",dstr.c_str());

	// On Win32 there are really only two options.. UTF8 or ANSI
	if(!strcmp(character_set,"UTF-8"))
		server_codepage=CP_UTF8;
	else
		server_codepage=CP_ACP;

	CLSID clsid;
	if(CLSIDFromProgID(szLang,&clsid)!=S_OK)
	{
		CServerIo::error("Couldn't load script engine for '%S'\n",szLang);
		return -1;
	}

	if(CoCreateInstance(clsid,NULL,CLSCTX_INPROC_SERVER,IID_IActiveScript,(void**)&g_pEngine)!=S_OK)
	{
		CServerIo::error("Couldn't load script engine for '%S'\n",szLang);
		return -1;
	}

	IActiveScriptParse *pParse;

	if(g_pEngine->QueryInterface(IID_IActiveScriptParse,(void**)&pParse)!=S_OK)
	{
		g_pEngine->Release();
		g_pEngine=NULL;
		CServerIo::error("Couldn't initialise parser '%S'\n",szLang);
		return -1;
	}

	g_pEngine->SetScriptSite(new CComObject<CActiveScriptSite>);

	if(pParse->InitNew()!=S_OK)
	{
		pParse->Release();
		g_pEngine->Release();
		g_pEngine=NULL;
		CServerIo::error("Couldn't initialise parser '%S'\n",szLang);
		return -1;
	}

	cvs::wstring strText;

	CFileAccess ac;
	if(!ac.open(str.c_str(),"r"))
	{
		CServerIo::error("Couldn't open script file: %s\n",strerror(errno));
		pParse->Release();
		g_pEngine->Release();
		g_pEngine=NULL;
		return -1;
	}

	size_t length = (size_t)ac.length();
	char *buf = new char[length];

	length = ac.read(buf,length);
	ac.close();

	if(buf[0]==0xef && buf[1]==0xbb && buf[2]==0xbf)
	{
		strText.resize(length*2);
		strText.resize(MultiByteToWideChar(CP_UTF8,0,buf+3,length-3,(wchar_t*)strText.data(),strText.length()));
	} else if(buf[0]==0xff && buf[1]==0xfe)
	{
		strText.resize(length-2);
		memcpy((wchar_t*)strText.data(),buf+2,length-2);
	} else if(buf[0]==0xfe && buf[1]==0xff)
	{
		pParse->Release();
		g_pEngine->Release();
		g_pEngine=NULL;
		CServerIo::error("script error: UCS2-LE not supported yet\n");
		return -1;
	} else
	{
		strText.resize(length*2);
		strText.resize(MultiByteToWideChar(CP_ACP,0,buf,length,(wchar_t*)strText.data(),strText.length()));
	}
	delete[] buf;

	EXCEPINFO excep = {0};
	HRESULT hr;

	if((hr=pParse->ParseScriptText(strText.c_str(),NULL,NULL,NULL,0,0,SCRIPTTEXT_ISVISIBLE,NULL,&excep))!=S_OK)
	{
		if(excep.pfnDeferredFillIn)
			excep.pfnDeferredFillIn(&excep);

		if(excep.scode || excep.bstrDescription)
		{
			if(excep.bstrDescription)
				cvs::swprintf(strText,128,L"Script error: %s",excep.bstrDescription);
			else
				cvs::swprintf(strText,128,L"Script error: %08x",excep.scode);
			CServerIo::error("%S\n",strText.c_str());
		}
		pParse->Release();
		g_pEngine->Release();
		g_pEngine=NULL;
		return -1;
	}

	pParse->Release();

	hr=g_pEngine->SetScriptState(SCRIPTSTATE_STARTED);

	hr=g_pEngine->AddNamedItem(L"Server",SCRIPTITEM_ISVISIBLE);

	hr=g_pEngine->SetScriptState(SCRIPTSTATE_CONNECTED);

	_variant_t args[11];
	args[0]=_ubstr_t(command).Detach();
	args[1]=_ubstr_t(date).Detach();
	args[2]=_ubstr_t(hostname).Detach();
	args[3]=_ubstr_t(username).Detach();
	args[4]=_ubstr_t(virtual_repository).Detach();
	args[5]=_ubstr_t(physical_repository).Detach();
	args[6]=_ubstr_t(sessionid).Detach();
	args[7]=_ubstr_t(editor).Detach();
	IDispatch *pDispatch = GetItemList(count_uservar,uservar,userval);
	args[8]=pDispatch;
	pDispatch->Release();
	args[9]=_ubstr_t(client_version).Detach();
	args[10]=_ubstr_t(character_set).Detach();
	return CallDispatch(L"init", args, sizeof(args)/sizeof(args[0]));
}

int close(const struct trigger_interface_t* cb)
{
	if(!g_pEngine)
		return 0;

	CallDispatch(L"close",NULL,0);

	if(g_pEngine)
	{
		g_pEngine->SetScriptState(SCRIPTSTATE_DISCONNECTED);
		g_pEngine->InterruptScriptThread(SCRIPTTHREADID_ALL, NULL, 0);
		g_pEngine->Close();
		g_pEngine->Release();
	}
	return 0;
}

int pretag(const struct trigger_interface_t* cb, const char *message, const char *directory, int name_list_count, const char **name_list, const char **version_list, char tag_type, const char *action, const char *tag)
{
	if(!g_pEngine)
		return 0;

	variant_t args[6];
	wchar_t t[2] = { tag_type,0 };

	args[0]=_ubstr_t(message).Detach();
	args[1]=_ubstr_t(directory).Detach();
	IDispatch *pDispatch = GetItemList(name_list_count,name_list,version_list);
	args[2]=pDispatch;
	pDispatch->Release();
	args[3]=t;
	args[4]=_ubstr_t(action).Detach();
	args[5]=_ubstr_t(tag).Detach();

	return CallDispatch(L"taginfo",args,sizeof(args)/sizeof(args[0]));
}

int verifymsg(const struct trigger_interface_t* cb, const char *directory, const char *filename)
{
	if(!g_pEngine)
		return 0;

	variant_t args[2];

	args[0]=_ubstr_t(directory).Detach();
	args[1]=_ubstr_t(filename).Detach();

	return CallDispatch(L"verifymsg",args,sizeof(args)/sizeof(args[0]));
}

int loginfo(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list)
{
	if(!g_pEngine)
		return 0;

	variant_t args[4];

	args[0]=_ubstr_t(message).Detach();
	args[1]=_ubstr_t(status).Detach();
	args[2]=_ubstr_t(directory).Detach();

	IDispatch* pDispatch;
	CChangeInfoCollection *pColl = new CComObject<CChangeInfoCollection>;
	pColl->AddRef();
	pColl->m_vec.resize(change_list_count);
	for(long n=0; n<change_list_count; n++)
	{
		CChangeInfoStruct *pList = new CComObject<CChangeInfoStruct>;
		wchar_t t[2] = { change_list[n].type, 0 };

		pList->AddRef();
		pList->filename=_ubstr_t(change_list[n].filename).Detach();
		pList->rev_old=_ubstr_t(change_list[n].rev_old).Detach();
		pList->rev_new=_ubstr_t(change_list[n].rev_new).Detach();
		pList->tag=_ubstr_t(change_list[n].tag).Detach();
		pList->type=t;
		pList->bugid=_ubstr_t(change_list[n].bugid).Detach();
		pList->QueryInterface(IID_IDispatch,(void**)&pDispatch);
		pColl->m_vec[n]=pDispatch;
		pList->Release();
		pDispatch->Release();
	}
	pColl->QueryInterface(IID_IDispatch,(void**)&pDispatch);
	pColl->Release();
	args[3]=pDispatch;

	return CallDispatch(L"loginfo",args,sizeof(args)/sizeof(args[0]));
}

int history(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message)
{
	if(!g_pEngine)
		return 0;

	variant_t args[7];
	wchar_t t[2] = { type,0};

	args[0]=t;
	args[1]=_ubstr_t(workdir).Detach();
	args[2]=_ubstr_t(revs).Detach();
	args[3]=_ubstr_t(name).Detach();
	args[4]=_ubstr_t(bugid).Detach();
	args[5]=_ubstr_t(message).Detach();
	args[6]=_ubstr_t("").Detach();

	return CallDispatch(L"history",args,sizeof(args)/sizeof(args[0]));
}

int notify(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file)
{
	if(!g_pEngine)
		return 0;

	variant_t args[7];

	args[0]=_ubstr_t(message).Detach();
	args[1]=_ubstr_t(bugid).Detach();
	args[2]=_ubstr_t(directory).Detach();
	args[3]=_ubstr_t(notify_user).Detach();
	args[4]=_ubstr_t(tag).Detach();
	args[5]=_ubstr_t(type).Detach();
	args[6]=_ubstr_t(file).Detach();

	return CallDispatch(L"notify",args,sizeof(args)/sizeof(args[0]));
}

int precommit(const struct trigger_interface_t* cb, int name_list_count, const char **name_list, const char *message, const char *directory)
{
	if(!g_pEngine)
		return 0;

	variant_t args[3];

	SAFEARRAY *sa = SafeArrayCreateVector(VT_VARIANT, 0, name_list_count);
	for(long n=0; n<name_list_count; n++)
		SafeArrayPutElement(sa, &n, &_variant_t(_ubstr_t(name_list[n])).Detach());

	args[0].parray=sa;
	args[0].vt=VT_ARRAY|VT_VARIANT;
	args[1]=_ubstr_t(message).Detach();
	args[2]=_ubstr_t(directory).Detach();

	return CallDispatch(L"commitinfo",args,sizeof(args)/sizeof(args[0]));
}

int postcommit(const struct trigger_interface_t* cb, const char *directory)
{
	if(!g_pEngine)
		return 0;

	variant_t args[1];

	args[0]=_ubstr_t(directory).Detach();

	return CallDispatch(L"postcommit",args,sizeof(args)/sizeof(args[0]));
}

int precommand(const struct trigger_interface_t* cb, int argc, const char **argv)
{
	if(!g_pEngine)
		return 0;

	variant_t args[1];

	SAFEARRAY *sa = SafeArrayCreateVector(VT_VARIANT, 0, argc);
	for(long n=0; n<argc; n++)
		SafeArrayPutElement(sa, &n, &_variant_t(_ubstr_t(argv[n])).Detach());

	args[0].parray=sa;
	args[0].vt=VT_ARRAY|VT_VARIANT;

	return CallDispatch(L"precommand",args,sizeof(args)/sizeof(args[0]));
}

int postcommand(const struct trigger_interface_t* cb, const char *directory, int return_code)
{
	char rc[32];
	if(!g_pEngine)
		return 0;

	variant_t args[2];

	snprintf(rc,32,"%d",return_code);

	args[0]=_ubstr_t(directory).Detach();
	args[1]=_ubstr_t(rc).Detach();

	return CallDispatch(L"postcommand",args,sizeof(args)/sizeof(args[0]));
}

int premodule(const struct trigger_interface_t* cb, const char *module)
{
	if(!g_pEngine)
		return 0;

	variant_t args[1];

	args[0]=_ubstr_t(module).Detach();

	return CallDispatch(L"premodule",args,sizeof(args)/sizeof(args[0]));
}

int postmodule(const struct trigger_interface_t* cb, const char *module)
{
	if(!g_pEngine)
		return 0;

	variant_t args[1];

	args[0]=_ubstr_t(module).Detach();

	return CallDispatch(L"postmodule",args,sizeof(args)/sizeof(args[0]));
}

int get_template(const struct trigger_interface_t *cb, const char *directory, const char **template_ptr)
{
	if(!template_ptr)
		return 0;
	
	if(!g_pEngine)
		return 0;

	variant_t args[1];

	args[0]=_ubstr_t(directory).Detach();

	_variant_t ret = CallDispatchVariant(L"template",args,sizeof(args)/sizeof(args[0]));
	if(ret.vt==VT_NULL)
		return 0;
	if(ret.vt==VT_I4 && ret.lVal==-1)
		return -1;
    
	_bstr_t str(ret);
	char * const p = (char * const) malloc(wcslen(str)*3);
	p[WideCharToMultiByte(server_codepage,0,str,wcslen(str),p,wcslen(str)*3,NULL,NULL)]='\0';
        *template_ptr = p;
	return 0;
}

int parse_keyword(const struct trigger_interface_t *cb, const char *keyword,const char *directory,const char *file,const char *branch,const char *author,const char *printable_date,const char *rcs_date,const char *locker,const char *state,const char *version,const char *name,const char *bugid, const char *commitid, const property_info *props, size_t numprops, const char **value)
{
	if(!value)
		return 0;

	if(!g_pEngine)
		return 0;

	variant_t args[15];

	args[0]=_ubstr_t(keyword).Detach();
	args[1]=_ubstr_t(directory).Detach();
	args[2]=_ubstr_t(file).Detach();
	args[3]=_ubstr_t(branch).Detach();
	args[4]=_ubstr_t(author).Detach();
	args[5]=_ubstr_t(printable_date).Detach();
	args[6]=_ubstr_t(rcs_date).Detach();
	args[7]=_ubstr_t(locker).Detach();
	args[8]=_ubstr_t(state).Detach();
	args[9]=_ubstr_t(version).Detach();
	args[10]=_ubstr_t(name).Detach();
	args[11]=_ubstr_t(bugid).Detach();
	args[12]=_ubstr_t(commitid).Detach();

	long global = 0;
	for(long n=0; n<(long)numprops; n++)
	{
		if(props[n].isglobal)
			global++;
	}
	IDispatch* pDispatch;
	CItemListCollection *pCollGlobal = new CComObject<CItemListCollection>;
	CItemListCollection *pCollLocal = new CComObject<CItemListCollection>;
	pCollGlobal->AddRef();
	pCollLocal->AddRef();
	pCollGlobal->m_vec.resize(global);
	pCollLocal->m_vec.resize(numprops -global);
	for(long n=0,l=0,g=0; n<(long)numprops; n++)
	{
		CItemListStruct *pList = new CComObject<CItemListStruct>;
		pList->AddRef();
		pList->name=_ubstr_t(props[n].property).Detach();
		pList->value=_ubstr_t(props[n].value).Detach();
		pList->QueryInterface(IID_IDispatch,(void**)&pDispatch);
		if(props[n].isglobal)
			pCollGlobal->m_vec[g++]=pDispatch;
		else
			pCollLocal->m_vec[l++]=pDispatch;
		pDispatch->Release();
		pList->Release();
	}
	pCollGlobal->QueryInterface(IID_IDispatch,(void**)&pDispatch);
	pCollGlobal->Release();
	args[13]=pDispatch;
	pDispatch->Release();
	pCollLocal->QueryInterface(IID_IDispatch,(void**)&pDispatch);
	pCollLocal->Release();
	args[14]=pDispatch;
	pDispatch->Release();

	_variant_t ret = CallDispatchVariant(L"keywords",args,sizeof(args)/sizeof(args[0]));
	if(ret.vt==VT_NULL || (ret.vt==VT_I4 && ret.lVal==-1))
		return 0;

	_bstr_t str(ret);
	char * const p = ( char * const)malloc(wcslen(str)*3);
	p[WideCharToMultiByte(server_codepage,0,str,wcslen(str),p,wcslen(str)*3,NULL,NULL)]='\0';
        *value = p;
	return 0;
}

int prercsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	// return 0 - no diff (rcsdiff not called)
	// return 1 - Unified diff
	if(!g_pEngine)
		return 0;

	variant_t args[8];

	args[0]=_ubstr_t(file).Detach();
	args[1]=_ubstr_t(directory).Detach();
	args[2]=_ubstr_t(oldfile).Detach();
	args[3]=_ubstr_t(newfile).Detach();
	args[4]=_ubstr_t(type).Detach();
	args[5]=_ubstr_t(options).Detach();
	args[6]=_ubstr_t(oldversion).Detach();
	args[7]=_ubstr_t(newversion).Detach();

	return CallDispatch(L"prercsdiff",args,sizeof(args)/sizeof(args[0]));
}

int rcsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *diff, size_t difflen, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	if(!g_pEngine)
		return 0;

	variant_t args[11];

	args[0]=_ubstr_t(file).Detach();
	args[1]=_ubstr_t(directory).Detach();
	args[2]=_ubstr_t(oldfile).Detach();
	args[3]=_ubstr_t(newfile).Detach();
	args[4]=_ubstr_t(diff).Detach();
	args[5]=_ubstr_t(type).Detach();
	args[6]=_ubstr_t(options).Detach();
	args[7]=_ubstr_t(oldversion).Detach();
	args[8]=_ubstr_t(newversion).Detach();
	args[9]=(long)added;
	args[10]=(long)removed;

	return CallDispatch(L"rcsdiff",args,sizeof(args)/sizeof(args[0]));
}

int CallDispatch(const wchar_t *name,variant_t* args, size_t nargs)
{
	_variant_t v = CallDispatchVariant(name,args,nargs);
	if(v.vt==VT_NULL)
		return 0;
	try
	{
		v.ChangeType(VT_I4);
	}
	catch(_com_error& /*e*/)
	{
		CServerIo::error("[script parser] Return type from '%S' incorrect.  Must be null or numeric.\n",name);
		return -1;
	}
	return (int)(long)v;
}

_variant_t CallDispatchVariant(const wchar_t *name,variant_t* args, size_t nargs)
{
	_variant_t result;
	cvs::wstring strText;
	DISPID dispid;
	HRESULT hr;
	LPOLESTR _name = (LPOLESTR)name;

	IDispatch *pDispatch = NULL;
	g_pEngine->GetScriptDispatch(NULL,&pDispatch);
	if(pDispatch && (hr=pDispatch->GetIDsOfNames(IID_NULL,&_name,1,0,&dispid))==S_OK)
	{
		DISPPARAMS params = {0};
		EXCEPINFO excep = {0};

		if(nargs)
		{
			params.cArgs = nargs;
			params.rgvarg = new VARIANTARG[nargs];
			for(size_t n=0; n<nargs; n++)
			{
				params.rgvarg[n]=args[(nargs-n)-1];
			}
		}

		switch((hr=pDispatch->Invoke(dispid,IID_NULL,GetThreadLocale(),DISPATCH_METHOD,&params,&result,&excep,NULL)))
		{
		case S_OK:
		case DISP_E_MEMBERNOTFOUND: // Not an error
			break;
		case DISP_E_EXCEPTION:
			if(excep.pfnDeferredFillIn)
				excep.pfnDeferredFillIn(&excep);
			if(excep.bstrDescription)
				cvs::swprintf(strText,128,L"%s: Script error: %s",name,excep.bstrDescription);
			else
				cvs::swprintf(strText,128,L"%s: Script error: %08x",name,excep.scode);
			CServerIo::error("%S\n",strText.c_str());
			result = (long)-1;
			break;
		case SCRIPT_E_REPORTED: // Error already reported
			result = (long)-1;
			break;
		default:
			CServerIo::error("Bad function definition for '%S' (%08x)\n",name,hr);
			result = (long)-1;
			break;
		}

		delete params.rgvarg;
	}
	if(pDispatch)
		pDispatch->Release();
	return result;
}

IDispatch* GetItemList(long count, const char **names, const char **values)
{
	IDispatch* pDispatch;
	CItemListCollection *pColl = new CComObject<CItemListCollection>;
	pColl->AddRef();
	pColl->m_vec.resize(count);
	for(long n=0; n<count; n++)
	{
		CItemListStruct *pList = new CComObject<CItemListStruct>;
		pList->AddRef();
		pList->name=_ubstr_t(names[n]).Detach();
		pList->value=_ubstr_t(values[n]).Detach();
		pList->QueryInterface(IID_IDispatch,(void**)&pDispatch);
		pColl->m_vec[n]=pDispatch;
		pList->Release();
		pDispatch->Release();
	}
	pColl->QueryInterface(IID_IDispatch,(void**)&pDispatch);
	pColl->Release();
	return pDispatch;
}

} // anonymous namespace

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static trigger_interface callbacks =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"ActiveScript Scripting extension",CVSNT_PRODUCTVERSION_STRING,"ScriptTrigger",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		win32config
	#else
		NULL
	#endif
	},
	init,
	close,
	pretag,
	verifymsg,
	loginfo,
	history,
	notify,
	precommit,
	postcommit,
	precommand,
	postcommand,
	premodule,
	postmodule,
	get_template,
	parse_keyword
};

static int init(const struct plugin_interface *plugin)
{
	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitTrigger)
		return NULL;

	return (void*)&callbacks;
}

plugin_interface *get_plugin_interface()
{
	return &callbacks.plugin;
}

#ifdef _WIN32
BOOL CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char value[MAX_PATH];
	int nSel;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		nSel = 0;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","ScriptTrigger",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,nSel,NULL);
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,NULL,NULL);
			snprintf(value,sizeof(value),"%d",nSel);
			CGlobalSettings::SetGlobalValue("cvsnt","Plugins","ScriptTrigger",value);
		case IDCANCEL:
			EndDialog(hWnd,LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

int win32config(const struct plugin_interface *ui, void *wnd)
{
	HWND hWnd = (HWND)wnd;
	int ret = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, ConfigDlgProc);
	return ret==IDOK?0:-1;
}
#endif


