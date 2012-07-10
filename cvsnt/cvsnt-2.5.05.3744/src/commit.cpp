/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 * Commit Files
 *
 * "commit" commits the present version to the RCS repository, AFTER
 * having done a test on conflicts.
 *
 * The call is: cvs commit [options] files...
 *
 */
#include "../version.h"

#include "cvs.h"
#include "getline.h"
#include "edit.h"
#include "fileattr.h"
#include "hardlink.h"

/* From update */
int checkout_file(struct file_info *finfo, Vers_TS *vers_ts,
				 int adding, int merging, int update_server, int resurrect, int is_rcs, const char *merge_rev1, const char *merge_rev2);

static Dtype check_direntproc (void *callerdat, char *dir,
				      char *repos, char *update_dir,
				      List *entries, const char *virtual_repository, Dtype hint);
static int check_fileproc (void *callerdat, struct file_info *finfo);
static int check_filesdoneproc (void *callerdat, int err,
				       char *repos, char *update_dir,
				       List *entries);
static Dtype commit_direntproc (void *callerdat, char *dir,
				       char *repos, char *update_dir,
				       List *entries, const char *virtual_repository, Dtype hint);
static int commit_dirleaveproc (void *callerdat, char *dir,
				       int err, char *update_dir,
				       List *entries);
static int commit_fileproc (void *callerdat, struct file_info *finfo);
static int commit_filesdoneproc (void *callerdat, int err,
					char *repository, char *update_dir,
					List *entries);
static int finaladd(struct file_info *finfo, char *revision, char *tag, char *options, const char *merge_tag1, const char *merge_tag2, const char *bugid);
static int findmaxrev (Node * p, void *closure);
static int lock_RCS (const char *user, RCSNode *rcs, const char *rev, const char *repository);
static int precommit_list_proc(Node * p, void *closure);
static int precommit_proc(void *param, const trigger_interface *cb);
static int remove_file (struct file_info *finfo, const char *tag, const char *message, const char *bugid);
static void fixbranch(RCSNode *, char *branch);
static void unlockrcs(RCSNode *rcs);
static void ci_delproc(Node *p);
static void masterlist_delproc(Node *p);
static char *locate_rcs(const char *file, const char *repository);
static int checkaddfile(const char *file, const char *mapped_file, const char *dir, const char *repository, const char *tag,
			       const char *options, RCSNode **rcsnode, RCSCHECKINPROC callback, const char *bugid);
static void fixaddfile(const char *file, const char *repository);

struct precommit_params_t
{
	const char *message;
	const char *directory;
};

struct commit_info
{
    Ctype status;			/* as returned from Classify_File() */
    char *rev;				/* a numeric rev, if we know it */
    char *tag;				/* any sticky tag, or -r option */
	char *options;			/* RCS file options */
	size_t lockId;			/* Advisory write lock */
};
struct master_lists
{
    List *ulist;			/* list for Update_Logfile */
    List *cilist;			/* list with commit_info structs */
};

int commit_keep_edits = 0;


static int check_valid_edit = 0;
static int force_ci = 0;
static int force_modified = 0;
static int got_message;
static int run_module_prog = 1;
static int aflag;
static const char *write_dirtag;
static int write_dirnonbranch;
static char *logfile;
static List *mulist;
static List *saved_ulist;
static char *saved_message;
static char *current_date;
static cvs::string bugid;
static int only_bug;
static int slide_tags;
static int ignore_keywords;
static time_t last_register_time;
static const char **precommit_list;
static int precommit_list_size, precommit_list_count;

static const char *const commit_usage[] =
{
    "Usage: %s %s [DnRlf] [-B bug | -b bug ] [-m msg | -F logfile] files...\n",
	"\t-b bugid\tOnly commit files related to bug.\n",
	"\t-B bugid\tMark files with bug.\n",
	"\t-c\t\tCheck for valid edits before committing.\n",
	"\t-D\t\tAssume all files are modified.\n",
	"\t-F logfile\tRead the log message from file.\n",
	"\t-e\t\tKeep edited files.\n",
	"\t-f\t\tForce the file to be committed; disables recursion.\n",
//	"\t-i\t\tIgnore keyword differences (EXPERIMENTAL).\n",
    "\t-n\t\tDo not run the module program (if any).\n",
	"\t-l\t\tLocal directory only (not recursive).\n",
#ifdef _WIN32
	"\t-m message\tLog message (the log message will include an advert).\n",
#else
	"\t-m message\tLog message.\n",
#endif
	"\t-M message\tLog message.\n",
	"\t-R\t\tProcess directories recursively.\n",
	"\t-T\t\tMove empty branches (where possible).\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/* Identify a file which needs "? foo" or a Questionable request.  */
struct question {
    /* The two fields for the Directory request.  */
    char *dir;
    char *repos;

    /* The file name.  */
    char *file;

    struct question *next;
};

struct find_data {
    List *ulist;
    List *ulistrenames;
    int argc;
    char **argv;

    /* This is used from dirent to filesdone time, for each directory,
       to make a list of files we have already seen.  */
    List *ignlist;

    /* Linked list of files which need "? foo" or a Questionable request.  */
    struct question *questionables;

    /* Only good within functions called from the filesdoneproc.  Stores
       the repository (pointer into storage managed by the recursion
       processor.  */
    char *repository;

    /* Non-zero if we should force the commit.  This is enabled by
       either -f or -r options, unlike force_ci which is just -f.  */
    int force;

    /* Non-zero we should assume the files are modified */
	int modified;

	/* Non-zero if there are any pending renames */
	int renames;
};


static Dtype find_dirent_proc (void *callerdat, char *dir, char *repository, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
    struct find_data *find_data = (struct find_data *)callerdat;
    Node *node;

	if(hint!=R_PROCESS)
		return hint;

    TRACE(3,"find_dirent_proc");

    /* initialize the ignore list for this directory */
    find_data->ignlist = getlist ();

    /* Print the same warm fuzzy as in check_direntproc, since that
       code will never be run during client/server operation and we
       want the messages to match. */
    if (!quiet)
	error (0, 0, "Examining %s", update_dir);

    char *renameadm;
    renameadm=(char*)xmalloc (sizeof (char) * strlen(update_dir) + strlen(CVSADM_RENAME) +20);
    if (renameadm!=NULL)
    {
	sprintf(renameadm,"%s/%s",update_dir,CVSADM_RENAME);
	if(isfile(renameadm))
	{
		find_data->renames++;
		node = getnode ();
		node->key = xstrdup (update_dir);
		node->type = DIRS;
		(void)addnode (find_data->ulistrenames, node);
	}
	xfree(renameadm);
	renameadm=NULL;
    }

    return R_PROCESS;
}

/* Here as a static until we get around to fixing ignore_files to pass
   it along as an argument.  */
static struct find_data *find_data_static;

static void find_ignproc (char *file, char *dir)
{
    struct question *p;

    p = (struct question *) xmalloc (sizeof (struct question));
    p->dir = xstrdup (dir);
    p->repos = xstrdup (find_data_static->repository);
    p->file = xstrdup (file);
    p->next = find_data_static->questionables;
    find_data_static->questionables = p;
}

static int find_filesdoneproc(void *callerdat, int err,
				      char *repository, char *update_dir,
				      List *entries)
{
    struct find_data *find_data = (struct find_data *)callerdat;
    find_data->repository = repository;

    TRACE(3,"find_filesdoneproc");
    /* if this directory has an ignore list, process it then free it */
    if (find_data->ignlist)
    {
	find_data_static = find_data;
	ignore_files (find_data->ignlist, entries, update_dir, find_ignproc);
	dellist (&find_data->ignlist);
    }

    find_data->repository = NULL;

    return err;
}


/* Machinery to find out what is modified, added, and removed.  It is
   possible this should be broken out into a new client_classify function;
   merging it with classify_file is almost sure to be a mess, though,
   because classify_file has all kinds of repository processing.  */
static int find_fileproc(void *callerdat, struct file_info *finfo)
{
    Vers_TS *vers;
    enum classify_type status;
    Node *node;
    struct find_data *args = (struct find_data *)callerdat;
    struct logfile_info *data;
    struct file_info xfinfo;

    TRACE(3,"find_fileproc(%s)",PATCH_NULL(finfo->file));
    /* if this directory has an ignore list, add this file to it */
    if (args->ignlist)
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (finfo->file);
	if (addnode (args->ignlist, p) != 0)
	    freenode (p);
    }

    xfinfo = *finfo;
    xfinfo.repository = NULL;
    xfinfo.rcs = NULL;

    vers = Version_TS (&xfinfo, NULL, NULL, NULL, 0, 0, 0);
    if (vers->ts_user == NULL && vers->vn_user != NULL && vers->vn_user[0] == '-')
	/* FIXME: If vn_user is starts with "-" but ts_user is
	   non-NULL, what classify_file does is print "%s should be
	   removed and is still there".  I'm not sure what it does
	   then.  We probably should do the same.  */
		status = T_REMOVED;
    else if (vers->vn_user == NULL)
    {
		if (vers->ts_user == NULL)
			error (0, 0, "nothing known about `%s'", fn_root(finfo->fullname));
		else
			error (0, 0, "use `%s add' to create an entry for %s",
			program_name, fn_root(finfo->fullname));
		freevers_ts (&vers);
		return 1;
    }
    else if (vers->ts_user != NULL && vers->vn_user != NULL  && vers->vn_user[0] == '0')
	/* FIXME: If vn_user is "0" but ts_user is NULL, what classify_file
	   does is print "new-born %s has disappeared" and removes the entry.
	   We probably should do the same.  */
		status = T_ADDED;
    else if (vers->ts_user != NULL && vers->ts_rcs != NULL  && (args->force || strcmp (vers->ts_user, vers->ts_rcs) != 0))
	/* If we are forcing commits, pretend that the file is
           modified.  */
		status = T_MODIFIED;
    else
    {
		/* This covers unmodified files, as well as a variety of other
		cases.  FIXME: we probably should be printing a message and
		returning 1 for many of those cases (but I'm not sure
		exactly which ones).  */
		freevers_ts (&vers);
		return 0;
    }

    node = getnode ();
    node->key = xstrdup (finfo->fullname);

    data = (struct logfile_info *) xmalloc (sizeof (struct logfile_info));
    data->type = status;
    data->tag = xstrdup (vers->tag);
	data->bugid = xstrdup (bugid.size()?bugid.c_str():(vers?vers->edit_bugid:NULL));
    data->rev_old = data->rev_new = NULL;

    node->type = UPDATE;
    node->delproc = update_delproc;
    node->data = (char *) data;
    (void)addnode (args->ulist, node);

    ++args->argc;

    freevers_ts (&vers);
    return 0;
}

