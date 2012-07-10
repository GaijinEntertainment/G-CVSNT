/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Add
 * 
 * Adds a file or directory to the RCS source repository.  For a file,
 * the entry is marked as "needing to be added" in the user's own CVS
 * directory, and really added to the repository when it is committed.
 * For a directory, it is added at the appropriate place in the source
 * repository and a CVS directory is generated within the directory.
 * 
 * The -m option is currently the only supported option.  Some may wish to
 * supply standard "rcs" options here, but I've found that this causes more
 * trouble than anything else.
 * 
 * The user files or directories must already exist.  For a directory, it must
 * not already have a CVS file in it.
 * 
 * An "add" on a file that has been "remove"d but not committed will cause the
 * file to be resurrected.
 */

#include "cvs.h"
#include "savecwd.h"
#include "fileattr.h"

static int add_directory(struct file_info *finfo);
static int build_entry(const char *repository, const char *user, const char *options,
		        const char *message, List * entries, const char *tag);


/* From update */
int checkout_file(struct file_info *finfo, Vers_TS *vers_ts,
				 int adding, int merging, int update_server, int resurrect, int is_rcs, const char *merge_rev1, const char *merge_rev2);

static const char *const add_usage[] =
{
    "Usage: %s %s [-k rcs-kflag] [-m message] files...\n",
	"\t-b <bugid>\tSet the bug identifier (repeat for multiple bugs).\n",
    "\t-k\tUse \"rcs-kflag\" to add the file with the specified kflag.\n",
    "\t-m\tUse \"message\" for the creation log.\n",
	"\t-r <branch>\tAdd onto a different branch.\n",
    "\t-f\tForce replacement of deleted files.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static cvs::string bugid;
static char *branch;

int add (int argc, char **argv)
{
    char *message = NULL;
    int i;
    char *repository;
    int c;
    int err = 0;
    int added_files = 0;
    int force_add = 0;
    char *options = NULL;
    List *entries;
    Vers_TS *vers;
    struct saved_cwd cwd;
    /* Nonzero if we found a slash, and are thus adding files in a
       subdirectory.  */
    int found_slash = 0;
    size_t cvsroot_len;
	const char *msg;
	const char *mapped_repository;

    if (argc == 1 || argc == -1)
	usage (add_usage);

    /* parse args */
    optind = 0;
	while ((c = getopt (argc, argv, "+b:k:m:r:f")) != -1)
    {
	switch (c)
	{
	    case 'k':
		if (options)
		    xfree (options);
		if(server_active && compat[compat_level].ignore_client_wrappers)
			break;
		options = RCS_check_kflag (optarg,true,true);
		break;

	    case 'm':
		message = xstrdup (optarg);
		break;
		case 'b':
		if(bugid.size())
			bugid+=",";
		bugid += optarg;
		break;
		case 'r':
		branch = xstrdup(optarg);
		break;
		case 'f':
		force_add = 1;
		break;
	    case '?':
	    default:
		usage (add_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

	if(branch && !RCS_check_tag(branch,true,false,false))
	{
		error(1,0,"Branch name '%s' is not valid.",branch);
	}

    if (argc <= 0)
	usage (add_usage);

    cvsroot_len = strlen (current_parsed_root->directory);

    /* First some sanity checks.  I know that the CVS case is (sort of)
       also handled by add_directory, but we need to check here so the
       client won't get all confused in send_file_names.  */
    for (i = 0; i < argc; i++)
    {
	int skip_file = 0;

	/* If it were up to me I'd probably make this a fatal error.
	   But some people are really fond of their "cvs add *", and
	   don't seem to object to the warnings.
	   Whatever.  */
	strip_trailing_slashes (argv[i]);
	if (strcmp (argv[i], ".") == 0
	    || strcmp (argv[i], "..") == 0
	    || fncmp (argv[i], CVSADM) == 0 
		|| fncmp (argv[i], CVSDUMMY) == 0)
	{
	    if (!quiet)
		error (0, 0, "cannot add special file `%s'; skipping", argv[i]);
	    skip_file = 1;
	}
	else
	{
	    char *p;
	    p = argv[i];
	    while (*p != '\0')
	    {
		if (ISDIRSEP (*p))
		{
		    found_slash = 1;
		    break;
		}
		++p;
	    }
	}

	if (skip_file)
	{
	    int j;

	    /* FIXME: We don't do anything about free'ing argv[i].  But
	       the problem is that it is only sometimes allocated (see
	       cvsrc.c).  */

		char *tmp = argv[i];
	    for (j = i; j < argc - 1; ++j)
		argv[j] = argv[j + 1];
		argv[j]=tmp;
	    --argc;
	    /* Check the new argv[i] again.  */
	    --i;
	    ++err;
	}
    }

    if (current_parsed_root->isremote)
    {
	int i;    

	/* Call expand_wild so that the local removal of files will
           work.  It's ok to do it always because we have to send the
           file names expanded anyway.  */
	expand_wild (argc, argv, &argc, &argv);

	if (argc == 0)
	    /* We snipped out all the arguments in the above sanity
	       check.  We can just forget the whole thing (and we
	       better, because if we fired up the server and passed it
	       nothing, it would spit back a usage message).  */
	    return err;

	if (options && options[0])
	{
	    option_with_arg("-k", options);
	    xfree (options);
	}
	option_with_arg ("-m", message);

	char *bg = xstrdup(bugid.c_str()),*p=NULL;
	while((p=strtok(p?NULL:bg,","))!=NULL)
	{
		option_with_arg("-b", p);
	}
	xfree(bg);

	option_with_arg ("-r", branch);

	/* If !found_slash, refrain from sending "Directory", for
	   CVS 1.9 compatibility.  If we only tried to deal with servers
	   which are at least CVS 1.9.26 or so, we wouldn't have to
	   special-case this.  */
	if (found_slash)
	{
	    repository = Name_Repository (NULL, NULL);
	    send_a_repository ("", repository, "");
	    xfree (repository);
	}

	for (i = 0; i < argc; ++i)
	{
	    /* FIXME: Does this erroneously call Create_Admin in error
	       conditions which are only detected once the server gets its
	       hands on things?  */
	    /* FIXME-also: if filenames are case-insensitive on the
	       client, and the directory in the repository already
	       exists and is named "foo", and the command is "cvs add
	       FOO", this call to Create_Admin puts the wrong thing in
	       CVS/Repository and so a subsequent "cvs update" will
	       give an error.  The fix will be to have the server report
	       back what it actually did (e.g. use tagged text for the
	       "Directory %s added" message), and then Create_Admin,
	       which should also fix the error handling concerns.  */

	    if (isdir (argv[i]))
	    {
		const char *tag;
		const char *date;
		int nonbranch;
		char *rcsdir;
		char *p;
		char *update_dir;
		/* This is some mungeable storage into which we can point
		   with p and/or update_dir.  */
		char *filedir;

		if (save_cwd (&cwd))
		    error_exit ();

		filedir = xstrdup (argv[i]);
		p = (char*)last_component (filedir);
		if (p == filedir)
		{
		    update_dir = "";
		}
		else
		{
		    p[-1] = '\0';
		    update_dir = filedir;
		    if (CVS_CHDIR (update_dir) < 0)
			error (1, errno,
			       "could not chdir to %s", update_dir);
		}

		/* find the repository associated with our current dir */
		repository = Name_Repository (NULL, update_dir);

		/* don't add stuff to Emptydir */
		if (strncmp (repository, current_parsed_root->directory, cvsroot_len) == 0
		    && ISDIRSEP (repository[cvsroot_len])
		    && strncmp (repository + cvsroot_len + 1,
				CVSROOTADM,
				sizeof CVSROOTADM - 1) == 0
		    && ISDIRSEP (repository[cvsroot_len + sizeof CVSROOTADM])
		    && strcmp (repository + cvsroot_len + sizeof CVSROOTADM + 1,
			       CVSNULLREPOS) == 0)
		    error (1, 0, "cannot add to %s", repository);

		if(!branch)
		{
			/* before we do anything else, see if we have any
			per-directory tags */
			ParseTag (&tag, &date, &nonbranch, NULL);

			/* It's possible for people to edit CVS/Tag themselves and make it invalid.. the
			server (at least cvshome one) doesn't check too hard in this case */
			if(tag && !isalpha(tag[0]))
			{
				error(1,0,"Directory sticky tag '%s' is not valid",tag);
			}
			if(nonbranch)
			{
				error(1,0,"Directory sticky tag '%s' is not a branch",tag);
			}
		}
		else
		{
			xfree(date);
			xfree(tag);
			nonbranch = 0;
			tag = xstrdup(branch);
		}

	   rcsdir = (char*)xmalloc (strlen (repository) + strlen (p) + 5);
		sprintf (rcsdir, "%s/%s", repository, p);

		if(fncmp (p, CVSADM))
		{
			/* Don't create the admin directories in the 'CVS' directory.  This is
			   reported to the user later on */
			Create_Admin (p, argv[i], rcsdir, tag, date,
				 nonbranch, 0);
			if (server_active)
				server_template (argv[i], rcsdir);
		}

		if (found_slash)
		    send_a_repository ("", repository, update_dir);

		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		free_cwd (&cwd);

	    xfree (tag);
	    xfree (date);
		xfree (rcsdir);

		if (p == filedir)
		    Subdir_Register ((List *) NULL, (char *) NULL, argv[i]);
		else
		{
		    Subdir_Register ((List *) NULL, update_dir, p);
		}
		xfree (repository);
		xfree (filedir);
	    }
	}
	send_arg("--");
	send_files (argc, argv, 0, 0, SEND_BUILD_DIRS | SEND_NO_CONTENTS);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_to_server ("add\n", 0);
	if (message)
	    xfree (message);
	return err + get_responses_and_close ();
    }

	if(!server_active)
		expand_wild(argc,argv,&argc,&argv);

    /* walk the arg list adding files/dirs */
    for (i = 0; i < argc; i++)
    {
	int begin_err = err;
#ifdef SERVER_SUPPORT
	int begin_added_files = added_files;
#endif
	struct file_info finfo;
	char *p;

	memset (&finfo, 0, sizeof finfo);

	if (save_cwd (&cwd))
	    error_exit ();

	finfo.fullname = xstrdup (argv[i]);
	p = (char*)last_component (argv[i]);
	if (p == argv[i])
	{
	    finfo.update_dir = "";
	    finfo.file = p;
	}
	else
	{
	    p[-1] = '\0';
	    finfo.update_dir = argv[i];
	    finfo.file = p;
	    if (CVS_CHDIR (finfo.update_dir) < 0)
		error (1, errno, "could not chdir to %s", finfo.update_dir);
	}

	/* Add wrappers for this directory.  They exist only until
	   the next call to wrap_add_file.  */
	wrap_add_file (CVSDOTWRAPPER, true);

	finfo.rcs = NULL;

	/* Find the repository associated with our current dir.  */
	repository = Name_Repository (NULL, finfo.update_dir);

	{
		const char *tag,*date,*version;
		int nonbranch;

		ParseTag_Dir(finfo.update_dir,&tag,&date,&nonbranch,&version);

		if(!branch)
		{
			/* It's possible for people to edit CVS/Tag themselves and make it invalid.. the
			server (at least cvshome one) doesn't check too hard in this case */
			if(tag && !isalpha(tag[0]))
			{
				error(1,0,"Directory sticky tag '%s' is not valid",tag);
			}
			if(nonbranch)
			{
				error(1,0,"Directory sticky tag '%s' is not a branch",tag);
			}
		}
		else
		{
			nonbranch = 0;
			xfree(date);
			xfree(tag);
			tag = xstrdup(branch);
		}

		open_directory(repository,finfo.update_dir,tag,date,nonbranch,version,0);
		xfree(tag);
		xfree(date);
		xfree(version);
	}
	finfo.mapped_file = map_filename(repository,finfo.file,&mapped_repository);
	xfree(mapped_repository);
	mapped_repository = map_repository(repository);

	/* don't add stuff to Emptydir */
	if (strncmp (mapped_repository, current_parsed_root->directory, cvsroot_len) == 0
	    && ISDIRSEP (mapped_repository[cvsroot_len])
	    && strncmp (mapped_repository + cvsroot_len + 1,
			CVSROOTADM,
			sizeof CVSROOTADM - 1) == 0
	    && ISDIRSEP (mapped_repository[cvsroot_len + sizeof CVSROOTADM])
	    && strcmp (mapped_repository + cvsroot_len + sizeof CVSROOTADM + 1,
		       CVSNULLREPOS) == 0)
	    error (1, 0, "cannot add to %s", fn_root(repository));

	entries = Entries_Open (0, NULL);

	finfo.repository = (char*)mapped_repository;
	finfo.virtual_repository = repository;
	finfo.entries = entries;

	/* We pass force_tag_match as 1.  If the directory has a
           sticky branch tag, and there is already an RCS file which
           does not have that tag, then the head revision is
           meaningless to us.  */
	vers = Version_TS (&finfo, options, branch, NULL, 1, 0, 0);

	if(!isdir(mapped_repository))
	{
		error(1,0, "Directory %s does not exist on the server",fn_root(repository));
	}
    if (! verify_create (mapped_repository, finfo.mapped_file, vers->tag, &msg,NULL))
	{
		if(vers->tag)
			error (msg?0:1, 0, "User '%s' cannot create %s on tag/branch %s", CVS_Username, fn_root(repository), vers->tag);
		else
			error (msg?0:1, 0, "User '%s' cannot create %s", CVS_Username, fn_root(repository));
		if(msg)
			error(1,0,"%s",msg);
	}

	if (vers->vn_user == NULL)
	{
	    /* No entry available, ts_rcs is invalid */
	    if (vers->vn_rcs == NULL)
	    {
		/* There is no RCS file either */
		if (vers->ts_user == NULL)
		{
		    /* There is no user file either */
		    error (0, 0, "nothing known about %s", fn_root(finfo.fullname));
		    err++;
		}
		else if (!isdir (finfo.file))
		{
		    /*
		     * See if a directory exists in the repository with
		     * the same name.  If so, blow this request off.
		     */
		    char *dname = (char*)xmalloc (strlen (repository)
					   + strlen (finfo.file)
					   + 10);
		    char *dnamem = (char*)xmalloc (strlen (mapped_repository)
					   + strlen (finfo.file)
					   + 10);
		    (void) sprintf (dname, "%s/%s", repository, finfo.file);
		    (void) sprintf (dnamem, "%s/%s", mapped_repository, finfo.file);
		    if (isdir (dnamem))
		    {
			error (0, 0,
			       "cannot add file `%s' since the directory",
			       fn_root(finfo.fullname));
			error (0, 0, "`%s' already exists in the repository",
			       fn_root(dname));
			error (1, 0, "illegal filename overlap");
		    }
		    xfree (dname);
			xfree (dnamem);

		    if (vers->options == NULL || *vers->options == '\0')
		    {
			/* No options specified on command line (or in
			   rcs file if it existed, e.g. the file exists
			   on another branch).  Check for a value from
			   the wrapper stuff.  */
			if (wrap_name_has (finfo.file, WRAP_RCSOPTION))
			{
			    if (vers->options)
				xfree (vers->options);
			    vers->options = wrap_rcsoption (finfo.file);
			}
		    }

		    if (vers->nonbranch)
		    {
			TRACE(3,"add - vers->nonbranch so cannot add file on non-branch tag");
			error (0, 0,
				"cannot add file on non-branch tag %s",
				vers->tag);
			++err;
		    }
		    else
		    {
			/* There is a user file, so build the entry for it */
			if (build_entry (repository, finfo.file, vers->options,
					 message, entries, vers->tag) != 0)
			    err++;
			else
			{
			    added_files++;
			    if (!quiet)
			    {
				if (vers->tag)
				    error (0, 0, "\
scheduling file `%s' for addition on branch `%s'",
					   fn_root(finfo.fullname), vers->tag);
				else
				    error (0, 0,
					   "scheduling file `%s' for addition",
					   fn_root(finfo.fullname));
			    }
			}
		    }
		}
	    }
	    else if (RCS_isdead (vers->srcfile, vers->vn_rcs))
	    {
		if (isdir (finfo.file))
		{
		    error (0, 0, "\
the directory `%s' cannot be added because a file of the", fn_root(finfo.fullname));
		    error (1, 0, "\
same name already exists in the repository.");
		}
		else
		{
		    if (vers->nonbranch)
		    {
			TRACE(3,"add - RCS_isdead(), vers->nonbranch so cannot add file on non-branch tag");
			error (0, 0,
			       "cannot add file on non-branch tag %s",
			       vers->tag);
			++err;
		    }
		    else
		    {
			if(vers->ts_user) // If the user file exists, this is an add, otherwise it's a resurrect
			{
				if (!quiet)
				{
					if (vers->tag)
					error (0, 0, "file `%s' will be added on branch `%s' from version %s",
						fn_root(finfo.fullname), vers->tag, vers->vn_rcs);
					else
					/* I'm not sure that mentioning
					vers->vn_rcs makes any sense here; I
					can't think of a way to word the
					message which is not confusing.  */
					error (0, 0, "re-adding file %s (in place of dead revision %s)",
						fn_root(finfo.fullname), vers->vn_rcs);
				}
				Register (entries, finfo.file, "0", vers->ts_user, vers->options,
						vers->tag, NULL, NULL, NULL, NULL, (time_t)-1, NULL, NULL, bugid.c_str(), NULL);
				++added_files;
			}
			else
			{
				if (!quiet)
				{
					if (vers->tag)
						error (0, 0, "%s, version %s, resurrected on branch '%s'", fn_root(finfo.fullname), vers->vn_rcs, vers->tag);
					else
						error (0, 0, "%s, version %s, resurrected", fn_root(finfo.fullname), vers->vn_rcs);
				}
				// Perform resurrection
				finfo.rcs = vers->srcfile;
				fileattr_startdir(finfo.repository);
				checkout_file(&finfo,vers,0,0,1,1,0,NULL,NULL);
				fileattr_free();
				finfo.rcs = NULL;
			}
		    }
		}
	    }
	    else
	    {
		/*
		 * There is an RCS file already, so somebody else must've
		 * added it
		 */
		error (0, 0, "%s added independently by second party",
		       fn_root(finfo.fullname));
		err++;
	    }
	}
	else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	{

	    /*
	     * An entry for a new-born file, ts_rcs is dummy, but that is
	     * inappropriate here
	     */
	    if (!quiet)
		error (0, 0, "%s has already been entered", fn_root(finfo.fullname));
	    err++;
	}
	else if (vers->vn_user[0] == '-')
	{
	    /* An entry for a removed file, ts_rcs is invalid */
	    if (vers->ts_user == NULL)
	    {
		/* There is no user file (as it should be) */
		if (vers->vn_rcs == NULL)
		{

		    /*
		     * There is no RCS file, so somebody else must've removed
		     * it from under us
		     */
		    error (0, 0, "\
cannot resurrect %s; RCS file removed by second party", fn_root(finfo.fullname));
		    err++;
		}
		else
		{

		    /*
		     * There is an RCS file, so remove the "-" from the
		     * version number and restore the file
		     */
		    char *tmp = (char*)xmalloc (strlen (finfo.file) + 50);

		    (void) strcpy (tmp, vers->vn_user + 1);
		    (void) strcpy (vers->vn_user, tmp);
		    (void) sprintf (tmp, "Resurrected %s", fn_root(finfo.file));
		    Register (entries, finfo.file, vers->vn_user, tmp,
			      vers->options,
			      vers->tag, vers->date, vers->ts_conflict, NULL, NULL, (time_t)-1, NULL, NULL, bugid.c_str(), NULL);
		    xfree (tmp);

		    /* XXX - bugs here; this really resurrect the head */
		    /* Note that this depends on the Register above actually
		       having written Entries, or else it won't really
		       check the file out.  */
		    if (update (2, argv + i - 1) == 0)
		    {
			error (0, 0, "%s, version %s, resurrected",
			       fn_root(finfo.fullname),
			       vers->vn_user);
		    }
		    else
		    {
			error (0, 0, "could not resurrect %s", fn_root(finfo.fullname));
			err++;
		    }
		}
	    }
	    else
	    {
		if(force_add)
		{
		    error (0, 0, "re-adding file %s (in place of dead revision %s)", fn_root(finfo.fullname), vers->vn_rcs);
		    char *tmp = (char*)xmalloc (strlen (finfo.file) + 50);

		    (void) strcpy (tmp, vers->vn_user + 1);
		    (void) strcpy (vers->vn_user, tmp);
		    (void) sprintf (tmp, "Added %s", fn_root(finfo.file));
		    Register (entries, finfo.file, vers->vn_user, tmp,
			      vers->options,
			      vers->tag, vers->date, vers->ts_conflict, NULL, NULL, (time_t)-1, NULL, NULL, bugid.c_str(), NULL);
		    xfree (tmp);
		}
		else
		{
			/* The user file shouldn't be there */
			error (0, 0, "%s should be removed and is still there (or is back again)", fn_root(finfo.fullname));
			err++;
		}
	    }
	}
	else
	{
	    /* A normal entry, ts_rcs is valid, so it must already be there */
	    if (!quiet)
		error (0, 0, "%s already exists, with version number %s",
			fn_root(finfo.fullname),
			vers->vn_user);
	    err++;
	}
	freevers_ts (&vers);

	/* passed all the checks.  Go ahead and add it if its a directory */
	if (begin_err == err
	    && isdir (finfo.file))
	{
	    err += add_directory (&finfo);
	}
	else
	{
#ifdef SERVER_SUPPORT
	    if (server_active && begin_added_files != added_files)
		server_checked_in (finfo.file, finfo.update_dir, repository);
#endif
	}
	xfree (repository);
	xfree (mapped_repository);
	Entries_Close (entries);
	TRACE(3,"add - close directory ?");
	close_directory();

	if (restore_cwd (&cwd, NULL))
	    error_exit ();
	free_cwd (&cwd);

	xfree (finfo.fullname);
	xfree (finfo.mapped_file);
    }
    if (added_files && !really_quiet)
	error (0, 0, "use '%s commit' to add %s permanently",
	       program_name,
	       (added_files == 1) ? "this file" : "these files");

    if (message)
	xfree (message);
    if (options)
	xfree (options);

	if(!server_active)
	    /* Free the data which expand_wild allocated.  */
	    free_names (&argc, argv);

    return (err);
}

/*
 * The specified user file is really a directory.  So, let's make sure that
 * it is created in the RCS source repository, and that the user's directory
 * is updated to include a CVS directory.
 * 
 * Returns 1 on failure, 0 on success.
 */
static int add_directory (struct file_info *finfo)
{
    const char *repository = finfo->repository;
	const char *virtual_repository = finfo->virtual_repository;
    List *entries = finfo->entries;
    const char *dir = finfo->file;
	int err = 0;
	CXmlNodePtr  node;

    char *rcsdir = NULL;
    struct saved_cwd cwd;
    char *message = NULL;
    const char *tag, *date;
    int nonbranch;
    CXmlNodePtr  attrs;

    if (strchr (dir, '/') != NULL)
    {
	/* "Can't happen".  */
	error (0, 0,
	       "directory %s not added; must be a direct sub-directory", dir);
	return (1);
    }
    if (fncmp (dir, CVSADM) == 0
		|| fncmp (dir, CVSDUMMY) == 0)
    {
	error (0, 0, "cannot add a `%s' directory", dir);
	return (1);
    }

	if(!branch)
	{
		/* before we do anything else, see if we have any per-directory tags */
		ParseTag (&tag, &date, &nonbranch, NULL);
	}
	else
	{
		tag = xstrdup(branch);
		nonbranch = 0;
		date = NULL;
	}

    /* Remember the default attributes from this directory, so we can apply
       them to the new directory.  */
    fileattr_startdir (repository);
	attrs = fileattr_copy(fileattr_find(NULL, "directory/default"));
    fileattr_free ();

    /* now, remember where we were, so we can get back */
    if (save_cwd (&cwd))
	return (1);
    if ( CVS_CHDIR (dir) < 0)
    {
	error (0, errno, "cannot chdir to %s", fn_root(finfo->fullname));
	return (1);
    }
    if (!server_active && isfile (CVSADM))
    {
	error (0, 0, "%s/%s already exists", fn_root(finfo->fullname), CVSADM);
	goto out;
    }

    rcsdir = (char*)xmalloc (strlen (repository) + strlen(virtual_repository) + strlen (dir) + 5);
    sprintf (rcsdir, "%s/%s", repository, dir);
    if (isfile (rcsdir) && !isdir (rcsdir))
    {
    sprintf (rcsdir, "%s/%s", virtual_repository, dir);
	error (0, 0, "%s is not a directory; %s not added", fn_root(rcsdir),
	       fn_root(finfo->fullname));
	goto out;
    }

    /* setup the log message */
    message = (char*)xmalloc (strlen (rcsdir)
		       + 80
		       + (tag == NULL ? 0 : strlen (tag) + 80)
		       + (date == NULL ? 0 : strlen (date) + 80));
    sprintf (rcsdir, "%s/%s", virtual_repository, dir);
    (void) sprintf (message, "Directory %s added to the repository\n", fn_root(rcsdir));
    if (tag)
    {
		if(branch)
			strcat (message, "--> Using sticky tag `");
		else
			strcat (message, "--> Using per-directory sticky tag `");
	(void) strcat (message, tag);
	(void) strcat (message, "'\n");
    }
    if (date)
    {
	(void) strcat (message, "--> Using per-directory sticky date `");
	(void) strcat (message, date);
	(void) strcat (message, "'\n");
    }

    sprintf (rcsdir, "%s/%s", repository, dir);
    if (!isdir (rcsdir))
    {
	mode_t omask;
	Node *p;
	List *ulist;
	struct logfile_info *li;

	/* There used to be some code here which would prompt for
	   whether to add the directory.  The details of that code had
	   bitrotted, but more to the point it can't work
	   client/server, doesn't ask in the right way for GUIs, etc.
	   A better way of making it harder to accidentally add
	   directories would be to have to add and commit directories
	   like for files.  The code was #if 0'd at least since CVS 1.5.  */

	if (!noexec)
	{
	    omask = umask (cvsumask);
	    if (CVS_MKDIR (rcsdir, 0777) < 0)
	    {
		error (0, errno, "cannot mkdir %s", fn_root(rcsdir));
		(void) umask (omask);
		goto out;
	    }
	    (void) umask (omask);
	   fileattr_startdir(rcsdir);
       change_owner(CVS_Username);

		/* Now set the default file attributes to the ones we inherited
		from the parent directory.  */
		node = fileattr_getroot();
		if(!node->GetChild("directory")) node->NewNode("directory");
		if(!node->GetChild("default")) node->NewNode("default");
		if(attrs)
		{
			fileattr_paste(node, attrs);
			fileattr_prune(node);
			fileattr_free_subtree(attrs);
		}
		fileattr_write ();
		fileattr_free ();
	}

	/*
	 * Set up an update list with a single title node for Update_Logfile
	 */
	ulist = getlist ();
	p = getnode ();
	p->type = UPDATE;
	p->delproc = update_delproc;
	p->key = xstrdup ("- New directory");
	li = (struct logfile_info *) xmalloc (sizeof (struct logfile_info));
	li->type = T_TITLE;
	li->tag = xstrdup (tag);
	li->bugid = xstrdup (bugid.c_str());
	li->rev_old = li->rev_new = NULL;
	p->data = (char *) li;
	(void) addnode (ulist, p);
	Update_Logfile (rcsdir, message, (FILE *) NULL, ulist, bugid.c_str());
	dellist (&ulist);
    }

    sprintf (rcsdir, "%s/%s", virtual_repository, dir);
    if (!server_active)
        Create_Admin (".", finfo->fullname, rcsdir, tag, date, nonbranch, 0);
    if (tag)
	xfree (tag);
    if (date)
	xfree (date);

    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);

    Subdir_Register (entries, (char *) NULL, dir);

    cvs_output (message, 0);

    xfree (rcsdir);
    xfree (message);

    return (0);

out:
    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);
    if (rcsdir != NULL)
	xfree (rcsdir);
    return err;
}

/*
 * Builds an entry for a new file and sets up "CVS/file",[pt] by
 * interrogating the user.  Returns non-zero on error.
 */
static int build_entry (const char *repository, const char *user, const char *options, const char *message, List *entries, const char *tag)
{
    char *fname;
    char *line;
    FILE *fp;

    if (noexec)
	return (0);

    /*
     * The requested log is read directly from the user and stored in the
     * file user,t.  If the "message" argument is set, use it as the
     * initial creation log (which typically describes the file).
     */
    fname = (char*)xmalloc (strlen (user) + 80);
    (void) sprintf (fname, "%s/%s%s", CVSADM, user, CVSEXT_LOG);
    fp = open_file (fname, "w+");
    if (message && fputs (message, fp) == EOF)
	    error (1, errno, "cannot write to %s", fname);
    if (fclose(fp) == EOF)
        error(1, errno, "cannot close %s", fname);
    xfree (fname);

    /*
     * Create the entry now, since this allows the user to interrupt us above
     * without needing to clean anything up (well, we could clean up the
     * ,t file, but who cares).
     */
    line = (char*)xmalloc (strlen (user) + 20);
    (void) sprintf (line, "Initial %s", user);
    Register (entries, user, "0", line, options, tag, (char *) 0, (char *) 0, NULL, NULL, (time_t)-1, NULL, NULL, bugid.c_str(), NULL);
    xfree (line);
    return (0);
}
