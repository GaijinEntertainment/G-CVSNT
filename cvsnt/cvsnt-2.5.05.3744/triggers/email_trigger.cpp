/*
	CVSNT Email trigger handler
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
#include <ws2tcpip.h>
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
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#define MODULE email

#include <ctype.h>
#include <cvstools.h>
#include <map>
#include "../version.h"

#define CVSROOT_USERS		"CVSROOT/users"
#define CVSROOT_LOGINFO		"CVSROOT/commit_email"
#define CVSROOT_TAGINFO		"CVSROOT/tag_email"
#define CVSROOT_NOTIFY		"CVSROOT/notify_email"

#ifdef _WIN32
HMODULE g_hInst;

BOOL CALLBACK DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hInst = hModule;
	return TRUE;
}

#include "email_resource.h"

int win32config(const struct plugin_interface *ui, void *wnd);
#endif

namespace {

bool parse_emailinfo(const char *file, const char *directory, cvs::string& emailfile, bool& cache_valid, std::vector<cvs::string>& cache);
const char *map_username(const char *user);
bool start_mail(const char *from, const std::vector<cvs::string>& to);
bool send_mail_line(const char *line);
bool end_mail();

struct loginfo_change_t
{
	cvs::string filename;
	cvs::string rev_old;
	cvs::string rev_new;
	cvs::string bugid;
	cvs::string tag;
	cvs::string type;
};

typedef std::vector<loginfo_change_t> loginfo_change_list_t;
typedef std::map<cvs::filename, loginfo_change_list_t> loginfo_list_t;
typedef std::map<cvs::filename, loginfo_list_t> loginfo_t;

struct taginfo_change_t
{
	cvs::string filename;
	cvs::string version;
};

struct taginfo_change_list_t : public std::vector<taginfo_change_t>
{
	cvs::string tag_type;
	cvs::string tag;
	cvs::string action;
};
typedef std::map<cvs::filename, taginfo_change_list_t> taginfo_list_t;
typedef std::map<cvs::filename, taginfo_list_t> taginfo_t;

struct notify_change_t
{
	cvs::string filename;
	cvs::string bugid;
	cvs::string tag;
	cvs::string type;
};
typedef std::vector<notify_change_t> notify_change_list_t;
typedef std::map<cvs::filename, notify_change_list_t> notify_dir_list_t;
typedef std::map<cvs::username, notify_dir_list_t> notify_list_t;
typedef std::map<cvs::filename, notify_list_t> notify_t;


loginfo_t loginfo_data;
taginfo_t taginfo_data;
notify_t notify_data;
cvs::string loginfo_message;
cvs::string last_module;

typedef std::map<const char *,const char *> uservar_t;

struct
{
	const char *command; 
	const char *date; 
	const char *hostname; // [hostname]
	const char *username;
	const char *virtual_repository; // [repository]
	const char *physical_repository; 
	const char *sessionid; // [sessionid]/[commitid]
	const char *editor; 
	const char *local_hostname; // [server_hostname]
	const char *local_directory; 
	const char *client_version; 
	const char *character_set;
	uservar_t uservar; // User variables
	const char *pid;
} gen_info = {0};

int init(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set)
{
	char value[256];
	int val = 0;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","EmailTrigger",value,sizeof(value)))
		val = atoi(value);
	if(!val)
	{
		CServerIo::trace(3,"Email trigger not enabled.");
		return -1;
	}

	gen_info.command=command;
	gen_info.date=date;
	gen_info.hostname=hostname;
	gen_info.username=username;
	gen_info.virtual_repository=virtual_repository;
	gen_info.physical_repository=physical_repository;
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

	addrinfo *addr,hint={0};
	hint.ai_flags=AI_CANONNAME;

	if(!getaddrinfo(cvs::idn(host),NULL,&hint,&addr))
	{
		strcpy(host,cvs::decode_idn(addr->ai_canonname));
		freeaddrinfo(addr);
	}

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
	cvs::string file,tmp;

	static bool cache_valid = false;
	static std::vector<cvs::string> cache;

	if(!parse_emailinfo(CVSROOT_TAGINFO,directory,file,cache_valid,cache))
		return 0;

	if(CFileAccess::absolute(file.c_str()) || CFileAccess::uplevel(file.c_str())>0)
	{
		CServerIo::error("tag_email: Template file '%s' has invalid path.\n",file.c_str());
		return 1;
	}
	cvs::sprintf(tmp,80,"%s/CVSROOT/%s",gen_info.physical_repository,file.c_str());
	if(!CFileAccess::exists(tmp.c_str()))
	{
		CServerIo::error("tag_email: Template file '%s' does not exist.\n",file.c_str());
		return 0;
	}

	if(!name_list_count)
		return 0;

	loginfo_message = message?message:"";

	taginfo_change_list_t& change = taginfo_data[file.c_str()][directory];
	change.resize(name_list_count);
	change.tag = tag?tag:"";
	change.action = action?action:"";
	change.tag_type = tag_type?tag_type:'?';
	for(size_t n=0;n<(size_t)name_list_count; n++)
	{
		change[n].filename = name_list[n]?name_list[n]:"";
		change[n].version = version_list[n]?version_list[n]:"";
	}
	return 0;
}

int verifymsg(const struct trigger_interface_t* cb, const char *directory, const char *filename)
{
	return 0;
}

int loginfo(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list)
{
	cvs::string file,tmp;
	static bool cache_valid = false;
	static std::vector<cvs::string> cache;

	if(!parse_emailinfo(CVSROOT_LOGINFO,directory,file,cache_valid,cache))
		return 0;

	last_module = directory;
	if(strchr(directory,'/'))
		last_module.resize(last_module.find('/'));

	if(CFileAccess::absolute(file.c_str()) || CFileAccess::uplevel(file.c_str())>0)
	{
		CServerIo::error("commit_email: Template file '%s' has invalid path.\n",file.c_str());
		return 1;
	}
	cvs::sprintf(tmp,80,"%s/CVSROOT/%s",gen_info.physical_repository,file.c_str());
	if(!CFileAccess::exists(tmp.c_str()))
	{
		CServerIo::error("commit_email: Template file '%s' does not exist.\n",file.c_str());
		return 0;
	}

	loginfo_message = message;

	loginfo_change_list_t& change = loginfo_data[file.c_str()][directory];
	change.resize(change_list_count);
	for(size_t n=0;n<(size_t)change_list_count; n++)
	{
		change[n].filename = change_list[n].filename;
		change[n].rev_old = change_list[n].rev_old?change_list[n].rev_old:"";
		change[n].rev_new = change_list[n].rev_new?change_list[n].rev_new:"";
		change[n].bugid = change_list[n].bugid?change_list[n].bugid:"";
		change[n].tag = change_list[n].tag?change_list[n].tag:"";
		change[n].type = change_list[n].type?change_list[n].type:'?';
	}

	return 0;
}

int history(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message)
{
	return 0;
}

int notify(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file)
{
	cvs::string nfile,tmp;
	static bool cache_valid = false;
	static std::vector<cvs::string> cache;

	if(!parse_emailinfo(CVSROOT_NOTIFY,directory,nfile,cache_valid,cache))
		return 0;

	if(CFileAccess::absolute(nfile.c_str()) || CFileAccess::uplevel(nfile.c_str())>0)
	{
		CServerIo::error("notify_email: Template file '%s' has invalid path.\n",nfile.c_str());
		return 1;
	}
	cvs::sprintf(tmp,80,"%s/CVSROOT/%s",gen_info.physical_repository,nfile.c_str());
	if(!CFileAccess::exists(tmp.c_str()))
	{
		CServerIo::error("notify_email: Template file '%s' does not exist.\n",nfile.c_str());
		return 0;
	}

	// Notify is per-user as well
	notify_change_list_t& change = notify_data[nfile.c_str()][notify_user][directory];
	size_t n = change.size();
	change.resize(n+1);
	change[n].bugid=bugid;
	change[n].filename=file;
	change[n].tag=tag;
	change[n].type=type;
	loginfo_message = message?message:"";

	CServerIo::trace(3,"Notify array modified, size=%d, count=%d",notify_data.size(),change.size());
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
	loginfo_data.clear();
	taginfo_data.clear();
	notify_data.clear();
	return 0;
}

bool cleanup_single_email(cvs::string& email, const char *source)
{
	const char *s = source, *p;
	if(strchr(s,'<'))
		s = strchr(source,'<'+1);
	while(*s && isspace((unsigned char)*s)) s++;
	for(p=s; *p && !isspace((unsigned char)*p) && *p!='<' && *p!='>' && *p!='"' && *p!=','; p++)
		;
	if(p>s)
	{
		email = s;
		email.resize(p-s);
	}
	return true;
}
	
bool cleanup_multi_email(std::vector<cvs::string>& email, const char *source)
{
	do
	{
		cvs::string tmp;
		const char *s = source, *p;
		if(strchr(s,'<'))
			s = strchr(source,'<'+1);
		while(*s && isspace((unsigned char)*s)) s++;
		for(p=s; *p && !isspace((unsigned char)*p) && *p!='<' && *p!='>' && *p!='"' && *p!=','; p++)
			;
		do
		{
			if(!*p)
				break;
			if(isspace((unsigned char)*p)) p++;
			else if(*p=='>') p++;
			else if(*p=='"') p++;
			else 
				break;
		} while(1);
		if(p>s)
		{
			tmp = s;
			tmp.resize(p-s);
			email.push_back(tmp);
		}
		if(*p==',')
		{
			p++;
			while(isspace((unsigned char)*p)) p++;
			source = p;
		}
		else
			source = NULL;
	} while(source);

	return true;
}


bool read_template(const char *filename, std::vector<cvs::string>& cache, cvs::string& from, std::vector<cvs::string>& to)
{
	CFileAccess acc;
	size_t pos;
	cvs::string tmp;
	bool headers_done = false;
	bool from_seen = false;
	bool to_seen = false;

	cvs::sprintf(tmp,80,"%s/CVSROOT/%s",gen_info.physical_repository,filename);
	if(!acc.open(tmp.c_str(),"r"))
		return false;

	cvs::string line;

	while(acc.getline(line))
	{
		if(!headers_done && !line.size())
		{
			cvs::sprintf(line,80,"Message-ID: <%s@%s>",gen_info.sessionid,gen_info.local_hostname);
			cache.push_back(line);
			cache.push_back("");
			headers_done = true;
			continue;
		}

		// nonrepeating
		while((pos=line.find("[user]"))!=cvs::string::npos)
			line.replace(pos,6,gen_info.username);
		while((pos=line.find("[email]"))!=cvs::string::npos)
			line.replace(pos,7,map_username(gen_info.username));
		while((pos=line.find("[date]"))!=cvs::string::npos)
			line.replace(pos,6,gen_info.date);
		while((pos=line.find("[hostname]"))!=cvs::string::npos)
			line.replace(pos,10,gen_info.hostname);
		while((pos=line.find("[repository]"))!=cvs::string::npos)
			line.replace(pos,12,gen_info.virtual_repository);
		while((pos=line.find("[sessionid]"))!=cvs::string::npos)
			line.replace(pos,11,gen_info.sessionid);
		while((pos=line.find("[commitid]"))!=cvs::string::npos)
			line.replace(pos,10,gen_info.sessionid);
		while((pos=line.find("[server_hostname]"))!=cvs::string::npos)
			line.replace(pos,17,gen_info.local_hostname);
		while((pos=line.find("[message]"))!=cvs::string::npos)
			line.replace(pos,9,loginfo_message);
		while((pos=line.find("[module]"))!=cvs::string::npos)
			line.replace(pos,8,last_module);

		if(!headers_done)
		{
			if(!from_seen && !strncasecmp(line.c_str(),"From: ",6))
			{
				if(cleanup_single_email(from, line.c_str()+6))
					from_seen = true;
			}
			if(!strncasecmp(line.c_str(),"To: ",4) ||
				!strncasecmp(line.c_str(),"Cc: ",4))
			{
				if(cleanup_multi_email(to,line.c_str()+4))
					to_seen = true;
			}
			if(!strncasecmp(line.c_str(),"Bcc: ",5))
			{
				if(cleanup_multi_email(to,line.c_str()+5))
					to_seen = true;
				continue; // We don't cache Bcc
			}
			if(!strncasecmp(line.c_str(),"Message-ID: ",12))
				continue; // We generate that ourselves
		}
		cache.push_back(line);
	}
	acc.close();
	if(!headers_done || !from_seen || !to_seen)
	{
		CServerIo::error("Malformed email in '%s'.. need From/To\n",filename);
		return false;
	}
	return true;
}

int postcommand(const struct trigger_interface_t* cb, const char *directory, int return_code)
{
	size_t pos;

	for(loginfo_t::const_iterator i = loginfo_data.begin(); i!=loginfo_data.end(); ++i)
	{
		std::vector<cvs::string> cache;
		cvs::string from;
		std::vector<cvs::string> to;

		if(read_template(i->first.c_str(),cache, from, to))
		{
			size_t dir_repeat = 0;
			size_t file_repeat = 0;
			loginfo_change_list_t::const_iterator file_iterator;
			loginfo_list_t::const_iterator directory_iterator;
			start_mail(from.c_str(),to);
			for(size_t n=0; n<cache.size(); n++)
			{
				cvs::string line = cache[n];

				// repeating
				if(line=="[begin_directory]")
				{
					dir_repeat = n;
					directory_iterator = i->second.begin();
					continue;
				}
				if(line=="[end_directory]" && dir_repeat)
				{
					++directory_iterator;
					if(directory_iterator == i->second.end())
						dir_repeat = 0;
					else
						n=dir_repeat;
					file_repeat = 0;
					continue;
				}
				if(line=="[begin_file]")
				{
					if(!dir_repeat)
					{
						CServerIo::error("commit_email: [begin_file] not within [begin_directory] block");
						return 1;
					}
					file_repeat = n;
					file_iterator = directory_iterator->second.begin();
					continue;
				}
				if(line=="[end_file]" && file_repeat)
				{
					++file_iterator;
					if(file_iterator == directory_iterator->second.end())
						file_repeat = 0;
					else
						n=file_repeat;
					continue;
				}

				if(dir_repeat)
				{
					while((pos=line.find("[directory]"))!=cvs::string::npos)
						line.replace(pos,11,directory_iterator->first.c_str());
				}
				if(file_repeat)
				{
					while((pos=line.find("[filename]"))!=cvs::string::npos)
						line.replace(pos,10,file_iterator->filename);
					while((pos=line.find("[old_revision]"))!=cvs::string::npos)
						line.replace(pos,14,file_iterator->rev_old);
					while((pos=line.find("[new_revision]"))!=cvs::string::npos)
						line.replace(pos,14,file_iterator->rev_new);
					while((pos=line.find("[bugid]"))!=cvs::string::npos)
						line.replace(pos,7,file_iterator->bugid);
					while((pos=line.find("[tag]"))!=cvs::string::npos)
						line.replace(pos,5,file_iterator->tag);
					while((pos=line.find("[change_type]"))!=cvs::string::npos)
						line.replace(pos,13,file_iterator->type);
				}
				send_mail_line(line.c_str());
			}
			end_mail();
		}
	}

	for(taginfo_t::const_iterator i = taginfo_data.begin(); i!=taginfo_data.end(); ++i)
	{
		std::vector<cvs::string> cache;
		cvs::string from;
		std::vector<cvs::string> to;
		if(read_template(i->first.c_str(),cache, from, to))
		{
			size_t dir_repeat = 0;
			size_t file_repeat = 0;
			taginfo_change_list_t::const_iterator file_iterator;
			taginfo_list_t::const_iterator directory_iterator;
			start_mail(from.c_str(),to);
			for(size_t n=0; n<cache.size(); n++)
			{
				cvs::string line = cache[n];

				// repeating
				if(line=="[begin_directory]")
				{
					dir_repeat = n;
					directory_iterator = i->second.begin();
					if(directory_iterator == i->second.end())
						dir_repeat = 0;
					continue;
				}
				if(line=="[end_directory]" && dir_repeat)
				{
					++directory_iterator;
					if(directory_iterator == i->second.end())
						dir_repeat = 0;
					else
						n=dir_repeat;
					file_repeat = 0;
					continue;
				}
				if(line=="[begin_file]")
				{
					if(!dir_repeat)
					{
						CServerIo::error("commit_email: [begin_file] not within [begin_directory] block");
						return 1;
					}
					file_repeat = n;
					file_iterator = directory_iterator->second.begin();
					if(file_iterator == directory_iterator->second.end())
						file_repeat = 0;
					continue;
				}
				if(line=="[end_file]" && file_repeat)
				{
					++file_iterator;
					if(file_iterator == directory_iterator->second.end())
						file_repeat = 0;
					else
						n=file_repeat;
					continue;
				}

				if(dir_repeat)
				{
					while((pos=line.find("[directory]"))!=cvs::string::npos)
						line.replace(pos,11,directory_iterator->first.c_str());
					while((pos=line.find("[tag_type]"))!=cvs::string::npos)
						line.replace(pos,10,directory_iterator->second.tag_type.c_str());
					while((pos=line.find("[tag]"))!=cvs::string::npos)
						line.replace(pos,5,directory_iterator->second.tag.c_str());
					while((pos=line.find("[action]"))!=cvs::string::npos)
						line.replace(pos,8,directory_iterator->second.action.c_str());
				}
				if(file_repeat)
				{
					while((pos=line.find("[filename]"))!=cvs::string::npos)
						line.replace(pos,10,file_iterator->filename);
					while((pos=line.find("[revision]"))!=cvs::string::npos)
						line.replace(pos,10,file_iterator->version);
				}
				send_mail_line(line.c_str());
			}
			end_mail();
		}
	}

	for(notify_t::const_iterator i = notify_data.begin(); i!=notify_data.end(); ++i)
	{
		for(notify_list_t::const_iterator j = i->second.begin(); j!=i->second.end(); ++j)
		{
			std::vector<cvs::string> cache;
			cvs::string from;
			std::vector<cvs::string> to;
			cvs::string user = map_username(j->first.c_str());
			if(read_template(i->first.c_str(),cache, from, to))
			{
				size_t dir_repeat = 0;
				size_t file_repeat = 0;
				notify_change_list_t::const_iterator file_iterator;
				notify_dir_list_t::const_iterator directory_iterator;
				for(size_t n=0; n<to.size(); n++)
				{
					if(!strcmp(to[n].c_str(),"[to_user]"))
					    to[n]=user;
				}
				start_mail(from.c_str(),to);
				for(size_t n=0; n<cache.size(); n++)
				{
					cvs::string& line = cache[n]; 

					while((pos=line.find("[to_user]"))!=cvs::string::npos)
						line.replace(pos,10,user);

					// repeating
					if(line=="[begin_directory]")
					{
						dir_repeat = n;
						directory_iterator = j->second.begin();
						continue;
					}
					if(line=="[end_directory]" && dir_repeat)
					{
						++directory_iterator;
						if(directory_iterator == j->second.end())
							dir_repeat = 0;
						else
							n=dir_repeat;
						file_repeat = 0;
						continue;
					}
					if(line=="[begin_file]")
					{
						if(!dir_repeat)
						{
							CServerIo::error("commit_email: [begin_file] not within [begin_directory] block");
							return 1;
						}
						file_repeat = n;
						file_iterator = directory_iterator->second.begin();
						continue;
					}
					if(line=="[end_file]" && file_repeat)
					{
						++file_iterator;
						if(file_iterator == directory_iterator->second.end())
							file_repeat = 0;
						else
							n=file_repeat;
						continue;
					}

					if(dir_repeat)
					{
						while((pos=line.find("[directory]"))!=cvs::string::npos)
							line.replace(pos,11,directory_iterator->first.c_str());
					}
					if(file_repeat)
					{
						while((pos=line.find("[filename]"))!=cvs::string::npos)
							line.replace(pos,10,file_iterator->filename);
						while((pos=line.find("[bugid]"))!=cvs::string::npos)
							line.replace(pos,7,file_iterator->bugid);
						while((pos=line.find("[tag]"))!=cvs::string::npos)
							line.replace(pos,5,file_iterator->tag);
						while((pos=line.find("[notify_type]"))!=cvs::string::npos)
							line.replace(pos,13,file_iterator->type);
					}
					send_mail_line(line.c_str());
				}
				end_mail();
			}
			else
				CServerIo::trace(3,"read_template() failed");
		}
	}

	loginfo_data.clear();
	taginfo_data.clear();
	notify_data.clear();
	return 0;
}

int premodule(const struct trigger_interface_t* cb, const char *module)
{
	return 0;
}

int postmodule(const struct trigger_interface_t* cb, const char *module)
{
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

bool parse_emailinfo(const char *file, const char *directory, cvs::string& emailfile, bool& cache_valid, std::vector<cvs::string>& cache)
{
	size_t current_line, default_current_line;
	cvs::string str,default_line,here_text;
	cvs::wildcard_filename mod(directory?directory:"");
	cvs::sprintf(str,512,"%s/%s",gen_info.physical_repository,file);
	bool found = false;

	CServerIo::trace(3,"email_trigger: parse_emailinfo(%s,%s)",file,directory?directory:"<null>");

	if(!cache_valid)
	{
		cvs::string line;
		CFileAccess acc;

		if(!acc.open(str.c_str(),"rb")) /* We have to open as binary, otherwise Win32 breaks... ftell() starts going negative!! */
		{
			CServerIo::trace(3,"email_trigger: no file");
			cache_valid = true;
			return false;
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
			CServerIo::trace(3,"Match found");
			emailfile=trailer;
		}
		else if(!strcmp(tok[0],"DEFAULT"))
		{
			CServerIo::trace(3,"Default found");
			default_current_line = current_line;
			default_line=trailer;
		}
		else
			CServerIo::trace(3,"No match");
	}
	if(!found && default_line.size())
	{
		CServerIo::trace(3,"using default line");
		emailfile=default_line;
		found = true;
	}
	if(!found)
		CServerIo::trace(3,"No match on any lines");

	return found;
}