static int copy_ulist (Node *node, void *data)
{
    struct find_data *args = (struct find_data *)data;
    TRACE(3,"copy_ulist( ) create the argv[%d] list \"%s\".",args->argc,PATCH_NULL(node->key));
    args->argv[args->argc++] = node->key;
    return 0;
}

bool bugid_in(const char *bugid, const char *bugs)
{
	if(!bugid || !bugs)
		return 0;
	if(!strcmp(bugid,bugs))
		return 1;
	char *bg = xstrdup(bugs), *p=NULL;
	while((p=strtok(p?NULL:bg,","))!=NULL)
	{
		if(!strcmp(bugid,p))
		{
			xfree(bg);
			return 1;
		}
	}
	xfree(bg);
	return 0;
}

int commit (int argc, char **argv)
{
    int c;
    int err = 0;
    int local = 0;
	int mhadvertise = 0, mhadvertiseserver = 0;

    if (argc == -1)
		usage (commit_usage);

#ifdef CVS_BADROOT
    /*
     * For log purposes, do not allow "root" to commit files.  If you look
     * like root, but are really logged in as a non-root user, it's OK.
     */
    /* FIXME: Shouldn't this check be much more closely related to the
       readonly user stuff (CVSROOT/readers, &c).  That is, why should
       root be able to "cvs init", "cvs import", &c, but not "cvs ci"?  */
    if (geteuid () == (uid_t) 0
	/* Who we are on the client side doesn't affect logging.  */
	&& !current_parsed_root->isremote
	)
    {
	struct passwd *pw;

	if ((pw = (struct passwd *) getpwnam (getcaller ())) == NULL)
	    error (1, 0, "you are unknown to this system");
	if (pw->pw_uid == (uid_t) 0)
	    error (1, 0, "cannot commit files as 'root'");
    }
#endif /* CVS_BADROOT */

#ifdef _WIN32
	char boughtsuite[100];
    if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","HaveBoughtSuite",boughtsuite,sizeof(boughtsuite)))
		strcpy(boughtsuite,"no");
#endif

    optind = 0;
	while ((c = getopt (argc, argv, "+cnlRm:M:fF:Db:B:eTi")) != -1)
    {
	switch (c)
	{
	case 'i':
		ignore_keywords = 1;
		break;
	case 'b':
		if(bugid.size() && !only_bug)
			error(1,0,"Cannot mix -B and -b");
		if(!RCS_check_bugid(optarg,true))
			error(1,0,"Invalid characters in bug identifier.  Please avoid \"'");
		if(bugid.size())
			bugid+=",";
		bugid+=optarg;
		only_bug = 1;
		break;
	case 'B':
		if(bugid.size() && only_bug)
			error(1,0,"Cannot mix -B and -b");
		if(!RCS_check_bugid(optarg,true))
			error(1,0,"Invalid characters in bug identifier.  Please avoid \"'");
		if(bugid.size())
			bugid+=",";
		bugid+=optarg;
		break;
    case 'c':
        check_valid_edit = 1;
        break;
	case 'n':
		run_module_prog = 0;
		break;
	case 'm':
#ifdef CVSSPAM
#ifdef _WIN32
		mhadvertiseserver = 1;
		if (strcasecmp(boughtsuite,"yes"))
			mhadvertise = 1;
#endif
#endif
	case 'M':
#ifdef FORCE_USE_EDITOR
		use_editor = 1;
#else
		use_editor = 0;
#endif
		if (saved_message)
		{
		    xfree (saved_message);
		    saved_message = NULL;
		}

		saved_message = xstrdup(optarg);
		break;
	case 'l':
		local = 1;
		break;
	case 'R':
		local = 0;
		break;
	case 'f':
		force_ci = 1;
        check_valid_edit = -1;
		local = 1;		/* also disable recursion */
		break;
	case 'F':
#ifdef FORCE_USE_EDITOR
		use_editor = 1;
#else
		use_editor = 0;
#endif
		logfile = optarg;
		break;
	case 'D':
			force_modified = 1;
			break;
	case 'e':
			commit_keep_edits = 1;
			break;
	    case '?':
		case 'T':
			slide_tags = 1;
			break;
	 default:
		usage (commit_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* some checks related to the "-F logfile" option */
    if (logfile)
    {
	size_t size = 0, len;
	kflag kf_empty;

	if (saved_message)
	    error (1, 0, "cannot specify both a message and a log file");

	get_file (logfile, logfile, "r", &saved_message, &size, &len, kf_empty);
    }

    if (current_parsed_root->isremote)
    {
	struct find_data find_args;

	find_args.ulist = getlist ();
	find_args.ulistrenames = getlist ();
	find_args.argc = 0;
	find_args.questionables = NULL;
	find_args.ignlist = NULL;
	find_args.repository = NULL;
	find_args.renames = 0;

	/* It is possible that only a numeric tag should set this.
	   I haven't really thought about it much.
	   Anyway, I suspect that setting it unnecessarily only causes
	   a little unneeded network traffic.  */
	find_args.force = force_ci || force_modified;

	err = start_recursion (find_fileproc, find_filesdoneproc,
			       (PREDIRENTPROC) NULL, find_dirent_proc, (DIRLEAVEPROC) NULL,
			       (void *)&find_args,
			       argc, argv, local, W_LOCAL, 0, 0,
			       (char *)NULL, NULL, 0, (PERMPROC) NULL, NULL);
	if (err)
	    error (1, 0, "correct above errors first!");

	if (find_args.argc == 0 && !find_args.renames)
	{
	    /* Nothing to commit.  Exit now without contacting the
	       server (note that this means that we won't print "?
	       foo" for files which merit it, because we don't know
	       what is in the CVSROOT/cvsignore file).  */
	    dellist (&find_args.ulist);
	    dellist (&find_args.ulistrenames);
	    return 0;
	}

	/* Now we keep track of which files we actually are going to
	   operate on, and only work with those files in the future.
	   This saves time--we don't want to search the file system
	   of the working directory twice.  */
	find_args.argv = (char **) xmalloc (find_args.argc * sizeof (char **));
	find_args.argc = 0;
	walklist (find_args.ulist, copy_ulist, &find_args);
	if (find_args.argc != 0 && find_args.renames)
	{
		TRACE(3,"* there are renames, so make sure we commit the '.' directory.");
		walklist (find_args.ulistrenames, copy_ulist, &find_args);
	}
	TRACE(3,"Commit created the argv[] list with find_args.argc=%d",find_args.argc);

	/*
	 * We do this once, not once for each directory as in normal CVS.
	 * The protocol is designed this way.  This is a feature.
	 */
	if (use_editor)
	{
	    do_editor (".", &saved_message, (char *)NULL, find_args.ulist);
	}

	/* Run the user-defined script to verify/check information in
	 *the log message
	 */
	err = do_verify (&saved_message, (char *)NULL);
	if (err)
	    error (1, 0, "correct above errors first!");

#ifdef CVSSPAM
#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
	if (strcasecmp(CVSNT_SPECIAL_BUILD,"Suite")!=0)
#endif
	{
	/* We always send some sort of message, even if empty.  */
	if (mhadvertise)
	{
		if (saved_message!=NULL)
		{
		size_t saved_message_size=strlen(saved_message);
		saved_message=(char *)xrealloc(saved_message,saved_message_size+250);
		}
		else
		saved_message=(char *)xmalloc(250);
		strcat(saved_message,"\nCommitted on the Free edition of March Hare Software CVSNT Client.\nUpgrade to CVS Suite for more features and support:\nhttp://march-hare.com/cvsnt/");
		size_t saved_message_size=strlen(saved_message);
	}
    }
#endif
	/* FIXME: is that true?  There seems to be some code in do_editor
	   which can leave the message NULL.  */
	option_with_arg ("-m", saved_message);

	if(commit_keep_edits)
		send_arg("-e");

	if(slide_tags)
		send_arg("-T");

	if(ignore_keywords)
		send_arg("i");

	if(bugid.size())
	{
		if(!only_bug)
		{
			char *bg = xstrdup(bugid.c_str()),*p=NULL;
			while((p=strtok(p?NULL:bg,","))!=NULL)
			{
				option_with_arg("-B", p);
			}
			xfree(bg);
		}
		else
		{
			char *bg = xstrdup(bugid.c_str()),*p=NULL;
			while((p=strtok(p?NULL:bg,","))!=NULL)
			{
				option_with_arg("-b", p);
			}
			xfree(bg);
		}
	}

	/* OK, now process all the questionable files we have been saving
	   up.  */
	{
	    struct question *p;
	    struct question *q;

	    p = find_args.questionables;
	    while (p != NULL)
	    {
		if (!supported_request ("Questionable"))
		{
		    cvs_output ("? ", 2);
		    if (p->dir[0] != '\0')
		    {
			cvs_output (p->dir, 0);
			cvs_output ("/", 1);
		    }
		    cvs_output (p->file, 0);
		    cvs_output ("\n", 1);
		}
		else
		{
		    send_to_server ("Directory ", 0);
		    send_to_server (p->dir[0] == '\0' ? "." : p->dir, 0);
		    send_to_server ("\n", 1);
		    send_to_server (p->repos, 0);
		    send_to_server ("\n", 1);

		    send_to_server ("Questionable ", 0);
		    send_to_server (p->file, 0);
		    send_to_server ("\n", 1);
		}
		xfree (p->dir);
		xfree (p->repos);
		xfree (p->file);
		q = p->next;
		xfree (p);
		p = q;
	    }
	}

	if (local)
	    send_arg("-l");
	if (force_ci)
	    send_arg("-f");
	if (!run_module_prog)
	    send_arg("-n");
	if (check_valid_edit>0)
	    send_arg("-c");
	send_arg("--");

	/* FIXME: This whole find_args.force/SEND_FORCE business is a
	   kludge.  It would seem to be a server bug that we have to
	   say that files are modified when they are not.  This makes
	   "cvs commit -r 2" across a whole bunch of files a very slow
	   operation (and it isn't documented in cvsclient.texi).  I
	   haven't looked at the server code carefully enough to be
	   _sure_ why this is needed, but if it is because the "ci"
	   program, which we used to call, wanted the file to exist,
	   then it would be relatively simple to fix in the server.  */
	send_files (find_args.argc, find_args.argv, local, 0,
		(find_args.force ? SEND_FORCE : 0));

	/* Sending only the names of the files which were modified, added,
	   or removed means that the server will only do an up-to-date
	   check on those files.  This is different from local CVS and
	   previous versions of client/server CVS, but it probably is a Good
	   Thing, or at least Not Such A Bad Thing.  */
	send_file_names (find_args.argc, find_args.argv, 0);
	xfree (find_args.argv);
	dellist (&find_args.ulist);
	dellist (&find_args.ulistrenames);

	send_to_server ("ci\n", 0);
	err = get_responses_and_close ();
	if (err != 0 && use_editor && saved_message != NULL)
	{
	    /* If there was an error, don't nuke the user's carefully
	       constructed prose.  This is something of a kludge; a better
	       solution is probably more along the lines of #150 in TODO
	       (doing a second up-to-date check before accepting the
	       log message has also been suggested, but that seems kind of
	       iffy because the real up-to-date check could still fail,
	       another error could occur, &c.  Also, a second check would
	       slow things down).  */

	    char *fname = NULL;
	    FILE *fp;

	    fp = cvs_temp_file (&fname);
	    if (fp == NULL)
		error (1, 0, "cannot create temporary file %s", fname);
	    if (fwrite (saved_message, 1, strlen (saved_message), fp)
		!= strlen (saved_message))
		error (1, errno, "cannot write temporary file %s", fname);
	    if (fclose (fp) < 0)
		error (0, errno, "cannot close temporary file %s", fname);
	    error (0, 0, "saving log message in %s", fname);
	    xfree (fname);
	}
#ifdef CVSSPAM
#ifdef _WIN32
#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
	if (strcasecmp(CVSNT_SPECIAL_BUILD,"Suite")!=0)
#endif
	{
	if (!mhadvertise)
	{
		error(0, 0, "Committed on the Free edition of March Hare Software CVSNT Client\n           Upgrade to CVS Suite for more features and support:\n           http://march-hare.com/cvsnt/");
	}
    }
#endif
#endif
	return err;
    }

    lock_tree_for_write (argc, argv, local, W_LOCAL, aflag);

    /*
     * Set up the master update list and hard link list
     */
    mulist = getlist ();

    /*
     * Run the recursion processor to verify the files are all up-to-date
     */
    err = start_recursion (check_fileproc, check_filesdoneproc,
			   (PREDIRENTPROC) NULL, check_direntproc, (DIRLEAVEPROC) NULL, NULL, argc,
			   argv, local, W_LOCAL, aflag, 0, (char *) NULL, NULL, 1,
			   verify_write, NULL);
    if (err)
    {
	Lock_Cleanup ();
	error (1, 0, "correct above errors first!");
    }

	current_date = date_from_time_t(global_session_time_t);

    /*
     * Run the recursion processor to commit the files
     */
	TRACE(3,"commit Run the recursion processor to commit the files");
    write_dirnonbranch = 0;
    if (noexec == 0)
	err = start_recursion (commit_fileproc, commit_filesdoneproc,
			       (PREDIRENTPROC) NULL, commit_direntproc, commit_dirleaveproc, NULL,
			       argc, argv, local, W_LOCAL|W_QUIET, aflag, 0,
			       (char *) NULL, NULL, 1, verify_write, NULL);

    /*
     * Unlock all the dirs and clean up
     */
	TRACE(3,"commit Unlock all the dirs and clean up");
    Lock_Cleanup ();
    dellist (&mulist);

	xfree(current_date);

	TRACE(3,"commit do_module stuff");
	{
    DBM *db = open_module ();
	int i;
	if(argc)
	{
		for (i = 0; i < argc; i++)
		{
			char *repos;
			if(isdir(argv[i]))
			{
				repos = Name_Repository (NULL, argv[i]);
				err += do_module (db, Short_Repository(repos), mtCHECKIN, "Checking in", NULL,
					NULL, 0, local, run_module_prog, 0,
					(char *) NULL);
				xfree(repos);
			}
		}
	}
	else
	{
	    char *repos = Name_Repository (NULL, NULL);
		err += do_module (db, repos + strlen(current_parsed_root->directory)+1, mtCHECKIN, "Checking in", NULL,
			  NULL, 0, local, run_module_prog, 0,
			  (char *) NULL);
		xfree(repos);
	}
    close_module (db);
	}

#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
	TRACE(3,"commit server_active=%s, mhadvertiseserver=%s, CVSNT_SPECIAL_BUILD=%s",(server_active)?"yes":"no",(mhadvertiseserver)?"yes":"no",CVSNT_SPECIAL_BUILD);
#else
	TRACE(3,"commit server_active=%s, mhadvertiseserver=%s",(server_active)?"yes":"no",(mhadvertiseserver)?"yes":"no");
#endif
	if (server_active)
	{
		return err;
	}

    /* see if we need to sleep before returning to avoid time-stamp races */
    if (last_register_time)
    {
	sleep_past (last_register_time);
    }

    return (err);
}

/* This routine determines the status of a given file and retrieves
   the version information that is associated with that file. */

static Ctype classify_file_internal(struct file_info *finfo, Vers_TS **vers)
{
    int save_noexec, save_quiet, save_really_quiet;
    Ctype status;

    /* FIXME: Do we need to save quiet as well as really_quiet?  Last
       time I glanced at Classify_File I only saw it looking at really_quiet
       not quiet.  */
    save_noexec = noexec;
    save_quiet = quiet;
    save_really_quiet = really_quiet;
    noexec = quiet = really_quiet = 1;

	status = Classify_File (finfo, NULL, (char *) NULL, (char *) NULL,
			1, 0, vers, 0, force_modified, ignore_keywords);

	noexec = save_noexec;
    quiet = save_quiet;
    really_quiet = save_really_quiet;

    return status;
}

/*
 * Check to see if a file is ok to commit and make sure all files are
 * up-to-date
 */
/* ARGSUSED */
static int check_fileproc (void *callerdat, struct file_info *finfo)
{
    Ctype status;
    const char *xdir ,*dir;
    Node *p;
    List *ulist, *cilist;
    Vers_TS *vers;
    struct commit_info *ci;
    struct logfile_info *li;
	kflag kftmp;

    size_t cvsroot_len = strlen (current_parsed_root->directory);

    if (!finfo->repository)
    {
	error (0, 0, "nothing known about `%s'", fn_root(finfo->fullname));
	return (1);
    }

    if (strncmp (finfo->repository, current_parsed_root->directory, cvsroot_len) == 0
	&& ISDIRSEP (finfo->repository[cvsroot_len])
	&& strncmp (finfo->repository + cvsroot_len + 1,
		    CVSROOTADM,
		    sizeof (CVSROOTADM) - 1) == 0
	&& ISDIRSEP (finfo->repository[cvsroot_len + sizeof (CVSROOTADM)])
	&& strcmp (finfo->repository + cvsroot_len + sizeof (CVSROOTADM) + 1,
		   CVSNULLREPOS) == 0
	)
	error (1, 0, "cannot check in to %s", finfo->repository);

    status = classify_file_internal (finfo, &vers);

	/* Implement -b */
	if(only_bug && bugid.size() && (!vers->edit_bugid || !bugid_in(bugid.c_str(),vers->edit_bugid)))
	{
		freevers_ts(&vers);
		return 0;
	}

	/*
     * If the force-commit option is enabled, and the file in question
     * appears to be up-to-date, just make it look modified so that
     * it will be committed.
     */
    if (force_ci && status == T_UPTODATE)
		status = T_MODIFIED;

	/* We're never going to get to the next stage, so update the entries file here */
	if(status == T_UPTODATE && vers->ts_rcs && vers->ts_user && 
		vers->ts_rcs[0] && strcmp(vers->ts_user,vers->ts_rcs))
	{
	    Register (finfo->entries, finfo->file, vers->vn_rcs, vers->ts_user,
		      vers->options, vers->tag, vers->date, (char *) 0, NULL, NULL, vers->tt_rcs, vers->edit_revision, vers->edit_tag, vers->edit_bugid, NULL);
#ifdef SERVER_SUPPORT
		if(server_active)
		{
			/* Need to update the checked out file on the client side.  */
			server_updated (finfo, vers, SERVER_UPDATED,
			    (mode_t) -1, NULL,
			    (struct buffer *) NULL);
		}
#endif
	}

   
    switch (status)
    {
	case T_CHECKOUT:
	case T_PATCH:
	case T_NEEDS_MERGE:
	case T_CONFLICT:
	case T_REMOVE_ENTRY:
	    error (0, 0, "Up-to-date check failed for `%s'", fn_root(finfo->fullname));
	    freevers_ts (&vers);
	    return (1);
	case T_MODIFIED:
	case T_ADDED:
	case T_REMOVED:
	    /*
	     * some quick sanity checks; if no numeric -r option specified:
	     *	- can't have a sticky date
	     *	- can't have a sticky tag that is not a branch
	     * Also,
	     *	- if status is T_REMOVED, can't have a numeric tag
	     *	- if status is T_ADDED, rcs file must not exist unless on
	     *    a branch or head is dead
	     *	- if status is T_ADDED, can't have a non-trunk numeric rev
	     *	- if status is T_MODIFIED and a Conflict marker exists, don't
	     *    allow the commit if timestamp is identical or if we find
	     *    an RCS_MERGE_PAT in the file.
	     */
		if (vers->date)
		{
		    error (0, 0,
			   "cannot commit with sticky date for file `%s'",
			   fn_root(finfo->fullname));
		    freevers_ts (&vers);
		    return (1);
		}
		if (status == T_MODIFIED && vers->tag &&
		    !RCS_isbranch (finfo->rcs, vers->tag))
		{
		    error (0, 0,
			   "sticky tag `%s' for file `%s' is not a branch",
			   vers->tag, fn_root(finfo->fullname));
		    freevers_ts (&vers);
		    return (1);
		}
	    /* See if we have write access to the file */
            dir = finfo->repository;
	    if (status == T_MODIFIED && !force_ci && vers->ts_conflict)
	    {
		char *filestamp;
		int retcode;

		/*
		 * We found a "conflict" marker.
		 *
		 * If the timestamp on the file is the same as the
		 * timestamp stored in the Entries file, we block the commit.
		 */
		if (server_active)
		    retcode = vers->ts_conflict[0] != '=';
		else {
		    filestamp = time_stamp (finfo->file, 0);
		    retcode = strcmp (vers->ts_conflict, filestamp);
		    xfree (filestamp);
		}
		if (retcode == 0)
		{
		    error (0, 0,
			  "file `%s' had a conflict and has not been modified",
			   fn_root(finfo->fullname));
		    freevers_ts (&vers);
		    return (1);
		}

		if (file_has_markers (finfo))
		{
		    /* Make this a warning, not an error, because we have
		       no way of knowing whether the "conflict indicators"
		       are really from a conflict or whether they are part
		       of the document itself (cvs.texinfo and sanity.sh in
		       CVS itself, for example, tend to want to have strings
		       like ">>>>>>>" at the start of a line).  Making people
		       kludge this the way they need to kludge keyword
		       expansion seems undesirable.  And it is worse than
		       keyword expansion, because there is no -ko
		       analogue.  */
		    error (0, 0,
			   "\
warning: file `%s' seems to still contain conflict indicators",
			   fn_root(finfo->fullname));
		}
	    }

	    if (status == T_REMOVED
		&& vers->tag &&
		(isdigit ((unsigned char) *vers->tag) ||
			!RCS_isbranch(finfo->rcs,vers->tag)))
	    {
		/* Remove also tries to forbid this, but we should check
		   here.  I'm only _sure_ about somewhat obscure cases
		   (hacking the Entries file, using an old version of
		   CVS for the remove and a new one for the commit), but
		   there might be other cases.  */
		error (0, 0,
	"cannot remove file `%s' which has a sticky tag of `%s'",
			   fn_root(finfo->fullname), vers->tag);
		freevers_ts (&vers);
		return (1);
	    }
	    if (status == T_ADDED)
	    {
	        if (vers->tag == NULL)
		{
		    if (finfo->rcs && finfo->rcs->head && 
			!RCS_isdead (finfo->rcs, finfo->rcs->head))
		    {
			error (0, 0,
		    "cannot add file `%s' when RCS file `%s' already exists",
			       fn_root(finfo->fullname), fn_root(finfo->rcs->path));
			freevers_ts (&vers);
			return (1);
		    }
		}
		else if (isdigit ((unsigned char) *vers->tag) &&
		    numdots (vers->tag) > 1)
		{
		    error (0, 0,
		"cannot add file `%s' with revision `%s'; must be on trunk",
			       fn_root(finfo->fullname), vers->tag);
		    freevers_ts (&vers);
		    return (1);
		}
	    }

		RCS_get_kflags(vers->options,false,kftmp);
		if ( (check_can_edit(finfo->repository, finfo->file, CVS_Username, vers->tag) ||
			(check_valid_edit>=0 && (check_valid_edit || 
				kftmp.flags&KFLAG_RESERVED_EDIT) )) && ( status == T_MODIFIED || status == T_REMOVED ) )
	    {
			CXmlNodePtr handle = fileattr_getroot();
			handle->xpathVariable("name",finfo->file);
			handle->xpathVariable("user",CVS_Username);
			if(!handle->Lookup("file[cvs:filename(@name,$name)]/editor[cvs:username(@name,$user)]"))
			  handle=NULL;
			while(handle)
			{
				const char *edittag = fileattr_getvalue(handle,"tag");
				if(!edittag)
					break;
				if(!vers->tag && !strcmp(edittag,"HEAD"))
					break;
				if(vers->tag && !strcmp(edittag,vers->tag))
					break;

				if(!handle->XPathResultNext())
				  handle = NULL;
			}
			if(!handle)
			{
				error( 0, 0,
					"user '%s' is not a valid editor of the file '%s'",
				getcaller(), fn_root(finfo->fullname) );
				freevers_ts( &vers );
				return 1;
			}
	    }

	    /* done with consistency checks; now, to get on with the commit */
	    if (finfo->update_dir[0] == '\0')
		xdir = ".";
	    else
		xdir = finfo->update_dir;
	    if ((p = findnode_fn (mulist, xdir)) != NULL)
	    {
		ulist = ((struct master_lists *) p->data)->ulist;
		cilist = ((struct master_lists *) p->data)->cilist;
	    }
	    else
	    {
		struct master_lists *ml;

		ulist = getlist ();
		cilist = getlist ();
		p = getnode ();
		p->key = xstrdup (xdir);
		p->type = UPDATE;
		ml = (struct master_lists *)
		    xmalloc (sizeof (struct master_lists));
		ml->ulist = ulist;
		ml->cilist = cilist;
		p->data = (char *) ml;
		p->delproc = masterlist_delproc;
		(void) addnode (mulist, p);
	    }

	    /* first do ulist, then cilist */
	    p = getnode ();
	    p->key = xstrdup (finfo->file);
	    p->type = UPDATE;
	    p->delproc = update_delproc;
	    li = ((struct logfile_info *)
		  xmalloc (sizeof (struct logfile_info)));
	    li->type = status;
	    li->tag = xstrdup (vers->tag);
	    li->rev_old = xstrdup (vers->vn_rcs);
		li->bugid = xstrdup(bugid.size()?bugid.c_str():vers->edit_bugid);
	    li->rev_new = NULL;
	    p->data = (char *) li;
	    (void) addnode (ulist, p);

	    p = getnode ();
	    p->key = xstrdup (finfo->file);
	    p->type = UPDATE;
	    p->delproc = ci_delproc;
	    ci = (struct commit_info *) xmalloc (sizeof (struct commit_info));
	    ci->status = status;
		{
			char *f = (char*)xmalloc(strlen(finfo->file)+sizeof(RCSEXT)+10);
			sprintf(f,"%s%s",finfo->file,RCSEXT);
			ci->lockId = do_lock_file(f, finfo->repository, 0, 0);
			/* To avoid deadlock, release the file, before doing a lock which waits */
			if(!ci->lockId)
			{
				if(finfo->rcs)
					rcsbuf_close(&finfo->rcs->rcsbuf);
				ci->lockId = do_lock_file(f, finfo->repository, 1, 1);
			}
			xfree(f);
		}
	    if (vers->tag)
		{
			if (isdigit ((unsigned char) *vers->tag))
				ci->rev = xstrdup (vers->tag);
			else
				ci->rev = RCS_whatbranch (finfo->rcs, vers->tag);
		}
	    else
			ci->rev = (char *) NULL;
	    ci->tag = xstrdup (vers->tag);
		ci->options = xstrdup (vers->options);
	    p->data = (char *) ci;
	    (void) addnode (cilist, p);
	    break;
	case T_UNKNOWN:
	    error (0, 0, "nothing known about `%s'", fn_root(finfo->fullname));
	    freevers_ts (&vers);
	    return (1);
	case T_UPTODATE:
	    break;
	default:
	    error (0, 0, "CVS internal error: unknown status %d", status);
	    break;
    }

    freevers_ts (&vers);
    return (0);
}

/*
 * By default, return the code that tells do_recursion to examine all
 * directories
 */
/* ARGSUSED */
static Dtype
check_direntproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	Vers_TS *vers;
	struct file_info finfo;
	Ctype type;

	if(hint!=R_PROCESS)
		return hint;

    if (!quiet)
	error (0, 0, "Examining %s", update_dir);

	if(!get_directory_finfo(repos,dir,update_dir,&finfo))
	{
		type = classify_file_internal(&finfo,&vers);
		freevers_ts(&vers);
		switch(type)
		{
			case T_UNKNOWN:
			case T_UPTODATE:
				break;
			default:
				TRACE(3,"classify_file_internal returned %d\n",type);
				error (0, 0, "Up-to-date check failed for directory `%s'", last_component(virtual_repository));
				dellist(&finfo.entries);
				return R_ERROR;
		}
		dellist(&finfo.entries);
	}

    return (R_PROCESS);
}

/*
 * Walklist proc to run pre-commit checks
 */
static int precommit_list_proc (Node *p, void *closure)
{
    struct logfile_info *li;

    li = (struct logfile_info *) p->data;
    if (li->type == T_ADDED
	|| li->type == T_MODIFIED
	|| li->type == T_REMOVED)
    {
		if(((long)closure)==1) /* dll call */
		{
			int pos = precommit_list_count++;
			if(pos==precommit_list_size)
			{
				if(precommit_list_size<64) precommit_list_size=64;
				else precommit_list_size*=2;
				precommit_list=(const char**)xrealloc((void*)precommit_list,precommit_list_size*sizeof(char*));
			}
			precommit_list[pos]=p->key;
		}
		else
		{
			char *file;
			shell_escape(&file,p->key);
			fprintf((FILE*)closure,"%s\n",file);
			xfree(file);
		}
    }
    return (0);
}

/*
 * Callback proc for pre-commit checking
 */
static int precommit_proc(void *param, const trigger_interface *cb)
{
	int ret = 0;
	precommit_params_t *args = (precommit_params_t *)param;

	TRACE(1,"precommit_proc(%s)",PATCH_NULL(args->directory));

	if(cb->precommit)
	{
		precommit_list_size=precommit_list_count=0;
		walklist(saved_ulist, precommit_list_proc, (void*)1);
		ret = cb->precommit(cb,precommit_list_count,precommit_list,args->message,args->directory);
		xfree(precommit_list);
	}
	return ret;
}

/*
 * Run the pre-commit checks for the dir
 */
/* ARGSUSED */
static int check_filesdoneproc (void *callerdat, int err, char *repos, char *update_dir, List *entries)
{
    int n;
    Node *p;

    /* find the update list for this dir */
    p = findnode_fn (mulist, update_dir);
    if (p != NULL)
	saved_ulist = ((struct master_lists *) p->data)->ulist;
    else
	saved_ulist = (List *) NULL;

    /* skip the checks if there's nothing to do */
    if (saved_ulist == NULL || saved_ulist->list->next == saved_ulist->list)
	return (err);

	precommit_params_t args;
	args.message=saved_message;
	args.directory=Short_Repository(repos);

    /* run any pre-commit checks */
    TRACE(3,"run precommit trigger");
    if ((n = run_trigger(&args, precommit_proc)) > 0)
    {
		error (0, 0, "Pre-commit check failed");
		err += n;
    }

    return (err);
}

/*
 * Do the work of committing a file
 */
static int maxrev;
static char *sbranch;

/* ARGSUSED */
static int commit_fileproc (void *callerdat, struct file_info *finfo)
{
    Node *p;
    int err = 0;
    List *ulist, *cilist;
    struct commit_info *ci;
	Entnode *ent = NULL;
    const char *dir;
	char *options = NULL;

	p = findnode_fn(finfo->entries, finfo->file);
	if(p)
		ent = (Entnode*)p->data;


    /* Keep track of whether write_dirtag is a branch tag.
       Note that if it is a branch tag in some files and a nonbranch tag
       in others, treat it as a nonbranch tag.  It is possible that case
       should elicit a warning or an error.  */
    if (write_dirtag != NULL && finfo->rcs != NULL)
    {
		char *rev = RCS_getversion (finfo->rcs, write_dirtag, NULL, 1, NULL);
		if (rev != NULL
			&& !RCS_nodeisbranch (finfo->rcs, write_dirtag))
			write_dirnonbranch = 1;
		if (rev != NULL)
			xfree (rev);
    }

    if (finfo->update_dir[0] == '\0')
		p = findnode_fn (mulist, ".");
    else
		p = findnode_fn (mulist, finfo->update_dir);

    /*
     * if p is null, there were file type command line args which were
     * all up-to-date so nothing really needs to be done
     */
    if (p == NULL)
		return (0);
    ulist = ((struct master_lists *) p->data)->ulist;
    cilist = ((struct master_lists *) p->data)->cilist;
    
    p = findnode_fn (cilist, finfo->file);
    if (p == NULL)
		return (0);

    dir = finfo->repository;

	if(ent)
		options = xstrdup(ent->options);

    /*
     * At this point, we should have the commit message unless we were called
     * with files as args from the command line.  In that latter case, we
     * need to get the commit message ourselves
     */
    if (!(got_message))
    {
	got_message = 1;
	if (use_editor)
	    do_editor (finfo->update_dir, &saved_message,
		       finfo->repository, ulist);
	err = do_verify (&saved_message, finfo->repository);
	if (err)
	    error (1, 0, "correct above errors first!");
    }

    ci = (struct commit_info *) p->data;
    if (ci->status == T_MODIFIED)
    {
		if (finfo->rcs == NULL)
			error (1, 0, "internal error: no parsed RCS file");
		if(slide_tags && ci->rev && numdots(ci->rev)==2)
		{
			char *rev = xstrdup(ci->rev);
			*strrchr(rev,'.')='\0';
			if(!strcmp(rev,ent->version)) // Are we really on the parent branch
			{
				char *bra = RCS_branch_head(finfo->rcs, rev);
				char *tmpfile = cvs_temp_name ();
				TRACE(3,"Checking out revision %s for difference check",bra);
				int status = RCS_checkout (finfo->rcs, NULL, bra, NULL, ci->options, tmpfile, NULL, NULL, NULL);
				if (status > 0)
				{
					err = 1;
					goto out;
				}
				status = diff_exec (finfo->file, tmpfile, NULL, NULL, "", DEVNULL);
				TRACE(3,"diff_exec returned status %d",status);
				if(!status)
				{
					ci->status = T_UPTODATE;
					xfree(rev);
					rev = (char*)xmalloc(strlen(bra)+strlen(ci->rev)+4);
					strcpy(rev,bra);
					strcat(rev,".0");
					strcat(rev,strrchr(ci->rev,'.'));

					RCS_settag(finfo->rcs,ci->tag,rev,current_date);

					Vers_TS * vers = Version_TS(finfo,ci->options,ci->tag,NULL,0,0,0);
					checkout_file(finfo,vers,0,0,1,0,0,NULL,NULL);
					Register(finfo->entries, finfo->file, vers->vn_rcs, vers->ts_rcs,
						vers->options, vers->tag, vers->date, vers->ts_conflict, NULL, NULL, (time_t)-1, NULL, NULL, NULL, NULL);
					freevers_ts(&vers);

					RCS_rewrite(finfo->rcs,NULL,NULL,0);
				}
				xfree(bra);
				xfree(tmpfile);
			}
			xfree(rev);
		}
		else
		{
			if (lock_RCS (finfo->file, finfo->rcs, ci->rev,
					finfo->repository) != 0)
			{
				unlockrcs (finfo->rcs);
				err = 1;
				goto out;
			}
		}
    }
    else if (ci->status == T_ADDED)
    {
	if (checkaddfile (finfo->file, finfo->mapped_file, finfo->update_dir, finfo->repository, ci->tag, options,
			  &finfo->rcs, NULL, bugid.size()?bugid.c_str():(ent?ent->edit_bugid:NULL)) != 0)
	{
	    fixaddfile (finfo->file, finfo->repository);
	    err = 1;
	    goto out;
	}

	/* adding files with a tag, now means adding them on a branch.
	   Since the branch test was done in check_fileproc for
	   modified files, we need to stub it in again here. */

	if (ci->tag

	    /* If numeric, it is on the trunk; check_fileproc enforced
	       this.  */
	    && !isdigit ((unsigned char) ci->tag[0]))
	{
	    if (finfo->rcs == NULL)
		error (1, 0, "internal error: no parsed RCS file");
	    if (ci->rev)
		xfree (ci->rev);
	    ci->rev = RCS_whatbranch (finfo->rcs, ci->tag);
	    err = Checkin ('A', finfo, finfo->rcs->path, ci->rev,
			ci->tag, options, saved_message, ent?ent->merge_from_tag_1:NULL, ent?ent->merge_from_tag_2:NULL, NULL, bugid.size()?bugid.c_str():(ent?ent->edit_bugid:NULL), NULL, NULL, NULL);
	    if (err != 0)
	    {
			unlockrcs (finfo->rcs);
			fixbranch (finfo->rcs, sbranch);
	    }

	    (void) time (&last_register_time);

	    ci->status = T_UPTODATE;
	}
    }

    /*
     * Add the file for real
     */
    if (ci->status == T_ADDED)
    {
	char *xrev = (char *) NULL;

	if (ci->rev == NULL)
	{
	    /* find the max major rev number in this directory */
	    maxrev = 0;
	    (void) walklist (finfo->entries, findmaxrev, NULL);
	    if (finfo->rcs->head) {
		/* resurrecting: include dead revision */
		int thisrev = atoi (finfo->rcs->head);
		if (thisrev > maxrev)
		    maxrev = thisrev;
	    }
	    if (maxrev == 0)
		maxrev = 1;
	    xrev = (char*)xmalloc (20);
	    (void) sprintf (xrev, "%d", maxrev);
	}

	/* XXX - an added file with symbolic -r should add tag as well */
	err = finaladd (finfo, ci->rev ? ci->rev : xrev, ci->tag, options, ent?ent->merge_from_tag_1:NULL, ent?ent->merge_from_tag_2:NULL,bugid.size()?bugid.c_str():(ent?ent->edit_bugid:NULL));
	if (xrev)
	    xfree (xrev);
    }
    else if (ci->status == T_MODIFIED)
    {
		if(force_ci)
		{
			kflag kf1,kf2;
			RCS_get_kflags(options,false,kf1);
			RCS_get_kflags(ci->options,false,kf2);
			/* It's possible someone will try to make a -k1 file out of a non-k1 file...*/
			if((kf1.flags&KFLAG_SINGLE) && !(kf2.flags&KFLAG_SINGLE))
			{
				kf1.flags&=~KFLAG_SINGLE;
				xfree(options);
				options = (char*)xmalloc(128);
				RCS_rebuild_options(&kf1,options);
			}
			/* Or de-k1 a file... */
			else if(!(kf1.flags&KFLAG_SINGLE) && (kf2.flags&KFLAG_SINGLE))
			{
				kf1.flags|=KFLAG_SINGLE;
				xfree(options);
				options = (char*)xmalloc(128);
				RCS_rebuild_options(&kf1,options);
			}
		}

		err = Checkin ('M', finfo,
		       finfo->rcs->path, ci->rev, ci->tag,
			   force_ci?options:ci->options, saved_message, ent?ent->merge_from_tag_1:NULL, ent?ent->merge_from_tag_2:NULL, NULL, bugid.size()?bugid.c_str():(ent?ent->edit_bugid:NULL),
			   commit_keep_edits?(ent?ent->edit_revision:NULL):NULL,
			   commit_keep_edits?(ent?ent->edit_tag:NULL):NULL,
			   commit_keep_edits?(ent?ent->edit_bugid:NULL):NULL);

		(void) time (&last_register_time);

		if (err != 0)
		{
			unlockrcs (finfo->rcs);
			fixbranch (finfo->rcs, sbranch);
		}
    }
    else if (ci->status == T_REMOVED)
    {
		err = remove_file (finfo, ci->tag, saved_message, bugid.c_str());
#ifdef SERVER_SUPPORT
	if (server_active) {
	    server_scratch_entry_only ();
	    server_updated (finfo,
			    NULL,

			    /* Doesn't matter, it won't get checked.  */
			    SERVER_UPDATED,

			    (mode_t) -1,
			    NULL,
			    (struct buffer *) NULL);
	}
#endif
    }

    /* Clearly this is right for T_MODIFIED.  I haven't thought so much
       about T_ADDED or T_REMOVED.  */
	if(!commit_keep_edits)
		err += notify_do ('C', finfo->file, getcaller (), NULL, NULL, NULL, NULL, finfo->repository, ci->tag, NULL, NULL, NULL);

	do_unlock_file(ci->lockId);

out:
	xfree(options);
    if (err != 0/* || ci->status == T_UPTODATE*/)
    {
	/* on failure, remove the file from ulist */
	p = findnode_fn (ulist, finfo->file);
	if (p)
	    delnode (p);
    }
    else
    {
	/* On success, retrieve the new version number of the file and
           copy it into the log information (see logmsg.c
           (logfile_write) for more details).  We should only update
           the version number for files that have been added or
           modified but not removed.  Why?  classify_file_internal
           will return the version number of a file even after it has
           been removed from the archive, which is not the behavior we
           want for our commitlog messages; we want the old version
           number and then "NONE." */

	if (ci->status != T_REMOVED)
	{
	    p = findnode_fn (ulist, finfo->file);
	    if (p)
	    {
		Vers_TS *vers;
		struct logfile_info *li;

		(void) classify_file_internal (finfo, &vers);
		li = (struct logfile_info *) p->data;
		li->rev_new = xstrdup (vers->vn_rcs);
		freevers_ts (&vers);
	    }
	}
    }

    return (err);
}

/*
 * Log the commit and clean up the update list
 */
/* ARGSUSED */
static int commit_filesdoneproc (void *callerdat, int err, char *repository, char *update_dir, List *entries)
{
    Node *p;
    List *ulist;

	p = findnode_fn (mulist, update_dir);
    if (p == NULL)
	return (err);

    ulist = ((struct master_lists *) p->data)->ulist;

    got_message = 0;

    Update_Logfile (repository, saved_message, (FILE *) 0, ulist, bugid.c_str());

    /* Build the administrative files if necessary.  */
    {
	char *p;

	if (fnncmp (current_parsed_root->directory, repository,
		     strlen (current_parsed_root->directory)) != 0)
	{
	    error (0, 0,
		 "internal error: repository (%s) doesn't begin with root (%s)",
		   repository, current_parsed_root->directory);
	}
	p = repository + strlen (current_parsed_root->directory);
	if (*p == '/')
	    ++p;
	if ((fncmp ("CVSROOT", p) == 0)
	    /* Check for subdirectories because people may want to create
	       subdirectories and list files therein in checkoutlist.  */
	    || (fnncmp ("CVSROOT/", p, strlen ("CVSROOT/")) == 0)
	    )
	{
	    /* "Database" might a little bit grandiose and/or vague,
	       but "checked-out copies of administrative files, unless
	       in the case of modules and you are using ndbm in which
	       case modules.{pag,dir,db}" is verbose and excessively
	       focused on how the database is implemented.  */

	    /* mkmodules requires the absolute name of the CVSROOT directory.
	       Remove anything after the `CVSROOT' component -- this is
	       necessary when committing in a subdirectory of CVSROOT.  */
	    char *admin_dir = xstrdup(repository);
	    assert(admin_dir[p - repository + strlen("CVSROOT")] == '\0'
		    || admin_dir[p - repository + strlen("CVSROOT")] == '/');

		p = admin_dir + (p - repository) + strlen("CVSROOT");
		*p='\0';
		
	    cvs_output (program_name, 0);
	    cvs_output (" ", 1);
	    cvs_output (command_name, 0);
	    cvs_output (": Rebuilding administrative file database\n", 0);
	    mkmodules (admin_dir);
	    xfree (admin_dir);
	}
    }

    return (err);
}

/*
 * Get the log message for a dir
 */
/* ARGSUSED */
static Dtype
commit_direntproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
    Node *p;
    List *ulist;
    char *real_repos;
	int err;

	if(hint!=R_PROCESS)
		return hint;

    /* find the update list for this dir */
    p = findnode_fn (mulist, update_dir);
    if (p != NULL)
		ulist = ((struct master_lists *) p->data)->ulist;
    else
		ulist = (List *) NULL;

    /* skip the files as an optimization */
    if (ulist == NULL || ulist->list->next == ulist->list)
		return (R_SKIP_FILES);

    /* get commit message */
    real_repos = Name_Repository (dir, update_dir);
    got_message = 1;
    if (use_editor)
		do_editor (update_dir, &saved_message, real_repos, ulist);
    err = do_verify (&saved_message, real_repos);
	if (err)
	    error (1, 0, "correct above errors first!");

    xfree (real_repos);
    return (R_PROCESS);
}

/*
 * Process the post-commit proc if necessary
 */
/* ARGSUSED */
static int commit_dirleaveproc (void *callerdat, char *dir, int err, char *update_dir, List *entries)
{
    /* update the per-directory tag info */
    /* FIXME?  Why?  The "commit examples" node of cvs.texinfo briefly
       mentions commit -r being sticky, but apparently in the context of
       this being a confusing feature!  */

    if (err == 0)
	{
		char *repos = Name_Repository (NULL, update_dir);

		/* Commit any pending renames to the persistent rename file */
		commit_directory(update_dir, repos, saved_message);

		if(!write_dirtag)
		{
			ParseTag(&write_dirtag,NULL,&write_dirnonbranch,NULL);
			WriteTag(NULL, write_dirtag, NULL, write_dirnonbranch, update_dir, repos, NULL);
			xfree(write_dirtag);
			write_dirnonbranch=0;
		}
		else
			WriteTag(NULL, write_dirtag, NULL, write_dirnonbranch, update_dir, repos, NULL);

		xfree (repos);
	}

    return (err);
}

/*
 * find the maximum major rev number in an entries file
 */
static int findmaxrev (Node *p, void *closure)
{
    int thisrev;
    Entnode *entdata;

    entdata = (Entnode *) p->data;
    if (entdata->type != ENT_FILE)
	return (0);
    thisrev = atoi (entdata->version);
    if (thisrev > maxrev)
	maxrev = thisrev;
    return (0);
}

/*
 * Actually remove a file by moving it to the attic
 * XXX - if removing a ,v file that is a relative symbolic link to
 * another ,v file, we probably should add a ".." component to the
 * link to keep it relative after we move it into the attic.

   Return value is 0 on success, or >0 on error (in which case we have
   printed an error message).  */
static int remove_file (struct file_info *finfo, const char *tag, const char *message, const char *bugid)
{
    int retcode;

    int branch;
    int lockflag;
    char *corev;
    char *rev;
    char *prev_rev;
    char *old_path;
	char *options;

    corev = NULL;
    rev = NULL;
    prev_rev = NULL;

    retcode = 0;

    if (finfo->rcs == NULL)
	error (1, 0, "internal error: no parsed RCS file");

    branch = 0;
    if (tag && !(branch = RCS_nodeisbranch (finfo->rcs, tag)))
    {
	/* a symbolic tag is specified; just remove the tag from the file */
	if ((retcode = RCS_deltag (finfo->rcs, tag)) != 0)
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag `%s' from `%s'", tag,
		       fn_root(finfo->fullname));
	    return (1);
	}
	RCS_rewrite (finfo->rcs, NULL, NULL, 0);
	Scratch_Entry (finfo->entries, finfo->file);
	return (0);
    }

    /* we are removing the file from either the head or a branch */
    /* commit a new, dead revision. */

    /* Print message indicating that file is going to be removed. */
    cvs_output ("Removing ", 0);
    cvs_output (fn_root(finfo->fullname), 0);
    cvs_output (";\n", 0);

    rev = NULL;
    lockflag = 1;
    if (branch)
    {
	char *branchname;

	rev = RCS_whatbranch (finfo->rcs, tag);
	if (rev == NULL)
	{
	    error (0, 0, "cannot find branch \"%s\".", tag);
	    return (1);
	}

	branchname = RCS_getbranch (finfo->rcs, rev, 1);
	if (branchname == NULL)
	{
	    /* no revision exists on this branch.  use the previous
	       revision but do not lock. */
	    corev = RCS_gettag (finfo->rcs, tag, 1, (int *) NULL);
	    prev_rev = xstrdup(rev);
	    lockflag = 0;
	} else
	{
	    corev = xstrdup (rev);
	    prev_rev = xstrdup(branchname);
	    xfree (branchname);
	}

    } else  /* Not a branch */
    {
        /* Get current head revision of file. */
	prev_rev = RCS_head (finfo->rcs);
    }

    /* if removing without a tag or a branch, then make sure the default
       branch is the trunk. */
    if (!tag && !branch)
    {
        if (RCS_setbranch (finfo->rcs, NULL) != 0)
	{
	    error (0, 0, "cannot change branch to default for %s",
		   fn_root(finfo->fullname));
	    return (1);
	}
	RCS_rewrite (finfo->rcs, NULL, NULL, 0);
    }

	char *real_rev = RCS_branch_head(finfo->rcs,rev?corev:NULL);
	options = RCS_getexpand(finfo->rcs, real_rev);

    /* check something out.  Generally this is the head.  If we have a
       particular rev, then name it.  */
    retcode = RCS_checkout (finfo->rcs, finfo->file, rev ? corev : NULL,
			    options, (char *) NULL, RUN_TTY,
			    (RCSCHECKOUTPROC) NULL, (void *) NULL, NULL);
    if (retcode != 0)
    {
	error (0, 0,
	       "failed to check out `%s'", fn_root(finfo->fullname));
	return (1);
    }

    /* Except when we are creating a branch, lock the revision so that
       we can check in the new revision.  */
    if (lockflag)
    {
	if (RCS_lock (finfo->rcs, rev ? corev : NULL, 1) == 0)
	    RCS_rewrite (finfo->rcs, NULL, NULL, 0);
    }

	xfree (corev);

    retcode = RCS_checkin (finfo->rcs, finfo->file, message, rev, options,
			   RCS_FLAGS_DEAD | RCS_FLAGS_QUIET, NULL, NULL, NULL, NULL, bugid, &variable_list);
	xfree(options);
	xfree(rev);
    if (retcode	!= 0)
    {
		if (!quiet)
			error (0, retcode == -1 ? errno : 0,
			"failed to commit dead revision for `%s'", fn_root(finfo->fullname));
		xfree(real_rev);
		return (1);
    }
    /* At this point, the file has been committed as removed.  We should
       probably tell the history file about it  */
    history_write ('R', NULL, real_rev, finfo->file, finfo->repository, bugid, message);

	xfree(real_rev);

    old_path = xstrdup (finfo->rcs->path);

    /* Print message that file was removed. */
    cvs_output (fn_root(old_path), 0);
    cvs_output ("  <--  ", 0);
    cvs_output (fn_root(finfo->file), 0);
    cvs_output ("\nnew revision: delete; previous revision: ", 0);
    cvs_output (prev_rev, 0);
    cvs_output ("\ndone\n", 0);

	xfree(prev_rev);
    xfree (old_path);

    Scratch_Entry (finfo->entries, finfo->file);
    return 0;
}

