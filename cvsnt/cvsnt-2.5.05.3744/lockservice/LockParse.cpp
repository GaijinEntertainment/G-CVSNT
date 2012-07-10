/*	cvsnt lockserver
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
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#if defined( __HP_aCC ) && defined ( __ia64 )
#include <unistd.h>
#endif
#include <time.h>
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "../cvsapi/cvsapi.h"

#include "LockService.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

static bool DoClient(CSocketIOPtr s,size_t client, char *param);
static bool DoLock(CSocketIOPtr s,size_t client, char *param);
static bool DoUnlock(CSocketIOPtr s,size_t client, char *param);
static bool DoMonitor(CSocketIOPtr s,size_t client, char *param);
static bool DoClients(CSocketIOPtr s,size_t client, char *param);
static bool DoLocks(CSocketIOPtr s,size_t client, char *param);
static bool DoModified(CSocketIOPtr s,size_t client, char *param);
static bool DoVersion(CSocketIOPtr s,size_t client, char *param);
static bool DoClose(CSocketIOPtr s,size_t client, char *param);

static bool request_lock(size_t client, const char *path, unsigned flags, size_t& lock_to_wait_for);

extern bool g_bTestMode;
#define DEBUG if(g_bTestMode) printf

static size_t global_lockId;

const char *StateString[] = { "Logging in", "Active", "Monitoring", "Closed" };
enum ClientState { lcLogin, lcActive, lcMonitor, lcClosed };
enum LockFlags { lfRead = 0x01, lfWrite = 0x02, lfClosed = 0x10 };
enum MonitorFlags { lcMonitorClient=0x01, lcMonitorLock=0x02, lcMonitorVersion=0x04 };

typedef std::map<std::string,std::string> VersionMapType;

#ifdef _WIN32
CRITICAL_SECTION g_crit;

void InitServer()
{
	InitializeCriticalSection(&g_crit);
}

void CloseServer()
{
	DeleteCriticalSection(&g_crit);
}

struct ClientLock
{
	ClientLock() { EnterCriticalSection(&g_crit); }
	~ClientLock() { LeaveCriticalSection(&g_crit); }
};

#elif defined(HAVE_PTHREAD_H)
pthread_mutex_t crit_lock = PTHREAD_MUTEX_INITIALIZER;

void InitServer()
{
}

void CloseServer()
{
}

struct ClientLock
{
	ClientLock() { pthread_mutex_lock(&crit_lock); }
	~ClientLock() { pthread_mutex_unlock(&crit_lock); }
};

#else
void InitServer()
{
}

void CloseServer()
{
}

struct ClientLock
{
	ClientLock() { }
	~ClientLock() { }
};

#endif

struct locktime_t
{
	clock_t clock;
	unsigned long rollover;

	locktime_t() { clock=0; rollover=0; }
	locktime_t(const locktime_t& other) { clock=other.clock; rollover=other.rollover; }
	locktime_t(clock_t _clock, unsigned long _roll) { clock=_clock; rollover=_roll; }
	bool operator<(const locktime_t& other) const { return rollover<other.rollover || (rollover==other.rollover && clock<other.clock); }
	bool operator>(const locktime_t& other) const { return rollover>other.rollover || (rollover==other.rollover && clock>other.clock); }
	bool operator<=(const locktime_t& other) const { return rollover<other.rollover || (rollover==other.rollover && clock<=other.clock); }
	bool operator>=(const locktime_t& other) const { return rollover>other.rollover || (rollover==other.rollover && clock<=other.clock); }
	bool operator==(const locktime_t& other) const { return rollover==other.rollover && clock==other.clock; }
	bool operator!=(const locktime_t& other) const { return rollover!=other.rollover || clock!=other.clock; }
	bool iszero() const { return !rollover && !clock; };
};

struct Lock
{
	size_t owner;
	std::string path;
	unsigned flags;
	size_t length; /* length of path */
	VersionMapType versions;
};

struct TransactionStruct
{
	TransactionStruct();
	~TransactionStruct();
	size_t owner;
	std::string path;
	std::string branch;
	std::string version;
	std::string oldversion;
	char type;

