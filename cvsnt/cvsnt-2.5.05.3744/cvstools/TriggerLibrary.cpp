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
#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0400
#include <windows.h>
#endif

#include <cvsapi.h>
#include <errno.h>
#include "export.h"

#include "GlobalSettings.h"
#include "TriggerLibrary.h"
#include "plugin_interface.h"

#ifdef _WIN32
#include "../../version.h"

#include <comdef.h>
#include <objbase.h>

#include "trigger_h.h"
#endif


CTriggerLibrary::trigger_list_t CTriggerLibrary::trigger_list;

#ifdef _WIN32
namespace
{
	class _ubstr_t
	{
	public:
		_ubstr_t(const char *str)
		{
			if(str)
			{
				bStr = SysAllocString(CFileAccess::Win32Wide(str));
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

	int COM_init(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret;
		SAFEARRAY *sa_uservar = SafeArrayCreateVector(VT_BSTR, 0, count_uservar);
		SAFEARRAY *sa_userval = SafeArrayCreateVector(VT_BSTR, 0, count_uservar);
		for(long n=0; n<count_uservar; n++)
		{
			SafeArrayPutElement(sa_uservar, &n, _ubstr_t(uservar[n]).Detach());
			SafeArrayPutElement(sa_userval, &n, _ubstr_t(userval[n]).Detach());
		}
		ret = i->i4->init(_ubstr_t(command),_ubstr_t(date),_ubstr_t(hostname),_ubstr_t(virtual_repository),_ubstr_t(physical_repository),_ubstr_t(sessionid), _ubstr_t(editor), sa_uservar, sa_userval, _ubstr_t(client_version));
		SafeArrayDestroy(sa_uservar);
		SafeArrayDestroy(sa_userval);
		return ret;
	}

	int COM_pretag(const struct trigger_interface_t* cb, const char *message, const char *directory, int name_list_count, const char **name_list, const char **version_list, char tag_type, const char *action, const char *tag)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret;
		SAFEARRAY *sa_name_list = SafeArrayCreateVector(VT_BSTR, 0, name_list_count);
		SAFEARRAY *sa_version_list = SafeArrayCreateVector(VT_BSTR, 0, name_list_count);
		for(long n=0; n<name_list_count; n++)
		{
			SafeArrayPutElement(sa_name_list, &n, _ubstr_t(name_list[n]).Detach());
			SafeArrayPutElement(sa_version_list, &n, _ubstr_t(version_list[n]).Detach());
		}
		ret = i->i4->pretag(_ubstr_t(message),_ubstr_t(directory),sa_name_list,sa_version_list, tag_type,_ubstr_t(action),_ubstr_t(tag));
		SafeArrayDestroy(sa_name_list);
		SafeArrayDestroy(sa_version_list);
		return ret;
	}

	int COM_verifymsg(const struct trigger_interface_t* cb, const char *directory, const char *filename)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		return i->i4->verifymsg(_ubstr_t(directory),_ubstr_t(filename));
	}

	int COM_loginfo(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret;
		ChangeInfoStruct *pData;
		IRecordInfo *pRI;
		HRESULT hr;
		hr = GetRecordInfoFromGuids(LIBID_CVSNT, 1, 0, 0x409, __uuidof(ChangeInfoStruct), &pRI);
		if(FAILED(hr))
		{
			CServerIo::error("GetRecordInfoFromGuids returned 0x%x\n",hr);
			return -1;
		}
		SAFEARRAY *sa_change_list = SafeArrayCreateVectorEx(VT_RECORD, 0, change_list_count,pRI);
		pRI->Release();
		hr = SafeArrayAccessData(sa_change_list, (void**)&pData);
		if(FAILED(hr))
		{
			CServerIo::error("SafeArrayAccessData returned 0x%x\n",hr);
			return -1;
		}
		for(long n=0; n<change_list_count; n++)
		{
			pData[n].filename=_ubstr_t(change_list[n].filename).Detach();
			pData[n].rev_new=_ubstr_t(change_list[n].rev_new).Detach();
			pData[n].rev_old=_ubstr_t(change_list[n].rev_old).Detach();
			pData[n].tag=_ubstr_t(change_list[n].tag).Detach();
			pData[n].type=change_list[n].type;
			pData[n].bugid=_ubstr_t(change_list[n].bugid).Detach();
		}
		hr = SafeArrayUnaccessData(sa_change_list);
		ret = i->i4->loginfo(_ubstr_t(message),_ubstr_t(status),_ubstr_t(directory),sa_change_list);
		SafeArrayDestroy(sa_change_list);
		return ret;
	}

