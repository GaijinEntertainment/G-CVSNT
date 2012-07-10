/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 */
#include "../version.h"

#include "cvs.h"
#include "getline.h"

static int find_type(Node * p, void *closure);
static int fmt_proc(Node * p, void *closure);
static int rcsinfo_proc(void *params, const trigger_interface *cb);
static int title_proc(Node * p, void *closure);
static int update_logfile_proc (void *params, const trigger_interface *cb);
static void setup_tmpfile(FILE * xfp, char *xprefix, List * changes);
static int verifymsg_proc(void *params, const trigger_interface *cb);

static FILE *fp;
static char *str_list;
static char *str_list_format;	/* The format for str_list's contents. */
static Ctype type;
static change_info *loginfo_array;
static int loginfo_count, loginfo_size;

int reread_log_after_verify = 0; /* Default to old behaviour */

struct rcsinfo_param_t
{
	const char *directory;
	const char *message;
};

struct verifymsg_param_t
{
	const char *directory;
	const char *filename;
};

struct loginfo_param_t
{
	const char *message;
	FILE *logfp;
	List *changes;
	const char *directory;
};

/*
 * Puts a standard header on the output which is either being prepared for an
 * editor session, or being sent to a logfile program.  The modified, added,
 * and removed files are included (if any) and formatted to look pretty. */
static char *prefix;
static int col;
static char *tag;
static void
setup_tmpfile (FILE *xfp, char *xprefix, List *changes)
{
    /* set up statics */
    fp = xfp;
    prefix = xprefix;

    type = T_MODIFIED;
    if (walklist (changes, find_type, NULL) != 0)
    {
	fprintf (fp, "%sModified Files:\n", prefix);
	col = 0;
	walklist (changes, fmt_proc, NULL);
	fprintf (fp, "\n");
	if (tag != NULL)
	{
	    xfree (tag);
	    tag = NULL;
	}
    }
    type = T_ADDED;
    if (walklist (changes, find_type, NULL) != 0)
    {
	fprintf (fp, "%sAdded Files:\n", prefix);
	col = 0;
	walklist (changes, fmt_proc, NULL);
	fprintf (fp, "\n");
	if (tag != NULL)
	{
	    xfree (tag);
	    tag = NULL;
	}
    }
    type = T_REMOVED;
    if (walklist (changes, find_type, NULL) != 0)
    {
	fprintf (fp, "%sRemoved Files:\n", prefix);
	col = 0;
	walklist (changes, fmt_proc, NULL);
	fprintf (fp, "\n");
	if (tag != NULL)
	{
	    xfree (tag);
	    tag = NULL;
	}
    }
}

static int array_proc (Node *p, void *closure)
{
    struct logfile_info *li;
	static const char change_type[] = { '?','C','G','M','O','A','R','W',' ','P','T' };

    li = (struct logfile_info *) p->data;
    if (li->type == type)
    {
		int pos = loginfo_count++;
		if(pos==loginfo_size)
		{
			if(loginfo_size<64) loginfo_size=64;
			else loginfo_size*=2;
			loginfo_array=(change_info *)xrealloc(loginfo_array,loginfo_size*sizeof(change_info));
		}
		loginfo_array[pos].filename=p->key;
		loginfo_array[pos].rev_new=li->rev_new;
		loginfo_array[pos].rev_old=li->rev_old;
		loginfo_array[pos].tag=li->tag;
		loginfo_array[pos].type=change_type[li->type-1];
		loginfo_array[pos].bugid=li->bugid;
	}
	return 0;
}

void setup_arrays(char *xprefix, List *changes)
{
    /* set up statics */
	loginfo_count=loginfo_size=0;
    prefix = xprefix;

    type = T_TITLE;
	walklist (changes, array_proc, NULL);
    type = T_MODIFIED;
	walklist (changes, array_proc, NULL);
    type = T_ADDED;
	walklist (changes, array_proc, NULL);
    type = T_REMOVED;
	walklist (changes, array_proc, NULL);
}

/*
 * Looks for nodes of a specified type and returns 1 if found
 */
static int
find_type (Node *p, void *closure)
{
    struct logfile_info *li;

    li = (struct logfile_info *) p->data;
    if (li->type == type)
	return (1);
    else
	return (0);
}