	bool operator==(const char *oth) { return !strcmp(path.c_str(),oth)?true:false; }
};

typedef std::vector<TransactionStruct> TransactionListType;

struct LockClient
{
	LockClient() { state=lcClosed; flags=0; }
	std::string server_host;
	std::string client_host;
	std::string user;
	std::string root;
	ClientState state;
	unsigned flags;
	locktime_t starttime;
	locktime_t endtime;
};

typedef std::map<size_t,LockClient> LockClientMapType;
typedef std::map<size_t,Lock> LockMapType;
LockClientMapType LockClientMap;
LockMapType LockMap;
TransactionListType TransactionList;

TransactionStruct::TransactionStruct()
{
}

TransactionStruct::~TransactionStruct()
{
}

std::map<int,size_t> SockToClient;

size_t next_client_id;

/* predicate for partition below */
struct IsWantedTransaction { bool operator()(const TransactionStruct& t) { return LockClientMap.find(t.owner)!=LockClientMap.end(); } };

bool TimeIntersects(locktime_t start1, locktime_t end1, locktime_t start2, locktime_t end2)
{
   return (start1<=start2 && (end1.iszero() || end1>=start2)) || (start1>=start2 && (end2.iszero() || end2>=start1));
}

locktime_t lock_time()
{
	static locktime_t last_clock;
	clock_t this_clock;

#ifdef _WIN32
	this_clock = GetTickCount();
#else
	struct tms ignored;
	this_clock = times(&ignored);
#endif

	if(this_clock<last_clock.clock)
		last_clock.rollover++;
	last_clock.clock = this_clock;
	return locktime_t(last_clock);
}

bool RootOverlaps(size_t client, std::string& root1, std::string& root2)
{
	if(root1.length()>root2.length())
		return strncmp(root1.c_str(),root2.c_str(),root2.length())?false:true;
	else
		return strncmp(root1.c_str(),root2.c_str(),root1.length())?false:true;
}

// When we're parsing stuff, it's sometimes handy
// to have strchr return the final token as if it
// had an implicit delimiter at the end.
static char *lock_strchr(const char *s, int ch)
{
	if(!*s)
		return NULL;
	const char *p=strchr(s,ch);
	if(p)
		return (char*)p;
	return (char*)s+strlen(s);
}

const char *FlagToString(unsigned flag)
{
	static char flg[1024];

	flg[0]='\0';
	if(flag&lfRead)
		strcat(flg,"Read ");
	if(flag&lfWrite)
		strcat(flg,"Write ");
	if(flg[0])
		flg[strlen(flg)-1]='\0'; // Chop trailing space
	return flg;
}

bool OpenLockClient(CSocketIOPtr s)
{
	cvs::string host;
#if defined( __HP_aCC ) && defined ( __ia64 )
	host.resize(255);
	if(gethostname(host.data(),255)==-1)
#else
	if(!s->gethostname(host))
#endif
	{
#if defined( __HP_aCC ) && defined ( __ia64 )
		DEBUG("gethostname (HP) failed: %s\n",strerror(errno));
#else
		DEBUG("gethostname failed: %s\n",s->error());
#endif
		return false;
	}

	LockClient l;
	l.server_host=host;
	l.state=lcLogin;
	l.starttime=lock_time();

	{
		ClientLock lock;

		size_t client = ++next_client_id;
		SockToClient[s->getsocket()]=client;
		LockClientMap[client]=l;

		DEBUG("Lock Client #%d(%s) opened\n",(int)client,host.c_str());
	}


	s->printf("CVSLock 2.21 Ready\n");

	return true;
}

