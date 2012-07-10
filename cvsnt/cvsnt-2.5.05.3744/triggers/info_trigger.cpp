/*
	CVSNT Default trigger handler
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
#ifdef _WIN32
#include <process.h>
#include <winsock2.h>
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
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#define MODULE info

#include <ctype.h>
#include <cvstools.h>
#include <map>
#include "../version.h"

#define CVSROOT_COMMITINFO	"CVSROOT/commitinfo"
#define CVSROOT_HISTORYINFO "CVSROOT/historyinfo"
#define	CVSROOT_LOGINFO		"CVSROOT/loginfo"
#define CVSROOT_NOTIFY		"CVSROOT/notify"
#define CVSROOT_POSTCOMMAND "CVSROOT/postcommand"
#define CVSROOT_POSTCOMMIT	"CVSROOT/postcommit"
#define CVSROOT_POSTMODULE	"CVSROOT/postmodule"
#define CVSROOT_PRECOMMAND	"CVSROOT/precommand"
#define CVSROOT_PREMODULE	"CVSROOT/premodule"
#define	CVSROOT_RCSINFO		"CVSROOT/rcsinfo"
#define CVSROOT_TAGINFO     "CVSROOT/taginfo"
#define CVSROOT_VERIFYMSG   "CVSROOT/verifymsg"
#define CVSROOT_KEYWORDS	"CVSROOT/keywords"

namespace {

struct options
{
	char command;
	const char **data;
	int (*fn)(int num, const char **reslt, void *param);
	void *param;
};

int parse_info(const char *file, const char *default_cmd_parms, const char *default_io_parms, const char *directory, options *generic_options, options *spec_options);
int parse_rcsinfo(const char *file, const char *directory, cvs::string& rcsline);
int parse_keywords(const char *file, const char *directory, const char *keyword, options *generic_options, options *spec_options, cvs::string& keywordline, bool have_locker);
int parse_info_line(std::vector<cvs::string>&cache, const char *line, options *generic_options, options *spec_options, const char *source_file, size_t& source_line);
int __parse_info_line(const char *line, options *generic_options, options *spec_options, const char *source_file, size_t& source_line, const char **here_text, cvs::string* io, cvs::string& out, bool no_escape);
const char *auto_escape(const char *_str, char quote);

typedef std::map<const char *,const char *> uservar_t;

cvs::string g_io;
size_t g_ioPos;

static const property_info *prop_info = NULL;
static size_t prop_count = 0;

struct generic_information
{
	const char *command; // %c - command being executed
	const char *date; // %d - date/time of action
	const char *hostname; // %h - remote host name
	const char *username; // %u - cvs username
	const char *virtual_repository; // %r - repository name (virtual)
	const char *physical_repository; // %R - repository name (physical)
	const char *sessionid; // %S - session id
	const char *editor; // %e - editor
	const char *local_hostname; // %H - local host name
	const char *local_directory; // %P - local directory
	const char *client_version; // %i - client version
	const char *character_set; // %x - character set
	// Also %n - empty string
	// %% - %
	uservar_t uservar; // User variables
	const char *pid;
};

generic_information gen_info;

options generic_options[] =
{
	{ 'c', &gen_info.command },
	{ 'd', &gen_info.date },
	{ 'h', &gen_info.hostname },
	{ 'u', &gen_info.username },
	{ 'r', &gen_info.virtual_repository },
	{ 'R', &gen_info.physical_repository },
	{ 'S', &gen_info.sessionid },
	{ 'e', &gen_info.editor },
	{ 'H', &gen_info.local_hostname },
	{ 'P', &gen_info.local_directory },
	{ 'i', &gen_info.client_version },
	{ 'x', &gen_info.character_set },
	{ 0 }
};

struct _env_info
{
	const char *env;
	const char **val;
} env_info[] =
{
	{ "CVSEDITOR", &gen_info.editor },
	{ "VISUAL", &gen_info.editor },
	{ "EDITOR", &gen_info.editor },
	{ "USER", &gen_info.username },
	{ "CVSPID", &gen_info.pid },
	{ "SESSIONID", &gen_info.sessionid },
	{ "COMMITID", &gen_info.sessionid },
	{ "CVSROOT", &gen_info.virtual_repository },
	{ "REAL_CVSROOT", &gen_info.physical_repository },
	{ "VIRTUAL_CVSROOT", &gen_info.virtual_repository },
	{ 0 }
};

int generic_get_char(int num, const char **reslt, void *param)
{
	static char ch[2]={ *(char*)param, 0 };
	if(num==-1) return 0;
	*reslt=ch;
	return 0;
}

//commitinfo
//
//  %r/%p %<s
//
struct precommit_information
{
	int name_list_count;
	const char **name_list; // %s - file name
	const char *message; // %m - message supplied on command line
	const char *directory; // %p - directory name
};

precommit_information precmt_info;

int prec_enum_name_list(int num, const char **reslt, void *param)
{
	precommit_information *info = (precommit_information*)param;
	if(num==-1) return 0;
	if(num>=info->name_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->name_list[num];
	return num+1<info->name_list_count;
}

options precommit_options[] =
{
	{ 'm', &precmt_info.message },
	{ 'p', &precmt_info.directory },
	{ 's', NULL, prec_enum_name_list, &precmt_info },
	{ 0 }
};

//taginfo
//
//  %t %o %r/%p  %<{%s %v}
//
struct pretag_information
{
	const char *message; // %m - message supplied on command line
	const char *directory; // %p - directory name
	int name_list_count;
	const char **name_list; // %s - file name
	const char **version_list; // %v - new version
	char tag_type; // %b - tag type (T,N,?)
	const char *action; // %o - operation (add,mov,del)
	const char *tag; // %t - tag name
};

pretag_information pret_info;

int pret_enum_name_list(int num, const char **reslt, void *param)
{
	pretag_information *info = (pretag_information*)param;
	if(num==-1) return 0;
	if(num>=info->name_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->name_list[num];
	return num+1<info->name_list_count;
}

int pret_enum_version_list(int num, const char **reslt, void *param)
{
	pretag_information *info = (pretag_information*)param;
	if(num==-1) return 0;
	if(num>=info->name_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->version_list[num];
	return num+1<info->name_list_count;
}

options pretag_options[] =
{
	{ 'm', &pret_info.message },
	{ 'p', &pret_info.directory },
	{ 's', NULL, pret_enum_name_list, &pret_info },
	{ 'v', NULL, pret_enum_version_list, &pret_info },
	{ 'b', NULL, generic_get_char, &pret_info.tag_type },
	{ 'o', &pret_info.action },
	{ 't', &pret_info.tag },
	{ 0 }
};

//verifymsg
//
// %l
//
struct verifymsg_information
{
	const char *directory; // %p - directory name
	const char *filename; // %l - path to file containing log message
};

verifymsg_information verif_info;

options verifymsg_options[] =
{
	{ 'p', &verif_info.directory },
	{ 'l', &verif_info.filename },
	{ 0 }
};

//loginfo
//
// %<<stuff (predefined) stuff
//
struct loginfo_information
{
	const char *message; // %m - message supplied on command line
	const char *status; // %T - status string
	const char *directory; // %p - directory name
	int change_list_count;
	bool rep_flag;
	change_info *change_list; // %s, %V, %v, %b, %t, %y
};

loginfo_information login_info;

int login_enum_filename(int num, const char **reslt, void *param)
{
	loginfo_information *info = (loginfo_information*)param;
	if(num==-1)
	{
		info->rep_flag = false;
		return 0;
	}
	if(!info->rep_flag)
	{
		*reslt=info->directory;
		info->rep_flag=true;
		return 2; /* Try again */
	}
	if(num>=info->change_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->change_list[num].filename;
	if(info->change_list[num].type=='T')
		return 5;
	else
		return num+1<info->change_list_count;
}