const char *map_username(const char *user)
{
	static cvs::string str;
	static bool cache_valid = false;
	static std::map<cvs::username,cvs::string> cache;
	static char emaildomain[256];

	CServerIo::trace(3,"email_trigger: map_username(%s)",user);

	if(!cache_valid)
	{
		cvs::string line;
		CFileAccess acc;

		if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","EmailDomain",emaildomain,sizeof(emaildomain)))
			emaildomain[0]='\0';

		cvs::sprintf(str,512,"%s/%s",gen_info.physical_repository,CVSROOT_USERS);
		if(!acc.open(str.c_str(),"r")) 
		{
			CServerIo::trace(3,"email_trigger: no file");
			cache_valid = true;
			if(strchr(user,'@') || !emaildomain[0])
				return user;
			cvs::sprintf(str,80,"%s@%s",user,emaildomain);
			return str.c_str();
		}
		while(acc.getline(line))
		{
			if(!line.length() && line[0]=='#')
				continue;
			const char *q=line.c_str();
			char *p=(char*)strchr(q,':');
			if(!p) continue;
			*(p++)='\0';
			cache[q]=p;
		}
		acc.close();
		cache_valid = true;
	}
	
	if(cache.find(user)!=cache.end())
		user = cache[user].c_str();
	if(strchr(user,'@') || !emaildomain[0])
		return user;
	cvs::sprintf(str,80,"%s@%s",user,emaildomain);
	return str.c_str();
}

