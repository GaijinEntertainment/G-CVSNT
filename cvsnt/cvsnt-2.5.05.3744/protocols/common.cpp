/*	cvsnt auth common routines
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
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <lmcons.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#include "common.h"

#ifdef _WIN32
#define socket_errno WSAGetLastError()
#define sock_strerror gai_strerror
#else
#define closesocket close
#define socket_errno errno
#define sock_strerror strerror
#endif

#ifdef _WIN32
HMODULE g_hInst;

BOOL CALLBACK DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	g_hInst = hModule;
	return TRUE;
}

#include "proto_resource.h"
#endif

/*
** Translation Table as described in RFC1113
*/
static const unsigned char cb64[65]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const struct server_interface *_current_server;
static int tcp_fd;
static struct addrinfo *tcp_addrinfo, *tcp_active_addrinfo;

void set_current_server(const struct server_interface *_cs)
{
  _current_server = _cs;
}

const struct server_interface *current_server()
{
  return _current_server;
}

/*
** encodeblock
**
** encode 3 8-bit binary bytes as 4 '6-bit' characters
*/
static void encodeblock(const unsigned char *in, unsigned char *out, int len )
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=';
    out[3] = len > 2 ? cb64[ in[2] & 0x3f ] : '=';
}

int base64enc(const unsigned char *in, unsigned char *out, int len)
{
	int l=0;
	for(;len>0; len-=3)
	{
		encodeblock(in, out, len);
		in+=3;
		out+=4;
		l+=4;
	}
	*out='\0';
	return l;
}

int server_error(int fatal, const char *fmt, ...)
{
	char temp[1024];
	va_list va;

	va_start(va,fmt);

	vsnprintf(temp,sizeof(temp),fmt,va);

	va_end(va);

	return _current_server->error(_current_server, fatal, temp);
}

static int tcp_connect_direct(const cvsroot *cvsroot)
{
	const char *port;
	int sock;

	port = get_default_port(cvsroot);
	sock = tcp_connect_bind(cvsroot->hostname, port, 0, 0);

	if(sock<0)
		return sock;

	return 0;
}

static int tcp_connect_http(const cvsroot *cvsroot)
{
	int sock;
	const char *port;
	char line[1024],*p;
	int code;

	port = cvsroot->proxyport;
	if(!port)
		port="3128"; // Some people like 8080...  No real standard here

	if(!cvsroot->proxy)
		server_error(1,"Proxy name must be specified for HTTP tunnelling");
	
	sock = tcp_connect_bind(cvsroot->proxy, port, 0, 0);
	if(sock<0)
		return sock;

	port = get_default_port(cvsroot);

	if(cvsroot->proxyuser && cvsroot->proxyuser[0])
	{
		char enc[sizeof(line)];
		sprintf(line,"%s:%s",cvsroot->proxyuser,cvsroot->proxypassword?cvsroot->proxypassword:"");
		base64enc((const unsigned char *)line,(unsigned char *)enc,strlen(line));
		tcp_printf("CONNECT %s:%s HTTP/1.1\nProxy-Authorization: Basic %s\n\n",cvsroot->hostname,port,enc);
	}
	else
		tcp_printf("CONNECT %s:%s HTTP/1.0\n\n",cvsroot->hostname,port);
	tcp_readline(line, sizeof(line));
	
	p=strchr(line,' ');
	if(p) p++;
	code=p?atoi(p):0;
	if((code/100)!=2)
	{
		if(code==407)
		{
			if(cvsroot->proxyuser && cvsroot->proxyuser[0])
				server_error(1,"Proxy server authentication failed");
			else
				server_error(1,"Proxy server requires authentication");
		}
		else
			server_error(1,"Proxy server connect failed: ",p?p:"No response");
	}

	while(strlen(line)>1)
		tcp_readline(line, sizeof(line));

	return 0;
}

