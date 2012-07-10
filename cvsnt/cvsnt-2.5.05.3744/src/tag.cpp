/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Tag and Rtag
 * 
 * Add or delete a symbolic name to an RCS file, or a collection of RCS files.
 * Tag uses the checked out revision in the current directory, rtag uses
 * the modules database, if necessary.
 */

#include "cvs.h"
#include "savecwd.h"

static int rtag_proc(int argc, char **argv, const char *xwhere,
		      const char *mwhere, const char *mfile, int shorten,
		      int local_specified, const char *mname, const char *msg);
static int check_fileproc(void *callerdat, struct file_info *finfo);
static int check_filesdoneproc(void *callerdat, int err,
				       char *repos, char *update_dir,
				       List *entries);
static int pretag_proc(void *params, const trigger_interface *cb);
static void masterlist_delproc(Node *p);
static void tag_delproc(Node *p);
static int pretag_list_proc(Node *p, void *closure);

static Dtype tag_dirproc(void *callerdat, char *dir,
				 char *repos, char *update_dir,
				 List *entries, const char *virtual_repository, Dtype hint);
static int rtag_fileproc(void *callerdat, struct file_info *finfo);
static int rtag_delete(RCSNode *rcsfile);
static int tag_fileproc(void *callerdat, struct file_info *finfo);
static int add_to_valtags(char *name);

static char *numtag;			/* specific revision to tag */
static int numtag_validated = 0;
static char *date = NULL;
static char *symtag;			/* tag to add or delete */
static int delete_flag;			/* adding a tag by default */
static int branch_mode;			/* make an automagic "branch" tag */
static int local;			/* recursive by default */
static int force_tag_match = 1;		/* force tag to match by default */
static int move_branch_tag;		/* Move branch tags as well */
static int force_tag_move;		/* don't force tag to move by default */
static int check_uptodate;		/* no uptodate-check by default */
static int is_rtag;
static int tag_set_ok;			/* Set if tags were really set (rtag only) */
static char *current_date;
static int floating_branch;		/* Floating or 'magic' branch */
static int alias_branch;
static const char **pretag_list;
static const char **pretag_version_list;
static int pretag_list_count,pretag_list_size;
static const char *message;

extern int lock_for_write;

struct pretag_params_t
{
	const char *message;
	const char *directory;
	char tag_type;
	const char *action;
	const char *tag;
};

struct tag_info
{
    Ctype status;
    char *rev;
    char *tag;
    char *options;
};

struct master_lists
{
    List *tlist;
};

static List *mtlist;
static List *tlist;