bool CloseLockClient(CSocketIOPtr s)
{
	ClientLock lock;

	if(SockToClient.find(s->getsocket())==SockToClient.end())
	   return true; // Already closed

	size_t client = SockToClient[s->getsocket()];
	int count=0;
	for(LockMapType::iterator i = LockMap.begin(); i!=LockMap.end();)
	{
		if(i->second.owner == client) 
		{
			count++;
			LockMap.erase(i++);
		}
		else
			++i;
	}
	DEBUG("Destroyed %d locks\n",count);
	SockToClient.erase(SockToClient.find(s->getsocket()));
	LockClientMap[client].state=lcClosed;
	LockClientMap[client].endtime=lock_time();
	if(LockClientMap.size()<=1)
	{
		// No clients, just empty the transaction store
		LockClientMap.clear();
		TransactionList.clear();
		DEBUG("No more clients\n");
	}
	else if(TransactionList.size())
	{
		// Find out which stored clients are redundant
		//
		// The rule is that as long as there's an active client
		// that started before our client finished, you can't
		// delete it.  This random overlapping is what makes
		// atomicity hard :)
		//
		// remove_if doesn't work on maps so we end up with trickery
		// like this...
/*		LockClientMapType::iterator c,d;
		for (c=LockClientMap.begin(); c!=LockClientMap.end();)
		{
		  if(!c->second.endtime.iszero())
		  {
		    bool can_delete = true;
		    for (d=LockClientMap.begin(); d!=LockClientMap.end(); ++d)
		    {
				if(c->first != d->first && d->second.endtime.iszero() &&
					TimeIntersects(c->second.starttime,c->second.endtime,d->second.starttime,d->second.endtime) &&
					RootOverlaps(d->first,c->second.root,d->second.root))
				can_delete = false;
		    }
		    if(can_delete)
				LockClientMap.erase(c++);
		    else
		      ++c;
		  }
		  else
		    ++c;
		}
		*/
		LockClientMap.erase(LockClientMap.find(client));
		DEBUG("%d clients left\n",(int)LockClientMap.size());
		/* Life is much easier on a vector */
		size_t pos = std::partition(TransactionList.begin(), TransactionList.end(), IsWantedTransaction()) - TransactionList.begin();
		TransactionList.resize(pos);
	}
	else
	{
	  /* No transactions, so just erase this client */
	  DEBUG("No pending transactions\n");
	  LockClientMap.erase(LockClientMap.find(client));
	}

	DEBUG("Lock Client #%d closed\n",(int)client);
	return true;
}

// Lock commands:
//
// Client <user>'|'<root>['|'<client_host>]
// Lock [Read] [Write]'|'<path>['|'<branch>]
// Unlock <LockId>
// Version <LockId>|<Branch>
// Monitor [C][L][V]
// Clients
// Locks
// Modified [Added|Deleted]'|'LockId'|'<branch>'|'<version>['|'<oldversion>]
//
// Errors:
// 000 OK {message}
// 001 FAIL {message}
// 002 WAIT {message}
// 003 WARN (message)
// 010 MONITOR {message}

bool ParseLockCommand(CSocketIOPtr s, const char *command)
{
	char *cmd,*p;
	char *param;
	bool bRet = true;
	size_t client;

	if(!*command)
		return bRet; // Empty line

	cmd = strdup(command);
	{
		ClientLock lock;
		client = SockToClient[s->getsocket()];
	}
	p=lock_strchr(cmd,' ');
	if(!p)
	{
		s->printf("001 FAIL Syntax error\n");
	}
	else
	{
		if(!*p)
			param = p;
		else
			param = p+1;
		*p='\0';
		if(!strcmp(cmd,"Client"))
			DoClient(s,client,param);
		else if(!strcmp(cmd,"Lock"))
			DoLock(s,client,param);
		else if(!strcmp(cmd,"Unlock"))
			DoUnlock(s,client,param);
		else if(!strcmp(cmd,"Version"))
			DoVersion(s,client,param);
		else if(!strcmp(cmd,"Modified"))
			DoModified(s,client,param);
		else if(!strcmp(cmd,"Monitor"))
			DoMonitor(s,client,param);
		else if(!strcmp(cmd,"Clients"))
			DoClients(s,client,param);
		else if(!strcmp(cmd,"Locks"))
			DoLocks(s,client,param);
		else if(!strcmp(cmd,"Close"))
		{
			DoClose(s,client,param);
			bRet=false;
		}
		else
			s->printf("001 FAIL Unknown command '%s'\n",cmd);
	}
	free(cmd);
	return bRet;
}

