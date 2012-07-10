/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 * "update" updates the version in the present directory with respect to the RCS
 * repository.  The present version must have been created by "checkout". The
 * user can keep up-to-date by calling "update" whenever he feels like it.
 *
 * The present version can be committed by "commit", but this keeps the version
 * in tact.
 *
 * Arguments following the options are taken to be file names to be updated,
 * rather than updating the entire directory.
 *
 * Modified or non-existent RCS files are checked out and reported as U
 * <user_file>
 *
 * Modified user files are reported as M <user_file>.  If both the RCS file and
 * the user file have been modified, the user file is replaced by the result
 * of rcsmerge, and a backup file is written for the user in .#file.version.
 * If this throws up irreconcilable differences, the file is reported as C
 * <user_file>, and as M <user_file> otherwise.
 *
 * Files added but not yet committed are reported as A <user_file>. Files
 * removed but not yet committed are reported as R <user_file>.
 *
 * If the current directory contains subdirectories that hold concurrent
 * versions, these are updated too.  If the -d option was specified, new
 * directories added to the repository are automatically created and updated
 * as well.
 */

#include "cvs.h"
#include "savecwd.h"
#include "watch.h"
#include "fileattr.h"
#include "edit.h"
#include "getline.h"
#include "buffer.h"
#include "hardlink.h"

int checkout_file(struct file_info *finfo, Vers_TS *vers_ts,
				 int adding, int merging, int update_server, int resurrect, int is_rcs, const char *merge_rev1, const char *merge_rev2);
#ifdef SERVER_SUPPORT
static void checkout_to_buffer (void *, const char *, size_t);
#endif
#ifdef SERVER_SUPPORT
static int patch_file (struct file_info *finfo,
			      Vers_TS *vers_ts, 
			      int *docheckout, mode_t &mode,
			      CMD5Calc* md5);
static void patch_file_write (void *, const char *, size_t);
#endif
static int merge_file (struct file_info *finfo, Vers_TS *vers);
static int scratch_file (struct file_info *finfo, Vers_TS *vers);
static int update_predirent_proc(void *callerdat, char *dir,
					char *repository, char *update_dir,
					List *entries, const char *virtual_repository, Dtype hint);
static Dtype update_dirent_proc(void *callerdat, char *dir,
					char *repository, char *update_dir,
					List *entries, const char *virtual_repository, Dtype hint);
static int update_dirleave_proc (void *callerdat, char *dir,
					int err, char *update_dir,
					List *entries);
static int update_fileproc (void *callerdat, struct file_info *);
static int update_filesdone_proc (void *callerdat, int err,
					 char *repository, char *update_dir,
					 List *entries);
static void write_letter (struct file_info *finfo, int letter);
static void join_file (struct file_info *finfo, Vers_TS *vers_ts);
static int baserev_update(struct file_info *finfo, Vers_TS *vers, Ctype status);

static const char *options;
static const char *tag;
static const char *date;
static const char *merge_bugid;
/* This is a bit of a kludge.  We call WriteTag at the beginning
   before we know whether nonbranch is set or not.  And then at the
   end, once we have the right value for nonbranch, we call WriteTag
   again.  I don't know whether the first call is necessary or not.
   rewrite_tag is nonzero if we are going to have to make that second
   call.  */
static int rewrite_tag;
static int nonbranch;
static int run_module_prog = 1;
static int update_baserev;
static int inverse_merges;

/* If we set the tag or date for a subdirectory, we use this to undo
   the setting.  See update_dirent_proc.  */
static const char *tag_update_dir;

static const char *join_rev1, *date_rev1;
static const char *join_rev2, *date_rev2;
static int aflag = 0;
static int toss_local_changes;
static int force_tag_match = 1;
static int update_build_dirs;
static int update_prune_dirs;
static int pipeout;
static int is_rcs;
static int merge_from_branchpoint;
static int conflict_3way;
static int case_sensitive;
static int force_checkout_time;
#ifdef SERVER_SUPPORT
static int patches = 0;
static int rcs_diff_patches ;
#endif
static List *ignlist = (List *) NULL;
static time_t last_register_time;
static List *renamed_file_list;
static int edit_modified,file_is_edited;
static const char *edit_modified_bugid;

extern int client_overwrite_existing;

