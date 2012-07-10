/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Check In
 * 
 * Does a very careful checkin of the file "user", and tries not to spoil its
 * modification time (to avoid needless recompilations). When RCS ID keywords
 * get expanded on checkout, however, the modification time is updated and
 * there is no good way to get around this.
 * 
 * Returns non-zero on error.
 */

#include "cvs.h"
#include "fileattr.h"
#include "edit.h"

int Checkin (int type, struct file_info *finfo, char *rcs, char *rev, char *tag,
    char *options, char *message, const char *merge_from_tag1, const char *merge_from_tag2,
	RCSCHECKINPROC callback, const char *bugid, const char *edit_revision, const char *edit_tag,
	const char *edit_bugid)
{
    Vers_TS *vers;
    int set_time;
	kflag kf;

    /* Hmm.  This message goes to stdout and the "foo,v  <--  foo"
       message from "ci" goes to stderr.  This doesn't make a whole
       lot of sense, but making everything go to stdout can only be
       gracefully achieved once RCS_checkin is librarified.  */

	cvs_output ("Checking in ", 0);
    cvs_output (fn_root(finfo->fullname), 0);
    cvs_output (";\n", 0);

    if (finfo->rcs == NULL)
		finfo->rcs = RCS_parse (finfo->mapped_file, finfo->repository);

	/* Migrate out of the attic */
	RCS_setattic(finfo->rcs);

	if(tag && !isdigit(tag[0]) && RCS_isfloating(finfo->rcs, tag))
	{
		char *cp;
		/* Checking into a magic branch */
		/* Change it into a 'normal' branch before the commit */
		char *branch_head = RCS_getbranch(finfo->rcs,rev,0);
		rev = RCS_magicrev(finfo->rcs, branch_head);
		if(!rev)
			error(1,0,"Internal error: RCS_magicrev failed.  Cannot checkin.");
		RCS_settag(finfo->rcs,tag,rev,NULL);
		xfree(branch_head);
		cp = strrchr(rev,'.');
		for(--cp;*cp!='.';--cp)
			;
		strcpy(cp,cp+2);
	}

    CXmlNodePtr node;
    switch (RCS_checkin (finfo->rcs, finfo->file, message, rev, options, RCS_FLAGS_KEEPFILE, merge_from_tag1, merge_from_tag2, callback, NULL, bugid, &variable_list))
    {
	case 0:			/* everything normal */
		TRACE(3,"The checkin succeeded.");
	    /* The checkin succeeded.  If checking the file out again
               would not cause any changes, we are done.  Otherwise,
               we need to check out the file, which will change the
               modification time of the file.

	       The only way checking out the file could cause any
	       changes is if the file contains RCS keywords.  So we if
	       we are not expanding RCS keywords, we are done.  */

	    /* FIXME: If PreservePermissions is on, RCS_cmp_file is
               going to call RCS_checkout into a temporary file
               anyhow.  In that case, it would be more efficient to
               call RCS_checkout here, compare the resulting files
               using xcmp, and rename if necessary.  I think this
               should be fixed in RCS_cmp_file.  */
		TRACE(3,"RCS_get_kflags called in Checkin");
		RCS_get_kflags(options, true, kf);
		TRACE(3,"RCS_cmp_file called in Checkin");
		if(callback || (kf.flags&(KFLAG_PRESERVE|KFLAG_BINARY))
			|| RCS_cmp_file (finfo->rcs, rev, options, finfo->file, 0) == 0)
	    {
		/* The existing file is correct.  We don't have to do
                   anything.  */
		set_time = 0;
	    }
	    else
	    {
		/* The existing file is incorrect.  We need to check
                   out the correct file contents.  */
		if (RCS_checkout (finfo->rcs, finfo->file, rev, (char *) NULL,
				  options, RUN_TTY, (RCSCHECKOUTPROC) NULL,
				  (void *) NULL, NULL) != 0)
		    error (1, 0, "failed when checking out new copy of %s",
			   fn_root(finfo->fullname));
		xchmod (finfo->file, 1);
		set_time = 1;
	    }

	    /*
	     * If we want read-only files, muck the permissions here, before
	     * getting the file time-stamp.
	     */
	    node = fileattr_getroot();
	    node->xpathVariable("file",finfo->file);
	    if (!(commit_keep_edits && edit_revision) && (!cvswrite || (node->Lookup("file[cvs:filename(@name,$file)]/watched") && node->XPathResultNext()) || (kf.flags&KFLAG_RESERVED_EDIT)))
			xchmod (finfo->file, 0);

	    /* Re-register with the new data.  */
	    vers = Version_TS (finfo, NULL, tag, NULL, 1, set_time, 0);
	    history_write (type, NULL, vers->vn_rcs,
			   finfo->file, finfo->repository,bugid,message);

		{
			CMD5Calc *md5 = NULL;
#ifdef SERVER_SUPPORT
			if(server_active)
			{
				char buf[BUFSIZ*10];
				FILE *cf = CVS_FOPEN(finfo->file,"r");
				if(!cf)
				{
					error(1,errno,"Unable to reopen %s for checksum");
				}
				CVS_FSEEK(cf,0,SEEK_END);
				if(CVS_FTELL(cf)>=server_checksum_threshold)
				{
					size_t l;

					CVS_FSEEK(cf,0,SEEK_SET);
					md5 = new CMD5Calc;
					while((l=fread(buf,1,sizeof(buf),cf))>0)
						md5->Update(buf,l);
				}
				fclose(cf);
			}
#endif
			Register (finfo->entries, finfo->file, vers->vn_rcs, vers->ts_user,
				vers->options, vers->tag, vers->date, (char *) 0, NULL, NULL, vers->tt_rcs, edit_revision, edit_tag, edit_bugid, md5?md5->Final():NULL); 

			if(md5)
				delete md5;
		}
	    break;

	case -1:			/* fork failed */
	    if (!noexec)
		error (1, errno, "could not check in %s -- fork failed",
		       fn_root(finfo->fullname));
	    return (1);

	default:			/* ci failed */

	    /* The checkin failed, for some unknown reason, so we
	       print an error, and return an error.  We assume that
	       the original file has not been touched.  */
	    if (!noexec)
		error (0, 0, "could not check in %s", fn_root(finfo->fullname));
	    return (1);
    }

    /*
     * When checking in a specific revision, we may have locked the wrong
     * branch, so to be sure, we do an extra unlock here before
     * returning.
     */
    if (rev)
    {
		RCS_unlock (finfo->rcs, NULL, 1);
		RCS_rewrite (finfo->rcs, NULL, NULL, 0);
    }

#ifdef SERVER_SUPPORT
    if (server_active)
    {
	if (set_time)
	    /* Need to update the checked out file on the client side.  */
	    server_updated (finfo, vers, SERVER_UPDATED,
			    (mode_t) -1, NULL,
			    (struct buffer *) NULL);
	else
	    server_checked_in (finfo->file, finfo->update_dir, finfo->repository);
    }
    else
#endif
	mark_up_to_date (finfo->file);

    freevers_ts (&vers);
    return (0);
}
