/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Status Information
 */

#include "cvs.h"

static Dtype status_dirproc(void *callerdat, char *dir,
				    char *repos, char *update_dir,
				    List *entries, const char *virtual_repository, Dtype hint);
static int status_fileproc(void *callerdat, struct file_info *finfo);
static int tag_list_proc(Node * p, void *closure);

static int local = 0;
static int quick = 0;
static int long_format = 0;
static RCSNode *xrcsnode;
static int supress_extra_fields;

static const char *const status_usage[] =
{
    "Usage: %s %s [-vlR] [files...]\n",
    "\t-v\tVerbose format; includes tag information for the file\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
	"\t-q\tDisplay a quick summary of each file (send more increased terseness).\n",
	"\t-x\tcvsnt 2.x compatible output (default).\n",
	"\t-X\tcvs 1.x compatible output.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int cvsstatus (int argc, char **argv)
{
    int c;
    int err = 0;

	supress_extra_fields = compat[compat_level].hide_extended_status;

    if (argc == -1)
	usage (status_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+vlRqxX")) != -1)
    {
	switch (c)
	{
	    case 'v':
		long_format = 1;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
		case 'q':
		quick++;
		break;
		case 'x':
			supress_extra_fields = 0;
			break;
		case 'X':
			supress_extra_fields = 1;
			break;
	    case '?':
	    default:
		usage (status_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote)
    {
	if (long_format)
	    send_arg("-v");
	if (local)
	    send_arg("-l");
	if (supress_extra_fields)
		send_arg("-X");
	for(c=0; c<quick; c++)
		send_arg("-q");
	send_arg("--");

	/* For a while, we tried setting SEND_NO_CONTENTS here so this
	   could be a fast operation.  That prevents the
	   server from updating our timestamp if the timestamp is
	   changed but the file is unmodified.  Worse, it is user-visible
	   (shows "locally modified" instead of "up to date" if
	   timestamp is changed but file is not).  And there is no good
	   workaround (you might not want to run "cvs update"; "cvs -n
	   update" doesn't update CVS/Entries; "cvs diff --brief" or
	   something perhaps could be made to work but somehow that
	   seems nonintuitive to me even if so).  Given that timestamps
	   seem to have the potential to get munged for any number of
	   reasons, it seems better to not rely too much on them.  */

	send_files (argc, argv, local, 0, 0);

	send_file_names (argc, argv, SEND_EXPAND_WILD);

	send_to_server ("status\n", 0);
	err = get_responses_and_close ();

	return err;
    }

    /* start the recursion processor */
    err = start_recursion (status_fileproc, (FILESDONEPROC) NULL,
			   (PREDIRENTPROC) NULL, status_dirproc, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, NULL, 1, verify_read, NULL);

    return (err);
}

/*
 * display the status of a file
 */
/* ARGSUSED */
static int status_fileproc (void *callerdat, struct file_info *finfo)
{
    Ctype status;
    char *sstat;
    Vers_TS *vers;
	Node *node;

    status = Classify_File (finfo, (char *) NULL, (char *) NULL, (char *) NULL,
			    1, 0, &vers, 0, 0, 0);
    sstat = "Classify Error";
    switch (status)
    {
	case T_UNKNOWN:
	    sstat = "Unknown";
	    break;
	case T_CHECKOUT:
	    sstat = "Needs Checkout";
	    break;
	case T_PATCH:
	    sstat = "Needs Patch";
	    break;
	case T_CONFLICT:
	    /* I _think_ that "unresolved" is correct; that if it has
	       been resolved then the status will change.  But I'm not
	       sure about that.  */
	    sstat = "Unresolved Conflict";
	    break;
	case T_ADDED:
	    sstat = "Locally Added";
	    break;
	case T_REMOVED:
	    sstat = "Locally Removed";
	    break;
	case T_MODIFIED:
	    if (vers->ts_conflict)
		sstat = "File had conflicts on merge";
	    else
		sstat = "Locally Modified";
	    break;
	case T_REMOVE_ENTRY:
	    sstat = "Entry Invalid";
	    break;
	case T_UPTODATE:
	    sstat = "Up-to-date";
	    break;
	case T_NEEDS_MERGE:
	    sstat = "Needs Merge";
	    break;
	case T_TITLE:
	    /* I don't think this case can occur here.  Just print
	       "Classify Error".  */
	    break;
    }
	if(quick>=2 && status == T_UPTODATE)
	{
		freevers_ts(&vers);
		return 0;
	}
	if(!quick)
	{
    cvs_output ("\
===================================================================\n", 0);
	}
    if (vers->ts_user == NULL)
    {
	cvs_output ("File: no file ", 0);
	cvs_output (fn_root(finfo->file), 0);
	cvs_output ("\t\tStatus: ", 0);
	cvs_output (sstat, 0);
	cvs_output ("\n", 0);
    }
    else
    {
	char *buf;
	buf = (char*)xmalloc (strlen(finfo->update_dir) + strlen (finfo->file) + strlen (sstat) + 80);
	if(quiet && quick) // If we're not displaying the 'looking in....' messages, show the relative file instead
	{
		char *buf2 = (char*)xmalloc(32 + strlen(finfo->file) + 80);
		sprintf (buf2, "%-.32s/%s",finfo->update_dir,finfo->file);
		sprintf (buf, "%-49s\t%s\n", buf2, sstat);
		xfree(buf2);
	}
	else
		sprintf (buf, "File: %-17s\tStatus: %s\n", finfo->file, sstat);
	cvs_output (buf, 0);
	xfree (buf);
    }

	if(!quick)
	{
		cvs_output ("\n", 0);
    if (vers->vn_user == NULL)
    {
	cvs_output ("   Working revision:\tNo entry for ", 0);
	cvs_output (fn_root(finfo->file), 0);
	cvs_output ("\n", 0);
    }
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	cvs_output ("   Working revision:\tNew file!\n", 0);
    else if (server_active)
    {
	cvs_output ("   Working revision:\t", 0);
	cvs_output (vers->vn_user, 0);
	cvs_output ("\n", 0);
    }
    else
    {
	cvs_output ("   Working revision:\t", 0);
	cvs_output (vers->vn_user, 0);
	cvs_output ("\t", 0);
	cvs_output (vers->ts_rcs, 0);
	cvs_output ("\n", 0);
    }

    if (vers->vn_rcs == NULL)
		cvs_output ("   Repository revision:\tNo revision control file\n", 0);
    else
    {
		cvs_output ("   Repository revision:\t", 0);
		cvs_output (vers->vn_rcs, 0);
		cvs_output ("\t", 0);
		/* We lie... */
		{
		char *repo = Name_Repository(NULL,NULL);
		char *tmp = (char*)xmalloc(strlen(repo)+strlen(finfo->file)+sizeof(RCSEXT)+10);
		sprintf(tmp,"%s/%s%s",fn_root(repo),finfo->file,RCSEXT);
		cvs_output (tmp, 0);
		xfree(tmp);
		xfree(repo);
		}
		cvs_output ("\n", 0);
	}

	if(!supress_extra_fields && vers->srcfile && vers->vn_rcs)
	{
		RCSVers *v;
		
		node=findnode(vers->srcfile->versions,vers->vn_rcs);
		if(node)
		{
			v=(RCSVers*)node->data;
			cvs_output("   Expansion option:\t", 0);
			if(v->kopt)
				cvs_output(v->kopt, 0);
			else
				cvs_output("(default)", 0);
			cvs_output("\n",0);
			node = findnode(v->other_delta,"commitid");
			cvs_output("   Commit Identifier:\t", 0);
			if(node && node->data)
				cvs_output(node->data, 0);
			else
				cvs_output("(none)",0);
			cvs_output("\n",0);
		}
    }


    if (vers->entdata)
    {
	Entnode *edata;

	edata = vers->entdata;
	if (edata->tag)
	{
	    if (vers->vn_rcs == NULL)
	    {
			cvs_output ("   Sticky Tag:\t\t", 0);
			cvs_output (edata->tag, 0);
			if (vers->vn_user[0] != '0' || vers->vn_user[1] != '\0')
				cvs_output (" - MISSING from RCS file!\n", 0);
			else
				cvs_output ("\n", 0);
	    }
	    else
	    {
			if (isdigit ((unsigned char) edata->tag[0]))
			{
				cvs_output ("   Sticky Tag:\t\t", 0);
				cvs_output (edata->tag, 0);
				cvs_output ("\n", 0);
			}
			else
			{
				char *branch = NULL;

				if (RCS_nodeisbranch (finfo->rcs, edata->tag))
				branch = RCS_whatbranch(finfo->rcs, edata->tag);

				cvs_output ("   Sticky Tag:\t\t", 0);
				cvs_output (edata->tag, 0);
				cvs_output (" (", 0);
				cvs_output (branch ? "branch" : "revision", 0);
				cvs_output (": ", 0);
				cvs_output (branch ? branch : vers->vn_rcs, 0);
				cvs_output (")\n", 0);

				if (branch)
				xfree (branch);
			}
	    }
	}
	else if (!really_quiet)
	    cvs_output ("   Sticky Tag:\t\t(none)\n", 0);

	if (edata->date)
	{
	    cvs_output ("   Sticky Date:\t\t", 0);
	    cvs_output (edata->date, 0);
	    cvs_output ("\n", 0);
	}
	else if (!really_quiet)
	    cvs_output ("   Sticky Date:\t\t(none)\n", 0);

	if (edata->options && edata->options[0])
	{
	    cvs_output ("   Sticky Options:\t", 0);
	    cvs_output (edata->options, 0);
	    cvs_output ("\n", 0);
	}
	else if (!really_quiet)
	    cvs_output ("   Sticky Options:\t(none)\n", 0);

	if(!supress_extra_fields)
	{
		if (edata->merge_from_tag_1 && edata->merge_from_tag_1[0])
		{
			cvs_output ("   Merge From:\t\t", 0);
			cvs_output (edata->merge_from_tag_1, 0);
			if (edata->merge_from_tag_2 && edata->merge_from_tag_2[0])
			{
				cvs_output (" and ",0);
				cvs_output (edata->merge_from_tag_2, 0);
			}
			cvs_output ("\n", 0);
		}
		else if (!really_quiet)
			cvs_output ("   Merge From:\t\t(none)\n", 0);
		}
	}
	
    if (long_format && vers->srcfile)
    {
	List *symbols = RCS_symbols(vers->srcfile);

	cvs_output ("\n   Existing Tags:\n", 0);
	if (symbols)
	{
	    xrcsnode = finfo->rcs;
	    (void) walklist (symbols, tag_list_proc, NULL);
	}
	else
	    cvs_output ("\tNo Tags Exist\n", 0);
    }
    cvs_output ("\n", 0);
	}

    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype status_dirproc (void *callerdat, char *dir, char *repos, char *update_dir, List *entries, const char *virtual_repository, Dtype hint)
{
	if(hint!=R_PROCESS)
		return hint;

    if (!quiet)
	error (0, 0, "Examining %s", update_dir);
    return (R_PROCESS);
}

/*
 * Print out a tag and its type
 */
static int tag_list_proc (Node *p, void *closure)
{
    char *branch = NULL;
    char *buf;

    if (RCS_nodeisbranch (xrcsnode, p->key))
	branch = RCS_whatbranch(xrcsnode, p->key) ;

    buf = (char*)xmalloc (80 + strlen (p->key)
		   + (branch ? strlen (branch) : strlen (p->data)));
    sprintf (buf, "\t%-25s\t(%s: %s)\n", p->key,
	     branch ? "branch" : "revision",
	     branch ? branch : p->data);
    cvs_output (buf, 0);
    xfree (buf);

    if (branch)
	xfree (branch);

    return (0);
}