/*
 * Do the actual checkin for added files
 */
static int finaladd(struct file_info *finfo, char *rev, char *tag, char *options, const char *merge_tag1, const char *merge_tag2, const char *bugid)
{
    int ret;
    char *rcs;
	char *opt;

	/* Set RCS keyword expansion options.  */
	/* If we're ignoring client side cvswrappers, then override the options string */
	if(server_active && compat[compat_level].ignore_client_wrappers)
	{
		opt = wrap_rcsoption(finfo->file);
		if(!opt) opt="kv";
	}
	else
		opt = options;

	assert(opt);

    rcs = locate_rcs (finfo->file, finfo->repository);
    ret = Checkin('A', finfo, rcs, rev, tag, opt, saved_message, merge_tag1, merge_tag2, NULL, bugid, NULL, NULL, NULL);
    if (ret == 0)
    {
		char *tmp = (char*)xmalloc (strlen (finfo->file) + sizeof (CVSADM)
			     + sizeof (CVSEXT_LOG) + 10);
	(void) sprintf (tmp, "%s/%s%s", CVSADM, finfo->file, CVSEXT_LOG);
	if (unlink_file (tmp) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", tmp);
	xfree (tmp);
    }
    else
	fixaddfile (finfo->file, finfo->repository);

    (void) time (&last_register_time);
    xfree (rcs);

    return (ret);
}

/*
 * Unlock an rcs file
 */
static void unlockrcs (RCSNode *rcs)
{
    int retcode;

    if ((retcode = RCS_unlock (rcs, NULL, 1)) != 0)
	error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
	       "could not unlock %s", fn_root(rcs->path));
    else
	RCS_rewrite (rcs, NULL, NULL, 0);
}

