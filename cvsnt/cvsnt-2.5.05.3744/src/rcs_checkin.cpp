/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 * The routines contained in this file do all the rcs file parsing and
 * manipulation
 */

#include "rcs.cpp"
/* Move RCS out of the Attic.  If the
   file is already in the desired place, return without doing
   anything.  At some point may want to think about how this relates
   to RCS_rewrite but that is a bit hairy (if one wants renames to be
   atomic, or that kind of thing).  If there is an error, print a message
   and return 1.  On success, return 0.  */
int RCS_setattic (RCSNode *rcs)
{
    char *newpath;
    char *p;
    char *q;

    /* Could make the pathname computations in this file, and probably
       in other parts of rcs.c too, easier if the REPOS and FILE
       arguments to RCS_parse got stashed in the RCSNode.  */

	if (!(rcs->flags & INATTIC))
		return 0;

	newpath = (char*)xmalloc (strlen (rcs->path));

	/* Example: rcs->path is "/foo/bar/Attic/baz,v".  */
	p = (char*)last_component (rcs->path);
	strncpy (newpath, rcs->path, p - rcs->path - 1);
	newpath[p - rcs->path - 1] = '\0';
	q = newpath + (p - rcs->path - 1) - (sizeof CVSATTIC - 1);
	assert (strncmp (q, CVSATTIC, sizeof CVSATTIC - 1) == 0);
	strcpy (q, p);

	rcsbuf_close (&rcs->rcsbuf);
	if (CVS_RENAME (rcs->path, newpath) < 0)
	{
		error (0, errno, "failed to move `%s' out of the attic", fn_root(rcs->path));
		xfree (newpath);
		return 1;
	}

	p=(char*)last_component(rcs->path);
	p[-1]='\0';
	if (isemptydir (rcs->path, 0))
	{
		if (unlink_file_dir (rcs->path) < 0 && !existence_error (errno))
			TRACE(3,"Couldn't remove empty attic directory - error %d",errno); 
	}
	p[-1]='/';

    xfree (rcs->path);
    rcs->path = newpath;
	xfree (rcs->rcsbuf.filename);
	rcs->rcsbuf.filename = xstrdup(newpath);
	free_rcsnode_contents(rcs);
	RCS_reparsercsfile(rcs);

    return 0;
}


/* Check out a revision from an RCS file.

   If PFN is not NULL, then ignore WORKFILE and SOUT.  Call PFN zero
   or more times with the contents of the file.  CALLERDAT is passed,
   uninterpreted, to PFN.  (The current code will always call PFN
   exactly once for a non empty file; however, the current code
   assumes that it can hold the entire file contents in memory, which
   is not a good assumption, and might change in the future).

   Otherwise, if WORKFILE is not NULL, check out the revision to
   WORKFILE.  However, if WORKFILE is not NULL, and noexec is set,
   then don't do anything.

   Otherwise, if WORKFILE is NULL, check out the revision to SOUT.  If
   SOUT is RUN_TTY, then write the contents of the revision to
   standard output.  When using SOUT, the output is generally a
   temporary file; don't bother to get the file modes correct.

   REV is the numeric revision to check out.  It may be NULL, which
   means to check out the head of the default branch.

   If NAMETAG is not NULL, and is not a numeric revision, then it is
   the tag that should be used when expanding the RCS Name keyword.

   OPTIONS is a string such as "-kb" or "-kv" for keyword expansion
   options.  It may be NULL to use the default expansion mode of the
   file, typically "-kkv".

   On an error which prevented checking out the file, either print a
   nonfatal error and return 1, or give a fatal error.  On success,
   return 0.  */

/* This function mimics the behavior of `rcs co' almost exactly.  The
   chief difference is in its support for preserving file ownership,
   permissions, and special files across checkin and checkout -- see
   comments in RCS_checkin for some issues about this. -twp */