bool get_smtp_response(CSocketIO& sock)
{
	cvs::string line;
	if(!sock.getline(line))
	{
		CServerIo::trace(3,"SMTP server dropped connection!\n");
		return false;
	}
	CServerIo::trace(3,"SMTP S: %s",line.c_str());
	int type = atoi(line.c_str())/100;
	if(type==2 || type==3)
		return true;
	CServerIo::error("SMTP error: %s\n",line.c_str());
	return false;
}

class CMailIo
{
public:
	CMailIo() { }
	virtual ~CMailIo() { }
	virtual bool start_mail(const char *from, const std::vector<cvs::string>& to) =0;
	virtual bool send_mail_line(const char *line) =0;
	virtual bool end_mail() =0;
};

class CSmtpMailIo : public CMailIo
{
public:
	CSmtpMailIo() { }
	virtual ~CSmtpMailIo() { }
	virtual bool start_mail(const char *from, const std::vector<cvs::string>& to);
	virtual bool send_mail_line(const char *line);
	virtual bool end_mail();

	CSocketIO m_sock;
};

class CCommandMailIo : public CMailIo
{
public:
	CCommandMailIo(const char *command) { m_command = command; }
	virtual ~CCommandMailIo() { }
	virtual bool start_mail(const char *from, const std::vector<cvs::string>& to);
	virtual bool send_mail_line(const char *line);
	virtual bool end_mail();

