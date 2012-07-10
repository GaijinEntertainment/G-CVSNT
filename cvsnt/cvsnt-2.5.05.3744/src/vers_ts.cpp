/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 */

#include "cvs.h"

#ifdef SERVER_SUPPORT
static void time_stamp_server (const char *file, Vers_TS *vers_ts, Entnode *entdata);
#endif

void assign_options(char **existing_options, const char *options)
{
	if(!options || !*options)
		return;
	char options_first_char = options[0];
	if(options_first_char=='+' || options_first_char=='-')
	{
		char *optx = (char*)xmalloc(128);
		*optx='\0';
		if (existing_options)
			TRACE(3,"assign_options: \"%s\" is a delta, apply to \"%s\"",PATCH_NULL(options),PATCH_NULL(*existing_options));
		else
			TRACE(3,"assign_options: \"%s\" is a delta, apply to \"%s\"",PATCH_NULL(options),"(existing options are NULL)");

		kflag k1;
		kflag k2;

		k1.flags = 0;
		k2.flags = 0;
		
		RCS_get_kflags(*existing_options, false, k1);
		RCS_get_kflags(options+1, true, k2);

		if(options_first_char=='+')
		{
			if(k1.flags&KFLAG_BINARY) /* Don't add kkv to binary files */
				k2.flags&=~(KFLAG_DOS|KFLAG_UNIX|KFLAG_KEYWORD|KFLAG_KEYWORD|KFLAG_VALUE|KFLAG_VALUE_LOGONLY);
			if(k2.flags&(KFLAG_DOS|KFLAG_UNIX)) /* Dos and Unix are toggles of each other */
				k1.flags&=~(KFLAG_DOS|KFLAG_UNIX);
			if(k2.flags&KFLAG_PRESERVE) /* Adding -ko removes -kkv */
				k1.flags&=~(KFLAG_KEYWORD|KFLAG_VALUE|KFLAG_VALUE_LOGONLY);
			if(k2.flags&(KFLAG_KEYWORD|KFLAG_VALUE)) /* Adding -kkv removes -ko */
				k1.flags&=~KFLAG_PRESERVE;
			k1.flags|=k2.flags;
			if(k2.flags&KFLAG_ENCODED)
				k1.encoding=k2.encoding;
		}
		else
		{
			k1.flags&=~k2.flags;
			if(k2.flags&KFLAG_ENCODED && k1.encoding.encoding && k2.encoding.encoding && !strcmp(k1.encoding.encoding,k2.encoding.encoding))
			{
				k1.encoding.encoding=NULL;
				k1.flags&=~KFLAG_ENCODED;
			}
		}
		RCS_rebuild_options(&k1,optx);
		xfree(*existing_options);
		if(optx[0])
		{
			*existing_options = optx;
			TRACE(3,"Done a + or -, result is: \"%s\"",optx);
		}
		else
		{
		    char buffer2[MAX_PATH]="\0";
			xfree(optx);
			/* If we've done a + or -, then we need to have something otherwise the empty
			   string would cause RCS to use its defaults */
			TRACE(3,"If we've done a + or -, then we need to have something otherwise the empty string would cause RCS to use its defaults");
			if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","DefaultKeyword",buffer2,sizeof(buffer2)))
			{
			  	*existing_options = xstrdup("kv");
				TRACE(3,"Probably RCS Build Options failed.  No setting DefaultKeyword from PServer, so \"kv\" was used.");
			}
			else
			{
				TRACE(3,"Probably RCS Build Options failed.  Used the setting DefaultKeyword from PServer \"%s\"",buffer2);
			  	*existing_options = xstrdup(buffer2);
			}
		}
	}
	else
	{
		xfree(*existing_options);
		*existing_options = xstrdup (options);
		TRACE(3,"assign_options: \"%s\" is not a delta",PATCH_NULL(options));

	}
}

/* Fill in and return a Vers_TS structure for the file FINFO.  TAG and
   DATE are from the command line.  */