/*
 * Breaks the files list into reasonable sized lines to avoid line wrap...
 * all in the name of pretty output.  It only works on nodes whose types
 * match the one we're looking for
 */
static int fmt_proc (Node *p, void *closure)
{
    struct logfile_info *li;

    li = (struct logfile_info *) p->data;
    if (li->type == type)
    {
        if (li->tag == NULL
	    ? tag != NULL
	    : tag == NULL || strcmp (tag, li->tag) != 0)
	{
	    if (col > 0)
	        fprintf (fp, "\n");
	    fprintf (fp, "%s", prefix);
	    col = strlen (prefix);
	    while (col < 6)
	    {
	        fprintf (fp, " ");
		++col;
	    }

	    if (li->tag == NULL)
	        fprintf (fp, "No tag");
	    else
	        fprintf (fp, "Tag: %s", li->tag);

	    if (tag != NULL)
	        xfree (tag);
	    tag = xstrdup (li->tag);

	    /* Force a new line.  */
	    col = 70;
	}

	if (col == 0)
	{
	    fprintf (fp, "%s\t", prefix);
	    col = 8;
	}
	else if (col > 8 && (col + (int) strlen (p->key)) > 70)
	{
	    fprintf (fp, "\n%s\t", prefix);
	    col = 8;
	}
	{
		char *tmp;
		fprintf (fp, "%s ", shell_escape(&tmp,p->key));
		col += strlen (tmp) + 1;
		xfree(tmp);
	}
    }
    return (0);
}

/*
 * Builds a temporary file using setup_tmpfile() and invokes the user's
 * editor on the file.  The header garbage in the resultant file is then
 * stripped and the log message is stored in the "message" argument.
 * 
 * If REPOSITORY is non-NULL, process rcsinfo for that repository; if it
 * is NULL, use the CVSADM_TEMPLATE file instead.
 */
