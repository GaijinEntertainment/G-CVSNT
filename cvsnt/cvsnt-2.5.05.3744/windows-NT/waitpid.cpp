/* waitpid.c --- waiting for process termination, under Windows NT
   Jim Blandy <jimb@cyclic.com> --- August 1995  */

#include <stdio.h>
#include <process.h>
#include <errno.h>
#include <assert.h>

#define MAIN_CVS
#include "config.h"

/* Wait for the process PID to exit.  Put the return status in *statusp.
   OPTIONS is not supported yet under Windows NT.  We hope it's always zero.  */
pid_t waitpid (pid_t pid, int *statusp, int options)
{
    /* We don't know how to deal with any options yet.  */
    assert (options == 0);
    
    return (pid_t)_cwait (statusp, (int)pid, _WAIT_CHILD);
}