	int COM_history(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		if(i->i6)
			return i->i6->history(type,_ubstr_t(workdir),_ubstr_t(revs),_ubstr_t(name),_ubstr_t(bugid),_ubstr_t(message));
		else
			return i->i4->history(type,_ubstr_t(workdir),_ubstr_t(revs),_ubstr_t(name));
	}

	int COM_notify(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		return i->i4->notify(_ubstr_t(message),_ubstr_t(bugid),_ubstr_t(directory),_ubstr_t(notify_user),_ubstr_t(tag),_ubstr_t(type),_ubstr_t(file));
	}

	int COM_precommit(const struct trigger_interface_t* cb, int name_list_count, const char **name_list, const char *message, const char *directory)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret;
		SAFEARRAY *sa_name_list = SafeArrayCreateVector(VT_BSTR, 0, name_list_count);
		for(long n=0; n<name_list_count; n++)
			SafeArrayPutElement(sa_name_list, &n, (BSTR)_ubstr_t(name_list[n]).Detach());
		ret = i->i4->precommit(sa_name_list,_ubstr_t(message),_ubstr_t(directory));
		SafeArrayDestroy(sa_name_list);
		return ret;
	}

	int COM_precommand(const struct trigger_interface_t* cb, int argc, const char **argv)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret;
		SAFEARRAY *sa_precommand_list = SafeArrayCreateVector(VT_BSTR, 0, argc);
		for(long n=0; n<argc; n++)
			SafeArrayPutElement(sa_precommand_list, &n, (BSTR)_ubstr_t(argv[n]).Detach());
		ret = i->i4->precommand(sa_precommand_list);
		SafeArrayDestroy(sa_precommand_list);
		return ret;
	}

	int COM_postcommand(const struct trigger_interface_t* cb, const char *directory, int return_code)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		return i->i4->postcommand(_ubstr_t(directory));
	}

	int COM_postcommit(const struct trigger_interface_t* cb, const char *directory)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		return i->i4->postcommit(_ubstr_t(directory));
	}

	int COM_premodule(const struct trigger_interface_t* cb, const char *module)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		return i->i4->premodule(_ubstr_t(module));
	}

	int COM_postmodule(const struct trigger_interface_t* cb, const char *module)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		return i->i4->postmodule(_ubstr_t(module));
	}

	int COM_get_template(const struct trigger_interface_t *cb, const char *directory, const char **template_ptr)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret = 0;
		BSTR bTemplate;
		ret = i->i4->get_template(_ubstr_t(directory),&bTemplate);
		if(!ret)
		{
			wchar_t *wstr = bTemplate;
			char *str = strdup(CFileAccess::Win32Narrow(wstr));
			inf->to_free.push_back(str);
			*template_ptr = str;
		}
		return ret;
	}

	int COM_parse_keyword(const struct trigger_interface_t *cb, const char *keyword, const char *directory, const char *file,const char *branch,const char *author,const char *printable_date,const char *rcs_date,const char *locker,const char *state,const char *version,const char *name,const char *bugid,const char *commitid,const property_info *props, size_t numprops, const char **value)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;
		int ret = 0;
		BSTR bValue;
		PropertyInfoStruct *pData;
		IRecordInfo *pRI;
		HRESULT hr;

		if(!i->i5)
			return 0;

		hr = GetRecordInfoFromGuids(LIBID_CVSNT, 1, 0, 0x409, __uuidof(PropertyInfoStruct), &pRI);
		if(FAILED(hr))
		{
			CServerIo::error("GetRecordInfoFromGuids returned 0x%x\n",hr);
			return -1;
		}
		SAFEARRAY *sa_prop_list = SafeArrayCreateVectorEx(VT_RECORD, 0, (ULONG)numprops, pRI);
		pRI->Release();
		hr = SafeArrayAccessData(sa_prop_list, (void**)&pData);
		if(FAILED(hr))
		{
			CServerIo::error("SafeArrayAccessData returned 0x%x\n",hr);
			return -1;
		}
		for(size_t n=0; n<numprops; n++)
		{
			pData[n].property=_ubstr_t(props[n].property).Detach();
			pData[n].value=_ubstr_t(props[n].value).Detach();
			pData[n].isglobal = props[n].isglobal;
		}
		hr = SafeArrayUnaccessData(sa_prop_list);
		ret = i->i5->parse_keyword(_ubstr_t(keyword),_ubstr_t(directory),_ubstr_t(file),_ubstr_t(branch),_ubstr_t(author),_ubstr_t(printable_date),_ubstr_t(rcs_date),_ubstr_t(locker),_ubstr_t(state),_ubstr_t(name),_ubstr_t(version),_ubstr_t(bugid),_ubstr_t(commitid),sa_prop_list,&bValue);
		if(!ret)
		{
			wchar_t *wstr = bValue;
			char *str = strdup(CFileAccess::Win32Narrow(wstr));
			inf->to_free.push_back(str);
			*value = str;
		}
		SafeArrayDestroy(sa_prop_list);
		return ret;
	}

	int COM_prercsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile,
					const char *newfile, const char *type, const char *options,
					const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;

		if(i->i6)
			return i->i6->prercsdiff(_ubstr_t(file),_ubstr_t(directory),_ubstr_t(oldfile),_ubstr_t(newfile),_ubstr_t(type),_ubstr_t(options),_ubstr_t(oldversion),_ubstr_t(newversion),(long)added,(long)removed);
		else
			return 0;
	}

	int COM_rcsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile,
					const char *newfile, const char *diff, size_t difflen, const char *type, const char *options,
					const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;

		if(i->i6)
			return i->i6->rcsdiff(_ubstr_t(file),_ubstr_t(directory),_ubstr_t(oldfile),_ubstr_t(newfile),_ubstr_t(diff),_ubstr_t(type),_ubstr_t(options),_ubstr_t(oldversion),_ubstr_t(newversion),(long)added,(long)removed);
		else
			return 0;
	}


	int COM_close(const struct trigger_interface_t *cb)
	{
		CTriggerLibrary::trigger_info_t *inf = (CTriggerLibrary::trigger_info_t*)cb->plugin.__cvsnt_reserved;
		CTriggerLibrary::InfoStruct *i = &inf->is;

		i->i4->Release();
		if(i->i5)
			i->i5->Release();
		if(i->i6)
			i->i6->Release();

		return 0;
	}
}
#endif

