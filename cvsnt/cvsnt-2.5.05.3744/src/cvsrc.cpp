/*
 * Copyright (c) 1993 david d zuhn
 * 
 * Written by david d `zoo' zuhn while at Cygnus Support
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 *
 */


#include "cvs.h"
#include "getline.h"

/* this file is to be found in the user's home directory */

#ifndef	CVSRC_FILENAME
#define	CVSRC_FILENAME	".cvsrc"
#endif
char cvsrc[] = CVSRC_FILENAME;

#define	GROW	10

#ifndef _WIN32
extern char *strtok (char *, const char *);
#endif

/* old_argc and old_argv hold the values returned from the
    previous invocation of read_cvsrc and are used to free the
    allocated memory.  The first invocation of read_cvsrc gets argv
    from the system, this memory must not be freed.  */
static int old_argc = 0;
static char **old_argv = NULL;

/* Read cvsrc, processing options matching CMDNAME ("cvs" for global
   options, and update *ARGC and *ARGV accordingly.  */

void read_cvsrc (int *argc, char ***argv, const char *cmdname)
{
    char *homedir;
    char *homeinit;
    FILE *cvsrcfile;

    char *line = NULL, *server_line = NULL;
    int line_length;
    size_t line_chars_allocated;

    char *optstart;

    int command_len;
    int found = 0, server_found = 0;

    int i;

    int new_argc;
    int max_new_argv;
    char **new_argv;

	int len;
	char *buffer = NULL, *ptr;

    /* don't do anything if argc is -1, since that implies "help" mode */
    if (*argc == -1)
		return;

	command_len = strlen (cmdname);

#ifdef SERVER_SUPPORT
	/* Attempt to read server cvsrc */
	if (server_started)
	{
		if(supported_request("read-cvsrc2")) /* Sensible version */
		{
			TRACE(1,"Requesting server cvsrc (read-cvsrc2)");
			send_to_server ("read-cvsrc2\n", 0);
			read_line(&server_line);
			if(server_line[0]=='E' && server_line[1]==' ')
			{
				fprintf (stderr, "%s\n", server_line + 2);
				error_exit();
			}
			len = atoi(server_line);
			buffer = (char*)xmalloc(len+1);
			ptr = buffer;
		    while (len > 0)
			{
				size_t n;

			    n = try_read_from_server (ptr, len);
				len -= n;
			    ptr += n;
			}
			*ptr = '\0';
		}
		else if(supported_request("read-cvsrc")) /* Version done after one too many beers */
		{
			TRACE(1,"Requesting server cvsrc (read-cvsrc)");
			send_to_server ("read-cvsrc\n", 0);
			buffer = (char*)xmalloc(1);
			len = 0;
			do
			{
				read_line(&server_line);
				if(server_line[0]=='E' && server_line[1]==' ')
				{
					fprintf (stderr, "%s\n", server_line + 2);
					error_exit();
				}
				if(!strcmp(server_line,"end-cvsrc"))
					break;
				buffer = (char*)xrealloc(buffer, len+strlen(server_line)+2);
				strcpy(buffer+len,server_line);
				len+=strlen(server_line);
				buffer[len++]='\n';
			} while(1);
			buffer[len]='\0';
		}
	}
	else
#endif
	if(current_parsed_root && (server_active || !current_parsed_root->hostname))
	{
		homeinit = (char *) xmalloc (strlen (current_parsed_root->directory) + sizeof(CVSROOTADM) + sizeof(CVSROOTADM_CVSRC) + 10);
		strcpy (homeinit, current_parsed_root->directory);
		strcat (homeinit, "/");
		strcat (homeinit, CVSROOTADM);
		strcat (homeinit, "/");
		strcat (homeinit, CVSROOTADM_CVSRC);

		TRACE(1,"Reading global cvsrc");

		/* if it can't be read, there's no point to continuing */
		if (isreadable (homeinit))
		{
			cvsrcfile = open_file (homeinit, "r");
			fseek(cvsrcfile,0,SEEK_END);
			len = ftell(cvsrcfile);
			fseek(cvsrcfile,0,SEEK_SET);
			buffer = (char*)xmalloc(len+1);
			len = fread(buffer,1,len,cvsrcfile);
			buffer[len]='\0';
			fclose(cvsrcfile);
		}
		xfree(homeinit);
	}

	if(buffer)
	{
		/* now scan the file until we find the line for the command in question */

		TRACE(3,"Parsing global cvsrc started");

		ptr = strtok(buffer, "\n");
		while(ptr && *ptr && !server_found)
		{
			TRACE(3,"%s",PATCH_NULL(ptr));
			/* skip over comment lines */
			if (ptr[0] == '#')
			{
				ptr = strtok(NULL,"\n");
				continue;
			}

			/* stop if we match the current command */
			if (!strncmp (ptr, cmdname, command_len)
				&& isspace ((unsigned char) *(ptr + command_len)))
			{
				server_found = 1;
				server_line = xstrdup(ptr);
				break;
			}
			ptr = strtok(NULL,"\n");
		}
		xfree(buffer);
		TRACE(3,"Parsing global cvsrc finished");
	}

    /* determine filename for ~/.cvsrc */

    homedir = get_homedir ();
    /* If we can't find a home directory, ignore ~/.cvsrc.  This may
       make tracking down problems a bit of a pain, but on the other
       hand it might be obnoxious to complain when CVS will function
       just fine without .cvsrc (and many users won't even know what
       .cvsrc is).  */
    if(homedir)
	{
		homeinit = (char *) xmalloc (strlen (homedir) + strlen (cvsrc) + 10);
		strcpy (homeinit, homedir);
		strcat (homeinit, "/");
		strcat (homeinit, cvsrc);

		/* if it can't be read, there's no point to continuing */

		if (isreadable (homeinit))
		{
			/* now scan the file until we find the line for the command in question */

			line = NULL;
			line_chars_allocated = 0;
			cvsrcfile = open_file (homeinit, "r");
			while ((line_length = getline (&line, &line_chars_allocated, cvsrcfile))
			>= 0)
			{
			/* skip over comment lines */
			if (line[0] == '#')
				continue;

			/* stop if we match the current command */
			if (!strncmp (line, cmdname, command_len)
				&& isspace ((unsigned char) *(line + command_len)))
			{
				found = 1;
				break;
			}
			}

			if (line_length < 0 && !feof (cvsrcfile))
			error (0, errno, "cannot read %s", homeinit);

			fclose (cvsrcfile);
		}
	    xfree (homeinit);
	}

    /* setup the new options list */

    new_argc = 1;
    max_new_argv = (*argc) + GROW;
    new_argv = (char **) xmalloc (max_new_argv * sizeof (char*));
    new_argv[0] = xstrdup ((*argv)[0]);

    if (server_found)
    {
		/* skip over command in the options line */
		for (optstart = strtok (server_line + command_len, "\t \n");
			optstart;
			optstart = strtok (NULL, "\t \n"))
		{
			new_argv [new_argc++] = xstrdup (optstart);
		  
			if (new_argc >= max_new_argv)
			{
				max_new_argv += GROW;
				new_argv = (char **) xrealloc (new_argv, max_new_argv * sizeof (char*));
			}
		}
    }

	if (found)
    {
		/* skip over command in the options line */
		for (optstart = strtok (line + command_len, "\t \n");
			optstart;
			optstart = strtok (NULL, "\t \n"))
		{
			new_argv [new_argc++] = xstrdup (optstart);
		  
			if (new_argc >= max_new_argv)
			{
				max_new_argv += GROW;
				new_argv = (char **) xrealloc (new_argv, max_new_argv * sizeof (char*));
			}
		}
    }

    if (line != NULL)
		xfree (line);
	if (server_line != NULL)
		xfree (server_line);

    /* now copy the remaining arguments */
  
    if (new_argc + *argc > max_new_argv)
    {
		max_new_argv = new_argc + *argc;
		new_argv = (char **) xrealloc (new_argv, max_new_argv * sizeof (char*));
    }
    for (i=1; i < *argc; i++)
    {
		new_argv [new_argc++] = xstrdup ((*argv)[i]);
    }

    if (old_argv != NULL)
    {
		/* Free the memory which was allocated in the previous
			read_cvsrc call.  */
		free_names (&old_argc, old_argv);
    }

	old_argc = *argc = new_argc;
	old_argv = *argv = new_argv;

    return;
}

int close_cvsrc()
{
	if(old_argv != NULL)
		free_names(&old_argc, old_argv);
	old_argv = NULL;
	return 0;
}

void reset_saved_cvsrc()
{
	old_argc = 0;
	old_argv = NULL;
}