static int tcp_connect_socks5(const cvsroot *cvsroot)
{
	int socks_sock;
	const char *port;
	unsigned short u_port;
	unsigned char resp[1024];

	port = cvsroot->proxyport;
	if(!port)
		port="1080";

	if(!cvsroot->proxy)
		server_error(1,"Proxy name must be specified for SOCKS tunnelling");
	
	socks_sock = tcp_connect_bind(cvsroot->proxy, port, 0, 0);
	if(socks_sock<0)
		return socks_sock;

	port = get_default_port(cvsroot);
	u_port = atoi(port);

	if(cvsroot->proxyuser && cvsroot->proxyuser[0])
	{
		unsigned char init[4]= { 5, 2, 2, 0 };
		tcp_write(init,sizeof(init));
	}
	else
	{
		unsigned char init[3] = { 5, 1, 0 };
		tcp_write(init,sizeof(init));
	}
	if(tcp_read(resp,2)!=2)
		server_error(1,"Unable to communicate with SOCKS server");

	if(resp[1]==0xFF)
	{
		server_error(1,"Socks server refused to talk to us");
	}
	if(resp[1]==2)
	{
		unsigned char auth[1024];
		int l = 0, s;
		auth[l++]=1;
		s=cvsroot->proxyuser?strlen(cvsroot->proxyuser):0;
		if(s>255) s=255;
		auth[l++]=(unsigned char)s;
		if(s)
			memcpy(&auth[l],cvsroot->proxyuser,s);
		l+=s;
		s=cvsroot->proxypassword?strlen(cvsroot->proxypassword):0;
		if(s>255) s=255;
		auth[l++]=(unsigned char)s;
		if(s)
			memcpy(&auth[l],cvsroot->proxypassword,s);
		l+=s;
		tcp_write(auth,l);
		if(tcp_read(resp,2)!=2)
			server_error(1,"Unable to communicate with SOCKS server");
		if(resp[1])
		{
			server_error(1,"Socks server rejected authentication: bad username/password?");
		}
	}

	unsigned char request[1024];
	int l=0,s;
	request[l++]=5;
	request[l++]=1;
	request[l++]=0;
	request[l++]=3;
	s=strlen(cvsroot->hostname);
	if(s>255) s=255;
	request[l++]=s;
	if(s)
		memcpy(&request[l],cvsroot->hostname,s);
	l+=s;
	*(unsigned short *)&request[l]=htons(u_port);
	l+=2;

	tcp_write(request,l);
	if(tcp_read(resp,4)!=4)
		server_error(1,"Unable to communicate with SOCKS server");
	switch(resp[1])
	{
	case 0:
		break;
	case 1:
		server_error(1,"General SOCKS server failure");
		break;
	case 2:
		server_error(1,"SOCKS error: Connection not allowed by ruleset");
		break;
	case 3:
		server_error(1,"SOCKS error: Remote Network unreachable");
		break;
	case 4:
		server_error(1,"SOCKS error: Remote Host unreachable");
		break;
	case 5:
		server_error(1,"SOCKS error: Connection refused by remote host");
		break;
	case 6:
		server_error(1,"SOCKS error: TTL expired");
		break;
	case 7:
		server_error(1,"SOCKS error: Command not supported");
		break;
	case 8:
		server_error(1,"SOCKS error: Address type not supported");
		break;
	default:
		server_error(1,"SOCKS error: unknown error %02x",resp[1]);
		break;
	}

	char addr[300];
	switch(resp[3])
	{
	case 0: // IPV4
	case 1: // IPV4, sent by dante (broken??)
		tcp_read(&resp[4],6);
		snprintf(addr,sizeof(addr),"%d.%d.%d.%d",resp[4],resp[5],resp[6],resp[7]);
		u_port=ntohs(*(unsigned short *)&resp[8]);
		break;
	case 3: // Domain name
		tcp_read(&resp[4],1);
		if(resp[4])
		{
			tcp_read(&resp[5],resp[4]+2);
			memcpy(addr,&resp[5],resp[4]);
			addr[resp[4]]='\0';
		}
		else
		{
			addr[0]='\0';
		}
		u_port=ntohs(*(unsigned short *)&resp[resp[4]+5]);
		break;
	case 4: // IPV6
		tcp_read(&resp[4],18);
		snprintf(addr,sizeof(addr),"%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x",resp[4],resp[5],resp[6],resp[7],resp[8],resp[9],resp[10],resp[11],resp[12],resp[13],resp[14],resp[15],resp[16],resp[17],resp[18],resp[19]);
		u_port=ntohs(*(unsigned short *)&resp[20]);
		break;
	default:
		server_error(1,"Unknown address type (%02x) sent by socks server",resp[3]);
		break;
	}

	return 0;
}