/*
 * remove a partially added file.  if we can parse it, leave it alone.
 */
void fixaddfile (const char *file, const char *repository)
{
    RCSNode *rcsfile;
    char *rcs;
    int save_really_quiet;

    rcs = locate_rcs (file, repository);
	if(isfile(rcs))
	{
		save_really_quiet = really_quiet;
		really_quiet = 1;
		if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
		{
			if (unlink_file (rcs) < 0)
				error (0, errno, "cannot remove %s", rcs);
		}
		else
		{
			freercsnode (&rcsfile);
			really_quiet = save_really_quiet;
			xfree (rcs);
		}
	}
}

/*
 * put the branch back on an rcs file
 */
static void fixbranch (RCSNode *rcs, char *branch)
{
    int retcode;

    if (branch != NULL)
    {
	if ((retcode = RCS_setbranch (rcs, branch)) != 0)
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		   "cannot restore branch to %s for %s", branch, fn_root(rcs->path));
	RCS_rewrite (rcs, NULL, NULL, 0);
    }
}

/*
 * do the initial part of a file add for the named file.  if adding
 * with a tag, put the file in the Attic and point the symbolic tag
 * at the committed revision.
 */

int checkaddfile (const char *file, const char *mapped_file, const char *dir, const char *repository, const char *tag, const char *options, RCSNode **rcsnode, RCSCHECKINPROC callback, const char *bugid)
{
    char *rcs;
    char *fname;
    int retcode = 0;
    int newfile = 0;
    RCSNode *rcsfile = NULL;
    int retval;
    int adding_on_branch;

    /* Callers expect to be able to use either "" or NULL to mean the
       default keyword expansion.  */
    if (options != NULL && options[0] == '\0')
		options = NULL;

    /* If numeric, it is on the trunk; check_fileproc enforced
       this.  */
   adding_on_branch = tag != NULL && !isdigit ((unsigned char) tag[0]);

	rcs = locate_rcs (file, repository);

	/* If this is a previously deleted file, then try to find an unused filename */
	/* Not very efficient, but it doesn't really need to be I expect */
	if(!mapped_file && isreadable(rcs))
	{
		int c = 1;
		char *file1,*file2;
		xfree(rcs);
		rcs = (char*)xmalloc(strlen(repository)+strlen(file)+32);
		do
		{
			sprintf(rcs,"%s/%s_%d%s",repository,file,c,RCSEXT);
			c++;
		} while(isreadable(rcs));
		file1 = (char*)xmalloc(strlen(rcs)+1);
		file2 = (char*)xmalloc(strlen(rcs)+1);
		sprintf(file1,"%s/%s",relative_repos(repository),file);
		sprintf(file2,"%s/%s_%d",relative_repos(repository),file,c-1);
		add_mapping(dir,file2,file1);
		xfree(file1);
		xfree(file2);
	}
	else if(!mapped_file)
	{
		char *file1 = (char*)xmalloc(strlen(rcs)+1);
		sprintf(file1,"%s/%s",relative_repos(repository),file);
		add_mapping(dir,file1,file1); /* Adding a mapping to self automatically overrides the earlier delete */
	}

    if (isreadable (rcs))
    {
	/* file has existed in the past.  Prepare to resurrect. */
	char *rev;

	if ((rcsfile = *rcsnode) == NULL)
	{
	    error (0, 0, "could not find parsed rcsfile %s", file);
	    retval = 1;
	    goto out;
	}

	rev = RCS_getversion (rcsfile, tag, NULL, 1, (int *) NULL);
	/* and lock it */
	if (lock_RCS ((char*)file, rcsfile, rev, repository))
	{
	    error (0, 0, "cannot lock `%s'.", rcs);
	    if (rev != NULL)
		xfree (rev);
	    retval = 1;
	    goto out;
	}

	if (rev != NULL)
	    xfree (rev);
    }
    else
    {
	/* this is the first time we have ever seen this file; create
	   an rcs file.  */

	char *desc;
	size_t descalloc;
	size_t desclen;
	kflag kf_empty;

	const char *opt;

	desc = NULL;
	descalloc = 0;
	desclen = 0;
	fname = (char*)xmalloc (strlen (file) + sizeof (CVSADM) +
			 + sizeof (CVSEXT_LOG) + 10);
	(void) sprintf (fname, "%s/%s%s", CVSADM, file, CVSEXT_LOG);
	/* If the file does not exist, no big deal.  In particular, the
	   server does not (yet at least) create CVSEXT_LOG files.  */
	if (isfile (fname))
	    /* FIXME: Should be including update_dir in the appropriate
	       place here.  */
	    get_file (fname, fname, "r", &desc, &descalloc, &desclen, kf_empty);
	xfree (fname);

	/* From reading the RCS 5.7 source, "rcs -i" adds a newline to the
	   end of the log message if the message is nonempty.
	   Do it.  RCS also deletes certain whitespace, in cleanlogmsg,
	   which we don't try to do here.  */
	if (desclen > 0)
	{
	    expand_string (&desc, &descalloc, desclen + 1);
	    desc[desclen++] = '\n';
	}

	/* Set RCS keyword expansion options.  */
	/* If we're ignoring client side cvswrappers, then override the options string */
	if(server_active && compat[compat_level].ignore_client_wrappers)
	{
		opt = wrap_rcsoption(file);
	}
	else
		opt = options;

	if(!opt)
		opt="kv";

	/* This message is an artifact of the time when this
	   was implemented via "rcs -i".  It should be revised at
	   some point (does the "initial revision" in the message from
	   RCS_checkin indicate that this is a new file?  Or does the
	   "RCS file" message serve some function?).  */
	cvs_output ("RCS file: ", 0);
	cvs_output (fn_root(rcs), 0);
	cvs_output ("\ndone\n", 0);

	if (add_rcs_file (NULL, rcs, (char*)file, NULL, opt,
			  NULL, NULL, 0, NULL,
			  desc, desclen, NULL, callback) != 0)
	{
	    retval = 1;
	    goto out;
	}
	rcsfile = RCS_parsercsfile (rcs);
	newfile = 1;
	if (desc != NULL)
	    xfree (desc);
	if (rcsnode != NULL)
	{
	    assert (*rcsnode == NULL);
	    *rcsnode = rcsfile;
	}
    }

    /* when adding a file for the first time, and using a tag, we need
       to create a dead revision on the trunk.  */
    if (adding_on_branch)
    {
	if (newfile)
	{
	    char *tmp;
	    FILE *fp;

	    /* move the new file out of the way. */
	    fname = (char*)xmalloc (strlen (file) + sizeof (CVSADM) +
			     + sizeof (CVSPREFIX) + 10);
	    (void) sprintf (fname, "%s/%s%s", CVSADM, CVSPREFIX, file);
	    rename_file (file, fname);

	    /* Create empty FILE.  Can't use copy_file with a DEVNULL
	       argument -- copy_file now ignores device files. */
	    fp = fopen (file, "w");
	    if (fp == NULL)
		error (1, errno, "cannot open %s for writing", file);
	    if (fclose (fp) < 0)
		error (0, errno, "cannot close %s", file);

	    tmp = (char*)xmalloc (strlen (file) + strlen (tag) + 80);
	    /* commit a dead revision. */
	    (void) sprintf (tmp, "file %s was initially added on branch %s.",
			    file, tag);
	    retcode = RCS_checkin(rcsfile, NULL, tmp, NULL, options,
				   RCS_FLAGS_DEAD | RCS_FLAGS_QUIET, NULL, NULL, NULL, NULL, bugid, &variable_list);
	    xfree (tmp);
	    if (retcode != 0)
	    {
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "could not create initial dead revision %s", rcs);
		retval = 1;
		goto out;
	    }

	    /* put the new file back where it was */
	    rename_file (fname, file);
	    xfree (fname);

	    /* double-check that the file was written correctly */
	    freercsnode (&rcsfile);
	    rcsfile = RCS_parse (file, repository);
	    if (rcsfile == NULL)
	    {
		error (0, 0, "could not read %s", rcs);
		retval = 1;
		goto out;
	    }
	    if (rcsnode != NULL)
		*rcsnode = rcsfile;

	    /* and lock it once again. */
	    if (lock_RCS ((char*)file, rcsfile, NULL, repository))
	    {
		error (0, 0, "cannot lock `%s'.", rcs);
		retval = 1;
		goto out;
	    }
	}

	/* when adding with a tag, we need to stub a branch, if it
	   doesn't already exist.  */

	if (rcsfile == NULL)
	{
	    if (rcsnode != NULL && *rcsnode != NULL)
		rcsfile = *rcsnode;
	    else
	    {
		rcsfile = RCS_parse (file, repository);
		if (rcsfile == NULL)
		{
		    error (0, 0, "could not read %s", rcs);
		    retval = 1;
		    goto out;
		}
	    }
	}

	if (!RCS_nodeisbranch (rcsfile, tag))
	{
	    /* branch does not exist.  Stub it.  */
	    char *head;
	    char *magicrev;

	    head = RCS_getversion (rcsfile, NULL, NULL, 0, (int *) NULL);
	    magicrev = RCS_magicrev (rcsfile, head);

	    retcode = RCS_settag (rcsfile, tag, magicrev, current_date);
	    RCS_rewrite (rcsfile, NULL, NULL, 0);

	    xfree (head);
	    xfree (magicrev);

	    if (retcode != 0)
	    {
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "could not stub branch %s for %s", tag, rcs);
		retval = 1;
		goto out;
	    }
	}
	else
	{
	    /* lock the branch. (stubbed branches need not be locked.)  */
	    if (lock_RCS ((char*)file, rcsfile, NULL, repository))
	    {
		error (0, 0, "cannot lock `%s'.", rcs);
		retval = 1;
		goto out;
	    }
	}

	if (rcsnode && *rcsnode != rcsfile)
	{
	    freercsnode(rcsnode);
	    *rcsnode = rcsfile;
	}
    }

    fileattr_newfile (file);

    /* At this point, we used to set the file mode of the RCS file
       based on the mode of the file in the working directory.  If we
       are creating the RCS file for the first time, add_rcs_file does
       this already.  If we are re-adding the file, then perhaps it is
       consistent to preserve the old file mode, just as we preserve
       the old keyword expansion mode.

       If we decide that we should change the modes, then we can't do
       it here anyhow.  At this point, the RCS file may be owned by
       somebody else, so a chmod will fail.  We need to instead do the
       chmod after rewriting it.

       FIXME: In general, I think the file mode (and the keyword
       expansion mode) should be associated with a particular revision
       of the file, so that it is possible to have different revisions
       of a file have different modes.  */

    retval = 0;

 out:
    if (retval != 0)
	    xfree (rcs);
    return retval;
}