	CRunFile m_run;
	size_t m_pos;
	cvs::string m_command,m_mail;

	static int _mailInput(char *buf, size_t len, void *param);
	int mailInput(char *buf, size_t len);

};

CMailIo* g_mailio;

bool start_mail(const char *from, const std::vector<cvs::string>& to)
{
	char mailcommand[1024];

	if(g_mailio)
		delete g_mailio;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","MailCommand",mailcommand,sizeof(mailcommand)) && mailcommand[0])
		g_mailio = new CCommandMailIo(mailcommand);
	else
		g_mailio = new CSmtpMailIo;
	return g_mailio->start_mail(from,to);
}

bool send_mail_line(const char *line)
{
	return g_mailio->send_mail_line(line);
}

bool end_mail()
{
	bool bRet = g_mailio->end_mail();
	delete g_mailio;
	g_mailio = NULL;
	return bRet;
}

bool CSmtpMailIo::start_mail(const char *from, const std::vector<cvs::string>& to)
{
	char mailserver[256],emaildomain[256];

	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","MailServer",mailserver,sizeof(mailserver)))
	{
		CServerIo::error("email_trigger: Mail server not set - cannot send.\n");
		return false;
	}
	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","EmailDomain",emaildomain,sizeof(emaildomain)))
		emaildomain[0]='\0';

	if(!m_sock.create(mailserver,"25",false,true) || !m_sock.connect())
	{
		CServerIo::error("email_trigger: Couldn't connect to mail server: %s\n",m_sock.error());
		return false;
	}
	if(!to.size())
		return false;

	if(!get_smtp_response(m_sock))
		return false;
	CServerIo::trace(3,"SMTP C: HELO %s",gen_info.local_hostname);
	m_sock.printf("HELO %s\r\n",gen_info.local_hostname);
	if(!get_smtp_response(m_sock))
		return false;
	if(strchr(from,'@') || !emaildomain[0])
	{
		CServerIo::trace(3,"SMTP C: MAIL FROM:<%s>",from);
		m_sock.printf("MAIL FROM:<%s>\r\n",from);
	}
	else
	{
		CServerIo::trace(3,"SMTP C: MAIL FROM:<%s@%s>",from,emaildomain);
		m_sock.printf("MAIL FROM:<%s@%s>\r\n",from,emaildomain);
	}
	if(!get_smtp_response(m_sock))
		return false;
	for(size_t n=0; n<to.size(); n++)
	{
		if(strchr(to[n].c_str(),'@') || !emaildomain[0])
		{
			CServerIo::trace(3,"SMTP C: RCPT TO:<%s>",to[n].c_str());
			m_sock.printf("RCPT TO:<%s>\r\n",to[n].c_str());
		}
		else
		{
			CServerIo::trace(3,"SMTP C: RCPT TO:<%s@%s>",to[n].c_str(),emaildomain);
			m_sock.printf("RCPT TO:<%s@%s>\r\n",to[n].c_str(),emaildomain);
		}
		if(!get_smtp_response(m_sock))
			return false;
	}
	CServerIo::trace(3,"SMTP C: DATA");
	m_sock.printf("DATA\r\n");
	if(!get_smtp_response(m_sock))
		return false;
	return true;
}

