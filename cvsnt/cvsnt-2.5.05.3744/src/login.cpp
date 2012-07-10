/*
 * Copyright (c) 1995, Cyclic Software, Bloomington, IN, USA
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with CVS.
 * 
 * Allow user to log in for an authenticating server.
 */

#include "cvs.h"
#include "getline.h"

/* Prompt for a password, and store it in the file "CVS/.cvspass".
 */

static const char *const login_usage[] =
{
    "Usage: %s %s [-p password]\n",
	"\t-p password\tSpecify password to use (default is to prompt)\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int login (int argc, char **argv)
{
    char typed_password[256];
    char *cvsroot_canonical;
	const char *specified_password = NULL;
	int c;

    if (argc < 0)
		usage (login_usage);

    optind = 0;
	while ((c = getopt (argc, argv, "+p:")) != -1)
    {
		switch (c)
		{
		case 'p':
			specified_password = optarg;
			break;
		case '?':
		default:
			usage (login_usage);
			break;
		}
    }
    argc -= optind;
    argv += optind;

	if(!client_protocol || !client_protocol->login)
	{
		error(1,0,"The :%s: protocol does not support the login command",(current_parsed_root&&current_parsed_root->method)?current_parsed_root->method:"local");
	}

    cvsroot_canonical = normalize_cvsroot(current_parsed_root);
    printf ("Logging in to %s\n", cvsroot_canonical);
    fflush (stderr);
    fflush (stdout);
    xfree (cvsroot_canonical);

	if (specified_password)
		;
	else if (current_parsed_root->password)
		specified_password = current_parsed_root->password;
    else
	{
		CProtocolLibrary lib;

		if(!lib.PromptForPassword("CVS Password: ",typed_password,sizeof(typed_password)))
			error(1,0,"Aborted");

		specified_password = typed_password;
	}

    if(!specified_password)
	    return 1;
 
	if(client_protocol->login(client_protocol,(char*)specified_password))
		return 1;
	if(start_server(1)) /* Verify the new password */
		return 1;
	return 0;
}

static const char *const logout_usage[] =
{
    "Usage: %s %s\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* Remove any entry for the CVSRoot repository found in .cvspass. */
int logout (int argc, char **argv)
{
    char *cvsroot_canonical;

    if (argc < 0)
	usage (logout_usage);

	if(!client_protocol || !client_protocol->logout)
	{
		error(1,0,"The :%s: protocol does not support the logout command",(current_parsed_root&&current_parsed_root->method)?current_parsed_root->method:"local");
	}

    /* Hmm.  Do we want a variant of this command which deletes _all_
       the entries from the current .cvspass?  Might be easier to
       remember than "rm ~/.cvspass" but then again if people are
       mucking with HOME (common in Win95 as the system doesn't set
       it), then this variant of "cvs logout" might give a false sense
       of security, in that it wouldn't delete entries from any
       .cvspass files but the current one.  */

    if (!quiet)
    {
		cvsroot_canonical = normalize_cvsroot(current_parsed_root);
		printf ("Logging out of %s\n", cvsroot_canonical);
		fflush (stderr);
		fflush (stdout);
		xfree (cvsroot_canonical);
    }

	if(client_protocol->logout(client_protocol))
		return 1;
    return 0;
}