void do_editor (const char *dir, char **messagep, const char *repository, List *changes)
{
    static int reuse_log_message = 0;
    char *line;
    int line_length;
    size_t line_chars_allocated;
    char *fname;
    struct stat pre_stbuf, post_stbuf;
    int retcode = 0;

    if (noexec || reuse_log_message)
		return;

    /* Abort creation of temp file if no editor is defined */
    if (strcmp (Editor, "") == 0)
		error(1, 0, "no editor defined, must use -e or -m");

	/* Create a temporary file */
	/* FIXME - It's possible we should be relying on cvs_temp_file to open
	* the file here - we get race conditions otherwise.
	*/
	fname = cvs_temp_name ();
again:
	if ((fp = CVS_FOPEN (fname, "w+")) == NULL)
		error (1, 0, "cannot create temporary file %s", fname);

	if (*messagep)
	{
		fprintf (fp, "%s", *messagep);

		if ((*messagep)[0] == '\0' ||
			(*messagep)[strlen (*messagep) - 1] != '\n')
			fprintf (fp, "\n");
	}

    if (repository != NULL)
	{
		rcsinfo_param_t args;
		args.directory = Short_Repository(repository);
		args.message=NULL;
		/* tack templates on if necessary */
		TRACE(3,"run rcsinfo trigger");
		if(!run_trigger(&args,rcsinfo_proc) && args.message)
		{
			fprintf(fp,"%s",args.message);
			if (args.message[0] == '\0' || args.message[strlen(args.message) - 1] != '\n')
				fprintf (fp, "\n");
		}
	}
    else
    {
		FILE *tfp;
		char buf[1024];
		size_t n;
		size_t nwrite;

		/* Why "b"?  */
		tfp = CVS_FOPEN (CVSADM_TEMPLATE, "rb");
		if (tfp == NULL)
		{
			if (!existence_error (errno))
			error (1, errno, "cannot read %s", CVSADM_TEMPLATE);
		}
		else
		{
			while (!feof (tfp))
			{
				char *p = buf;
				n = fread (buf, 1, sizeof buf, tfp);
				nwrite = n;
				while (nwrite > 0)
				{
					n = fwrite (p, 1, nwrite, fp);
					nwrite -= n;
					p += n;
				}
				if (ferror (tfp))
					error (1, errno, "cannot read %s", CVSADM_TEMPLATE);
			}
			if (fclose (tfp) < 0)
				error (0, errno, "cannot close %s", CVSADM_TEMPLATE);
		}
	}

	fprintf (fp,"%s----------------------------------------------------------------------\n",CVSEDITPREFIX);
#ifdef CVSSPAM
#ifdef _WIN32
#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
	if (strcasecmp(CVSNT_SPECIAL_BUILD,"Suite")!=0)
#endif
	{
	fprintf (fp,"%s     Committed on the Free edition of March Hare Software CVSNT Server\n",CVSEDITPREFIX);
	fprintf (fp,"%s     Upgrade to CVS Suite for more features and support:\n",CVSEDITPREFIX);
	fprintf (fp,"%s           http://march-hare.com/cvsnt/\n",CVSEDITPREFIX);
	fprintf (fp,"%s----------------------------------------------------------------------\n",CVSEDITPREFIX);
    }
#endif
#endif
	fprintf (fp,"%sEnter Log.  Lines beginning with `%.*s' are removed automatically\n%s\n",
				CVSEDITPREFIX, CVSEDITPREFIXLEN, CVSEDITPREFIX,CVSEDITPREFIX);
	if (dir != NULL && *dir)
		fprintf (fp, "%sCommitting in %s\n%s\n", CVSEDITPREFIX,	dir, CVSEDITPREFIX);
	if (changes != NULL)
		setup_tmpfile (fp, CVSEDITPREFIX, changes);
	fprintf (fp,"%s----------------------------------------------------------------------\n",
				CVSEDITPREFIX);

	/* finish off the temp file */
	if (fclose (fp) == EOF)
		error (1, errno, "%s", fname);
	if ( CVS_STAT (fname, &pre_stbuf) == -1)
		pre_stbuf.st_mtime = 0;

	/* run the editor */
	run_setup (Editor);
	run_arg (fname);
	if ((retcode = run_exec (true)) != 0)
		error (0, retcode == -1 ? errno : 0, "warning: editor session failed");

	/* put the entire message back into the *messagep variable */
	fp = open_file (fname, "r");

	if (*messagep)
		xfree (*messagep);

	if ( CVS_STAT (fname, &post_stbuf) != 0)
		error (1, errno, "cannot find size of temp file %s", fname);

	if (post_stbuf.st_size == 0)
		*messagep = NULL;
	else
	{
		/* On NT, we might read less than st_size bytes, but we won't
		read more.  So this works.  */
		*messagep = (char *) xmalloc (post_stbuf.st_size + 1);
 		*messagep[0] = '\0';
	}

	line = NULL;
	line_chars_allocated = 0;

	if (*messagep)
	{
		size_t message_len = post_stbuf.st_size + 1;
		size_t offset = 0;
		while (1)
		{
			line_length = getline (&line, &line_chars_allocated, fp);
			if (line_length == -1)
			{
				if (ferror (fp))
					error (0, errno, "warning: cannot read %s", fname);
				break;
			}
			if (strncmp (line, CVSEDITPREFIX, CVSEDITPREFIXLEN) == 0)
				continue;
			if (offset + line_length >= message_len)
			expand_string (messagep, &message_len, offset + line_length + 1);
			strcpy (*messagep + offset, line);
			offset += line_length;
		}
	}
	
	if (fclose (fp) < 0)
		error (0, errno, "warning: cannot close %s", fname);

	if (pre_stbuf.st_mtime == post_stbuf.st_mtime || *messagep == NULL || strcmp (*messagep, "\n") == 0)
	{
		for (;;)
		{
			printf ("\nLog message unchanged or not specified\n");
			printf ("a)bort, c)ontinue, e)dit, !)reuse this message unchanged for remaining dirs\n");
			printf ("Action: (continue) ");
			fflush (stderr);
			fflush (stdout);
			line_length = getline (&line, &line_chars_allocated, stdin);
			if (line_length < 0)
			{
				error (0, errno, "cannot read from stdin");
				if (unlink_file (fname) < 0)
					error (0, errno, "warning: cannot remove temp file %s", fname);
				error (1, 0, "aborting");
			}
			else if (line_length == 0 || *line == '\n' || *line == 'c' || *line == 'C')
				break;
			if (*line == 'a' || *line == 'A')
			{
				if (unlink_file (fname) < 0)
					error (0, errno, "warning: cannot remove temp file %s", fname);
				error (1, 0, "aborted by user");
			}
			if (*line == 'e' || *line == 'E')
				goto again;
			if (*line == '!')
			{
				reuse_log_message = 1;
				break;
			}
			printf ("Unknown input\n");
		}
    }
    if (line)
		xfree (line);
    if (unlink_file (fname) < 0)
		error (0, errno, "warning: cannot remove temp file %s", fname);
    xfree (fname);
}