int login_enum_oldrev(int num, const char **reslt, void *param)
{
	loginfo_information *info = (loginfo_information*)param;
	if(num==-1) return 0;
	if(num>=info->change_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	if(info->change_list[num].type=='T')
		return 4;
	*reslt=info->change_list[num].rev_old;
	if(!*reslt)
		*reslt="NONE";
	return num+1<info->change_list_count;
}

int login_enum_newrev(int num, const char **reslt, void *param)
{
	loginfo_information *info = (loginfo_information*)param;
	if(num==-1) return 0;
	if(num>=info->change_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	if(info->change_list[num].type=='T')
		return 4;
	*reslt=info->change_list[num].rev_new;
	if(!*reslt)
		*reslt="NONE";
	return num+1<info->change_list_count;
}

int login_enum_bugid(int num, const char **reslt, void *param)
{
	loginfo_information *info = (loginfo_information*)param;
	if(num==-1) return 0;
	if(num>=info->change_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->change_list[num].bugid;
	return num+1<info->change_list_count;
}

int login_enum_tag(int num, const char **reslt, void *param)
{
	loginfo_information *info = (loginfo_information*)param;
	if(num==-1) return 0;
	if(num>=info->change_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->change_list[num].tag;
	return num+1<info->change_list_count;
}

int login_enum_type(int num, const char **reslt, void *param)
{
	static char ch[2]={0};
	loginfo_information *info = (loginfo_information*)param;
	if(num==-1) return 0;
	if(num>=info->change_list_count)
	{
		*reslt=NULL;
		return 0;
	}
	ch[0]=info->change_list[num].type;
	*reslt=ch;
	return num+1<info->change_list_count;
}

options loginfo_options[] =
{
	{ 'm', &login_info.message },
	{ 'T', &login_info.status },
	{ 'p', &login_info.directory },
	{ 's', NULL, login_enum_filename, &login_info },
	{ 'V', NULL, login_enum_oldrev, &login_info },
	{ 'v', NULL, login_enum_newrev, &login_info },
	{ 'b', NULL, login_enum_bugid, &login_info },
	{ 't', NULL, login_enum_tag, &login_info },
	{ 'y', NULL, login_enum_type, &login_info },
	{ 0 }
};

// history
// %t|%d|%u|%w|%s|%v
//
struct history_information
{
	char type; // %t - history type
	const char *workdir; // %w - working dir
	const char *revs;  // %v - revisions
	const char *name; // %s - name
	const char *bugid; // %b - bugid
	const char *message; // %m - message
};

history_information hist_info;

options history_options[] =
{
	{ 't', NULL, generic_get_char, &hist_info.type },
	{ 'w', &hist_info.workdir },
	{ 'v', &hist_info.revs },
	{ 's', &hist_info.name },
	{ 'b', &hist_info.bugid },
	{ 'm', &hist_info.message },
	{ 0 }
};

//precommand
//
//  %r %c %<a
//
struct precommand_information
{
	int argc;
	const char **argv; // %a - command arguments
};

precommand_information prec_info;

int prec_enum_arguments(int num, const char **reslt, void *param)
{
	precommand_information *info = (precommand_information*)param;
	if(num==-1) return 0;
	if(num>=info->argc)
	{
		*reslt=NULL;
		return 0;
	}
	*reslt=info->argv[num];
	return num+1<info->argc;
}

options precommand_options[] =
{
	{ 'a', NULL, prec_enum_arguments, &prec_info },
	{ 0 }
};

//premodule
//
//  %r/%p %c %o
//
struct premodule_information
{
	const char *module; // %o - module name
};

premodule_information prem_info;

options premodule_options[] =
{
	{ 'o', &prem_info.module},
	{ 0 }
};

//postcommand
//
//  %r/%p %c
//
struct postcommand_information
{
	const char *directory; // %p - directory name
	const char *return_code; // %x - exit value
};

postcommand_information postcmd_info;

options postcommand_options[] =
{
	{ 'p', &postcmd_info.directory },
	{ 'x', &postcmd_info.return_code },
	{ 0 }
};

//postcommit
//
//  %r/%p
//
struct postcommit_information
{
	const char *directory; // %p - directory name
};

postcommit_information postcmt_info;

options postcommit_options[] =
{
	{ 'p', &postcmt_info.directory },
	{ 0 }
};

//postmodule
//
//  %r/%p %c %o
//
struct postmodule_information
{
	const char *module; // %o - module name
};

postmodule_information postm_info;

options postmodule_options[] =
{
	{ 'o', &postm_info.module},
	{ 0 }
};

//notify
//
// %<< [predefined]
struct notify_information
{
	const char *message; // %m - message supplied on command line
	const char *bugid; // %b - bug identifier
	const char *directory; // %p - directory name
	const char *notify_user; // %s -  user being notified
	const char *tag; // %t - tag or branch being edited
	const char *type; // %y - notification type
	const char *file; // %f - file being notified
};

notify_information notif_info;

options notify_options[] =
{
	{ 'm', &notif_info.message },
	{ 'b', &notif_info.bugid },
	{ 'p', &notif_info.directory },
	{ 's', &notif_info.notify_user },
	{ 't', &notif_info.tag },
	{ 'y', &notif_info.type },
	{ 'f', &notif_info.file },
	{ 0 }
};

//keywords
//
// Author	%a
// Date		%d
// Header	%r/%p/%f %v %d %a %s %l
// CVSHeader %p/%f %v %d %a %s %l
// Id		%f %v %d %a %s %l
// Locker	%l
// Log		%f
// Name		%N
// RCSfile	%f
// Revision %v
// Source	%r/%p/%f
// State	%s
// CommitId %C

struct keyword_information
{
	const char *directory; // %p - relative path
	const char *file; // %f - file
	const char *author; // %a - author
	const char *printable_date; // %d - date
	const char *rcs_date; // %D - rcs date
	const char *locker; // %l - locker
	const char *state; // %s - state
	const char *version; // %v - version
	const char *name; // %N - name
	const char *bugid; // %b - bugid
	const char *commitid; // %C - commitid
	const char *branch; // %t - branch
};

keyword_information keyword_info;
options keyword_options[] =
{
	{ 'p', &keyword_info.directory },
	{ 'f', &keyword_info.file },
	{ 'a', &keyword_info.author },
	{ 'd', &keyword_info.printable_date },
	{ 'D', &keyword_info.rcs_date },
	{ 'l', &keyword_info.locker },
	{ 's', &keyword_info.state },
	{ 'v', &keyword_info.version },
	{ 'N', &keyword_info.name },
	{ 'b', &keyword_info.bugid },
	{ 'C', &keyword_info.commitid },
	{ 't', &keyword_info.branch }
};

int init(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
{
	gen_info.command=command;
	gen_info.date=date;
	gen_info.hostname=hostname;
	gen_info.username=username;
	gen_info.virtual_repository=virtual_repository;
	gen_info.physical_repository=physical_repository;
	CServerIo::trace(3,"info_trigger: init, physical_repository=%s",physical_repository);
	gen_info.sessionid=sessionid;
	gen_info.editor=editor;
	gen_info.client_version=client_version;
	gen_info.character_set=character_set;
	for(int n=0; n<count_uservar; n++)
		gen_info.uservar[uservar[n]]=userval[n];
	static char pid[32];
	gen_info.pid=pid;
	sprintf(pid,"%08x",getpid());
	static char host[256];
	gethostname(host,sizeof(host));
	gen_info.local_hostname=host;
	static char cwd[PATH_MAX];
	getcwd(cwd,sizeof(cwd));
	gen_info.local_directory=cwd;
	return 0;
}

int close(const struct trigger_interface_t* cb)
{
	return 0;
}

int pretag(const struct trigger_interface_t* cb, const char *message, const char *directory, int name_list_count, const char **name_list, const char **version_list, char tag_type, const char *action, const char *tag)
{
	pret_info.message=message;
	pret_info.directory=directory;
	pret_info.tag_type=tag_type;
	pret_info.action=action;
	pret_info.tag=tag;
	pret_info.name_list_count=name_list_count;
	pret_info.name_list=name_list;
	pret_info.version_list=version_list;

	return parse_info(CVSROOT_TAGINFO,"%t %o %r/%p","%<{s v}",directory,generic_options,pretag_options);
}

int verifymsg(const struct trigger_interface_t* cb, const char *directory, const char *filename)
{
	verif_info.directory=directory;
	verif_info.filename=filename;

	return parse_info(CVSROOT_VERIFYMSG,"%l","",directory,generic_options,verifymsg_options);
}

static void loginfo_filesub(cvs::string& msg, const char *header, char type, int change_list_count, change_info_t *change_list)
{
	cvs::string line;
	std::map<cvs::string,int> tagList;
	bool header_done = false;

	for(int n=0; n<change_list_count; n++)
		tagList[change_list[n].tag?change_list[n].tag:"\xff"]++;
	for(std::map<cvs::string,int>::const_iterator i = tagList.begin(); i!=tagList.end(); ++i)
	{
		line="";
		if(i->first=="\xff" && tagList.size()>1)
			line+="      No tag\\n";
		else if(i->first!="\xff")
			line+="      Tag: "+i->first+"\\n";
		line+="\\t";
		for(int n=0; n<change_list_count; n++)
		{
			if(change_list[n].type!=type)
				continue;
			if(i->first!=(change_list[n].tag?change_list[n].tag:"\xff"))
				continue;
			if(!header_done)
			{
				msg+=header;
				header_done=true;
			}
			if(line.length()>1 && line.length()+8+strlen(change_list[n].filename)>72)
			{
				line+="\\n";
				msg+=line;
				line="\\t";
			}
			line+=change_list[n].filename;
			line+=' ';
		}
		if(header_done)
		{
			line+="\\n";
			msg+=line;
		}
	}
}

int loginfo(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list)
{
	cvs::string msg,line;

	login_info.message=message;
	login_info.status=status;
	login_info.directory=directory;
	login_info.change_list_count=change_list_count;
	login_info.change_list=change_list;

	msg="%<< Update of %r/%p\\nIn directory %H:%P\\n\\n";
	if(change_list_count)
	{
		loginfo_filesub(msg,"Modified Files:\\n",'M',change_list_count,change_list);
		loginfo_filesub(msg,"Added Files:\\n",'A',change_list_count,change_list);
		loginfo_filesub(msg,"Removed Files:\\n",'R',change_list_count,change_list);
	}
	msg+="Log Message:\\n%m";
	if(!*message || message[strlen(message)-1]!='\n')
	  msg+='\n';

	if(status && *status)
	{
		msg+="\\nStatus:\\n%T";
		if(status[strlen(status)-1]!='\n')
			msg+='\n';
	}

	return parse_info(CVSROOT_LOGINFO,"",msg.c_str(),directory,generic_options,loginfo_options);
}

int history(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message)
{
	hist_info.type=type;
	hist_info.revs=revs;
	hist_info.workdir=workdir;
	hist_info.name=name;
	hist_info.bugid=bugid;
	hist_info.message=message;

	return parse_info(CVSROOT_HISTORYINFO,"%t|%d|%u|%w|%s|%v","",NULL,generic_options,history_options);
}

int notify(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file)
{
	cvs::string msg;

	notif_info.message=message;
	notif_info.bugid=bugid;
	notif_info.directory=directory;
	notif_info.notify_user=notify_user;
	notif_info.tag=tag;
	notif_info.type=type;
	notif_info.file=file;

	return parse_info(CVSROOT_NOTIFY,"","%<< %p %f\\n---\\nTriggered %y watch on %r\\nBy %u",directory,generic_options,notify_options);
}

int precommit(const struct trigger_interface_t* cb, int name_list_count, const char **name_list, const char *message, const char *directory)
{
	precmt_info.message=message;
	precmt_info.directory=directory;
	precmt_info.name_list=name_list;
	precmt_info.name_list_count=name_list_count;

	return parse_info(CVSROOT_COMMITINFO,"%r/%p","%<s",directory,generic_options,precommit_options);
}

int postcommit(const struct trigger_interface_t* cb, const char *directory)
{
	postcmt_info.directory = directory;
	return parse_info(CVSROOT_POSTCOMMIT,"%r/%p","",directory,generic_options,postcommit_options);
}

int precommand(const struct trigger_interface_t* cb, int argc, const char **argv)
{
	prec_info.argc=argc;
	prec_info.argv=argv;

	return parse_info(CVSROOT_PRECOMMAND,"%r %c","%<a",NULL,generic_options,precommand_options);
}

int postcommand(const struct trigger_interface_t* cb, const char *directory, int return_code)
{
	char rc[32];
	snprintf(rc,sizeof(rc),"%d",return_code);
	postcmd_info.directory = directory;
	postcmd_info.return_code = rc;
	return parse_info(CVSROOT_POSTCOMMAND,"%r/%p %c","",directory,generic_options,postcommand_options);
}

int premodule(const struct trigger_interface_t* cb, const char *module)
{
	prem_info.module = module;

	return parse_info(CVSROOT_PREMODULE,"%r/%p %c %o","",module,generic_options,premodule_options);
}

int postmodule(const struct trigger_interface_t* cb, const char *module)
{
	postm_info.module = module;

	return parse_info(CVSROOT_POSTMODULE,"%r/%p %c %o","",module,generic_options,postmodule_options);
}

int get_template(const struct trigger_interface_t *cb, const char *directory, const char **template_ptr)
{
	if(!template_ptr)
		return 0;
	
	CServerIo::trace(3,"get_template(%s)",directory);
	static cvs::string temp;
	cvs::string file;
	temp="";
	int ret = parse_rcsinfo(CVSROOT_RCSINFO,directory,file);
	CFileAccess fa;
	if(file.size() && fa.open(file.c_str(),"rb"))
	{
		size_t len = (size_t)fa.length();
		CServerIo::trace(3,"Found a %d byte file",len);
		temp.resize(len);
		len = fa.read((void*)temp.data(),len);
		CServerIo::trace(3,"Read %d bytes",len);
		temp.resize(len);
		fa.close();
	}
	if(!ret && temp.size())
		*template_ptr = temp.c_str();
	return ret;
}

int parse_keyword(const struct trigger_interface_t *cb, const char *keyword,const char *directory,const char *file,const char *branch,const char *author,const char *printable_date,const char *rcs_date,const char *locker,const char *state,const char *version,const char *name,const char *bugid, const char *commitid, const property_info *props, size_t numprops, const char **value)
{
	if(!value)
		return 0;

	keyword_info.author=author;
	keyword_info.bugid=bugid;
	keyword_info.directory=directory;
	keyword_info.file=file;
	keyword_info.locker=locker;
	keyword_info.name=name;
	keyword_info.printable_date=printable_date;
	keyword_info.rcs_date=rcs_date;
	keyword_info.state=state;
	keyword_info.version=version;
	keyword_info.commitid=commitid;
	keyword_info.branch=branch;

	prop_info = props;
	prop_count = numprops;

	static cvs::string temp;
	temp="";
	int ret = parse_keywords(CVSROOT_KEYWORDS,directory,keyword,generic_options,keyword_options,temp,(locker&&*locker)?true:false);

	if(!ret && temp.size())
		*value=temp.c_str();

	prop_info = NULL;
	prop_count =0;

	return ret;
}

int prercsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	/* Not exposed to the commit support API */
	return 0;
}

int rcsdiff(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *diff, size_t difflen, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed)
{
	/* Not exposed to the commit support API */
	return 0;
}

/*
 *
 * Generic parser.
 *
 * <match|DEFAULT|ALL> <line>
 *
 * %x - Option x, possibly repeated
 * %{x,y,z} - Option x,y,z, possibly repeated
 * %<x - Option x to STDIN, possibly repeated
 * %<{x y z} - Option x,y,z to STDIN, possible repeated
 * %<< text - text to STDIN
 * %<<FOO
 *
 *   .... %x   %{%x %y %z}
 * FOO - Text to STDIN, with linefeeds
 */
int parse_info(const char *file, const char *default_cmd_parms, const char *default_io_parms, const char *directory, options *generic_options, options *spec_options)
{
	int ret = 0;
	size_t current_line = 0, default_current_line;
	cvs::string physical_file,default_line,here_text;
	cvs::wildcard_filename mod(directory?directory:"");
	cvs::sprintf(physical_file,512,"%s/%s",gen_info.physical_repository,file);
	bool found = false;
	const char *p;

	static std::map<cvs::filename,bool> meta_cache_valid;
	static std::map<cvs::filename,std::vector<cvs::string> > meta_cache;

	bool& cache_valid = meta_cache_valid[file];
	std::vector<cvs::string>& cache = meta_cache[file];
	char cwd[PATH_MAX];
	getcwd(cwd,sizeof(cwd));

	CServerIo::trace(3,"default_trigger: parse_info(%s,%s,%s,%s) cwd(%s)",file,default_cmd_parms,default_io_parms,directory?directory:"<null>",cwd);
	if(!cache_valid)
	{
		cvs::string line;
		CFileAccess acc;

		getcwd(cwd,sizeof(cwd));
		CServerIo::trace(3,"default_trigger: open(%s) cwd(%s)",physical_file.c_str(),cwd);
		if(!acc.open(physical_file.c_str(),"rb")) /* We have to open as binary, otherwise Win32 breaks... ftell() starts going negative!! */
		{
			getcwd(cwd,sizeof(cwd));
			CServerIo::trace(3,"default_trigger: no file cwd=%s",cwd);
			cache_valid = true;
			return 0;
		}
		while(acc.getline(line))
		{
			if(!line.length() && line[0]=='#')
				line.resize(0);
			cache.push_back(line);
		}
		acc.close();
		cache_valid = true;
	}

	for(current_line=0; current_line<cache.size(); current_line++)
	{
		cvs::string line;

		if(!cache[current_line].length() || cache[current_line][0]=='#')
			continue;

		line = cache[current_line];

		if(here_text.length())
		{
			if(here_text!=line)
				continue;
			here_text="";
			continue;
		}

		if((p=strstr(line.c_str(),"%<"))!=NULL)
		{
			p+=3;
			if(!isspace(*p))
				here_text=p;
		}

		if(!strchr(line.c_str(),'%') && default_cmd_parms)
		{
			line+=" ";
			line+=default_cmd_parms;
		}
		if(!strstr(line.c_str(),"%<") && default_io_parms)
		{
			line+=" ";
			line+=default_io_parms;
		}
		CTokenLine tok;
		const char *trailer;
		tok.addArgs(line.c_str(),1,&trailer);

		while(*trailer && isspace(*trailer))
			trailer++;

		CServerIo::trace(3,"Regexp match: %s - %s",tok[0],directory?directory:"");
		const char *tk = tok[0];
		bool add = false;
		if(tk[0]=='+') 
		{
			add=true;
			tk++;
		}
		if(strcmp(tok[0],"ALL")==0)
		{
			CServerIo::trace(3, "ALL found");
			ret += parse_info_line(cache,trailer,generic_options,spec_options,file,current_line);
			here_text="";
		}
		else if((!found || add) && mod.matches_regexp(tk)) // Wildcard match
		{
			CServerIo::trace(3,"Match found!");
			ret += parse_info_line(cache,trailer,generic_options,spec_options,file,current_line);
			here_text="";
			found=true;
		}
		else if(!strcmp(tok[0],"DEFAULT"))
		{
			default_current_line = current_line;
			default_line=trailer;
		}
	}
	if(!found && default_line.size())
	{
		ret += parse_info_line(cache,default_line.c_str(),generic_options,spec_options,file,default_current_line);
	}

	return ret;
}

int parse_rcsinfo(const char *file, const char *directory, cvs::string& rcsline)
{
	int ret = 0;
	size_t current_line, default_current_line;
	cvs::string str,default_line,here_text;
	cvs::wildcard_filename mod(directory?directory:"");
	cvs::sprintf(str,512,"%s/%s",gen_info.physical_repository,file);
	bool found = false;

	static bool cache_valid = false;
	static std::vector<cvs::string> cache;

	CServerIo::trace(3,"default_trigger: parse_rcsinfo(%s,%s)",file,directory?directory:"<null>");

	if(!cache_valid)
	{
		cvs::string line;
		CFileAccess acc;

		if(!acc.open(str.c_str(),"rb")) /* We have to open as binary, otherwise Win32 breaks... ftell() starts going negative!! */
		{
			CServerIo::trace(3,"default_trigger: no file");
			cache_valid = true;
			return 0;
		}
		while(acc.getline(line))
		{
			if(!line.length() && line[0]=='#')
				line.resize(0);
			cache.push_back(line);
		}
		acc.close();
		cache_valid = true;
	}

	for(current_line=0; !found && current_line<cache.size(); current_line++)
	{
		cvs::string line;

		if(!cache[current_line].length() || cache[current_line][0]=='#')
			continue;

		line = cache[current_line];

		CTokenLine tok;
		const char *trailer;
		tok.addArgs(line.c_str(),1,&trailer);

		while(*trailer && isspace(*trailer))
			trailer++;

		CServerIo::trace(3,"Regexp match: %s - %s",tok[0],directory?directory:"");
		if(mod.matches_regexp(tok[0])) // Wildcard match
		{
			found=true;
			CServerIo::trace(3,"Match found!");
			rcsline=trailer;
		}
		else if(!strcmp(tok[0],"DEFAULT"))
		{
			default_current_line = current_line;
			default_line=trailer;
		}
	}
	if(!found && default_line.size())
		rcsline=default_line;

	return ret;
}

int parse_keywords(const char *file, const char *directory, const char *keyword, options *generic_options, options *spec_options, cvs::string& keywordline, bool have_locker)
{
	int ret = 0;
	size_t current_line,found_line=0;
	cvs::string str;
	cvs::wildcard_filename mod(directory?directory:"");
	cvs::sprintf(str,512,"%s/%s",gen_info.physical_repository,file);
	bool insection = false,found = false,all = false,all_found=false;

	static bool cache_valid = false;
	static std::vector<cvs::string> cache;

	CServerIo::trace(3,"default_trigger: parse_keywords(%s,%s)",file,directory?directory:"<null>");

	if(!cache_valid)
	{
		cvs::string line;
		CFileAccess acc;

		if(!acc.open(str.c_str(),"rb")) /* We have to open as binary, otherwise Win32 breaks... ftell() starts going negative!! */
		{
			CServerIo::trace(3,"default_trigger: no file");
		}
		else
		{
			while(acc.getline(line))
			{
				if(!line.length() && line[0]=='#')
					line.resize(0);
				cache.push_back(line);
			}
			acc.close();
		}
		cache_valid = true;
	}

	str="";

	for(current_line=0; !found && current_line<cache.size(); current_line++)
	{
		cvs::string line;

		if(!cache[current_line].length() || cache[current_line][0]=='#')
			continue;

		line = cache[current_line];

		CTokenLine tok;
		const char *trailer;
		tok.addArgs(line.c_str(),1,&trailer);

		while(*trailer && isspace(*trailer))
			trailer++;

		if(isspace((unsigned char)line[0]))
		{
			if(!insection || strcmp(tok[0],keyword))
				continue;
			str = trailer;
			found_line = current_line;
			if(!all)
				found=true;
			else
				all_found=true;
			continue;
		}

		if(insection)
			insection=false;
		CServerIo::trace(3,"Regexp match: %s - %s",tok[0],directory?directory:"");
		if(mod.matches_regexp(tok[0])) // Wildcard match
		{
			insection=true;
			all=false;
			CServerIo::trace(3,"Match found!");
		}
		else if(!strcmp(tok[0],"ALL"))
		{
			insection=true;
			all=true;
			CServerIo::trace(3,"ALL section found");
		}
	}

	found|=all_found;

	if(!found)
	{
		if(!strcmp(keyword,"Author")) str="%a";
		else if(!strcmp(keyword,"Date")) str="%d";
		else if(!strcmp(keyword,"Header")) { if(have_locker) str="%r/%p/%f %v %d %a %s %l"; else str="%r/%p/%f %v %d %a %s"; }
		else if(!strcmp(keyword,"CVSHeader")) { if(have_locker) str="%p/%f %v %d %a %s %l"; else str="%p/%f %v %d %a %s"; }
		else if(!strcmp(keyword,"Id")) { if(have_locker) str="%f %v %d %a %s %l"; else str="%f %v %d %a %s"; }
		else if(!strcmp(keyword,"Locker")) str="%l";
		else if(!strcmp(keyword,"Log")) str="%f";
		else if(!strcmp(keyword,"Name")) str="%N";
		else if(!strcmp(keyword,"RCSfile")) str="%f";
		else if(!strcmp(keyword,"Revision")) str="%v";
		else if(!strcmp(keyword,"Source")) str="%r/%p/%f";
		else if(!strcmp(keyword,"State")) str="%s";
		else if(!strcmp(keyword,"CommitId")) str="%C";
		else if(!strcmp(keyword,"Branch")) str="%t";
		found=true;
	}

	if(found)
	{
		__parse_info_line(str.c_str(), generic_options, spec_options, file, found_line, NULL, NULL, keywordline, true);
	}

	return ret;
}

int parse_output(const char *str, size_t len, void *)
{
	return CServerIo::output(len,str);
}

int parse_error(const char *str, size_t len, void *)
{
	return CServerIo::error(len,str);
}

int parse_input(char *str, size_t len, void *)
{
	const char *s = g_io.c_str();
	if(g_ioPos>=g_io.size())
		return -1;
	size_t l = g_io.size()-g_ioPos;
	if(l>len) l=len;
	memcpy(str,s+g_ioPos,l);
	g_ioPos+=l;
	return l;
}


int parse_info_line(std::vector<cvs::string>&cache, const char *line, options *generic_options, options *spec_options, const char *source_file, size_t& source_line)
{
	int state;
	const char *here_text = NULL;
	cvs::string io,out;

	CServerIo::trace(3,"parse_info_line: Line=%s",line);
	state = __parse_info_line(line,generic_options,spec_options,source_file,source_line,&here_text,&io,out,false);
	if(state==3)
	{
		cvs::string l,o;
		/* Multiline text */
		do
		{
			source_line++;
			cvs::string& l = cache[source_line];
			if(source_line>=cache.size())
			{
				CServerIo::error("Unterminated multiline expansion at line %d of %s\n",source_line,source_file);
				return 1;
			}

			if(!strcmp(l.c_str(),here_text))
				break;
			/* Parse the line...*/
			o="";
			state = __parse_info_line(l.c_str(),generic_options,spec_options,source_file,source_line,NULL,NULL,o,false);
			if(state<0)
				return 1;
			io+=o+'\n';
		} while(1);
	}

	CRunFile rf;

	CServerIo::trace(3,"Run arguments: %s",out.c_str());

	rf.setArgs(out.c_str());
	if(io.size())
		rf.setInput(parse_input,NULL);
	rf.setOutput(parse_output,NULL);
	rf.setError(parse_error,NULL);
	g_io = io;
	g_ioPos=0;
	if(!rf.run(NULL))
	{
		CServerIo::warning("Script execution failed\n");
		return -1;
	}
	else
	{
		int ret;
		rf.wait(ret);
		return ret;
	}
}

int __parse_info_line(const char *line, options *generic_options, options *spec_options, const char *source_file, size_t& source_line, const char **here_text, cvs::string* io, cvs::string& out, bool no_escape)
{
	int state = 0, group_count = 0;
	const char *grouping = NULL;
	int stdio_state = 0;
	int more_todo = 0;
	bool spacer=false, bracket=false;
	cvs::string cur,tmp;
	options *o;
	const char *q;
	int l;

	cur.reserve(256);
	out.reserve(out.size()+strlen(line)+256);
	if(io)
		io->reserve(io->size()+strlen(line)+256);

	for(const char *p=line; *p; p++)
	{
		switch(state)
		{
		case 0:
			if(grouping)
			{
				CServerIo::error("Internal error: reached state 0 within group in line %d of %s\n",source_line,source_file);
				return -1;
			}
			switch(*p)
			{
			case '%':
				state=2; break;
			case '\\':
				state=1; break;
			case '$':
				state=4; break;
			default:
				cur+=*p;
				if(stdio_state==1)
					stdio_state=0;
				break;
			}
			break;
		case 1: // \x
			if(grouping)
			{
				CServerIo::error("Internal error: reached state 1 within group in line %d of %s\n",source_line,source_file);
				return -1;
			}
			switch(*p)
			{
			case 'n':
				cur+='\n';
				break;
			case 'r':
				cur+='\r';
				break;
			case 'b':
				cur+='\b';
				break;
			case 't':
				cur+='\t';
				break;
			default:
				if(isspace(*p) || *p=='%' || *p=='$' || *p==',' || *p=='{' || *p=='}' || *p=='<' || *p=='>' || *p=='\\' || *p=='\'' || *p=='"')
					cur+=*p;
				else
				{
					CServerIo::warning("Unknown escape character '\\%c' ignored.\n",*p);
					cur+='\\';
					cur+=*p;
				}
				break;
			}
			if(stdio_state==1)
				stdio_state=0;
			state=0;
			break;
		case 2: // %
			switch(*p)
			{
			case 'n':
				state=grouping?2:0;
				spacer=true;
				break;
			case ' ':
			case ',':
			case '%':
				spacer=true;
				cur+=*p;
				if(!grouping)
				{
					if(stdio_state==1)
						stdio_state=0;
				}
				state=grouping?2:0;
				break;
			case '{':
				if(grouping)
				{
					CServerIo::error("Invalid character '{' in line %d of %s\n",source_line,source_file);
					return -1;
				}
				grouping=p+1;
				more_todo = 0;
				group_count = -1;
				spacer=true;
				state=2;
				break;
			case '}':
				if(!grouping)
				{
					CServerIo::error("Invalid character '}' in line %d of %s\n",source_line,source_file);
					return -1;
				}
				if(stdio_state)
				{
					(*io)+=cur;
					(*io)+='\n';
				}
				if(more_todo)
				{
					if(!stdio_state)
						cur+=' ';
					else
						cur="";
					p = grouping-1;
					state = 2;
					if(group_count==-1) group_count++;
					if(!(more_todo&2))
					  group_count++;
					spacer=true;
					more_todo = 0;
				}
				else
				{
					if(!stdio_state)
					{
						if(no_escape)
							out+=cur.c_str();
						else
							out+=auto_escape(cur.c_str(),'"');
					}
					cur="";
					if(stdio_state==1)
						stdio_state=0;
					grouping = NULL;
					state = 0;
				}
				break;
			case '<':
				if(!grouping)
				{
					state=3;
					break;
				}
				/* Fall through */
			default:
				if(grouping)
				{
					if(!spacer)
						cur+=',';
					spacer = false;
				}
				/* process option lists */
				bool opt_found = false;
				for(size_t pass=0; pass<2 && !opt_found; pass++)
				{
					switch(pass)
					{
					case 0: o = spec_options; break;
					case 1: o = generic_options; break;
					}
					for(;o && o->command; o++)
					{
						if(o->command==*p)
						{
							opt_found = true;
							if(!o->fn)
							{
								if(*o->data)
								{
									if(grouping && !no_escape)
										cur+=auto_escape(*o->data,'\\');
									else if(!stdio_state && !no_escape)
										cur+=auto_escape(*o->data,'"');
									else
										cur+=*o->data;
								}
							}
							else
							{
								const char *d = NULL;
								if(!grouping)
								{
									group_count=0;
									o->fn(-1,NULL,o->param);
									do
									{
										more_todo = o->fn(group_count,&d,o->param);
										if(d)
										{
											if(stdio_state)
											{
												cur+=d;
												cur+='\n';
											}
											else
											{
												if((more_todo || group_count) && !no_escape)
													cur+=auto_escape(d,'\\');
												else
													cur+=d;
												if(more_todo)
													cur+=' ';
											}
											if(more_todo&1)
												group_count++;
											d=NULL;
										}
										else if(more_todo)
											group_count++;
									} while(more_todo);
									if(!stdio_state && !no_escape)
										cur=auto_escape(cur.c_str(),'"');
								}
								else
								{
									if(group_count==-1)
									{
										o->fn(-1,NULL,o->param);
										more_todo |= o->fn(0,&d,o->param);
									}
									else
										more_todo |= o->fn(group_count,&d,o->param);
									if(d)
									{
										if(no_escape)
											cur+=d;
										else
											cur+=auto_escape(d,'\\');
									}
								}
							}
							break;
						}
					}
				}
				if(!opt_found)
					CServerIo::warning("Unrecognised expansion '%c' in line %d of %s\n",*p,source_line,source_file);
				state=grouping?2:0; break;
			}
			break;
		case 3: // %<<
			if(!here_text)
			{
				CServerIo::error("Invalid < within multiline text at line %d of %s\n",source_line,source_file);
				return -1;
			}
			if(!io)
			{
				CServerIo::error("%< is not allowed in this file at line %d of %s\n",source_line,source_file);
				return -1;
			}
			switch(*p)
			{
			case '<':
				/* here document */
				if(!isspace(*(p+1)))
				{
					*here_text=p+1;
					p+=strlen(p);
					return state;
				}
				p++;
				state = 0;
				stdio_state=2;
				break;
			default:
				p--; /* Step back */
				stdio_state=1;
				state=2;
				break;
			}
			break;
		case 4: /* $var */
			if(*p=='{')
			{
				p++;
				q=strchr(p,'}');
				if(!q)
				{
					CServerIo::error("unterminated '{' in line %d of %s\n",source_line,source_file);
					return -1;
				}
				l=q-p;
				q+=2;
			}
			else
			{
				q=p;
				if(*q=='=')
					q++;
				while(isalnum(*q) || *q=='_')
					q++;
				l=q-p;
			}

			bool only_uservar=false;
			if(*p=='=')
			{
				p++;
				l--;
				only_uservar=true;
			}
			
			tmp=p;
			p=q-1;
			tmp.resize(l);

			size_t n = 0;
			if(!only_uservar)
			{
				for(n; env_info[n].env; n++)
				{
					if(!strcmp(env_info[n].env,tmp.c_str()) && env_info[n].val)
					{
						if(no_escape)
							cur += *env_info[n].val;
						else
							cur += auto_escape(*env_info[n].val,'\\');
						break;
					}
				}
			}
			if(only_uservar || !env_info[n].env)
			{
				uservar_t::const_iterator i = gen_info.uservar.find(tmp.c_str());
				bool ff = false;
				if(i!=gen_info.uservar.end())
				{
					if(no_escape)
						cur+=gen_info.uservar[tmp.c_str()];
					else
						cur+=auto_escape(gen_info.uservar[tmp.c_str()],'\\');
					ff = true;
				}
				if(!ff && prop_info && prop_count)
				{
					for(size_t p=0; p<prop_count; p++)
					{
						if(!strcmp(prop_info[p].property,tmp.c_str()))
						{
							if(no_escape)
								cur+=prop_info[p].value;
							else
								cur+=auto_escape(prop_info[p].value,'\\');
							ff = true;
							break;
						}
					}
				}
				if(!only_uservar && !ff)
				{
					q = CProtocolLibrary::GetEnvironment(tmp.c_str());
					if(q)
					{
						if(no_escape)
							cur+=q;
						else
							cur+=auto_escape(q,'\\');
					}
				}
			}

			state=0;
			break;
		}
		if(!grouping && cur.size())
		{
			if(stdio_state)
				(*io)+=cur;
			else
			{
				if(cur=="\\")
					out+="\\\\";
				else
					out+=cur;
			}
			if(stdio_state==1)
				stdio_state=0;
			cur="";
		}
		if(grouping && more_todo&6)
			p=strchr(p,'}')-1;
		more_todo=more_todo&~4;
	}
	if(grouping)
	{
		CServerIo::error("Unterminated group in line %d of %s\n",source_line,source_file);
		return -1;
	}

	return state;
}

const char *auto_escape(const char *_str, char quote)
{
    static const char meta[] = "`\"'\\ ";
	static cvs::string str;

	str=_str;

	if(!strpbrk(str.c_str(),meta))
		return str.c_str();
	else
	{
		str.reserve(str.size()+16);
		if(quote!='\\')
		{
			/* Replace any '\' or quote character within the string with \char */
			int i=0;
			char bs = '\\';
			char mt[3] = ".\\";

			mt[0]=quote;
			do
			{
				i=str.find_first_of(mt,i);
				if(i==cvs::string::npos)
					break;
				str.insert(i++,&bs,1);
				++i;
			} while(1);

			str.insert(str.begin(),quote);
			str.insert(str.end(),quote);
		}
		else
		{
			int i=0;

			do
			{
				i=str.find_first_of(meta,i);
				if(i==cvs::string::npos)
					break;
				str.insert(i++,&quote,1);
				++i;
			} while(1);
		}
	}
	return str.c_str();
}

} // anonymous namespace

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static trigger_interface callbacks =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"Generic commit support file handler",CVSNT_PRODUCTVERSION_STRING,NULL,
		init,
		destroy,
		get_interface,
		NULL
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

