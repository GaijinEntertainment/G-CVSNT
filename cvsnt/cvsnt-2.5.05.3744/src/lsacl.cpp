/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * lsacl
 * 
 * Shows the current permissions
 */

#include "cvs.h"
#include "fileattr.h"

static int is_rlsacl;

static const char *const lsacl_usage[] =
{
    "Usage: %s %s [-d] [-R] [file or directory...]\n",
	"\t-d\tShow directories only\n",
	"\t-R\tRecurse unto subdirectories\n",
    NULL
};

static const char *const rlsacl_usage[] =
{
    "Usage: %s %s [-d] [-R] [module...]\n",
	"\t-d\tShow directories only\n",
	"\t-R\tRecurse unto subdirectories\n",
    NULL
};

static Dtype lsacl_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint);
static int lsacl_fileproc(void *callerdat, struct file_info *finfo);
static int rlsacl_proc(int argc, char **argv, const char *xwhere,
			    const char *mwhere, const char *mfile, int shorten,
			    int local_specified, const char *mname, const char *msg);

int lsacl (int argc, char **argv)
{
	int c;
	int err = 0;
	int local = 1;
	int directories_only = 0;

	is_rlsacl = !strcmp(command_name,"rlsacl");

	if (argc == -1)
		usage (is_rlsacl?rlsacl_usage:lsacl_usage);

	optind = 0;
	while ((c = getopt(argc, argv, "+dR")) != -1)
	{
		switch (c)
		{
		case 'd':
			directories_only = 1;
			break;
		case 'R':
			local = 0;
			break;
		case '?':
		default:
		usage (lsacl_usage);
		break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 0)
		usage (is_rlsacl?rlsacl_usage:lsacl_usage);

	if (current_parsed_root->isremote)
	{
		if(is_rlsacl)
		{
			if (!supported_request ("rlsacl"))
				error (1, 0, "server does not support rlsacl");
		}
		else
		{
			if (!supported_request ("lsacl"))
				error (1, 0, "server does not support lsacl");
		}

		if(!local)
			send_arg("-R");

		if(directories_only)
			send_arg("-d");

		send_arg("--");
		if (is_rlsacl)
		{
			int i;
			for (i = 0; i < argc; i++)
			send_arg (argv[i]);
			send_to_server ("rlsacl\n", 0);
		}
		else
		{
			send_file_names (argc, argv, SEND_EXPAND_WILD);
			send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
			send_to_server ("lsacl\n", 0);
		}
		return get_responses_and_close ();
   }

	if(!acl_mode)
		error(1,0,"Access control is disabled on this repository.");
    
	if (is_rlsacl)
	{
		DBM *db;
		int i;
		db = open_module ();
		if(!argc)
		{
				err += do_module (db, ".", MISC, "Listing", rlsacl_proc,
						(char *) NULL, 0, local, 0, 0, (char *) NULL);
		}
		else
		{
			for (i = 0; i < argc; i++)
			{
				err += do_module (db, argv[i], MISC, "Listing", rlsacl_proc,
						(char *) NULL, 0, local, 0, 0, (char *) NULL);
			}
		}
		close_module (db);
	}
	else
	{
		/* start the recursion processor */
		err = start_recursion (directories_only?NULL:lsacl_fileproc, (FILESDONEPROC) NULL,
					(PREDIRENTPROC) NULL, lsacl_dirproc, (DIRLEAVEPROC) NULL, NULL,
					argc, argv, local,
					W_LOCAL, 0, 1, (char *) NULL, NULL, 1, NULL, NULL);
	}

    return (err);

}

static void acl_details(CXmlNodePtr  acl,const char *type)
{
	CXmlNodePtr  h=fileattr_find(acl,type);
	const char *d,*i;
	if(h)
	{
		d=fileattr_getvalue(h,"@deny");
		i=fileattr_getvalue(h,"@inherit");
		if(!strcmp(type,"all") && (d && atoi(d)))
		{
			type="none";
			d=NULL;
		}
		printf("\t%s",type);
		if((d && atoi(d)) || (i && !atoi(i)))
		{
			printf("(");
			if(d && atoi(d))
			{
				printf("deny");
				if(i && atoi(i))
					printf(",");
			}
			if(i&&!atoi(i))
			{
				printf("noinherit");
			}
			printf(")");
		}
		printf("\n");
	}
}

static void show_acl(CXmlNodePtr  acl)
{
	while(acl)
	{
		const char *user = fileattr_getvalue(acl,"@user");
		const char *branch = fileattr_getvalue(acl,"@branch");
		const char *merge = fileattr_getvalue(acl,"@merge");
		const char *priority = fileattr_getvalue(acl,"@priority");
		const char *message = fileattr_getvalue(acl,"message");
		const char *comma="";
		printf("\n");
		if(user)
		{
			printf("user=%s",user);
			comma=",";
		}
		if(branch)
		{
			printf("%sbranch=%s",comma,branch);
			comma=",";
		}
		if(merge)
		{
			printf("%smerge=%s",comma,merge);
		}
		if(!user && !branch && !merge)
			printf("<default>");
		if(priority)
			printf(",priority=%s",priority);
		if(message)
			printf(",message=%s",message);
		printf("\n");
		acl_details(acl,"all");
		acl_details(acl,"read");
		acl_details(acl,"write");
		acl_details(acl,"create");
		acl_details(acl,"tag");
		acl_details(acl,"control");

		acl = fileattr_next(acl);
	}
}

/*
 * Show file ACL
 */
static Dtype lsacl_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	const char *owner;
	CXmlNodePtr  acl;

	if(hint!=R_PROCESS)
	  return hint;

	owner = fileattr_getvalue(NULL,"directory/owner");

	if(is_rlsacl && !strcmp(update_dir,"."))
		update_dir="<root>";
	printf("Directory: %s\n",update_dir);
	printf("Owner: %s\n",(owner&&*owner)?owner:"<not set>");
	
	acl = fileattr_find(NULL,"directory/acl");
	show_acl(acl);

	return R_PROCESS;
}