/* Runs the user-defined verification script as part of the commit or import 
   process.  This verification is meant to be run whether or not the user 
   included the -m atribute.  unlike the do_editor function, this is 
   independant of the running of an editor for getting a message.
 */
int do_verify (char **message, const char *repository)
{
    FILE *fp;
    char *fname = NULL;
    int retcode = 0;
    int len;
	//char boughtsuite[100];

    if (current_parsed_root->isremote)
	/* The verification will happen on the server.  */
		return 0;

    /* FIXME? Do we really want to skip this on noexec?  What do we do
       for the other administrative files?  */
    if (noexec)
		return 0;
	
    /* If there's no message, then we have nothing to verify.  Can this
       case happen?  And if so why would we print a message?  */
    if (message == NULL)
    {
		cvs_output ("No message to verify\n", 0);
		return 0;
    }

    /* open a temporary file, write the message to the 
       temp file, and close the file.  */

    if ((fp = cvs_temp_file (&fname)) == NULL)
		error (1, errno, "cannot create temporary file %s", fname);
    else
    {
		fprintf (fp, "%s", *message);
		if ((*message)[0] == '\0' ||
			(*message)[strlen (*message) - 1] != '\n')
			fprintf (fp, "%s", "\n");
		if (fclose (fp) == EOF)
			error (1, errno, "%s", fname);

		/* Run the verify message  */

		if (repository != NULL)
		{
			verifymsg_param_t args;
			args.directory = Short_Repository(repository);
			args.filename = fname;

			TRACE(3,"run verifymsg trigger");
			retcode = run_trigger(&args,verifymsg_proc);

			if(!retcode && reread_log_after_verify)
			{
				if((fp=fopen(fname,"r"))==NULL)
					error(1, errno, "Couldn't reread message file");
				fseek(fp,0,SEEK_END);
				len = ftell(fp);
				fseek(fp,0,SEEK_SET);
				*message = (char*)xmalloc(len+1);
				if(!*message)
					error(1,errno,"Out of memory rereading message");
				len = fread(*message, 1, len, fp);
				if(len < 0)
					error(1, errno, "Couldn't reread message file");
				(*message)[len]='\0';
				if (fclose (fp) == EOF)
	    				error (1, errno, "Couldn't close %s", fname);
			}
#ifdef CVSSPAM
#ifdef _WIN32
#if (CVSNT_SPECIAL_BUILD_FLAG != 0)
	if (strcasecmp(CVSNT_SPECIAL_BUILD,"Suite")!=0)
#endif
	{
			if (message!=NULL)
			{
				if (strstr(*message,"Committed on the Free edition of March Hare Software CVSNT")==NULL)
				{
				    if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","HaveBoughtSuite",boughtsuite,sizeof(boughtsuite)))
						strcpy(boughtsuite,"no");
					if (strcasecmp(boughtsuite,"yes"))
					{
						len=strlen(*message);
						*message=(char *)xrealloc(*message,len+400);
						strcat(*message,"\nCommitted on the Free edition of March Hare Software CVSNT Server.\nUpgrade to CVS Suite for more features and support:\nhttp://march-hare.com/cvsnt/");
					}
				}
			}
    }
#endif
#endif
		}

		/* Delete the temp file  */

		if (unlink_file (fname) < 0)
			error (0, errno, "cannot remove %s", fname);
		xfree (fname);
    }
	return retcode;
}

/*
 * callback proc for Parse_Info for rcsinfo templates this routine basically
 * copies the matching templat onto the end of the tempfile we are setting
 * up
 */
static int rcsinfo_proc(void *params, const trigger_interface *cb)
{
	rcsinfo_param_t *args = (rcsinfo_param_t *)params;
	int ret = 0; 

	TRACE(1,"rcsinfo_proc(%s)",PATCH_NULL(args->directory));
	if(cb->get_template)
	{
		ret = cb->get_template(cb,args->directory,&args->message);
	}
	return ret;
}