static int tcp_connect_socks4(const cvsroot *cvsroot)
{
	int socks_sock;
	const char *port;
	unsigned short u_port;
	unsigned char resp[1024];

	port = cvsroot->proxyport;
	if(!port)
		port="1080";

	if(!cvsroot->proxy)
		server_error(1,"Proxy name must be specified for SOCKS tunnelling");

	if(cvsroot->proxypassword)
		server_error(1,"SOCKS4 does not support password authentication");
    	
	socks_sock = tcp_connect_bind(cvsroot->proxy, port, 0, 0);
	if(socks_sock<0)
		return socks_sock;

	port = get_default_port(cvsroot);
	u_port = atoi(port);

	resp[0]=4;
	resp[1]=1;
	*(unsigned short *)&resp[2]=htons(u_port);

	struct addrinfo hint = {0};
	int res;
	hint.ai_protocol=IPPROTO_TCP;
	hint.ai_socktype=SOCK_STREAM;
	if((res=getaddrinfo(cvs::idn(cvsroot->hostname),port,&hint,&tcp_addrinfo))!=0)
	{
		server_error (1,"Error connecting to host %s: %s\n", cvsroot->hostname, gai_strerror(socket_errno));
		return -1;
	}

	sockaddr_in* sin = (sockaddr_in*)tcp_addrinfo->ai_addr;
	memcpy(&resp[4],&sin->sin_addr.s_addr,4);

	freeaddrinfo(tcp_addrinfo);

	int l = 8;
	if(cvsroot->proxyuser)
	{
		l+=strlen(cvsroot->proxyuser);
		strcpy((char*)&resp[8],cvsroot->proxyuser);
	}
	else
	{
		const char *u = get_username(cvsroot);
		l+=strlen(u);
		strcpy((char*)&resp[8],u);
	}
	l++;

	tcp_write(resp,l);
	if(tcp_read(resp,8)!=8)
		server_error(1,"Unable to communicate with SOCKS server");

	switch(resp[1])
	{
	case 90:
		break;
	case 91:
		server_error(1,"SOCKS4 request failed");
		break;
	case 92:
		server_error(1,"SOCKS4 ident lookup failed");
		break;
	case 93:
		server_error(1,"SOCKS4 ident reports different identities");
		break;
	default:
		server_error(1,"SOCKS4 error %02x",resp[1]);
	}

	return 0;
}

int tcp_connect(const cvsroot *cvsroot)
{
	const char *protocol = cvsroot->proxyprotocol;
	if(!protocol && cvsroot->proxy)
		protocol = "HTTP"; // Compatibility case, where proxy is specified but no tunnelling protocol

	if(!protocol)
		return tcp_connect_direct(cvsroot);
	if(!strcasecmp(protocol,"HTTP"))
		return tcp_connect_http(cvsroot);
	if(!strcasecmp(protocol,"SOCKS5"))
		return tcp_connect_socks5(cvsroot);
	if(!strcasecmp(protocol,"SOCKS"))
		return tcp_connect_socks5(cvsroot);
	if(!strcasecmp(protocol,"SOCKS4"))
		return tcp_connect_socks4(cvsroot);
	tcp_fd = -1;
	server_error(1, "Unsupported tunnelling protocol '%s' specified",protocol);
	return -1;
}

