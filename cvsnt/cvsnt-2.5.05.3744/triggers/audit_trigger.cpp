/*
	CVSNT Audit trigger handler
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
#include <stdio.h>
#include <stdlib.h>
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

#define MODULE audit
#define MHMANGLE 

#include <ctype.h>
#include <cvstools.h>
#include "../version.h"

// Database version
#define SCHEMA_VERSION 3

#ifdef _WIN32
HMODULE g_hInst;

BOOL CALLBACK DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hInst = hModule;
	return TRUE;
}

#include "audit_resource.h"

static int win32config(const struct plugin_interface *ui, void *wnd);
#endif

namespace {

struct diffstore_t
{
	diffstore_t() { added=removed=0; }
	unsigned long added;
	unsigned long removed;
	cvs::string diff;
};

CSqlConnection *g_pDb;
CSqlConnectionInformation *g_pCI;
cvs::string g_error;
char g_engine[64];
bool g_AuditLogSessions;
bool g_AuditLogCommits;
bool g_AuditLogDiffs;
bool g_AuditLogTags;
bool g_AuditLogHistory;
unsigned long g_nSessionId;
std::map<cvs::filename,diffstore_t> g_diffStore;

#define NULLSTR(s) ((s)?(s):"")

// Old->New mapping
static const char *const dbList[] =
{
	"mysql",
	"sqlite",
	"postgres",
	"odbc",
	"mssql",
	"db2"
};
#define dbList_Max (sizeof(dbList)/sizeof(dbList[0]))

int initaudit(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
{
	int nType;
	char value[1024];
	int val = 0;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","AuditTrigger",value,sizeof(value)))
		val = atoi(value);
	if(!val)
	{
		CServerIo::trace(3,"Audit trigger not enabled.");
		return -1;
	}

	g_diffStore.clear();

	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseEngine",g_engine,sizeof(g_engine)))
	{
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseType",value,sizeof(value)))
			nType = atoi(value);
		else
		{
			CServerIo::trace(3,"Audit plugin: Database engine not set");
			g_error = "Database engine not set";
			g_pDb = NULL;
			return 0;
		}
		if(nType<0 || nType>=dbList_Max)
		{
			CServerIo::trace(3,"Audit plugin: Database engine type invalid");
			g_error = "Database engine type invalid";
			g_pDb = NULL;
			return 0;
		}
		strcpy(g_engine,dbList[nType]);
	}

	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseName",value,sizeof(value)))
	{
		CServerIo::trace(3,"Audit plugin: Database name not set");
		g_error = "Database name not set";
		g_pDb = NULL;
		return 0;
	}

	g_pDb = CSqlConnection::CreateConnection(g_engine,CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDDatabase));

	if(!g_pDb)
	{
		CServerIo::trace(3,"Audit plugin: Couldn't initialise %s database engine.",g_engine);
		cvs::sprintf(g_error,80,"Couldn't initialise %s database engine.",g_engine);
		return 0;
	}

	g_pCI = g_pDb->GetConnectionInformation();
	if(!g_pCI)
	{
		CServerIo::trace(3,"Audit plugin: Could not get connection information");
		g_error = "Database engine failed to initialise";
		delete g_pDb;
		g_pDb = NULL;
		return 0;
	}

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseHost",value,sizeof(value)))
		g_pCI->setVariable("hostname",value);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseName",value,sizeof(value)))
		g_pCI->setVariable("database",value);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseUsername",value,sizeof(value)))
		g_pCI->setVariable("username",value);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabasePassword",value,sizeof(value)))
		g_pCI->setVariable("password",value);
	// Legacy support - this is not part of the base
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabasePrefix",value,sizeof(value)))
		g_pCI->setVariable("prefix",value);

	const char *v;
	for(size_t n=CSqlConnectionInformation::firstDriverVariable; (v = g_pCI->enumVariableNames(n))!=NULL; n++)
	{
		cvs::string tmp;
		cvs::sprintf(tmp,80,"AuditDatabase_%s_%s",g_engine,v);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp.c_str(),value,sizeof(value)))
			g_pCI->setVariable(v,value);
	}

	g_error = "";
	if(!g_pDb->Open())
	{
		CServerIo::trace(3,"Audit trigger: database connection failed");
		g_error = "Failed to connect to database";
		delete g_pDb;
		g_pDb = NULL;
		return 0;
	}

	g_AuditLogSessions = false;
	g_AuditLogCommits = false;
	g_AuditLogDiffs = false;
	g_AuditLogTags = false;
	g_AuditLogHistory = false;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogSessions",value,sizeof(value)))
		g_AuditLogSessions=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogCommits",value,sizeof(value)))
		g_AuditLogCommits=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogDiffs",value,sizeof(value)))
		g_AuditLogDiffs=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogTags",value,sizeof(value)))
		g_AuditLogTags=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogHistory",value,sizeof(value)))
		g_AuditLogHistory=atoi(value)?true:false;

	cvs::string tbl = g_pDb->parseTableName("SchemaVersion");
	CSqlRecordsetPtr rs = g_pDb->Execute("Select Version From %s",tbl.c_str());
	int nVer;
	if(g_pDb->Error() || rs->Eof())
	{
		nVer = 1;
		CServerIo::trace(3,"Audit SchemaVersion=Error or Eof");
	}
	else
	{
		nVer = (int)*rs[0];
		CServerIo::trace(3,"Audit SchemaVersion=%d",nVer);
	}
	if(nVer!=SCHEMA_VERSION)
	{
		CServerIo::error("audit_trigger error: database is version %d but need version %d.\n", nVer, SCHEMA_VERSION);
		delete g_pDb;
		g_pDb = NULL;
		return 0;
	}
	rs->Close();

	g_nSessionId = 0;
	if(g_AuditLogSessions)
	{
		time_t d = get_date((char*)date,NULL);
		char dt[64];
		tbl = g_pDb->parseTableName("SessionLog");
		strftime(dt,sizeof(dt),"%Y-%m-%d %H:%M:%S",localtime(&d));
		g_pDb->Bind(0,command?command:"");
		g_pDb->Bind(1,hostname?hostname:"");
		g_pDb->Bind(2,username?username:"");
		g_pDb->Bind(3,virtual_repository?virtual_repository:"");
		g_pDb->Bind(4,physical_repository?physical_repository:"");
		g_nSessionId = g_pDb->ExecuteAndReturnIdentity("Insert Into %s (Command, StartTime, Hostname, Username, SessionId, VirtRepos, PhysRepos, Client) Values (?,'%s',?,?,'%s',?,?,'%1.63s')",
			tbl.c_str(),dt,NULLSTR(sessionid),NULLSTR(client_version));
		if(g_pDb->Error())
		{
			CServerIo::error("audit_trigger error (session): %s\n",g_pDb->ErrorString());
			g_error = "Database initialisation failed";
			delete g_pDb;
			g_pDb = NULL;
			return 0;
		}
	}

	return 0;
}

int closeaudit(const struct trigger_interface_t* cb)
{
	if(g_pDb)
		delete g_pDb;
	return 0;
}

int pretagaudit(const struct trigger_interface_t* cb, const char *message, const char *directory, int name_list_count, const char **name_list, const char **version_list, char tag_type, const char *action, const char *tag)
{
	if(g_AuditLogTags)
	{
		for(int n=0; n<name_list_count; n++)
		{
			const char *filename = name_list[n];
			const char *rev = version_list[n];
			cvs::string tbl = g_pDb->parseTableName("TagLog");

			g_pDb->Bind(0,directory?directory:"");
			g_pDb->Bind(1,filename?filename:"");
			g_pDb->Bind(2,message?message:"");
			if(g_AuditLogSessions)
				g_pDb->Execute("Insert Into %s (SessionId, Directory, Filename, Tag, Revision, Message, Action, Type) Values (%lu,?,?,'%s','%s',?,'%s','%c')",
					tbl.c_str(),g_nSessionId,NULLSTR(tag),NULLSTR(rev),NULLSTR(action),tag_type);
			else
				g_pDb->Execute("Insert Into %s (Directory, Filename, Tag, Revision, Message, Action, Type) Values (?,?,'%s','%s',?,'%s','%c')",
					tbl.c_str(),NULLSTR(tag),NULLSTR(rev),NULLSTR(action),tag_type);
			if(g_pDb->Error())
			{
				CServerIo::error("audit_trigger error (pretag): %s\n",g_pDb->ErrorString());
				return -1;
			}
		}
	}
	return 0;
}

int verifymsgaudit(const struct trigger_interface_t* cb, const char *directory, const char *filename)
{
	return 0;
}

int loginfoaudit(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list)
{
	if(g_AuditLogCommits)
	{
		for(int n=0; n<change_list_count; n++)
		{
			const char *diff = g_diffStore[change_list[n].filename].diff.c_str();
			unsigned long added = g_diffStore[change_list[n].filename].added;
			unsigned long removed = g_diffStore[change_list[n].filename].removed;
			cvs::string tbl = g_pDb->parseTableName("CommitLog");

			g_pDb->Bind(0,directory?directory:"");
			g_pDb->Bind(1,message?message:"");
			g_pDb->Bind(2,change_list[n].filename?change_list[n].filename:"");
			g_pDb->Bind(3,diff);
			if(g_AuditLogSessions)
				g_pDb->Execute("Insert Into %s (SessionId, Directory, Message, Type, Filename, Tag, BugId, OldRev, NewRev, Added, Removed, Diff) Values (%lu, ?, ? ,'%c',?,'%s','%s','%s','%s',%lu, %lu, ? )",
					tbl.c_str(),g_nSessionId,change_list[n].type,NULLSTR(change_list[n].tag),NULLSTR(change_list[n].bugid),NULLSTR(change_list[n].rev_old),NULLSTR(change_list[n].rev_new),added,removed);
			else
				g_pDb->Execute("Insert Into %s (Directory, Message, Type, Filename, Tag, BugId, OldRev, NewRev, Added, Removed, Diff) Values (?,? ,'%c',?,'%s','%s','%s','%s',%lu, %lu, ? )",
					tbl.c_str(),change_list[n].type,NULLSTR(change_list[n].tag),NULLSTR(change_list[n].bugid),NULLSTR(change_list[n].rev_old),NULLSTR(change_list[n].rev_new),added,removed);
			if(g_pDb->Error())
			{
				CServerIo::error("audit_trigger error (loginfo): %s\n",g_pDb->ErrorString());
				return -1;
			}
		}
	}
	g_diffStore.clear();
	return 0;
}

int historyaudit(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message)
{
	if(g_AuditLogHistory)
	{
		cvs::string tbl = g_pDb->parseTableName("HistoryLog");
		g_pDb->Bind(0,workdir?workdir:"");
		g_pDb->Bind(1,name?name:"");
		g_pDb->Bind(2,message?message:"");
		if(g_AuditLogSessions)
			g_pDb->Execute("Insert Into %s (SessionId, Type, Workdir, Revs, Name, BugId, Message) Values (%lu, '%c',?,'%s',?,'%s', ? )",
				tbl.c_str(),g_nSessionId,type,NULLSTR(revs),NULLSTR(bugid));
		else
			g_pDb->Execute("Insert Into %s (Type, Workdir, Revs, Name, BugId, Message) Values ('%c',?,'%s',?,'%s', ? )",
				tbl.c_str(),type,NULLSTR(revs),NULLSTR(bugid));
		if(g_pDb->Error())
		{
			CServerIo::error("audit_trigger error (history): %s\n",g_pDb->ErrorString());
			return -1;
		}
	}
	return 0;
}

int notifyaudit(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file)
{
	return 0;
}

int precommitaudit(const struct trigger_interface_t* cb, int name_list_count, const char **name_list, const char *message, const char *directory)
{
	return 0;
}

int postcommitaudit(const struct trigger_interface_t* cb, const char *directory)
{
	return 0;
}

int precommandaudit(const struct trigger_interface_t* cb, int argc, const char **argv)
{
	CServerIo::trace(3,"Audit precommand, g_pDb=%p",g_pDb);
	if(!g_pDb)
	{
		CServerIo::error("Audit trigger initialiasation failed: %s\n",g_error.c_str());
		return -1;
	}
	return 0;
}

int postcommandaudit(const struct trigger_interface_t* cb, const char *directory, int return_code)
{
	if(!g_pDb)
	{
		CServerIo::error("Audit trigger initialiasation failed: %s\n",g_error.c_str());
		return -1;
	}
	if(g_AuditLogSessions)
	{
		cvs::string tbl = g_pDb->parseTableName("SessionLog");
		const char *audit_session_time;
		time_t audit_session_time_t;
		char dt[64];

		time (&audit_session_time_t);
		audit_session_time = strdup(asctime (gmtime(&audit_session_time_t)));
		((char*)audit_session_time)[24] = '\0';
		time_t d = get_date((char*)audit_session_time,NULL);

		strftime(dt,sizeof(dt),"%Y-%m-%d %H:%M:%S",localtime(&d));
		g_pDb->Execute("Update %s set EndTime='%s', FinalReturnCode=%lu where Id=%lu",
			tbl.c_str(),dt,return_code,g_nSessionId);
		if(g_pDb->Error())
		{
			CServerIo::error("audit_trigger error (session end): %s\n",g_pDb->ErrorString());
			return -1;
		}
	}
	return 0;
}

int premoduleaudit(const struct trigger_interface_t* cb, const char *module)
{
	return 0;
}

int postmoduleaudit(const struct trigger_interface_t* cb, const char *module)
{
	return 0;
}

int get_templateaudit(const struct trigger_interface_t *cb, const char *directory, const char **template_ptr)
{
	return 0;
}

int parse_keywordaudit(const struct trigger_interface_t *cb, const char *keyword,const char *directory,const char *file,const char *branch,const char *author,const char *printable_date,const char *rcs_date,const char *locker,const char *state,const char *version,const char *name,const char *bugid, const char *commitid, const property_info *props, size_t numprops, const char **value)
{
	return 0;
}

int prercsdiffaudit(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	g_diffStore[file].added=added;
	g_diffStore[file].removed=removed;

	// return 0 - no diff (rcsdiff not called)
	// return 1 - Unified diff
	if(g_AuditLogDiffs && (added || removed) && (!options || !strchr(options,'b')))
		return 1;
	return 0;
}

int rcsdiffaudit(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *diff, size_t difflen, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	g_diffStore[file].added=added;
	g_diffStore[file].removed=removed;
	g_diffStore[file].diff=diff;
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
		"Repository auditing extension",CVSNT_PRODUCTVERSION_STRING,"AuditTrigger",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		win32config
	#else
		NULL
	#endif
	},
	initaudit,
	closeaudit,
	pretagaudit,
	verifymsgaudit,
	loginfoaudit,
	historyaudit,
	notifyaudit,
	precommitaudit,
	postcommitaudit,
	precommandaudit,
	postcommandaudit,
	premoduleaudit,
	postmoduleaudit,
	get_templateaudit,
	parse_keywordaudit,
	prercsdiffaudit,
	rcsdiffaudit
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
static bool SelectDb(HWND hWnd, const char *db)
{
	char value[1024];

	if(g_pDb)
		delete g_pDb;

	strncpy(g_engine,db,sizeof(g_engine));
	g_pDb = CSqlConnection::CreateConnection(g_engine,CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDDatabase));

	if(!g_pDb)
	{
		cvs::sprintf(g_error,80,"Couldn't initialise %s database engine.",g_engine);
//		MessageBox(hWnd, cvs::wide(g_error.c_str()),_T("Audit Trigger"), MB_ICONSTOP|MB_OK);
		return false;
	}

	g_pCI = g_pDb->GetConnectionInformation();
	if(!g_pCI)
	{
		CServerIo::trace(3,"Audit plugin: Could not get connection information");
		g_error = "Database engine failed to initialise";
//		MessageBox(hWnd, cvs::wide(g_error.c_str()),_T("Audit Trigger"), MB_ICONSTOP|MB_OK);
		delete g_pDb;
		g_pDb = NULL;
		return false;
	}

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseHost",value,sizeof(value)))
		g_pCI->setVariable("hostname",value);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseName",value,sizeof(value)))
		g_pCI->setVariable("database",value);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseUsername",value,sizeof(value)))
		g_pCI->setVariable("username",value);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabasePassword",value,sizeof(value)))
		g_pCI->setVariable("password",value);
	// Legacy support - this is not part of the base
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabasePrefix",value,sizeof(value)))
		g_pCI->setVariable("prefix",value);

	const char *v;
	for(size_t n=CSqlConnectionInformation::firstDriverVariable; (v = g_pCI->enumVariableNames(n))!=NULL; n++)
	{
		cvs::string tmp;
		cvs::sprintf(tmp,80,"AuditDatabase_%s_%s",g_engine,v);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp.c_str(),value,sizeof(value)))
			g_pCI->setVariable(v,value);
	}
	return true;
}

BOOL sql_process_file(HWND hWnd, cvs::string &fn,HCURSOR hCursor,LPCWSTR title)
{
					CFileAccess acc;
					cvs::string line,comp_line;
					if(!acc.open(fn.c_str(),"r"))
					{
						g_pDb->Close();
						MessageBox(hWnd,L"Script could not be opened",title,MB_ICONSTOP);
						return TRUE;
					}

					g_pDb->BeginTrans();

					comp_line="";
					while(acc.getline(line))
					{
						if(line.size()<2 || !strncmp(line.c_str(),"--",2))
							continue;
						comp_line+=" "+line;
						if(line[line.size()-1]!=';')
							continue;

						// a more generic search/replace of all vars
						CServerIo::trace(3,"Audit search and replace varaiables in %s",fn.c_str());
						comp_line.resize(comp_line.size()-1); // Some DBs don't like the semicolon...
						int state=0,start;
						for(size_t n=0; state>=0 && n<comp_line.size(); n++)
						{
							char c = comp_line[n];
							switch(state)
							{
							case 0:
								if(c=='%') {state=1; start=n; }
								break;
							case 1:
								if(c=='%')
								{ 
									// possible keyword %FOO%
									const char *varname = comp_line.substr(start+1,n-(start+1)).c_str();
									const char *var = g_pCI->getVariable(varname);
									if(var)
									{
										CServerIo::trace(3,"Audit replacing varaiable %%%s%% value is \"%s\"",PATCH_NULL(varname),PATCH_NULL(var));
										comp_line.replace(start,(n-start)+1,var);
										n+=(strlen(var)-((n-start)+1));
									}
									else
										CServerIo::trace(3,"Audit not replacing varaiable %%%s%% value is \"%s\"",PATCH_NULL(varname),PATCH_NULL(var));
									state=0;
									break;
								}
								if(!isupper(c)) state=0; 
								break;
							}
						}

						// just a poor hack to fix the above if it didnt work...
						size_t posprefix;
						const char *varprefix = g_pCI->getVariable("PREFIX");
						while((posprefix=comp_line.find("%PREFIX%"))!=cvs::string::npos)
						{
							CServerIo::trace(3,"Audit requires hack replace %%%s%% value is \"%s\"","PREFIX",PATCH_NULL(varprefix));
							comp_line.replace(posprefix,8,(varprefix)?varprefix:"");
						}

						CServerIo::trace(3,"Audit execute \"%s\"",comp_line.c_str());
						g_pDb->Execute("%s",comp_line.c_str());
						if(g_pDb->Error())
						{
							MessageBox(hWnd,cvs::wide(g_pDb->ErrorString()),title,MB_ICONSTOP|MB_OK);
							g_pDb->RollbackTrans();
							g_pDb->Close();
							return TRUE;
						}
						comp_line="";
					}
					g_pDb->CommitTrans();
  return FALSE;
}

BOOL CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char value[1024];
	int nSel;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			value[0]='\0';
			if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseEngine",value,sizeof(value)))
			{
				if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditDatabaseType",nSel))
				{
					if(nSel>=0 && nSel<dbList_Max)
						strcpy(value,dbList[nSel]);
				}
			}

			CSqlConnection::connectionList_t list;
			CSqlConnection::GetConnectionList(list, CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDDatabase));

			
			for(size_t n=0; n<list.size(); n++)
				SendDlgItemMessage(hWnd,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)(const wchar_t *)cvs::wide(list[n].second.c_str()));

			nSel = SendDlgItemMessage(hWnd,IDC_COMBO1,CB_FINDSTRINGEXACT,0,(LPARAM)(const wchar_t*)cvs::wide(value));
			SendDlgItemMessage(hWnd,IDC_COMBO1,CB_SETCURSEL,nSel,0);

			nSel = 0;
			CGlobalSettings::GetGlobalValue("cvsnt","Plugins","AuditTrigger",nSel);
			if(!nSel)
			{
				EnableWindow(GetDlgItem(hWnd,IDC_COMBO1),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_TEXT1),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_TEXT2),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_BUTTON1),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_BUTTON2),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_BUTTON3),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_BUTTON4),FALSE);
				SetDlgItemText(hWnd,IDC_TEXT1,_T(""));
				SetDlgItemText(hWnd,IDC_TEXT2,_T(""));
			}
			else
			{
				SendDlgItemMessage(hWnd,IDC_CHECK6,BM_SETCHECK,1,NULL);
				if(!SelectDb(hWnd,value))
				{
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON1),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON2),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON3),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON4),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_TEXT1),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_TEXT2),FALSE);
					SetDlgItemText(hWnd,IDC_TEXT1,_T(""));
					SetDlgItemText(hWnd,IDC_TEXT2,_T(""));
				}
				else
				{
					SetDlgItemText(hWnd,IDC_TEXT1,cvs::wide(g_pCI->getVariable("database")));
					SetDlgItemText(hWnd,IDC_TEXT2,cvs::wide(g_pCI->getVariable("hostname")));
				}
			}


			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogSessions",value,sizeof(value)))
				SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,(WPARAM)atoi(value)?1:0,NULL);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogCommits",value,sizeof(value)))
				SendDlgItemMessage(hWnd,IDC_CHECK2,BM_SETCHECK,(WPARAM)atoi(value)?1:0,NULL);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogDiffs",value,sizeof(value)))
				SendDlgItemMessage(hWnd,IDC_CHECK3,BM_SETCHECK,(WPARAM)atoi(value)?1:0,NULL);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogTags",value,sizeof(value)))
				SendDlgItemMessage(hWnd,IDC_CHECK4,BM_SETCHECK,(WPARAM)atoi(value)?1:0,NULL);
			if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","AuditLogHistory",value,sizeof(value)))
				SendDlgItemMessage(hWnd,IDC_CHECK5,BM_SETCHECK,(WPARAM)atoi(value)?1:0,NULL);
		}
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_CHECK6:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK6,BM_GETCHECK,NULL,NULL);
			EnableWindow(GetDlgItem(hWnd,IDC_COMBO1),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_BUTTON1),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_BUTTON2),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_BUTTON3),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_BUTTON4),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_TEXT1),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_TEXT2),nSel?TRUE:FALSE);
			if(nSel)
			{
				TCHAR szVal[256];
				GetDlgItemText(hWnd,IDC_COMBO1,szVal,sizeof(szVal));
				if(SelectDb(hWnd,cvs::narrow(szVal)))
				{
					SetDlgItemText(hWnd,IDC_TEXT1,cvs::wide(g_pCI->getVariable("database")));
					SetDlgItemText(hWnd,IDC_TEXT2,cvs::wide(g_pCI->getVariable("hostname")));
				}
				else
				{
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON1),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON2),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON3),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_BUTTON4),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_TEXT1),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_TEXT2),FALSE);
					SetDlgItemText(hWnd,IDC_TEXT1,_T(""));
					SetDlgItemText(hWnd,IDC_TEXT2,_T(""));
				}
			}
			return TRUE;
		case IDC_COMBO1:
			switch(HIWORD(wParam))
			{
			case CBN_SELENDOK:
				{
					TCHAR szVal[256];
					GetDlgItemText(hWnd,IDC_COMBO1,szVal,sizeof(szVal));
					if(SelectDb(hWnd,cvs::narrow(szVal)))
					{
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON1),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON2),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON3),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON4),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_TEXT1),TRUE);
						EnableWindow(GetDlgItem(hWnd,IDC_TEXT2),TRUE);
						SetDlgItemText(hWnd,IDC_TEXT1,cvs::wide(g_pCI->getVariable("database")));
						SetDlgItemText(hWnd,IDC_TEXT2,cvs::wide(g_pCI->getVariable("hostname")));
					}
					else
					{
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON1),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON2),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON3),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_BUTTON4),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_TEXT1),FALSE);
						EnableWindow(GetDlgItem(hWnd,IDC_TEXT2),FALSE);
						SetDlgItemText(hWnd,IDC_TEXT1,_T(""));
						SetDlgItemText(hWnd,IDC_TEXT2,_T(""));
					}
				}
				break;
			}
			break;
		case IDC_BUTTON1: // Test connection
			if(g_pDb)
			{
				// get generic connection info from current engine
				HCURSOR hCursor = SetCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_WAIT)));
				SetCursor(hCursor);
				if(!g_pDb)
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Connection failed:\n\nEngine not initialised");
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Connection test",MB_ICONSTOP);
				}
				if(!g_pDb->Open())
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Connection failed:\n\n%s",g_pDb->ErrorString());
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Connection test",MB_ICONSTOP);
				}
				else
				{
					g_pDb->Close();
					MessageBox(hWnd,L"Connection OK",L"Connection test",MB_ICONINFORMATION);
				}
			}
			break;
		case IDC_BUTTON2: // Create tables
			if(g_pDb)
			{
				cvs::string fn;

				fn = CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDLib);
				fn+="/sql/create_tables_";

				fn+=g_engine;
				fn+=".sql";
				if(!CFileAccess::exists(fn.c_str()))
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Script %s not found.",fn.c_str());
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Create tables",MB_ICONSTOP);
					return TRUE;
				}

				// get generic connection info from current engine

				HCURSOR hCursor = SetCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_WAIT)));
				SetCursor(hCursor);
				if(!g_pDb)
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Connection failed:\n\nEngine not initialised");
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Connection test",MB_ICONSTOP);
				}
				if(!g_pDb->Open())
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Connection failed:\n\n%s",g_pDb->ErrorString());
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Connection test",MB_ICONSTOP);
				}
				else
				{
					if (sql_process_file(hWnd,fn,hCursor,L"Create tables")==TRUE)
						return TRUE;
					g_pDb->Close();
					MessageBox(hWnd,L"Tables Created Successfully",L"Create tables",MB_ICONINFORMATION);
				}
			}
			break;
		case IDC_BUTTON3: // Edit connection
			if(g_pDb)
			{
				if(g_pCI->connectionDialog((const void *)hWnd))
				{
					SetDlgItemText(hWnd,IDC_TEXT1,cvs::wide(g_pCI->getVariable("database")));
					SetDlgItemText(hWnd,IDC_TEXT2,cvs::wide(g_pCI->getVariable("hostname")));
				}
			}
			break;
		case IDC_BUTTON4: // Upgrade database
			if(g_pDb)
			{
				cvs::string fn;

				// get generic connection info from current engine

				HCURSOR hCursor = SetCursor(LoadCursor(NULL,MAKEINTRESOURCE(IDC_WAIT)));
				SetCursor(hCursor);
				if(!g_pDb)
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Connection failed:\n\nEngine not initialised");
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Upgrade",MB_ICONSTOP);
					return TRUE;
				}
				if(!g_pDb->Open())
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Connection failed:\n\n%s",g_pDb->ErrorString());
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Ugrade",MB_ICONSTOP);
					return TRUE;
				}

				cvs::string tbl = g_pDb->parseTableName("SchemaVersion");
				CSqlRecordsetPtr rs = g_pDb->Execute("Select Version From %s",tbl.c_str());
				int nVer;
				if(g_pDb->Error() || rs->Eof())
					nVer = 1;
				else
					nVer = (int)*rs[0];
				if(nVer==SCHEMA_VERSION)
				{
					MessageBox(hWnd,_T("Audit database is up to date"),_T("Upgrade"),MB_ICONINFORMATION);
					g_pDb->Close();
					return TRUE;
				}

				fn = CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDLib);
				cvs::sprintf(tbl,80,"/sql/upgrade_%d_%s.sql",nVer,g_engine);
				if(!CFileAccess::exists(fn.c_str()))
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"Script %s not found.",fn.c_str());
					MessageBox(hWnd,cvs::wide(tmp.c_str()),L"Upgrade",MB_ICONSTOP);
					g_pDb->Close();
					return TRUE;
				}

				if (sql_process_file(hWnd,fn,hCursor,L"Upgrade")==TRUE)
					return TRUE;

				g_pDb->Close();
				MessageBox(hWnd,L"Upgraded Successfully",L"Upgrade",MB_ICONINFORMATION);
			}
			break;
		case IDOK:
			if(g_pDb)
			{
				nSel=SendDlgItemMessage(hWnd,IDC_CHECK6,BM_GETCHECK,NULL,NULL);
				CGlobalSettings::SetGlobalValue("cvsnt","Plugins","AuditTrigger",nSel);
				CGlobalSettings::DeleteGlobalValue("cvsnt","PServer","AuditDatabaseType");
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditDatabaseEngine",g_engine);

				// Generic connection info
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditDatabaseName",g_pCI->getVariable("database"));
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditDatabaseUsername",g_pCI->getVariable("username"));
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditDatabasePassword",g_pCI->getVariable("password"));
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditDatabaseHost",g_pCI->getVariable("hostname"));
				CGlobalSettings::DeleteGlobalValue("cvsnt","PServer","AuditDatabasePrefix");

				const char *v;
				for(size_t n=CSqlConnectionInformation::firstDriverVariable; (v = g_pCI->enumVariableNames(n))!=NULL; n++)
				{
					cvs::string tmp;
					cvs::sprintf(tmp,80,"AuditDatabase_%s_%s",g_engine,v);
					CGlobalSettings::SetGlobalValue("cvsnt","PServer",tmp.c_str(),g_pCI->getVariable(v));
				}

				nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,NULL,NULL);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditLogSessions",nSel);
				nSel=SendDlgItemMessage(hWnd,IDC_CHECK2,BM_GETCHECK,NULL,NULL);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditLogCommits",nSel);
				nSel=SendDlgItemMessage(hWnd,IDC_CHECK3,BM_GETCHECK,NULL,NULL);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditLogDiffs",nSel);
				nSel=SendDlgItemMessage(hWnd,IDC_CHECK4,BM_GETCHECK,NULL,NULL);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditLogTags",nSel);
				nSel=SendDlgItemMessage(hWnd,IDC_CHECK5,BM_GETCHECK,NULL,NULL);
				CGlobalSettings::SetGlobalValue("cvsnt","PServer","AuditLogHistory",nSel);
			}
		// Drop through
		case IDCANCEL:
			EndDialog(hWnd,LOWORD(wParam));
			if(g_pDb)
				delete g_pDb;
			g_pDb = NULL;
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

