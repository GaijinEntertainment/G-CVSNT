/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * trigger library (c) 2004-5 Tony Hoyle and March-Hare Software Ltd.
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 */

#include "cvs.h"
#include "getline.h"
#include "../version.h"

extern char *logHistory;

bool tf_loaded;

static std::vector<const char *> uservar,userval;

int run_trigger(void *param, CALLPROC callproc)
{
	int ret = 0;
	cvs::string infopath;
	FILE *f;
    char *line = NULL, *cp;
    size_t line_allocated = 0;
	int line_number = 0;

	TRACE(1,"run_trigger()");

    if (current_parsed_root == NULL)
    {
		/* XXX - should be error maybe? */
		error (0, 0, "CVSROOT variable not set");
		return 1;
    }

	CTriggerLibrary lib;
	if(!tf_loaded)
	{
#ifdef _WIN32
		/* We do this here so it's done before the CRT in the DLL/COM object initialises */
		if(server_active && server_io_socket)
		{
			SetStdHandle(STD_INPUT_HANDLE,(HANDLE)_get_osfhandle(server_io_socket));
			SetStdHandle(STD_OUTPUT_HANDLE,(HANDLE)_get_osfhandle(server_io_socket));
			SetStdHandle(STD_ERROR_HANDLE,(HANDLE)_get_osfhandle(server_io_socket));
		}
#endif

		int count_uservar = variable_list.size();
		uservar.resize(count_uservar);
		userval.resize(count_uservar);
		size_t n=0;
		for(variable_list_t::const_iterator i = variable_list.begin(); i!=variable_list.end(); ++i, n++)
		{
			uservar[n]=i->first.c_str();
			userval[n]=i->second.c_str();
		}
		const char *codep;
#ifdef _WIN32
		if(win32_global_codepage==CP_UTF8)
			codep="UTF-8";
		else
#endif
			codep=CCodepage::GetDefaultCharset();

		const char *cv;
		if(server_active)
			cv = serv_client_version;
		else
			cv = "CVSNT "CVSNT_PRODUCTVERSION_STRING;

		cvs::sprintf(infopath,512,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSROOTADM_TRIGGERS);
		if((f=fopen(infopath.c_str(),"r"))==NULL)
		{
			if(!existence_error(errno))
				error(0,errno,"Couldn't read triggers file '%s'",fn_root(infopath.c_str()));
			TRACE(3,"trigger file not found.");
		}

		TRACE(3,"LoadTrigger info trigger, phys repos=%s.",(current_parsed_root)?PATCH_NULL(current_parsed_root->directory):"no current_parsed_root");
		if (strcmp((current_parsed_root->method)?current_parsed_root->method:"","sync")==0)
		{
		TRACE(3,"LoadTrigger info trigger, phys repos is proxy=%s.",(current_parsed_root)?PATCH_NULL(current_parsed_root->proxy_repository_root):"no current_parsed_root");
		if(!lib.LoadTrigger("info"SHARED_LIBRARY_EXTENSION,server_active?server_command_name:command_name,global_session_time,remote_host_name?remote_host_name:hostname,CVS_Username,current_parsed_root->unparsed_directory,current_parsed_root->proxy_repository_root,global_session_id,Editor,count_uservar,count_uservar?&uservar[0]:NULL,count_uservar?&userval[0]:NULL,cv,codep))
		{
			error(1,errno,"Couldn't open default trigger library");
		}
		}
		else
		{
		if(!lib.LoadTrigger("info"SHARED_LIBRARY_EXTENSION,server_active?server_command_name:command_name,global_session_time,remote_host_name?remote_host_name:hostname,CVS_Username,current_parsed_root->unparsed_directory,current_parsed_root->directory,global_session_id,Editor,count_uservar,count_uservar?&uservar[0]:NULL,count_uservar?&userval[0]:NULL,cv,codep))
		{
			error(1,errno,"Couldn't open default trigger library");
		}
		}

		CDirectoryAccess da;
		DirectoryAccessInfo inf;

		da.open(CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDTriggers),"*"SHARED_LIBRARY_EXTENSION);
		while(da.next(inf))
		{
			cvs::filename fn = inf.filename;
			fn=fn.substr(fn.rfind('/')+1);
			if(!lib.LoadTrigger(fn.c_str(),server_active?server_command_name:command_name,
				global_session_time,remote_host_name?remote_host_name:hostname,
				CVS_Username,current_parsed_root->unparsed_directory,
				current_parsed_root->directory,global_session_id,Editor,
				count_uservar,count_uservar?&uservar[0]:NULL,count_uservar?&userval[0]:NULL,cv,codep))
			{
				TRACE(3,"Couldn't load trigger %s",fn.c_str());
			}
		}
		da.close();

		if(f)
		{
			while (getline (&line, &line_allocated, f) >= 0)
			{
				line_number++;

				/* skip lines starting with # */
				if (line[0] == '#')
					continue;

				/* Strip whitespace off the end */
				for (cp = line+strlen(line)-1; cp>=line && isspace(*cp);--cp)
					*cp='\0';

				/* skip whitespace at beginning of line */
				for (cp = line; *cp && isspace ((unsigned char) *cp); cp++)
					;

				/* if *cp is null, the whole line was blank */
				if (*cp == '\0')
					continue;

				const trigger_interface *cb = lib.LoadTrigger(cp,server_active?server_command_name:command_name,
					global_session_time,remote_host_name?remote_host_name:hostname,CVS_Username,
					current_parsed_root->unparsed_directory,current_parsed_root->directory,
					global_session_id,Editor,count_uservar,&uservar[0],&userval[0],cv,codep);
				if(!cb)
					error(0,0,"Couldn't open trigger library '%s'",fn_root(cp));
			}

			fclose(f);
		}
		tf_loaded = true;
	}

	bool first = true;
	const trigger_interface *trig;
	const char *name;
	while((trig=lib.EnumLoadedTriggers(first, name))!=NULL)
	{
		TRACE(1,"Call pre-loaded '%s'",name);
		ret += callproc(param,trig);
	}

	return ret;
}

