/* Implementation for "cvs edit", "cvs watch on", and related commands

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "getline.h"
#include "watch.h"
#include "edit.h"
#include "fileattr.h"

static int check_edited = 0;
static int watch_onoff(int argc, char **argv);

static int setting_default;
static int turning_on;

static int keep_write;
static int revert_only;
static cvs::string bugid;
static const char *message;
static int exclusive_edit;
static int whole_file;
static int all_editors;
static int verbose;
static int check_edit;
static int check_uptodate;
#if defined(_WIN32) && !defined(CVS95)
static int set_acl;
#endif

static int setting_tedit;
static int setting_tunedit;
static int setting_tcommit;
static int editors_found;
static int gzip_copies;
static char *notify_user;
static int force_unedit;

/* Watcher who always gets notifications */
const char *global_watcher;

static int unedit_fileproc (void *callerdat, struct file_info *finfo);

static int onoff_fileproc(void *callerdat, struct file_info *finfo)
{
	CXmlNodePtr handle = fileattr_getroot();
	handle->xpathVariable("name",finfo->file);
	if(!handle->Lookup("file[cvs:filename(@name,$name)]") || !handle->XPathResultNext())
	{
		handle = fileattr_getroot();
		handle->NewNode("file");
		handle->NewAttribute("name",finfo->file);
	}

	if(turning_on)
	{
		if(!handle->GetChild("watched")) handle->NewNode("watched");
	}
	else
	{
		if(handle->GetChild("watched")) handle->Delete();
	}
    return 0;
}

static int onoff_filesdoneproc (void *callerdat, int err, char *repository, char *update_dir, List *entries)
{
    if (setting_default)
	{
		CXmlNodePtr handle = fileattr_find(NULL,"/directory/default");

		if(!handle)
		{
			handle = fileattr_getroot();
			handle->NewNode("directory");
			handle->NewNode("default");
		}

		if(turning_on)
		{
			if(!handle->GetChild("watched")) handle->NewNode("watched");
		}
		else
		{
			if(handle->GetChild("watched")) handle->Delete();
		}
	}
    return err;
}