bool DoClient(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	char *user, *root, *host, *flags;
	if(LockClientMap[client].state!=lcLogin)
	{
		s->printf("001 FAIL Unexpected 'Client' command\n");
		return false;
	}
	root = strchr(param,'|');
	if(!root)
	{
		s->printf("001 FAIL Client command expects <user>|<root>[[|<client host>]|Case]\n");
		return false;
	}
	*(root++)='\0';

	host = strchr(root,'|');
	if(host)
	{
		*(host++)='\0';
		flags = strchr(host,'|');
		if(flags)
			*(flags++)='\0';
		if(flags)
		{
			if(!strcmp(flags,"Case"))
			{
				s->printf("001 FAIL Case insensitive locks no longer supported\n");
				return false;
			}
		}
	}

	user = param;

	LockClientMap[client].user=user;
	LockClientMap[client].root=root;
	LockClientMap[client].client_host=host?host:"";
	LockClientMap[client].state=lcActive;

	DEBUG("(#%d) New client %s(%s) root %s\n",(int)client,user,host?host:"unknown",root);
	s->printf("000 OK Client registered\n");
	return true;
}

bool DoLock(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	char *flags, *path,*p;
	unsigned uFlags=0;
	size_t lock_to_wait_for;

	if(LockClientMap[client].state!=lcActive)
	{
		s->printf("001 FAIL Unexpected 'Lock' command\n");
		return false;
	}
	path = strchr(param,'|');
	if(!path)
	{
		s->printf("001 FAIL Lock command expects <flags>|<path>\n");
		return false;
	}
	*(path++)='\0';
	flags = param;
	while((p=lock_strchr(flags,' '))!=NULL)
	{
		char c=*p;
		*p='\0';
		if(!strcmp(flags,"Read"))
			uFlags|=lfRead;
		else if(!strcmp(flags,"Write"))
			uFlags|=lfWrite;
		else
		{
			s->printf("001 FAIL Unknown flag '%s'\n",flags);
			return false;
		}
		if(c) flags=p+1;
		else break;
	}
	if(!(uFlags&lfRead) && !(uFlags&lfWrite))
	{
		s->printf("001 FAIL Must specify read or write\n");
		return false;
	}

	if(strncmp(path,LockClientMap[client].root.c_str(),LockClientMap[client].root.size()))
	{
		DEBUG("(#%d) Lock Fail %s not within %s\n",(int)client,path,LockClientMap[client].root.c_str());
		s->printf("001 FAIL Lock not within repository\n");
		return false;
	}

	if(request_lock(client,path,uFlags,lock_to_wait_for))
	{
		VersionMapType ver;
		size_t newId = ++global_lockId;
		if(((uFlags&lfRead)) || (uFlags&lfWrite))
		{
			TransactionListType::const_iterator i = std::find(TransactionList.begin(), TransactionList.end(), path);
			if(i!=TransactionList.end())
			{
				/* This is where atomicity really 'happens' */
				if(i->owner!=client &&   /* Not us */
				   LockClientMap.find(i->owner)!=LockClientMap.end() && /* Exists */
				   TimeIntersects(LockClientMap[i->owner].starttime,LockClientMap[i->owner].endtime,
					   LockClientMap[client].starttime,LockClientMap[client].endtime)) /* Overlaps us */
				{
					printf("Found version %s:%s\n",i->branch.c_str(),i->oldversion.c_str());
					ver[i->branch]=i->oldversion;

					/* If a transaction is still in progress and a version has been committed,
						we can't allow even an advisory write yet. */
					if(uFlags&lfWrite)
					{
						DEBUG("(#%d) Lock request on %s (%s) (wait for transaction by client %d)\n",(int)client,path,FlagToString(uFlags),(unsigned)i->owner);
						s->printf("002 WAIT Lock busy|%s|%s|%s\n",LockClientMap[i->owner].user.c_str(),LockClientMap[i->owner].client_host.c_str(),path);
						return true;
					}
				}
			}
		}
		DEBUG("(#%d) Lock request on %s (%s) (granted %u)\n",(int)client,path,FlagToString(uFlags),(unsigned)newId);
		s->printf("000 OK Lock granted (%u)\n",(unsigned)newId);
		LockMap[newId].flags=uFlags;
		LockMap[newId].path=path;
		LockMap[newId].length=strlen(path);
		LockMap[newId].owner=client;
		LockMap[newId].versions = ver;
	}
	else
	{
		DEBUG("(#%d) Lock request on %s (%s) (wait for %u)\n",(int)client,path,FlagToString(uFlags),(unsigned)lock_to_wait_for);
		s->printf("002 WAIT Lock busy|%s|%s|%s\n",LockClientMap[LockMap[lock_to_wait_for].owner].user.c_str(),LockClientMap[LockMap[lock_to_wait_for].owner].client_host.c_str(),LockMap[lock_to_wait_for].path.c_str());
	}

	return true;
}

