/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * Copyright (c) 2004, Tony Hoyle
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Call external diff on a file version
 */

#include "cvs.h"

#include "../cvsapi/TagDate.h"
#include "../cvsapi/TokenLine.h"
#define XDIFF_EXPORT
#include "../xdiff/xdiff.h"

enum xdiff_file
{
    XDIFF_ERROR,
    XDIFF_ADDED,
    XDIFF_REMOVED,
    XDIFF_DIFFERENT,
    XDIFF_SAME
};

static int xdiff_fileproc(void *callerdat, struct file_info *finfo);
static Dtype xdiff_direntproc(void *callerdat, char *dir,
				      char *repos, char *update_dir,
				      List *entries, const char *virtual_repository, Dtype hint);
static int xdiff_filesdoneproc (void *callerdat, int err, char *repos, char *update_dir, List *entries);
static int xdiff_dirleaveproc (void *callerdat, char *dir, int err, char *update_dir, List *entries);
static enum xdiff_file xdiff_file_nodiff(struct file_info *finfo, Vers_TS *vers, enum xdiff_file empty_file, const char *default_branch);
static int xdiff_exec(const char *name, const char *file1, const char *file2, const char *label1, const char *label2);

/* Revision of the user file, if it is unchanged from something in the
   repository and we want to use that fact.  */
static char *user_file_rev;
static int empty_files;
static int diff_errors;
static CTagDate tags(false);
static char *use_rev1, *use_rev2;
static int have_rev1_label, have_rev2_label;
static const char *xdiff_options;