static int watch_onoff (int argc, char **argv)
{
    int c;
    int local = 0;
    int err;

    optind = 0;
    while ((c = getopt (argc, argv, "+lR")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (watch_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote)
    {
	if (local)
	    send_arg ("-l");
	send_arg("--");
	send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_to_server (turning_on ? "watch-on\n" : "watch-off\n", 0);
	return get_responses_and_close ();
    }

    setting_default = (argc <= 0);

    lock_tree_for_write (argc, argv, local, W_LOCAL, 0);

    err = start_recursion (onoff_fileproc, onoff_filesdoneproc,
			   (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
			   0, verify_write, NULL);

    Lock_Cleanup ();
    return err;
}

int watch_on (int argc, char **argv)
{
    turning_on = 1;
    return watch_onoff (argc, argv);
}

int watch_off (int argc, char **argv)
{
    turning_on = 0;
    return watch_onoff (argc, argv);
}

static int dummy_fileproc(void *callerdat, struct file_info *finfo)
{
    /* This is a pretty hideous hack, but the gist of it is that recurse.c
       won't call notify_check unless there is a fileproc, so we can't just
       pass NULL for fileproc.  */
    return 0;
}

/* Check for and process notifications.  Local only.  I think that doing
   this as a fileproc is the only way to catch all the
   cases (e.g. foo/bar.c), even though that means checking over and over
   for the same CVSADM_NOTIFY file which we removed the first time we
   processed the directory.  */

static int ncheck_fileproc (void *callerdat, struct file_info *finfo)
{
    int notif_type;
    char *filename;
    char *cp,*cpo;
    char *watches;
	char *date, *hostname, *pathname, *flags, *bugno, *msg, *tag;
	int err = 0;
	List *rollback_list = NULL;

    FILE *fp;
    char *line = NULL;
    size_t line_len = 0;

    /* We send notifications even if noexec.  I'm not sure which behavior
       is most sensible.  */
    fp = CVS_FOPEN (CVSADM_NOTIFY, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", CVSADM_NOTIFY);
	return 0;
    }

    while (getline (&line, &line_len, fp) > 0)
    {
	notif_type = line[0];
	if (notif_type == '\0')
	    continue;
	filename = line + 1;
	cp = strchr (filename, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	date = cp;
	cp = strchr (date, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	hostname=cp;
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	pathname=cp;
	cp = strchr (cp, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	watches = cp;
	cpo = cp;
	cp = strchr (cp, '\t');
	if(cp)
	{
		*cp++ = '\0';
		tag = cp;
		cp = strchr (cp, '\t');
		if (cp == NULL)
			continue;
		*cp++ = '\0';
		flags = cp;
		cp = strchr (cp, '\t');
		if (cp == NULL)
			continue;
		*cp++ = '\0';
		bugno = cp;
		cp = strchr (cp, '\t');
		if (cp == NULL)
			continue;
		*cp++ = '\0';
		msg = cp;
	}
	else
	{
		cp = cpo;
		tag = NULL;
		flags = NULL;
		bugno = NULL;
		msg = NULL;
	}
	cp = strchr (cp, '\n');
	if (cp == NULL)
	    continue;
	*cp = '\0';

	if( notify_do (notif_type, filename, notify_user?notify_user:getcaller (), date,hostname,pathname, watches,
			finfo->repository, tag, flags, bugno&&*bugno?bugno:NULL, msg) )
	{
		if(!rollback_list)
			rollback_list=getlist();
		Node *n = findnode_fn(rollback_list,filename);
		if(!n)
		{
			n=getnode();
			n->key=xstrdup(filename);
			addnode(rollback_list,n);
		}
	}
    }
    xfree (line);

    if (ferror (fp))
	error (0, errno, "cannot read %s", CVSADM_NOTIFY);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", CVSADM_NOTIFY);

    if ( CVS_UNLINK (CVSADM_NOTIFY) < 0)
	error (0, errno, "cannot remove %s", CVSADM_NOTIFY);

	if(rollback_list)
	{
		Node *n = rollback_list->list->next;
		while(n!=rollback_list->list)
		{
			file_info fi;
			fi.file=n->key;
			fi.entries=finfo->entries;
			fi.update_dir=finfo->update_dir;
			unedit_fileproc(NULL,&fi);
			n=n->next;
		}
		dellist(&rollback_list);
	}

    return err;
}

/* Look through the CVSADM_NOTIFY file and process each item there
   accordingly.  */
static int send_notifications (int argc, char **argv, int local)
{
    int err = 0;

    /* OK, we've done everything which needs to happen on the client side.
       Now we can try to contact the server; if we fail, then the
       notifications stay in CVSADM_NOTIFY to be sent next time.  */
    if (current_parsed_root->isremote)
    {
		err += start_recursion (dummy_fileproc, (FILESDONEPROC) NULL,
					(PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
					argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
					0, NULL, NULL);

		send_to_server ("noop\n", 0);
		if (strcmp (command_name, "release") == 0)
    			err += get_server_responses ();
		else
    			err += get_responses_and_close ();
    }
    else
    {
	/* Local.  */

	lock_tree_for_write (argc, argv, local, W_LOCAL, 0);
	err += start_recursion (ncheck_fileproc, (FILESDONEPROC) NULL,
				(PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
				argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
				0, NULL, NULL);
	Lock_Cleanup ();
    }
    return err;
}

static int editors_output (struct file_info *finfo)
{
	CXmlNodePtr  handle;
	const char *username, *hostname, *pathname, *time, *tag, *vtag, *bug;
	Vers_TS *vers;
	int out = 0;

	handle = fileattr_getroot();
	handle->xpathVariable("name",finfo->file);
	if(!handle->Lookup("file[cvs:filename(@name,$name)]/editor") || !handle->XPathResultNext())
	   handle = NULL;

	vers = Version_TS(finfo,NULL,NULL,NULL,0,0,0);

    while (handle)
    {
		username=fileattr_getvalue(handle,"@name");
		time=fileattr_getvalue(handle,"time");
		hostname=fileattr_getvalue(handle,"hostname");
		pathname=fileattr_getvalue(handle,"pathname");
		tag=fileattr_getvalue(handle,"tag");
		bug=fileattr_getvalue(handle,"bugid");
		vtag = vers->tag;

		if(!vtag) vtag="HEAD";

		if(all_editors || !tag || !strcmp(tag,vtag))
		{
			if(!out)
				cvs_output (fn_root(finfo->fullname), 0);
			out=1;

			cvs_output ("\t", 1);
			if(username)
				cvs_output(username,0);
			cvs_output("\t", 1);
			if(time)
				cvs_output(time,0);
			cvs_output("\t", 1);
			if(hostname)
				cvs_output(hostname,0);
			cvs_output("\t", 1);
			if(pathname)
				cvs_output(fn_root(pathname),0);
			if(verbose)
			{
				cvs_output("\t", 1);
				if(bug)
					cvs_output(bug,0);
			}
			if(all_editors)
			{
				cvs_output("\t", 1);
				if(tag)
					cvs_output(tag,0);
			}
			cvs_output ("\n", 1);
		}

		if(!handle->XPathResultNext())
		   handle = NULL;
	}

	freevers_ts(&vers);

    return 0;
}


/* check file that is to be edited if it's already being edited */

static int check_fileproc (void *callerdat, struct file_info *finfo)
{
	CXmlNodePtr  handle;
    char *editors = NULL;
    int status;
	int errors = 0;
	Vers_TS *v;
	
    if (check_uptodate) 
    {
		int q;

		q=really_quiet;

		really_quiet = 1; /* Supress any messages.. we just want status */

		Ctype status = Classify_File (finfo, (char *) NULL, (char *) NULL,
						(char *) NULL, 1, 0, &v, 0, 0, 0);

		really_quiet = q;

		if ((status != T_UPTODATE) && (status != T_CHECKOUT) &&
			(status != T_PATCH) && (status != T_REMOVE_ENTRY))
		{
			error (0, 0, "%s is locally modified", fn_root(finfo->fullname));
			freevers_ts (&v);
			return (1);
		}
    }
    else
		v = Version_TS (finfo, NULL, NULL, NULL, 0, 0, 0);

	kflag kftmp;
	RCS_get_kflags(v->options,false,kftmp);
	if(kftmp.flags&KFLAG_EXCLUSIVE_EDIT)
		exclusive_edit = 1;

	editors_found = 0;

    if (current_parsed_root->isremote)
    {
        int first_time;
        int len = 0;
        int possibly_more_editors = 0;
		char *argv = (char*)finfo->fullname;

        send_file_names (1, &argv, 0);
		if(supported_request("editors-edit"))
			send_to_server("editors-edit\n", 0);
		else
			send_to_server ("editors\n", 0);

        first_time = 1;
        do
        {
            possibly_more_editors = 0;

            to_server_buffer_flush ();
            from_server_buffer_read (&editors, &len);

            if (editors != NULL)
            {
		if (!strncmp(editors, "error ",6))
		{
			errors = 1;
			if(editors[6])
			{
				cvs_outerr(editors+6,0);
				cvs_outerr("\n",0);
			}
			possibly_more_editors = 0;
		}
                else if (strcmp (editors, "ok"))
                {
                    possibly_more_editors = 1;

                    switch (editors[0])
                    {
                        case 'M':
                        {
							if(fnncmp(editors+2,finfo->fullname,strchr(editors+2,'\t')-(editors+2)))
								break;

							kflag kftmp;
							RCS_get_kflags(v->options,false,kftmp);
							if(check_edited>=0 && (check_edited || kftmp.flags&KFLAG_RESERVED_EDIT))
								editors_found = 2;
							else
								editors_found = 1;

                            if(!really_quiet)
                            {
                                cvs_output (editors + 2, 0);
                                cvs_output ("\n", 0);
                            }

                            break;
                        }

                        default:
                        {
                            struct response *rs = NULL;
                            char *cmd = NULL;

                            cmd = editors;

                            for (rs = responses; rs->name != NULL; ++rs)
                                if (strncmp (cmd, rs->name, strlen (rs->name)) == 0)
                                {
                                    int cmdlen = strlen (rs->name);
                                    if (cmd[cmdlen] == ' ')
                                        ++cmdlen;
                                    else if (cmd[cmdlen] != '\0')
                                        /*
                                         * The first len characters match, but it's a different
                                         * response.  e.g. the response is "oklahoma" but we
                                         * matched "ok".
                                         */
                                        continue;
                                    (*rs->func) (cmd + cmdlen, len - cmdlen);
                                    break;
                                }
                            if (rs->name == NULL)
                                /* It's OK to print just to the first '\0'.  */
                                /* We might want to handle control characters and the like
                                   in some other way other than just sending them to stdout.
                                   One common reason for this error is if people use :ext:
                                   with a version of rsh which is doing CRLF translation or
                                   something, and so the client gets "ok^M" instead of "ok".
                                   Right now that will tend to print part of this error
                                   message over the other part of it.  It seems like we could
                                   do better (either in general, by quoting or omitting all
                                   control characters, and/or specifically, by detecting the CRLF
                                   case and printing a specific error message).  */
                                error (0, 0,
                                       "warning: unrecognized response `%s' from cvs server",
                                       cmd);

                            break;
                        }
                    }
                }

                xfree(editors);
            }
        } while (!errors && possibly_more_editors);
    }
    else
    {
        /* This is a somewhat screwy way to check for this, because it
           doesn't help errors other than the nonexistence of the file
           (e.g. permissions problems).  It might be better to rearrange
           the code so that CVSADM_NOTIFY gets written only after the
           various actions succeed (but what if only some of them
           succeed).  */
        if (!isfile (finfo->file))
        {
            error (0, 0, "no such file %s; ignored", fn_root(finfo->fullname));
            return 0;
        }

        handle = fileattr_getroot();
	handle->xpathVariable("name",finfo->file);
	if(handle->Lookup("file[cvs:filename(@name,$name)]/editor") && handle->XPathResultNext())
	{
          if(!really_quiet)
            editors_output (finfo);

	  kflag kftmp;
	  RCS_get_kflags(v->options,false,kftmp);
	  if(check_edited>=0 && (check_edited || kftmp.flags&KFLAG_RESERVED_EDIT))
		editors_found = 2;
	  else
		editors_found = 1;
        }
    }

    if(errors || editors_found==2)
        status = 1;
    else
        status = 0;

	freevers_ts(&v);
    return status;
}

/* Look through the CVS/fileattr file and check for editors */
static int check_edits (int argc, char **argv, int local)
{
    int err = 0;

    if (current_parsed_root->isremote)
    {
        if (local)
            send_arg ("-l");
		send_arg("--");
        send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
    }

	err += start_recursion (check_fileproc, (FILESDONEPROC) NULL,
                            (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
                            argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
                            0, verify_read, NULL);

    if (current_parsed_root->isremote)
    {
        send_to_server ("noop\n", 0);
        err += get_server_responses ();
    }
    return err;
}

static int edit_fileproc (void *callerdat, struct file_info *finfo)
{
    FILE *fp;
    char *basefilename;
    char *oldfilename;
	Vers_TS *vers;
	const char *file = finfo->file;

    if (noexec)
		return 0;

    /* This is a somewhat screwy way to check for this, because it
       doesn't help errors other than the nonexistence of the file
       (e.g. permissions problems).  It might be better to rearrange
       the code so that CVSADM_NOTIFY gets written only after the
       various actions succeed (but what if only some of them
       succeed).  */
    if (!isfile (finfo->file))
    {
		error (0, 0, "no such file %s; ignored", fn_root(finfo->fullname));
		return 0;
    }

    fp = open_file (CVSADM_NOTIFY, "a");

	vers = Version_TS(finfo,NULL,NULL,NULL,0,0,0);

	if(vers->entdata && vers->entdata->user)
		file = vers->entdata->user;

    char *wd = (char *) xmalloc (strlen (CurDir) + strlen ("/") + strlen (finfo->update_dir) + 1);

    strcpy(wd, CurDir);

    if(finfo->update_dir != NULL  &&  *finfo->update_dir != '\0')
    {
        strcat(wd, "/");
        strcat(wd, finfo->update_dir);
    }

	fprintf (fp, "E%s\t%s GMT\t%s\t%s\t", file, global_session_time, hostname, wd);

    xfree(wd);

    if (setting_tedit)
		fprintf (fp, "E");
    if (setting_tunedit)
		fprintf (fp, "U");
    if (setting_tcommit)
		fprintf (fp, "C");

	fprintf(fp,"\t%s\t%s\t%s\t%s",whole_file?"":vers->tag?vers->tag:"HEAD",exclusive_edit?"X":"",bugid.size()?bugid.c_str():"",message?message:"");
    fprintf (fp, "\n");

    if (fclose (fp) < 0)
    {
	if (finfo->update_dir[0] == '\0')
	    error (0, errno, "cannot close %s", CVSADM_NOTIFY);
	else
	    error (0, errno, "cannot close %s/%s", finfo->update_dir,
		   CVSADM_NOTIFY);
    }

#if defined(_WIN32) && !defined(CVS95)
	if(set_acl)
		win32_set_edit_acl(file);
#endif
    xchmod (file, 1);

    /* Now stash the file away in CVSADM so that unedit can revert even if
       it can't communicate with the server. */
    /* Could save a system call by only calling mkdir_if_needed if
       trying to create the output file fails.  But copy_file isn't
       set up to facilitate that.  */
    mkdir_if_needed (CVSADM_BASE);
    basefilename = (char*)xmalloc (16 + sizeof CVSADM_BASE + strlen (file));
    oldfilename = (char*)xmalloc (16 + sizeof CVSADM_BASE + strlen (file));
    strcpy (basefilename, CVSADM_BASE);
    strcat (basefilename, "/");
    strcat (basefilename, file);
    strcpy(oldfilename,basefilename);
    if(gzip_copies)
    {
      strcat (basefilename, ".gz");
      copy_and_zip_file (file, basefilename, 1, 1);
    }
    else
    {
      strcat (oldfilename, ".gz");
      copy_file (file, basefilename, 1, 1);
    }
    if(unlink_file(oldfilename) && !existence_error(errno))
	error(1, errno, "unable to remove old %s", oldfilename);

	xchmod (basefilename, 0);
    xfree (basefilename);
    xfree (oldfilename);

	if (vers->vn_user)
	{
		Register(finfo->entries, file, vers->vn_user, vers->ts_rcs,
			vers->options, vers->tag, vers->date,
			vers->ts_conflict, vers->entdata->merge_from_tag_1, vers->entdata->merge_from_tag_2, vers->tt_rcs, vers->vn_user,whole_file||vers->tag?vers->tag:"HEAD",bugid.c_str(), NULL);
	}
	freevers_ts(&vers);

    return 0;
}

static const char *const edit_usage[] =
{
    "Usage: %s %s [-cflRz] [files...]\n",
#if defined(_WIN32) && !defined(CVS95)
    "\t-A\t\tSet ACL on edited file (experimental)\n",
#endif
    "\t-a\t\tSpecify what actions for temporary watch, one of\n",
    "\t\t\t\tedit,unedit,commit,all,none\n",
	"\t-b bugid\tBug to associate with edit (repeat for multiple bugs)\n",
    "\t-c\t\tCheck that working files are unedited\n",
    "\t-C\t\tCheck that working files are up to date\n",
    "\t-f\t\tForce edit if working files are edited (default)\n",
    "\t-l\t\tLocal directory only, not recursive\n",
	"\t-m message\tSpecify reason for edit\n",
    "\t-R\t\tProcess directories recursively (default)\n",
	"\t-w\t\tLock whole file, not just this branch\n",
	"\t-x\t\tExclusive edit (Stop other users editing this file)\n",
    "\t-z\t\tCompress base revision copies\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int edit (int argc, char **argv)
{
    int local = 0;
    int c;
    int err = 0;
    int a_omitted;

    if (argc == -1)
		usage (edit_usage);

    a_omitted = 1;
    setting_tedit = 0;
    setting_tunedit = 0;
    setting_tcommit = 0;
    optind = 0;
	while ((c = getopt (argc, argv, "+cflRa:zb:m:xwAC")) != -1)
    {
	switch (c)
	{
        case 'c':
            check_edited = 1;
            break;
		case 'C':
			check_uptodate = 1;
			break;
        case 'f':
            check_edited = -1;
            break;
	    case 'l':
			local = 1;
			break;
	    case 'R':
			local = 0;
			break;
		case 'w':
			whole_file = 1;
			break;
		case 'A':
#if defined(_WIN32) && !defined(CVS95)
			set_acl = 1;
#endif
			break;
		case 'a':
			a_omitted = 0;
			if (strcmp (optarg, "edit") == 0)
				setting_tedit = 1;
			else if (strcmp (optarg, "unedit") == 0)
				setting_tunedit = 1;
			else if (strcmp (optarg, "commit") == 0)
				setting_tcommit = 1;
			else if (strcmp (optarg, "all") == 0)
			{
				setting_tedit = 1;
				setting_tunedit = 1;
				setting_tcommit = 1;
			}
			else if (strcmp (optarg, "none") == 0)
			{
				setting_tedit = 0;
				setting_tunedit = 0;
				setting_tcommit = 0;
			}
			else
				usage (edit_usage);
			break;
	    case 'z':
			gzip_copies = 1;
		break;
		case 'b':
			if(bugid.size())
				bugid+=",";
			bugid += optarg;
			break;
		case 'm':
			message = xstrdup(optarg);
			break;
		case 'x':
			exclusive_edit = 1;
			break;
	    case '?':
	    default:
			usage (edit_usage);
			break;
	}
    }
    argc -= optind;
    argv += optind;

    if (a_omitted)
    {
	setting_tedit = 1;
	setting_tunedit = 1;
	setting_tcommit = 1;
    }

    if (strpbrk (hostname, "+,>;=\t\n") != NULL)
	error (1, 0,
	       "host name (%s) contains an invalid character (+,>;=\\t\\n)",
	       hostname);
    if (strpbrk (CurDir, "+,>;=\t\n") != NULL)
	error (1, 0,
"current directory (%s) contains an invalid character (+,>;=\\t\\n)",
	       CurDir);

#ifdef SERVER_SUPPORT
   if(current_parsed_root->isremote && supported_request("Error-If-Reader"))
    send_to_server("Error-If-Reader The 'cvs edit' command requires write access to the repository\n",0);
#endif
    /* No need to readlock since we aren't doing anything to the
       repository.  */
    err = check_edits (argc, argv, local);
	if(err && editors_found)
		error(1,0,"Files being edited!");
    if(!err)
    {
		err = start_recursion (edit_fileproc, (FILESDONEPROC) NULL,
				(PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
				argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
				0, verify_write, NULL);
		err += send_notifications (argc, argv, local);
    }

    return err;
}

static int unedit_fileproc (void *callerdat, struct file_info *finfo)
{
    FILE *fp;
    char *basefilename;
    char *gzipfilename;
	int gzip = 0;
	Node *node;
	Entnode *entdata;
	bool do_unedit=true;

    if (noexec)
		return 0;

	node = findnode_fn (finfo->entries, finfo->file);
	if(!node)
	{
		error(0,0,"? %s",finfo->file);
		return 0; /* No file */
	}
	entdata = (Entnode *) node->data;

	if(bugid.size() && (!entdata->edit_bugid || !bugid_in(bugid.c_str(),entdata->edit_bugid)))
		return 0;

    basefilename = (char*)xmalloc (10 + sizeof CVSADM_BASE + strlen (entdata->user));
    gzipfilename = (char*)xmalloc (10 + sizeof CVSADM_BASE + strlen (entdata->user));
    strcpy (basefilename, CVSADM_BASE);
    strcat (basefilename, "/");
    strcat (basefilename, entdata->user);
    strcpy (gzipfilename, basefilename);
    strcat (gzipfilename, ".gz");

    if(isfile(gzipfilename))
    {
      copy_and_unzip_file(gzipfilename,basefilename, 1, 1);
	  if(revert_only)
		  gzip = 1;
	  else
	  {
		if(unlink_file(gzipfilename) && !existence_error(errno))
			error(1, errno, "Unable to remove gzip copy %s", gzipfilename);
	  }
    }
    xfree(gzipfilename);

	if(isfile(basefilename))
	{
		if (!force_unedit && isfile(entdata->user) && xcmp (entdata->user, basefilename) != 0)
		{
			char *tmp=(char*)xmalloc(strlen(fn_root(finfo->fullname))+sizeof(" has been modified; revert changes? ")+100);
			sprintf(tmp,"%s has been modified; revert changes? ",fn_root(finfo->fullname));
			if (yesno_prompt(tmp,"Modified file",0)!='y')
			{
				/* "no".  */
				xfree (basefilename);
				xfree(tmp);
				return 0;
			}
			xfree(tmp);
		}
		if(revert_only && !gzip)
			copy_file(basefilename, entdata->user,1,1);
		else
			rename_file (basefilename, entdata->user);
	}
	else
		do_unedit=false;

	xfree (basefilename);

	if(!revert_only)
	{
		fp = open_file (CVSADM_NOTIFY, "a");

		fprintf (fp, "U%s\t%s GMT\t%s\t%s\t\t%s\t\t%s\t%s\n", entdata->user, global_session_time, hostname, CurDir, entdata->edit_tag?entdata->edit_tag:"", entdata->edit_bugid?entdata->edit_bugid:"", message?message:"");

		if (fclose (fp) < 0)
		{
			if (finfo->update_dir[0] == '\0')
				error (0, errno, "cannot close %s", CVSADM_NOTIFY);
			else
				error (0, errno, "cannot close %s/%s", finfo->update_dir,
				CVSADM_NOTIFY);
		}
	}

	cvs::filename fn = entdata->user;
	if(isfile(fn.c_str()))
	{
		/* Now update the revision number in CVS/Entries from CVS/Baserev.
		The basic idea here is that we are reverting to the revision
		that the user edited.  If we wanted "cvs update" to update
		CVS/Base as we go along (so that an unedit could revert to the
		current repository revision), we would need:

		update (or all send_files?) (client) needs to send revision in
		new Entry-base request.  update (server/local) needs to check
		revision against repository and send new Update-base response
		(like Update-existing in that the file already exists.  While
		we are at it, might try to clean up the syntax by having the
		mode only in a "Mode" response, not in the Update-base itself).  */

		if(do_unedit)
		{
			Register (finfo->entries, entdata->user, (entdata->edit_revision&&*entdata->edit_revision)?entdata->edit_revision:entdata->version, entdata->timestamp,
				entdata->options, (entdata->edit_tag&&*entdata->edit_tag)?(strcmp(entdata->edit_tag,"HEAD")?entdata->edit_tag:NULL):entdata->tag, entdata->date,
				entdata->conflict, NULL, NULL, entdata->rcs_timestamp, revert_only?entdata->edit_revision:NULL, revert_only?entdata->edit_tag:NULL, revert_only?entdata->edit_revision:NULL, entdata->md5);
		}
		else
		{
			/* No unedit was done...  remove the (bogus) edit information only */
			Register (finfo->entries, entdata->user, entdata->version, entdata->timestamp,
				entdata->options, entdata->tag, entdata->date,
				entdata->conflict, NULL, NULL, entdata->rcs_timestamp, NULL, NULL, NULL, entdata->md5);
		}
	}

	/* Note entdata is most likely invalid at this point as Register() changes the entry list */
    xchmod (fn.c_str(), keep_write);
    return 0;
}

static const char *const unedit_usage[] =
{
	"Usage: %s %s [-lRwy] [-r] [-u user] [-b bug] [-m message] [files...]\n",
	"\t-b <bugid>\tUnedit only files related to bug\n",
    "\t-l\t\tLocal directory only, not recursive\n",
	"\t-m <message>\tSpecify reason for unedit\n",
	"\t-r\t\tRevert file only, don't unedit\n",
    "\t-R\t\tProcess directories recursively\n",
    "\t-u <username>\tUnedit other user (repository administrators only)\n",
	"\t-w\t\tLeave file writable after unedit\n",
	"\t-y\t\tForce unedit of modified file\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int unedit (int argc, char **argv)
{
    int local = 0;
    int c;
    int err;

    if (argc == -1)
	usage (unedit_usage);

    optind = 0;
	while ((c = getopt (argc, argv, "+lRu:wrb:m:y")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'u':
		if(notify_user)
			error(1,0,"Can only specify -u once.");
		if(server_active && !supported_request("NotifyUser"))
			error(1,0,"Remote server does not support unediting other users");
		  notify_user = xstrdup(optarg);
		  local = 1;
		break;
		case 'w':
			keep_write = 1;
			break;
		case 'r':
			revert_only = 1;
			break;
		case 'b':
			if(!RCS_check_bugid(optarg,true))
				error(1,0,"Invalid characters in bug identifier.  Please avoid \"'");
			if(bugid.size())
				bugid+=",";
			bugid +=optarg;
			break;
		case 'y':
			force_unedit = 1;
			break;
		case 'm':
			message = xstrdup(optarg);
			break;
	    case '?':
	    default:
		usage (unedit_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* No need to readlock since we aren't doing anything to the
       repository.  */
    err = start_recursion (unedit_fileproc, (FILESDONEPROC) NULL,
			   (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
			   0, verify_write, NULL);

    err += send_notifications (argc, argv, local);
	xfree(notify_user);

    return err;
}

void mark_up_to_date (const char *file)
{
    char *base;

	if(commit_keep_edits)
		return;

    /* The file is up to date, so we better get rid of an out of
       date file in CVSADM_BASE.  */
    base = (char*)xmalloc (strlen (file) + 80);
    strcpy (base, CVSADM_BASE);
    strcat (base, "/");
    strcat (base, file);
    if (unlink_file (base) < 0 && ! existence_error (errno))
		error (0, errno, "cannot remove %s", file);
	strcat(base,".gz");
    if (unlink_file (base) < 0 && ! existence_error (errno))
		error (0, errno, "cannot remove %s", file);
    xfree (base);
}

static void editor_set (char type, const char *filename, const char *editor, const char *time, const char *hostname, const char *pathname, const char *tag, const char *flags, const char *bugid, const char *message, const char *repository)
{
	CXmlNodePtr  handle,ehandle;

	TRACE(2,"editor_set(%c,%s,%s,%s,%s,%s)",type,PATCH_NULL(filename),PATCH_NULL(editor),PATCH_NULL(time),PATCH_NULL(hostname),PATCH_NULL(pathname));
	if(type=='C' || type=='U')
	{
		handle = fileattr_getroot();
		handle->xpathVariable("name", filename);
		if(handle->Lookup("file[cvs:filename(@name,$name)]") && handle->XPathResultNext())
		{
			ehandle = handle->Clone();
			ehandle->xpathVariable("name",editor);
			if(ehandle->Lookup("editor[cvs:username(@name,$name)]"))
			while(ehandle->XPathResultNext())
			    fileattr_delete_child(handle,ehandle);
			fileattr_prune(handle);
		}
		history_write('u',pathname,tag,filename,repository,bugid,message);
	}
	else
	{
		handle = fileattr_getroot();
		handle->xpathVariable("name", filename);
		handle->xpathVariable("user",editor);
		if(!handle->Lookup("file[cvs:filename(@name,$name)]") || !handle->XPathResultNext())
		{
			handle = fileattr_getroot();
			handle->NewNode("file");
			handle->NewAttribute("name",filename);
		}
		if(!handle->Lookup("editor[cvs:username(@name,$user)]") || !handle->XPathResultNext())
		{
			handle->NewNode("editor");
			handle->NewAttribute("name",editor);
		}

		if(tag && *tag)
			fileattr_setvalue(handle,"tag",tag);
		fileattr_setvalue(handle,"time",time);
		fileattr_setvalue(handle,"hostname",hostname);
		fileattr_setvalue(handle,"pathname",fn_root(pathname));
		if(bugid && *bugid)
			fileattr_setvalue(handle,"bugid",bugid);
		if(message && *message)
			fileattr_setvalue(handle,"message",message);
		if(flags && flags[0]=='X')
			fileattr_setvalue(handle,"exclusive",NULL);
		history_write('e',pathname,tag,filename,repository,bugid,message);
	}
}

struct notify_proc_args {
    /* What kind of notification, "edit", "tedit", etc.  */
    const char *type;
    /* User who is running the command which causes notification.  */
    const char *who;
	/* Time of notification */
	const char *date;
	/* Tag/Branch being edited */
	const char *tag;
	/* Bug id */
	const char *bugid;
	/* Unedit message */
	const char *message;
    /* User to be notified.  */
    const char *notifyee;
    /* File.  */
    const char *file;
	/* Repository */
	const char *repository;
};

static int notify_proc(void *params, const trigger_interface *cb)
{
    notify_proc_args *args = (notify_proc_args *)params;
	int ret = 0;

	if(cb->notify)
	{
	    const char *srepos;

	    srepos = Short_Repository (args->repository);
		ret = cb->notify(cb,args->message,args->bugid,srepos,args->notifyee,args->tag,args->type?args->type:"",args->file);
	}
	return ret;
}

int check_can_edit(const char *repository, const char *filename, const char *who, const char *tag)
{
	if(!verify_write(repository,filename,tag,NULL,NULL))
		return 1;

	CXmlNodePtr handle = fileattr_getroot();
	handle->xpathVariable("name",filename);
	if(!handle->Lookup("file[cvs:filename(@name,$name)]/editor") || !handle->XPathResultNext())
	    handle=NULL;

	while(handle)
	{
		const char *username=fileattr_getvalue(handle,"@name");
		const char *edittag = fileattr_getvalue(handle,"tag");
		int exclusive = fileattr_find(handle,"exclusive")==NULL?0:1;

		if(exclusive)
		{
			if(!edittag)
				return 1; /* Exclusive lock on whole file */

			if(!tag)
				return 1; /* Someone else has an exclusive lock, we can't get a file lock */

			if(!strcmp(edittag,tag) && usercmp(who,username))
				return 1; /* Someone else has an exclusive lock on this branch */
		}

		if(!handle->XPathResultNext())
		  handle=NULL;
	}
	return 0;
}

int notify_do (int type, const char *filename, const char *who, const char *date,
			   const char *hostname, const char *pathname, const char *watches, 
			   const char *repository, const char *tag, const char *flags, 
			   const char *bugid, const char *message)
{
	CXmlNodePtr  filehandle;
	struct addremove_args args = {0};

	TRACE(3,"notify_do (%c, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
		type,
		PATCH_NULL(filename), PATCH_NULL(who), PATCH_NULL(date), PATCH_NULL(hostname),
		PATCH_NULL(pathname), PATCH_NULL(watches), PATCH_NULL(repository), PATCH_NULL(tag),
		PATCH_NULL(flags), PATCH_NULL(bugid), PATCH_NULL(message));

    switch (type)
    {
	case 'E':
		if(check_can_edit(repository,filename,who,tag))
		{
			error(0,0,"Edit on file '%s' refused by server", filename);
			return 1;
		}
	    editor_set (type, filename, who, date,hostname,pathname,tag,flags,bugid,message,repository);
	    break;
	case 'U':
	case 'C':
	    editor_set (type, filename, who, date,hostname,pathname,tag,flags,bugid,message,repository);
	    break;
	default:
		error(0,0,"Unknown notify '%c' ignored",type);
	    return 0;
    }

	filehandle = fileattr_getroot();
	filehandle->xpathVariable("name",filename);
	if(!filehandle->Lookup("file[cvs:filename(@name,$name)]/watcher") || !filehandle->XPathResultNext())
	  filehandle = NULL;

	while(filehandle)
	{
		const char *username = fileattr_getvalue(filehandle,"@name");
		const char *notif = NULL;

	    /* Don't notify user of their own changes.  Would perhaps
	       be better to check whether it is the same working
	       directory, not the same user, but that is hairy.  */
		if(username && usercmp(username,CVS_Username))
		{
	    if (type == 'E' && fileattr_find(filehandle,"edit"))
			notif = "edit";
	    else if(type == 'U' && fileattr_find(filehandle,"unedit"))
			notif = "unedit";
	    else if (type == 'C' && fileattr_find(filehandle,"commit"))
			notif = "commit";
	    else if (type == 'E' && fileattr_find(filehandle,"temp_edit"))
			notif = "temporary edit";
	    else if(type == 'U' && fileattr_find(filehandle,"temp_unedit"))
			notif = "temporary unedit";
	    else if (type == 'C' && fileattr_find(filehandle,"temp_commit"))
			notif = "temporary commit";
		}

		if (notif != NULL)
		{
			struct notify_proc_args args;
			size_t user_len = strlen(username);
			FILE *fp;
			char *usersname;
			char *line = NULL;
			size_t line_len = 0;

			args.notifyee = NULL;
			usersname = (char*)xmalloc (strlen (current_parsed_root->directory)
					+ sizeof CVSROOTADM
					+ sizeof CVSROOTADM_USERS
					+ 20);
			strcpy (usersname, current_parsed_root->directory);
			strcat (usersname, "/");
			strcat (usersname, CVSROOTADM);
			strcat (usersname, "/");
			strcat (usersname, CVSROOTADM_USERS);
			fp = CVS_FOPEN (usersname, "r");
			if (fp == NULL && !existence_error (errno))
				error (0, errno, "cannot read %s", usersname);
			if (fp != NULL)
			{
			while (getline (&line, &line_len, fp) >= 0)
			{
				if (strncmp (line, username, user_len) == 0 && line[user_len] == ':')
				{
					char *cp;

					args.notifyee = xstrdup (line + user_len + 1);

								/* There may or may not be more
								colon-separated fields added to this in the
								future; in any case, we ignore them right
								now, and if there are none we make sure to
								chop off the final newline, if any. */
					cp = (char*)strpbrk (args.notifyee, ":\n");

					if (cp != NULL)
						*cp = '\0';
					break;
				}
			}
			if (ferror (fp))
				error (0, errno, "cannot read %s", usersname);
			if (fclose (fp) < 0)
				error (0, errno, "cannot close %s", usersname);
			}
			xfree (usersname);
			if (line != NULL)
			xfree (line);

			if (args.notifyee == NULL)
				args.notifyee = xstrdup(username);

			args.type = notif;
			args.who = who;
			args.file = filename;
			args.date = date?date:"";
			args.bugid = bugid?bugid:"";
			args.message = message?message:"";
			args.tag = tag?tag:"";
			args.repository = repository?repository:"";

			TRACE(3,"run notify trigger 1");
			run_trigger(&args,notify_proc);
			xfree (args.notifyee);
		}

		if(!filehandle->XPathResultNext())
		  filehandle = NULL;
	}

	if(global_watcher)
	{
		struct notify_proc_args args;
		const char *notif;

	    if (type == 'E')
			notif = "edit";
	    else if(type == 'U')
			notif = "unedit";
	    else if (type == 'C')
			notif = "commit";

		args.notifyee = global_watcher;
		args.type = notif;
		args.who = who;
		args.file = filename;
		args.date = date?date:"";
		args.bugid = bugid?bugid:"";
		args.message = message?message:"";
		args.tag = tag?tag:"";
		args.repository = repository?repository:"";

		TRACE(3,"run notify trigger 2");
		run_trigger(&args, notify_proc);
	}

    switch (type)
    {
		case 'E':
			args.add_tedit = 1;
			args.add_tunedit = 1;
			args.add_tcommit = 1;
			args.adding=1;
		    watch_modify_watchers (filename, who, &args);
			break;
		case 'U':
		case 'C':
			args.remove_temp = 1;
			args.adding=0;
			watch_modify_watchers (filename, who, &args);
			break;
    }
	return 0;
}

/* Check and send notifications.  This is only for the client.  */
void client_notify_check (char *repository, char *update_dir)
{
    FILE *fp;
    char *line = NULL;
    size_t line_len = 0;

    if (! server_started)
	/* We are in the midst of a command which is not to talk to
	   the server (e.g. the first phase of a cvs edit).  Just chill
	   out, we'll catch the notifications on the flip side.  */
	return;

    /* We send notifications even if noexec.  I'm not sure which behavior
       is most sensible.  */

    fp = CVS_FOPEN (CVSADM_NOTIFY, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", CVSADM_NOTIFY);
	return;
    }
    while (getline (&line, &line_len, fp) > 0)
    {
	int notif_type;
	char *filename;
	char *val;
	char *cp;

	notif_type = line[0];
	if (notif_type == '\0')
	    continue;
	filename = line + 1;
	cp = strchr (filename, '\t');
	if (cp == NULL)
	    continue;
	*cp++ = '\0';
	val = cp;

	client_notify (repository, update_dir, filename, notif_type, val, notify_user);
    }
    if (line)
	xfree (line);
    if (ferror (fp))
	error (0, errno, "cannot read %s", CVSADM_NOTIFY);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", CVSADM_NOTIFY);

    /* Leave the CVSADM_NOTIFY file there, until the server tells us it
       has dealt with it.  */
}


static const char *const editors_usage[] =
{
    "Usage: %s %s [-aclRv] [files...]\n",
	"\t-a\tShow all branches.\n",
	"\t-c\tCheck whether edit is valid on file.\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
	"\t-v\tShow bugs.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static int editors_fileproc (void *callerdat, struct file_info *finfo)
{
	if(check_edit)
	{
		Node *n = findnode_fn(finfo->entries,finfo->file);
		if(n && n->data)
		{
			Entnode *ent = (Entnode*)n->data;

			if(check_can_edit(finfo->repository,finfo->file,CVS_Username,ent->tag))
				error(1,0,"Edit on file '%s' refused by server", finfo->file);
		}
	}
    return editors_output (finfo);
}

int editors (int argc, char **argv)
{
    int local = 0;
    int c;

    if (argc == -1)
		usage (editors_usage);

	if(!strcmp(command_name,"editors-edit"))
		check_edit = 1;

    optind = 0;
    while ((c = getopt (argc, argv, "+lRavc")) != -1)
    {
		switch (c)
		{
			case 'a':
				all_editors = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'l':
				local = 1;
				break;
			case 'R':
				local = 0;
				break;
			case 'c':
				check_edit = 1;
				break;
			case '?':
			default:
				usage (editors_usage);
				break;
		}
    }
    argc -= optind;
    argv += optind;

    if (current_parsed_root->isremote)
    {
		if (local)
			send_arg ("-l");
		if(all_editors)
			send_arg ("-a");
		send_arg("--");
		send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
		send_file_names (argc, argv, SEND_EXPAND_WILD);
		send_to_server ("editors\n", 0);
		return get_responses_and_close ();
    }

    return start_recursion (editors_fileproc, (FILESDONEPROC) NULL,
			    (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			    argc, argv, local, W_LOCAL, 0, 1, (char *)NULL, NULL,
			    0, verify_read, NULL);
}