const trigger_interface *CTriggerLibrary::LoadTrigger(const char *library, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
{
	trigger_interface *cb;

	cb = trigger_list[library];
	if(cb)
		return cb;

	CServerIo::trace(3,"LoadTrigger(%s)",library);

#ifdef _WIN32
	if(library[0]=='{')
	{
		/* COM filter */
		CLSID id;
		wchar_t str[128];
		const char *p = strchr(library,'}');
		if(!p)
			return 0;

		size_t n = (p-library)+1;
		if(n>=(sizeof(str)/sizeof(str[0])))
			n=(sizeof(str)/sizeof(str[0]))-1;
		wcsncpy(str,CFileAccess::Win32Wide(library),sizeof(str)/sizeof(str[0]));
		str[n]='\0';
		HRESULT hr = CLSIDFromString(str,&id);
		if(hr!=NOERROR)
		{
			str[wcslen(str)-1]='\0';
			hr = CLSIDFromProgID(str+1,&id);
			if(hr!=NOERROR)
				return NULL;
		}
		ICvsInfo4 *i4 = NULL;
		ICvsInfo5 *i5 = NULL;
		ICvsInfo6 *i6 = NULL;
		hr = CoCreateInstance(id,NULL,CLSCTX_ALL,IID_ICvsInfo4,(void**)&i4);
		if(FAILED(hr))
		{
			if(FAILED(hr))
			{
				CServerIo::trace(3,"Failed to read COM reference to {%S} - must support at least revision 4",str);
				return NULL;
			}
		}
		if(FAILED(i4->QueryInterface(IID_ICvsInfo5,(void**)&i5)))
			i5 = NULL;
		if(FAILED(i4->QueryInterface(IID_ICvsInfo6,(void**)&i6)))
			i6 = NULL;

		trigger_info_t *inf = new trigger_info_t;
		cb = new trigger_interface;
		inf->delete_trig = true;
		inf->is.i4 = i4;
		inf->is.i5 = i5;
		inf->is.i6 = i6;
		cb->plugin.__cvsnt_reserved = inf;
		cb->plugin.interface_version=PLUGIN_INTERFACE_VERSION;
		cb->plugin.description="COM Interface";
		cb->plugin.version=CVSNT_PRODUCTVERSION_STRING;
		cb->plugin.init=NULL;
		cb->plugin.destroy=NULL;
		cb->plugin.get_interface=NULL;
		cb->plugin.configure=NULL;
		cb->init=COM_init;
		cb->close=COM_close;
		cb->history=COM_history;
		cb->notify=COM_notify;
		cb->precommand=COM_precommand;
		cb->postcommand=COM_postcommand;
		cb->premodule=COM_premodule;
		cb->postmodule=COM_postmodule;
		cb->precommit=COM_precommit;
		cb->postcommit=COM_postcommit;
		cb->pretag=COM_pretag;
		cb->verifymsg=COM_verifymsg;
		cb->loginfo=COM_loginfo;
		cb->get_template=COM_get_template;
		cb->rcsdiff=COM_rcsdiff;
		cb->prercsdiff=COM_prercsdiff;
		cb->parse_keyword=COM_parse_keyword;
	}
	else
#endif
	{
		CLibraryAccess lib;
		cvs::filename fn;

		if(!lib.Load(library,CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDTriggers)))
			return NULL;

		get_plugin_interface_t pCvsInfo = (get_plugin_interface_t)lib.GetProc("get_plugin_interface");
		if(!pCvsInfo)
		{
			CServerIo::trace(3,"Library has no get_plugin_interface entrypoint.");
			return NULL;
		}
		plugin_interface *plug = pCvsInfo();
		if(!plug)
		{
			CServerIo::trace(3,"Library get_plugin_interface() failed.");
			return NULL;
		}
		if(plug->interface_version!=PLUGIN_INTERFACE_VERSION)
		{
			CServerIo::trace(3,"Library has wrong interface version.");
			return NULL;
		}

		if(plug->key)
		{
			char value[64];
			int val = 1;

			if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins",plug->key,value,sizeof(value)))
				val = atoi(value);
			if(!val)
			{
				CServerIo::trace(3,"Not loading disabled trigger %s.",library);
				return NULL;
			}
		}

		if(plug->init)
		{
			if(plug->init(plug))
			{
				CServerIo::trace(3,"Not loading Library - initialisation failed");
				return NULL;
			}
		}
		if(!plug->get_interface || (cb = (trigger_interface*)plug->get_interface(plug,pitTrigger,NULL))==NULL)
		{
			CServerIo::trace(3,"Library does not support trigger interface.");
			return NULL;
		}
		trigger_info_t *inf = new trigger_info_t;
		inf->pLib = lib.Detach();
		cb->plugin.__cvsnt_reserved = inf;
	}
	if(cb)
	{
		if(cb->init)
		{
			CServerIo::trace(3,"call library init with physical_repository=%s.",physical_repository);
			if(cb->init(cb, command, date, hostname, username, virtual_repository, physical_repository, sessionid, editor, count_uservar, uservar, userval, client_version, character_set))
			{
				trigger_info_t *inf = (trigger_info_t *)cb->plugin.__cvsnt_reserved;
				if(cb->plugin.destroy)
					cb->plugin.destroy(&cb->plugin);
				CLibraryAccess lib(inf->pLib);
				lib.Unload();
				for(size_t n=0; n<inf->to_free.size(); n++)
					free(inf->to_free[n]);
				if(inf->delete_trig)
					delete cb;
				delete inf;
				cb = NULL;
			}

		}
		if(cb)
			trigger_list[library]=cb;
	}
	return cb;
}