bool CSmtpMailIo::send_mail_line(const char *line)
{
	if(!strcmp(line,"."))
		m_sock.printf("..\r\n");
	else
		m_sock.printf("%s\r\n",line);
	return true;
}

bool CSmtpMailIo::end_mail()
{
	m_sock.printf(".\r\n");
	if(!get_smtp_response(m_sock))
		return false;
	CServerIo::trace(3,"SMTP C: QUIT");
	m_sock.printf("QUIT\r\n");
	if(!get_smtp_response(m_sock))
		return false;
	m_sock.close();
	return true;
}

bool CCommandMailIo::start_mail(const char *from, const std::vector<cvs::string>& to)
{
	CServerIo::trace(3,"email_trigger: Sending mail with command: %s",m_command.c_str());
	m_run.setArgs(m_command.c_str());
	for(size_t n=0; n<to.size(); n++)
	{
		CServerIo::trace(3,"email_trigger: Argument: %s",to[n].c_str());
		m_run.addArg(to[n].c_str());
	}
	m_mail="";
	m_pos=0;
	return true;
}

bool CCommandMailIo::send_mail_line(const char *line)
{
	m_mail+=line;
	m_mail+="\n";
	return true;
}

bool CCommandMailIo::end_mail()
{
	m_run.setInput(_mailInput,this);
	if(!m_run.run(NULL))
	{
		CServerIo::trace(3,"unable to run MailCommand");
		return false;
	}
	int ret;
	if(!m_run.wait(ret))
	{
		CServerIo::trace(3,"unable to run MailCommand");
		return false;
	}
	if(ret)
		CServerIo::trace(3,"MailCommand returned %d",ret);
	return true;
}