/*
 * Attempt to place a lock on the RCS file; returns 0 if it could and 1 if it
 * couldn't.  If the RCS file currently has a branch as the head, we must
 * move the head back to the trunk before locking the file, and be sure to
 * put the branch back as the head if there are any errors.
 */
static int lock_RCS (const char *user, RCSNode *rcs, const char *rev, const char *repository)
{
    char *branch = NULL;
    int err = 0;

    /*
     * For a specified, numeric revision of the form "1" or "1.1", (or when
     * no revision is specified ""), definitely move the branch to the trunk
     * before locking the RCS file.
     *
     * The assumption is that if there is more than one revision on the trunk,
     * the head points to the trunk, not a branch... and as such, it's not
     * necessary to move the head in this case.
     */
    if (rev == NULL || (rev && isdigit ((unsigned char) *rev) && numdots (rev) < 2))
    {
		branch = xstrdup (rcs->branch);
		if (branch != NULL)
		{
			if (RCS_setbranch (rcs, NULL) != 0)
			{
				error (0, 0, "cannot change branch to default for %s", fn_root(rcs->path));
				xfree (branch);
				return 1;
		    }
		}
		err = RCS_lock(rcs, NULL, 1);
    }
    else
    {
		RCS_lock(rcs, rev, 1);
    }

    /* We used to call RCS_rewrite here, and that might seem
       appropriate in order to write out the locked revision
       information.  However, such a call would actually serve no
       purpose.  CVS locks will prevent any interference from other
       CVS processes.  The comment above rcs_internal_lockfile
       explains that it is already unsafe to use RCS and CVS
       simultaneously.  It follows that writing out the locked
       revision information here would add no additional security.

       If we ever do care about it, the proper fix is to create the
       RCS lock file before calling this function, and maintain it
       until the checkin is complete.

       The call to RCS_lock is still required at present, since in
       some cases RCS_checkin will determine which revision to check
       in by looking for a lock.  FIXME: This is rather roundabout,
       and a more straightforward approach would probably be easier to
       understand.  */

    if (err == 0)
    {
		xfree (sbranch);
		sbranch = branch;
		return 0;
    }

    /* try to restore the branch if we can on error */
    if (branch != NULL)
		fixbranch (rcs, branch);

	xfree (branch);
    return 1;
}

