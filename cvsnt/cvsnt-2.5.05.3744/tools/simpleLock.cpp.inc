#include "simpleOsWrap.cpp.inc"
#include "../lockservice/lockservice_uds_name.h"
#define L_OK		0		/* success */
#define L_ERROR		1		/* error condition */
#define L_LOCKED	2		/* lock owned by someone else */


static int lock_server_command(int sock, char *line, int line_len, const char *cmd, ...);

int cvs_tcp_connect(const char *servername, const char *port, int supress_errors);
int cvs_tcp_close(int sock);
const char *fn_root(const char*s){return s;}

static int lock_register_client(const char *username, const char *root, const char *lock_server_)
{
  char line[1024];

  int lock_server_socket = -1;
  char *lock_server = strdup(lock_server_);
  const char *serv;
  char *_root = strdup(root);
  const char *roo = _root;

  /* First time - connect to lock server & register ourselves */
  char *p=strchr(lock_server,':');
  int l;
  if(p) *(p++)='\0';
  if(!strcmp(lock_server,"localhost"))
  	serv = "127.0.0.1"; /* XP Home breakage */
  else
  	serv = lock_server;
  #ifndef _WIN32
  if (strcmp(serv, "127.0.0.1") == 0)// on linux we connect to UDS instead. that's way faster
    serv = uds_lock_socket_name;
  #endif
  TRACE(3,"Lock server connect to %s port %s",PATCH_NULL(serv),p?p:"2402");

  if((lock_server_socket=cvs_tcp_connect(serv,p?p:"2402", 0))<0)
  	error(1,0,"Couldn't connect to lock server");
  if(p)
  	*(--p)=':';
  if((l=recv(lock_server_socket,line,sizeof(line),0))<=0)
  {
  	error(1,errno,"Error communicating with lock server");
  }
  do
  {
  	line[l--]='\0';
  } while(l && isspace(line[l]));
  // Line now contains signon string, including protocol version. 

  {
  	int ver1,ver2;
  	if(!strchr(line,' ') || sscanf(strchr(line,' ')+1,"%d.%d",&ver1,&ver2)!=2)
  	{
  		error(1,0,"Lockserver returned invalid version string: %s",line+4);
  	}
  	if(ver1<2 || ver2<2)
  	{
  		error(1,0,"Invalid Lockserver version - got %d.%d, wanted 2.2",ver1,ver2);
  	}
  }

  switch(lock_server_command(lock_server_socket, line,sizeof(line),"Client %s|%s|%s\n",username,roo,""))
  {
  case 0:
  	break;
  default:
  	error(1,0,"Couldn't authenticate with lock server: %s",line+4);
  }

  free(_root);
  free(lock_server);
 return lock_server_socket;
}

static int lock_server_command(int lock_server_socket, char *line, int line_len, const char *cmd, ...)
{
	int l;
	va_list va;

	va_start(va,cmd);
	vsnprintf(line,line_len,cmd,va);
	va_end(va);
	if(send(lock_server_socket,line,strlen(line),0)<=0)
	{
		error(1,errno,"Error communicating with lock server (send)");
	}
	if((l=recv(lock_server_socket,line,line_len,0))<=0)
	{
		error(1,errno,"Error communicating with lock server (recv)");
	}
	do
	{
		line[l--]='\0';
	} while(l && isspace(line[l]));
	if(!isdigit(line[0]) || !isdigit(line[1]) || !isdigit(line[2]) || line[3]!=' ')
		return 99; // Unknown
	return atoi(line);
}

/* Connect to the lock server and attain a lock */
static size_t do_lock_server(int lock_server_socket, const char *object, const char *flags, int wait)
{
	char line[65536],*p,*q, *ob = NULL;
	int bWaited;
	size_t id;
	unsigned helper;

	bWaited = 0;
	do
	{
		time_t now;
		char tm[32], time_cnv[32];
	    time (&now);
        CTIME_S(time_cnv, sizeof(time_cnv), &now);
		strncpy(tm,time_cnv+11,sizeof(tm));
		tm[8]='\0';
		switch(lock_server_command(lock_server_socket, line,sizeof(line),"Lock %s|%s\n",flags,object))
		{
		case 0:
		case 3:
			if(bWaited)
				error(0,0,"[%s] obtained lock in %s", tm, fn_root(object));
			p=strchr(line,'(');
			if(!p)
				error(1,0,"Lockserver did not return a lock ID.  Check version.");
			p++;
			q=strchr(p,')');
			if(!q)
				error(1,0,"Lockserver did not return a lock ID.  Check version.");
			*q='\0';
			sscanf(p,"%u",&helper);
			id = helper; /* 64 bit aware */
			free(ob);
			return id;
		case 1:
			error(1,0,"Failed to obtain lock on %s: %s",fn_root(object),line+4);
		case 2:
			if(!wait) /* In the case of no wait, return immediately */
			{
				free(ob);
				return 0;
			}
			else
			{
				char *owner, *host, *path;
				owner = strchr(line,'|');
				if(owner)
				{
					*(owner++)='\0';
					host = strchr(owner,'|');
				}
				else
					host = NULL;
				if(host)
				{
					*(host++)='\0';
					path = strchr(host,'|');
				}
				else
					path = NULL;

				if(path)
					*(path++)='\0';

				if(!(bWaited%5)) // No need to keep going on about it..
				{
					if(host && *host)
						error(0,0,"[%s] waiting for %s on %s's lock in %s", tm, owner, host, fn_root(path));
					else
						error(0,0,"[%s] waiting for %s's lock in %s", tm, owner, fn_root(path));
				}
				bWaited++;
				if(bWaited==20)
					error(1,0,"Failed to obtain lock on %s",fn_root(object));
			}
			break;
		default:
			error(1,0,"Unknown response from lock server: %s", line+4);
		}
		// Only get here in case 2 == WAIT
		sleep_ms(1);
	} while(1);
	free(ob);
	return 0;
}

size_t do_lock_file(int lock_server_socket, const char *file_with_dir, int write, int wait)
{
  return do_lock_server(lock_server_socket, file_with_dir,write?"Write":"Read", wait);
}

/* Cleanup lock(s) */
int do_unlock_file(int lock_server_socket, size_t lockId)
{
 	char line[1024];
 	switch(lock_server_command(lock_server_socket, line,sizeof(line),"Unlock %u\n",lockId))
 	{
 	case 0:
 		break;
 	case 1:
 		return -1;
 	default:
 		error(1,0,"Unknown response from lock server: %s", line+4);
 	}
	return 0;
}

static void print_status(int lock_server_socket)
{
	char line[65536];
	size_t ret;
	unsigned helper;

	if ((ret = lock_server_command(lock_server_socket, line,sizeof(line),"Monitor\n")) != 0)
		error(1,0,"Failed to set monitor on: %lld %s",ret,line);

    if ((ret = lock_server_command(lock_server_socket, line,sizeof(line),"Clients\n")) != 0)
		error(1,0,"Failed to get clients on: %lld %s",ret,line);
    printf("clients:\n%s\n", line);

    if ((ret = lock_server_command(lock_server_socket, line,sizeof(line),"Locks\n")) != 0)
		error(1,0,"Failed to get locks on: %lld %s",ret,line);
    printf("locks:\n%s\n", line);
}