bool CTriggerLibrary::CloseTrigger(trigger_interface *cb)
{
	return true;
};

bool CTriggerLibrary::CloseAllTriggers()
{
	trigger_list_t::iterator i;
	for(i=trigger_list.begin();i!=trigger_list.end();++i)
	{
		if(i->second)
		{
			CServerIo::trace(3,"Unloading %s",i->first.c_str());

			trigger_info_t *inf = (trigger_info_t *)i->second->plugin.__cvsnt_reserved;

			if(i->second->close)
				i->second->close(i->second);

			if(i->second->plugin.destroy)
				i->second->plugin.destroy(&i->second->plugin);

			if(inf->pLib)
			{
				CLibraryAccess lib(inf->pLib);
				lib.Unload();
			}

			for(size_t n=0; n<inf->to_free.size(); n++)
				free(inf->to_free[n]);
			if(inf->delete_trig)
				delete i->second;
			delete inf;
		}
	}
	trigger_list.clear();
	return true;
}

const trigger_interface *CTriggerLibrary::EnumLoadedTriggers(bool& first, const char *& name)
{
	if(first)
		m_it = trigger_list.begin();

	first = false;

	const trigger_interface *tri;
	do
	{
		if(m_it==trigger_list.end())
			return NULL;

		tri = m_it->second;
		name = m_it->first.c_str();
		++m_it;
	} while(!tri);
	return tri;
}