int RCS_checkout (RCSNode *rcs, const char *workfile, const char *rev, const char *nametag, const char *options,
     const char *sout, RCSCHECKOUTPROC pfn, void *callerdat, mode_t *pmode, bool *is_ref)
{
  TRACE(1,"RCS_checkout (%s, %s, %s, %s)",
  		PATCH_NULL(fn_root(rcs->path)),
  		rev != NULL ? rev : "",
  		options != NULL ? options : "",
  		(pfn != NULL ? "(function)"
  		 : (workfile != NULL
  		    ? workfile
  		    : (sout != RUN_TTY ? sout : "(stdout)"))));

  if (noexec && workfile != NULL)
  	return 0;

  assert (sout == RUN_TTY || workfile == NULL);
  assert (pfn == NULL || (sout == RUN_TTY && workfile == NULL));
  kflag expand;
  char *value = nullptr; size_t len = 0;
  int free_value = 0;
  char *log = nullptr;
  size_t loglen = 0;
  int free_rev =0;
  if (!RCS_checkout_raw_value (rcs, rev, free_rev, nametag, options, expand, value, free_value, len, log, loglen))
    return 0;

  FILE *fp = nullptr, *ofp = nullptr;
  if(expand.flags&KFLAG_BINARY_DELTA)
  {
    if (!RCS_read_binary_rev_data(&value, &len, &free_value, expand.flags&KFLAG_COMPRESS_DELTA, is_ref))
    {
      rcsbuf_valfree(&rcs->rcsbuf, &log);
      if (free_rev)
        xfree (rev);
      return 0;
    }
  }

  /* Handle permissions */
  mode_t rcs_mode;
  Node *vp = NULL;
  {
  	RCSVers *vers;
  	Node *info;

  	vp = findnode (rcs->versions, rev == NULL ? rcs->head : rev);
  	if (vp == NULL)
  		error (1, 0, "internal error: no revision information for %s",
  		rev == NULL ? rcs->head : rev);
  	vers = (RCSVers *) vp->data;

  	info = findnode (vers->other_delta, "permissions");
  	if (info != NULL)
  	{
  		rcs_mode = (mode_t) strtoul (info->data, NULL, 8) & 0777;
  		TRACE(3,"got rcs_mode = %04o from rcs-permissions-tag",rcs_mode);
  	}
  	else
  	{
    #ifndef _WIN32
      struct stat sb;
      if( rcs->path && (CVS_LSTAT (rcs->path, &sb) >= 0))
      {
      	rcs_mode =  (mode_t)(sb.st_mode & 0777);
      	TRACE(3,"got rcs_mode = %04o from file permissions <%s>",rcs_mode, rcs->path);
      } 
      else
    #endif
      {
      	rcs_mode = 0644;
      	TRACE(3,"using default rcs_mode = %04o <%s>",rcs_mode, rcs->path);
      }
  	}
  }

    if (!(expand.flags&KFLAG_PRESERVE))
    {
		char *newvalue;

		/* Don't fetch the delta node again if we already have it. */
		if (vp == NULL)
		{
			vp = findnode (rcs->versions, rev == NULL ? rcs->head : rev);
			if (vp == NULL)
			error (1, 0, "internal error: no revision information for %s",
				rev == NULL ? rcs->head : rev);
		}

		expand_keywords (rcs, (RCSVers *) vp->data, nametag, log, loglen,
				expand, value, len, &newvalue, &len);

		if (newvalue != value)
		{
			if (free_value)
				xfree (value);
			value = newvalue;
			free_value = 1;
		}
    }

	rcsbuf_valfree(&rcs->rcsbuf,&log);

	LineType crlf=crlf_mode;
	if(expand.flags&KFLAG_MAC)
		crlf=ltCr;
	else if(expand.flags&KFLAG_DOS)
		crlf=ltCrLf;
	else if(expand.flags&(KFLAG_BINARY|KFLAG_UNIX))
		crlf=ltLf;

	bool oopen_binary = ((expand.flags&KFLAG_BINARY) || (expand.flags&KFLAG_UNIX));
	bool oencode = (((expand.flags&KFLAG_ENCODED) || (crlf!=CRLF_DEFAULT)) && !server_active);
	bool nopen_binary = ((expand.flags&KFLAG_BINARY) || (crlf!=CRLF_DEFAULT) || ((expand.flags&(KFLAG_BINARY|KFLAG_ENCODED) && (!server_active))));
	bool nencode = !(expand.flags&KFLAG_BINARY) && (((expand.flags&KFLAG_ENCODED) || (crlf!=CRLF_DEFAULT)) && !server_active);
	bool xopen_binary = (expand.flags&(KFLAG_BINARY|KFLAG_ENCODED)) || (crlf!=CRLF_DEFAULT) && !server_active;
	bool xencode = !(expand.flags&KFLAG_BINARY) && ((expand.flags&KFLAG_ENCODED) || (crlf!=CRLF_DEFAULT)) && !server_active;
	bool encode=oencode;
	bool open_binary=xopen_binary;

	if (free_rev)
		xfree (rev);
    if (pfn != NULL)
    {
		/* The PFN interface is very simple to implement right now, as
			we always have the entire file in memory.  */
		pfn(callerdat, len?value:"", len);
    }
    else
    {
	/* Not a special file: write to WORKFILE or SOUT. */
	if (workfile == NULL)
	{
	    if (sout == RUN_TTY)
		ofp = stdout;
	    else
	    {
		/* Symbolic links should be removed before replacement, so that
		   `fopen' doesn't follow the link and open the wrong file. */
		if (islink (sout))
		    if (unlink_file (sout) < 0)
			error (1, errno, "cannot remove %s", sout);
		ofp = CVS_FOPEN (sout, open_binary ? "wb" : "w");
		if (ofp == NULL)
		    error (1, errno, "cannot open %s", sout);
	    }
	}
	else
	{
	    /* Output is supposed to go to WORKFILE, so we should open that
	       file.  Symbolic links should be removed first (see above). */
	    if (islink (workfile))
		if (unlink_file (workfile) < 0)
		    error (1, errno, "cannot remove %s", workfile);

		ofp = CVS_FOPEN (workfile, open_binary ? "wb" : "w");

	    /* If the open failed because the existing workfile was not
	       writable, try to chmod the file and retry the open.  */
	    if (ofp == NULL && errno == EACCES
		&& isfile (workfile) && !iswritable (workfile))
	    {
		xchmod (workfile, 1);
		ofp = CVS_FOPEN (workfile, open_binary ? "wb" : "w");
	    }

	    if (ofp == NULL)
	    {
		error (0, errno, "cannot open %s", workfile);
		if (free_value)
		    xfree (value);
		return 1;
	    }
	}
		
	if (workfile == NULL && sout == RUN_TTY)
	{
	    if (expand.flags&KFLAG_BINARY)
			cvs_output_binary (value, len);
	    else
	    {
			/* cvs_output requires the caller to check for zero
			length.  */
			if (len > 0)
			{
#ifdef SERVER_SUPPORT
				cvs_no_translate_begin();
#endif
				if(!server_active)
				{
					cvs_output(value,len);
				}
				else
				{
					char *p;
					p=(char*)memchr(value,'\0',len);
					if(!p)
					{
					  if(free_value)
					    value=(char*)xrealloc(value,len+1);
					  else
					  {
					    p=(char*)xmalloc(len+1);
					    memcpy(p,value,len);
					    value=p;
					    free_value=1;
					  }
					  value[len]='\0'; 
					}	
					p=strrchr(value,'\n');
					if(!p)
						cvs_output_tagged("text",value);
					else if(!*(p+1))
						cvs_output(value,len);
					else
					{
						/* If the file doesn't end in '\n' then send a special 'MT text' output */
						cvs_output(value,(p-value)+1);
						cvs_output_tagged("text",p+1);
					}
				}
#ifdef SERVER_SUPPORT
				cvs_no_translate_end();
#endif
			}
	    }
	}
	else if(encode) // In server mode we never use unicode
	{
		CCodepage cdp;

		TRACE(3,"checkout -> workfile != NULL || sout != RUN_TTY && encode");
		TRACE(3,"checkout -> encode %s",PATCH_NULL(expand.encoding.encoding));
		cdp.BeginEncoding(CCodepage::NullEncoding,expand.encoding);
		if(cdp.OutputAsEncoded(fileno(ofp),value,len,crlf))
		{
		    error (0, errno, "cannot write %s",
			   (workfile != NULL
			    ? workfile
			    : (sout != RUN_TTY ? sout : "stdout")));
		    if (free_value)
				xfree (value);
			cdp.EndEncoding();
		    return 1;
		}
		cdp.EndEncoding();
	}
	else
	{
		TRACE(3,"checkout -> workfile != NULL || sout != RUN_TTY && !encode");
	    /* NT 4.0 is said to have trouble writing 2099999 bytes
	       (for example) in a single fwrite.  So break it down
	       (there is no need to be writing that much at once
	       anyway; it is possible that LARGEST_FWRITE should be
	       somewhat larger for good performance, but for testing I
	       want to start with a small value until/unless a bigger
	       one proves useful).  */
#define LARGEST_FWRITE 102400
	    size_t nleft = len;
	    size_t nstep = (len < LARGEST_FWRITE ? len : LARGEST_FWRITE);
	    char *p = value;

	    while (nleft > 0)
	    {
		if (fwrite (p, 1, nstep, ofp) != nstep)
		{
		    error (0, errno, "cannot write %s",
			   (workfile != NULL
			    ? workfile
			    : (sout != RUN_TTY ? sout : "stdout")));
		    if (free_value)
				xfree (value);
		    return 1;
		}
		p += nstep;
		nleft -= nstep;
		if (nleft < nstep)
		    nstep = nleft;
	    }
	}
    }

    if (free_value)
		xfree (value);

	// Must remove write here as rest of code relies on it
	rcs_mode = modify_mode(rcs_mode,0,S_IWUSR|S_IWGRP|S_IWOTH);

    if(pmode) 
      *pmode = rcs_mode;

    if (workfile != NULL)
    {
		int ret;

		if (fclose (ofp) < 0)
		{
			error (0, errno, "cannot close %s", workfile);
			return 1;
		}
	
		ret = chmod (workfile, rcs_mode);
		if (ret < 0)
		{
		error (0, errno, "cannot change mode of file %s",
		fn_root(workfile));
		}
    }
    else if (sout != RUN_TTY)
    {
		if (fclose (ofp) < 0)
		{
			error (0, errno, "cannot close %s", sout);
			return 1;
		}
    }

	/* We only trace the first 256 chars */
	if(trace>1 && sout!=RUN_TTY)
	{
		int len, bom;
		char buf[256], encoding[256];
		FILE *f;

		f=fopen(sout,"r");
		if(f==NULL)
			TRACE(1,"checkout didn't generate a file... something wierd happened");
		else
		{
			len = fread(buf,255,1,f);
			fclose(f);
			buf[len]='\0';
			strcpy(encoding,"No Encoding");

			bom=0;
			// Check for encoding header
			if(buf[0]==(char)0xff && buf[1]==(char)0xfe)
			{
				strcpy(encoding,"UCS-2LE");
				bom=1;
			}

			// Byteswap encoding header
			if(buf[0]==(char)0xfe && buf[1]==(char)0xff)
			{
				strcpy(encoding,"UCS-2BE");
				bom=1;
			}

			if (bom)
				TRACE(1,"checkout -> BOM %s \"%s\" (len=%d)",PATCH_NULL(encoding),PATCH_NULL(&buf[2]),len);
			else
				TRACE(1,"checkout -> \"%s\" (len=%d)",PATCH_NULL(buf),len);
		}
	}
	TRACE(3,"RCS_checkout -> return ok");
    return 0;
}

/* Check in to RCSFILE with revision REV (which must be greater than
   the largest revision) and message MESSAGE (which is checked for
   legality).  If FLAGS & RCS_FLAGS_DEAD, check in a dead revision.
   If FLAGS & RCS_FLAGS_QUIET, tell ci to be quiet.  If FLAGS &
   RCS_FLAGS_MODTIME, use the working file's modification time for the
   checkin time.  WORKFILE is the working file to check in from, or
   NULL to use the usual RCS rules for deriving it from the RCSFILE.
   If FLAGS & RCS_FLAGS_KEEPFILE, don't unlink the working file;
   unlinking the working file is standard RCS behavior, but is rarely
   appropriate for CVS.

   This function should almost exactly mimic the behavior of `rcs ci'.  The
   principal point of difference is the support here for preserving file
   ownership and permissions in the delta nodes.  This is not a clean
   solution -- precisely because it diverges from RCS's behavior -- but
   it doesn't seem feasible to do this anywhere else in the code. [-twp]

   Return value is -1 for error (and errno is set to indicate the
   error), positive for error (and an error message has been printed),
   or zero for success.  */

