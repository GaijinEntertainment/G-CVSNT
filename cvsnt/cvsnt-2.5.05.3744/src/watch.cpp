/* Implementation for "cvs watch add", "cvs watchers", and related commands

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "edit.h"
#include "fileattr.h"
#include "watch.h"

const char *const watch_usage[] =
{
    "Usage: %s %s [on|off|ro|rw|add|remove] [-lR] [-a action] [files...]\n",
    "on/off/readonly/readwrite: turn on/off read-only checkouts of files\n",
    "add/remove: add or remove notification on actions\n",
    "-l (on/off/ro/rw/add/remove): Local directory only, not recursive\n",
    "-R (on/off/ro/rw/add/remove): Process directories recursively\n",
    "-a (add/remove): Specify what actions, one of\n",
    "    edit,unedit,commit,all,none\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static struct addremove_args the_args;

void watch_modify_watchers (const char *file, const char *who, struct addremove_args *what)
{
	CXmlNodePtr  filehandle;
	
	TRACE(3,"watch_modify_watchers(%s,%s)",PATCH_NULL(file),PATCH_NULL(who));
	filehandle = fileattr_getroot();
	filehandle->xpathVariable("name",file?file:"");
	filehandle->xpathVariable("user",who?who:"");
	if(file)
		if(!filehandle->Lookup("file[cvs:filename(@name,$name)]")) filehandle=NULL;
	else
		if(!filehandle->Lookup("directory/default")) filehandle=NULL;

	if(filehandle && !filehandle->XPathResultNext()) filehandle = NULL;
	
	if(filehandle && (!filehandle->Lookup("watcher[cvs:username(@name,$user)]") || !filehandle->XPathResultNext()))
	  filehandle=NULL;

	if(filehandle && !what->adding)
		return;
	if(!filehandle)
	{
		if(file)
		{
			filehandle = fileattr_getroot();
			filehandle->xpathVariable("name",file);
			if(!filehandle->Lookup("file[cvs:filename(@name,$name)]") || !filehandle->XPathResultNext())
			{
				filehandle=fileattr_getroot();
				filehandle->NewNode("file");
				filehandle->NewAttribute("name",file);
			}
		}
		else
		{
			filehandle = fileattr_getroot();
			if(!filehandle->Lookup("directory/default") || !filehandle->XPathResultNext())
			{
				filehandle=fileattr_getroot();
				filehandle->NewNode("directory");
				filehandle->NewNode("default");
			}
		}

		// We already know that these don't exist, from the search above
		filehandle->NewNode("watcher");
		filehandle->NewAttribute("name",who);
	}
	if(!filehandle)
		error(0,0,"Couldn't create node in modify_watchers");
	
	if(!what->adding)
	{
		if(what->edit)
			fileattr_delete(filehandle,"edit");
		if(what->commit)
			fileattr_delete(filehandle,"commit");
		if(what->unedit)
			fileattr_delete(filehandle,"unedit");
		if(what->remove_temp)
		{
			fileattr_delete(filehandle,"temp_edit");
			fileattr_delete(filehandle,"temp_commit");
			fileattr_delete(filehandle,"temp_unedit");
		}
		fileattr_prune(filehandle);
	}
	else
	{
		if(what->edit)
			filehandle->NewNode("edit",NULL,false);
		if(what->commit)
			filehandle->NewNode("commit",NULL,false);
		if(what->unedit)
			filehandle->NewNode("unedit",NULL,false);
		if(what->add_tedit)
			filehandle->NewNode("temp_edit",NULL,false);
		if(what->add_tcommit)
			filehandle->NewNode("temp_commit",NULL,false);
		if(what->add_tunedit)
			filehandle->NewNode("temp_unedit",NULL,false);
	}
}

static int addremove_fileproc(void *callerdat,
				      struct file_info *finfo)
{
    watch_modify_watchers (finfo->file, CVS_Username, &the_args);
    return 0;
}

static int addremove_filesdoneproc (void *callerdat, int err, char *repository,
    char *update_dir, List *entries)
{
    if (the_args.setting_default)
		watch_modify_watchers (NULL, CVS_Username, &the_args);
    return err;
}

static int watch_addremove(int argc, char **argv)
{
    int c;
    int local = 0;
    int err;
    int a_omitted;

    a_omitted = 1;
    the_args.commit = 0;
    the_args.edit = 0;
    the_args.unedit = 0;
    optind = 0;
    while ((c = getopt (argc, argv, "+lRa:")) != -1)
    {
	switch (c)
	{
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'a':
		a_omitted = 0;
		if (strcmp (optarg, "edit") == 0)
		    the_args.edit = 1;
		else if (strcmp (optarg, "unedit") == 0)
		    the_args.unedit = 1;
		else if (strcmp (optarg, "commit") == 0)
		    the_args.commit = 1;
		else if (strcmp (optarg, "all") == 0)
		{
		    the_args.edit = 1;
		    the_args.unedit = 1;
		    the_args.commit = 1;
		}
		else if (strcmp (optarg, "none") == 0)
		{
		    the_args.edit = 0;
		    the_args.unedit = 0;
		    the_args.commit = 0;
		}
		else
		    usage (watch_usage);
		break;
	    case '?':
	    default:
		usage (watch_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    if (a_omitted)
    {
	the_args.edit = 1;
	the_args.unedit = 1;
	the_args.commit = 1;
    }

    if (current_parsed_root->isremote)
    {
	if (local)
	    send_arg ("-l");
	/* FIXME: copes poorly with "all" if server is extended to have
	   new watch types and client is still running an old version.  */
	if (the_args.edit)
	{
	    send_arg ("-a");
	    send_arg ("edit");
	}
	if (the_args.unedit)
	{
	    send_arg ("-a");
	    send_arg ("unedit");
	}
	if (the_args.commit)
	{
	    send_arg ("-a");
	    send_arg ("commit");
	}
	if (!the_args.edit && !the_args.unedit && !the_args.commit)
	{
	    send_arg ("-a");
	    send_arg ("none");
	}
	send_arg("--");
	send_files (argc, argv, local, 0, SEND_NO_CONTENTS);
	send_file_names (argc, argv, SEND_EXPAND_WILD);
	send_to_server (the_args.adding ?
                        "watch-add\n" : "watch-remove\n",
                        0);
	return get_responses_and_close ();
    }

    the_args.setting_default = (argc <= 0);

    lock_tree_for_write (argc, argv, local, W_LOCAL, 0);

    err = start_recursion (addremove_fileproc, addremove_filesdoneproc,
			   (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			   argc, argv, local, W_LOCAL, 0, 0, (char *)NULL, NULL,
			   1, verify_write, NULL);

    Lock_Cleanup ();
    return err;
}

