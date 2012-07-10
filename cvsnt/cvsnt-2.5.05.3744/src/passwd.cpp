/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * passwd
 * 
 * Changes the password of the caller
 *
 * Corey Minyard:				Initial development
 * Tony Hoyle:		23/11/2001	Rewrite for cvsnt
 *
 */

#include "cvs.h"

#include "getline.h"

#if defined(_WIN32) && defined(SERVER_SUPPORT)
/* We use this for sanity checking */
extern int isDomainMember();
#endif

static passwd_entry *passwd_list = NULL;
static int passwd_list_size = 0;

void free_passwd_list()
{
	if(passwd_list)
	{
		int u;
		for(u=0; u<passwd_list_size; u++)
		{
			xfree(passwd_list[u].username);
			xfree(passwd_list[u].password);
			xfree(passwd_list[u].real_username);
		}
		xfree(passwd_list);
	}
}

void init_passwd_list()
{
	if(passwd_list)
		free_passwd_list();
	passwd_list = NULL;
	passwd_list_size = 0;
}


passwd_entry *new_passwd_entry()
{
	passwd_list_size++;
	passwd_list = (passwd_entry*)xrealloc(passwd_list,sizeof(passwd_entry)*passwd_list_size);
	memset(passwd_list+passwd_list_size-1,0,sizeof(passwd_entry));
	return passwd_list+passwd_list_size-1;
}

passwd_entry *find_passwd_entry(const char *username)
{
	int u;
	for(u=0; u<passwd_list_size; u++)
		if(passwd_list[u].username && !usercmp(passwd_list[u].username,username))
			return passwd_list+u;
	return NULL;
}

int does_passwd_user_exist(const char *username)
{
	if(find_passwd_entry(username))
		return 1;
	if(system_auth && getpwnam(username))
		return 1;
	return 0;
}

static int write_passwd_entry(FILE *fp, int entry_num)
{
	if(!passwd_list[entry_num].username)
		return 0;

	fprintf(fp,"%s",passwd_list[entry_num].username);
	if(passwd_list[entry_num].password)
		fprintf(fp,":%s",passwd_list[entry_num].password);
	else
		fprintf(fp,":");
	if(passwd_list[entry_num].real_username)
		fprintf(fp,":%s",passwd_list[entry_num].real_username);
	fprintf(fp,"\n");
	return 0;
}

int read_passwd_list()
{
	char *filename;
	FILE *fp;
	char *linebuf = NULL;
	size_t linebuf_len;
	passwd_entry *passnode;

	filename = (char*)xmalloc(strlen(current_parsed_root->directory)
			   + strlen("/CVSROOT/passwd")
			   + 1);
	strcpy (filename, current_parsed_root->directory);
	strcat (filename, "/CVSROOT/passwd");

	init_passwd_list();

	fp = CVS_FOPEN (filename, "r");
	if (fp != NULL)
	{
	    while (getline (&linebuf, &linebuf_len, fp) >= 0)
	    {
			char *user;
			if(linebuf[strlen(linebuf)-1]=='\n')
				linebuf[strlen(linebuf)-1]='\0';
			if(linebuf[0]=='#')
				continue;
			user = cvs_strtok(linebuf,":");
			if(!user || !*user)
				continue;
			passnode = new_passwd_entry();
			passnode->username=xstrdup(user);
			passnode->password=xstrdup(cvs_strtok(NULL,":"));
			passnode->real_username=xstrdup(cvs_strtok(NULL,":"));
	    
			xfree (linebuf);
			linebuf = NULL;
	    }
	    if (ferror (fp))
	    {
			error (1, errno, "cannot read %s", filename);
	    }
	    if (fclose (fp) < 0)
			error (0, errno, "cannot close %s", filename);
	}
	return 0;
}

int write_passwd_list()
{
	FILE *fp;
	char *filename;

	filename = (char*)xmalloc(strlen(current_parsed_root->directory)
			   + strlen("/CVSROOT/passwd")
			   + 1);
	strcpy (filename, current_parsed_root->directory);
	strcat (filename, "/CVSROOT/passwd");

	fp = CVS_FOPEN (filename, "w");
	if (fp == NULL)
	{
	    error (0, errno, "cannot open %s for writing", filename);
	}
	else
	{
		int u;
		for(u=0; u<passwd_list_size; u++)
			write_passwd_entry(fp, u);
	    if (fclose (fp) < 0)
			error (0, errno, "cannot close %s", filename);
	}

	xfree(filename);
	return 0;
}

