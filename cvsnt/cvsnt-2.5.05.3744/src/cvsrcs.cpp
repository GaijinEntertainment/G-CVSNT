/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * Copyright (c) 2001, Tony Hoyle
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Query CVS/Entries from server
 */

#include "cvs.h"

static const char *const cvsrcs_usage[] =
{
	"Usage: %s %s <command> [args...]\n",
    "\tco\tExecute rcs 'co'\n",
    "\trcsdiff\tExecute rcs 'diff'\n",
    "\trlog\tExecute rcs 'rlog'\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int cvsrcs(int argc, char **argv)
{
    if (argc == -1)
		usage (cvsrcs_usage);

    argc --;
    argv ++;

	if(!argc)
	{
		usage(cvsrcs_usage);
		return 0;
	}

	current_parsed_root = local_cvsroot(NULL,NULL,true,RootTypeStandard,NULL,NULL,NULL,NULL);

	if(!strcmp(argv[0],"co"))
	{
		return checkout(argc,argv);
	}
	else if(!strcmp(argv[0],"rcsdiff"))
	{
		return diff(argc,argv);
	}
	else if(!strcmp(argv[0],"rlog"))
	{
		return cvslog(argc,argv);
	}
	else
		usage(cvsrcs_usage);

    return 0;
}