static const char rtag_opts[] = "+AabdFBflnQqRr:D:m:";
static const char *const rtag_usage[] =
{
    "Usage: %s %s [-abdFflnR] [-r rev|-D date] tag modules...\n",
	"\t-A\tMake alias of existing branch (requires -r).\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-d\tDelete the given tag.\n",
    "\t-F\tMove tag if it already exists.\n",
    "\t-B\tAllow move/delete of branch tag (not recommended).\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive.\n",
    "\t-n\tNo execution of 'tag program'.\n",
	"\t-m message\tSpecify message for logs.\n",
	"\t-M\tCreate floating branch.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-r rev\tExisting revision/tag.\n",
    "\t-D\tExisting date.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static const char tag_opts[] = "+AbcdFBflQqRr:D:Mm:";
static const char *const tag_usage[] =
{
    "Usage: %s %s [-bcdFflR] [-r rev|-D date] tag [files...]\n",
	"\t-A\tMake alias of existing branch (requires -r).\n",
    "\t-b\tMake the tag a \"branch\" tag, allowing concurrent development.\n",
    "\t-c\tCheck that working files are unmodified.\n",
    "\t-d\tDelete the given tag.\n",
    "\t-F\tMove tag if it already exists.\n",
    "\t-B\tAllow move/delete of branch tag (not recommended).\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
    "\t-l\tLocal directory only, not recursive.\n",
	"\t-m message\tSpecify message for logs.\n",
	"\t-M\tCreate floating branch.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-r rev\tExisting revision/tag.\n",
    "\t-D\tExisting date.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int cvstag (int argc, char **argv)
{
    int c;
    int err = 0;
    int run_module_prog = 1;

    is_rtag = (strcmp (command_name, "rtag") == 0);
    
    if (argc == -1)
	usage (is_rtag ? rtag_usage : tag_usage);

    optind = 0;
    while ((c = getopt (argc, argv, is_rtag ? rtag_opts : tag_opts)) != -1)
    {
	switch (c)
	{
		case 'A':
			alias_branch = 1;
			if(floating_branch)
				error(1,0,"Cannot mix -M and -A");
			break;
	    case 'a':
		break;
	    case 'b':
		branch_mode = 1;
		break;
	    case 'c':
		check_uptodate = 1;
		break;
	    case 'd':
		delete_flag = 1;
		break;
	    case 'B':
		move_branch_tag = 1;
		break;
            case 'F':
		force_tag_move = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'l':
		local = 1;
		break;
		case 'm':
			message = optarg;
			break;
		case 'M':
			branch_mode = 1;
			floating_branch = 1;
			if(alias_branch)
				error(1,0,"Cannot mix -M and -A");
			break;
	    case 'n':
		run_module_prog = 0;
		break;
	    case 'Q':
	    case 'q':
		/* The CVS 1.5 client sends these options (in addition to
		   Global_option requests), so we must ignore them.  */
		if (!server_active)
		    error (1, 0,
			   "-q or -Q must be specified before \"%s\"",
			   command_name);
		break;
	    case 'R':
		local = 0;
		break;
            case 'r':
                numtag = optarg;
                break;
            case 'D':
                if (date)
                    xfree (date);
                date = Make_Date (optarg);
                break;
	    case '?':
	    default:
		usage (is_rtag ? rtag_usage : tag_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (argc < (is_rtag ? 2 : 1))
	usage (is_rtag ? rtag_usage : tag_usage);
    symtag = argv[0];
    argc--;
    argv++;

    if (date && numtag)
	error (1, 0, "-r and -D options are mutually exclusive");
    if (delete_flag && branch_mode)
	error (0, 0, "warning: -b ignored with -d options");
    RCS_check_tag (symtag,true,false,false);

    if (current_parsed_root->isremote)
    {
	if (branch_mode)
	    send_arg("-b");
	if (check_uptodate)
	    send_arg("-c");
	if (delete_flag)
	    send_arg("-d");
	if (move_branch_tag)
	    send_arg("-B");
	if (force_tag_move)
	    send_arg("-F");
	if (!force_tag_match)
	    send_arg ("-f");
	if (local)
	    send_arg("-l");
	if (message)
		send_arg("-m");
	if (floating_branch)
		send_arg("-M");
	if (alias_branch)
		send_arg("-A");
	if (!run_module_prog)
	    send_arg("-n");

	if (numtag)
	    option_with_arg ("-r", numtag);
	if (date)
	    client_senddate (date);

	send_arg("--");

	send_arg (symtag);

	if (is_rtag)
	{
	    int i;
	    for (i = 0; i < argc; ++i)
			send_arg (argv[i]);
	    send_to_server ("rtag\n", 0);
	}
	else
	{

	    send_files (argc, argv, local, 0,

		    /* I think the -c case is like "cvs status", in
		       which we really better be correct rather than
		       being fast; it is just too confusing otherwise.  */
			check_uptodate ? 0 : SEND_NO_CONTENTS);
	    send_file_names (argc, argv, SEND_EXPAND_WILD);
	    send_to_server ("tag\n", 0);
	}

        return get_responses_and_close ();
    }

	lock_for_write = 1;
    if (is_rtag)
    {
	DBM *db;
	int i;
	db = open_module ();
	for (i = 0; i < argc; i++)
	{
	    /* XXX last arg should be repository, but doesn't make sense here */
	    history_write ('T', (delete_flag ? "D" : (numtag ? numtag : 
			   (date ? date : "A"))), symtag, argv[i], "", NULL, NULL);
	    err += do_module (db, argv[i], TAG,
			      delete_flag ? "Untagging" : "Tagging",
			      rtag_proc, (char *) NULL, 0, 0, run_module_prog,
			      0, symtag);
	}
	close_module (db);
    }
    else
    {
	err = rtag_proc (argc + 1, argv - 1, NULL, NULL, NULL, 0, 0, NULL,
			 NULL);
    }
	lock_for_write = 0;

    Lock_Cleanup ();
    return (err);
}

/*
 * callback proc for doing the real work of tagging
 */
/* ARGSUSED */
static int rtag_proc(int argc, char **argv, const char *xwhere,
		      const char *mwhere, const char *mfile, int shorten,
		      int local_specified, const char *mname, const char *msg)
{
    /* Begin section which is identical to patch_proc--should this
       be abstracted out somehow?  */
    char *myargv[2];
    int err = 0;
    int which;
    char *repository;
    char *where;

    if (is_rtag)
    {
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

	{
	char *mapped_repository = map_repository(repository);

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
	}
	/* End section which is identical to patch_proc.  */

	    which = W_REPOS;
		repository = NULL;
    }
    else
    {
        where = NULL;
        which = W_LOCAL;
        repository = "";
    }

    if (numtag != NULL && !numtag_validated)
    {
	tag_check_valid (numtag, argc - 1, argv + 1, local, 0, repository);
	numtag_validated = 1;
    }

	/* check to make sure they are authorized to tag all the 
       specified files in the repository */

    mtlist = getlist();
    err = start_recursion (check_fileproc, check_filesdoneproc,
                           (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
                           argc - 1, argv + 1, local, which, 0, 1,
                           where, repository, 1, verify_tag, numtag);
    
    if (err)
    {
       error (1, 0, "correct the above errors first!");
    }
     
    /* It would be nice to provide consistency with respect to
       commits; however CVS lacks the infrastructure to do that (see
       Concurrency in cvs.texinfo and comment in do_recursion).  We
       do need to ensure that the RCS file info that gets read and
       cached in do_recursion isn't stale by the time we get around
       to using it to rewrite the RCS file in the callback, and this
       is the easiest way to accomplish that.  */
    lock_tree_for_write (argc - 1, argv + 1, local, which, 0);
	tag_set_ok = 0;

	current_date = date_from_time_t(global_session_time_t);
    /* start the recursion processor */
    err = start_recursion (is_rtag ? rtag_fileproc : tag_fileproc,
			   (FILESDONEPROC) NULL, (PREDIRENTPROC) NULL, tag_dirproc,
			   (DIRLEAVEPROC) NULL, NULL, argc - 1, argv + 1,
			   local, which, 0, 0, where, repository, 1, verify_tag, numtag);
	xfree(current_date);
    dellist (&mtlist);
    if (where != NULL)
	xfree (where);

	if(tag_set_ok)
	{
		/* This is rtag so we have a chance at setting val-tags */
		add_to_valtags(symtag);
	}

    return (err);
}

/* check file that is to be tagged */
/* All we do here is add it to our list */

static int check_fileproc (void *callerdat, struct file_info *finfo)
{
    const char *xdir;
    Node *p;
    Vers_TS *vers;
    
    if (check_uptodate) 
    {
		Ctype status = Classify_File (finfo, (char *) NULL, (char *) NULL,
						(char *) NULL, 1, 0, &vers, 0, 0, 0);

		if ((status != T_UPTODATE) && (status != T_CHECKOUT) &&
			(status != T_PATCH) && (status != T_REMOVE_ENTRY))
		{
			error (0, 0, "%s is locally modified", fn_root(finfo->fullname));
			freevers_ts (&vers);
			return (1);
		}
    }
    else
		vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0, 0);
    
    if (finfo->update_dir[0] == '\0')
	xdir = ".";
    else
	xdir = finfo->update_dir;
    if ((p = findnode_fn (mtlist, xdir)) != NULL)
    {
	tlist = ((struct master_lists *) p->data)->tlist;
    }
    else
    {
	struct master_lists *ml;
        
	tlist = getlist ();
	p = getnode ();
	p->key = xstrdup (xdir);
	p->type = UPDATE;
	ml = (struct master_lists *)
	    xmalloc (sizeof (struct master_lists));
	ml->tlist = tlist;
	p->data = (char *) ml;
	p->delproc = masterlist_delproc;
	(void) addnode (mtlist, p);
    }
    /* do tlist */
    p = getnode ();
    p->key = xstrdup (finfo->file);
    p->type = UPDATE;
    p->delproc = tag_delproc;
    if (vers->srcfile == NULL)
    {
        if (!really_quiet)
	    error (0, 0, "nothing known about %s", fn_root(finfo->file));
	freevers_ts (&vers);
	freenode (p);
	return (1);
    }

    /* Here we duplicate the calculation in tag_fileproc about which
       version we are going to tag.  There probably are some subtle races
       (e.g. numtag is "foo" which gets moved between here and
       tag_fileproc).  */
    if (!is_rtag && numtag == NULL && date == NULL)
	p->data = xstrdup (vers->vn_user);
    else
	{
	TRACE(3,"check_fileproc(0) rcs get version rcsfile=\"%s\" symtag=\"%s\"",
					PATCH_NULL(vers->srcfile->path),
					PATCH_NULL(numtag) );
	p->data = RCS_getversion (vers->srcfile, numtag, date,
				  force_tag_match, NULL);
	}
    if (p->data != NULL)
    {
        int addit = 1;
        char *oversion;
        
        oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1,
				   (int *) NULL);
        if (oversion == NULL) 
        {
            if (delete_flag)
            {
		/* Deleting a tag which did not exist is a noop and
		   should not be logged.  */
                addit = 0;
            }
        }
	else if (delete_flag)
	{
	    xfree (p->data);
	    p->data = xstrdup (oversion);
	}
        else if (strcmp(oversion, p->data) == 0)
        {
            addit = 0;
        }
        else if (!force_tag_move)
        {
            addit = 0;
        }
        if (oversion != NULL)
        {
            xfree(oversion);
        }
        if (!addit)
        {
            xfree(p->data);
            p->data = NULL;
        }
    }
    freevers_ts (&vers);
    (void) addnode (tlist, p);
    return (0);
}
                         
static int check_filesdoneproc (void *callerdat, int err, char *repos, char *update_dir, List *entries)
{
    int n;
    Node *p;

    p = findnode_fn(mtlist, update_dir);
    if (p != NULL)
    {
        tlist = ((struct master_lists *) p->data)->tlist;
    }
    else
    {
        tlist = (List *) NULL;
    }
    if ((tlist == NULL) || (tlist->list->next == tlist->list))
    {
        return (err);
    }

	pretag_params_t args;
	args.directory=Short_Repository(repos);
	args.message=message;
	args.action=delete_flag?"del":force_tag_move?"mov":"add";
	args.tag=symtag;
	args.tag_type=delete_flag?'?':branch_mode?'T':'N';

	TRACE(3,"run pretag proc");
	if ((n = run_trigger(&args, pretag_proc)) > 0)
    {
        error (0, 0, "Pre-tag check failed");
        err += n;
    }
    return (err);
}

static int pretag_proc(void *params, const trigger_interface *cb)
{
	pretag_params_t *args = (pretag_params_t *)params;
	int ret;

	TRACE(1,"pretag_proc(%s,%s,%s,%c)",PATCH_NULL(args->directory),PATCH_NULL(args->action),PATCH_NULL(args->tag),args->tag_type);

	if(cb->pretag)
	{
		pretag_list_size=pretag_list_count=0;
		walklist(tlist, pretag_list_proc, NULL);
		ret = cb->pretag(cb,args->message,args->directory,pretag_list_count,pretag_list,pretag_version_list,args->tag_type,args->action,args->tag);
		xfree(pretag_list);
		xfree(pretag_version_list);
	}
			
	return ret;
}

static void masterlist_delproc(Node *p)
{
    struct master_lists *ml;

    ml = (struct master_lists *)p->data;
    dellist(&ml->tlist);
    xfree(ml);
    return;
}

static void tag_delproc(Node *p)
{
    if (p->data != NULL)
    {
        xfree(p->data);
        p->data = NULL;
    }
    return;
}

static int pretag_list_proc(Node *p, void *closure)
{
    if (p->data != NULL)
    {
		int pos = pretag_list_count++;
		if(pos==pretag_list_size)
		{
			if(pretag_list_size<64) pretag_list_size=64;
			else pretag_list_size*=2;
			pretag_list=(const char**)xrealloc((void*)pretag_list,pretag_list_size*sizeof(char*));
			pretag_version_list=(const char**)xrealloc((void*)pretag_version_list,pretag_list_size*sizeof(char*));
		}
		pretag_list[pos]=p->key;
		pretag_version_list[pos]=p->data;
	}
    return (0);
}


/*
 * Called to rtag a particular file, as appropriate with the options that were
 * set above.
 */
/* ARGSUSED */
static int rtag_fileproc (void *callerdat, struct file_info *finfo)
{
    RCSNode *rcsfile;
    char *version, *rev;
    int retcode = 0;

    TRACE(3,"rtag_fileproc");
    /* find the parsed RCS data */
    if ((rcsfile = finfo->rcs) == NULL)
		return 1;

    /*
     * For tagging an RCS file which is a symbolic link, you'd best be
     * running with RCS 5.6, since it knows how to handle symbolic links
     * correctly without breaking your link!
     */

    if (delete_flag)
		return rtag_delete (rcsfile);

	if(numtag && !date && alias_branch)
	{
		if(!isdigit(numtag[0]) && RCS_isfloating(rcsfile,numtag))
		{
			cvs_output ("W ", 2);
			cvs_output (fn_root(rcsfile->path), 0);
			cvs_output (" : ", 0);
			cvs_output (numtag, 0);
			cvs_output (" is floating branch", 0);
			cvs_output (" : NOT CREATING ALIAS", 0);
			cvs_output ("\n", 1);
			return 0;
		}
		version = RCS_tag2rev(rcsfile,numtag);
		if(!strcmp(numtag,"HEAD")) // for HEAD
			*strrchr(version,'.')='\0';
	}
	else
	{
		TRACE(3,"rtag_fileproc(0) rcs get version rcsfile=\"%s\" numtag=\"%s\", force=%d",
					PATCH_NULL(rcsfile->path),
					PATCH_NULL(numtag), force_tag_match );
		version = RCS_getversion(rcsfile,
								numtag,
								date,
								force_tag_match, (int *) NULL);
	}
	if (version == NULL)
	{
		/* Clean up any old tags */
		rtag_delete (rcsfile);

		if (!quiet && !force_tag_match)
		{
			error (0, 0, "cannot find tag `%s' in `%s'",
			numtag ? numtag : "head", fn_root(rcsfile->path));
			return (1);
		}
		return (0);
	}

    if (numtag	&& isdigit ((unsigned char) *numtag) && strcmp (numtag, version) != 0)
    {
		/*
		* We didn't find a match for the numeric tag that was specified, but
		* that's OK.  just pass the numeric tag on to rcs, to be tagged as
		* specified.  Could get here if one tried to tag "1.1.1" and there
		* was a 1.1.1 branch with some head revision.  In this case, we want
		* the tag to reference "1.1.1" and not the revision at the head of
		* the branch.  Use a symbolic tag for that.
		*/
		if(floating_branch)
			sprintf(strrchr(version,'.'),".0");
		rev = (!alias_branch && branch_mode) ? RCS_magicrev (rcsfile, version) : version; // was : numtag
		retcode = RCS_settag(rcsfile, symtag, numtag, current_date);
		if (retcode == 0)
		{
			TRACE(3,"rtag_fileproc(1) rewrite rcsfile symtag=\"%s\", rev=\"%s\", date=\"%s\"",
					PATCH_NULL(symtag),
					PATCH_NULL(numtag),
					PATCH_NULL(current_date) );
			RCS_rewrite (rcsfile, NULL, NULL, 0);
			tag_set_ok = 1;
		}
    }
    else
    {
	char *oversion;
       
	/*
	 * As an enhancement for the case where a tag is being re-applied to
	 * a large body of a module, make one extra call to RCS_getversion to
	 * see if the tag is already set in the RCS file.  If so, check to
	 * see if it needs to be moved.  If not, do nothing.  This will
	 * likely save a lot of time when simply moving the tag to the
	 * "current" head revisions of a module -- which I have found to be a
	 * typical tagging operation.
	 */
	if(floating_branch)
		sprintf(strrchr(version,'.'),".0");
    rev = (!alias_branch && branch_mode) ? RCS_magicrev (rcsfile, version) : version;
	TRACE(3,"rtag_fileproc(4) rcs get version rcsfile=\"%s\" symtag=\"%s\", force=%d",
					PATCH_NULL(rcsfile->path),
					PATCH_NULL(symtag), 1 );
	oversion = RCS_getversion (rcsfile, symtag, (char *) NULL, 1,
				   (int *) NULL);
	if (oversion != NULL)
	{
	    int isbranch = RCS_nodeisbranch (finfo->rcs, symtag);

	    /*
	     * if versions the same and neither old or new are branches don't
	     * have to do anything
	     */
	    if (strcmp (version, oversion) == 0 && !branch_mode && !isbranch)
	    {
		xfree (oversion);
		xfree (version);
		return (0);
	    }
	  
	    if (!force_tag_move || (isbranch && !move_branch_tag))
	    {
		/* we're NOT going to move the tag */
		(void) printf ("W %s", fn_root(finfo->fullname));

		(void) printf (" : %s already exists on %s %s", 
			       symtag, isbranch ? "branch" : "version",
			       oversion);
		(void) printf (" : NOT MOVING tag to %s %s\n", 
			       branch_mode ? "branch" : "version", rev);
		xfree (oversion);
		xfree (version);
		return (0);
	    }
	    xfree (oversion);
	}
	retcode = RCS_settag(rcsfile, symtag, rev, current_date);
	if (retcode == 0)
	{
		TRACE(3,"rtag_fileproc(2) rewrite rcsfile symtag=\"%s\", rev=\"%s\", date=\"%s\"",
					PATCH_NULL(symtag),
					PATCH_NULL(rev),
					PATCH_NULL(current_date) );
	    RCS_rewrite (rcsfile, NULL, NULL, 0);
		tag_set_ok = 1;
	}
    }

    if (retcode != 0)
    {
	error (1, retcode == -1 ? errno : 0,
	       "failed to set tag `%s' to revision `%s' in `%s'",
	       symtag, rev, fn_root(rcsfile->path));
        if (branch_mode)
	    xfree (rev);
        xfree (version);
        return (1);
    }
	else
		tag_set_ok = 1;
    if (branch_mode)
		xfree (rev);
    xfree (version);
    TRACE(3,"rtag_fileproc return 0");
    return (0);
}

/*
 * If -d is specified, "force_tag_match" is set, so that this call to
 * RCS_getversion() will return a NULL version string if the symbolic
 * tag does not exist in the RCS file.
 * 
 * If the -r flag was used, numtag is set, and we only delete the
 * symtag from files that have numtag.
 * 
 * This is done here because it's MUCH faster than just blindly calling
 * "rcs" to remove the tag... trust me.
 */
static int rtag_delete (RCSNode *rcsfile)
{
    char *version;
    int retcode;

	TRACE(3,"rtag_delete()");
    if (numtag)
    {
	version = RCS_getversion (rcsfile, numtag, (char *) NULL, 1,
				  (int *) NULL);
	if (version == NULL)
	    return (0);
	xfree (version);
    }

    version = RCS_getversion (rcsfile, symtag, (char *) NULL, 1,
			      (int *) NULL);
    if (version == NULL)
	return (0);
    xfree (version);

    {
	int isbranch = RCS_nodeisbranch (rcsfile, symtag);

	if (isbranch && !move_branch_tag)
	{
	    /* we're NOT going to delete the tag */
	    cvs_output ("W ", 2);
	    cvs_output (fn_root(rcsfile->path), 0);
	    cvs_output (" : ", 0);
	    cvs_output (symtag, 0);
	    cvs_output (" is branch tag", 0);
	    cvs_output (" : NOT DELETING", 0);
	    cvs_output ("\n", 1);
	    return (0);
	}
    }

    if ((retcode = RCS_deltag(rcsfile, symtag)) != 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "failed to remove tag `%s' from `%s'", symtag,
		   fn_root(rcsfile->path));
	return (1);
    }
	TRACE(3,"rtag_delete(2) rewrite rcsfile");
    RCS_rewrite (rcsfile, NULL, NULL, 0);
    return (0);
}


/*
 * Called to tag a particular file (the currently checked out version is
 * tagged with the specified tag - or the specified tag is deleted).
 */
/* ARGSUSED */
static int tag_fileproc (void *callerdat, struct file_info *finfo)
{
    char *version, *oversion;
    char *nversion = NULL;
    char *rev;
    Vers_TS *vers;
    int retcode = 0;

    TRACE(3,"tag_fileproc");
    vers = Version_TS (finfo, NULL, NULL, NULL, 0, 0, 0);

    TRACE(3,"tag_fileproc - Version_TS complete");
    if ((numtag != NULL) || (date != NULL))
    {
	    TRACE(3,"tag_fileproc - ((numtag != NULL) || (date != NULL)");
		if(numtag && !date && alias_branch)
		{
		    TRACE(3,"tag_fileproc - numtag && !date && alias_branch");
			if(!isdigit(numtag[0]) && RCS_isfloating(vers->srcfile,numtag))
			{
			    TRACE(3,"tag_fileproc - !isdigit(numtag[0]) && RCS_isfloating(vers->srcfile,numtag)");
				cvs_output ("W ", 2);
				cvs_output (fn_root(vers->srcfile->path), 0);
				cvs_output (" : ", 0);
				cvs_output (numtag, 0);
				cvs_output (" is floating branch", 0);
				cvs_output (" : NOT CREATING ALIAS", 0);
				cvs_output ("\n", 1);
				freevers_ts (&vers);
				return 0;
			}
			TRACE(3,"tag_fileproc() - RCS_tag2rev(%s,%s)",PATCH_NULL(vers->srcfile->path),PATCH_NULL(numtag));
			nversion = RCS_tag2rev(vers->srcfile,numtag);
			if(!strcmp(numtag,"HEAD")) // for HEAD
			{
				TRACE(3,"tag_fileproc() - numtag is HEAD");
				*strrchr(nversion,'.')='\0';
			}
		}
		else
		{
			int userevone=0;




			int force_tag_match_now=force_tag_match;
			int this_is_repoversion=0;
			TRACE(3,"tag_fileproc() - RCS_getversion(%s,%s,%s,force=%d)",
						PATCH_NULL(vers->srcfile->path),
						PATCH_NULL(numtag),
						PATCH_NULL(date),
						force_tag_match);
			if ((numtag!=NULL) && (userevone!=0) && (strstr(vers->srcfile->path, RCSREPOVERSION)!=NULL))
			{
				force_tag_match_now=1;
				this_is_repoversion=1;
			}
			nversion = RCS_getversion(vers->srcfile,
									numtag,
									date,
									force_tag_match_now,
					(int *) NULL);
 			if ((this_is_repoversion) && (userevone!=0) && (nversion == NULL))
 			{
 				TRACE(3,"tag_fileproc() - use version 1.1 in this case");
 				nversion = RCS_getversion(vers->srcfile,
 									"1.1",
 									date,
  									force_tag_match,
  					(int *) NULL);
 			}
 			TRACE(3,"tag_fileproc() - nversion \"%s\"",PATCH_NULL(nversion));
		}
        if (nversion == NULL)
        {
	    freevers_ts (&vers);
            return (0);
        }
    }
	else
		TRACE(3,"tag_fileproc() - numtag == NULL && date == NULL");

    if (delete_flag)
    {
		TRACE(3,"tag_fileproc() - delete_flag");

	TRACE(3,"* If -d is specified, \"force_tag_match\" is set, so that this call to");
	TRACE(3,"* RCS_getversion() will return a NULL version string if the symbolic");
	TRACE(3,"* tag does not exist in the RCS file.");
	TRACE(3,"* ");
	TRACE(3,"* This is done here because it's MUCH faster than just blindly calling");
	TRACE(3,"* \"rcs\" to remove the tag... trust me.");
	/*
	 * If -d is specified, "force_tag_match" is set, so that this call to
	 * RCS_getversion() will return a NULL version string if the symbolic
	 * tag does not exist in the RCS file.
	 * 
	 * This is done here because it's MUCH faster than just blindly calling
	 * "rcs" to remove the tag... trust me.
	 */

	TRACE(3,"tag_fileproc(0) rcs get version rcsfile=\"%s\" symtag=\"%s\"",
					PATCH_NULL(vers->srcfile->path),
					PATCH_NULL(symtag) );
	version = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1,
				  (int *) NULL);
	if (version == NULL || vers->srcfile == NULL)
	{
		TRACE(3,"tag_fileproc(0) rcs get version returned NULL version or scrfile NULL");
	    freevers_ts (&vers);
	    return (0);
	}
	xfree (version);
    {
	int isbranch = RCS_nodeisbranch (vers->srcfile, symtag);

	if (isbranch && !move_branch_tag)
	{
	    /* we're NOT going to delete the tag */
	    cvs_output ("W ", 2);
	    cvs_output (fn_root(vers->srcfile->path), 0);
	    cvs_output (" : ", 0);
	    cvs_output (symtag, 0);
	    cvs_output (" is branch tag", 0);
	    cvs_output (" : NOT DELETING", 0);
	    cvs_output ("\n", 1);
	    freevers_ts (&vers);
	    return (0);
	}
    }

    TRACE(3,"tag_fileproc - RCS_deltag");
	if ((retcode = RCS_deltag(vers->srcfile, symtag)) != 0) 
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag %s from %s", symtag,
		       fn_root(vers->srcfile->path));
	    freevers_ts (&vers);
	    return (1);
	}
	TRACE(3,"tag_fileproc(1) rewrite rcsfile=\"%s\" symtag=\"%s\"",
					PATCH_NULL(vers->srcfile->path),
					PATCH_NULL(symtag) );
	RCS_rewrite (vers->srcfile, NULL, NULL, 0);

	/* warm fuzzies */
	if (!really_quiet)
	{
	    cvs_output ("D ", 2);
	    cvs_output (fn_root(finfo->fullname), 0);
	    cvs_output ("\n", 1);
	}

	freevers_ts (&vers);
	TRACE(3,"tag_fileproc - return 0 (#1)");
	return (0);
    }

    /*
     * If we are adding a tag, we need to know which version we have checked
     * out and we'll tag that version.
     */
	TRACE(3,"* If we are adding a tag, we need to know which version we have checked" );
	TRACE(3,"* out and we'll tag that version." );
    if (nversion == NULL)
    {
		TRACE(3,"tag_fileproc() - nversion == NULL" );
		if(!strcmp(finfo->file,RCSREPOVERSION))
		{
			TRACE(3,"tag_fileproc() - use vers->vn_rcs" );
			version = vers->vn_rcs;
		}
		else
		{
			TRACE(3,"tag_fileproc() - use vers->vn_user" );
			version = vers->vn_user;
		}
    }
    else
    {
		TRACE(3,"tag_fileproc() - nversion != NULL" );
        version = nversion;
    }
	TRACE(3,"tag_fileproc() - version = \"%s\".", PATCH_NULL(version) );
    if (version == NULL)
    {
	TRACE(3,"tag_fileproc - nversion still == NULL");
	freevers_ts (&vers);
	return (0);
    }
    else if (strcmp (version, "0") == 0)
    {
	TRACE(3,"tag_fileproc - uncommitted file");
	if (!quiet)
	    error (0, 0, "couldn't tag added but un-commited file `%s'", finfo->file);
	freevers_ts (&vers);
	return (0);
    }
    else if (version[0] == '-')
    {
	TRACE(3,"tag_fileproc - removed but un-committed file");
	if (!quiet)
	    error (0, 0, "skipping removed but un-commited file `%s'", finfo->file);
	freevers_ts (&vers);
	return (0);
    }
    else if (vers->srcfile == NULL)
    {
	TRACE(3,"tag_fileproc - cannot file file ! (renamed?)");
	if (!quiet)
	    error (0, 0, "cannot find revision control file for `%s'", finfo->file);
	freevers_ts (&vers);
	return (0);
    }

    /*
     * As an enhancement for the case where a tag is being re-applied to a
     * large number of files, make one extra call to RCS_getversion to see
     * if the tag is already set in the RCS file.  If so, check to see if it
     * needs to be moved.  If not, do nothing.  This will likely save a lot of
     * time when simply moving the tag to the "current" head revisions of a
     * module -- which I have found to be a typical tagging operation.
     */
	if(floating_branch)
		sprintf(strrchr(version,'.'),".0");
	TRACE(3,"tag_fileproc - RCS_magicrev");
    rev = (!alias_branch && branch_mode) ? RCS_magicrev (vers->srcfile, version) : version;
	TRACE(3,"tag_fileproc(4) rcs get version rcsfile=\"%s\" symtag=\"%s\"",
					PATCH_NULL(vers->srcfile->path),
					PATCH_NULL(symtag) );
    oversion = RCS_getversion (vers->srcfile, symtag, (char *) NULL, 1,
			       (int *) NULL);
    if (oversion != NULL)
    {
	TRACE(3,"tag_fileproc - oversion != NULL");
	int isbranch = RCS_nodeisbranch (finfo->rcs, symtag);
	TRACE(3,"tag_fileproc() - isbranch=%d",isbranch);

	/*
	 * if versions the same and neither old or new are branches don't have 
	 * to do anything
	 */
	if (strcmp (version, oversion) == 0 && !branch_mode && !isbranch)
	{
	    xfree (oversion);
	    if (branch_mode)
		xfree (rev);
	    freevers_ts (&vers);
	    return (0);
	}

	if (!force_tag_move || (isbranch && !move_branch_tag))
	{
		TRACE(3,"tag_fileproc - !force_tag_move || (isbranch && !move_branch_tag)");
	    /* we're NOT going to move the tag */
	    cvs_output ("W ", 2);
	    cvs_output (fn_root(finfo->fullname), 0);
	    cvs_output (" : ", 0);
	    cvs_output (symtag, 0);
	    cvs_output (" already exists on ", 0);
	    cvs_output (isbranch ? "branch" : "version", 0);
	    cvs_output (" ", 0);
	    cvs_output (oversion, 0);
	    cvs_output (" : NOT MOVING tag to ", 0);
	    cvs_output (branch_mode ? "branch" : "version", 0);
	    cvs_output (" ", 0);
	    cvs_output (rev, 0);
	    cvs_output ("\n", 1);
	    xfree (oversion);
	    if (branch_mode)
		xfree (rev);
	    freevers_ts (&vers);
	    return (0);
	}
	xfree (oversion);
    }

	TRACE(3,"tag_fileproc - finally RCS_settag");
    if ((retcode = RCS_settag(vers->srcfile, symtag, rev, current_date)) != 0)
    {
	error (1, retcode == -1 ? errno : 0,
	       "failed to set tag %s to revision %s in %s",
	       symtag, rev, fn_root(vers->srcfile->path));
	if (branch_mode)
	    xfree (rev);
	freevers_ts (&vers);
	return (1);
    }
	else
		tag_set_ok = 1;
	TRACE(3,"tag_fileproc - finally RCS_settag ok");
    if (branch_mode)
	xfree (rev);
	TRACE(3,"tag_fileproc(2) rewrite rcsfile=\"%s\" symtag=\"%s\", rev=\"%s\", date=\"%s\"",
					PATCH_NULL(vers->srcfile->path),
					PATCH_NULL(symtag),
					PATCH_NULL(rev),
					PATCH_NULL(current_date) );
    RCS_rewrite (vers->srcfile, NULL, NULL, 0);

    /* more warm fuzzies */
    if (!really_quiet)
    {
	cvs_output ("T ", 2);
	cvs_output (fn_root(finfo->fullname), 0);
	cvs_output ("\n", 1);
    }

    if (nversion != NULL)
    {
        xfree (nversion);
    }
    freevers_ts (&vers);
    TRACE(3,"tag_fileproc return 0");
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype tag_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

    TRACE(3,"tag_dirproc");
    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	  error (0, 0, "Ignoring %s", update_dir);
        return R_SKIP_ALL;
    }

    if (!quiet)
		error (0, 0, "%s %s", delete_flag ? "Untagging" : "Tagging", update_dir);

	{
		struct file_info finfo;

	    TRACE(3,"tag_dirproc - get_directory_finfo()");
		if(!get_directory_finfo(repos,dir,update_dir,&finfo))
		{
			if(is_rtag)
				rtag_fileproc(NULL,&finfo);
			else
				tag_fileproc(NULL,&finfo);
		    TRACE(3,"tag_dirproc - get_directory_finfo() done, now dellist");
			dellist(&finfo.entries);
		}
	}
    TRACE(3,"tag_dirproc return");
    return (R_PROCESS);
}

