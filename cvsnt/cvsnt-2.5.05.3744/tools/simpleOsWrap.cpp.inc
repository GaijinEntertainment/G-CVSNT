#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
//#define _CRT_NONSTDC_DEPRECATE
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <sys/stat.h>
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
  #define CTIME_S ctime_s
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #define CTIME_S(a,s,n) ctime_r(n, a);
#endif
#define xmalloc malloc
#define xfree free
#undef max
#undef min

void sleep_ms(int ms)
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
void error(int a, int b, const char *cmd, ...)
{
  char line[1024];int line_len = sizeof(line);
  int l;
  va_list va;

  va_start(va,cmd);
  vsnprintf(line,line_len,cmd,va);
  va_end(va);
  printf("error %d, <%s>\n", b, line);
  if (a)
    exit(1);
}

#if _WIN32
size_t get_file_size(const char *file)
{
  struct __stat64 buf;
  if (_stat64(file, &buf) != 0)
      return 0; // error, could use errno to find out more

  return buf.st_size;
}
int isreadable (const char *file)
{
  DWORD fa = GetFileAttributes(file);
  if (fa == INVALID_FILE_ATTRIBUTES)
	return 0;
  return 1;  
}
int iswriteable (const char *file)
{
  DWORD fa = GetFileAttributes(file);
  if (fa == INVALID_FILE_ATTRIBUTES)
	return 0;
  if(fa&FILE_ATTRIBUTE_DIRECTORY)
  	return 1;
  if (FILE_ATTRIBUTE_READONLY)
    return 0;
  return 1;
}
inline int mkdir (const char *path, int mode)
{
  return CreateDirectory(path,NULL);
}
inline void wnt_get_temp_dir(char *tempdir, int tempdir_size)
{
  DWORD fa;

  if(!(GetEnvironmentVariableA("TEMP",tempdir,tempdir_size) ||
       GetEnvironmentVariableA("TMP",tempdir,tempdir_size)))
  {
  	// No TEMP or TMP, use default <windir>\TEMP
  	GetWindowsDirectoryA(tempdir,tempdir_size);
  	strcat(tempdir,"\\TEMP");
  }
  if((fa=GetFileAttributesA(tempdir))==0xFFFFFFFF || !(fa&FILE_ATTRIBUTE_DIRECTORY))
  {
  	// Last resort, can't find a valid temp.... use C:\...
  	strcpy(tempdir,"C:\\");
  }
}

FILE *cvs_temp_file (char **filename, const char *mode)
{
  char tempdir[MAX_PATH];
  FILE *f;
  mode = mode ? mode : "wb+";
  wnt_get_temp_dir(tempdir,sizeof(tempdir));
  *filename=(char*)xmalloc(MAX_PATH);
  GetTempFileNameA(tempdir,"cvs",0,*filename);
  f=fopen(*filename, mode);
  return f;
}
inline int unlink_file (const char *f) { return _unlink(f); }

#else
size_t get_file_size(const char *file)
{
  struct stat sb;
  if (stat(file, &sb) == -1)
    return 0;
  return sb.st_size;
}
int isreadable (const char *file)
{
  return access(file, R_OK) == 0;
}
int iswriteable (const char *file)
{
  return access(file, W_OK) == 0;
}
static const char *def_tmp_dir = nullptr;
#define Tmpdir def_tmp_dir
static void init_temp_dir()
{
  if (def_tmp_dir)
    return;
  const char *tmp = getenv("CVS_TMP");
  if (tmp)
    def_tmp_dir = tmp;
  else
  {
    const char *tmp = getenv("TMPDIR");
    def_tmp_dir = tmp ? strdup(tmp) : "/tmp";
  }
}

FILE *cvs_temp_file (char **filename, const char *mode)
{
  init_temp_dir();
  char *fn;
  FILE *fp;

  mode = mode ? mode : "w+";
  {
  int fd;

  fn = (char*)xmalloc (strlen (Tmpdir) + 11);
  sprintf (fn, "%s/%s", Tmpdir, "cvsXXXXXX" );
  fd = mkstemp (fn);

  /* a NULL return will be interpreted by callers as an error and
   * errno should still be set
   */
  if (fd == -1) fp = NULL;
  else if ((fp = fdopen (fd, mode)) == NULL)
  {
  /* attempt to close and unlink the file since mkstemp returned sucessfully and
   * we believe it's been created and opened
   */
   int save_errno = errno;
  if (close (fd))
      error (0, errno, "Failed to close temporary file %s", fn);
  if (unlink (fn))
      error (0, errno, "Failed to unlink temporary file %s", fn);
  errno = save_errno;
  }

  }

  *filename = fn;
  return fp;
}
inline int unlink_file (const char *f) { return unlink(f); }
#endif

bool change_file_mode(const char *f, int mode)
{
  return chmod(f, mode&0777) >= 0;
}

bool get_file_mode_and_size(const char *file, mode_t &mode, size_t &sz)
{
  struct stat sb;
  if (stat(file, &sb) == -1)
    return false;
  mode = sb.mode;
  sz = sb.st_size;
  return true;
}

#ifndef CVS_MKDIR
#define CVS_MKDIR mkdir
#endif

void blob_free(void *p) {xfree(p);}
void *blob_alloc(size_t sz) {return xmalloc(sz);}
int blob_mkdir (const char *path, int mode){return CVS_MKDIR(path, mode);}

bool rename_file (const char *from, const char *to, bool fail_on_error)
{
  if (rename (from, to) < 0)
  {
    error (fail_on_error ? 1 : 0, errno, "cannot rename file %s to %s", from, to);
    return false;
  }
  return true;
}

void rename_file (const char *from, const char *to)
{
  rename_file(from, to, true);
}

time_t get_file_mtime(const char *file)
{
  struct stat sb;

  if (stat (file, &sb) < 0)
    return 0;
  return sb.st_mtime;
}

time_t get_file_atime(const char *file)
{
  struct stat sb;

  if (stat (file, &sb) < 0)
    return 0;
  return sb.st_atime;
}

#if _MSC_VER
 #include <sys/utime.h>
#else
 #include <sys/types.h>
 #include <utime.h>
#endif

void set_file_mtime(const char *filename, time_t mtime)
{
  struct utimbuf t;
  memset (&t, 0, sizeof (t));
  t.modtime = t.actime = mtime;
  utime (filename, &t);
}

inline const char *idn(const char*a) {return a;}
inline const char *decode_idn(const char*a) {return a;}

inline void tcp_init()
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
inline int cvs_tcp_connect(const char *servername, const char *port, int supress_errors)
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

inline int cvs_tcp_close(int sock)
{
	return closesocket(sock);
}
uint64_t get_user_session_salt(){return 14695981039346656037LU;}//link time resolved dependency
bool rename_attempts(const char *from, const char *to, unsigned attempts)
{
  unsigned i = 0;
  for (i = 0; i < attempts; ++i)
  {
    if (rename (from, to) >= 0)
      break;
    printf("can't rename file <%s> to <%s> %d/%d attempt\n", from, to, i, attempts);
    sleep_ms(1000);
  }
  return i < attempts;
}