int lsacl_fileproc(void *callerdat, struct file_info *finfo)
{
	CXmlNodePtr  acl;

	acl = fileattr_getroot();
	acl->xpathVariable("name",finfo->file);
	if(acl->Lookup("file[cvs:filename(@name,$name)]/acl") && acl->XPathResultNext())
	{
		printf("File: %s\n",finfo->file);
		show_acl(acl);
	}
	return 0;
}

static int rlsacl_proc(int argc, char **argv, const char *xwhere,
			    const char *mwhere, const char *mfile, int shorten,
			    int local_specified, const char *mname, const char *msg)
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    char *repository, *mapped_repository;
    char *where;

	repository = (char*)xmalloc (strlen (current_parsed_root->directory) + strlen (argv[0])
			      + (mfile == NULL ? 0 : strlen (mfile) + 1) + 2);
	(void) sprintf (repository, "%s/%s", current_parsed_root->directory, argv[0]);
	where = (char*)xmalloc (strlen (argv[0]) + (mfile == NULL ? 0 : strlen (mfile) + 1)
			 + 1);
	(void) strcpy (where, argv[0]);

	/* if mfile isn't null, we need to set up to do only part of the module */
	if (mfile != NULL)
	{
	    char *cp;
	    char *path;

	    /* if the portion of the module is a path, put the dir part on repos */
	    if ((cp = (char*)strrchr (mfile, '/')) != NULL)
	    {
		*cp = '\0';
		(void) strcat (repository, "/");
		(void) strcat (repository, mfile);
		(void) strcat (where, "/");
		(void) strcat (where, mfile);
		mfile = cp + 1;
	    }

	    /* take care of the rest */
	    path = (char*)xmalloc (strlen (repository) + strlen (mfile) + 5);
	    (void) sprintf (path, "%s/%s", repository, mfile);
	    if (isdir (path))
	    {
		/* directory means repository gets the dir tacked on */
		(void) strcpy (repository, path);
		(void) strcat (where, "/");
		(void) strcat (where, mfile);
	    }
	    else
	    {
		myargv[0] = argv[0];
		myargv[1] = (char*)mfile;
		argc = 2;
		argv = myargv;
	    }
	    xfree (path);
	}

	mapped_repository = map_repository(repository);

	/* cd to the starting repository */
	if ( CVS_CHDIR (mapped_repository) < 0)
	{
	    error (0, errno, "cannot chdir to %s", fn_root(repository));
	    xfree (repository);
	    xfree (mapped_repository);
	    return (1);
	}
	xfree (repository);
	/* End section which is identical to patch_proc.  */

    err = start_recursion (lsacl_fileproc, (FILESDONEPROC) NULL, (PREDIRENTPROC) NULL, lsacl_dirproc,
			   (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local_specified, W_REPOS, 0, 1,
			   where, mapped_repository, 1, NULL, NULL);

	xfree (mapped_repository);
    return err;
}