static const char *const passwd_usage[] =
{
    "Usage: %s %s [-a] [-x] [-X] [-r real_user] [-R] [-D domain] [username]\n",
    "\t-a\tAdd user\n",
	"\t-x\tDisable user\n",
	"\t-X\tDelete user\n",
	"\t-r\tAlias username to real system user\n",
	"\t-R\tRemove alias to real system user\n",
	"\t-D\tUse domain password\n",
    NULL
};

int passwd (int argc, char **argv)
{
    int    c;
    int    err = 0;
	bool md5 = true;
	char   typed_password[128]={0}, typed_password2[128]={0};
    const char   *username, *user;
	passwd_entry *passnode;
	char *real_user = NULL;
	char *password_domain = NULL;
	int adduser=0,deluser=0,disableuser=0,realuser=0,remove_realuser=0,use_domain=0;
	int arg_specified = 0;
	CCrypt cr;

    if (argc == -1)
	usage (passwd_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+axXr:RD:")) != -1)
    {
	switch (c)
	{
	case 'a':
		if(arg_specified)
			usage (passwd_usage);
		arg_specified = 1;
	    adduser = 1;
	    break;
	case 'x':
		if(arg_specified)
			usage (passwd_usage);
		arg_specified = 1;
		disableuser = 1;
		break;
	case 'X':
		if(arg_specified)
			usage (passwd_usage);
		arg_specified = 1;
		deluser = 1;
		break;
	case 'r':
		realuser = 1;
		real_user = xstrdup(optarg);
		break;
	case 'R':
		remove_realuser = 1;
		break;
	case 'D':
		use_domain = 1;
		password_domain = xstrdup(optarg);
		break;
	case '?':
	default:
	    usage (passwd_usage);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

	if(!argc)
		user = NULL;
	else
		user=argv[0];

    if (current_parsed_root->isremote)
    {
	if (argc > 1)
	    usage (passwd_usage);

	if (!supported_request ("passwd"))
	    error (1, 0, "server does not support passwd");

	if (!supported_request ("passwd-md5"))
	    md5=false;

	if(!user && adduser)
	{
		error(1,0,"You cannot add yourself");
	}
	if(!user && deluser)
	{
		error(1,0,"You cannot delete yourself");
	}

	if(user || current_parsed_root->username || current_parsed_root->hostname)
	{
		printf ("%s %s@%s\n",
			(adduser) ? "Adding user" : (deluser) ? "Deleting user" : "Changing repository password for",
			user?user:current_parsed_root->username?current_parsed_root->username:getcaller(),current_parsed_root->hostname);
	}
	else
	{
		printf ("Changing repository password for %s\n",getcaller());
	}
	fflush (stdout);

	if(!use_domain && !deluser && !disableuser)
	{
		CProtocolLibrary lib;

		if(!lib.PromptForPassword("New Password: ",typed_password,sizeof(typed_password)))
			error(1,0,"Aborted");

		if(!lib.PromptForPassword("Verify Password: ",typed_password2,sizeof(typed_password2)))
			error(1,0,"Aborted");

		if (strcmp(typed_password, typed_password2) != 0)
		{
			memset (typed_password, 0, strlen (typed_password));
			memset (typed_password2, 0, strlen (typed_password2));
			error (1, 0, "Passwords do not match, try again");
		}
		memset (typed_password2, 0, strlen (typed_password2));
		if(strlen(typed_password))
			strcpy(typed_password,cr.crypt(typed_password, md5));
	}

	if (adduser)
	    send_arg ("-a");
	if (disableuser)
		send_arg ("-x");
	if (deluser)
		send_arg ("-X");
	if (realuser)
	{
		send_arg ("-r");
	 	send_arg (real_user);
	}
	if (remove_realuser)
		send_arg ("-R");
	if(use_domain)
	{
		send_arg ("-D");
		send_arg (password_domain);
	}

	if (argc == 1)
	    send_arg(user);
	else
		send_arg("*");

	if(typed_password)
	{
		send_arg (typed_password); /* Send the new password */
		memset (typed_password, 0, strlen (typed_password));
	}

	send_to_server ("passwd\n", 0);
	return get_responses_and_close ();
    }
	if(!server_active)
	{
		if(argc!=0 && argc!=1)
			usage (passwd_usage);

		if(!user && adduser)
		{
			error(1,0,"You cannot add yourself");
		}
		if(!user && deluser)
		{
			error(1,0,"You cannot delete yourself");
		}

		if(user || current_parsed_root->username)
		{
			printf ("%s %s\n",
				(adduser) ? "Adding user" : (deluser) ? "Deleting user" : "Changing password for",
				user?user:current_parsed_root->username);
		}
		else
		{
			printf ("Changing repository password for %s\n",getcaller());
		}
		fflush (stdout);

  		if (argc == 0)
			username = CVS_Username;
		else
		{
			username = user;
		}

		if(!use_domain && !deluser && !disableuser)
		{
			CProtocolLibrary lib;

			if(!lib.PromptForPassword("New Password: ",typed_password,sizeof(typed_password)))
				error(1,0,"Aborted");

			if(!lib.PromptForPassword("Verify Password: ",typed_password2,sizeof(typed_password2)))
				error(1,0,"Aborted");

			if (strcmp(typed_password, typed_password2) != 0)
			{
				memset (typed_password, 0, strlen (typed_password));
				memset (typed_password2, 0, strlen (typed_password2));
				error (1, 0, "Passwords do not match, try again");
			}
			memset (typed_password2, 0, strlen (typed_password2));
			if(strlen(typed_password))
				strcpy(typed_password,cr.crypt(typed_password, md5));
		}
	} 
#ifdef SERVER_SUPPORT
	if(server_active)
	{
		if ((argc != 1) && (argc != 2))
			usage (passwd_usage);

		if(!strcmp(user,"*"))
			username = CVS_Username;
		else
			username = user;

		if(argc==2)
			strncpy(typed_password,argv[1],sizeof(typed_password));
	}
#endif

    if (typed_password[0] && 
	(strcmp(username, CVS_Username) != 0) && 
	(! verify_admin ()))
		error (1, 0, "Only administrators can add or change another's password");

	read_passwd_list();
	passnode = find_passwd_entry(username);
	if (passnode == NULL)
	{
	    if (!adduser)
			error (1, 0, "Could not find %s in password file", username);

	    if (! verify_admin())
		{
			error (1, 0, "Only administrators can add users" );
	    }

		passnode = new_passwd_entry();
		passnode->username=xstrdup(username);
		passnode->password=xstrdup(typed_password);
		passnode->real_username=NULL;
	}

	if(deluser)
	{
	    if (! verify_admin())
		{
			error (1, 0, "Only administrators can delete users" );
	    }
		xfree(passnode->username);
		passnode->username = NULL;
	}
	else if(disableuser)
	{
	    if (! verify_admin())
		{
			error (1, 0, "Only administrators can disable users" );
	    }
		xfree(passnode->password);
		passnode->password=xstrdup("#DISABLED#");
	}
	else
	{
		xfree(passnode->password);
#ifdef _WIN32 /* Unix servers can't make any sense of this */
		if(use_domain)
		{
			passnode->password = (char*)xmalloc(strlen(password_domain)+2);
			strcpy(passnode->password,"!");
			strcat(passnode->password,password_domain);
		}
		else
#endif
			passnode->password = xstrdup(typed_password);

		if(realuser)
		{
			if(!getpwnam(real_user))
				error(1, 0, "User '%s' is not a real user on the system.",real_user);

			xfree(passnode->real_username);
			passnode->real_username = xstrdup(real_user);
		}
		else if (remove_realuser)
		{
			xfree(passnode->real_username);
			passnode->real_username=NULL;
		}

		if(!runas_user && ((passnode->real_username && !getpwnam(passnode->real_username)) || (!passnode->real_username && passnode->username && !getpwnam(passnode->username))))
		{
			error(0,0,"*WARNING* CVS user '%s' will not be able to log in until they are aliased to a valid system user.",username);
		}
	}

	write_passwd_list();
	free_passwd_list();
	xfree(real_user);
	xfree(password_domain);

    return (err);
}