int tcp_connect_bind(const char *servername, const char *remote_port, int min_local_port, int max_local_port)
{
	struct addrinfo hint = {0}, *localinfo;
	int res,sock,localport,last_error;

	hint.ai_socktype=SOCK_STREAM;
	if((res=getaddrinfo(cvs::idn(servername),remote_port,&hint,&tcp_addrinfo))!=0)
	{
		server_error (1,"Error connecting to host %s: %s\n", servername, gai_strerror(socket_errno));
		return -1;
	}

	for(tcp_active_addrinfo = tcp_addrinfo; tcp_active_addrinfo; tcp_active_addrinfo=tcp_active_addrinfo->ai_next)
	{
#ifdef _WIN32
win32_port_reuse_hack:
#endif

		sock = socket(tcp_active_addrinfo->ai_family, tcp_active_addrinfo->ai_socktype, tcp_active_addrinfo->ai_protocol);
		if (sock == -1)
			server_error (1, "cannot create socket: %s", sock_strerror(socket_errno));

		if(min_local_port || max_local_port)
		{
			for(localport=min_local_port; localport<max_local_port; localport++)
			{
				char lport[32];
				snprintf(lport,sizeof(lport),"%d",localport);
				hint.ai_flags=AI_PASSIVE;
				hint.ai_protocol=tcp_active_addrinfo->ai_protocol;
				hint.ai_socktype=tcp_active_addrinfo->ai_socktype;
				hint.ai_family=tcp_active_addrinfo->ai_family;
				localinfo=NULL;
				if((res=getaddrinfo(NULL,lport,&hint,&localinfo))!=0)
				{
					server_error (1,"Error connecting to host %s: %s\n", servername, gai_strerror(socket_errno));
					return -1;
				}
				if(bind(sock, (struct sockaddr *)localinfo->ai_addr, localinfo->ai_addrlen) ==0)
					break;
				freeaddrinfo(localinfo);
			}
			freeaddrinfo(localinfo);
			if(localport==max_local_port)
				server_error (1, "Couldn't bind to local port - %s",sock_strerror(socket_errno));
		}

		if(connect (sock, (struct sockaddr *) tcp_active_addrinfo->ai_addr, tcp_active_addrinfo->ai_addrlen)==0)
			break;
		last_error = socket_errno;
		closesocket(sock);
#ifdef _WIN32
		if(last_error == 10048 && (min_local_port || max_local_port))
			// Win32 bug - should have been caught by 'bind' above, but instead gets caught by 'connect'
		{
			min_local_port = localport+1;
			goto win32_port_reuse_hack;
		}
#endif
	}
	if(!tcp_active_addrinfo)
		server_error (1, "connect to %s:%s failed: %s", servername, remote_port, sock_strerror(last_error));

	tcp_fd = sock;

/*	{
	int v=1;
	setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(const char *)&v,sizeof(v));
	} */

	return sock;
}

int tcp_disconnect()
{
	if (tcp_fd != -1)
	{
		if(closesocket(tcp_fd))
			return -1;
	    tcp_fd = -1;
		freeaddrinfo(tcp_addrinfo);
	}
	return 0;
}

enum {TIMEOUT_SECONDS = 10*60};//10 minutes is big enough timeout
template <typename T> inline intptr_t handle_EINTR(T fn) {
  intptr_t res = false;
  while (true) {
    res = fn();
    if (res < 0 && errno == EINTR) { continue; }
    break;
  }
  return res;
}

inline int select_read(SOCKET  sock, time_t sec, time_t usec) {
#ifndef _WIN32
  if (sock >= FD_SETSIZE) { return 1; }
#endif

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  timeval tv;
  tv.tv_sec = static_cast<long>(sec);
  tv.tv_usec = static_cast<decltype(tv.tv_usec)>(usec);

  return handle_EINTR([&]() {
    return select(static_cast<int>(sock + 1), &fds, nullptr, nullptr, &tv);
  });
}

inline int select_write(SOCKET sock, time_t sec, time_t usec) {
#ifndef _WIN32
  if (sock >= FD_SETSIZE) { return 1; }
#endif

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  timeval tv;
  tv.tv_sec = static_cast<long>(sec);
  tv.tv_usec = static_cast<decltype(tv.tv_usec)>(usec);

  return handle_EINTR([&]() {
    return select(static_cast<int>(sock + 1), nullptr, &fds, nullptr, &tv);
  });
}