bool DoUnlock(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	size_t lockId;
	unsigned helper;

	if(LockClientMap[client].state!=lcActive)
	{
		s->printf("001 FAIL Unexpected 'Unlock' command\n");
		return false;
	}
	sscanf(param,"%u",&helper); /* 64 bit aware */
	lockId = helper;

	if(LockMap.find(lockId)==LockMap.end())
	{
		s->printf("001 FAIL Unknown lock id %u\n",(unsigned)lockId);
		return false;
	}
	if(LockMap[lockId].owner != client)
	{
		s->printf("001 FAIL Do not own lock id %u\n",(unsigned)lockId);
		return false;
	}

	LockMap.erase(LockMap.find(lockId));

	DEBUG("(#%d) Unlock request on lock %u\n",(int)client,(unsigned)lockId);

	s->printf("000 OK Unlocked\n");
	return true;
}

bool DoMonitor(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	if(LockClientMap[client].state!=lcLogin)
	{
		s->printf("001 FAIL Unexpected 'Monitor' command\n");
		return false;
	}
	LockClientMap[client].state=lcMonitor;
	s->printf("000 OK Entering monitor mode\n");
	return true;
}

bool DoClients(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	if(LockClientMap[client].state!=lcMonitor)
	{
		s->printf("001 FAIL Unexpected 'Clients' command\n");
		return false;
	}
	LockClientMapType::const_iterator i;
	for(i = LockClientMap.begin(); !(i==LockClientMap.end()); ++i)
	{
		s->printf("(#%d) %s|%s|%s|%s|%s\n",
			(int)i->first,
			i->second.server_host.c_str(),
			i->second.client_host.c_str(),
			i->second.user.c_str(),
			i->second.root.c_str(),
			StateString[i->second.state]);
	}
	s->printf("000 OK\n");
	return true;
}

bool DoLocks(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	if(LockClientMap[client].state!=lcMonitor)
	{
		s->printf("001 FAIL Unexpected 'Locks' command\n");
		return false;
	}
	for(LockMapType::const_iterator i = LockMap.begin(); i!=LockMap.end(); ++i)
		s->printf("(#%d) %s|%s (%u)\n",(int)i->second.owner,i->second.path.c_str(),FlagToString(i->second.flags), (unsigned)i->first);

	s->printf("000 OK\n");
	return true;
}