static const char *const xdiff_usage[] =
{
    "Usage: %s %s [-lNR] [-o xdiff-options]\n",
    "    [[-r rev1 | -D date1] [-r rev2 | -D date2]] [files...]\n",
    "\t-D d1\tDiff revision for date against working file.\n",
    "\t-D d2\tDiff rev1/date1 against date2.\n",
    "\t-N\tinclude diffs for added and removed files.\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-l\tLocal directory only, not recursive\n",
	"\t-o\tPass extra options to xdiff\n",
    "\t-r rev1\tDiff revision for rev1 against working file.\n",
    "\t-r rev2\tDiff rev1/date1 against rev2.\n",
    "(consult the documentation for your xdiff extension for xdiff-options.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int xdiff(int argc, char **argv)
{
    int c;
    int err = 0;
	int local = 0;
	int which;

    if (argc == -1)
		usage (xdiff_usage);

    optind = 0;
	while ((c = getopt (argc, argv, "+lNRr:D:o:")) != -1)
    {
	switch (c)
	{
	case 'o':
		xdiff_options = xstrdup(optarg);
		break;
	case 'l':
		local = 1;
		break;
	case 'r':
		if(!tags.AddTag(optarg))
			error(1,0,"Couldn't parse tag '%s'",optarg);

		if(tags.size()>2)
			usage(xdiff_usage);
		break;
	case 'D':
		if(!tags.AddTag(optarg,true))
			error(1,0,"Couldn't parse date '%s'",optarg);

		if(tags.size()>2)
			usage(xdiff_usage);
		break;
	case 'N':
		empty_files = 1;
	case 'R':
		local = 0;
		break;
    case '?':
    default:
		usage (xdiff_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote)
    {
		if(!supported_request("xdiff"))
			error (1, 0, "server does not support %s",command_name);
	
		if(local)
			send_arg("-l");
		if(empty_files)
			send_arg("-N");
		
		if(tags.size())
		{
			for(size_t n=0; n<tags.size(); n++)
			{
				if(tags[n].hasDate())
					option_with_arg("-D",tags[n].dateText());
				else
					option_with_arg("-r",tags[n].tag());
			}
		}

		send_arg("--");
		/* Send the current files unless diffing two revs from the archive */
		if (tags.size()<2)
			send_files (argc, argv, local, 0, 0);
		else
			send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
		send_file_names (argc, argv, SEND_EXPAND_WILD);

		send_to_server ("xdiff\n", 0);
		err = get_responses_and_close ();
		xfree(xdiff_options);
		return err;
    }

	for(size_t n=0; n<tags.size(); n++)
	{
		if(tags[n].hasTag())
			tag_check_valid(tags[n].tag(), argc, argv, local, 0, "");
	}

	which = W_LOCAL;
	if (tags.size())
		which |= W_REPOS;

	/* start the recursion processor */
	err = start_recursion (xdiff_fileproc, xdiff_filesdoneproc, (PREDIRENTPROC) NULL, xdiff_direntproc,
			xdiff_dirleaveproc, NULL, argc, argv, local,
			which, 0, 1, (char *) NULL, NULL, 1, verify_read, NULL);

	xfree(xdiff_options);
    return err;
}


/*
 * display the status of a file
 */
/* ARGSUSED */
static int xdiff_fileproc(void *callerdat, struct file_info *finfo)
{
    int status, err = 2;		/* 2 == trouble, like rcsdiff */
    Vers_TS *vers;
    enum xdiff_file empty_file = XDIFF_DIFFERENT;
    char *tmp=NULL,*tmp2=NULL;
    char *fname;
    char *label1;
    char *label2;
	char *default_branch;

    /* Initialize these solely to avoid warnings from gcc -Wall about
       variables that might be used uninitialized.  */
    tmp = NULL;
    fname = NULL;

    user_file_rev = 0;
    vers = Version_TS (finfo, NULL, NULL, NULL, 1, 0, 0);
	if(vers->tag && RCS_isbranch(finfo->rcs, vers->tag))
		default_branch = vers->tag;
	else
		default_branch = NULL;

    if (tags.size()==2)
    {
		/* Skip all the following checks regarding the user file; we're
		   not using it.  */
    }
    else if (vers->vn_user == NULL)
    {
		/* The file does not exist in the working directory.  */
		if (tags.size() && vers->srcfile != NULL)
		{
			/* The file does exist in the repository.  */
			if (empty_files)
				empty_file = XDIFF_REMOVED;
			else
			{
				int exists=0;

				/* special handling for TAG_HEAD */
				if (tags[0].hasTag() && !strcmp(tags[0].tag(), TAG_HEAD))
				{
					const char *head = (vers->vn_rcs == NULL ? NULL : RCS_branch_head (vers->srcfile, vers->vn_rcs));
					exists = head != NULL;
					xfree (head);
				}
				else
				{
					Vers_TS *xvers;

					xvers = Version_TS (finfo, NULL, tags[0].hasTag()?tags[0].tag():default_branch, tags[0].hasDate()?tags[0].dateText():NULL, 1, 0, 0);
					exists = xvers->vn_rcs != NULL;
					freevers_ts (&xvers);
				}
				if (exists)
					error (0, 0, "%s no longer exists, no comparison available", fn_root(finfo->fullname));
				freevers_ts (&vers);
				diff_errors+=err;
				return err;
			}
		}
		else
		{
			error (0, 0, "I know nothing about %s", fn_root(finfo->fullname));
			freevers_ts (&vers);
			diff_errors+=err;
			return err;
		}
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
    {
		if (empty_files)
			empty_file = XDIFF_ADDED;
		else
		{
			error (0, 0, "%s is a new entry, no comparison available", fn_root(finfo->fullname));
			freevers_ts (&vers);
			diff_errors+=err;
			return err;
		}
    }
    else if (vers->vn_user[0] == '-')
    {
		if (empty_files)
			empty_file = XDIFF_REMOVED;
		else
		{
			error (0, 0, "%s was removed, no comparison available", fn_root(finfo->fullname));
			freevers_ts (&vers);
			diff_errors+=err;
			return err;
		}
    }
    else
    {
		if (vers->vn_rcs == NULL && vers->srcfile == NULL)
		{
			error (0, 0, "cannot find revision control file for %s",
			fn_root(finfo->fullname));
			freevers_ts (&vers);
			diff_errors+=err;
			return err;
		}
		else
		{
			if (vers->ts_user == NULL)
			{
				error (0, 0, "cannot find %s", fn_root(finfo->fullname));
				freevers_ts (&vers);
				diff_errors+=err;
				return err;
			}
			else if (!strcmp (vers->ts_user, vers->ts_rcs)) 
			{
				/* The user file matches some revision in the repository
				Diff against the repository (for remote CVS, we might not
				have a copy of the user file around).  */
				user_file_rev = vers->vn_user;
			}
		}
    }

    empty_file = xdiff_file_nodiff (finfo, vers, empty_file, default_branch);
    if (empty_file == XDIFF_SAME || empty_file == XDIFF_ERROR)
    {
		freevers_ts (&vers);
		if (empty_file == XDIFF_SAME)
			return 0;
		else
		{
			diff_errors+=err;
			return err;
		}
    }

    if (empty_file == XDIFF_DIFFERENT)
    {
		int dead1, dead2;

		if (use_rev1 == NULL)
			dead1 = 0;
		else
			dead1 = RCS_isdead (vers->srcfile, use_rev1);
		if (use_rev2 == NULL)
			dead2 = 0;
		else
			dead2 = RCS_isdead (vers->srcfile, use_rev2);

		if (dead1 && dead2)
		{
			freevers_ts (&vers);
			return 0;
		}
		else if (dead1)
		{
			if (empty_files)
				empty_file = XDIFF_ADDED;
			else
			{
				error (0, 0, "%s is a new entry, no comparison available", fn_root(finfo->fullname));
				freevers_ts (&vers);
				diff_errors+=err;
				return err;
			}
		}
		else if (dead2)
		{
			if (empty_files)
				empty_file = XDIFF_REMOVED;
			else
			{
				error (0, 0, "%s was removed, no comparison available",	fn_root(finfo->fullname));
				freevers_ts (&vers);
				diff_errors+=err;
				return err;
			}
		}
    }

    /* Output an "Index:" line for patch to use */
	cvs_output ("Index: ", 0);
	cvs_output (fn_root(finfo->fullname), 0);
	cvs_output ("\n", 1);

    /* Set up file labels appropriate for compatibility with the Larry Wall
     * implementation of patch if the user didn't specify.  This is irrelevant
     * according to the POSIX.2 specification.
     */
    label1 = NULL;
    label2 = NULL;
    if (!have_rev1_label)
    {
		if (empty_file == XDIFF_ADDED)
			label1 = make_file_label (DEVNULL, NULL, NULL, 0);
		else
			label1 = make_file_label (fn_root(finfo->fullname), use_rev1, vers ? vers->srcfile : NULL, 0);
    }

    if (!have_rev2_label)
    {
	if (empty_file == XDIFF_REMOVED)
	    label2 = make_file_label (DEVNULL, NULL, NULL, 0);
	else
	    label2 = make_file_label (fn_root(finfo->fullname), use_rev2, vers ? vers->srcfile : NULL, 0);
    }

	/* This is fullname, not file, possibly despite the POSIX.2
	* specification, because that's the way all the Larry Wall
	* implementations of patch (are there other implementations?) want
	* things and the POSIX.2 spec appears to leave room for this.
	*/
	cvs_output ("===================================================================\nRCS file: ", 0);
	if(finfo->rcs)
		cvs_output (fn_root(finfo->rcs->path), 0);
	else
		cvs_output ("New file!", 0);
	cvs_output ("\n", 1);

	if (empty_file == XDIFF_ADDED || empty_file == XDIFF_REMOVED)
    {
		if (empty_file == XDIFF_ADDED)
		{
			if (use_rev2 == NULL)
				status = xdiff_exec (last_component(finfo->file),DEVNULL, finfo->file, label1+2, label2+2);
			else
			{
				int retcode;

				printf("retrieving revision %s\n",use_rev2);
				tmp = cvs_temp_name ();
				retcode = RCS_checkout (vers->srcfile, (char *) NULL,
							use_rev2, (char *) NULL,
							vers->options,
							tmp, (RCSCHECKOUTPROC) NULL,
							(void *) NULL, NULL);
				if (retcode != 0)
				{
					diff_errors+=err;
					return err;
				}

				status = xdiff_exec (last_component(finfo->file),DEVNULL, tmp, label1+2, label2+2);
			}
		}
		else
		{
			int retcode;

			printf("retrieving revision %s\n",use_rev1);
			tmp = cvs_temp_name ();
			retcode = RCS_checkout (vers->srcfile, (char *) NULL,
						use_rev1, (char *) NULL,
						vers->options,
						tmp, (RCSCHECKOUTPROC) NULL,
						(void *) NULL, NULL);
			if (retcode != 0)
			{
				diff_errors+=err;
				return err;
			}

			status = xdiff_exec (last_component(finfo->file),tmp, DEVNULL, label1+2, label2+2);
		}
    }
    else
    {
		int retcode;

		printf("retrieving revision %s\n",use_rev1);
		tmp = cvs_temp_name ();
		retcode = RCS_checkout (vers->srcfile, (char *) NULL,
					use_rev1, (char *) NULL,
					vers->options,
					tmp, (RCSCHECKOUTPROC) NULL,
					(void *) NULL, NULL);
		if (retcode != 0)
		{
			diff_errors+=err;
			return err;
		}

		if(tags.size()==2)
		{
			printf("retrieving revision %s\n",use_rev2);
			tmp2 = cvs_temp_name ();
			retcode = RCS_checkout (vers->srcfile, (char *) NULL,
						use_rev2, (char *) NULL,
						vers->options,
						tmp2, (RCSCHECKOUTPROC) NULL,
						(void *) NULL, NULL);
			if (retcode != 0)
			{
				diff_errors+=err;
				return err;
			}
		}

		status = xdiff_exec (last_component(finfo->file),tmp, tmp2?tmp2:finfo->file, label1+2, label2+2);

    }
	xfree (label1);
	xfree (label2);

    switch (status)
    {
	case -1:			/* fork failed */
	    error (1, errno, "unable to execute xdiff program while diffing %s", fn_root(vers->srcfile->path));
	case 0:				/* everything ok */
	    err = 0;
	    break;
	default:			/* other error */
	    err = status;
	    break;
    }

	if(tmp)
    {
		if (CVS_UNLINK (tmp) < 0)
			error (0, errno, "cannot remove %s", tmp);
		xfree (tmp);
    }

	if(tmp2)
    {
		if (CVS_UNLINK (tmp2) < 0)
			error (0, errno, "cannot remove %s", tmp2);
		xfree (tmp2);
    }

    freevers_ts (&vers);
    diff_errors+=err;
    return err;
}

static Dtype xdiff_direntproc(void *callerdat, char *dir,
				      char *repos, char *update_dir,
				      List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

    if (!quiet)
		error (0, 0, "Diffing %s", update_dir);
    return R_PROCESS;
}

/*
 * Concoct the proper exit status - done with files
 */
/* ARGSUSED */
static int xdiff_filesdoneproc (void *callerdat, int err, char *repos, char *update_dir, List *entries)
{
    return diff_errors;
}

/*
 * Concoct the proper exit status - leaving directories
 */
/* ARGSUSED */
static int xdiff_dirleaveproc (void *callerdat, char *dir, int err, char *update_dir, List *entries)
{
    return diff_errors;
}

/*
 * verify that a file is different
 */
static enum xdiff_file xdiff_file_nodiff(struct file_info *finfo, Vers_TS *vers, enum xdiff_file empty_file, const char *default_branch)
{
    Vers_TS *xvers;

    /* free up any old use_rev* variables and reset 'em */
	xfree (use_rev1);
	xfree (use_rev2);

    if (tags.size())
    {
		/* special handling for TAG_HEAD */
		if (tags[0].hasTag() && !strcmp (tags[0].tag(), TAG_HEAD))
			use_rev1 = ((vers->vn_rcs == NULL || vers->srcfile == NULL) ? NULL : RCS_branch_head (vers->srcfile, vers->vn_rcs));
		else
		{
			xvers = Version_TS(finfo, NULL, tags[0].hasTag()?tags[0].tag():default_branch, tags[0].hasDate()?tags[0].dateText():NULL, 1, 0, 0);
			if (xvers->vn_rcs != NULL)
				use_rev1 = xstrdup (xvers->vn_rcs);
			freevers_ts (&xvers);
		}
    }
    if (tags.size()==2)
    {
		/* special handling for TAG_HEAD */
		if (tags[1].hasTag() && !strcmp (tags[1].tag(), TAG_HEAD))
			use_rev1 = ((vers->vn_rcs == NULL || vers->srcfile == NULL) ? NULL : RCS_branch_head (vers->srcfile, vers->vn_rcs));
		else
		{
			xvers = Version_TS(finfo, NULL, tags[1].hasTag()?tags[1].tag():default_branch, tags[1].hasDate()?tags[1].dateText():NULL, 1, 0, 0);
			if (xvers->vn_rcs != NULL)
				use_rev2 = xstrdup (xvers->vn_rcs);
			freevers_ts (&xvers);
		}

		if (use_rev1 == NULL)
		{
			/* The first revision does not exist.  If EMPTY_FILES is
				true, treat this as an added file.  Otherwise, warn
				about the missing tag.  */
			if (use_rev2 == NULL)
			/* At least in the case where XDIFF_REV1 and XDIFF_REV2
			are both numeric, we should be returning some kind
			of error (see basicb-8a0 in testsuite).  The symbolic
			case may be more complicated.  */
				return XDIFF_SAME;
			else if (empty_files)
				return XDIFF_ADDED;
			else if (tags[0].hasTag())
				error (0, 0, "tag %s is not in file %s", tags[0].tag(), fn_root(finfo->fullname));
			else
				error (0, 0, "no revision for date %s in file %s", tags[0].tag(), fn_root(finfo->fullname));
			return XDIFF_ERROR;
		}

		if (use_rev2 == NULL)
		{
			/* The second revision does not exist.  If EMPTY_FILES is
				true, treat this as a removed file.  Otherwise warn
				about the missing tag.  */
			if (empty_files)
				return XDIFF_REMOVED;
			else if (tags[1].hasTag())
				error (0, 0, "tag %s is not in file %s", tags[1].tag(), fn_root(finfo->fullname));
			else
				error (0, 0, "no revision for date %s in file %s", tags[1].tag(), fn_root(finfo->fullname));
			return XDIFF_ERROR;
		}

		/* now, see if we really need to do the diff */
		if (!strcmp (use_rev1, use_rev2))
			return XDIFF_SAME;
		else
			return XDIFF_DIFFERENT;
    }

    if (tags.size() && use_rev1 == NULL)
    {
		/* The first revision does not exist, and no second revision
			was given.  */
		if (empty_files)
		{
			if (empty_file == XDIFF_REMOVED)
				return XDIFF_SAME;
			else
			{
				if (user_file_rev && use_rev2 == NULL)
					use_rev2 = xstrdup (user_file_rev);
				return XDIFF_ADDED;
			}
		}
		else
		{
			if (tags[0].hasTag())
				error (0, 0, "tag %s is not in file %s", tags[0].tag(), fn_root(finfo->fullname));
			else
				error (0, 0, "no revision for date %s in file %s", tags[0].tag(), fn_root(finfo->fullname));
			return XDIFF_ERROR;
		}
    }

    if (user_file_rev)
    {
        /* drop user_file_rev into first unused use_rev */
        if (!use_rev1) 
			use_rev1 = xstrdup (user_file_rev);
		else if (!use_rev2)
			use_rev2 = xstrdup (user_file_rev);
		/* and if not, it wasn't needed anyhow */
		user_file_rev = 0;
    }

    /* now, see if we really need to do the diff */
    if (use_rev1 && use_rev2) 
    {
		if (strcmp (use_rev1, use_rev2) == 0)
			return XDIFF_SAME;
		else
			return XDIFF_DIFFERENT;
    }

    if (use_rev1 == NULL || (vers->vn_user != NULL && !strcmp (use_rev1, vers->vn_user)))
    {
		if (empty_file == XDIFF_DIFFERENT && vers->ts_user && !strcmp (vers->ts_rcs, vers->ts_user))
		    return XDIFF_SAME;
		
		if (use_rev1 == NULL && (vers->vn_user[0] != '0' || vers->vn_user[1] != '\0'))
		{
			if (vers->vn_user[0] == '-')
				use_rev1 = xstrdup (vers->vn_user + 1);
			else
			use_rev1 = xstrdup (vers->vn_user);
		}
    }

	return empty_file;
}

static int xdiff_exec (const char *name, const char *file1, const char *file2, const char *label1, const char *label2)
{
	const char *wrap = wrap_xdiffwrapper(name);

	if(wrap)
	{
		CTokenLine line(wrap);

		if(xdiff_options)
			line.addArgs(xdiff_options);

		if(line.size())
		{
			CLibraryAccess lib;
			if(!lib.Load(line[0],CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDXdiff)))
				error(1,0,"Cannot load xdiff handler for %s",name);

			get_plugin_interface_t fn = (get_plugin_interface_t)lib.GetProc("get_plugin_interface");
			if(!fn)
				error(1,0,"Cannot load xdiff handler for %s",name);

			plugin_interface *pi = fn();
			if(!pi)
				error(1,0,"Cannot load xdiff handler for %s",name);

			if(pi->interface_version!=PLUGIN_INTERFACE_VERSION)
				error(1,0,"xdiff plugin handler for %s is wrong version",name);

			if(pi->init)
			{
				if(pi->init(pi))
				{
					CServerIo::trace(3,"Not loading xdiff %s - initialisation failed",name);
					return 0;
				}
			}

			xdiff_interface *xd;

			if(!pi->get_interface || (xd = (xdiff_interface*)pi->get_interface(pi,pitXdiff,NULL))==NULL)
			{
					CServerIo::trace(3,"Xdiff library does not support protocol interface.");
					return 0;
			}
		
			const char *const*argv = line.toArgv(1);
			int argc = line.size()-1;

			int ret = xd->xdiff_function(name,file1,file2,label1,label2,argc,argv,cvs_output);
			return ret;
		}
		else
		{
			error(0,0,"%s has no xdiff wrapper defined",name);
		}
	}
	else
	{
		error(0,0,"%s has no xdiff wrapper defined",name);
	}
	return 0;
}