/*
 * Uses setup_tmpfile() to pass the updated message on directly to any
 * logfile programs that have a regular expression match for the checked in
 * directory in the source repository.  The log information is fed into the
 * specified program as standard input.
 */
void Update_Logfile (const char *repository, const char *xmessage, FILE *xlogfp, List *xchanges, const char *xbugid)
{
    /* nothing to do if the list is empty */
    if (xchanges == NULL || xchanges->list->next == xchanges->list)
		return;

	loginfo_param_t args;
	args.message = xmessage;
	args.logfp=xlogfp;
	args.changes=xchanges;
	args.directory=Short_Repository(repository);

    /* call Parse_Info to do the actual logfile updates */
	TRACE(3,"run loginfo trigger");
	run_trigger(&args, update_logfile_proc);
}

/*
 * callback proc to actually do the logfile write from Update_Logfile
 */
static int update_logfile_proc (void *params, const trigger_interface *cb)
{
	int ret = 0;
	loginfo_param_t *args = (loginfo_param_t*)params;

	if(cb->loginfo)
	{
		char *status=NULL;

		if (args->logfp)
		{
			off_t len;
			fseek(args->logfp,0,SEEK_END);
			len=ftell(args->logfp);
			status = (char*)xmalloc(len+1);
			rewind (args->logfp);
			len = fread(status,1,len,args->logfp);
			if(len<0)
				xfree(status);
			else
				status[len]='\0';
		}
		setup_arrays("",args->changes);
		ret = cb->loginfo(cb,args->message,status,args->directory,loginfo_count,loginfo_array);
		xfree(status);
		xfree(loginfo_array);
	}
	return ret;
}

/*
 * concatenate each filename/version onto str_list
 */
static int title_proc (Node *p, void *closure)
{
    struct logfile_info *li;
    char *c;
    char *escaped_file;

    li = (struct logfile_info *) p->data;
    if (li->type == type)
    {
	/* Until we decide on the correct logging solution when we add
	   directories or perform imports, T_TITLE nodes will only
	   tack on the name provided, regardless of the format string.
	   You can verify that this assumption is safe by checking the
	   code in add.c (add_directory) and import.c (import). */

	str_list = (char*)xrealloc (str_list, strlen (str_list) + 5);
	strcat (str_list, " ");

	shell_escape_backslash(&escaped_file,p->key);

	if (li->type == T_TITLE)
	{
	    str_list = (char*)xrealloc (str_list,
				 strlen (str_list) + strlen (escaped_file) + 5);
		strcat (str_list, escaped_file);
	}
	else
	{
	    /* All other nodes use the format string. */

	    for (c = str_list_format; *c != '\0'; c++)
	    {
		switch (*c)
		{
		case 's':
		    str_list =
			(char*)xrealloc (str_list,
				  strlen (str_list) + strlen (escaped_file) + 5);
		    strcat (str_list, escaped_file);
		    break;
		case 'V':
		    str_list =
			(char*)xrealloc (str_list,
				  (strlen (str_list)
				   + (li->rev_old ? strlen (li->rev_old) : 0)
				   + 10)
				  );
		    strcat (str_list, (li->rev_old
					      ? li->rev_old : "NONE"));
		    break;
		case 'v':
		    str_list =
			(char*)xrealloc (str_list,
				  (strlen (str_list)
				   + (li->rev_new ? strlen (li->rev_new) : 0)
				   + 10)
				  );
		    strcat (str_list, (li->rev_new
					      ? li->rev_new : "NONE"));
		    break;
		/* All other characters, we insert an empty field (but
		   we do put in the comma separating it from other
		   fields).  This way if future CVS versions add formatting
		   characters, one can write a loginfo file which at least
		   won't blow up on an old CVS.  */
		}
		if (*(c + 1) != '\0')
		{
		    str_list = (char*)xrealloc (str_list, strlen (str_list) + 5);
		    strcat (str_list, ",");
		}
	    }
	}
	xfree(escaped_file);
    }
    return (0);
}

static int verifymsg_proc (void *params, const trigger_interface *cb)
{
	verifymsg_param_t *args = (verifymsg_param_t*)params;
	int ret = 0;

	TRACE(1,"verifymsg_proc(%s,%s)",PATCH_NULL(args->directory),PATCH_NULL(args->filename));
	if(cb->verifymsg)
		ret = cb->verifymsg(cb,args->directory,args->filename);
	return ret;
}
