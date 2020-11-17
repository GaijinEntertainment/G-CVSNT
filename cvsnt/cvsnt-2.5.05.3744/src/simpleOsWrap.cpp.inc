#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sched.h>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif
#ifdef _WIN32
#define socket_errno WSAGetLastError()
#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#define sock_strerror gai_strerrorA
#undef gai_strerror
#define gai_strerror gai_strerrorA
#else
#define closesocket close
#define socket_errno errno
#define sock_strerror strerror
#endif

#include <stdarg.h>
#include <time.h>

#if _WIN32
  #include <windows.h>
  #define strdup _strdup
  #define sscanf sscanf_s
  #define strncpy strncpy_s
  #define CTIME_S CTIME_S
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #define CTIME_S(a,s,n) ctime_r(n, a);
#endif

void sleep(int ms)
{
#if _WIN32
  if (ms == 0)
    SwitchToThread(); // Sleep(0) doesn't give up cpu to lower priority threads
  else
    SleepEx(ms, TRUE);
#elif defined(__GNUC__)
  if (ms == 0)
    sched_yield();
  else
  {
    struct timespec req, rem;
    req.tv_sec = ms / 1000;
    req.tv_nsec = ms ? ((ms % 1000) * 1000000) : 30000;
    while (1)
    {
      if (nanosleep(&req, &rem) < 0 && errno == EINTR)
        req = rem; // sleep was interrupted by signal, continue sleeping
      else
        break;
    }
  }
#endif
}


#define TRACE(...)
static void error(int a, int b, const char *cmd, ...)
{
  char line[1024];int line_len = sizeof(line);
  int l;
  va_list va;

  va_start(va,cmd);
  vsnprintf(line,line_len,cmd,va);
  va_end(va);
  printf("error <%s>\n", line);
  if (a)
    exit(1);
}

const char *idn(const char*a) {return a;}
const char *decode_idn(const char*a) {return a;}

void tcp_init()
{
#ifdef _WIN32
  WSADATA data;
  if(WSAStartup (MAKEWORD (1, 1), &data))
  {
    printf("can't init socket!\n");
  	exit(1);
  }
#endif
}
int cvs_tcp_connect(const char *servername, const char *port, int supress_errors)
{
	struct addrinfo hint = {0};
	struct addrinfo *tcp_addrinfo;
	int res,sock;
#ifdef _WIN32
	u_long b;
#else
	size_t b;
#endif
	int err;

	hint.ai_flags=supress_errors?0:AI_CANONNAME;
	hint.ai_socktype=SOCK_STREAM;
	if((res=getaddrinfo(idn(servername),port,&hint,&tcp_addrinfo))!=0)
	{
		if(!supress_errors)
			error(0,0, "Error connecting to host %s: %s\n", servername, gai_strerror(socket_errno));
		return -1;
	}

    sock = socket(tcp_addrinfo->ai_family, tcp_addrinfo->ai_socktype, tcp_addrinfo->ai_protocol);
    if (sock == -1)
	{
		if(!supress_errors)
			error(0,0, "cannot create socket: %s", sock_strerror(socket_errno));
		return -1;
	}

	if(supress_errors)
	{
#ifdef _WIN32
		b=1;
		ioctlsocket(sock,FIONBIO,&b);
#else	
		b=fcntl(sock,F_GETFL,0);
		fcntl(sock,F_SETFL,b|O_NONBLOCK);
#endif
	}

	/* If errors are supressed we use a nonblocking connect with a 1000us select... this is enough
	   for a connect to localhost (used by the agent code) and isn't going to be noticed by the user */
	if(connect (sock, (struct sockaddr *) tcp_addrinfo->ai_addr, tcp_addrinfo->ai_addrlen) <0)
	{
		err = socket_errno;
		if(err==EWOULDBLOCK)
		{
			fd_set fds;
			struct timeval tv = { 15000,0 };
			FD_ZERO(&fds);
			FD_SET(sock,&fds);
			err = select(sock,NULL,&fds,NULL,&tv);
			if(err!=1)
			{
				if(!supress_errors)
					error(0,0, "connect to %s(%s):%s failed: %s", servername, (const char *)decode_idn(tcp_addrinfo->ai_canonname), port, sock_strerror(socket_errno));
				closesocket(sock);
				return -1;
			}
		}
		else
		{
			if(!supress_errors)
				error(0,0, "connect to %s(%s):%s failed: %s", servername, (const char *)decode_idn(tcp_addrinfo->ai_canonname), port, sock_strerror(socket_errno));
			closesocket(sock);
			return -1;
		}
	}

	if(supress_errors)
	{
#ifdef _WIN32
		b=0;
		ioctlsocket(sock,FIONBIO,&b);
#else	
		b=fcntl(sock,F_GETFL,0);
		fcntl(sock,F_SETFL,b&~O_NONBLOCK);
#endif
	}

	freeaddrinfo(tcp_addrinfo);

	/* Disable socket send delay.  Since this is mostly used for cvslock over loopback this make sense */
	{
	int v=1;
	setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(const char *)&v,sizeof(v));
	} 

	return sock;
}

int cvs_tcp_close(int sock)
{
	return closesocket(sock);
}
