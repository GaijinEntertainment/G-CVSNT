#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>

#include "config.h"
#include "daemon.h"
#include "getopt1.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#define SOCKET int
#define DEFAULT_PORT 12401

#include "CvsControl.h"

bool g_bTestMode = false;
int listen_port = DEFAULT_PORT;

static void usage(const char *prog)
{
  fprintf(stderr,"Usage: %s [-d] [-p port]\n",prog);
  exit(-1);
}

int main(int argc, char *argv[])
{
  int c;
  int digit_optind = 0;

  while(1)
  {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] =
    {
      {0,0,0,0}
    };

    c = getopt_long (argc,argv,"+dp:",long_options, &option_index);
    if (c == -1)
      break;

    switch(c)
    {
      case 'd':
        g_bTestMode = true;
        break;
      case 'p':
        listen_port = atoi(optarg);
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

  run_server(listen_port,0,0);

  return 0;
}