/* Code relating to the val-tags file.  Note that this file has no way
   of knowing when a tag has been deleted.  The problem is that there
   is no way of knowing whether a tag still exists somewhere, when we
   delete it some places.  Using per-directory val-tags files (in
   CVSREP) might be better, but that might slow down the process of
   verifying that a tag is correct (maybe not, for the likely cases,
   if carefully done), and/or be harder to implement correctly.  */

struct val_args {
    const char *name;
    int found;
};

static int val_fileproc(void *callerdat, struct file_info *finfo)
{
    RCSNode *rcsdata;
    struct val_args *args = (struct val_args *)callerdat;
    char *tag;

    if ((rcsdata = finfo->rcs) == NULL)
	/* Not sure this can happen, after all we passed only
	   W_REPOS */
	return 0;

    tag = RCS_gettag (rcsdata, args->name, 1, (int *) NULL);
    if (tag != NULL)
    {
	/* FIXME: should find out a way to stop the search at this point.  */
	args->found = 1;
	xfree (tag);
	return 1;
    }
    return 0;
}

static Dtype
val_direntproc (void *callerdat, char *dir, char *repository, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
    /* This is not quite right--it doesn't get right the case of "cvs
       update -d -r foobar" where foobar is a tag which exists only in
       files in a directory which does not exist yet, but which is
       about to be created.  */
	struct val_args *the_val_args = (struct val_args *)callerdat;

    TRACE(3,"val_direntproc");
	if(the_val_args->found)
		return R_SKIP_ALL;
    TRACE(3,"return val_direntproc");
	return hint;
}