int CCommandMailIo::_mailInput(char *buf, size_t len, void *param)
{
	return ((CCommandMailIo*)param)->mailInput(buf,len);
}

int CCommandMailIo::mailInput(char *buf, size_t len)
{
	int todo;
	if(m_pos>=m_mail.size())
		return 0;
	if(len>(m_mail.size()-m_pos))
		todo = m_mail.size()-m_pos;
	else
		todo = len;

	memcpy(buf,m_mail.c_str()+m_pos,todo);
	m_pos+=todo;
	return todo;
}

} // anonymous namespace

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static trigger_interface callbacks =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"Email notification extension",CVSNT_PRODUCTVERSION_STRING,"EmailTrigger",
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
		if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","EmailTrigger",value,sizeof(value)))
			nSel = atoi(value);
		if(!nSel)
		{
			EnableWindow(GetDlgItem(hWnd,IDC_DOMAINNAME),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPSERVERNAME),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_COMMANDNAME),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPEXTERNAL),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPINTERNAL),FALSE);
		}
		else
			SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,1,NULL);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","EmailDomain",value,sizeof(value)))
			SetDlgItemText(hWnd,IDC_DOMAINNAME,value);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","MailServer",value,sizeof(value)))
			SetDlgItemText(hWnd,IDC_SMTPSERVERNAME,value);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","MailCommand",value,sizeof(value)))
			SetDlgItemText(hWnd,IDC_COMMANDNAME,value);
		else
			value[0]='\0';
		if(*value)
		{
			CheckRadioButton(hWnd,IDC_SMTPINTERNAL,IDC_SMTPEXTERNAL,IDC_SMTPEXTERNAL);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPSERVERNAME),FALSE);
		}
		else
		{
			CheckRadioButton(hWnd,IDC_SMTPINTERNAL,IDC_SMTPEXTERNAL,IDC_SMTPINTERNAL);
			EnableWindow(GetDlgItem(hWnd,IDC_COMMANDNAME),FALSE);
		}
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_CHECK1:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,NULL,NULL);
			EnableWindow(GetDlgItem(hWnd,IDC_DOMAINNAME),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPSERVERNAME),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_COMMANDNAME),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPEXTERNAL),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPINTERNAL),nSel?TRUE:FALSE);
			return TRUE;
		case IDC_SMTPEXTERNAL:
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPSERVERNAME),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_COMMANDNAME),TRUE);
			return TRUE;
		case IDC_SMTPINTERNAL:
			EnableWindow(GetDlgItem(hWnd,IDC_SMTPSERVERNAME),TRUE);
			EnableWindow(GetDlgItem(hWnd,IDC_COMMANDNAME),FALSE);
			return TRUE;
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,NULL,NULL);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","Plugins","EmailTrigger",value);
			GetDlgItemText(hWnd,IDC_DOMAINNAME,value,sizeof(value));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","EmailDomain",value);
			GetDlgItemText(hWnd,IDC_SMTPSERVERNAME,value,sizeof(value));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","MailServer",value);
			GetDlgItemText(hWnd,IDC_COMMANDNAME,value,sizeof(value));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","MailCommand",value);
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

