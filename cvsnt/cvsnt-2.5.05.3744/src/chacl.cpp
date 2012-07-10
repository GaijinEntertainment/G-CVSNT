/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * chacl
 * 
 * Sets the permission for the specified user for the directory
 */

#include "cvs.h"
#include "fileattr.h"

static const char *const chacl_usage[] =
{
	"Usage: %s %s [-R] [-r branch] [-u user] [-j branch] [-n] [-p priority] [-m message] [-a [no]{read|write|create|tag|control|all|none}[,...]] [-d] [file or directory...]\n",
	"\t-a access\tSet access\n",
	"\t-d\t\tDelete ACL\n",
	"\t-j branch\tApply when merging from branch\n",
	"\t-m message\tCustom error message\n",
    "\t-n\t\tDo not inherit ACL\n",
	"\t-p priority\tOverride ACL priority\n",
    "\t-r branch\tApply to single branch\n",
	"\t-R\t\tRecursively change subdirectories\n",
    "\t-u user\t\tApply to single user\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static const char *const rchacl_usage[] =
{
	"Usage: %s %s [-R] [-r branch] [-u user] [-j branch] [-n] [-p priority] [-m message] [-a [no]{read|write|create|tag|control|all|none}[,...]] [-d] [module...]\n",
	"\t-a access\tSet access\n",
	"\t-d\t\tDelete ACL\n",
	"\t-j branch\tApply when merging from branch\n",
	"\t-m message\tCustom error message\n",
    "\t-n\t\tDo not inherit ACL\n",
	"\t-p priority\tOverride ACL priority\n",
    "\t-r branch\tApply to single branch\n",
	"\t-R\t\tRecursively change subdirectories\n",
    "\t-u user\t\tApply to single user\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static const char *acl_directory_set;
static char *current_date;

static struct
{
	const char *branch;
	const char *user;
	const char *message;
	const char *merge;
	const char *priority;
	int del;
	int noinherit;
	char *access;
} parms;

static void set_attrs(CXmlNodePtr acl, const char *type, int deny, int noinherit)
{
	acl->NewNode(type);
	if(deny)
		acl->NewAttribute("deny","1");
	if(noinherit)
		acl->NewAttribute("inherit","0");
	acl->GetParent();
}

static void set_acl(CXmlNodePtr base)
{
	CXmlNodePtr acl, acl_to_set = NULL;
	acl = fileattr_find(base,"acl");

	while(acl)
	{
		const char *user = fileattr_getvalue(acl,"@user");
		const char *branch = fileattr_getvalue(acl,"@branch");
		const char *merge = fileattr_getvalue(acl,"@merge");
		if(((!user && !parms.user) || (user && parms.user && !usercmp(user,parms.user))) &&
		   ((!branch && !parms.branch) || (branch && parms.branch && !strcmp(branch,parms.branch))) &&
		   ((!merge && !parms.merge) || (merge && parms.merge && !strcmp(merge,parms.merge))))
		{
			acl_to_set = acl;
			break;
		}
		acl = fileattr_next(acl);
	}
	if(acl_to_set)
		fileattr_batch_delete(acl_to_set);
		
	if(!parms.del)
	{
		char *parm = xstrdup(parms.access);
		char *acc = parm?strtok(parm,","):NULL;

		base->NewNode("acl");
		fileattr_modified();
		if(parms.user)
			base->NewAttribute("user",parms.user);
		if(parms.branch)
			base->NewAttribute("branch",parms.branch);
		if(parms.merge)
			base->NewAttribute("merge",parms.merge);
		if(parms.priority && atoi(parms.priority))
			base->NewAttribute("priority",parms.priority);
		if(parms.message)
			base->NewNode("message",parms.message,false);
		base->NewNode("modified_by",getcaller(),false);
		base->NewNode("modified_date",current_date,false);
		while(acc)
		{
			int deny=0;
			if(!strncmp(acc,"no",2) && strcmp(acc,"none"))
			{
				deny=1;
				acc+=2;
			}
			if(!strcmp(acc,"all"))
				set_attrs(base,"all",deny,parms.noinherit);
			else if(!strcmp(acc,"none"))
				set_attrs(base,"all",!deny,parms.noinherit);
			else if(!strcmp(acc,"read"))
				set_attrs(base,"read",deny,parms.noinherit);
			else if(!strcmp(acc,"write"))
				set_attrs(base,"write",deny,parms.noinherit);
			else if(!strcmp(acc,"create"))
				set_attrs(base,"create",deny,parms.noinherit);
			else if(!strcmp(acc,"tag"))
				set_attrs(base,"tag",deny,parms.noinherit);
			else if(!strcmp(acc,"control"))
				set_attrs(base,"control",deny,parms.noinherit);
			else
				error(1,0,"Invalid access control attribute '%s'",acc);
			acc = strtok(NULL,",");
		}
		base->GetParent();
		fileattr_prune(base);
		xfree(parm);
	}
	else
	{
		if(acl_to_set)
			fileattr_prune(acl_to_set);
	}
}

static Dtype chacl_dirproc (void *callerdat, char *dir, char *repos, char *update_dir,  List *entries, const char *virtual_repository, Dtype hint)
{
	CXmlNodePtr curdir;

	if(hint!=R_PROCESS)
		return hint;

	if(!quiet)
		printf("%sing ACL for directory %s\n",parms.del?"delet":"sett",update_dir);

	curdir = fileattr_getroot();
	if(!curdir->GetChild("directory")) curdir->NewNode("directory");
	set_acl(curdir);

	xfree(acl_directory_set);
	acl_directory_set = xstrdup(repos);

	return R_PROCESS;
}

static int chacl_dirleaveproc(void *callerdat, char *dir, int err, char *update_dir, List *entries)
{
	xfree(acl_directory_set);
	return 0;
}

int chacl_fileproc(void *callerdat, struct file_info *finfo)
{
	CXmlNodePtr acl;

	/* If someone has specified 'chacl foo' and foo is a directory, you'll
       get a dirent plus every file in the directory.  We only want to set
	   the directory in this case */
	if(acl_directory_set && !strcmp(finfo->repository, acl_directory_set))
		return 0;

	Vers_TS *vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0, 0);
	if(!vers->vn_user && !vers->vn_rcs)
	{
	    if (!really_quiet)
			error (0, 0, "nothing known about %s", fn_root(finfo->fullname));
		freevers_ts(&vers);
		return 0;
	}
	freevers_ts(&vers);

	if(!quiet)
		printf("%sing ACL for file %s\n",parms.del?"delet":"sett",finfo->file);
	
	acl = fileattr_getroot();
	acl->xpathVariable("name",finfo->file);
	if(!acl->Lookup("file[cvs:filename(@name,$name)]") || !acl->XPathResultNext())
	{
		acl = fileattr_getroot();
		acl->NewNode("file");
		acl->NewAttribute("name",finfo->file);
	}

	set_acl(acl);
	return 0;
}

static int rchacl_proc(int argc, char **argv, const char *xwhere,
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

	current_date = date_from_time_t(global_session_time_t);
	err = start_recursion (chacl_fileproc, (FILESDONEPROC) NULL, (PREDIRENTPROC) NULL, chacl_dirproc,
			   (DIRLEAVEPROC) NULL, NULL,
			   argc - 1, argv + 1, local_specified, W_REPOS, 0, 1,
			   where, mapped_repository, 1, verify_control, parms.branch);

	xfree(current_date);
	xfree (mapped_repository);
    return err;
}


int chacl (int argc, char **argv)
{
	int c;
	int local = 1;
	int err = 0;
	int is_rchacl = !strcmp(command_name,"rchacl");

	if (argc == 1 || argc == -1)
		usage (is_rchacl?rchacl_usage:chacl_usage);

	memset(&parms,0,sizeof(parms));
	optind = 0;
	while ((c = getopt (argc, argv, "+a:dnRr:u:m:j:p:")) != -1)
	{
		switch (c)
		{
		case 'a':
			if(parms.del)
				error(1,0,"Cannot combine -a and -d");
			parms.access = xstrdup(optarg);
			break;
		case 'd':
			if(parms.access)
				error(1,0,"Cannot combine -a and -d");
			parms.del = 1;
			break;
		case 'j':
			parms.merge=xstrdup(optarg);
			break;
		case 'm':
			parms.message=xstrdup(optarg);
			break;
		case 'n':
			parms.noinherit=1;
			break;
		case 'p':
			parms.priority=xstrdup(optarg);
			break;
		case 'r':
			if(parms.branch)
				error(1,0,"Cannot have multiple -r options");
			parms.branch = xstrdup(optarg);
			break;
		case 'R':
			local = 0;
			break;
		case 'u':
			if(parms.user)
				error(1,0,"Cannot have multiple -u options");
			parms.user = xstrdup(optarg);
			break;
		case '?':
		default:
			usage (chacl_usage);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 0)
		usage (is_rchacl?rchacl_usage:chacl_usage);

	if (current_parsed_root->isremote)
	{
		if(is_rchacl)
		{
			if (!supported_request ("rchacl2"))
				error (1, 0, "server does not support rchacl");
		}
		else
		{
			if (!supported_request ("chacl2"))
				error (1, 0, "server does not support v2 chacl");
		}

		if(parms.branch)
		{
			send_arg("-r");
			send_arg(parms.branch);
		}
		if(parms.user)
		{
			send_arg("-u");
			send_arg(parms.user);
		}
		if(parms.del)
			send_arg("-d");
		if(parms.noinherit)
			send_arg("-n");
		if(parms.access)
		{
			send_arg("-a");
			send_arg(parms.access);
		}
		if(parms.message)
		{
			send_arg("-m");
			send_arg(parms.message);
		}
		if(parms.merge)
		{
			send_arg("-j");
			send_arg(parms.merge);
		}
		if(parms.priority)
		{
			send_arg("-p");
			send_arg(parms.priority);
		}
		if(!local)
		{
			send_arg("-R");
		}
		send_arg("--");
		if (is_rchacl)
		{
			int i;
			for (i = 0; i < argc; i++)
			send_arg (argv[i]);
			send_to_server ("rchacl2\n", 0);
		}
		else
		{
			send_file_names (argc, argv, SEND_EXPAND_WILD);
			send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
			send_to_server ("chacl2\n", 0);
		}
		return get_responses_and_close ();
	}

	if(!acl_mode)
		error(1,0,"Access control is disabled on this repository.");

	if (is_rchacl)
	{
		DBM *db;
		int i;
		db = open_module ();
		for (i = 0; i < argc; i++)
		{
			err += do_module (db, argv[i], MISC, "Changing", rchacl_proc,
					(char *) NULL, 0, local, 0, 0, (char *) NULL);
		}
		close_module (db);
	}
	else
	{
		current_date = date_from_time_t(global_session_time_t);
		err = start_recursion(chacl_fileproc, NULL, (PREDIRENTPROC) NULL, chacl_dirproc, chacl_dirleaveproc, (void*)NULL,
			argc, argv, local, W_LOCAL, 0, 0, (char*)NULL, NULL, 1, verify_control, parms.branch);
		xfree(current_date);
	}

	return (err);
}