int tcp_read(void *data, int length)
{
	if(!tcp_fd) /* Using stdin/out, probably */
		return read(_current_server->in_fd,data,length);
	else
	{
  		int err = select_read(tcp_fd, TIMEOUT_SECONDS, 0);
		if (err > 0)
			err = recv(tcp_fd, (char*)data, length, 0);
#ifdef _WIN32
		if(err<0)
			errno = -socket_errno;
#endif
		return err;
	}
}

int tcp_write(const void *data, int length)
{
	if(!tcp_fd) /* Using stdin/out, probably */
		return write(_current_server->out_fd,data,length);
	else
	{
  		int err = select_write(tcp_fd, TIMEOUT_SECONDS, 0);
		if (err > 0)
			err = send(tcp_fd, (const char*)data, length, 0);
#ifdef _WIN32
		if(err<0)
			errno = -socket_errno;
#endif
		return err;
	}
}

int tcp_shutdown()
{
	if(!tcp_fd)
		return 0;
	shutdown(tcp_fd,0);
	return 0;
}

int tcp_printf(const char *fmt, ...)
{
	char temp[1024];
	va_list va;

	va_start(va,fmt);

	vsnprintf(temp,sizeof(temp),fmt,va);

	va_end(va);

	return tcp_write(temp,strlen(temp));
}

int tcp_readline(char* buffer, int buffer_len)
{
	char *p,c;
	int l;

	l=0;
	p=buffer;
	while(l<buffer_len-1 && tcp_read(&c,1)>0)
	{
		if(c=='\n')
			break;
		*(p++)=(char)c;
		l++;
	}
	*p='\0';
	return l;
}

int server_getc()
{
	char c;

	if(read(_current_server->in_fd,&c,1)<1)
		return EOF;
	return c;
}

int server_getline(char** buffer, int buffer_max)
{
	char *p;
	int l,c=0;

	*buffer=(char*)malloc(buffer_max);
	if(!*buffer)
		return -1;

	l=0;
	p=*buffer;
	*p='\0';
	while(l<buffer_max-1 && (c=server_getc())!=EOF)
	{
		if(c=='\n')
			break;
		*(p++)=(char)c;
		l++;
	}
	if(l==0 && c==EOF)
		return -1; /* EOF */
	*p='\0';
	return l;
	
}

#ifdef _WIN32
const char* get_username(const cvsroot* current_root)
{
    const char* username;
    static char username_buffer[UNLEN+1];
    DWORD buffer_length=UNLEN+1;
    username=current_root->username;
    if(!username)
    {
		if(GetUserNameA(username_buffer,&buffer_length))
		{
			username=username_buffer;
		}
    }
    return username;
}
#else
const char* get_username(const cvsroot* current_root)
{
    const char* username;
    username=current_root->username;
    if(!username)
    {
		username = getpwuid(getuid())->pw_name;
    }
    return username;
}
#endif

const char *get_default_port(const cvsroot *root)
{
	struct servent *ent;
	static char p[32];
	const char *env;

	if(root->port)
		return root->port;

	if((env=CProtocolLibrary::GetEnvironment("CVS_CLIENT_PORT"))!=NULL)
		return env;

	if((ent=getservbyname("cvspserver","tcp"))!=NULL)
	{
		sprintf(p,"%u",ntohs(ent->s_port));
		return p;
	}

	return "2401";
}

#ifdef _WIN32
static INT_PTR CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char value[MAX_PATH];
	int nSel;
	struct plugin_interface *ui = (struct plugin_interface*)GetWindowLongPtr(hWnd,GWLP_USERDATA);

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLongPtr(hWnd,GWLP_USERDATA,lParam);
			ui = (struct plugin_interface*)lParam;
			SetWindowText(hWnd,ui->description);
			nSel = 1;
			if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins",ui->key,value,sizeof(value)))
				nSel = atoi(value);
			SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,nSel,0);
		}
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","Plugins",ui->key,value);
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
	int ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, ConfigDlgProc,(LPARAM)ui);
	return ret==IDOK?0:-1;
}
#endif