static const char *const update_usage[] =
{
    "Usage: %s %s [-ACPdfilRpbmt] [-k kopt] [-r rev] [-D date] [-j rev]\n",
    "    [-B bugid] [-I ign] [-W spec] [files...]\n",
    "\t-3\tProduce 3-way conflicts.\n",
    "\t-A\tReset any sticky tags/date/kopts.\n",
	"\t-B bugid\tPerform -j Merge bounded by bug.\n",
	"\t-b\tPerform -j merge from branch point.\n",
    "\t-C\tOverwrite locally modified files with clean repository copies.\n",
	"\t-c\tUpdate base revision copies.\n",
    "\t-D date\tSet date to update from (is sticky).\n",
    "\t-d\tBuild directories, like checkout does.\n",
	"\t-e[bugid]\tAutomatically edit modified/merged files.\n",
    "\t-f\tForce a head revision match if tag/date not found.\n",
       "\t-i\tUse bidirectional mergepoints (A->B == B->A).\n",
    "\t-I ign\tMore files to ignore (! to reset).\n",
    "\t-j rev\tMerge in changes made between current revision and rev.\n",
    "\t-k kopt\tUse RCS kopt -k option on checkout. (is sticky)\n",
    "\t-l\tLocal directory only, no recursion.\n",
	"\t-m\tPerform -j merge from last merge point (default).\n",
    "\t-P\tPrune empty directories.\n",
    "\t-p\tSend updates to standard output (avoids stickiness).\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-r rev\tUpdate using specified revision/tag (is sticky).\n",
	"\t-S\tSelect between conflicting case sensitive names.\n",
	"\t-t\tUpdate using last checkin time.\n",
    "\t-W spec\tWrappers specification line (! to reset).\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

/*
 * update is the argv,argc based front end for arg parsing
 */
int update (int argc, char **argv)
{
    int c, err;
    int local = 0;			/* recursive by default */
    int which;				/* where to look for files and dirs */

    if (argc == -1)
		usage (update_usage);

    /* parse the args */
    optind = 0;
	while ((c = getopt (argc, argv, "+AB:pCcPflRQqduk:r:D:j:bmI:W:3Stxe::i")) != -1)
    {
	switch (c)
	{
	    case 'A':
		aflag = 1;
		break;
		case'B':
			if(merge_bugid)
				error(1,0,"Cannot specify multiple -B options");
			if(!RCS_check_bugid(optarg,false))
				error(1,0,"Invalid characters in bug identifier.  Please avoid \"'");
			merge_bugid = optarg;
			break;
	    case 'C':
		toss_local_changes = 1;
		break;
		case 'c':
		update_baserev = 1;
		break;
	    case 'I':
		ign_add (optarg, 0);
		break;
	    case 'i':
		inverse_merges = 1;
		break;
	    case 'W':
		wrap_add (optarg, false, false, true, false); // Don't set fromCommand here
		break;
	    case 'k':
		if (options)
		    xfree (options);
		options = RCS_check_kflag (optarg,true,true);
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
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
	    case 'd':
		update_build_dirs = 1;
		break;
	    case 'f':
		force_tag_match = 0;
		break;
	    case 'r':
		tag = optarg;
		break;
	    case 'D':
		date = Make_Date (optarg);
		break;
	    case 'P':
		update_prune_dirs = 1;
		break;
	    case 'p':
		pipeout = 1;
		noexec = 1;		/* so no locks will be created */
		break;
		case 'o':
		pipeout = 1;
		noexec = 1;
		break;
	    case 'j':
		if (join_rev2)
		    error (1, 0, "only two -j options can be specified");
		if (join_rev1)
		    join_rev2 = optarg;
		else
		    join_rev1 = optarg;
		break;
		case 'b':
			merge_from_branchpoint = 1;
			break;
		case 'm':
			merge_from_branchpoint = 0;
			break;
	    case 'u':
#ifdef SERVER_SUPPORT
		if (server_active)
		{
		    patches = 1;
		    rcs_diff_patches = server_use_rcs_diff ();
		}
		else
#endif
		    usage (update_usage);
		break;
		case '3':
			conflict_3way = 1;
			break;
		case 'S':
			case_sensitive = 1;
			break;
		case 't':
			force_checkout_time = 1;
			break;
		case 'e':
			edit_modified = 1;
			edit_modified_bugid = optarg;
			break;
	    case '?':
	    default:
		usage (update_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

	if(tag && !strcmp(tag,"HEAD"))
	{
		aflag=1;
		tag = NULL;
	}

    if (current_parsed_root->isremote) 
    {
	int pass;

	/* The first pass does the regular update.  If we receive at least
	   one patch which failed, we do a second pass and just fetch
	   those files whose patches failed.  */
	pass = 1;
	do
	{
	    int status;

		if(!server_started)
			start_server(0); 

	    if (local)
		send_arg("-l");
	    if (update_build_dirs)
		send_arg("-d");
	    if (pipeout)
		send_arg("-p");
	    if (!force_tag_match)
		send_arg("-f");
	    if (aflag)
		send_arg("-A");
	    if (toss_local_changes)
		send_arg("-C");
	    if (update_prune_dirs)
		send_arg("-P");
	    if (inverse_merges)
		send_arg("-i");
	    client_prune_dirs = update_prune_dirs;
	    option_with_arg ("-r", tag);
	    if (options && options[0] != '\0')
			option_with_arg ("-k", options);
	    if (date)
		client_senddate (date);
	    if (join_rev1)
		option_with_arg ("-j", join_rev1);
	    if (join_rev2)
		option_with_arg ("-j", join_rev2);
		if (conflict_3way)
			send_arg("-3");
		if (force_checkout_time)
			send_arg("-t");
		if (update_baserev)
			send_arg("-c");
		if (edit_modified)
		{
			if(edit_modified_bugid)
				option_with_arg("-e", edit_modified_bugid);
			else
				send_arg("-e");
		}
		ign_send ();
	    wrap_send ();
		if (merge_from_branchpoint)
			send_arg("-b");
		if (merge_bugid)
			option_with_arg("-B", merge_bugid);

		client_overwrite_existing = case_sensitive;

	    if (failed_patches_count == 0)
	    {
                unsigned int flags = 0;

		/* If the server supports the command "update-patches", that 
		   means that it knows how to handle the -u argument to update,
		   which means to send patches instead of complete files.

		   We don't send -u if failed_patches != NULL, so that the
		   server doesn't try to send patches which will just fail
		   again.  At least currently, the client also clobbers the
		   file and tells the server it is lost, which also will get
		   a full file instead of a patch, but it seems clean to omit
		   -u.  */
		if (supported_request ("update-patches"))
		    send_arg ("-u");

        if (update_build_dirs)
            flags |= SEND_BUILD_DIRS;

        if (toss_local_changes) {
            flags |= SEND_NO_CONTENTS;
            flags |= BACKUP_MODIFIED_FILES;
        }

		if(case_sensitive)
			flags |= SEND_CASE_SENSITIVE;

		send_arg("--");
		/* If noexec, probably could be setting SEND_NO_CONTENTS.
		   Same caveats as for "cvs status" apply.  */

		send_files (argc, argv, local, aflag, flags);
		send_file_names (argc, argv, SEND_EXPAND_WILD|(case_sensitive?SEND_CASE_SENSITIVE:0));
	    }
	    else
	    {
		int i, flags = 0;

		(void) printf ("%s client: refetching unpatchable files\n",
			       program_name);

		if (toplevel_wd != NULL
		    && CVS_CHDIR (toplevel_wd) < 0)
		{
		    error (1, errno, "could not chdir to %s", toplevel_wd);
		}

		for (i = 0; i < failed_patches_count; i++)
		    if (unlink_file (failed_patches[i]) < 0
			&& !existence_error (errno))
			error (0, errno, "cannot remove %s",
			       failed_patches[i]);

		if(update_build_dirs)
			flags |= SEND_BUILD_DIRS;

		if(case_sensitive)
			flags |= SEND_CASE_SENSITIVE;

		send_files (failed_patches_count, failed_patches, local,
			    aflag, flags);
		send_file_names (failed_patches_count, failed_patches, 0);
		free_names (&failed_patches_count, failed_patches);
	    }

	    send_to_server ("update\n", 0);

	    status = get_responses_and_close(); 

	    /* If there are any conflicts, the server will return a
               non-zero exit status.  If any patches failed, we still
               want to run the update again.  We use a pass count to
               avoid an endless loop.  */

	    /* Notes: (1) assuming that status != 0 implies a
	       potential conflict is the best we can cleanly do given
	       the current protocol.  I suppose that trying to
	       re-fetch in cases where there was a more serious error
	       is probably more or less harmless, but it isn't really
	       ideal.  (2) it would be nice to have a testsuite case for the
	       conflict-and-patch-failed case.  */

	    if (status != 0 && (failed_patches_count == 0 || pass > 1))
	    {
			if (failed_patches_count > 0)
				free_names (&failed_patches_count, failed_patches);
			return status;
	    }

	    ++pass;
	} while (failed_patches_count > 0);

//	cleanup_and_close_server();

	return 0;
    }

    if (tag != NULL)
		tag_check_valid (tag, argc, argv, local, aflag, "");
    if (join_rev1 != NULL)
        tag_check_valid_join (join_rev1, argc, argv, local, aflag, "");
    if (join_rev2 != NULL)
        tag_check_valid_join (join_rev2, argc, argv, local, aflag, "");

    /*
     * If we are updating the entire directory (for real) and building dirs
     * as we go, we make sure there is no static entries file and write the
     * tag file as appropriate
     */
    if (argc <= 0 && !pipeout)
    {
	if (update_build_dirs)
	{
	    if (unlink_file (CVSADM_ENTSTAT) < 0 && ! existence_error (errno))
		error (1, errno, "cannot remove file %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
	    if (server_active)
	    {
			char *repos = Name_Repository (NULL, NULL);
			server_clear_entstat (".", repos);
			xfree (repos);
	    }
#endif
	}
    }

    /* look for files/dirs locally and in the repository */
    which = W_LOCAL | W_REPOS;

    /* call the command line interface */
    err = do_update (argc, argv, options, tag, date, force_tag_match,
		     local, update_build_dirs, aflag, update_prune_dirs,
		     pipeout, which, join_rev1, join_rev2, (char *) NULL, (char*) NULL);

    /* free the space Make_Date allocated if necessary */
    if (date != NULL)
	xfree (date);

	if(!noexec)
	{
    DBM *db = open_module ();
	int i;
	if(argc)
	{
		for (i = 0; i < argc; i++)
		{
		    const char *repos;
			char *dir = xstrdup(argv[i]);
			
			if(!isdir(dir))
			{
				char *p = (char*)last_component(dir);
				if(p && p>dir) p[-1]='\0';
				else xfree(dir);
			}
			if(dir && isdir(dir))
			{
				repos = Name_Repository (dir, dir?dir:".");
				err += do_module (db, (char*)relative_repos(repos), mtUPDATE, "Updating", NULL,
					NULL, 0, local, run_module_prog, 0,
					(char *) NULL);
				xfree(repos);
			}
			xfree(dir);
		}
	}
	else
	{
	    const char *repos = Name_Repository (NULL, NULL);
		err += do_module (db, (char*)relative_repos(repos), mtUPDATE, "Updating", NULL,
			  NULL, 0, local, run_module_prog, 0,
			  (char *) NULL);
		xfree(repos);
	}
    close_module (db);
	}

    return (err);
}

/*
 * Command line interface to update (used by checkout)
 */
int do_update (int argc, char **argv, const char *xoptions, const char *xtag, const char *xdate, int xforce, int local, int xbuild,
    int xaflag, int xprune, int xpipeout, int which, const char *xjoin_rev1, const char *xjoin_rev2, const char *preload_update_dir,
    const char *preload_repository)
{
    int err = 0;
    char *cp;

    /* fill in the statics */
    options = xoptions;
    tag = xtag;
    date = xdate;
    force_tag_match = xforce;
    update_build_dirs = xbuild;
    aflag = xaflag;
    update_prune_dirs = xprune;
    pipeout = xpipeout;
	is_rcs = 0;

    /* setup the join support */
    join_rev1 = xjoin_rev1;
    join_rev2 = xjoin_rev2;
    if (join_rev1 && (cp = (char*)strchr (join_rev1, ':')) != NULL)
    {
	*cp++ = '\0';
	date_rev1 = Make_Date (cp);
    }
    else
	date_rev1 = (char *) NULL;
    if (join_rev2 && (cp = (char*)strchr (join_rev2, ':')) != NULL)
    {
	*cp++ = '\0';
	date_rev2 = Make_Date (cp);
    }
    else
	date_rev2 = (char *) NULL;

    /* call the recursion processor */
    err = start_recursion (update_fileproc, update_filesdone_proc, update_predirent_proc,
			   update_dirent_proc, update_dirleave_proc, NULL,
			   argc, argv, local, which, aflag, 1,
			   preload_update_dir, preload_repository, 1, verify_read, join_rev1);

    if (server_active)
		return err;

    /* see if we need to sleep before returning to avoid time-stamp races */
    if (last_register_time)
		sleep_past (last_register_time);

    return (err);
}

/* This is called to automatically edit a merged file */
static void edit_modified_file(file_info *finfo, Vers_TS *vers)
{
	bool exclusive_edit;

	if(is_rcs || !edit_modified)
		return;

	TRACE(3,"Automatic edit of %s revision %s",PATCH_NULL(fn_root(finfo->file)),PATCH_NULL(vers->vn_rcs));

	kflag kftmp;
	RCS_get_kflags(vers->options,false,kftmp);
	if(kftmp.flags&KFLAG_EXCLUSIVE_EDIT)
		exclusive_edit = true;
	else
		exclusive_edit = false;

	if(notify_do('E',finfo->file,CVS_Username,global_session_time,remote_host_name,finfo->update_dir,"",finfo->repository,vers->tag,exclusive_edit?"X":"",edit_modified_bugid,NULL))
	{
		error(0,0,"Unable to perform automatic edit of file '%s'",fn_root(finfo->file));
	}
	else
	{
		xfree(vers->edit_bugid);
		xfree(vers->edit_revision);
		xfree(vers->edit_tag);
		vers->edit_bugid = xstrdup(edit_modified_bugid?edit_modified_bugid:"");
		vers->edit_revision = xstrdup(vers->vn_rcs?vers->vn_rcs:vers->vn_user?vers->vn_user:"");
		vers->edit_tag = xstrdup(vers->tag?vers->tag:"");
		file_is_edited = 1;
	}
}


/*
 * This is the callback proc for update.  It is called for each file in each
 * directory by the recursion code.  The current directory is the local
 * instantiation.  file is the file name we are to operate on. update_dir is
 * set to the path relative to where we started (for pretty printing).
 * repository is the repository. entries and srcfiles are the pre-parsed
 * entries and source control files.
 * 
 * This routine decides what needs to be done for each file and does the
 * appropriate magic for checkout
 */
static int update_fileproc (void *callerdat, struct file_info *finfo)
{
    int retval;
    Ctype status;
    Vers_TS *vers;

	file_is_edited = 0;

    status = Classify_File (finfo, tag, date, options, force_tag_match,
			    aflag, &vers, pipeout, 0, 0);

    /* Keep track of whether TAG is a branch tag.
       Note that if it is a branch tag in some files and a nonbranch tag
       in others, treat it as a nonbranch tag.  It is possible that case
       should elicit a warning or an error.  */
    if (rewrite_tag
	&& tag != NULL
	&& finfo->rcs != NULL)
    {
	char *rev = RCS_getversion (finfo->rcs, tag, NULL, 1, NULL);
	if (rev != NULL
	    && !RCS_nodeisbranch (finfo->rcs, tag))
	    nonbranch = 1;
	if (rev != NULL)
	    xfree (rev);
    }

    if (pipeout)
    {
	/*
	 * We just return success without doing anything if any of the really
	 * funky cases occur
	 * 
	 * If there is still a valid RCS file, do a regular checkout type
	 * operation
	 */
	switch (status)
	{
	    case T_UNKNOWN:		/* unknown file was explicitly asked
					 * about */
	    case T_REMOVE_ENTRY:	/* needs to be un-registered */
	    case T_ADDED:		/* added but not committed */
		retval = 0;
		break;
	    case T_CONFLICT:		/* old punt-type errors */
		retval = 1;
		break;
	    case T_UPTODATE:		/* file was already up-to-date */
	    case T_NEEDS_MERGE:		/* needs merging */
	    case T_MODIFIED:		/* locally modified */
	    case T_REMOVED:		/* removed but not committed */
	    case T_CHECKOUT:		/* needs checkout */
	    case T_PATCH:		/* needs patch */
		retval = checkout_file (finfo, vers, 0, 0, 0, 0, is_rcs, NULL, NULL);
		break;

	    default:			/* can't ever happen :-) */
		error (0, 0,
		       "unknown file status %d for file %s", status, finfo->file);
		retval = 0;
		break;
	}
    }
    else
    {
	switch (status)
	{
	    case T_UNKNOWN:		/* unknown file was explicitly asked about */
	    case T_UPTODATE:		/* file was already up-to-date */
			retval = 0;
			break;
		case T_CONFLICT:		/* old punt-type errors */
			retval = 1;
			write_letter (finfo, 'C');
			edit_modified_file(finfo,vers);
			baserev_update(finfo, vers, status);
			break;
		case T_NEEDS_MERGE:		/* needs merging */
			if (! toss_local_changes)
			{
				edit_modified_file(finfo,vers);
				retval = merge_file (finfo, vers);
				baserev_update(finfo, vers, status);
				break;
			}
		/* else FALL THROUGH */
	    case T_MODIFIED:		/* locally modified */
			retval = 0;
            if (toss_local_changes)
            {
                char *bakname;
                bakname = backup_file (finfo->file, vers->vn_user);
                /* This behavior is sufficiently unexpected to
                    justify overinformativeness, I think. */
                if ((! really_quiet) && (! server_active))
                    (void) printf ("(Locally modified %s moved to %s)\n",
                                    finfo->file, bakname);
                xfree (bakname);

                /* The locally modified file is still present, but
                    it will be overwritten by the repository copy
                    after this. */
                status = T_CHECKOUT;
                retval = checkout_file (finfo, vers, 0, 0, 1, 0, 0, NULL, NULL);
            }
            else 
            {
                if (vers->ts_conflict)
                {
                    char *filestamp;
                    int retcode;

                    /*
                        * If the timestamp has changed and no
                        * conflict indicators are found, it isn't a
                        * 'C' any more.
                        */

                    if (server_active)
                        retcode = vers->ts_conflict[0] != '=';
                    else 
                    {
                        filestamp = time_stamp (finfo->file, 0);
                        retcode = strcmp (vers->ts_conflict, filestamp);
                        xfree (filestamp);
                    }

                    if (retcode)
                    {
                        /* The timestamps differ.  But if there
                            are conflict markers print 'C' anyway.  */
                        retcode = !file_has_markers (finfo);
                    }

                    if (!retcode)
                    {
                        write_letter (finfo, 'C');
                        retval = 1;
                    }
                    else
                    {
                        /* Reregister to clear conflict flag. */
                        Register (finfo->entries, finfo->file, 
                                    vers->vn_rcs, vers->ts_rcs,
                                    vers->options, vers->tag,
									vers->date, (char *)0, NULL, NULL, vers->tt_rcs, update_baserev?vers->vn_rcs:vers->edit_revision, update_baserev?vers->tag:vers->edit_tag, vers->edit_bugid, NULL);
                    }
                }
                if (!retval)
                {
                    write_letter (finfo, 'M');
                    retval = 0;
                }
            }
			break;
	    case T_PATCH:		/* needs patch */
#ifdef SERVER_SUPPORT
		if (patches)
		{
		    int docheckout;
		    mode_t mode;
			CMD5Calc md5;

			edit_modified_file(finfo,vers);

		    retval = patch_file (finfo,
					 vers, &docheckout,
					 mode, &md5);
		    if (! docheckout)
		    {
		        if (server_active && retval == 0)
					server_updated (finfo, vers, (rcs_diff_patches? SERVER_RCS_DIFF : SERVER_PATCHED),
					    mode, &md5,(struct buffer *) NULL);
				baserev_update(finfo, vers, status);
				break;
		    }
		}
#endif
		/* If we're not running as a server, just check the
		   file out.  It's simpler and faster than producing
		   and applying patches.  */
		/* Fall through.  */
	    case T_CHECKOUT:		/* needs checkout */
		edit_modified_file(finfo,vers);
		retval = checkout_file (finfo, vers, 0, 0, 1, is_rcs, is_rcs, NULL, NULL);
		if(!is_rcs)
			baserev_update(finfo, vers, status);
		break;
	    case T_ADDED:		/* added but not committed */
		/* Regegister to take into account new options/tags */
		if(!is_rcs)
		{
			TRACE(3,"checkout_file() call Register if !is_rcs and T_ADDED");
			if(vers->tag && isdigit(vers->tag[0]))
				error(1,0,"Numeric revision '%s' illegal for new file",vers->tag);
			Register(finfo->entries, finfo->file, "0", vers->ts_user,
				  vers->options, vers->tag, NULL, NULL, NULL, NULL, (time_t)-1, vers->edit_revision, vers->edit_tag, vers->edit_bugid, NULL);
#ifdef SERVER_SUPPORT
			if (server_active)
			{
				/* We need to update the entries line on the client side.
				It is possible we will later update it again via
				server_updated or some such, but that is OK.  */
				server_update_entries(finfo->file, finfo->update_dir, finfo->repository, SERVER_UPDATED);
			}
#endif
		}
		write_letter (finfo, 'A');
		retval = 0;
		break;
	    case T_REMOVED:		/* removed but not committed */
		write_letter (finfo, 'R');
		retval = 0;
		break;
	    case T_REMOVE_ENTRY:	/* needs to be un-registered */
		retval = scratch_file (finfo, vers);
		baserev_update(finfo, vers, status);
		break;
	    default:			/* can't ever happen :-) */
		error (0, 0,
		       "unknown file status %d for file %s", status, finfo->file);
		retval = 0;
		break;
	}
    }

    /* only try to join if things have gone well thus far */
    if (retval == 0 && join_rev1)
		join_file (finfo, vers);

    /* if this directory has an ignore list, add this file to it */
    if (ignlist && (status != T_UNKNOWN || vers->ts_user == NULL))
    {
	Node *p;

	p = getnode ();
	p->type = FILES;
	p->key = xstrdup (finfo->file);
	if (addnode (ignlist, p) != 0)
	    freenode (p);
    }

    freevers_ts (&vers);
    return (retval);
}

int rcs_update_fileproc(struct file_info *finfo,
	char *xoptions, char *xtag, char *xdate, int xforce,
    int xbuild, int xaflag, int xprune, int xpipeout,
    int which, char *xjoin_rev1, char *xjoin_rev2)
{
	char *cp;

    /* fill in the statics */
    options = xoptions;
    tag = xtag;
    date = xdate;
    force_tag_match = xforce;
    update_build_dirs = xbuild;
    aflag = xaflag;
    update_prune_dirs = xprune;
    pipeout = xpipeout;
	is_rcs = 1;

    /* setup the join support */
    join_rev1 = xjoin_rev1;
    join_rev2 = xjoin_rev2;
    if (join_rev1 && (cp = (char*)strchr (join_rev1, ':')) != NULL)
    {
		*cp++ = '\0';
		date_rev1 = Make_Date (cp);
    }
    else
		date_rev1 = (char *) NULL;
    if (join_rev2 && (cp = (char*)strchr (join_rev2, ':')) != NULL)
    {
		*cp++ = '\0';
		date_rev2 = Make_Date (cp);
    }
    else
		date_rev2 = (char *) NULL;

	return update_fileproc(NULL, finfo);
}

static void update_ignproc (char *file, char *dir)
{
    struct file_info finfo;

    memset (&finfo, 0, sizeof (finfo));
    finfo.file = file;
    finfo.update_dir = dir;
    if (dir[0] == '\0')
	finfo.fullname = xstrdup (file);
    else
    {
		char *ff = (char*)xmalloc (strlen (file) + strlen (dir) + 10);
		strcpy (ff, dir);
		strcat (ff, "/");
		strcat (ff, file);
		finfo.fullname = ff;
    }

    write_letter (&finfo, '?');
    xfree (finfo.fullname);
}

/* ARGSUSED */
static int update_filesdone_proc (void *callerdat, int err, char *repository, char *update_dir, List *entries)
{

	if(rewrite_tag)
	{
		if(!noexec)
			WriteTag (NULL, tag, date, nonbranch, update_dir, repository, NULL);
		rewrite_tag = 0;
	}

    /* if this directory has an ignore list, process it then free it */
    if (ignlist)
    {
		if(entries) /* If there's no entries, we're in W_FAKE land */
			ignore_files (ignlist, entries, update_dir, update_ignproc);
		dellist (&ignlist);
    }

    /* Clean up CVS admin dirs if we are export */
    if (strcmp (command_name, "export") == 0)
    {
	/* I'm not sure the existence_error is actually possible (except
	   in cases where we really should print a message), but since
	   this code used to ignore all errors, I'll play it safe.  */
	if (unlink_file_dir (CVSADM) < 0 && !existence_error (errno))
	    error (0, errno, "cannot remove %s directory", CVSADM);
    }
    else if (!server_active && !pipeout)
    {
        /* If there is no CVS/Root file, add one */
        if (!isfile (CVSADM_ROOT))
	    Create_Root ((char *) NULL, current_parsed_root->original);
    }

    return (err);
}

/* Called before the directory is opened.  We reset the sticky directory version here. */
static int update_predirent_proc (void *callerdat, char *dir, char *repository, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	const char *olddir_vers;
	List *ent;

	if(tag && isdigit((unsigned char)*tag))
		error(1,0,"Numeric directory tags are not allowed.");

    /* If we set the tag or date for a new subdirectory in
       update_dirent_proc, and we're now done with that subdirectory,
       undo the tag/date setting.  Note that we know that the tag and
       date were both originally NULL in this case.  */
    if (tag_update_dir != NULL /*&& strcmp (update_dir, tag_update_dir) == 0*/)
    {
	    xfree (tag);
	    xfree (date);
		nonbranch = 0;
		xfree (tag_update_dir);
    }

	if(ignore_directory(update_dir))
		return R_SKIP_ALL;
	if(!isdir(dir))
	{
		if(!update_build_dirs || (!server_active && !isdir (repository)))
			return R_SKIP_ALL;

	    /* otherwise, create the dir and appropriate adm files */

	    /* If no tag or date were specified on the command line,
               and we're not using -A, we want the subdirectory to use
               the tag and date, if any, of the current directory.
               That way, update -d will work correctly when working on
               a branch.

	       We use TAG_UPDATE_DIR to undo the tag setting in
	       update_dirleave_proc.  If we did not do this, we would
	       not correctly handle a working directory with multiple
	       tags (and maybe we should prohibit such working
	       directories, but they work now and we shouldn't make
	       them stop working without more thought).  */

		if ((tag == NULL && date == NULL) && ! aflag)
	    {
			ParseTag(&tag,&date,&nonbranch,NULL); /* Was olddir_vers...? */
			if (tag != NULL || date != NULL)
				tag_update_dir = xstrdup (update_dir);
	    }

	    make_directory (dir);
	    Create_Admin (dir, update_dir, virtual_repository, tag, date,
			  /* This is a guess.  We will rewrite it later
			     via WriteTag.  */
			  0,
			  0);
		if (server_active)
			server_template (update_dir, virtual_repository);
	    rewrite_tag = 1;
	    nonbranch = 0;
	    Subdir_Register (entries, (char *) NULL, dir);
		/* Wipe the current directory version as we're updating */
		WriteTag(dir, tag, date, nonbranch, update_dir, repository, "_H_");
	}
	else
	{
		const char *dirtag=NULL,*dirdate=NULL;
		if (!(aflag || tag || date))
			ParseTag_Dir(dir,&dirtag,&dirdate,&nonbranch,&olddir_vers);
		else
		{
			nonbranch = 0;
			rewrite_tag = 1;
			ParseTag_Dir(dir,NULL,NULL,NULL,&olddir_vers);
			dirtag=xstrdup(tag);
			dirdate=xstrdup(date);
		}
		/* Wipe the current directory version as we're updating */
		if(!noexec)
			WriteTag(dir, dirtag, dirdate, nonbranch, update_dir, repository, "_H_");
		if (server_active)
			server_template (update_dir, virtual_repository);

		if(!current_parsed_root->isremote)
		{
			int openres1, openres2;
			/* This should be made more efficient.  FIXME */
			openres1=open_directory(repository,dir,dirtag,dirdate,nonbranch,olddir_vers,0);
			openres2=open_directory(repository,dir,dirtag,dirdate,nonbranch,"_H_",0);

			ent = Entries_Open_Dir(0,dir,NULL);
			upgrade_entries(repository,dir,&ent, &renamed_file_list);
			Entries_Close_Dir(ent,dir);

			TRACE(3,"more efficient - close directory twice in update_predirent_proc?");
			if (openres1==0)
				close_directory();
			if (openres2==0)
				close_directory();
		}

		xfree(olddir_vers);
		xfree(dirtag);
		xfree(dirdate);
	}
	return 0;
}

static int send_client_rename(Node *p, void *callerdat)
{
	error(0,0,"%s has been renamed to %s",p->key,p->data);
#ifdef SERVER_SUPPORT
	if(server_active)
		server_rename_file(p->key,p->data);
#endif
	return 0;
}

/*
 * update_dirent_proc () is called back by the recursion processor before a
 * sub-directory is processed for update.  In this case, update_dirent proc
 * will probably create the directory unless -d isn't specified and this is a
 * new directory.  A return code of 0 indicates the directory should be
 * processed by the recursion code.  A return of non-zero indicates the
 * recursion code should skip this directory.
 */
static Dtype update_dirent_proc (void *callerdat, char *dir, char *repository, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	const char *msg;

    TRACE(1,"debug: update_dirent_proc");
    if (ignore_directory (update_dir))
    {
	/* print the warm fuzzy message */
	if (!quiet)
	  error (0, 0, "Ignoring %s", update_dir);
        return R_SKIP_ALL;
    }

	if(join_rev1 || join_rev2)
	{
		/* before we do anything else, see if we have any
			per-directory tags */
		if (join_rev1 && !verify_read(repository,NULL,join_rev1, &msg, NULL))
		{
			error (0, 0, "User '%s' cannot read %s on tag/branch %s", CVS_Username, fn_root(dir), join_rev1);
			if(msg)
				error (0, 0, "%s", msg);
			return R_SKIP_ALL;
		}
		/* before we do anything else, see if we have any
			per-directory tags */
		if (join_rev2 && ! verify_read(repository,NULL,join_rev2, &msg, NULL))
		{
			error (0, 0, "User '%s' cannot read %s on tag/branch %s", CVS_Username, fn_root(dir), join_rev2);
			if(msg)
				error (0, 0, "%s", msg);
			return R_SKIP_ALL;
		}
	}

    TRACE(1,"debug: udp1");
    if (!isdir (dir))
    {
	/* if we aren't building dirs, blow it off */
	if (!update_build_dirs)
	    return (R_SKIP_ALL);

	/* Various CVS administrators are in the habit of removing
	   the repository directory for things they don't want any
	   more.  I've even been known to do it myself (on rare
	   occasions).  Not the usual recommended practice, but we
	   want to try to come up with some kind of
	   reasonable/documented/sensible behavior.  Generally
	   the behavior is to just skip over that directory (see
	   dirs test in sanity.sh; the case which reaches here
	   is when update -d is specified, and the working directory
	   is gone but the subdirectory is still mentioned in
	   CVS/Entries).  */

		/* In the remote case, the client should refrain from
	       sending us the directory in the first place.  So we
	       want to continue to give an error, so clients make
	       sure to do this.  */

    TRACE(1,"debug: udp2");
	    if(!server_active && !isdir (repository))
		    return R_SKIP_ALL;

    TRACE(1,"debug: udp3");
	if (noexec)
	{
		if(compat[compat_level].old_checkout_n_behaviour)
		{
			/* ignore the missing dir if -n is specified */
			error (0, 0, "New directory `%s' -- ignored", update_dir);
		    return (R_SKIP_ALL);
		}
	}
	else
	{
		/* This probably won't ever be called any more... just here for boilerplate.  See predirentproc, above. */

	    /* otherwise, create the dir and appropriate adm files */

	    /* If no tag or date were specified on the command line,
               and we're not using -A, we want the subdirectory to use
               the tag and date, if any, of the current directory.
               That way, update -d will work correctly when working on
               a branch.

	       We use TAG_UPDATE_DIR to undo the tag setting in
	       update_dirleave_proc.  If we did not do this, we would
	       not correctly handle a working directory with multiple
	       tags (and maybe we should prohibit such working
	       directories, but they work now and we shouldn't make
	       them stop working without more thought).  */
	    if ((tag == NULL && date == NULL) && ! aflag)
	    {
			ParseTag(&tag,&date,&nonbranch,NULL); /* Was olddir_vers...? */
    TRACE(1,"debug: udp4");
			if (tag != NULL || date != NULL)
				tag_update_dir = xstrdup (update_dir);
	    }

    TRACE(1,"debug: udp4");
	    make_directory (dir);
    TRACE(1,"debug: udp5");
	    Create_Admin (dir, update_dir, virtual_repository, tag, date,
			  /* This is a guess.  We will rewrite it later
			     via WriteTag.  */
			  0,
			  0);
    TRACE(1,"debug: udp6");
		if (server_active)
			server_template (update_dir, virtual_repository);
    TRACE(1,"debug: udp7");
	    rewrite_tag = 1;
	    nonbranch = 0;
	    Subdir_Register (entries, (char *) NULL, dir);
	}
    }
    /* Do we need to check noexec here? */
    else if (!pipeout && !noexec)
    {
		char *cvsadmdir;

		/* The directory exists.  Check to see if it has a CVS
		subdirectory.  */

		cvsadmdir = (char*)xmalloc (strlen (dir) + 80);
		strcpy (cvsadmdir, dir);
		strcat (cvsadmdir, "/");
		strcat (cvsadmdir, CVSADM);

		if (!isdir (cvsadmdir))
		{
			/* We cannot successfully recurse into a directory without a CVS
			subdirectory.  Generally we will have already printed
			"? foo".  */
			xfree (cvsadmdir);
			return R_SKIP_ALL;
		}
		xfree (cvsadmdir);
    }

    /*
     * If we are building dirs and not going to stdout, we make sure there is
     * no static entries file and write the tag file as appropriate
     */
    if (!pipeout )
    {
		if (!noexec && update_build_dirs)
		{
			char *tmp;

			tmp = (char*)xmalloc (strlen (dir) + sizeof (CVSADM_ENTSTAT) + 10);
			(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENTSTAT);
			if (unlink_file (tmp) < 0 && ! existence_error (errno))
			error (1, errno, "cannot remove file %s", tmp);
#ifdef SERVER_SUPPORT
			if (server_active)
			server_clear_entstat (update_dir, repository);
#endif
			xfree (tmp);
		}

		/* print the warm fuzzy message */
		if (!quiet)
		{
#ifdef SERVER_SUPPORT
			if(server_dir)
				error (0, 0, "Updating %s/%s", client_where(server_dir),client_where(update_dir));
			else
#endif
				error (0, 0, "Updating %s", client_where(update_dir));
		}

		const char *dirtag = NULL,*dirdate = NULL;
		/* keep the CVS/Tag file current with the specified arguments */
		if (!(aflag || tag || date))
		{
			ParseTag_Dir(dir,&dirtag,&dirdate,&nonbranch,NULL);
		}
		else
		{
			nonbranch = 0;
			rewrite_tag = 1;
			dirtag=xstrdup(tag);
			dirdate=xstrdup(date);
		}

		if (join_rev1 && !verify_merge(repository,NULL,dirtag,join_rev1,&msg))
		{
			if(!tag) tag="HEAD";
			error (0, 0, "User '%s' cannot merge branch %s with branch %s", CVS_Username, tag, join_rev1);
			if(msg)
				error (0, 0, "%s", msg);
			return R_SKIP_ALL;
		}

		if (join_rev2 && !verify_merge(repository,NULL,dirtag,join_rev2,&msg))
		{
			if(!tag) tag="HEAD";
			error (0, 0, "User '%s' cannot merge branch %s with branch %s", CVS_Username, tag, join_rev2);
			if(msg)
				error (0, 0, "%s", msg);
			return R_SKIP_ALL;
		}

		/* Put the new directory version in place */
		if(!noexec)
			WriteTag (dir, dirtag, dirdate, nonbranch, update_dir, repository, NULL);

		xfree(dirtag);
		xfree(dirdate);

		/* initialize the ignore list for this directory */
		ignlist = getlist ();
    }

	if(renamed_file_list)
	{
		walklist(renamed_file_list,send_client_rename, NULL);
		dellist(&renamed_file_list);
	}

    return (R_PROCESS);
}

/*
 * update_dirleave_proc () is called back by the recursion code upon leaving
 * a directory.  It will prune empty directories if needed and will execute
 * any appropriate update programs.
 */
/* ARGSUSED */
static int update_dirleave_proc (void *callerdat, char *dir, int err, char *update_dir, List *entries)
{
    /* Delete the ignore list if it hasn't already been done.  */
    if (ignlist)
		dellist (&ignlist);

    /* If we set the tag or date for a new subdirectory in
       update_dirent_proc, and we're now done with that subdirectory,
       undo the tag/date setting.  Note that we know that the tag and
       date were both originally NULL in this case.  */
    if (tag_update_dir != NULL && strcmp (update_dir, tag_update_dir) == 0)
    {
	    xfree (tag);
	    xfree (date);
		nonbranch = 0;
		xfree (tag_update_dir);
    }

    if (strchr (dir, '/') == NULL)
    {
	/* FIXME: chdir ("..") loses with symlinks.  */
	/* Prune empty dirs on the way out - if necessary */
	(void) CVS_CHDIR ("..");
	if (update_prune_dirs && isemptydir (dir, noexec))
	{
	    /* I'm not sure the existence_error is actually possible (except
	       in cases where we really should print a message), but since
	       this code used to ignore all errors, I'll play it safe.	*/
	    if (unlink_file_dir (dir) < 0 && !existence_error (errno))
		error (0, errno, "cannot remove %s directory", dir);
	    Subdir_Deregister (entries, (char *) NULL, dir);	
	}
    }

    return (err);
}

/* Returns 1 if the file indicated by node has been removed.  */
static int isremoved (Node *node, void *closure)
{
    Entnode *entdata = (Entnode*) node->data;

    /* If the first character of the version is a '-', the file has been
       removed. */
    return (entdata->version && entdata->version[0] == '-') ? 1 : 0;
}

/* Returns 1 if the argument directory is completely empty, other than the
   existence of the CVS directory entry.  Zero otherwise.  If MIGHT_NOT_EXIST
   and the directory doesn't exist, then just return 0.  */
int isemptydir (const char *dir, int might_not_exist)
{
    DIR *dirp;
    struct dirent *dp;

    if ((dirp = opendir (dir)) == NULL)
    {
	if (might_not_exist && existence_error (errno))
	    return 0;
	error (0, errno, "cannot open directory %s for empty check", dir);
	return (0);
    }
    errno = 0;
    while ((dp = readdir (dirp)) != NULL)
    {
	if (strcmp (dp->d_name, ".") != 0
	    && strcmp (dp->d_name, "..") != 0)
	{
	    if (strcmp (dp->d_name, CVSADM) != 0)
	    {
		/* An entry other than the CVS directory.  The directory
		   is certainly not empty. */
		(void) closedir (dirp);
		return (0);
	    }
	    else
	    {
		/* The CVS directory entry.  We don't have to worry about
		   this unless the Entries file indicates that files have
		   been removed, but not committed, in this directory.
		   (Removing the directory would prevent people from
		   comitting the fact that they removed the files!) */
		List *l;
		int files_removed;
		struct saved_cwd cwd;

		if (save_cwd (&cwd))
		    error_exit ();

		if (CVS_CHDIR (dir) < 0)
		    error (1, errno, "cannot change directory to %s", fn_root(dir));
		l = Entries_Open (0, NULL);
		files_removed = walklist (l, isremoved, 0);
		Entries_Close (l);

		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		free_cwd (&cwd);

		if (files_removed != 0)
		{
		    /* There are files that have been removed, but not
		       committed!  Do not consider the directory empty. */
		    (void) closedir (dirp);
		    return (0);
		}
	    }
	}
	errno = 0;
    }
    if (errno != 0)
    {
	error (0, errno, "cannot read directory %s", dir);
	(void) closedir (dirp);
	return (0);
    }
    (void) closedir (dirp);
    return (1);
}

/*
 * scratch the Entries file entry associated with a file
 */
static int scratch_file (struct file_info *finfo, Vers_TS *vers)
{
    history_write ('W', finfo->update_dir, "", finfo->file, finfo->repository,NULL,NULL);
    Scratch_Entry (finfo->entries, finfo->file);
#ifdef SERVER_SUPPORT
    if (server_active)
    {
	if (vers->ts_user == NULL)
	    server_scratch_entry_only ();
	server_updated (finfo, vers,
		SERVER_UPDATED, (mode_t) -1,
		NULL,
		(struct buffer *) NULL);
    }
#endif
    if (unlink_file (finfo->file) < 0 && ! existence_error (errno))
	error (0, errno, "unable to remove %s", fn_root(finfo->fullname));
    else
	/* skip this step when the server is running since
	 * server_updated should have handled it */
	if (!server_active)
    {
	/* keep the vers structure up to date in case we do a join
	 * - if there isn't a file, it can't very well have a version number, can it?
	 */
	if (vers->vn_user != NULL)
	{
	    xfree (vers->vn_user);
	    vers->vn_user = NULL;
	}
	if (vers->ts_user != NULL)
	{
	    xfree (vers->ts_user);
	    vers->ts_user = NULL;
	}
    }
    return (0);
}

/*
 * Check out a file.
 */
int checkout_file (struct file_info *finfo, Vers_TS *vers_ts, int adding,
    int merging, int update_server, int resurrect, int is_rcs, const char *merge_rev1, const char *merge_rev2)
{
    char *backup;
	char *vn_rcs;
    int set_time, retval = 0;
    int status;
    int file_is_dead;
    struct buffer *newrevbuf;
	mode_t mode=0666;
	const char *xfile = finfo->file;

    backup = NULL;
    newrevbuf = NULL;

	if(vers_ts && vers_ts->filename)
		xfile = vers_ts->filename;

    /* Don't screw with backup files if we're going to stdout, or if
       we are the server.  */
    if (!is_rcs && !pipeout && ! server_active	)
    {
	backup = (char*)xmalloc (strlen (xfile)
			  + sizeof (CVSADM)
			  + sizeof (CVSPREFIX)
			  + 10);
	(void) sprintf (backup, "%s/%s%s", CVSADM, CVSPREFIX, xfile);
	if (isfile (xfile))
	    rename_file (xfile, backup);
	else
	{
	    /* If -f/-t wrappers are being used to wrap up a directory,
	       then backup might be a directory instead of just a file.  */
	    if (unlink_file_dir (backup) < 0)
	    {
		/* Not sure if the existence_error check is needed here.  */
		if (!existence_error (errno))
		    /* FIXME: should include update_dir in message.  */
		    error (0, errno, "error removing %s", backup);
	    }
	    xfree (backup);
	    backup = NULL;
	}
    }

	file_is_dead = RCS_isdead (vers_ts->srcfile, vers_ts->vn_rcs);
	if(file_is_dead && resurrect)
	{
		Node *p = findnode(finfo->rcs->versions, vers_ts->vn_rcs);
		RCSVers *vers = (RCSVers *) p->data;

		file_is_dead = 0;
		adding = 1;
		vn_rcs = vers->version;
	}
	else
		vn_rcs = vers_ts->vn_rcs;


    if (!file_is_dead)
    {
	/*
	 * if we are checking out to stdout, print a nice message to
	 * stderr, and add the -p flag to the command */
	if (pipeout && !is_rcs)
	{
	    if (!quiet)
	    {
		cvs_outerr ("\
===================================================================\n\
Checking out ", 0);
		cvs_outerr (fn_root(finfo->fullname), 0);
		cvs_outerr ("\n\
RCS:  ", 0);
		cvs_outerr (fn_root(vers_ts->srcfile->path), 0);
		cvs_outerr ("\n\
VERS: ", 0);
		cvs_outerr (vn_rcs, 0);
		cvs_outerr ("\n***************\n", 0);
	    }
	}

#ifdef SERVER_SUPPORT
	if (update_server
	    && server_active
	    && ! pipeout
	    && ! joining ())
	{
	    newrevbuf = buf_nonio_initialize ((BUFMEMERRPROC) NULL);
		TRACE(3,"checkout_file: revbuf%sNULL",(newrevbuf==NULL)?"==":"!=");
		if (newrevbuf!=NULL) TRACE(3,"checkout_file: revbuf->data%sNULL",(newrevbuf->data==NULL)?"==":"!=");
	    status = RCS_checkout (vers_ts->srcfile, (char *) NULL,
				   vn_rcs, vers_ts->vn_tag,
				   vers_ts->options, RUN_TTY,
				   checkout_to_buffer, newrevbuf, &mode);
		TRACE(3,"checkout_file: after RCS_checkout revbuf%sNULL",(newrevbuf==NULL)?"==":"!=");
		if (newrevbuf!=NULL) TRACE(3,"checkout_file: after RCS_checkout revbuf->data%sNULL",(newrevbuf->data==NULL)?"==":"!=");
	}
	else
#endif
	{
		if(is_rcs)
		{
			char *file = finfo->fullname?xstrdup(finfo->fullname):NULL;
			if(file && strlen(file)>(sizeof(RCSEXT)-1))
				file[strlen(file)-(sizeof(RCSEXT)-1)]='\0';

			fprintf(stderr,"%s  -->  %s\n",finfo->fullname,pipeout?"standard output":file);
			fprintf(stderr,"revision %s\n",vers_ts->vn_rcs);
		    status = RCS_checkout (vers_ts->srcfile,
				   pipeout ? NULL : file,
				   vn_rcs, vers_ts->vn_tag,
				   vers_ts->options, RUN_TTY,
				   (RCSCHECKOUTPROC) NULL, (void *) NULL, &mode);
			if(!pipeout)
				fprintf(stderr,"done\n");
			xfree(file);
		}
		else
		{
		    status = RCS_checkout (vers_ts->srcfile,
				   pipeout ? NULL : xfile,
				   vn_rcs, vers_ts->vn_tag,
				   vers_ts->options, RUN_TTY,
				   (RCSCHECKOUTPROC) NULL, (void *) NULL, &mode);
		}
	}
    }
    if (file_is_dead || status == 0)
    {
	if (!pipeout)
	{
	    Vers_TS *xvers_ts;
		kflag kftmp;

		RCS_get_kflags(vers_ts->options,false,kftmp);
		CXmlNodePtr node = fileattr_getroot();
		node->xpathVariable("name",xfile);

		if (!is_rcs
		&& cvswrite
		&& !file_is_dead
		&& (!node->Lookup("file[cvs:filename(@name,$name)]/watched") || !node->XPathResultNext())
		&& !(kftmp.flags&KFLAG_RESERVED_EDIT))
		{
			if (newrevbuf == NULL)
				xchmod (xfile, 1);

			mode = modify_mode(mode, 
					  ((mode & S_IRUSR) ? S_IWUSR : 0)
					| ((mode & S_IRGRP) ? S_IWGRP : 0)
					| ((mode & S_IROTH) ? S_IWOTH : 0),0);
		}

		/* set the time from the RCS file iff it was unknown before */
		set_time = force_checkout_time ||
		(!noexec
		&& (vers_ts->vn_user == NULL ||
			strncmp (vers_ts->ts_rcs, "Initial", 7) == 0)
		&& !file_is_dead);

		xvers_ts = Version_TS (finfo, options, tag, date, 
				force_tag_match, set_time, 0);

		if (newrevbuf != NULL)
		{
			/* If we stored the file data into a buffer, then we
					didn't create a file at all, so xvers_ts->ts_user
					is wrong.  The correct value is to have it be the
					same as xvers_ts->ts_rcs, meaning that the working
					file is unchanged from the RCS file.

			FIXME: We should tell Version_TS not to waste time
			statting the nonexistent file.

			FIXME: Actually, I don't think the ts_user value
			matters at all here.  The only use I know of is
			that it is printed in a trace message by
			Server_Register.  */

			if (xvers_ts->ts_user != NULL)
				xfree (xvers_ts->ts_user);
			xvers_ts->ts_user = xstrdup (xvers_ts->ts_rcs);
		}

		(void) time (&last_register_time);

		if (file_is_dead)
		{
			if (xvers_ts->vn_user != NULL)
			{
				error (0, 0,
				"warning: %s is not (any longer) pertinent",
 				fn_root(finfo->fullname));
			}
			Scratch_Entry (finfo->entries, xfile);
#ifdef SERVER_SUPPORT
			if (server_active && xvers_ts->ts_user == NULL)
				server_scratch_entry_only ();
#endif
			/* FIXME: Rather than always unlink'ing, and ignoring the
			existence_error, we should do the unlink only if
			vers_ts->ts_user is non-NULL.  Then there would be no
			need to ignore an existence_error (for example, if the
			user removes the file while we are running).  */
			if (unlink_file (xfile) < 0 && ! existence_error (errno))
			{
				error (0, errno, "cannot remove %s", fn_root(finfo->fullname));
			}
		}
		else
		{
			TRACE(3,"checkout_file() call Register if !is_rcs");
			if(!is_rcs)
			  Register(finfo->entries, xfile,
				adding ? "0" : xvers_ts->vn_rcs,
				xvers_ts->ts_user, xvers_ts->options,
				xvers_ts->tag, xvers_ts->date,
				(char *)0, merge_rev1, merge_rev2, xvers_ts->tt_rcs, update_baserev?xvers_ts->vn_rcs:vers_ts->edit_revision, update_baserev?vers_ts->tag:vers_ts->edit_tag, vers_ts->edit_bugid, NULL); /* Clear conflict flag on fresh checkout */
			TRACE(3,"checkout_file() call to Register complete");
		}

		TRACE(3,"checkout_file() vers_ts%sNULL, xvers_ts%sNULL, finfo%sNULL, command_name%sNULL",(vers_ts==NULL)?"==":"!=",(xvers_ts==NULL)?"==":"!=",(finfo==NULL)?"==":"!=",(command_name==NULL)?"==":"!=");
		/* fix up the vers structure, in case it is used by join */
		if (join_rev1)
		{
			TRACE(3,"fix up the vers structure, in case it is used by join.");
			if (vers_ts->vn_user != NULL)
				xfree (vers_ts->vn_user);
			if (vn_rcs != NULL)
				xfree (vn_rcs);
			vers_ts->vn_user = xstrdup (xvers_ts->vn_rcs);
			vers_ts->vn_rcs = xstrdup (xvers_ts->vn_rcs);
		}

		/* If this is really Update and not Checkout, recode history */
		if (strcmp (command_name, "update") == 0)
		{
			TRACE(3,"If this is really Update and not Checkout, recode history.");
			history_write ('U', finfo->update_dir, xvers_ts->vn_rcs, xfile,
				finfo->repository, xvers_ts->edit_bugid, NULL);
		}
		TRACE(3,"free xvers_ts");
		freevers_ts (&xvers_ts);
		TRACE(3,"free xvers_ts OK");

		if (!really_quiet && !file_is_dead && !is_rcs)
		{
			TRACE(3,"!really_quiet && !file_is_dead && !is_rcs, so write the letter 'U'");
			write_letter (finfo, 'U');
		}
	}

#ifdef SERVER_SUPPORT
	if (update_server && server_active)
	{
		TRACE(3,"checkout_file: update_server && server_active, so calculate the md5");
		CMD5Calc *md5 = NULL;
		TRACE(3,"checkout_file: is revbuf?");
		if (newrevbuf!=NULL)
		{
		size_t newrevbuf_length;

		TRACE(3,"checkout_file: we have a revbuf therefore find the buffer length (buf->data%sNULL).",(newrevbuf->data==NULL)?"==":"!=");
		newrevbuf_length=buf_length(newrevbuf);

		TRACE(3,"checkout_file: revbuf%sNULL len=%d, threshhold=%d",(newrevbuf==NULL)?"==":"!=",(int)newrevbuf_length,(int)server_checksum_threshold);
		if(newrevbuf_length>=server_checksum_threshold)
		{
			TRACE(3,"checkout_file: create the md5");
			md5 = new CMD5Calc;
			buf_md5(newrevbuf,md5);
		}
		}
		else
			TRACE(3,"checkout_file: revbuf is NULL (therefore no md5)");

		TRACE(3,"checkout_file: server_updated (with md5)");
		server_updated (finfo, vers_ts,
			merging ? SERVER_MERGED : SERVER_UPDATED,
			mode, md5, newrevbuf);

		delete md5;
	}
#endif
	}
	else
	{
		TRACE(3,"checkout_file: !(update_server && server_active), so no need to calculate the md5");
		if (backup != NULL)
		{
			TRACE(3,"checkout_file: there was a backup, so free it");
			rename_file (backup, xfile);
			xfree (backup);
			backup = NULL;
		}

		TRACE(3,"checkout_file: could not check out...");
		error (0, 0, "could not check out %s", fn_root(finfo->fullname));

		retval = status;
	}

	if (backup != NULL)
	{
		TRACE(3,"checkout_file: there was a backup...");
		/* If -f/-t wrappers are being used to wrap up a directory,
		then backup might be a directory instead of just a file.  */
		if (unlink_file_dir (backup) < 0)
		{
			TRACE(3,"checkout_file: If -f/-t wrappers are being used to wrap up a directory,");
			TRACE(3,"               then backup might be a directory instead of just a file.  ");
			/* Not sure if the existence_error check is needed here.  */
			if (!existence_error (errno))
			TRACE(3,"checkout_file: Not sure if the existence_error check is needed here.  ");
			/* FIXME: should include update_dir in message.  */
			error (0, errno, "error removing %s", backup);
		}
		TRACE(3,"checkout_file: free the backup.");
		xfree (backup);
	}

	TRACE(3,"checkout_file: compelted.");
	return (retval);
}

#ifdef SERVER_SUPPORT

/* This function is used to write data from a file being checked out
   into a buffer.  */

static void checkout_to_buffer (void *callerdat, const char *data, size_t len)
{
    struct buffer *buf = (struct buffer *) callerdat;

    buf_output (buf, data, len);
}

#endif /* SERVER_SUPPORT */

#ifdef SERVER_SUPPORT

/* This structure is used to pass information between patch_file and
   patch_file_write.  */

struct patch_file_data
{
    /* File name, for error messages.  */
    const char *filename;
    /* File to which to write.  */
    FILE *fp;
    /* Whether to compute the MD5 checksum.  */
    int compute_checksum;
    /* Data structure for computing the MD5 checksum.  */
    CMD5Calc *md5;
    /* Set if the file has a final newline.  */
    int final_nl;
};

/* Patch a file.  Runs diff.  This is only done when running as the
 * server.  The hope is that the diff will be smaller than the file
 * itself.
 */
static int patch_file (struct file_info *finfo, Vers_TS *vers_ts, int *docheckout, mode_t &xmode, CMD5Calc* md5)
{
    char *backup;
    char *file1;
    char *file2;
    int retval = 0;
    int retcode = 0;
    int fail;
    FILE *e;
    struct patch_file_data data;
	kflag kf;
    mode_t mode;

    *docheckout = 0;

    if (noexec || pipeout || joining ())
    {
	*docheckout = 1;
	return 0;
    }

    /* If this file has been marked as being binary, then never send a
       patch.  */
	RCS_get_kflags(vers_ts->options, false, kf);
    if (kf.flags&KFLAG_BINARY)
    {
		*docheckout = 1;
		return 0;
    }

    /* First check that the first revision exists.  If it has been nuked
       by cvs admin -o, then just fall back to checking out entire
       revisions.  In some sense maybe we don't have to do this; after
       all cvs.texinfo says "Make sure that no-one has checked out a
       copy of the revision you outdate" but then again, that advice
       doesn't really make complete sense, because "cvs admin" operates
       on a working directory and so _someone_ will almost always have
       _some_ revision checked out.  */
    {
	char *rev;

	rev = RCS_gettag (finfo->rcs, vers_ts->vn_user, 1, NULL);
	if (rev == NULL)
	{
	    *docheckout = 1;
	    return 0;
	}
	else
	    xfree (rev);
    }

    /* If the revision is dead, let checkout_file handle it rather
       than duplicating the processing here.  */
    if (RCS_isdead (vers_ts->srcfile, vers_ts->vn_rcs))
    {
	*docheckout = 1;
	return 0;
    }

    backup = (char*)xmalloc (strlen (finfo->file)
		      + sizeof (CVSADM)
		      + sizeof (CVSPREFIX)
		      + 10);
    (void) sprintf (backup, "%s/%s%s", CVSADM, CVSPREFIX, finfo->file);
    if (isfile (finfo->file))
        rename_file (finfo->file, backup);
    else
    {
	if (unlink_file (backup) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", backup);
    }

    file1 = (char*)xmalloc (strlen (finfo->file)
		     + sizeof (CVSADM)
		     + sizeof (CVSPREFIX)
		     + 10);
    (void) sprintf (file1, "%s/%s%s-1", CVSADM, CVSPREFIX, finfo->file);
    file2 = (char*)xmalloc (strlen (finfo->file)
		     + sizeof (CVSADM)
		     + sizeof (CVSPREFIX)
		     + 10);
    (void) sprintf (file2, "%s/%s%s-2", CVSADM, CVSPREFIX, finfo->file);

    fail = 0;

    /* We need to check out both revisions first, to see if either one
       has a trailing newline.  Because of this, we don't use rcsdiff,
       but just use diff.  */

    e = CVS_FOPEN (file1, "wb");
    if (e == NULL)
	error (1, errno, "cannot open %s", file1);

    data.filename = file1;
    data.fp = e;
    data.final_nl = 0;
    data.compute_checksum = 0;
	data.md5 = md5;

    retcode = RCS_checkout (vers_ts->srcfile, (char *) NULL,
			    vers_ts->vn_user, (char *) NULL,
			    vers_ts->options, RUN_TTY,
			    patch_file_write, (void *) &data, &mode);

    if (fclose (e) < 0)
	error (1, errno, "cannot close %s", file1);

    if (retcode != 0 || ! data.final_nl)
	fail = 1;

    if (! fail)
    {
	e = CVS_FOPEN (file2, "wb");
	if (e == NULL)
	    error (1, errno, "cannot open %s", file2);

	data.filename = file2;
	data.fp = e;
	data.final_nl = 0;
	data.compute_checksum = 1;
	data.md5 = md5;

	retcode = RCS_checkout (vers_ts->srcfile, (char *) NULL,
				vers_ts->vn_rcs, vers_ts->vn_tag,
				vers_ts->options, RUN_TTY,
				patch_file_write, (void *) &data, &mode);

	if (fclose (e) < 0)
	    error (1, errno, "cannot close %s", file2);

	if (retcode != 0 || ! data.final_nl)
	    fail = 1;
	else
		md5->Final();
    }	  

    CXmlNodePtr node = fileattr_getroot();
    node->xpathVariable("name",finfo->file);

    if (cvswrite
	&& (!node->Lookup("file[cvs:filename(@name,$name)]/watched") || !node->XPathResultNext())
	&& !(kf.flags&KFLAG_RESERVED_EDIT))
	{
		mode = modify_mode(mode, 
				  ((mode & S_IRUSR) ? S_IWUSR : 0)
				| ((mode & S_IRGRP) ? S_IWGRP : 0)
				| ((mode & S_IROTH) ? S_IWOTH : 0),0);
	}

    xmode = mode;
    retcode = 0;
    if (! fail)
    {
	char *diff_options;

	/* If the client does not support the Rcs-diff command, we
           send a context diff, and the client must invoke patch.
           That approach was problematical for various reasons.  The
           new approach only requires running diff in the server; the
           client can handle everything without invoking an external
           program.  */
	if (! rcs_diff_patches)
	{
	    /* We use -c, not -u, because that is what CVS has
	       traditionally used.  Kind of a moot point, now that
	       Rcs-diff is preferred, so there is no point in making
	       the compatibility issues worse.  */
	    diff_options = "-c";
	}
	else
	{
	    /* Now that diff is librarified, we could be passing -a if
	       we wanted to.  However, it is unclear to me whether we
	       would want to.  Does diff -a, in any significant
	       percentage of cases, produce patches which are smaller
	       than the files it is patching?  I guess maybe text
	       files with character sets which diff regards as
	       'binary'.  Conversely, do they tend to be much larger
	       in the bad cases?  This needs some more
	       thought/investigation, I suspect.  */

	    diff_options = "-n";
	}
	retcode = diff_exec (file1, file2, NULL, NULL, diff_options, finfo->file);

	/* A retcode of 0 means no differences.  1 means some differences.  */
	if (retcode != 0
	    && retcode != 1)
	{
	    fail = 1;
	}
	else
	{
	    
#define BINARY "Binary"
	    char buf[sizeof BINARY];
	    unsigned int c;

	   CXmlNodePtr node = fileattr_getroot();
	   node->xpathVariable("name",finfo->file);

   	    if (cvswrite
			&& (!node->Lookup("file[cvs:filename(@name,$name)]/watched") || !node->XPathResultNext())
			&& !(kf.flags&KFLAG_RESERVED_EDIT))
		{
			xchmod (finfo->file, 1);
		}
       	else
			xchmod(finfo->file, 0);

	    /* Check the diff output to make sure patch will be handle it.  */
	    e = CVS_FOPEN (finfo->file, "r");
	    if (e == NULL)
		error (1, errno, "could not open diff output file %s",
		       fn_root(finfo->fullname));
	    c = fread (buf, 1, sizeof BINARY - 1, e);
	    buf[c] = '\0';
	    if (strcmp (buf, BINARY) == 0)
	    {
		/* These are binary files.  We could use diff -a, but
		   patch can't handle that.  */
		fail = 1;
	    }
	    fclose (e);
	}
    }

    if (! fail)
    {
        Vers_TS *xvers_ts;
	 struct stat file_info;

        /* This stuff is just copied blindly from checkout_file.  I
	   don't really know what it does.  */
        xvers_ts = Version_TS (finfo, options, tag, date,
			       force_tag_match, 0, 0);

	Register (finfo->entries, finfo->file, xvers_ts->vn_rcs,
		  xvers_ts->ts_user, xvers_ts->options,
		  xvers_ts->tag, xvers_ts->date, NULL, NULL, NULL, xvers_ts->tt_rcs, update_baserev?xvers_ts->vn_rcs:vers_ts->edit_revision, update_baserev?vers_ts->tag:vers_ts->edit_tag, vers_ts->edit_bugid, NULL);

	if (CVS_STAT (finfo->file, &file_info) < 0)
	    error (1, errno, "could not stat %s", finfo->file);

	/* If this is really Update and not Checkout, recode history */
	if (strcmp (command_name, "update") == 0)
	    history_write ('P', finfo->update_dir, xvers_ts->vn_rcs, finfo->file,
			   finfo->repository, xvers_ts->edit_bugid, NULL);

	freevers_ts (&xvers_ts);

	if (!really_quiet)
	{
	    write_letter (finfo, 'P');
	}
    }
    else
    {
	int old_errno = errno;		/* save errno value over the rename */

	if (isfile (backup))
	    rename_file (backup, finfo->file);

	if (retcode != 0 && retcode != 1)
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? old_errno : 0,
		   "could not diff %s", fn_root(finfo->fullname));

	*docheckout = 1;
	retval = retcode;
    }

    if (unlink_file (backup) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", backup);
    if (unlink_file (file1) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", file1);
    if (unlink_file (file2) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", file2);

    xfree (backup);
    xfree (file1);
    xfree (file2);
    return (retval);
}

/* Write data to a file.  Record whether the last byte written was a
   newline.  Optionally compute a checksum.  This is called by
   patch_file via RCS_checkout.  */

static void patch_file_write (void *callerdat, const char *buffer, size_t len)
{
    struct patch_file_data *data = (struct patch_file_data *) callerdat;

    if (fwrite (buffer, 1, len, data->fp) != len)
	error (1, errno, "cannot write %s", data->filename);

    data->final_nl = (buffer[len - 1] == '\n');

    if (data->compute_checksum)
		data->md5->Update(buffer,len);
}

#endif /* SERVER_SUPPORT */

/*
 * Several of the types we process only print a bit of information consisting
 * of a single letter and the name.
 */
static void write_letter (struct file_info *finfo, int letter)
{
    if (!really_quiet)
    {
	char *tag = NULL;
	/* Big enough for "+updated" or any of its ilk.  */
	char buf[4096];

	switch (letter)
	{
	    case 'U':
		tag = "updated";
		break;
	    default:
		/* We don't yet support tagged output except for "U".  */
		break;
	}

	if (tag != NULL)
	{
	    sprintf (buf, "+%s", tag);
	    cvs_output_tagged (buf, NULL);
	}
	buf[0] = letter;
	buf[1] = ' ';
	buf[2] = '\0';
	cvs_output_tagged ("text", buf);
#ifdef SERVER_SUPPORT
	if(server_dir)
	{
		snprintf(buf,sizeof(buf),"%s/%s",client_where(server_dir),client_where(fn_root(finfo->fullname)));
		cvs_output_tagged ("fname", buf);
	}
	else
#endif
		cvs_output_tagged ("fname", client_where(fn_root(finfo->fullname)));
	cvs_output_tagged ("newline", NULL);
	if (tag != NULL)
	{
	    sprintf (buf, "-%s", tag);
	    cvs_output_tagged (buf, NULL);
	}
    }
    return;
}

/*
 * Do all the magic associated with a file which needs to be merged
 */
static int merge_file (struct file_info *finfo, Vers_TS *vers)
{
    char *backup;
    int status;
    int retcode = 0;
    int retval;
	kflag kf;
	kflag kftmp;
	mode_t mode = 0644; /* stupid default! */
	CXmlNodePtr node;

    /*
     * The users currently modified file is moved to a backup file name
     * ".#filename.version", so that it will stay around for a few days
     * before being automatically removed by some cron daemon.  The "version"
     * is the version of the file that the user was most up-to-date with
     * before the merge.
     */
    backup = (char*)xmalloc (strlen (finfo->file)
		      + strlen (vers->vn_user)
		      + sizeof (BAKPREFIX)
		      + 10);
    (void) sprintf (backup, "%s%s.%s", BAKPREFIX, finfo->file, vers->vn_user);

    copy_file (finfo->file, backup, 1, 1);
    xchmod (finfo->file, 1);

	RCS_get_kflags(vers->options, false, kf);
    if ((kf.flags&KFLAG_BINARY)
		|| wrap_merge_is_copy (finfo->file)
		|| special_file_mismatch (finfo, NULL, vers->vn_rcs))
    {
	/* For binary files, a merge is always a conflict.  Same for
	   files whose permissions or linkage do not match.  We give the
	   user the two files, and let them resolve it.  It is possible
	   that we should require a "touch foo" or similar step before
	   we allow a checkin.  */

	/* TODO: it may not always be necessary to regard a permission
	   mismatch as a conflict.  The working file and the RCS file
	   have a common ancestor `A'; if the working file's permissions
	   match A's, then it's probably safe to overwrite them with the
	   RCS permissions.  Only if the working file, the RCS file, and
	   A all disagree should this be considered a conflict.  But more
	   thought needs to go into this, and in the meantime it is safe
	   to treat any such mismatch as an automatic conflict. -twp */

#ifdef SERVER_SUPPORT
	if (server_active)
	    server_copy_file (finfo->file, finfo->update_dir,
			      finfo->repository, backup);
#endif

	status = checkout_file (finfo, vers, 0, 1, 1, 0, is_rcs, NULL, NULL);

	/* Is there a better term than "nonmergeable file"?  What we
	   really mean is, not something that CVS cannot or does not
	   want to merge (there might be an external manual or
	   automatic merge process).  */
	error (0, 0, "nonmergeable file needs merge");
	error (0, 0, "revision %s from repository is now in %s",
	       vers->vn_rcs, fn_root(finfo->fullname));
	error (0, 0, "file from working directory is now in %s", backup);
	write_letter (finfo, 'C');

	history_write ('C', finfo->update_dir, vers->vn_rcs, finfo->file,
		       finfo->repository, vers->edit_bugid, NULL);
	retval = 0;
	goto out;
    }

    status = RCS_merge(finfo->rcs, vers->srcfile->path, finfo->file,
		       vers->options, vers->vn_user, vers->vn_rcs, conflict_3way, &mode);
	if(status == 3)
	{
		//cvs_output ("No difference between revisions ", 0);
		//cvs_output (vers->vn_user, 0);
		//cvs_output (" and ", 0);
		//cvs_output (vers->vn_rcs, 0);
		//cvs_output (" of ", 0);
		//cvs_output (finfo->file, 0);
		//cvs_output ("\n", 1);

		retval = 0;
		goto out;
	}

	RCS_get_kflags(vers->options,false,kftmp);

	node=fileattr_getroot();
	node->xpathVariable("name",finfo->file);

	if (!is_rcs
		&& cvswrite
		&& (!node->Lookup("file[cvs:filename(@name,$name)]/watched") || !node->XPathResultNext())
		&& !(kftmp.flags&KFLAG_RESERVED_EDIT))
	{
		mode = modify_mode(mode, 
				  ((mode & S_IRUSR) ? S_IWUSR : 0)
				| ((mode & S_IRGRP) ? S_IWGRP : 0)
				| ((mode & S_IROTH) ? S_IWOTH : 0),0);
	}

    if (status != 0 && status != 1)
    {
		error (0, status == -1 ? errno : 0,
			"could not merge revision %s of %s", vers->vn_user, fn_root(finfo->fullname));
		error (status == -1 ? 1 : 0, 0, "restoring %s from backup file %s",
			fn_root(finfo->fullname), backup);
		rename_file (backup, finfo->file);
		retval = 1;
		goto out;
    }

    /* This file is the result of a merge, which means that it has
       been modified.  We use a special timestamp string which will
       not compare equal to any actual timestamp.  */
    {
		char *cp = 0;

		if (status == 1)
		{
			(void) time (&last_register_time);
			cp = time_stamp (finfo->file, 0);
		}
		Register (finfo->entries, finfo->file, vers->vn_rcs,
			"Result of merge", vers->options, vers->tag,
			vers->date, cp, NULL, NULL, vers->tt_rcs, update_baserev?vers->vn_rcs:vers->edit_revision, update_baserev?vers->tag:vers->edit_tag, vers->edit_bugid, NULL);
		if (cp)
			xfree (cp);
    }

    /* fix up the vers structure, in case it is used by join */
    if (join_rev1)
    {
		if (vers->vn_user != NULL)
			xfree (vers->vn_user);
		vers->vn_user = xstrdup (vers->vn_rcs);
    }

#ifdef SERVER_SUPPORT
    /* Send the new contents of the file before the message.  If we
       wanted to be totally correct, we would have the client write
       the message only after the file has safely been written.  */
    if (server_active)
    {
        server_copy_file (finfo->file, finfo->update_dir, finfo->repository,
			  backup);

		server_updated (finfo, vers, SERVER_MERGED, mode, NULL, (struct buffer *) NULL);
    }
#endif

    /* FIXME: the noexec case is broken.  RCS_merge could be doing the
       xcmp on the temporary files without much hassle, I think.  */
    if (!noexec && !xcmp (backup, finfo->file))
    {
	cvs_output (fn_root(finfo->fullname), 0);
	cvs_output (" already contains the differences between ", 0);
	cvs_output (vers->vn_user, 0);
	cvs_output (" and ", 0);
	cvs_output (vers->vn_rcs, 0);
	cvs_output ("\n", 1);

	history_write ('G', finfo->update_dir, vers->vn_rcs, finfo->file,
		       finfo->repository,vers->edit_bugid, NULL);
	retval = 0;
	goto out;
    }

    if (status == 1)
    {
	error (0, 0, "conflicts found in %s", fn_root(finfo->fullname));

	write_letter (finfo, 'C');

	history_write ('C', finfo->update_dir, vers->vn_rcs, finfo->file, finfo->repository, vers->edit_bugid, NULL);

    }
    else if (retcode == -1)
    {
	error (1, errno, "fork failed while examining update of %s",
	       fn_root(finfo->fullname));
    }
    else
    {
	write_letter (finfo, 'M');
	history_write ('G', finfo->update_dir, vers->vn_rcs, finfo->file,
		       finfo->repository, vers->edit_bugid, NULL);
    }
    retval = 0;
 out:
    xfree (backup);
    return retval;
}

static char *getmergepoint(RCSNode *rcs, const char *baserev, const char *headrev, const char *mergefrom)
{
    Node *p,*q;
    Node *basenode = NULL;
    char *mp = NULL;
    char *head;
    char *branch=xstrdup(mergefrom), *branchbase;
    int dotcount;
	RCSVers *v;

    head = RCS_branch_head(rcs, headrev);
    p = findnode(rcs->versions, head);
    xfree(head);

	if(numdots(headrev)>1)
	{
		branchbase = xstrdup(p->key);
		*(strrchr(branchbase,'.')+1)='\0';
		strcat(branchbase,"1");

		basenode = findnode(rcs->versions, branchbase);
		xfree(branchbase);
	}

    dotcount = numdots(branch);
    *(strrchr(branch,'.')+1)='\0';

    /* walk the next pointers until you find the end */
    while (p != NULL && strcmp(p->key,baserev))
    {
        RCSVers *vers = (RCSVers *) p->data;

        if(vers->other_delta)
        {
            q=findnode(vers->other_delta,"mergepoint1");
            if(q && numdots(q->data)==dotcount && !strncmp(branch,q->data,strlen(branch)))
			{
                    mp = q->data;
                    break;
            }
	    }

	    /* if there is a next version, find the node */
		/* On the branch, we have to go backwards, since the revisions are
		   stored in the opposite order */
		if(!basenode) /* If we're on the main branch */
		{
			if (vers->next != NULL)
				p = findnode (rcs->versions, vers->next);
			else
				p =  NULL;
		}
		else
		{
			p = basenode;
			do
			{
				v=(RCSVers*)p->data;
				if(!v->next)
					break;					
				if(!strcmp(v->next,vers->version))
					break;
				p = findnode (rcs->versions, v->next);
			} while(p);
			if(!v->next)
				p=NULL;
		}
    }

    xfree(branch);

    if(!mp)
        return xstrdup(baserev);
    else
        return xstrdup(mp);
}

static char *branchpoint_of(RCSNode *rcs, const char *tag)
{
	char *ver, *p;

	if(!tag || !*tag)
		return NULL;

	/* Techincally this shouldn't happen */
	if(!RCS_isbranch(rcs,tag))
		return NULL;

	ver = RCS_tag2rev(rcs, tag);
	if(!ver)
		return NULL;

	p=strrchr(ver,'.');
	if(!p)
	{
		xfree(ver);
		return NULL;
	}
	*p='\0';
	p=strrchr(ver,'.');
	if(!p)
	{
		xfree(ver);
		return NULL;
	}
	*p='\0';
	return ver;
}

static Node *previous_version(List *versions, RCSVers *vers)
{
	Node *node;

	if(numdots(vers->version)==1) /* Main branch */
	{
		if(vers->next)
			node = findnode(versions,vers->next);
		else
			node=NULL;
	}
	else
	{
		Node *p = versions->list->next;
		do
		{
			RCSVers *v = (RCSVers*)p->data;
			if(v->next && !strcmp(v->next,vers->version))
				break;
			if(v->branches)
			{
				Node *br = findnode(v->branches,vers->version);
				if(br)
					break;
			}
			p=p->next;
		} while(p!=versions->list);
		if(p==versions->list)
			node=NULL;
		else
			node=p;
	}

	return node;
}

/* Find the maximum and minimum revisions between rev1 and rev2 that contain bug bugid */
/* Returns false if the bug was not found between these revisions */
bool bound_merge_by_bugid(RCSNode *rcs, const char *&rev1, const char *&rev2, const char *bugid)
{
	Node *node, *startnode, *endnode;
	bool main_branch;
	
	TRACE(3,"bound_merge_by_bugid start: %s -> %s",rev1,rev2);
	startnode=endnode=NULL;
	node = findnode(rcs->versions, rev2);
	if(numdots(rev2)>1)
		main_branch = false;
	else
		main_branch = true;
	while(node)
	{
		RCSVers *vers = (RCSVers*)node->data;

		/* rev1 isn't normally considered */
		if(!strcmp(node->key,rev1))
			break;

		Node *bug=findnode(vers->other_delta,"bugid");
		if(bug && bug->data && bugid_in(bugid,bug->data))
		{
			if(!startnode)
				startnode = node;
			endnode = node;
		}

		node = previous_version(rcs->versions,vers);
	}
	if(!startnode)
	{
		TRACE(3,"bound_merge_by_bugid: no bug changes found");
		return false;
	}
	xfree(rev1);
	xfree(rev2);
	endnode = previous_version(rcs->versions,(RCSVers*)endnode->data);
	rev1=xstrdup(endnode->key);
	rev2=xstrdup(startnode->key);
	TRACE(3,"bound_merge_by_bugid end: %s -> %s",rev1,rev2);
	return true;
}

/*
 * Do all the magic associated with a file which needs to be joined
 * (-j option)
 */
static void join_file (struct file_info *finfo, Vers_TS *vers)
{
    char *backup;
    char *t_options;
	kflag kf;
    int status;

    const char *rev1;
    const char *rev2;
    const char *jrev1;
    const char *jrev2;
    const char *jdate1;
    const char *jdate2;
    mode_t mode = 0644; /* stupid default! */

	TRACE(1,"join_file(%s, %s%s%s%s, %s, %s)",
		PATCH_NULL(fn_root(finfo->file)),
		vers->tag ? vers->tag : "",
		vers->tag ? " (" : "",
		vers->vn_rcs ? vers->vn_rcs : "",
		vers->tag ? ")" : "",
		join_rev1 ? join_rev1 : "",
		join_rev2 ? join_rev2 : "");

    jrev1 = join_rev1;
    jrev2 = join_rev2;
    jdate1 = date_rev1;
    jdate2 = date_rev2;

    /* Determine if we need to do anything at all.  */
    if (vers->srcfile == NULL || vers->srcfile->path == NULL)
		return;

    /* If only one join revision is specified, it becomes the second
       revision.  */
    if (jrev2 == NULL)
    {
	jrev2 = jrev1;
	jrev1 = NULL;
	jdate2 = jdate1;
	jdate1 = NULL;
    }

    /* Convert the second revision, walking branches and dates.  */
    rev2 = RCS_getversion (vers->srcfile, jrev2, jdate2, 1, (int *) NULL);

    /* If this is a merge of two revisions, get the first revision.
       If only one join tag was specified, then the first revision is
       the greatest common ancestor of the second revision and the
       working file.  */
    if (jrev1 != NULL)
	rev1 = RCS_getversion (vers->srcfile, jrev1, jdate1, 1, (int *) NULL);
    else
    {
		/* Note that we use vn_rcs here, since vn_user may contain a
			special string such as "-nn".  */
		if (vers->vn_rcs == NULL)
			rev1 = NULL;
		else if (rev2 == NULL)
		{
			/* This means that the file never existed on the branch.
				It does not mean that the file was removed on the
				branch: that case is represented by a dead rev2.  If
				the file never existed on the branch, then we have
				nothing to merge, so we just return.  */
			return;
		}
		else
		{
 			char *rev_tmp = gca (vers->vn_rcs, rev2);
 			char *branchpoint = branchpoint_of(vers->srcfile, vers->tag);

			/* Special case if we're at the branchpoint and/or there are no revisions on the branch */
			if(merge_from_branchpoint || (branchpoint && !strcmp(vers->vn_rcs, branchpoint)) || (branchpoint && !strcmp(rev2, branchpoint)))
			{
				rev1 = rev_tmp;
				xfree(branchpoint);
			}
			else
			{
				/* We found the branchpoint, now find the highest mergepoint with this prefix */
				if(inverse_merges)
				{
					char *rev1_tmp;
					rev1_tmp = getmergepoint(vers->srcfile, rev_tmp, vers->vn_rcs, rev2);
					TRACE(3,"getmergepoint(%s, %s, %s)=%s",PATCH_NULL(rev_tmp),PATCH_NULL(vers->vn_rcs),PATCH_NULL(rev2),PATCH_NULL(rev1_tmp));

					rev1 = getmergepoint(vers->srcfile, rev1_tmp, rev2, vers->vn_rcs);
					TRACE(3,"getmergepoint(%s, %s, %s)=%s", PATCH_NULL(rev1_tmp),PATCH_NULL(rev2),PATCH_NULL(vers->vn_rcs),PATCH_NULL(rev1));

					xfree(rev1_tmp);
				}
				else
				{
					rev1 = getmergepoint(vers->srcfile, rev_tmp, vers->vn_rcs, rev2);
					TRACE(3,"getmergepoint(%s, %s, %s)=%s",PATCH_NULL(rev_tmp),PATCH_NULL(vers->vn_rcs),PATCH_NULL(rev2),PATCH_NULL(rev1));
					xfree(rev_tmp);
					xfree(branchpoint);
				}
			}
		}
    }

	/* If we have selected a bug to bound the merge with, then modifify the minimum/maximum revisions */
	if(rev1 && rev2 && merge_bugid)
	{
		if(!bound_merge_by_bugid(vers->srcfile, rev1,rev2,merge_bugid))
		{
		    xfree(rev1);
			xfree(rev2);
			return;
		}
	}

    /* Handle a nonexistent or dead merge target.  */
    if (rev2 == NULL || RCS_isdead (vers->srcfile, rev2))
    {
	char *mrev;

    xfree (rev2);

	/* If the first revision doesn't exist either, then there is
           no change between the two revisions, so we don't do
           anything.  */
	if (jrev1 && (rev1 == NULL || RCS_isdead (vers->srcfile, rev1)))
	{
		xfree (rev1);
	    return;
	}

	/* If we are merging two revisions, then the file was removed
	   between the first revision and the second one.  In this
	   case we want to mark the file for removal.

	   If we are merging one revision, then the file has been
	   removed between the greatest common ancestor and the merge
	   revision.  From the perspective of the branch on to which
	   we ar emerging, which may be the trunk, either 1) the file
	   does not currently exist on the target, or 2) the file has
	   not been modified on the target branch since the greatest
	   common ancestor, or 3) the file has been modified on the
	   target branch since the greatest common ancestor.  In case
	   1 there is nothing to do.  In case 2 we mark the file for
	   removal.  In case 3 we have a conflict.

	   Note that the handling is slightly different depending upon
	   whether one or two join targets were specified.  If two
	   join targets were specified, we don't check whether the
	   file was modified since a given point.  My reasoning is
	   that if you ask for an explicit merge between two tags,
	   then you want to merge in whatever was changed between
	   those two tags.  If a file was removed between the two
	   tags, then you want it to be removed.  However, if you ask
	   for a merge of a branch, then you want to merge in all
	   changes which were made on the branch.  If a file was
	   removed on the branch, that is a change to the file.  If
	   the file was also changed on the main line, then that is
	   also a change.  These two changes--the file removal and the
	   modification--must be merged.  This is a conflict.  */

	/* If the user file is dead, or does not exist, or has been
           marked for removal, then there is nothing to do.  */
	if (vers->vn_user == NULL
	    || vers->vn_user[0] == '-'
	    || RCS_isdead (vers->srcfile, vers->vn_user))
	{
	    if (rev1 != NULL)
		xfree (rev1);
	    return;
	}

	/* If the user file has been marked for addition, or has been
	   locally modified, then we have a conflict which we can not
	   resolve.  No_Difference will already have been called in
	   this case, so comparing the timestamps is sufficient to
	   determine whether the file is locally modified.  */
	if (strcmp (vers->vn_user, "0") == 0
	    || (vers->ts_user != NULL
		&& strcmp (vers->ts_user, vers->ts_rcs) != 0))
	{
	    if (jdate2 != NULL)
		error (0, 0,
		       "file %s is locally modified, but has been removed in revision %s as of %s",
		       fn_root(finfo->fullname), jrev2, jdate2);
	    else
		error (0, 0,
		       "file %s is locally modified, but has been removed in revision %s",
		       fn_root(finfo->fullname), jrev2);

	    /* FIXME: Should we arrange to return a non-zero exit
               status?  */

	    if (rev1 != NULL)
		xfree (rev1);

	    return;
	}

	/* If only one join tag was specified, and the user file has
           been changed since the greatest common ancestor (rev1),
           then there is a conflict we can not resolve.  See above for
           the rationale.  */
	if (join_rev2 == NULL
	    && strcmp (rev1, vers->vn_user) != 0)
	{
	    if (jdate2 != NULL)
		error (0, 0,
		       "file %s has been modified, but has been removed in revision %s as of %s",
		       fn_root(finfo->fullname), jrev2, jdate2);
	    else
		error (0, 0,
		       "file %s has been modified, but has been removed in revision %s",
		       fn_root(finfo->fullname), jrev2);

	    /* FIXME: Should we arrange to return a non-zero exit
               status?  */

	    if (rev1 != NULL)
		xfree (rev1);

	    return;
	}

	/* The user file exists and has not been modified.  Mark it
           for removal.  FIXME: If we are doing a checkout, this has
           the effect of first checking out the file, and then
           removing it.  It would be better to just register the
           removal. 
	
	   The same goes for a removal then an add.  e.g.
	   cvs up -rbr -jbr2 could remove and readd the same file
	 */
	/* save the rev since server_updated might invalidate it */
	mrev = (char*)xmalloc (strlen (vers->vn_user) + 2);
	sprintf (mrev, "-%s", vers->vn_user);
	edit_modified_file(finfo,vers);
#ifdef SERVER_SUPPORT
	if (server_active)
	{
	    server_scratch (finfo->file);
	    server_updated (finfo, vers, SERVER_UPDATED, (mode_t) -1, /* -1 is ok here, cause the file gets deleted any way */
			    NULL, (struct buffer *) NULL);
	}
#endif
	TRACE(3,"join_file() call Register (client or server)");
	Register(finfo->entries, finfo->file, mrev, vers->ts_rcs,
		vers->options, vers->tag, vers->date, vers->ts_conflict, merge_bugid?NULL:(join_rev2?rev1:rev2), merge_bugid?NULL:(join_rev2?rev2:NULL), vers->tt_rcs, update_baserev?vers->vn_rcs:vers->edit_revision, update_baserev?vers->tag:vers->edit_tag, vers->edit_bugid, NULL);
	xfree (mrev);
	/* We need to check existence_error here because if we are
           running as the server, and the file is up to date in the
           working directory, the client will not have sent us a copy.  */
	if (unlink_file (finfo->file) < 0 && ! existence_error (errno))
	    error (0, errno, "cannot remove file %s", fn_root(finfo->fullname));
#ifdef SERVER_SUPPORT
	if (server_active)
	    server_checked_in (finfo->file, finfo->update_dir,
			finfo->repository);
#endif
	if (! really_quiet)
	    error (0, 0, "scheduling %s for removal", fn_root(finfo->fullname));

	return;
    }

    /* If the target of the merge is the same as the working file
       revision, then there is nothing to do.  */
    if (vers->vn_user != NULL && strcmp (rev2, vers->vn_user) == 0)
    {
	if (rev1 != NULL)
	    xfree (rev1);
	xfree (rev2);
	return;
    }

    /* If rev1 is dead or does not exist, then the file was added
       between rev1 and rev2.  */
    if (rev1 == NULL || RCS_isdead (vers->srcfile, rev1))
    {
		/* If the file does not exist in the working directory, then
			we can just check out the new revision and mark it for
			addition.  */
		if (vers->vn_user == NULL)
		{
			Vers_TS *xvers;
			const char *saved_options = options;

			edit_modified_file(finfo,vers);
			xvers = Version_TS (finfo, options, jrev2, jdate2, 1, 0, 0);

			options = xvers->options;
			/* FIXME: If checkout_file fails, we should arrange to
				return a non-zero exit status.  */
			status = checkout_file(finfo, xvers, 1, 0, 1, 0, is_rcs, jrev1?rev1:rev2, jrev1?rev2:NULL);

			options = saved_options;

			freevers_ts (&xvers);

			xfree (rev1);
			xfree (rev2);
			return;
		}

		/* If we're only merging from a single branch, just take
		   a delta from an imaginary empty revision.

			There are actually two possible situations here:

			(a) the two files were both added identically in the
				branch, and we want to merge them just in case there
				are differences
			(b) two different files were added with the same name,
				and we should produce a confilict 

			The old cvs behaviour was to print a warning and punt, which
			is fine(ish) if you're merging from the branchpoint each time,
			but no good if you want to track two parallel branches with
			mergepoints.
		*/
		if(jrev1 || jdate1 || jdate2)
		{
			if (jdate2 != NULL)
				error (0, 0,
				"file %s exists, but has been added in revision %s as of %s",
				fn_root(finfo->fullname), jrev2, jdate2);
			else
				error (0, 0,
				"file %s exists, but has been added in revision %s",
				fn_root(finfo->fullname), jrev2);

			xfree (rev1);
			xfree (rev2);
			return;
		}
	}

    /* If the two merge revisions are the same, then there is nothing
       to do.  */
    if (rev1 && rev2 && strcmp (rev1, rev2) == 0)
    {
	xfree (rev1);
	xfree (rev2);
	return;
    }

    /* If there is no working file, then we can't do the merge.  */
    if (vers->vn_user == NULL)
    {
	xfree (rev1);
	xfree (rev2);

	if (jdate2 != NULL)
	    error (0, 0,
		   "file %s does not exist, but is present in revision %s as of %s",
		   fn_root(finfo->fullname), jrev2, jdate2);
	else
	    error (0, 0,
		   "file %s does not exist, but is present in revision %s",
		   fn_root(finfo->fullname), jrev2);

	/* FIXME: Should we arrange to return a non-zero exit status?  */

	return;
    }

	edit_modified_file(finfo,vers);

    t_options = xstrdup(vers->options);
	RCS_get_kflags(t_options, false, kf);
	if(kf.flags&KFLAG_VALUE)
	{
		kf.flags=(kf.flags&~KFLAG_VALUE)|KFLAG_VALUE_LOGONLY;
		char *topt = (char*)xmalloc(128);
		xfree(t_options);
		t_options=RCS_rebuild_options(&kf,(char*)topt);
	}

#ifdef SERVER_SUPPORT
    if (server_active && !isreadable (finfo->file))
    {
	int retcode;

	if(!strcmp(vers->vn_user,"0"))
		return; /* Should we print something here? */

	/* The file is up to date.  Need to check out the current contents.  */
	retcode = RCS_checkout (vers->srcfile, finfo->file,
				vers->vn_user, (char *) NULL,
				t_options, RUN_TTY,
				(RCSCHECKOUTPROC) NULL, (void *) NULL, &mode);
	if (retcode != 0)
	    error (1, 0,
		   "failed to check out %s file", fn_root(finfo->fullname));
    }
#endif

    /*
     * The users currently modified file is moved to a backup file name
     * ".#filename.version", so that it will stay around for a few days
     * before being automatically removed by some cron daemon.  The "version"
     * is the version of the file that the user was most up-to-date with
     * before the merge.
     */
    backup = (char*)xmalloc (strlen (finfo->file)
		      + strlen (vers->vn_user)
		      + sizeof (BAKPREFIX)
		      + 10);
    (void) sprintf (backup, "%s%s.%s", BAKPREFIX, finfo->file, vers->vn_user);

    copy_file (finfo->file, backup, 1, 1);
    xchmod (finfo->file, 1);

    /* If the source of the merge is the same as the working file
       revision, then we can just RCS_checkout the target (no merging
       as such).  In the text file case, this is probably quite
       similar to the RCS_merge, but in the binary file case,
       RCS_merge gives all kinds of trouble.  */
    if (vers->vn_user != NULL && rev1 != NULL && strcmp (rev1, vers->vn_user) == 0
	/* See comments above about how No_Difference has already been
	   called.  */
	&& vers->ts_user != NULL && strcmp (vers->ts_user, vers->ts_rcs) == 0

	/* This is because of the worry below about $Name.  If that
	   isn't a problem, I suspect this code probably works for
	   text files too.  */
	&& ((kf.flags&KFLAG_BINARY) || wrap_merge_is_copy (finfo->file)))
    {
		/* FIXME: what about nametag?  What does RCS_merge do with
		$Name?  */
		if (RCS_checkout (finfo->rcs, finfo->file, rev2, NULL, t_options,
				RUN_TTY, (RCSCHECKOUTPROC)0, NULL, &mode) != 0)
			status = 2;
		else
			status = 0;

		/* OK, this is really stupid.  RCS_checkout carefully removes
		write permissions, and we carefully put them back.  But
		until someone gets around to fixing it, that seems like the
		easiest way to get what would seem to be the right mode.
		I don't check CVSWRITE or _watched; I haven't thought about
		that in great detail, but it seems like a watched file should
		be checked out (writable) after a merge.  */
		xchmod (finfo->file, 1);

		/* Traditionally, the text file case prints a whole bunch of
		scary looking and verbose output which fails to tell the user
		what is really going on (it gives them rev1 and rev2 but doesn't
		indicate in any way that rev1 == vn_user).  I think just a
		simple "U foo" is good here; it seems analogous to the case in
		which the file was added on the branch in terms of what to
		print.  */
		write_letter (finfo, 'U');
    }
    else if ((kf.flags&KFLAG_BINARY)
	     || wrap_merge_is_copy (finfo->file)
	     || special_file_mismatch (finfo, rev1, rev2))
    {
	/* We are dealing with binary files, or files with a
	   permission/linkage mismatch, and real merging would
	   need to take place.  This is a conflict.  We give the user
	   the two files, and let them resolve it.  It is possible
	   that we should require a "touch foo" or similar step before
	   we allow a checkin.  */
	if (RCS_checkout (finfo->rcs, finfo->file, rev2, NULL, t_options,
			  RUN_TTY, (RCSCHECKOUTPROC)0, NULL, &mode) != 0)
	    status = 2;
	else
	    status = 0;

	/* OK, this is really stupid.  RCS_checkout carefully removes
	   write permissions, and we carefully put them back.  But
	   until someone gets around to fixing it, that seems like the
	   easiest way to get what would seem to be the right mode.
	   I don't check CVSWRITE or _watched; I haven't thought about
	   that in great detail, but it seems like a watched file should
	   be checked out (writable) after a merge.  */
	xchmod (finfo->file, 1);

	/* Hmm.  We don't give them REV1 anywhere.  I guess most people
	   probably don't have a 3-way merge tool for the file type in
	   question, and might just get confused if we tried to either
	   provide them with a copy of the file from REV1, or even just
	   told them what REV1 is so they can get it themself, but it
	   might be worth thinking about.  */
	/* See comment in merge_file about the "nonmergeable file"
	   terminology.  */
	error (0, 0, "nonmergeable file needs merge");
	error (0, 0, "revision %s from repository is now in %s",
	       rev2, fn_root(finfo->fullname));
	error (0, 0, "file from working directory is now in %s", backup);
	write_letter (finfo, 'C');
    }
    else
	status = RCS_merge (finfo->rcs, vers->srcfile->path, finfo->file,
			t_options, (char*)(rev1?rev1:"0"), rev2, conflict_3way, &mode);

	if(status == 3) 
	{
		//cvs_output ("No difference between revisions ", 0);
		//cvs_output (rev1?rev1:"0",0);
		//cvs_output (" and ", 0);
		//cvs_output (rev2, 0);
		//cvs_output (" of ", 0);
		//cvs_output (finfo->file, 0);
		//cvs_output ("\n", 1);

		xfree(rev1);
		xfree(rev2);
		xfree(backup);
		xfree(options);
		return;
	}

	kflag kftmp;
	RCS_get_kflags(vers->options,false,kftmp);

	CXmlNodePtr node=fileattr_getroot();
	node->xpathVariable("name",finfo->file);

	if (!is_rcs
		&& cvswrite
		&& (!node->Lookup("file[cvs:filename(@name,$name)]/watched") || !node->XPathResultNext())
		&& !(kftmp.flags&KFLAG_RESERVED_EDIT))
	{
		mode = modify_mode(mode, 
				  ((mode & S_IRUSR) ? S_IWUSR : 0)
				| ((mode & S_IRGRP) ? S_IWGRP : 0)
				| ((mode & S_IROTH) ? S_IWOTH : 0),0);
	}

	if (status != 0 && status != 1 && status != 3)
    {
	error (0, status == -1 ? errno : 0,
	       "could not merge revision %s of %s", rev2, fn_root(finfo->fullname));
	error (status == -1 ? 1 : 0, 0, "restoring %s from backup file %s",
	       fn_root(finfo->fullname), backup);
	rename_file (backup, finfo->file);
    }
	else if(status == 1)
	{
		// RCS_merge failed: conflict
		write_letter (finfo, 'C');
	}

    /* The file has changed, but if we just checked it out it may
       still have the same timestamp it did when it was first
       registered above in checkout_file.  We register it again with a
       dummy timestamp to make sure that later runs of CVS will
       recognize that it has changed.

       We don't actually need to register again if we called
       RCS_checkout above, and we aren't running as the server.
       However, that is not the normal case, and calling Register
       again won't cost much in that case.  */
    {
	char *cp = 0;

	if (status)
	{
	    (void) time (&last_register_time);
	    cp = time_stamp (finfo->file, 0);
	}
	Register (finfo->entries, finfo->file,
		  vers->vn_rcs ? vers->vn_rcs : "0", "Result of merge",
		  vers->options, vers->tag, vers->date, cp, join_rev2?rev1:rev2, join_rev2?rev2:NULL, vers->tt_rcs, update_baserev?vers->vn_rcs:vers->edit_revision, update_baserev?vers->tag:vers->edit_tag, vers->edit_bugid, NULL);
	if (cp)
	    xfree(cp);
    }

    xfree (rev1);
    xfree (rev2);
#ifdef SERVER_SUPPORT
    if (server_active)
    {
		server_copy_file (finfo->file, finfo->update_dir, finfo->repository,
				backup);

		server_updated (finfo, vers, SERVER_MERGED,
				mode, NULL,
				(struct buffer *) NULL);
    }
#endif
    xfree (backup);
	xfree (t_options);
	baserev_update(finfo, vers, T_NEEDS_MERGE);
}

/*
 * Report whether revisions REV1 and REV2 of FINFO agree on:
 *   . file ownership
 *   . permissions
 *   . major and minor device numbers
 *   . symbolic links
 *   . hard links
 *
 * If either REV1 or REV2 is NULL, the working copy is used instead.
 *
 * Return 1 if the files differ on these data.
 */

int special_file_mismatch (struct file_info *finfo, const char *rev1, const char *rev2)
{
    struct stat sb;
    RCSVers *vp;
    Node *n;
    mode_t rev1_mode, rev2_mode;
    int check_modes;
    int result;

    check_modes = 1;

    /* Obtain file information for REV1.  If this is null, then stat
       finfo->file and use that info. */
    /* If a revision does not know anything about its status,
       then presumably it doesn't matter, and indicates no conflict. */

    if (rev1 == NULL)
    {
	    if (CVS_LSTAT (finfo->file, &sb) < 0)
			error (1, errno, "could not get file information for %s",
		       finfo->file);
	    rev1_mode = sb.st_mode;
	}
    else
    {
	n = findnode (finfo->rcs->versions, rev1);
	vp = (RCSVers *) n->data;

	n = findnode (vp->other_delta, "permissions");
	if (n == NULL)
		check_modes = 0;	/* don't care */
	else
		rev1_mode = strtoul (n->data, NULL, 8) & 0777;
    }

    /* Obtain file information for REV2. */
    if (rev2 == NULL)
    {
	    if (CVS_LSTAT (finfo->file, &sb) < 0)
		error (1, errno, "could not get file information for %s",
		       fn_root(finfo->file));
	    rev2_mode = sb.st_mode;
	}
    else
    {
	n = findnode (finfo->rcs->versions, rev2);
	vp = (RCSVers *) n->data;

	n = findnode (vp->other_delta, "permissions");
	if (n == NULL)
		check_modes = 0;	/* don't care */
	else
		rev2_mode = strtoul (n->data, NULL, 8) & 0777;
    }

    /* Check the file permissions, printing
       an error for each mismatch found.  Return 0 if all characteristics
       matched, and 1 otherwise. */

    result = 0;
  
	/* Only check the execute bit as that's the only sane thing to check */
	/* Win32 doesn't understand permissions in any meaningful sense, so 
	   we just skip the check there */
#ifndef _WIN32
	if (check_modes &&
	    (rev1_mode & 0111) != (rev2_mode & 0111))
	{
	    error (0, 0, "%s: permission mismatch between %s and %s",
		   fn_root(finfo->file),
		   (rev1 == NULL ? "working file" : rev1),
		   (rev2 == NULL ? "working file" : rev2));
	    result = 0;
	}
#endif

    return result;
}

int
joining ()
{
    return (join_rev1 != NULL);
}

void set_global_update_options(int _merge_from_branchpoint, int _conflict_3way, int _case_sensitive, int _force_checkout_time)
{
	merge_from_branchpoint=_merge_from_branchpoint;
	conflict_3way=_conflict_3way;
	case_sensitive=_case_sensitive;
	force_checkout_time=_force_checkout_time;
}

static int baserev_update(struct file_info *finfo, Vers_TS *vers, Ctype status)
{
	char *basefile, *basefilegz;
	int gzip = 0;

	if(noexec)
		return 0;
	if(!update_baserev && !file_is_edited)
		return 0;

	make_directories(CVSADM_BASE);

	basefile = (char*)xmalloc(strlen(finfo->fullname)+sizeof(CVSADM_BASE)+10);
	basefilegz = (char*)xmalloc(strlen(finfo->fullname)+sizeof(CVSADM_BASE)+15);
	sprintf(basefile,"%s/%s",CVSADM_BASE,finfo->file);
	sprintf(basefilegz,"%s/%s.gz",CVSADM_BASE,finfo->file);
	if(isfile(basefilegz))
		gzip=1;

	switch(status)
	{
		case T_UNKNOWN:			
		case T_UPTODATE:			
		case T_CONFLICT:		
		case T_MODIFIED:		
		case T_ADDED:			
		case T_REMOVED:	
		case T_TITLE:	
			/* Do nothing */
			break;
		case T_NEEDS_MERGE:
			/* Checkout new baserev */
		    RCS_checkout (vers->srcfile,
				   basefile,
				   vers->vn_rcs, vers->vn_tag,
				   vers->options, RUN_TTY,
				   NULL, NULL, NULL);
			if(!server_active)
			{
				if(gzip)
				{
					copy_and_zip_file(basefile,basefilegz,1,1);
					CVS_UNLINK(basefile);
				}
			}
#ifdef SERVER_SUPPORT
			else
				server_send_baserev(finfo, basefile, "U");
#endif
			break;
		case T_REMOVE_ENTRY:
			if(!server_active)
			{
				/* Remove baserev */
				CVS_UNLINK(basefile);
				CVS_UNLINK(basefilegz);
			}
#ifdef SERVER_SUPPORT
			else
				server_send_baserev(finfo, basefile, "R");
#endif
			break;
		case T_CHECKOUT:
		case T_PATCH:
			if(!server_active)
			{
				/* Copy baserev from new file */
				if(gzip)
					copy_and_zip_file(finfo->file,basefilegz,1,1);
				else
					copy_file(finfo->file,basefile,1,1);
			}
#ifdef SERVER_SUPPORT
			else
				server_send_baserev(finfo, basefile, "C");
#endif
			break;
	}

	xfree(basefile);
	xfree(basefilegz);

	xfree(vers->edit_revision);
	xfree(vers->edit_tag);

	vers->edit_revision = xstrdup(vers->vn_rcs);
	vers->edit_tag = xstrdup(vers->vn_tag);
	return 0;
}