/* Keyword expansion options, I think generally from the command
    line.  Can be either NULL or "" to indicate none are specified
    here.  */
Vers_TS *Version_TS (struct file_info *finfo, const char *options, const char *tag,
    const char *date, int force_tag_match, int set_time, int force_case_match)
{
    Node *p;
    RCSNode *rcsdata;
    Vers_TS *vers_ts;
    struct stickydirtag *sdtp;
    Entnode *entdata;

	TRACE(3,"Version_TS(%s,%s,%d)",PATCH_NULL(tag),PATCH_NULL(date),force_tag_match);
#ifdef UTIME_EXPECTS_WRITABLE
    int change_it_back = 0;
#endif

    /* get a new Vers_TS struct */
    vers_ts = (Vers_TS *) xmalloc (sizeof (Vers_TS));
    memset ((char *) vers_ts, 0, sizeof (*vers_ts));

    /*
     * look up the entries file entry and fill in the version and timestamp
     * if entries is NULL, there is no entries file so don't bother trying to
     * look it up (used by checkout -P)
     */
    if (finfo->entries == NULL)
    {
		sdtp = NULL;
		p = NULL;
    }
    else
    {
		if(force_case_match)
			p = findnode (finfo->entries, finfo->file);
		else
			p = findnode_fn (finfo->entries, finfo->file);
		sdtp = (struct stickydirtag *) finfo->entries->list->data; /* list-private */
    }

    entdata = NULL;
    if (p != NULL)
    {
		entdata = (Entnode *) p->data;

		if (entdata->type == ENT_SUBDIR)
		{
			/* According to cvs.texinfo, the various fields in the Entries
			file for a directory (other than the name) do not have a
			defined meaning.  We need to pass them along without getting
			confused based on what is in them.  Therefore we make sure
			not to set vn_user and the like from Entries, add.c and
			perhaps other code will expect these fields to be NULL for
			a directory.  */
			vers_ts->entdata = entdata;
		}
		else
#ifdef SERVER_SUPPORT
	/* An entries line with "D" in the timestamp indicates that the
	   client sent Is-modified without sending Entry.  So we want to
	   use the entries line for the sole purpose of telling
	   time_stamp_server what is up; we don't want the rest of CVS
	   to think there is an entries line.  */
	if (strcmp (entdata->timestamp, DATE_CHAR_S) != 0)
#endif
		{
			vers_ts->vn_user = xstrdup (entdata->version);
			vers_ts->ts_rcs = xstrdup (entdata->timestamp);

			vers_ts->ts_conflict = xstrdup (entdata->conflict);
			if (!(tag || date) && !(sdtp && sdtp->aflag))
			{
				vers_ts->tag = xstrdup (entdata->tag);
				vers_ts->date = xstrdup (entdata->date);
			}
			vers_ts->entdata = entdata;
		}
		/* Even if we don't have an "entries line" as such
		(vers_ts->entdata), we want to pick up options which could
		have been from a Kopt protocol request.  */
		if (!options || *options == '\0')
		{
			if (!(sdtp && sdtp->aflag))
				vers_ts->options = xstrdup (entdata->options);
		}
    }

    /*
     * -k options specified on the command line override (and overwrite)
     * options stored in the entries file
     */
    if (options && *options != '\0')
	{
		TRACE(3,"Version_TS: got an open (eg: -k+x), need to find the 'default' for \"%s\"",PATCH_NULL(finfo->file));
		char *existing_options=wrap_rcsoption(finfo->file);
		if ((vers_ts->options==NULL)&&(existing_options!=NULL))
		{
			TRACE(3,"Version_TS: an default options of \"%s\".",PATCH_NULL(existing_options));
			vers_ts->options=xstrdup(existing_options);
		}
		TRACE(3,"Version_TS: assign_options(\"%s\",\"%s\")",PATCH_NULL(vers_ts->options),PATCH_NULL(options));
		assign_options(&vers_ts->options,options);
	}

    /*
     * if tags were specified on the command line, they override what is in
     * the Entries file
     */
    if (tag)
		vers_ts->tag = xstrdup (tag);
	if (date)
		vers_ts->date = xstrdup (date);
    if (!vers_ts->entdata && (sdtp && sdtp->aflag == 0))
    {
		if (!vers_ts->tag)
		{
			vers_ts->tag = xstrdup (sdtp->tag);
			vers_ts->nonbranch = sdtp->nonbranch;
		}
		if (!vers_ts->date)
			vers_ts->date = xstrdup (sdtp->date);
    }

    /* Now look up the info on the source controlled file */
    if (finfo->rcs != NULL)
    {
	rcsdata = finfo->rcs;
	rcsdata->refcount++;
    }
    else if (finfo->repository != NULL && !current_parsed_root->isremote)
		rcsdata = RCS_parse (finfo->mapped_file, finfo->repository);
    else
		rcsdata = NULL;

    if (rcsdata != NULL)
    {
		/* squirrel away the rcsdata pointer for others */
		vers_ts->srcfile = rcsdata;

		if (vers_ts->tag && strcmp (vers_ts->tag, TAG_BASE) == 0)
		{
			vers_ts->vn_rcs = xstrdup (vers_ts->vn_user);
			vers_ts->vn_tag = xstrdup (vers_ts->vn_user);
		}
		else
		{
			int simple=0;

			int userevone=1;


			int force_tag_match_now=force_tag_match;
			int this_is_repoversion=0;

			if ((vers_ts->tag!=NULL) && (userevone!=0) && (strstr(rcsdata->path, RCSREPOVERSION)!=NULL))
			{
				force_tag_match_now=1;
				this_is_repoversion=1;
			}
			TRACE(3,"Version_TS - call RCS_getversion(\"%s\",%s,%s,%d,%d)",
							PATCH_NULL(rcsdata->path),
							PATCH_NULL(vers_ts->tag),
							PATCH_NULL(vers_ts->date),force_tag_match_now,simple);
			vers_ts->vn_rcs = RCS_getversion (rcsdata, vers_ts->tag,
							vers_ts->date, force_tag_match_now,
							&simple);
			if ((this_is_repoversion) && (userevone!=0) && (vers_ts->vn_rcs == NULL))
			{
				TRACE(3,"Version_TS() - tag match failed, try not forcing...");
				TRACE(3,"Version_TS - call RCS_getversion(\"%s\",%s,%s,%d,%d)",
							PATCH_NULL(rcsdata->path),
							PATCH_NULL(vers_ts->tag),
							PATCH_NULL(vers_ts->date),force_tag_match,simple);
				vers_ts->vn_rcs = RCS_getversion (rcsdata, vers_ts->tag,
							vers_ts->date, force_tag_match,
							&simple);
				if (vers_ts->vn_rcs == NULL)
				{
					TRACE(3,"Version_TS() - still no sign of it, use version 1.1 in this case");
					vers_ts->vn_rcs = RCS_getversion (rcsdata, "1.1",
							vers_ts->date, force_tag_match,
							&simple);
				}
			}
			if (vers_ts->vn_rcs == NULL)
			{
				/* In a directory all tags exist in a virtual sense, and have revision 1.1 */
				if(!strcmp(finfo->file,RCSREPOVERSION))
					vers_ts->vn_rcs = xstrdup("1.1");
				else
					vers_ts->vn_tag = NULL;
			}
			else if (simple)
				vers_ts->vn_tag = xstrdup (vers_ts->tag);
			else
				vers_ts->vn_tag = NULL; //xstrdup (vers_ts->vn_rcs);
		}

		vers_ts->prop_global = RCS_getproplist(rcsdata,NULL);
		if(vers_ts->vn_rcs)
		{
			vers_ts->filename = RCS_getfilename(rcsdata, vers_ts->vn_rcs);
			vers_ts->tt_rcs = RCS_getrevtime(rcsdata, vers_ts->vn_rcs, NULL, 0);
			vers_ts->prop_local = RCS_getproplist(rcsdata, vers_ts->vn_rcs);
		}

		if (!options || !*options || (options[0]=='-' || options[0]=='+'))
		{
			/* If no keyword expansion was specified on command line,
			use whatever was in the rcs file (if there is one).  This
			is how we, if we are the server, tell the client whether
			a file is binary.  */
			if(vers_ts->vn_rcs)
			{
				char *rcsexpand = RCS_getexpand(rcsdata?rcsdata:finfo->rcs,vers_ts->vn_rcs);
				xfree(vers_ts->options);
				vers_ts->options = xstrdup(rcsexpand);
			}
			assign_options(&vers_ts->options,options);
		}

		/*
		* If the source control file exists and has the requested revision,
		* get the Date the revision was checked in.  If "user" exists, set
		* its mtime.
		*/
		if (set_time && vers_ts->vn_rcs != NULL)
		{
#ifdef SERVER_SUPPORT
			if (server_active)
				server_modtime (finfo, vers_ts);
			else
#endif
			{
				struct utimbuf t;

				memset (&t, 0, sizeof (t));
				t.modtime =
					RCS_getrevtime (rcsdata, vers_ts->vn_rcs, 0, 0);
				if (t.modtime != (time_t) -1)
				{
					t.actime = t.modtime;

#ifdef UTIME_EXPECTS_WRITABLE
					if (!iswritable (vers_ts->filename?vers_ts->filename:finfo->file))
					{
						xchmod (vers_ts->filename?vers_ts->filename:finfo->file, 1);
						change_it_back = 1;
					}
#endif  /* UTIME_EXPECTS_WRITABLE  */

					/* This used to need to ignore existence_errors
					(for cases like where update.c now clears
					set_time if noexec, but didn't used to).  I
					think maybe now it doesn't (server_modtime does
					not like those kinds of cases).  */
					(void) utime (vers_ts->filename?vers_ts->filename:finfo->file, &t);

#ifdef UTIME_EXPECTS_WRITABLE
					if (change_it_back == 1)
					{
					xchmod (vers_ts->filename?vers_ts->filename:finfo->file, 0);
					change_it_back = 0;
					}
#endif  /*  UTIME_EXPECTS_WRITABLE  */
				}
			}
		}
    }

	if (!vers_ts->options)
		vers_ts->options = xstrdup ("");

    /* get user file time-stamp in ts_user */
    if (finfo->entries != (List *) NULL)
    {
		/* If this is a directory history file, use the directory version */
		if(!strcmp(finfo->file,RCSREPOVERSION))
		{
			vers_ts->ts_user = xstrdup("0"); /* We set a bogus timestamp so classify_file does the right thing */
		}
		else
#ifdef SERVER_SUPPORT
		if (server_active)
			time_stamp_server (finfo->file, vers_ts, entdata);
		else
#endif
		{
			if(filenames_case_insensitive && force_case_match && !case_isfile(vers_ts->filename?vers_ts->filename:finfo->file,NULL))
				vers_ts->ts_user = NULL;
			else
				vers_ts->ts_user = time_stamp (vers_ts->filename?vers_ts->filename:finfo->file,0);
		}

		if(entdata)
		{
			vers_ts->edit_revision = xstrdup(entdata->edit_revision);
			vers_ts->edit_tag = xstrdup(entdata->edit_tag);
			vers_ts->edit_bugid = xstrdup(entdata->edit_bugid);
		}
    }

    return (vers_ts);
}