int watch_add(int argc, char **argv)
{
    the_args.adding = 1;
    return watch_addremove (argc, argv);
}

int watch_remove (int argc, char **argv)
{
    the_args.adding = 0;
    return watch_addremove (argc, argv);
}

int watch (int argc, char **argv)
{
    if (argc <= 1)
	usage (watch_usage);
    if (!strcmp(argv[1], "on") || !strcmp(argv[1],"ro"))
    {
	--argc;
	++argv;
	return watch_on (argc, argv);
    }
    else if(!strcmp(argv[1], "off") || !strcmp(argv[1],"rw"))
    {
	--argc;
	++argv;
	return watch_off (argc, argv);
    }
    else if (strcmp (argv[1], "add") == 0)
    {
	--argc;
	++argv;
	return watch_add (argc, argv);
    }
    else if (strcmp (argv[1], "remove") == 0)
    {
	--argc;
	++argv;
	return watch_remove (argc, argv);
    }
    else
	usage (watch_usage);
    return 0;
}

static const char *const watchers_usage[] =
{
    "Usage: %s %s [-lR] [files...]\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static int watchers_fileproc (void *callerdat, struct file_info *finfo)
{
	CXmlNodePtr  handle;
	const char *name;

	handle = fileattr_getroot();
	handle->xpathVariable("name",finfo->file);
	if(!handle->Lookup("file[cvs:filename(@name,$name)]/watcher") || !handle->XPathResultNext())
	  handle = NULL;

	if(!handle)
		return 0;

    cvs_output (fn_root(finfo->fullname), 0);

    while (handle)
    {
		cvs_output ("\t", 1);
		name=fileattr_getvalue(handle,"@name");
		cvs_output(name,0);
		if(fileattr_find(handle,"edit"))
			cvs_output("\tedit",0);
		if(fileattr_find(handle,"unedit"))
			cvs_output("\tunedit",0);
		if(fileattr_find(handle,"commit"))
			cvs_output("\tcommit",0);
		if(fileattr_find(handle,"temp_edit"))
			cvs_output("\ttedit",0);
		if(fileattr_find(handle,"temp_unedit"))
			cvs_output("\ttunedit",0);
		if(fileattr_find(handle,"temp_commit"))
			cvs_output("\ttcommit",0);
	    cvs_output ("\n", 1);
		if(!handle->XPathResultNext())
		   handle = NULL;
	}

    return 0;
}

int watchers (int argc, char **argv)
{
    int local = 0;
    int c;

    if (argc == -1)
	usage (watchers_usage);

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
		usage (watchers_usage);
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
	send_to_server ("watchers\n", 0);
	return get_responses_and_close ();
    }

    return start_recursion (watchers_fileproc, (FILESDONEPROC) NULL,
			    (PREDIRENTPROC) NULL, (DIRENTPROC) NULL, (DIRLEAVEPROC) NULL, NULL,
			    argc, argv, local, W_LOCAL, 0, 1, (char *)NULL, NULL,
			    1, verify_read, NULL);
}
