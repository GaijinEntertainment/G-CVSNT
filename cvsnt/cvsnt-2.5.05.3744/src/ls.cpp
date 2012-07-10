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

// Horrid hack for legacy servers
unsigned global_ls_response_hack;

static int ls_proc (int argc, char **argv, const char *xwhere, const char *mwhere, const char *mfile, int shorten, int local, const char *mname, const char *msg);
static int ls_fileproc(void *callerdat, struct file_info *finfo);
static Dtype ls_direntproc(void *callerdat, char *dir,
				      char *repos, char *update_dir,
				      List *entries, const char *virtual_repository, Dtype hint);

static const char *const ls_usage[] =
{
    "Usage: %s %s [-q] [-e] [-l] [-R] [-r rev] [-D date] [-t] [modules...]\n",
    "\t-D date\tShow files from date.\n",
    "\t-e\tDisplay in CVS/Entries format.\n",
    "\t-l\tDisplay all details.\n",
	"\t-P\tIgnore empty directories.\n",
	"\t-q\tQuieter output.\n",
	"\t-R\tList recursively.\n",
    "\t-r rev\tShow files with revision or tag.\n",
    "\t-T\tShow time in local time instead of GMT.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static int entries_format;
static int long_format;
static char *show_tag;
static char *show_date;
static int tag_validated;
static int recurse;
static char *regexp_match;
static int local_time;
static int local_time_offset;
static int prune;

int ls(int argc, char **argv)
{
    int c;
    int err = 0;
	std::vector<cvs::filename> args;

    if (argc == -1)
		usage (ls_usage);

	entries_format = 0;
	long_format = 0;
	show_tag = NULL;
	show_date = NULL;
	tag_validated = 0;
	quiet = 0;
	recurse = 0;

    local_time_offset = get_local_time_offset();

    optind = 0;
    while ((c = getopt (argc, argv, "+qelr:D:RTo:P")) != -1)
    {
	switch (c)
	{
	case 'q':
		quiet = 1;
		break;
	case 'e':
		entries_format = 1;
		break;
	case 'l':
		long_format = 1;
		break;
	case 'r':
		show_tag = optarg;
		break;
	case 'D':
		show_date = Make_Date (optarg);
		break;
	case 'R':
		recurse = 1;
		break;
	case 'T':
		local_time = 1;
		break;
	case 'o':
		local_time_offset = atoi(optarg);
		break;
	case 'P':
		prune = 1;
		break;
    case '?':
    default:
		usage (ls_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote)
    {
		char *req;

		for(int i=0; i<argc; i++)
			args.push_back(argv[i]);

		if(supported_request("rls"))
			req="rls\n";
		else if(supported_request("ls"))
			req="ls\n";
		else
		{
			req = NULL;
			if(recurse || prune || long_format)
			{
				error(1,0,"Remote server does not support rls.  Requested options not available.");
			}
			local_time = 0;
			send_to_server("Global_option -n\n",0);

			if(argc==1 && !strcmp(argv[0],"/"))
				argc=0;

			if(supported_request("expand-modules"))
			{
				send_file_names(argc,argv,SEND_EXPAND_WILD);
				client_module_expansion.clear();
				send_to_server("expand-modules\n",0);
				err = get_server_responses_noproxy();
				if(err)
					return err;

				args=client_module_expansion;

				if(!args.size())
					args.push_back(".");
			}
			send_arg("-N");
		}

		if(quiet)
			send_arg("-q");
		if(req && entries_format)
			send_arg("-e");
		if(long_format)
			send_arg("-l");
		if(recurse)
			send_arg("-R");
		if(prune)
			send_arg("-P");
		if(local_time)
		{
			char tmp[64];
			send_arg("-T");
			sprintf(tmp,"%d",local_time_offset);
			option_with_arg("-o",tmp);
		}
		if(show_tag)
			option_with_arg("-r",show_tag);
		if(show_date)
			option_with_arg("-D",show_date);

		send_arg("--");

		for (size_t i = 0; i < args.size(); ++i)
		{
			send_arg (args[i].c_str());
			if(!req)
			{
				if(strcmp(args[i].c_str(),"."))
				{
					cvs::string tmp;

					send_to_server("Directory ",0);
					send_to_server(args[i].c_str(),0);
					send_to_server("\n",0);

					tmp = current_parsed_root->directory;
					tmp+="/";
					tmp+=args[i].c_str();
					send_to_server(tmp.c_str(),0);
					send_to_server("\n",0);
				}
			}
		}

		if(!req)
		{
			send_to_server("Directory .\n",0);
			send_to_server(current_parsed_root->directory,0);
			send_to_server("\n",0);
		}

		send_to_server (req?req:"co\n", 0);

		if(!req)
		{
			if(entries_format)
				global_ls_response_hack |= 2;
			if(quiet)
				global_ls_response_hack |= 4;

			global_ls_response_hack |= 1;
		}

		err = get_responses_and_close ();

		return err;
    }

    {
	DBM *db;
	int i;
	db = open_module ();
	if(argc)
	{
		for (i = 0; i < argc; i++)
		{
			char *mod = xstrdup(argv[i]);
			char *p;

			for(p=strchr(mod,'\\'); p; p=strchr(p,'\\'))
				*p='/';

			p = strrchr(mod,'/');
			if(p && (strchr(p,'?') || strchr(p,'*')))
			{
				*p='\0';
				regexp_match = p+1;
			}
			else
				regexp_match = NULL;

			/* Frontends like to do 'ls -q /', so we support it explicitly */
			if(!strcmp(mod,"/"))
			{
				*mod='\0';
			}

			err += do_module (db, mod, MISC,
					  "Listing",
					  ls_proc, (char *) NULL, 0, 0, 0, 0, (char*)NULL);

			xfree(mod);
		}
	}
	else
	{
		err += do_module (db, ".", MISC,
				  "Listing",
				  ls_proc, (char *) NULL, 0, 0, 0, 0, (char*)NULL);
	}
	close_module (db);
    }

    return (err);
}

static int ls_proc (int argc, char **argv, const char *xwhere, const char *mwhere, const char *mfile, int shorten, int local, const char *mname, const char *msg)
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    int which;
    char *repository, *mapped_repository;
    char *where;

	if(!quiet)
	{
		if(strcmp(mname,"."))
		  {
		   cvs_outerr("Listing module: ", 0);
		   cvs_outerr(mname, 0);
		   cvs_outerr("\n\n", 0);
		  }
	   else
		   cvs_outerr("Listing modules on server\n\n", 0);

	}

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
	xfree (mapped_repository);
	/* End section which is identical to patch_proc.  */

    which = W_REPOS;
	repository = NULL;

    if (show_tag != NULL && !tag_validated)
    {
	tag_check_valid (show_tag, argc - 1, argv + 1, local, 0, repository);
	tag_validated = 1;
    }

    err = start_recursion (ls_fileproc, (FILESDONEPROC) NULL,(PREDIRENTPROC) NULL,
			   ls_direntproc, (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local, which, 0, 1,
			   where, repository, 1, verify_read, show_tag);

	if(!strcmp(mname,"."))
	{
		DBM *db;
	    if ((db = open_module ())!=NULL)
		{
			datum key = dbm_firstkey(db);
			if(key.dptr)
			{
				cvs_output("\n",1);
				if(!quiet)
					cvs_outerr("Virtual modules on server (CVSROOT/modules file)\n\n",0);
				cat_module(0);
			}
			close_module(db);
		}
	}
    return err;
}


/*
 * display the status of a file
 */
/* ARGSUSED */
static int
ls_fileproc(void *callerdat, struct file_info *finfo)
{
	Vers_TS *vers;
	char outdate[32],tag[64];
	time_t t;

	if(regexp_match && !regex_filename_match(regexp_match,finfo->file))
		return 0;

    vers = Version_TS (finfo, NULL, show_tag, show_date, 1, 0, 0);
	if(!vers->vn_rcs)
	{
		freevers_ts(&vers);
		return 0;
	}

	if(RCS_isdead(finfo->rcs, vers->vn_rcs))
	{
		freevers_ts(&vers);
		return 0;
	}

	t=RCS_getrevtime (finfo->rcs, vers->vn_rcs, 0, 0);
	
	if(local_time)
		t+=local_time_offset;
	strcpy(outdate,asctime(gmtime(&t)));
	outdate[strlen(outdate)-1]='\0';

	if(entries_format)
	{
		tag[0]='\0';
		if(show_tag)
			sprintf(tag,"T%s",show_tag);

		printf("/%s/%s/%s/%s/%s\n",finfo->file,vers->vn_rcs,outdate,vers->options,tag);
	}
	else if(long_format)
	{
		printf("%-32.32s%-8.8s%s %s\n",finfo->file,vers->vn_rcs,outdate,vers->options);
	}
	else
		printf("%s\n",finfo->file);

	freevers_ts(&vers);
	return 0;
}

Dtype ls_direntproc(void *callerdat, char *dir,
				      char *repos, char *update_dir,
				      List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

	if(prune && (!entries || list_isempty(entries)))
		return R_SKIP_ALL;

	if(!strcasecmp(dir,"."))
		return R_PROCESS;
	if(recurse)
	{
		printf("\nDirectory %s\n\n",update_dir);
		return R_PROCESS;
	}
	if(entries_format)
	{
		printf("D/%s////\n",dir);
	}
	else if(long_format)
	{
		printf("%-32.32s(directory)\n",dir);
	}
	else
		printf("%s\n",dir);
	return R_SKIP_ALL;
}

