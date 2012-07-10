/*
	CVSNT Checkout trigger handler
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

#ifdef _WIN32
#pragma warning(disable:4503) // Decorated name length warning
#endif

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <process.h>
#include <winsock2.h>
#include <mapi.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define MODULE checkout

#include <ctype.h>
#include <cvstools.h>
#include <map>
#include "../version.h"

#define CVSROOT_SHADOW		"CVSROOT/shadow"

#ifdef _WIN32
HMODULE g_hInst;

BOOL CALLBACK DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hInst = hModule;
	return TRUE;
}

#include "checkout_resource.h"

int win32config(const struct plugin_interface *ui, void *wnd);
#endif

namespace {

static bool g_verbose;
cvs::filename g_repos;
cvs::string g_command;
std::map<cvs::filename, int> module_list;
std::map<cvs::string, int> tag_list;

int init(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
{
	char value[256];
	int val = 0;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","CheckoutTrigger",value,sizeof(value)))
		val = atoi(value);
	if(!val)
	{
		CServerIo::trace(3,"Checkout trigger not enabled.");
		return -1;
	}

	g_verbose = false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CheckoutVerbose",value,sizeof(value)))
		g_verbose = atoi(value)?true:false;

	g_repos = physical_repository;
	g_command = command;

	return 0;
}

int close(const struct trigger_interface_t* cb)
{
	return 0;
}

int pretag(const struct trigger_interface_t* cb, const char *message, const char *directory, int name_list_count, const char **name_list, const char **version_list, char tag_type, const char *action, const char *tag)
{
	module_list[directory]++;
	if(tag && tag[0])
		tag_list[tag]++;
	else
		tag_list["HEAD"]++;

	return 0;
}

int verifymsg(const struct trigger_interface_t* cb, const char *directory, const char *filename)
{
	return 0;
}

int loginfo(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list)
{
	module_list[directory]++;

	for(int n=0; n<change_list_count; n++)
	{
		if(change_list[n].tag)
			tag_list[change_list[n].tag]++;
		else
			tag_list["HEAD"]++;
	}
	return 0;
}

int history(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message)
{
	return 0;
}

int notify(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file)
{
	return 0;
}

int precommit(const struct trigger_interface_t* cb, int name_list_count, const char **name_list, const char *message, const char *directory)
{
	return 0;
}

int postcommit(const struct trigger_interface_t* cb, const char *directory)
{
	return 0;
}

int precommand(const struct trigger_interface_t* cb, int argc, const char **argv)
{
	return 0;
}

static int outputProc(const char *str, size_t len, void * /*param*/)
{
	if(g_verbose)
		return CServerIo::output(len,str);
	return 0;
}

static int errorProc(const char *str, size_t len, void * /*param*/)
{
	if(g_verbose)
		return CServerIo::error(len,str);
	return 0;
}

int postcommand(const struct trigger_interface_t* cb, const char *directory, int return_code)
{
	cvs::filename fn;
	CFileAccess acc;
	cvs::string line;

	if(g_command!="tag" && g_command!="rtag" && g_command!="commit")
		return 0;

	cvs::sprintf(fn,80,"%s/%s",g_repos.c_str(),CVSROOT_SHADOW);
	if(!acc.open(fn.c_str(),"r"))
	{
		CServerIo::trace(3,"Could not open "CVSROOT_SHADOW);
		return 0;
	}

	int linenum;
	for(linenum=1; acc.getline(line); linenum++)
	{
		const char *p=line.c_str();
		while(isspace((unsigned char)*p))
			p++;
		if(*p=='#')
			continue;

		CTokenLine tok(p);

		if(tok.size()!=3)
		{
			CServerIo::error("Malformed line %d in "CVSROOT_SHADOW" - Need Module Tag Directory",linenum);
			continue;
		}

		bool found = false;
		cvs::string match;
		for(std::map<cvs::filename,int>::const_iterator i = module_list.begin(); i!=module_list.end(); ++i)
		{
			CServerIo::trace(3,"Regexp match: %s - %s",tok[0],i->first.c_str());
			cvs::wildcard_filename mod(i->first.c_str());
			if(mod.matches_regexp(tok[0])) // Wildcard match
			{
				CServerIo::trace(3,"Match found!");
				found=true;
				match = i->first.c_str();
				break;
			}
		}
		if(!found)
			continue;

		found=false;
		for(std::map<cvs::string,int>::const_iterator i = tag_list.begin(); i!=tag_list.end(); ++i)
		{
			if(!strcmp(i->first.c_str(),tok[1]))
			{
				found=true;
				break;
			}
		}
		if(!found)
			continue;

		CRunFile rf;

		rf.setOutput(outputProc,NULL);
		rf.setError(errorProc,NULL);

		rf.addArg(CGlobalSettings::GetCvsCommand());
		rf.addArg("-d");
		rf.addArg(g_repos.c_str());
		rf.addArg("co");
		rf.addArg("-d");
		rf.addArg(tok[2]);
		rf.addArg("-r");
		rf.addArg(tok[1]);
		rf.addArg(match.c_str());
		if(!rf.run(NULL))
		{
			CServerIo::error("Unable to run cvs checkout");
			return 0;
		}
		int ret;
		rf.wait(ret);
	}

	return 0;
}

int premodule(const struct trigger_interface_t* cb, const char *module)
{
	module_list[module]++;
	return 0;
}

int postmodule(const struct trigger_interface_t* cb, const char *module)
{
	module_list[module]++;
	return 0;
}

int get_template(const struct trigger_interface_t *cb, const char *directory, const char **template_ptr)
{
	return 0;
}

int parse_keyword(const struct trigger_interface_t *cb, const char *keyword,const char *directory,const char *file,const char *branch,const char *author,const char *printable_date,const char *rcs_date,const char *locker,const char *state,const char *version,const char *name,const char *bugid, const char *commitid, const property_info *props, size_t numprops, const char **value)
{
	return 0;
}

int prercsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	return 0;
}

int rcsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *diff, size_t difflen, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	return 0;
}

} // anonymous namespace

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static trigger_interface callbacks =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"Automatic checkout extension",CVSNT_PRODUCTVERSION_STRING,"CheckoutTrigger",
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
	parse_keyword,
	prercsdiff,
	rcsdiff
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
		if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","CheckoutTrigger",value,sizeof(value)))
			nSel = atoi(value);
		if(!nSel)
		{
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);
		}
		else
			SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,1,NULL);
		nSel = 0;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CheckoutVerbose",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK2,BM_SETCHECK,nSel?1:0,NULL);
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_CHECK1:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,NULL,NULL);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),nSel?TRUE:FALSE);
			return TRUE;
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,NULL,NULL);
            CGlobalSettings::SetGlobalValue("cvsnt","Plugins","CheckoutTrigger",nSel);
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK2,BM_GETCHECK,NULL,NULL);
            CGlobalSettings::SetGlobalValue("cvsnt","PServer","CheckoutVerbose",nSel);
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
	int ret = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, ConfigDlgProc);
	return ret==IDOK?0:-1;
}
#endif

