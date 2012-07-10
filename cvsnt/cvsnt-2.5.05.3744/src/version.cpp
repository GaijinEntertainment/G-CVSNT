/*
 * Copyright (c) 1994 david d `zoo' zuhn
 * Copyright (c) 1994 Free Software Foundation, Inc.
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with this  CVS source distribution.
 * 
 * version.c - the CVS version number
 */

#include "cvs.h"

#include "../version.h"
char *version_string = "Concurrent Versions System (CVSNT) "CVSNT_PRODUCTVERSION_STRING;

#ifdef SERVER_SUPPORT
char *config_string = " (client/server)\n";
#else
char *config_string = " (client)\n";
#endif



static const char *const version_usage[] =
{
    // -c does a 'crash'
    "Usage: %s %s\n",
 	"\t-q\tJust display version number.\n",
 	"\t-b\tJust display build number.\n",
 	"\t-h\tDisplay human readable.\n",
    "(Specify the --help global option for a list of other help options)\n",
   NULL
};



/*
 * Output a version string for the client and server.
 *
 * This function will output the simple version number (for the '--version'
 * option) or the version numbers of the client and server (using the 'version'
 * command).
 */
int version (int argc, char **argv)
{
	int c;
    int err = 0;
    char fancystr[100] = "\0";
	int quick = 0;

    if (argc == -1)
	usage (version_usage);

    optind = 0;
    while (argv && (c = getopt (argc, argv, "+cbhq")) != -1)
    {
		switch (c)
		{
			case 'c':
			*(int*)0=0x12345678;
			break;
			case 'b':
			quick = 2;
			break;
			case 'h':
			quick = 3;
			break;
			case 'q':
			quick = 1;
			break;
			case '?':
			default:
			usage (version_usage);
			break;
		}
    }
    argc -= optind;
	if(argv)
	    argv += optind;

	switch(quick)
	{
	case 1:
		if(!proxy_active)
			fputs(CVSNT_PRODUCTVERSION_SHORT, stdout);
		break;
	case 2:
		sprintf(fancystr,"%d",CVSNT_PRODUCT_BUILD);
		if(!proxy_active)
			fputs(fancystr, stdout);
		break;
	case 3:
		sprintf(fancystr,"%s (%d)",CVSNT_PRODUCT_NAME,CVSNT_PRODUCT_BUILD);
		if(!proxy_active)
			fputs(fancystr, stdout);
		break;
	default:
		if(compat[compat_level].return_fake_version)
			version_string = "Concurrent Versions System (CVS) 1.11.2";

		if(!proxy_active)
		{
			if (current_parsed_root && current_parsed_root->isremote)
				(void) fputs ("Client: ", stdout);

			/* Having the year here is a good idea, so people have
			some idea of how long ago their version of CVS was
			released.  */
			(void) fputs (version_string, stdout);
			(void) fputs (config_string, stdout);
		}

		if (current_parsed_root && current_parsed_root->isremote)
		{
			if(!proxy_active)
				fputs ("Server: ", stdout);
			if (supported_request ("version"))
				send_to_server ("version\n", 0);
			else
			{
				send_to_server ("noop\n", 0);
				fputs ("(unknown)\n", stdout);
			}
			fflush(stdout);
			err = get_responses_and_close ();
		}
	}
    return err;
}
	