/* Parse the CVS config file.  The syntax right now is a bit ad hoc
   but tries to draw on the best or more common features of the other
   *info files and various unix (or non-unix) config file syntaxes.
   Lines starting with # are comments.  Settings are lines of the form
   KEYWORD=VALUE.  There is currently no way to have a multi-line
   VALUE (would be nice if there was, probably).

   CVSROOT is the $CVSROOT directory (current_parsed_root->directory might not be
   set yet).

   Returns 0 for success, negative value for failure.  Call
   error(0, ...) on errors in addition to the return value.  */
int parse_config (const char *cvsroot)
{
    char *infopath;
    FILE *fp_info;
    char *line = NULL;
    size_t line_allocated = 0;
    size_t len;
    char *p;
    /* FIXME-reentrancy: If we do a multi-threaded server, this would need
       to go to the per-connection data structures.  */
    static int parsed = 0;

    /* Authentication code and serve_root might both want to call us.
       Let this happen smoothly.  */
    if (parsed)
	return 0;
    parsed = 1;

    infopath = (char*)xmalloc (strlen (cvsroot)
			+ sizeof (CVSROOTADM_CONFIG)
			+ sizeof (CVSROOTADM)
			+ 10);
    if (infopath == NULL)
    {
	error (0, 0, "E out of memory; cannot allocate infopath");
	goto error_return;
    }

    strcpy (infopath, cvsroot);
    strcat (infopath, "/");
    strcat (infopath, CVSROOTADM);
    strcat (infopath, "/");
    strcat (infopath, CVSROOTADM_CONFIG);

    fp_info = CVS_FOPEN (infopath, "r");
    if (fp_info == NULL)
    {
	/* If no file, don't do anything special.  */
	if (!existence_error (errno))
	{
	    /* Just a warning message; doesn't affect return
	       value, currently at least.  */
	    error (0, errno, "E cannot open %s", infopath);
	}
	xfree (infopath);
	return 0;
    }

    while (getline (&line, &line_allocated, fp_info) >= 0)
    {
	/* Skip comments.  */
	if (line[0] == '#')
	    continue;

	/* At least for the moment we don't skip whitespace at the start
	   of the line.  Too picky?  Maybe.  But being insufficiently
	   picky leads to all sorts of confusion, and it is a lot easier
	   to start out picky and relax it than the other way around.

	   Is there any kind of written standard for the syntax of this
	   sort of config file?  Anywhere in POSIX for example (I guess
	   makefiles are sort of close)?  Red Hat Linux has a bunch of
	   these too (with some GUI tools which edit them)...

	   Along the same lines, we might want a table of keywords,
	   with various types (boolean, string, &c), as a mechanism
	   for making sure the syntax is consistent.  Any good examples
	   to follow there (Apache?)?  */

	/* Strip the training newline.  There will be one unless we
	   read a partial line without a newline, and then got end of
	   file (or error?).  */

	len = strlen (line) - 1;
	if (line[len] == '\n')
	    line[len] = '\0';

	/* Skip blank lines.  */
	if (line[0] == '\0')
	    continue;

	/* The first '=' separates keyword from value.  */
	p = strchr (line, '=');
	if (p == NULL)
	{
	    /* Probably should be printing line number.  */
	    error (0, 0, "syntax error in %s: line '%s' is missing '='",
		   infopath, line);
	    goto error_return;
	}

	*p++ = '\0';

	if (strcasecmp (line, "RCSBIN") == 0)
	{
	    /* This option used to specify the directory for RCS
	       executables.  But since we don't run them any more,
	       this is a noop.  Silently ignore it so that a
	       repository can work with either new or old CVS.  */
	    ;
	}
	else if (strcasecmp (line, "SystemAuth") == 0)
	{
	    if (strcasecmp (p, "no") == 0)
			system_auth = 0;
	    else if (strcasecmp (p, "yes") == 0)
			system_auth = 1;
	    else
	    {
		error (0, 0, "unrecognized value '%s' for SystemAuth", p);
		goto error_return;
	    }
	}
	else if (strcasecmp (line, "PreservePermissions") == 0)
	{
	}
	else if (strcasecmp (line, "TopLevelAdmin") == 0)
	{
	    if (strcasecmp (p, "no") == 0)
		top_level_admin = 0;
	    else if (strcasecmp (p, "yes") == 0)
		top_level_admin = 1;
	    else
	    {
		error (0, 0, "unrecognized value '%s' for TopLevelAdmin", p);
		goto error_return;
	    }
	}
	else if (strcasecmp (line, "AclMode") == 0)
	{
	    if (strcasecmp (p, "none") == 0)
		acl_mode = 0;
	    else if (strcasecmp (p, "compat") == 0)
		acl_mode = 1;
	    else if (strcasecmp (p, "normal") == 0)
		acl_mode = 2;
	    else
	    {
			error (0, 0, "unrecognized value '%s' for AclMode", p);
			goto error_return;
	    }
	}
	else if (strcasecmp (line, "LockDir") == 0)
	{
	    if (lock_dir != NULL)
			xfree (lock_dir);
	    lock_dir = xstrdup (p);
	    /* Could try some validity checking, like whether we can
	       opendir it or something, but I don't see any particular
	       reason to do that now rather than waiting until lock.c.  */
	}
	else if (strcasecmp (line, "LockServer") == 0)
	{
	    xfree (lock_server);
	    if (strcasecmp (p, "none"))
	    	lock_server = xstrdup (p);
	}
	else if (strcasecmp (line, "LogHistory") == 0)
	{
	    if (strcasecmp (p, "all") != 0)
	    {
		logHistory=(char*)xmalloc(strlen (p) + 1);
		strcpy (logHistory, p);
	    }
	}
	else if (strcasecmp (line, "AtomicCommits") == 0)
	{
	    if (strcasecmp (p, "no") == 0)
			atomic_commits = 0;
	    else if (strcasecmp (p, "yes") == 0)
			atomic_commits = 1;
	    else
	    {
			error (0, 0, "unrecognized value '%s' for AtomicCommits", p);
			goto error_return;
	    }
	}
	else if (strcasecmp (line, "RereadLogAfterVerify") == 0)
	{
	    if (strcasecmp (p, "no") == 0 || strcasecmp (p,"never") == 0)
			reread_log_after_verify = 0;
	    else if (strcasecmp (p, "yes") == 0 || strcasecmp(p,"always") == 0 || strcasecmp(p,"stat") == 0 )
			reread_log_after_verify = 1;
	    else
	    {
			error (0, 0, "unrecognized value '%s' for RereadLogAfterVerify", p);
			goto error_return;
	    }
	}
	else if (strcasecmp (line, "Watcher") == 0)
	{
		global_watcher = xstrdup(p);
	}
	else
	{
	    /* We may be dealing with a keyword which was added in a
	       subsequent version of CVS.  In that case it is a good idea
	       to complain, as (1) the keyword might enable a behavior like
	       alternate locking behavior, in which it is dangerous and hard
	       to detect if some CVS's have it one way and others have it
	       the other way, (2) in general, having us not do what the user
	       had in mind when they put in the keyword violates the
	       principle of least surprise.  Note that one corollary is
	       adding new keywords to your CVSROOT/config file is not
	       particularly recommended unless you are planning on using
	       the new features.  */
	    error (0, 0, "E %s: unrecognized keyword '%s'",
		   infopath, line);
	    goto error_return;
	}
    }
    if (ferror (fp_info))
    {
		error (0, errno, "E cannot read %s", infopath);
		goto error_return;
    }
    if (fclose (fp_info) < 0)
    {
		error (0, errno, "E cannot close %s", infopath);
		goto error_return;
    }
    xfree (infopath);
	xfree (line);
    return 0;

 error_return:
	xfree (infopath);
	xfree (line);
    return -1;
}