static const size_t blobrefdifflen = blob_reference_size + strlen("d1 1\na1 1\n");
int RCS_checkin (RCSNode *rcs, const char *workfile, const char *message, const char *rev, const char *options, int flags,
			 const char *merge_from_tag1, const char *merge_from_tag2, RCSCHECKINPROC callback, char **pnewversion, const char *bugid, variable_list_t *props)
{
	bool pnewversiondone=false;
    RCSVers *delta, *commitpt;
    Deltatext *dtext;
    Node *nodep;
    char *tmpfile, *changefile, *chtext;
    const char *diffopts;
    int diff_res;
    size_t bufsize;
    int buflen, chtextlen;
    int status, checkin_quiet, allocated_workfile;
    struct tm *ftm;
    time_t modtime;
    int adding_branch = 0;
    struct stat sb;
	Node *np;
	char *workfilename;
	char buf[64];	/* static buffer should be safe: see usage. -twp */
	kflag kf;
	const char *deltat;
	kflag kf_empty,kf_binary;
	size_t lockId_temp;
	int ret;

    if (options)
      while (char*s = strstr(const_cast<char*>(options), "b"))//on checkin do not allow old binary files
        *s = 'B';
	RCS_get_kflags(options, true, kf);

	if(!options)
		TRACE(1,"No expansion specified in Checkin - assuming -kkv");

	kf_binary.flags=KFLAG_BINARY;

    commitpt = NULL;

    /* Get basename of working file.  Is there a library function to
       do this?  I couldn't find one. -twp */
    allocated_workfile = 0;
	workfilename = NULL;
    if (workfile == NULL)
    {
		if(callback)
		{
			char *text;
			size_t len;
			FILE *fp;

			workfile = cvs_temp_name();
			if(callback(workfile, &text, &len, &workfilename))
				error(1,0,"Checkin callback failed");
			fp = CVS_FOPEN(workfile,"wb");
			if(!fp)
				error(1,errno,"Couldn't create working file");
			if(len)
			{
				if(fwrite(text,1,len,fp)!=len)
					error(1,errno,"Couldn't write to working file");
			}
			fclose(fp);
			xfree(text);
		}
		else
		{
			char *p;
			int extlen = strlen (RCSEXT);
			workfile = xstrdup (last_component (rcs->path));
			p = ((char*)workfile) + (strlen (workfile) - extlen);
			assert (strncmp (p, RCSEXT, extlen) == 0);
			*p = '\0';
		}
		allocated_workfile = 1;
    }

    /* If the filename is a symbolic link, follow it and replace it
       with the destination of the link.  We need to do this before
       calling rcs_internal_lockfile, or else we won't put the lock in
       the right place. */
    resolve_symlink (&(rcs->path));

    checkin_quiet = flags & RCS_FLAGS_QUIET;
    if (!checkin_quiet)
    {
    cvs_output (fn_root(rcs->path), 0);
	cvs_output ("  <--  ", 7);
	cvs_output (workfilename?workfilename:workfile, 0);
	cvs_output ("\n", 1);
    }

    /* Create new delta node. */
    delta = (RCSVers *) xmalloc (sizeof (RCSVers));
    memset (delta, 0, sizeof (RCSVers));
    delta->author = xstrdup(getcaller ());
	/* Deltatype is text unless specified otherwise */
	if(kf.flags&KFLAG_BINARY_DELTA)
		delta->type = (char*)"text";
	else
		delta->type = (char*)((kf.flags&KFLAG_COMPRESS_DELTA)?"compressed_text":"text");
    if (flags & RCS_FLAGS_MODTIME)
    {
	struct stat ws;
	if (stat (workfile, &ws) < 0)
	{
	    error (1, errno, "cannot stat %s", workfile);
	}
	modtime = ws.st_mtime;
    }
    else
	(void) time (&modtime);
    ftm = gmtime (&modtime);
    delta->date = (char *) xmalloc (MAXDATELEN);
    (void) sprintf (delta->date, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
    if (flags & RCS_FLAGS_DEAD)
    {
	delta->state = RCSDEAD;
	delta->dead = 1;
    }
    else
	delta->state = "Exp";

	delta->other_delta = getlist();

    /*  save the permission info. */
	if(callback)
	{
		strcpy(buf,"0644");
	}
	else
	{
		if (CVS_LSTAT (workfile, &sb) < 0)
			error (1, errno, "cannot lstat %s", workfile);

		(void) sprintf (buf, "%o", (int)(sb.st_mode & 0777));
	}
	np = getnode();
	np->type = RCSFIELD;
	np->key = xstrdup ("permissions");
	np->data = xstrdup (buf);
	addnode (delta->other_delta, np);

	/* save the commit ID */
	np = getnode();
	np->type = RCSFIELD;
	np->key = xstrdup ("commitid");
	np->data = xstrdup(global_session_id);
	addnode (delta->other_delta, np);

	/* save the keyword expansion mode */
	delta->kopt = xstrdup(options);

	/* Record the mergepoints */
	/* We didn't used to record mergepoints unless they were single ones - I think this behaviour
	   is/was incorrect */
	if(merge_from_tag1 && merge_from_tag1[0])
	{
		np = getnode();
		np->type = RCSFIELD;
		np->key = xstrdup ("mergepoint1");
		if(merge_from_tag2 && merge_from_tag2[0]) /* For a two revision merge, record the second target */
			np->data = xstrdup(merge_from_tag2); 
		else
			np->data = xstrdup(merge_from_tag1);
		addnode (delta->other_delta, np);
	}

	if(bugid && bugid[0])
	{
		np = getnode();
		np->type = RCSFIELD;
		np->key = xstrdup ("bugid");
			np->data = xstrdup(bugid);
		addnode (delta->other_delta, np);
	}

	/* save the current filename */
	np = getnode();
	np->type = RCSFIELD;
	np->key = xstrdup ("filename");
	np->data = xstrdup(workfile);
	addnode (delta->other_delta, np);

	if(props && props->size())
	{
		delta->properties = getlist();
		for(variable_list_t::const_iterator i = props->begin(); i!=props->end(); ++i)
		{
			np = getnode();
			np->type = RCSFIELD;
			np->key = xstrdup(i->first.c_str());
			np->data = xstrdup(i->second.c_str());
			addnode(delta->properties,np);
		}
	}

	/* Create a new deltatext node. */
    dtext = (Deltatext *) xmalloc (sizeof (Deltatext));
    memset (dtext, 0, sizeof (Deltatext));

    dtext->log = make_message_rcslegal (message);

	cvs::string dir = Short_Repository(rcs->path);
	const char *p = strrchr(dir.c_str(),'/');
	if(p)
		dir.resize(p-dir.c_str());
	else
		dir="";

    /* Derive a new revision number.  From the `ci' man page:

	 "If rev  is  a revision number, it must be higher than the
	 latest one on the branch to which  rev  belongs,  or  must
	 start a new branch.

	 If  rev is a branch rather than a revision number, the new
	 revision is appended to that branch.  The level number  is
	 obtained  by  incrementing the tip revision number of that
	 branch.  If rev  indicates  a  non-existing  branch,  that
	 branch  is  created  with  the  initial  revision numbered
	 rev.1."

       RCS_findlock_or_tip handles the case where REV is omitted.
       RCS 5.7 also permits REV to be "$" or to begin with a dot, but
       we do not address those cases -- every routine that calls
       RCS_checkin passes it a numeric revision. */

    if (rev == NULL || *rev == '\0')
    {
	/* Figure out where the commit point is by looking for locks.
	   If the commit point is at the tip of a branch (or is the
	   head of the delta tree), then increment its revision number
	   to obtain the new revnum.  Otherwise, start a new
	   branch. */
	commitpt = RCS_findlock_or_tip (rcs);
	if (commitpt == NULL)
	{
		if(rcs->head)
		{
			status = 1;
			goto checkin_done;
		}
		else
			delta->version = xstrdup("1.1");
	}
	else if (commitpt->next == NULL || STREQ (commitpt->version, rcs->head) || (kf.flags&KFLAG_SINGLE))
	    delta->version = increment_revnum (commitpt->version);
	else
	    delta->version = RCS_addbranch (rcs, commitpt->version);
    }
    else
    {
	/* REV is either a revision number or a branch number.  Find the
	   tip of the target branch. */
	char *branch, *tip, *newrev, *p;
	int dots, isrevnum;

	assert (isdigit ((unsigned char) *rev));

	dots = numdots (rev);

	if((kf.flags&KFLAG_SINGLE))
	{
		newrev=xstrdup(rcs->branch);
		if(!newrev) newrev=xstrdup("1");
		dots=0;
	}
	else
		newrev = xstrdup (rev);

	isrevnum = dots & 1;


	branch = xstrdup (newrev);
	if (isrevnum)
	{
	    p = strrchr (branch, '.');
	    *p = '\0';
	}

	/* Find the tip of the target branch.  If we got a one- or two-digit
	   revision number, this will be the head of the tree.  Exception:
	   if rev is a single-field revision equal to the branch number of
	   the trunk (usually "1") then we want to treat it like an ordinary
	   branch revision. */
	if (dots == 0)
	{
	    tip = xstrdup (rcs->head);
		if(!tip)
			tip=xstrdup("1");
	    if (atoi (tip) != atoi (branch))
	    {
			newrev = (char *) xrealloc (newrev, strlen (newrev) + 3);
			strcat (newrev, ".1");
			dots = isrevnum = 1;
	    }
	}
	else if (dots == 1)
	    tip = xstrdup (rcs->head);
	else
	    tip = RCS_getbranch (rcs, branch, 1);

	/* If the branch does not exist, and we were supplied an exact
	   revision number, signal an error.  Otherwise, if we were
	   given only a branch number, create it and set COMMITPT to
	   the branch point. */
	if (tip == NULL)
	{
	    if (isrevnum)
	    {
		error (0, 0, "%s: can't find branch point %s",
		       fn_root(rcs->path), branch);
		xfree (branch);
		xfree (newrev);
		status = 1;
		goto checkin_done;
	    }
	    delta->version = RCS_addbranch (rcs, branch);
	    if (!delta->version)
	    {
		xfree (branch);
		xfree (newrev);
		status = 1;
		goto checkin_done;
	    }
	    adding_branch = 1;
	    p = strrchr (branch, '.');
	    *p = '\0';
	    tip = xstrdup (branch);
	}
	else
	{
	    if (isrevnum)
	    {
		/* NEWREV must be higher than TIP. */
		if (compare_revnums (tip, newrev) >= 0)
		{
		    error (0, 0,
			   "%s: revision %s too low; must be higher than %s",
			   fn_root(rcs->path),
			   newrev, tip);
		    xfree (branch);
		    xfree (newrev);
		    xfree (tip);
		    status = 1;
		    goto checkin_done;
		}
		delta->version = xstrdup (newrev);
	    }
	    else
		{
			if(rcs->head)
			{
				/* Just increment the tip number to get the new revision. */
				delta->version = increment_revnum (tip);
			}
			else
				delta->version = xstrdup("1.1");
		}
	}

	nodep = findnode (rcs->versions, tip);
	if(!nodep && rcs->head)
	{
		error(0,0,"Branch tip missing.  Internal consistency error.");
		status = 1;
		goto checkin_done;
	}
	commitpt = (RCSVers *) (nodep?nodep->data:NULL);

	xfree (branch);
	xfree (newrev);
	xfree (tip);
    }

    /* If COMMITPT is locked by us, break the lock.  If it's locked
       by someone else, signal an error. */
	nodep = commitpt?findnode (RCS_getlocks (rcs), commitpt->version):NULL;
    if (nodep != NULL)
    {
	if (! STREQ (nodep->data, delta->author))
	{
	    /* If we are adding a branch, then leave the old lock around.
	       That is sensible in the sense that when adding a branch,
	       we don't need to use the lock to tell us where to check
	       in.  It is fishy in the sense that if it is our own lock,
	       we break it.  However, this is the RCS 5.7 behavior (at
	       the end of addbranch in ci.c in RCS 5.7, it calls
	       removelock only if it is our own lock, not someone
	       else's).  */

	    if (!adding_branch)
	    {
		error (0, 0, "%s: revision %s locked by %s",
		       fn_root(rcs->path),
		       nodep->key, nodep->data);
		status = 1;
		goto checkin_done;
	    }
	}
	else
	    delnode (nodep);
    }

	dtext->version = xstrdup (delta->version);

	/* If the delta tree is empty, then there's nothing to link the
       new delta into.  So make a new delta tree, snarf the working
       file contents, and just write the new RCS file. */
    if ((kf.flags&KFLAG_SINGLE) || rcs->head == NULL)
    {
		FILE *fout;
		RCSVers *vers = NULL;

		rcs->head = xstrdup(delta->version?delta->version:"1.1");
		rcs->free_head=true;
		nodep = getnode();
		nodep->type = RCSVERS;
		nodep->delproc = rcsvers_delproc;
		nodep->data = (char *) delta;
		nodep->key = xstrdup(rcs->head);
		(void) addnode (rcs->versions, nodep);

		bufsize = 0;
		get_file (workfile, workfile,
			(kf.flags&KFLAG_BINARY) ? "rb" : "r",
			&dtext->text, &bufsize, &dtext->len, kf);

		if (!checkin_quiet)
		{
			if(commitpt)
			{
				cvs_output ("new revision: ", 14);
				cvs_output (delta->version, 0);
				cvs_output ("; previous revision: ", 21);
				cvs_output (commitpt->version, 0);
				cvs_output ("\n", 1);
			}
			else
			{
				cvs_output ("initial revision: ", 0);
				cvs_output (rcs->head, 0);
				cvs_output ("\n", 1);
			}
		}
		if(pnewversion)
		{
			if (!pnewversiondone)
			{
			*pnewversion=xstrdup((commitpt)?delta->version:rcs->head);
			pnewversiondone=true;
			}
		}

        if(kf.flags&KFLAG_BINARY_DELTA)
        {
          RCS_write_binary_rev_data(rcs->path, dtext->text, dtext->len, kf.flags&KFLAG_COMPRESS_DELTA, true);
          bufsize = dtext->len+1;
        }

		fout = rcs_internal_lockfile (rcs->path, &lockId_temp);
		RCS_putadmin (rcs, fout);
		RCS_putdtree (rcs, rcs->head, fout);
		RCS_putdesc (rcs, fout);
		rcs->delta_pos = CVS_FTELL (fout);
		if (rcs->delta_pos == -1)
			error (1, errno, "cannot ftell for %s", fn_root(rcs->path));
		putdeltatext (fout, dtext, (kf.flags & (KFLAG_BINARY_DELTA|KFLAG_COMPRESS_DELTA)) == KFLAG_COMPRESS_DELTA);

		if (commitpt != NULL && commitpt->text != NULL)
		{
			freedeltatext (commitpt->text);
			commitpt->text = NULL;
		}
		commitpt = NULL;

		rcsbuf_close(&rcs->rcsbuf);
		if(atomic_checkouts)
			do_modified(lockId_temp,dtext->version,"","HEAD",commitpt?'A':'M');
		rcs_internal_unlockfile(fout, rcs->path, lockId_temp);
		free_rcsnode_contents(rcs);
		RCS_reparsercsfile(rcs);

		if ((flags & RCS_FLAGS_KEEPFILE) == 0)
		{
			if (unlink_file (workfile) < 0)
			/* FIXME-update-dir: message does not include update_dir.  */
			error (0, errno, "cannot remove %s", workfile);
		}

		if (!checkin_quiet)
			cvs_output ("done\n", 5);

		status = 0;
		goto checkin_done;
    }

    /* Obtain the change text for the new delta.  If DELTA is to be the
       new head of the tree, then its change text should be the contents
       of the working file, and LEAFNODE's change text should be a diff.
       Else, DELTA's change text should be a diff between LEAFNODE and
       the working file. */

    tmpfile = cvs_temp_name();
    if(kf.flags&KFLAG_BINARY_DELTA)
    {
      bool is_ref = false;
      status = RCS_checkout (rcs, NULL, commitpt->version, NULL, "B",
			   tmpfile,
			   (RCSCHECKOUTPROC)0, NULL, NULL, &is_ref);
    }
    else
      status = RCS_checkout (rcs, NULL, commitpt->version, NULL,
			   (kf.flags&KFLAG_BINARY)
			    ? "b"//should not be happening
			    : "o",
			   tmpfile,
			   (RCSCHECKOUTPROC)0, NULL, NULL);

    if (status != 0)
	error (1, 0,
	       "could not check out revision %s of `%s'",
	       commitpt->version, fn_root(rcs->path));

    bufsize = buflen = 0;
    chtext = NULL;
    chtextlen = 0;
    changefile = cvs_temp_name();

    /* Diff options should include --binary if the RCS file has -kb set
       in its `expand' field. */
	if(kf.flags&KFLAG_BINARY)
		diffopts = "-a -n --binary-input --binary-output";
	else if(kf.flags&KFLAG_ENCODED && !server_active)
	{
		static char __diffopts[64] = "-a -n --binary-output --encoding=";
		strcat(__diffopts,kf.encoding.encoding);
		if(kf.encoding.bom)
			strcat(__diffopts," --bom");
		diffopts = __diffopts;
	}
	else
		diffopts = "-a -n --binary-output";

	rcsdiff_param_t rcsdiff_args;

	if (STREQ (commitpt->version, rcs->head) && numdots (delta->version) == 1)
    {
		TRACE(2,"Insert delta at head (%s,%s)",PATCH_NULL(commitpt->version),PATCH_NULL(delta->version));

		/* If this revision is being inserted on the trunk, the change text
		for the new delta should be the contents of the working file ... */
		bufsize = 0;
		get_file (workfile, workfile,
			(kf.flags&KFLAG_BINARY) ? "rb" : "r",
			&dtext->text, &bufsize, &dtext->len, kf);

		/* ... and the change text for the old delta should be a diff. */
		commitpt->text = (Deltatext *) xmalloc (sizeof (Deltatext));
		memset (commitpt->text, 0, sizeof (Deltatext));

		if(kf.flags&KFLAG_BINARY_DELTA)
		{
          if (!delta->dead || !is_blob_reference_data(dtext->text, dtext->len))
          {
            RCS_write_binary_rev_data(rcs->path, dtext->text, dtext->len, kf.flags&KFLAG_COMPRESS_DELTA, !delta->dead);
            bufsize = dtext->len+1;
          }
          char *tmp_work_file = cvs_temp_name();
          if (!write_file_line(tmp_work_file, dtext->text, dtext->len))
            error(1,errno,"cant write workfile %s", tmp_work_file);
          diff_res = diff_exec (tmp_work_file, tmpfile, NULL, NULL, diffopts, changefile);
          unlink_file(tmp_work_file);
          xfree(tmp_work_file);
		} else
          diff_res = diff_exec (workfile, tmpfile, NULL, NULL, diffopts, changefile);
      	bufsize = 0;

      	switch (diff_res)
      	{
      		case 0:
      		case 1:
      		break;
      		case -1:
      		/* FIXME-update-dir: message does not include update_dir.  */
      		error (1, errno, "error diffing %s", workfile);
      		break;
      		default:
      		/* FIXME-update-dir: message does not include update_dir.  */
      		error (1, 0, "error diffing %s", workfile);
      		break;
      	}

      	/* Diff will have produced output in RCS format (lf),
      	   so we read it as binary */

      	get_file (changefile, changefile,
      			"rb", 
      			&commitpt->text->text, &bufsize, &commitpt->text->len, kf_binary);


        if (commitpt->text->text && (kf.flags&KFLAG_BINARY_DELTA) && commitpt->text->len != 0 && commitpt->text->len != blobrefdifflen && commitpt->text->len != blob_reference_size)//for initial/head revision it is blake3:hash, for others it is d1 1\na1 1\nblake3:hash
        {
          error(1,0, "internal error, kB HEAD change has invalid length of %d, should be %d or %d", (int)commitpt->text->len, (int)blobrefdifflen, (int)blob_reference_size);
        }
		{

			calc_add_remove(rcs,commitpt->text->text, commitpt->text->len, rcsdiff_args.removed, rcsdiff_args.added);

			/* If COMMITPT->TEXT->TEXT is NULL, it means that CHANGEFILE
			was empty and that there are no differences between revisions.
			In that event, we want to force RCS_rewrite to write an empty
			string for COMMITPT's change text.  Leaving the change text
			field set NULL won't work, since that means "preserve the original
			change text for this delta." */
			if (commitpt->text->text == NULL)
			{
				commitpt->text->text = xstrdup ("");
				commitpt->text->len = 0;
			}

			/* Patch up the existing delta as it is converted to text if not already set */
            if(kf.flags&KFLAG_BINARY_DELTA)
              deltat = "text";
            else
              deltat = (kf.flags&KFLAG_COMPRESS_DELTA)?"compressed_text":"text";
			if(!commitpt->type || strcmp(commitpt->type,deltat))
			{
				/* Patch up the existing delta as it is converted to binary if not already set */
				if(commitpt->type_alloc)
					xfree(commitpt->type);
				else
					rcsbuf_valfree(&rcs->rcsbuf,&commitpt->type);
				commitpt->type = (char*)deltat;
				commitpt->type_alloc=false;
			}
		}

		rcsdiff_args.file = xstrdup(workfile);
		rcsdiff_args.directory = dir.c_str();
		rcsdiff_args.oldfile = tmpfile;
		rcsdiff_args.newfile = workfile;
		rcsdiff_args.type = commitpt->type;
		rcsdiff_args.options = options;
		rcsdiff_args.oldversion = commitpt->version;
		rcsdiff_args.newversion = dtext->version;

		if((kf.flags & (KFLAG_BINARY_DELTA|KFLAG_COMPRESS_DELTA)) == KFLAG_COMPRESS_DELTA)
		{
			/* We need to compress the delta here, because it won't be compressed by RCS_rewrite */
			uLong zlen;
			void *zbuf;

			z_stream stream = {0};
			deflateInit(&stream,Z_DEFAULT_COMPRESSION);
			zlen = deflateBound(&stream, commitpt->text->len);
			stream.avail_in = commitpt->text->len;
			stream.next_in = (Bytef*)commitpt->text->text;
			stream.avail_out = zlen;
			zbuf = xmalloc(zlen+4);
			stream.next_out = (Bytef*)((char*)zbuf)+4;
			*(unsigned long *)zbuf=htonl(commitpt->text->len);
			if(deflate(&stream, Z_FINISH)!=Z_STREAM_END)
			{
				error(1,0,"internal error: deflate failed");
			}
			deflateEnd(&stream);
			xfree(commitpt->text->text);
			commitpt->text->text = (char*)zbuf;
			commitpt->text->len = zlen+4;
		}
    }
    else
    {
		TRACE(2,"Insert delta at branch (%s,%s)",PATCH_NULL(commitpt->version),PATCH_NULL(delta->version));

		/* This file is not being inserted at the head, but on a side
		branch somewhere.  Make a diff from the previous revision
		to the working file. */

		if(kf.flags&KFLAG_BINARY_DELTA)
		{
          if (!delta->dead)
        		get_file (workfile, workfile, "rb", &dtext->text, &bufsize, &dtext->len, kf);
          if (!delta->dead || !is_blob_reference_data(dtext->text, dtext->len))
            RCS_write_binary_rev_data(rcs->path, dtext->text, dtext->len, kf.flags&KFLAG_COMPRESS_DELTA, !delta->dead);
          bufsize = dtext->len+1;
          char *tmp_work_file = cvs_temp_name();
          if (!write_file_line(tmp_work_file, dtext->text, dtext->len))
            error(1,errno,"cant write workfile %s", tmp_work_file);

          diff_res = diff_exec (tmpfile, tmp_work_file, NULL, NULL, diffopts, changefile);
          unlink_file(tmp_work_file);
          xfree(tmp_work_file);
          //diff_res = diff_exec (workfile, tmpfile, NULL, NULL, diffopts, changefile);
		} else
          diff_res = diff_exec (tmpfile, workfile, NULL, NULL, diffopts, changefile);

		{
			switch (diff_res)
			{
				case 0:
				case 1:
				break;
				case -1:
				/* FIXME-update-dir: message does not include update_dir.  */
				error (1, errno, "error diffing %s", workfile);
				break;
				default:
				/* FIXME-update-dir: message does not include update_dir.  */
				error (1, 0, "error diffing %s", workfile);
				break;
			}
			/* See the comment above, at the other get_file invocation,
				regarding binary vs. text.  */
				get_file (changefile, changefile,
					"rb",
					&dtext->text, &bufsize,
					&dtext->len, kf_binary);

                if (dtext->text && (kf.flags&KFLAG_BINARY_DELTA) && dtext->len != 0 && dtext->len != blobrefdifflen && dtext->len != blob_reference_size)//for initial/head revision it is blake3:hash, for others it is d1 1\na1 1\nblake3:hash
                {
                  error(1,0, "internal error, kB branch change has invalid length of %d, should be %d or %d", (int)dtext->len, (int)blobrefdifflen, (int)blob_reference_size);
                }
				calc_add_remove(rcs,dtext->text, dtext->len, rcsdiff_args.added, rcsdiff_args.removed);
		}
		if (dtext->text == NULL)
		{
			dtext->text = xstrdup ("");
			dtext->len = 0;
		}
    }

	rcsdiff_args.file = xstrdup(workfile);
	rcsdiff_args.directory = dir.c_str();
	rcsdiff_args.oldfile = tmpfile;
	rcsdiff_args.newfile = workfile;
	rcsdiff_args.type = commitpt->type;
	rcsdiff_args.options = options;
	rcsdiff_args.oldversion = dtext->version;
	rcsdiff_args.newversion = commitpt->version;

	TRACE(3,"run prercsdiff trigger");
	ret  = run_trigger(&rcsdiff_args,prercsdiff_proc);
	if(!ret || (kf.flags&KFLAG_BINARY_DELTA))
		diffopts = NULL;
	else
	{
		// Unified diff
		if(kf.flags&KFLAG_BINARY)
			diffopts = "-a -u --binary-input --binary-output";
		else if(kf.flags&KFLAG_ENCODED && !server_active)
		{
			static char __diffopts[64] = "-a -u --binary-output --encoding=";
			strcat(__diffopts,kf.encoding.encoding);
			if(kf.encoding.bom)
				strcat(__diffopts," --bom");
			diffopts = __diffopts;
		}
		else
			diffopts = "-a -u --binary-output";
	}

	if(diffopts)
	{
		cvs::string fn;
		const char *pathpos=NULL;
		if (rcsdiff_args.file)
			pathpos=strpbrk(rcsdiff_args.file,"\\/");
		if (rcsdiff_args.file && pathpos)
			cvs::sprintf(fn,80,"%s",rcsdiff_args.file);
		else
			cvs::sprintf(fn,80,"%s/%s",rcsdiff_args.directory,rcsdiff_args.file);

		const char *label1 = make_file_label(fn.c_str(),rcsdiff_args.oldversion,rcs,0);
		const char *label2 = make_file_label(fn.c_str(),rcsdiff_args.newversion,rcs,0);
		switch (diff_exec (rcsdiff_args.oldfile, rcsdiff_args.newfile, label1, label2, diffopts, changefile))
		{
			case 0:
			case 1:
				break;
			case -1:
				/* FIXME-update-dir: message does not include update_dir.  */
				error (0, errno, "error rediffing %s", workfile);
				break;
			default:
				/* FIXME-update-dir: message does not include update_dir.  */
				error (0, 0, "error rediffing %s", workfile);
			break;
		}
		xfree(label1);
		xfree(label2);
		rcsdiff_args.diff = NULL;
		bufsize = 0;
		get_file (changefile, changefile,
			"rb",
			(char **)&rcsdiff_args.diff, &bufsize,
			&rcsdiff_args.difflen, kf_binary);

		TRACE(3,"run rcsdiff trigger");
		int ret  = run_trigger(&rcsdiff_args,rcsdiff_proc);
		if(ret)
			error(1,0,"Error running rcsdiff trigger function");
		xfree(rcsdiff_args.diff);
	}

	xfree(rcsdiff_args.file);

    /* Update DELTA linkage.  It is important not to do this before
       the very end of RCS_checkin; if an error arises that forces
       us to abort checking in, we must not have malformed deltas
       partially linked into the tree.

       If DELTA and COMMITPT are on different branches, do nothing --
       DELTA is linked to the tree through COMMITPT->BRANCHES, and we
       don't want to change `next' pointers.

       Otherwise, if the nodes are both on the trunk, link DELTA to
       COMMITPT; otherwise, link COMMITPT to DELTA. */

    if (numdots (commitpt->version) == numdots (delta->version))
    {
	if (STREQ (commitpt->version, rcs->head))
	{
		delta->next_alloc=true;
	    delta->next = xstrdup(rcs->head);
		if(rcs->free_head)
			xfree(rcs->free_head);
		else
			rcsbuf_valfree(&rcs->rcsbuf,&rcs->head);
	    rcs->head = xstrdup (delta->version);
		rcs->free_head=true;
	}
	else
	{
		commitpt->next_alloc=true;
	    commitpt->next = xstrdup (delta->version);
	}
    }

    /* Add DELTA to RCS->VERSIONS. */
    if (rcs->versions == NULL)
	rcs->versions = getlist();
    nodep = getnode();
    nodep->type = RCSVERS;
    nodep->delproc = rcsvers_delproc;
    nodep->data = (char *) delta;
    nodep->key = delta->version;
    (void) addnode (rcs->versions, nodep);

    /* Write the new RCS file, inserting the new delta at COMMITPT. */
    if (!checkin_quiet)
    {
		cvs_output ("new revision: ", 14);
		cvs_output (delta->version, 0);
		cvs_output ("; previous revision: ", 21);
		cvs_output (commitpt->version, 0);
		cvs_output ("\n", 1);
    }
	if(pnewversion)
	{
		if (!pnewversiondone)
		{
		*pnewversion=xstrdup(delta->version);
		pnewversiondone=true;
		}
	}

    RCS_rewrite (rcs, dtext, commitpt->version, (kf.flags & (KFLAG_BINARY_DELTA|KFLAG_COMPRESS_DELTA)) == KFLAG_COMPRESS_DELTA);
	commitpt=NULL; /* rewrite frees the memory associated with commitpt */

    if ((flags & RCS_FLAGS_KEEPFILE) == 0)
    {
		if (unlink_file (workfile) < 0)
			/* FIXME-update-dir: message does not include update_dir.  */
			error (1, errno, "cannot remove %s", workfile);
    }
    if (unlink_file (tmpfile) < 0)
		error (0, errno, "cannot remove %s", tmpfile);
    xfree (tmpfile);
    if (unlink_file (changefile) < 0)
		error (0, errno, "cannot remove %s", changefile);
    xfree (changefile);

    if (!checkin_quiet)
	cvs_output ("done\n", 5);

 checkin_done:
    if (allocated_workfile)
	{
		if(callback)
			CVS_UNLINK(workfile);
		xfree (workfile);
	}
	xfree(workfilename);

    if (commitpt != NULL && commitpt->text != NULL)
    {
		freedeltatext (commitpt->text);
		commitpt->text = NULL;
    }

    freedeltatext (dtext);

	if(pnewversion)
	{
		if (!pnewversiondone)
		{
		*pnewversion=xstrdup(delta->version);
		pnewversiondone=true;
		}
	}

    if (status != 0)
		free_rcsvers_contents (delta);

    return status;
}

/* This structure is passed between RCS_cmp_file and cmp_file_buffer.  */

struct cmp_file_data
{
    const char *filename;
    FILE *fp;
    int different;
	int ignore_keywords;
	kflag expand;
	RCSNode *rcs;
	RCSVers *ver;
};

/* Compare the contents of revision REV of RCS file RCS with the
   contents of the file FILENAME.  OPTIONS is a string for the keyword
   expansion options.  Return 0 if the contents of the revision are
   the same as the contents of the file, 1 if they are different.  */

int RCS_cmp_file (RCSNode *rcs, const char *rev, const char *options, const char *filename, int ignore_keywords)
{
    int binary;
    FILE *fp;
    struct cmp_file_data data;
    int retcode;
    kflag expand;
    static char _opt[256];
    const char *exp;

    exp = RCS_getexpand (rcs, rev);
    RCS_get_kflags(options?options:exp, false, expand);
    xfree(exp);
    binary = expand.flags&KFLAG_BINARY;
    if(!server_active && expand.flags&KFLAG_ENCODED) binary=1;

    // Effectively do a -k-v in the expansion
    if(ignore_keywords&!(expand.flags&KFLAG_BINARY))
    {
        expand.flags&=~(KFLAG_VALUE|KFLAG_VALUE_LOGONLY);
        options = RCS_rebuild_options(&expand,_opt);
    }

    if ((expand.flags&KFLAG_BINARY_DELTA))
    {
      kflag expand2;
      char *value = nullptr; size_t len = 0;
      int free_value = 0;
      char *log = nullptr;
      size_t loglen = 0;
      int free_rev =0;
      int checkOutSuccess = RCS_checkout_raw_value (rcs, rev, free_rev, NULL, options, expand2, value, free_value, len, log, loglen);
      if (!checkOutSuccess || !(expand2.flags&KFLAG_BINARY_DELTA))
      {
        if (free_rev)
          xfree(rev);
        if (free_value)
          xfree(value);
        return 1;
      }
      char *data_path;
      char *rev_file_name = get_binary_blob_ver_file_path(data_path, rcs, value, len);
      char hash_encoded[hash_encoded_size+1];hash_encoded[hash_encoded_size] = 0;
      bool had_errors = false;
      if (!is_blob_reference_data(value, len))
      //if (!get_blob_reference_hash(rev_file_name, hash_encoded))//hash_encoded==char[65], encoded 32 bytes + \0
      {
        //todo: should not be needed as soon as all converted. Everything is a reference
        if (!caddressed_fs::get_file_content_hash(rev_file_name, hash_encoded, sizeof(hash_encoded)))
        {
          error(0,0, "can't read file revision <%s> to compare sha", rev_file_name);
          had_errors = true;
        }
      } else
        memcpy(hash_encoded, value + hash_type_magic_len, hash_encoded_size);
      xfree(data_path);

      char hash_encoded_sent[hash_encoded_size+1];hash_encoded_sent[hash_encoded_size] = 0;
      if (!get_session_blob_reference_hash(filename, hash_encoded_sent))//hash_encoded==char[65], encoded 32 bytes + \0
      {
        if (!caddressed_fs::get_file_content_hash(filename, hash_encoded_sent, sizeof(hash_encoded_sent)))
        {
          error(0,0, "can't read file <%s> to compare sha", filename);
          had_errors = true;
        }
      }
      if (free_rev)
        xfree(rev);
      if (free_value)
        xfree(value);
      TRACE(3,"RCS_cmp_file() for KB returns %d || %d", had_errors, memcmp(hash_encoded, hash_encoded_sent, hash_encoded_size) != 0);
      return had_errors || memcmp(hash_encoded, hash_encoded_sent, hash_encoded_size) != 0;
    }

    Node *n = NULL;
    if(rev)
      n = findnode(rcs->versions,rev);
    else
    {
      rev = RCS_head(rcs);
      if(rev)
      	n = findnode(rcs->versions,rev);
      xfree(rev);
    }

    {
		char *dir=xgetwd();
        fp = CVS_FOPEN (filename, binary ? FOPEN_BINARY_READ : "r");
        if (fp == NULL)
        {


			if (strcmp(filename,RCSREPOVERSION)==0)
			{
				return 1;
			}

            /* FIXME-update-dir: should include update_dir in message.  */
            error (1, errno, "cannot open file %s for comparing in %s", filename, PATCH_NULL(dir));
        }

        data.filename = filename;
        data.fp = fp;
        data.different = 0;
		data.expand = expand;
		data.ignore_keywords = ignore_keywords;
		data.rcs = rcs;
		data.ver = n?(RCSVers*)n->data:NULL;
        int64_t src_data_sz = 0;
        if (expand.flags&KFLAG_BINARY_DELTA)
        {
          fseek(fp,0,SEEK_END);
          src_data_sz = ftell(fp);
          fseek(fp,0,SEEK_SET);
        }

        retcode = RCS_checkout (rcs, (char *) NULL, rev, (char *) NULL,
				options, RUN_TTY, cmp_file_buffer,
				(void *) &data, NULL);

        if (src_data_sz < 0)
        {
          fclose (fp);
          return 1;
        }

        /* If we have not yet found a difference, make sure that we are at
           the end of the file.  */
        if (! data.different)
        {
	    if (getc (fp) != EOF)
		data.different = 1;
        }

        fclose (fp);

	if (retcode != 0)
	    return 1;

        return data.different;
    }
}

/* This is a subroutine of RCS_cmp_file.  It is passed to
   RCS_checkout.  */
#define MAX_CMP_BUF 5*1024*1024 // 5MB

static void cmp_file_buffer (void *callerdat, const char *buffer, size_t len)
{
	int memcmpres2=0;
    struct cmp_file_data *data = (struct cmp_file_data *) callerdat;
    char *filebuf;
	char *expbuf = NULL;
	void *unibuf = NULL;
	int unicode=0;
	size_t filelen,unilen,unimax,explen;
	CCodepage *cdp;

    /* If we've already found a difference, we don't need to check
       further.  */
    if (data->different)
	{
		return;
	}

	fseek(data->fp,0,SEEK_END);
	filelen = ftell(data->fp);
	fseek(data->fp,0,SEEK_SET);

	// Fast path.. comparing (apparently) identical files.
	// Have to take into account CRLF conversion through fread also so filelen != the
	// final length of the file...
	if((data->expand.flags&KFLAG_BINARY) || (!data->ignore_keywords && !(data->expand.flags&KFLAG_ENCODED)))
	{
		size_t filebuflen = filelen>MAX_CMP_BUF?MAX_CMP_BUF:filelen+1;
		filebuf=(char*)xmalloc(filebuflen+1);
		size_t l=0;
		int memcmpres1=0;
		do
		{
			l=fread(filebuf,1,filebuflen,data->fp);
			memcmpres1=0;
			if(l<0 || (l<filebuflen && l!=len) ||  l>len || ((memcmpres1=memcmp(buffer,filebuf,l)!=0)))
			{
				if (ferror (data->fp))
				{
					error (1, errno, "cannot read file %s for comparing", data->filename);
				}
				data->different = 1;
				xfree (filebuf);
				return;
			}
			buffer+=l;
			len-=l;
		} while(len>0);
		xfree(filebuf);
		return;
	}

	if((!server_active) && (data->expand.flags&KFLAG_ENCODED))
	{
		unicode=1;
		cdp = new CCodepage;
		cdp->BeginEncoding(data->expand.encoding,CCodepage::Utf8Encoding);
	}

	if(unicode)
	{
		/* Fixup the file length to cope with the unicode->utf8 translation */
		unimax = filelen*3+1;
		unibuf = (char*)xmalloc (unimax);
	}
	else
	{
		unibuf = NULL;
	}

	filebuf = (char*)xmalloc (filelen+1);

	size_t checklen, file2len;
	char *checkbuf;

	checklen = filelen;
	if ((file2len = fread (filebuf, 1, checklen, data->fp))<1)
	{
		if (ferror (data->fp))
		error (1, errno, "cannot read file %s for comparing",
			data->filename);
		data->different = 1;
		xfree (filebuf);
		xfree (unibuf);
		xfree (expbuf);
		return;
	}
	filebuf[file2len]='\0';

	checkbuf = filebuf;
	checklen = file2len;

	if(unicode)
	{
		unilen = unimax;
		int res;
		if(unicode==1 && (res=cdp->ConvertEncoding(filebuf,checklen,unibuf,unilen))>0)
		{

			cdp->StripCrLf(unibuf,unilen);
			checkbuf = (char*)unibuf;
			checklen = unilen;
		}

		if(res<0)
			error(0,0,"Unable to convert from %s to UTF-8",data->expand.encoding.encoding);
	}

	if(data->ignore_keywords)
	{
		kflag ef = data->expand;
		ef.flags&=~(KFLAG_VALUE|KFLAG_VALUE_LOGONLY);
		expand_keywords(data->rcs,data->ver,"","",0,ef,checkbuf,checklen,&expbuf,&explen);
		checkbuf = expbuf;
		checklen = explen;
	}

	memcmpres2=0;
	if (len!=checklen || ((memcmpres2=memcmp (checkbuf, buffer, checklen)) != 0))
	{
		data->different = 1;
		xfree (filebuf);
		xfree (unibuf);
		xfree (expbuf);
		return;
	}

    xfree (filebuf);
	xfree (unibuf);
	xfree (expbuf);

	if(unicode)
	{
		cdp->EndEncoding();
		delete cdp;
	}
}
/* Delete revisions between REV1 and REV2.  The changes between the two
   revisions must be collapsed, and the result stored in the revision
   immediately preceding the lower one.  Return 0 for successful completion,
   1 otherwise.

   Solution: check out the revision preceding REV1 and the revision
   following REV2.  Use call_diff to find aggregate diffs between
   these two revisions, and replace the delta text for the latter one
   with the new aggregate diff.  Alternatively, we could write a
   function that takes two change texts and combines them to produce a
   new change text, without checking out any revs or calling diff.  It
   would be hairy, but so, so cool.

   If INCLUSIVE is set, then TAG1 and TAG2, if non-NULL, tell us to
   delete that revision as well (cvs admin -o tag1:tag2).  If clear,
   delete up to but not including that revision (cvs admin -o tag1::tag2).
   This does not affect TAG1 or TAG2 being NULL; the meaning of the start
   point in ::tag2 and :tag2 is the same and likewise for end points.  */

int RCS_delete_revs (RCSNode *rcs, char *tag1, char *tag2, int inclusive)
{
    char *next;
    Node *nodep;
    RCSVers *revp = NULL;
    RCSVers *beforep;
    int status, found;
    int save_noexec;

    char *branchpoint = NULL;
    char *rev1 = NULL;
    char *rev2 = NULL;
    int rev1_inclusive = inclusive;
    int rev2_inclusive = inclusive;
    char *before = NULL;
    char *after = NULL;
    char *beforefile = NULL;
    char *afterfile = NULL;
    char *outfile = NULL;
	kflag kf;
	
    if (tag1 == NULL && tag2 == NULL)
		return 0;

    /* Assume error status until everything is finished. */
    status = 1;

    /* Make sure both revisions exist. */
    if (tag1 != NULL)
    {
	rev1 = RCS_gettag (rcs, tag1, 1, NULL);
	if (rev1 == NULL || (nodep = findnode (rcs->versions, rev1)) == NULL)
	{
	    error (0, 0, "%s: Revision %s doesn't exist.", fn_root(rcs->path), tag1);
	    goto delrev_done;
	}
    }
    if (tag2 != NULL)
    {
	rev2 = RCS_gettag (rcs, tag2, 1, NULL);
	if (rev2 == NULL || (nodep = findnode (rcs->versions, rev2)) == NULL)
	{
	    error (0, 0, "%s: Revision %s doesn't exist.", fn_root(rcs->path), tag2);
	    goto delrev_done;
	}
    }

    /* If rev1 is on the trunk and rev2 is NULL, rev2 should be
       RCS->HEAD.  (*Not* RCS_head(rcs), which may return rcs->branch
       instead.)  We need to check this special case early, in order
       to make sure that rev1 and rev2 get ordered correctly. */
    if (rev2 == NULL && numdots (rev1) == 1)
    {
	rev2 = xstrdup (rcs->head);
	rev2_inclusive = 1;
    }

    if (rev2 == NULL)
	rev2_inclusive = 1;

    if (rev1 != NULL && rev2 != NULL)
    {
	/* A range consisting of a branch number means the latest revision
	   on that branch. */
	if (RCS_isbranch (rcs, rev1) && STREQ (rev1, rev2))
	    rev1 = rev2 = RCS_getbranch (rcs, rev1, 0);
	else
	{
	    /* Make sure REV1 and REV2 are ordered correctly (in the
	       same order as the next field).  For revisions on the
	       trunk, REV1 should be higher than REV2; for branches,
	       REV1 should be lower.  */
	    /* Shouldn't we just be giving an error in the case where
	       the user specifies the revisions in the wrong order
	       (that is, always swap on the trunk, never swap on a
	       branch, in the non-error cases)?  It is not at all
	       clear to me that users who specify -o 1.4:1.2 really
	       meant to type -o 1.2:1.4, and the out of order usage
	       has never been documented, either by cvs.texinfo or
	       rcs(1).  */
	    char *temp;
	    int temp_inclusive;
	    if (numdots (rev1) == 1)
	    {
		if (compare_revnums (rev1, rev2) <= 0)
		{
		    temp = rev2;
		    rev2 = rev1;
		    rev1 = temp;

		    temp_inclusive = rev2_inclusive;
		    rev2_inclusive = rev1_inclusive;
		    rev1_inclusive = temp_inclusive;
		}
	    }
	    else if (compare_revnums (rev1, rev2) > 0)
	    {
		temp = rev2;
		rev2 = rev1;
		rev1 = temp;

		temp_inclusive = rev2_inclusive;
		rev2_inclusive = rev1_inclusive;
		rev1_inclusive = temp_inclusive;
	    }
	}
    }

    /* Basically the same thing; make sure that the ordering is what we
       need.  */
    if (rev1 == NULL)
    {
	assert (rev2 != NULL);
	if (numdots (rev2) == 1)
	{
	    /* Swap rev1 and rev2.  */
	    int temp_inclusive;

	    rev1 = rev2;
	    rev2 = NULL;

	    temp_inclusive = rev2_inclusive;
	    rev2_inclusive = rev1_inclusive;
	    rev1_inclusive = temp_inclusive;
	}
    }

    /* Put the revision number preceding the first one to delete into
       BEFORE (where "preceding" means according to the next field).
       If the first revision to delete is the first revision on its
       branch (e.g. 1.3.2.1), BEFORE should be the node on the trunk
       at which the branch is rooted.  If the first revision to delete
       is the head revision of the trunk, set BEFORE to NULL.

       Note that because BEFORE may not be on the same branch as REV1,
       it is not very handy for navigating the revision tree.  It's
       most useful just for checking out the revision preceding REV1. */
    before = NULL;
    branchpoint = RCS_getbranchpoint (rcs, rev1 != NULL ? rev1 : rev2);
    if (rev1 == NULL)
    {
	rev1 = xstrdup (branchpoint);
	if (numdots (branchpoint) > 1)
	{
	    char *bp;
	    bp = strrchr (branchpoint, '.');
	    while (*--bp != '.')
		;
	    *bp = '\0';
	    /* Note that this is exclusive, always, because the inclusive
	       flag doesn't affect the meaning when rev1 == NULL.  */
	    before = xstrdup (branchpoint);
	    *bp = '.';
	}
    }
    else if (! STREQ (rev1, branchpoint))
    {
	/* Walk deltas from BRANCHPOINT on, looking for REV1. */
	nodep = findnode (rcs->versions, branchpoint);
	revp = (RCSVers *) nodep->data;
	while (revp->next != NULL && ! STREQ (revp->next, rev1))
	{
	    revp = (RCSVers *) nodep->data;
	    nodep = findnode (rcs->versions, revp->next);
	}
	if (revp->next == NULL)
	{
	    error (0, 0, "%s: Revision %s doesn't exist.", fn_root(rcs->path), rev1);
	    goto delrev_done;
	}
	if (rev1_inclusive)
	    before = xstrdup (revp->version);
	else
	{
	    before = rev1;
	    nodep = findnode (rcs->versions, before);
	    rev1 = xstrdup (((RCSVers *)nodep->data)->next);
	}
    }
    else if (!rev1_inclusive)
    {
	before = rev1;
	nodep = findnode (rcs->versions, before);
	rev1 = xstrdup (((RCSVers *)nodep->data)->next);
    }
    else if (numdots (branchpoint) > 1)
    {
	/* Example: rev1 is "1.3.2.1", branchpoint is "1.3.2.1".
	   Set before to "1.3".  */
	char *bp;
	bp = strrchr (branchpoint, '.');
	while (*--bp != '.')
	    ;
	*bp = '\0';
	before = xstrdup (branchpoint);
	*bp = '.';
    }

    /* If any revision between REV1 and REV2 is locked or is a branch point,
       we can't delete that revision and must abort. */
    after = NULL;
    next = rev1;
    found = 0;
    while (!found && next != NULL)
    {
	nodep = findnode (rcs->versions, next);
	revp = (RCSVers *) nodep->data;

	if (rev2 != NULL)
	    found = STREQ (revp->version, rev2);
	next = revp->next;

	if ((!found && next != NULL) || rev2_inclusive || rev2 == NULL)
	{
	    if (findnode (RCS_getlocks (rcs), revp->version))
	    {
			error (0, 0, "%s: can't remove locked revision %s",
				fn_root(rcs->path),
				revp->version);
			goto delrev_done;
	    }
	    if (revp->branches != NULL)
	    {
			error (0, 0, "%s: can't remove branch point %s",
				fn_root(rcs->path),
				revp->version);
			goto delrev_done;
	    }
		const char *exp = RCS_getexpand(rcs,revp->version);
		RCS_get_kflags(exp,false,kf);
		xfree(exp);
		if((kf.flags&(KFLAG_COMPRESS_DELTA|KFLAG_BINARY_DELTA)) == KFLAG_COMPRESS_DELTA)
		{
			error (0, 0, "%s: can't remove compressed delta %s",
				fn_root(rcs->path),
				revp->version);
			goto delrev_done;
	    }

	    /* Doing this only for the :: syntax is for compatibility.
	       See cvs.texinfo for somewhat more discussion.  */
	    if (walklist (RCS_symbols (rcs), findtag, revp->version))
	    {
			/* We don't print which file this happens to on the theory
			that the caller will print the name of the file in a
			more useful fashion (fullname not rcs->path).  */
			error (0, 0, "cannot remove revision %s because it has tags",
				revp->version);
			goto delrev_done;
	    }

	    /* It's misleading to print the `deleting revision' output
	       here, since we may not actually delete these revisions.
	       But that's how RCS does it.  Bleah.  Someday this should be
	       moved to the point where the revs are actually marked for
	       deletion. -twp */
	    cvs_output ("deleting revision ", 0);
	    cvs_output (revp->version, 0);
	    cvs_output ("\n", 1);

  		if(kf.flags&KFLAG_BINARY_DELTA)
  		{
        //
  		}
	}
    }

    if (rev2 == NULL)
	;
    else if (found)
    {
	if (rev2_inclusive)
	    after = xstrdup (next);
	else
	    after = xstrdup (revp->version);
    }
    else if (!inclusive)
    {
	/* In the case of an empty range, for example 1.2::1.2 or
	   1.2::1.3, we want to just do nothing.  */
	status = 0;
	goto delrev_done;
    }
    else
    {
	/* This looks fishy in the cases where tag1 == NULL or tag2 == NULL.
	   Are those cases really impossible?  */
	assert (tag1 != NULL);
	assert (tag2 != NULL);

	error (0, 0, "%s: invalid revision range %s:%s", fn_root(rcs->path),
	       tag1, tag2);
	goto delrev_done;
    }

    if (after == NULL && before == NULL)
    {
	/* The user is trying to delete all revisions.  While an
	   RCS file without revisions makes sense to RCS (e.g. the
	   state after "rcs -i"), CVS has never been able to cope with
	   it.  So at least for now we just make this an error.

	   We don't include rcs->path in the message since "cvs admin"
	   already printed "RCS file:" and the name.  */
	error (1, 0, "attempt to delete all revisions");
    }

    /* The conditionals at this point get really hairy.  Here is the
       general idea:

       IF before != NULL and after == NULL
         THEN don't check out any revisions, just delete them
       IF before == NULL and after != NULL
         THEN only check out after's revision, and use it for the new deltatext
       ELSE
         check out both revisions and diff -n them.  This could use
	 RCS_exec_rcsdiff with some changes, like being able
	 to suppress diagnostic messages and to direct output. */

    if (after != NULL)
    {
	char *diffbuf;
	size_t bufsize, len;

	afterfile = cvs_temp_name();
	status = RCS_checkout (rcs, NULL, after, NULL, "b", afterfile,
			       (RCSCHECKOUTPROC)0, NULL, NULL);
	if (status > 0)
	    goto delrev_done;

	if (before == NULL)
	{
	    /* We are deleting revisions from the head of the tree,
	       so must create a new head. */
	    diffbuf = NULL;
	    bufsize = 0;
		get_file (afterfile, afterfile, "rb", &diffbuf, &bufsize, &len, kf);

	    save_noexec = noexec;
	    noexec = 0;
	    if (unlink_file (afterfile) < 0)
		error (0, errno, "cannot remove %s", afterfile);
	    noexec = save_noexec;

	    xfree (afterfile);
	    afterfile = NULL;

		if(rcs->free_head)
			xfree (rcs->head);
		else
			rcsbuf_valfree(&rcs->rcsbuf,&rcs->head);
	    rcs->head = xstrdup (after);
		rcs->free_head=true;
	}
	else
	{
	    beforefile = cvs_temp_name();
	    status = RCS_checkout (rcs, NULL, before, NULL, "b", beforefile,
				   (RCSCHECKOUTPROC)0, NULL, NULL);
	    if (status > 0)
		goto delrev_done;

	    outfile = cvs_temp_name();
	    status = diff_exec (beforefile, afterfile, NULL, NULL, "-a -n --binary-input --binary-output", outfile);

	    if (status == 2)
	    {
		/* Not sure we need this message; will diff_exec already
		   have printed an error?  */
		error (0, 0, "%s: could not diff", fn_root(rcs->path));
		status = 1;
		goto delrev_done;
	    }

	    diffbuf = NULL;
	    bufsize = 0;
		kflag kf_binary;
		kf_binary.flags=KFLAG_BINARY;
		get_file (outfile, outfile, "rb", &diffbuf, &bufsize, &len, kf_binary);
	}

	/* Save the new change text in after's delta node. */
	nodep = findnode (rcs->versions, after);
	revp = (RCSVers *) nodep->data;

	assert (revp->text == NULL);

	revp->text = (Deltatext *) xmalloc (sizeof (Deltatext));
	memset ((Deltatext *) revp->text, 0, sizeof (Deltatext));
	revp->text->version = xstrdup (revp->version);
	revp->text->text = diffbuf;
	revp->text->len = len;

	/* If DIFFBUF is NULL, it means that OUTFILE is empty and that
	   there are no differences between the two revisions.  In that
	   case, we want to force RCS_copydeltas to write an empty string
	   for the new change text (leaving the text field set NULL
	   means "preserve the original change text for this delta," so
	   we don't want that). */
	if (revp->text->text == NULL)
	    revp->text->text = xstrdup ("");
    }

    /* Walk through the revisions (again) to mark each one as
       outdated.  (FIXME: would it be safe to use the `dead' field for
       this?  Doubtful.) */
    for (next = rev1;
	 next != NULL && (after == NULL || ! STREQ (next, after));
	 next = revp->next)
    {
	nodep = findnode (rcs->versions, next);
	revp = (RCSVers *) nodep->data;
	revp->outdated = 1;
    }

    /* Update delta links.  If BEFORE == NULL, we're changing the
       head of the tree and don't need to update any `next' links. */
    if (before != NULL)
    {
	/* If REV1 is the first node on its branch, then BEFORE is its
	   root node (on the trunk) and we have to update its branches
	   list.  Otherwise, BEFORE is on the same branch as AFTER, and
	   we can just change BEFORE's `next' field to point to AFTER.
	   (This should be safe: since findnode manages its lists via
	   the `hashnext' and `hashprev' fields, rather than `next' and
	   `prev', mucking with `next' and `prev' should not corrupt the
	   delta tree's internal structure.  Much. -twp) */

	if (rev1 == NULL)
	    /* beforep's ->next field already should be equal to after,
	       which I think is always NULL in this case.  */
	    ;
	else if (STREQ (rev1, branchpoint))
	{
	    nodep = findnode (rcs->versions, before);
	    revp = (RCSVers *) nodep->data;
	    nodep = revp->branches->list->next;
	    while (nodep != revp->branches->list &&
		   ! STREQ (nodep->key, rev1))
		nodep = nodep->next;
	    assert (nodep != revp->branches->list);
	    if (after == NULL)
		delnode (nodep);
	    else
	    {
		xfree (nodep->key);
		nodep->key = xstrdup (after);
	    }
	}
	else
	{
	    nodep = findnode (rcs->versions, before);
	    beforep = (RCSVers *) nodep->data;
		if(beforep->next_alloc)
			xfree (beforep->next);
		else
			rcsbuf_valfree(&rcs->rcsbuf,&beforep->next);
	    beforep->next = xstrdup (after);
		beforep->next_alloc=true;
	}
    }

    status = 0;

 delrev_done:
    if (rev1 != NULL)
	xfree (rev1);
    if (rev2 != NULL)
	xfree (rev2);
    if (branchpoint != NULL)
	xfree (branchpoint);
    if (before != NULL)
	xfree (before);
    if (after != NULL)
	xfree (after);

    save_noexec = noexec;
    noexec = 0;
    if (beforefile != NULL)
    {
	if (unlink_file (beforefile) < 0)
	    error (0, errno, "cannot remove %s", beforefile);
	xfree (beforefile);
    }
    if (afterfile != NULL)
    {
	if (unlink_file (afterfile) < 0)
	    error (0, errno, "cannot remove %s", afterfile);
	xfree (afterfile);
    }
    if (outfile != NULL)
    {
	if (unlink_file (outfile) < 0)
	    error (0, errno, "cannot remove %s", outfile);
	xfree (outfile);
    }
    noexec = save_noexec;

    return status;
}

