#include <config.h>
#include "daemon.h"
#include "getopt1.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h> // HPUX
#include <vector>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <cvsapi.h>
#include <cvstools.h>

#define SOCKET int
#define DEFAULT_PORT 2402

#include "LockService.h"

#define CVSLIB_ENV	"CVSLIB"	/* Location of protocol libraries */

bool g_bTestMode = false;
int listen_port = DEFAULT_PORT;
int local_port = 1;

static void usage(const char *prog)
{
  fprintf(stderr,"Usage: %s [-d] [-l] [-g] [-p port]\n",prog);
  exit(-1);
}

int main(int argc, char *argv[])
{
  int c;
  int digit_optind = 0;
  char buf[32];
	char*		cp;

	if ((cp = getenv (CVSLIB_ENV)) != NULL)
	{
		CGlobalSettings::SetLibraryDirectory(cp);
	}
	
  if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer", "LockServer", buf, sizeof(buf)))
  {
    char *p=strchr(buf,':');
    if(p)
      listen_port=atoi(p+1);
    if(!listen_port) listen_port=DEFAULT_PORT;
  }
  if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer", "LockServerLocal", buf, sizeof(buf)))
    local_port=atoi(buf);
  
  while(1)
  {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] =
    {
      {0,0,0,0}
    };

    c = getopt_long (argc,argv,"+dp:lg",long_options, &option_index);
    if (c == -1)
      break;

    switch(c)
    {
      case 'd':
        g_bTestMode = true;
		CServerIo::loglevel(3);
        break;
      case 'p':
        listen_port = atoi(optarg);
        break;
      case 'l':
	local_port = 1;
	break;
      case 'g':
	local_port = 0;
	break;
      default:
        usage(argv[0]);
        break;
    }
  }
  if(optind < argc)
    usage(argv[0]);

  if(!g_bTestMode)
  {
    if(daemon(0,0))
    {
      perror("Couldn't daemonize");
      return -1;
    }
  }

  run_server(listen_port,0,local_port);

  return 0;
}

// vi:ts=4:sw=4