bool DoModified(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	char *id,*branch,*version,*oldversion;
	char type;
	size_t lockId;
	unsigned helper;

	if(LockClientMap[client].state!=lcActive)
	{
		s->printf("001 FAIL Unexpected 'Modified' command\n");
		return false;
	}
	id = strchr(param,'|');
	if(!id)
	{
		s->printf("001 FAIL Modified command expects <flags>|<lockId>|<branch>|<version>|oldversion\n");
		return false;
	}
	(*id++)='\0';
	sscanf(id,"%u",&helper);
	lockId = helper;
	branch = strchr(id,'|');
	if(!branch)
	{
		s->printf("001 FAIL Modified command expects <flags>|<lockId>|<branch>|<version>|oldversion\n");
		return false;
	}
	*(branch++)='\0';
	version = strchr(branch,'|');
	if(!version)
	{
		s->printf("001 FAIL Modified command expects <flags>|<lockId>|<branch>|<version>|oldversion\n");
		return false;
	}
	*(version++)='\0';
	oldversion = strchr(version,'|');
	if(strchr(oldversion+1,'|'))
	{
		s->printf("001 FAIL Modified command expects <flags>|<lockId>|<branch>|<version>|oldversion\n");
		return false;
	}
	if(oldversion)
	  *(oldversion++)='\0';
	if(!*param)
		type='M';
	else if(!strcmp(param,"Added"))
		type='A';
	else  if(!strcmp(param,"Deleted"))
		type='D';
	else
	{
		s->printf("001 FAIL Modified command expects <flags>|<lockId>|<branch>|<version>\n");
		return false;
	}

	if(LockMap.find(lockId)==LockMap.end())
	{
		s->printf("001 FAIL Unknown lock id %u\n",(unsigned)lockId);
		return false;
	}
	if(LockMap[lockId].owner != client)
	{
		s->printf("001 FAIL Do not own lock id %u\n",(unsigned)lockId);
		return false;
	}

	if(!(LockMap[lockId].flags&lfWrite))
	{
		s->printf("001 FAIL No write lock on file\n");
		return false;
	}

	DEBUG("(#%d) Modified request on lock %u (%s:%s [%c])\n",(int)client,(unsigned)lockId,branch,version,type);

	s->printf("000 OK\n");

	TransactionStruct t;
	t.owner=client;
	t.branch=branch;
	t.path=LockMap[lockId].path.c_str();
	t.version=version;
	t.oldversion=oldversion;
	t.type=type;

	TransactionList.push_back(t);

	return true;
}

bool DoVersion(CSocketIOPtr s,size_t client, char *param)
{
	ClientLock lock;
	char *branch;
	size_t lockId;
	unsigned helper;

	if(LockClientMap[client].state!=lcActive)
	{
		s->printf("001 FAIL Unexpected 'Version' command\n");
		return false;
	}
	branch = strchr(param,'|');
	if(!branch)
	{
		s->printf("001 FAIL Version command expects <lockid>|<branch>\n");
		return false;
	}
	(*branch++)='\0';
	
	sscanf(param,"%u",&helper); /* 64 bit aware */
	lockId = helper;

	if(LockMap.find(lockId)==LockMap.end())
	{
		s->printf("001 FAIL Unknown lock id %u\n",(unsigned)lockId);
		return false;
	}
	if(LockMap[lockId].owner != client)
	{
		s->printf("001 FAIL Do not own lock id %u\n",(unsigned)lockId);
		return false;
	}

	VersionMapType& ver = LockMap[lockId].versions;

	if(ver.find(branch)==ver.end())
	{ 
		s->printf("000 OK\n");
		DEBUG("(#%d) Version request on lock %u (%s)\n",(int)client,(unsigned)lockId,branch);
	}
	else
	{
		s->printf("000 OK (%s)\n",ver[branch].c_str());
		DEBUG("(#%d) Version request on lock %u (%s) returned %s\n",(int)client,(unsigned)lockId,branch,ver[branch].c_str());
	}

	return true;
}

bool DoClose(CSocketIOPtr s,size_t client, char *param)
{
	CloseLockClient(s);
	s->printf("000 OK\n");
	DEBUG("(#%d) Close request\n",(int)client);

	return true;
}

bool request_lock(size_t client, const char *path, unsigned flags, size_t& lock_to_wait_for)
{
	LockMapType::const_iterator i;
	size_t pathlen = strlen(path);
	for(i=LockMap.begin(); i!=LockMap.end(); ++i)
	{
		size_t locklen = i->second.length;

		if((locklen==pathlen && !strcmp(path,i->second.path.c_str())))
		{
			if(flags&lfWrite) 
			{
				/* Mixed Write lock only possible with the same client */
				if(i->second.owner!=client)
					break;
			}
			else /* read lock */
			{
				/* If there is a write lock on this object then fail */
				if((i->second.flags&lfWrite) && i->second.owner!=client)
					break;
			}
		}
	}
	if(i!=LockMap.end())
	{
		lock_to_wait_for = i->first;
		return false;
	}
	return true;
}