/* Check to see whether NAME is a valid tag.  If so, return.  If not
   print an error message and exit.  ARGC, ARGV, LOCAL, and AFLAG specify
   which files we will be operating on.

   REPOSITORY is the repository if we need to cd into it, or NULL if
   we are already there, or "" if we should do a W_LOCAL recursion.
   Sorry for three cases, but the "" case is needed in case the
   working directories come from diverse parts of the repository, the
   NULL case avoids an unneccesary chdir, and the non-NULL, non-""
   case is needed for checkout, where we don't want to chdir if the
   tag is found in CVSROOTADM_VALTAGS, but there is not (yet) any
   local directory.  */
void tag_check_valid (const char *name, int argc, char **argv, int local, int aflag, const char *repository)
{
    DBM *db;
    char *valtags_filename;
    int err;
    int nowrite = 0;
    datum mytag;
    struct val_args the_val_args;
    struct saved_cwd cwd;
    int which;

    /* Numeric tags require only a syntactic check.  */
    if (isdigit ((unsigned char) name[0]))
    {
		const char *p;
		for (p = name; *p != '\0'; ++p)
		{
			if (!(isdigit ((unsigned char) *p) || *p == '.'))
				error (1, 0, "Numeric tag %s contains characters other than digits and '.'", name);
		}
		return;
    }

    /* Special tags are always valid.  */
    if (strcmp (name, TAG_BASE) == 0
	|| strcmp (name, TAG_HEAD) == 0)
	return;


    /* FIXME: This routine doesn't seem to do any locking whatsoever
       (and it is called from places which don't have locks in place).
       If two processes try to write val-tags at the same time, it would
       seem like we are in trouble.  */

    mytag.dptr = (char*)name;
    mytag.dsize = strlen (name);

    valtags_filename = (char*)xmalloc (strlen (current_parsed_root->directory)
				+ sizeof CVSROOTADM
				+ sizeof CVSROOTADM_VALTAGS + 3);
    sprintf (valtags_filename, "%s/%s/%s", current_parsed_root->directory,
					   CVSROOTADM, CVSROOTADM_VALTAGS);
    db = dbm_open (valtags_filename, O_RDWR, 0666);
    if (db == NULL)
    {
	if (!existence_error (errno))
	{
	    error (0, errno, "warning: cannot open %s read/write",
		   valtags_filename);
	    db = dbm_open (valtags_filename, O_RDONLY, 0666);
	    if (db != NULL)
		nowrite = 1;
	    else if (!existence_error (errno))
		error (1, errno, "cannot read %s", valtags_filename);
	}
	/* If the file merely fails to exist, we just keep going and create
	   it later if need be.  */
    }
    if (db != NULL)
    {
	datum val;

	val = dbm_fetch (db, mytag);
	if (val.dptr != NULL)
	{
	    /* Found.  The tag is valid.  */
	    dbm_close (db);
	    xfree (valtags_filename);
	    return;
	}
	/* FIXME: should check errors somehow (add dbm_error to myndbm.c?).  */
    }

    // val-tags sucks.  If someone mistypes a tag what does it matter?
    // we can catch it at the end and warn them.  Scanning the entire
    // repository for a flippin' tag is just stupid.
    TRACE(3,"Tag %s not found in val-tags, but we don't care any more.", mytag);
    xfree(valtags_filename);
    return;

    /* We didn't find the tag in val-tags, so look through all the RCS files
       to see whether it exists there.  Yes, this is expensive, but there
       is no other way to cope with a tag which might have been created
       by an old version of CVS, from before val-tags was invented.

       Since we need this code anyway, we also use it to create
       entries in val-tags in general (that is, the val-tags entry
       will get created the first time the tag is used, not when the
       tag is created).  */

    the_val_args.name = name;
    the_val_args.found = 0;

    which = W_REPOS;

    if (repository != NULL)
    {
	if (repository[0] == '\0')
	    which |= W_LOCAL;
	else
	{
	    if (save_cwd (&cwd))
		error_exit ();
	    if ( CVS_CHDIR (repository) < 0)
		error (1, errno, "cannot change to %s directory", fn_root(repository));
	}
    }

    err = start_recursion (val_fileproc, (FILESDONEPROC) NULL,
			   (PREDIRENTPROC) NULL, val_direntproc, (DIRLEAVEPROC) NULL,
			   (void *)&the_val_args,
			   argc, argv, local, which, aflag,
			   1, NULL, NULL, 1, NULL,NULL);
    if (repository != NULL && repository[0] != '\0')
    {
	if (restore_cwd (&cwd, NULL))
	    exit (EXIT_FAILURE);
	free_cwd (&cwd);
    }

    if (!the_val_args.found)
		error (1, 0, "no such tag %s", name);
    else
    {
	/* The tags is valid but not mentioned in val-tags.  Add it.  */
	datum value;

	if (noexec || nowrite)
	{
	    if (db != NULL)
		dbm_close (db);
	    xfree (valtags_filename);
	    return;
	}

	if (db == NULL)
	{
	    mode_t omask;
	    omask = umask (cvsumask);
	    db = dbm_open (valtags_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
	    (void) umask (omask);

	    if (db == NULL)
	    {
		error (0, errno, "warning: cannot create %s", valtags_filename);
		xfree (valtags_filename);
		return;
	    }
	}
	value.dptr = "y";
	value.dsize = 1;
	if (dbm_store (db, mytag, value, DBM_REPLACE) < 0)
	    error (0, errno, "cannot store %s into %s", name,
		   valtags_filename);
	dbm_close (db);
    }
    xfree (valtags_filename);
}

/*
 * Check whether a join tag is valid.  This is just like
 * tag_check_valid, but we must stop before the colon if there is one.
 */

void tag_check_valid_join (const char *join_tag, int argc, char **argv, int local, int aflag, const char *repository)
{
    char *c, *s;

    c = xstrdup (join_tag);
    s = strchr (c, ':');
    if (s != NULL)
    {
        if (isdigit ((unsigned char) join_tag[0]))
	    error (1, 0,
		   "Numeric join tag %s may not contain a date specifier",
		   join_tag);

        *s = '\0';
	/* hmmm...  I think it makes sense to allow -j:<date>, but
	 * for now this fixes a bug where CVS just spins and spins (I
	 * think in the RCS code) looking for a zero length tag.
	 */
	if (!*c)
	    error (1, 0,
		   "argument to join may not contain a date specifier without a tag");
    }

    tag_check_valid (c, argc, argv, local, aflag, repository);

    xfree (c);
}

int add_to_valtags(char *name)
{
    DBM *db;
    char *valtags_filename;
    int nowrite = 0;
    datum mytag;

    /* Ignore numeric tags */
    if (isdigit ((unsigned char) name[0]))
		return 0;

    /* Special tags are always valid.  */
    if (strcmp (name, TAG_BASE) == 0 || strcmp (name, TAG_HEAD) == 0)
		return 0;

    /* FIXME: This routine doesn't seem to do any locking whatsoever
       (and it is called from places which don't have locks in place).
       If two processes try to write val-tags at the same time, it would
       seem like we are in trouble.  */

    mytag.dptr = name;
    mytag.dsize = strlen (name);

    valtags_filename = (char*)xmalloc (strlen (current_parsed_root->directory)
				+ sizeof CVSROOTADM
				+ sizeof CVSROOTADM_VALTAGS + 3);
    sprintf (valtags_filename, "%s/%s/%s", current_parsed_root->directory,
					   CVSROOTADM, CVSROOTADM_VALTAGS);
    db = dbm_open (valtags_filename, O_RDWR, 0666);
    if (db == NULL)
    {
	if (!existence_error (errno))
	{
	    error (0, errno, "warning: cannot open %s read/write",
		   valtags_filename);
	    db = dbm_open (valtags_filename, O_RDONLY, 0666);
	    if (db != NULL)
		nowrite = 1;
	    else if (!existence_error (errno))
		error (1, errno, "cannot read %s", valtags_filename);
	}
	/* If the file merely fails to exist, we just keep going and create
	   it later if need be.  */
    }
    if (db != NULL)
    {
	datum val;

	val = dbm_fetch (db, mytag);
	if (val.dptr != NULL)
	{
	    /* Found.  The tag is valid.  */
	    dbm_close (db);
	    xfree (valtags_filename);
	    return 0;
	}
	/* FIXME: should check errors somehow (add dbm_error to myndbm.c?).  */
    }

	{
	/* The tags is valid but not mentioned in val-tags.  Add it.  */
	datum value;

	if (noexec || nowrite)
	{
	    if (db != NULL)
		dbm_close (db);
	    xfree (valtags_filename);
	    return 0;
	}

	if (db == NULL)
	{
	    mode_t omask;
	    omask = umask (cvsumask);
	    db = dbm_open (valtags_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
	    (void) umask (omask);

	    if (db == NULL)
	    {
		error (0, errno, "warning: cannot create %s", valtags_filename);
		xfree (valtags_filename);
		return 0;
	    }
	}
	value.dptr = "y";
	value.dsize = 1;
	if (dbm_store (db, mytag, value, DBM_REPLACE) < 0)
	    error (0, errno, "cannot store %s into %s", name,
		   valtags_filename);
	dbm_close (db);
    }
    xfree (valtags_filename);
	return 0;
}