/*
 * free an UPDATE node's data
 */
void update_delproc (Node *p)
{
    struct logfile_info *li;

    li = (struct logfile_info *) p->data;
	xfree (li->tag);
	xfree (li->bugid);
	xfree (li->rev_old);
	xfree (li->rev_new);
    xfree (li);
}

/*
 * Free the commit_info structure in p.
 */
static void ci_delproc (Node *p)
{
    struct commit_info *ci;

    ci = (struct commit_info *) p->data;
	xfree (ci->rev);
	xfree (ci->tag);
	xfree (ci->options);
    xfree (ci);
}

/*
 * Free the commit_info structure in p.
 */
static void masterlist_delproc (Node *p)
{
    struct master_lists *ml;

    ml = (struct master_lists *) p->data;
    dellist (&ml->ulist);
    dellist (&ml->cilist);
    xfree (ml);
}

/* Find an RCS file in the repository.  Most parts of CVS will want to
   rely instead on RCS_parse which performs a similar operation and is
   called by recurse.c which then puts the result in useful places
   like the rcs field of struct file_info.

   REPOSITORY is the repository (including the directory) and FILE is
   the filename within that directory (without RCSEXT).  Returns a
   newly-malloc'd array containing the absolute pathname of the RCS
   file that was found.  */
static char *locate_rcs (const char *file, const char *repository)
{
    char *rcs;

    rcs = (char*)xmalloc (strlen (repository) + strlen (file) + sizeof (RCSEXT) + 10);
    (void) sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
    if (!isreadable (rcs))
    {
	(void) sprintf (rcs, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);
	if (!isreadable (rcs))
	{
	    (void) sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
	}
    }
    return rcs;
}