#ifdef SERVER_SUPPORT

/* Set VERS_TS->TS_USER to time stamp for FILE.  */

/* Separate these out to keep the logic below clearer.  */
#define mark_lost(V)		((V)->ts_user = 0)
#define mark_unchanged(V)	((V)->ts_user = xstrdup ((V)->ts_rcs))

static void time_stamp_server (const char *file, Vers_TS *vers_ts, Entnode *entdata)
{
    struct stat sb;
    char *cp;

    if (CVS_LSTAT (file, &sb) < 0)
    {
	if (! existence_error (errno))
	    error (1, errno, "cannot stat temp file");

	/* Missing file means lost or unmodified; check entries
	   file to see which.

	   XXX FIXME - If there's no entries file line, we
	   wouldn't be getting the file at all, so consider it
	   lost.  I don't know that that's right, but it's not
	   clear to me that either choice is.  Besides, would we
	   have an RCS string in that case anyways?  */
	if (entdata == NULL)
	    mark_lost (vers_ts);
	else if (entdata->timestamp
		 && entdata->timestamp[0] == UNCHANGED_CHAR)
	    mark_unchanged (vers_ts);
	else if (entdata->timestamp != NULL
		 && (entdata->timestamp[0] == MODIFIED_CHAR
		     || entdata->timestamp[0] == DATE_CHAR)
		 && entdata->timestamp[1] == '\0')
	    vers_ts->ts_user = xstrdup ("Is-modified");
	else
	    mark_lost (vers_ts);
    }
    else if (sb.st_mtime == 0)
    {
	/* We shouldn't reach this case any more!  */
	abort ();
    }
    else
    {
        struct tm *tm_p;
        struct tm local_tm;

	vers_ts->ts_user = (char*)xmalloc (25);
	/* We want to use the same timestamp format as is stored in the
	   st_mtime.  For unix (and NT I think) this *must* be universal
	   time (UT), so that files don't appear to be modified merely
	   because the timezone has changed.  For VMS, or hopefully other
	   systems where gmtime returns NULL, the modification time is
	   stored in local time, and therefore it is not possible to cause
	   st_mtime to be out of sync by changing the timezone.  */
	tm_p = gmtime (&sb.st_mtime);
	if (tm_p)
	{
	    memcpy (&local_tm, tm_p, sizeof (local_tm));
	    cp = asctime (&local_tm);	/* copy in the modify time */
	}
	else
	    cp = ctime (&sb.st_mtime);

	cp[24] = 0;
	(void) strcpy (vers_ts->ts_user, cp);

    }
}

#endif /* SERVER_SUPPORT */

/*
 * Gets the time-stamp for the file "file" and returns it in space it
 * allocates
 */
char *time_stamp (const char *file, int local)
{
    struct stat sb;
    char *cp;
    char *ts;

    if (CVS_LSTAT (file, &sb) < 0)
    {
	ts = NULL;
    }
    else
    {
	struct tm *tm_p;
        struct tm local_tm;
	ts = (char*)xmalloc (25);
	/* We want to use the same timestamp format as is stored in the
	   st_mtime.  For unix (and NT I think) this *must* be universal
	   time (UT), so that files don't appear to be modified merely
	   because the timezone has changed.  For VMS, or hopefully other
	   systems where gmtime returns NULL, the modification time is
	   stored in local time, and therefore it is not possible to cause
	   st_mtime to be out of sync by changing the timezone.  */
	tm_p = local?NULL:gmtime (&sb.st_mtime);
	if (tm_p)
	{
	    memcpy (&local_tm, tm_p, sizeof (local_tm));
	    cp = asctime (&local_tm);	/* copy in the modify time */
	}
	else
	    cp = ctime(&sb.st_mtime);

	cp[24] = 0;
	(void) strcpy (ts, cp);
    }

    return (ts);
}

/*
 * free up a Vers_TS struct
 */
void freevers_ts(Vers_TS **versp)
{
	if(*versp)
	{
		if ((*versp)->srcfile)
			freercsnode (&((*versp)->srcfile));
		xfree ((*versp)->vn_user);
		xfree ((*versp)->vn_rcs);
		xfree ((*versp)->vn_tag);
		xfree ((*versp)->ts_user);
		xfree ((*versp)->ts_rcs);
		xfree ((*versp)->options);
		xfree ((*versp)->tag);
		xfree ((*versp)->date);
		xfree ((*versp)->ts_conflict);
		xfree ((*versp)->filename);
		xfree ((*versp)->edit_revision);
		xfree ((*versp)->edit_tag);
		xfree ((*versp)->edit_bugid);
		xfree (*versp);
	}
}

