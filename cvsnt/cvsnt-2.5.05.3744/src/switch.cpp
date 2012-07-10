/*
 * Copyright (c) 2010, Tony Hoyle and March Hare Software Ltd
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Modify the CVS/Root files in a sandbox
 */

#include "cvs.h"

static const char *const switch_usage[] =
{
    "Usage: %s %s [-f] newroot\n",
	"\t-f\tChange all roots, not just those matching the current directory.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

bool force;

static bool switch_recurse(cvsroot *old_root, cvsroot *new_root, cvs::string& dirname);

int cvsswitch(int argc, char **argv)
{
    int c;
    int err = 0;

	force = false;

    if (argc == -1)
		usage (switch_usage);

    optind = 0;
    while ((c = getopt (argc, argv, "+f")) != -1)
    {
	switch (c)
	{
	case 'f':
		force = true;
		break;
    case '?':
    default:
		usage (switch_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

	if(argc!=1)
		usage(switch_usage);

	cvsroot *old_root = current_parsed_root;

	// Client code should have caught this before we get here, but just in case...
	if(!old_root)
		error(1,0,"This command must be issued from within a sandbox");

	cvsroot *new_root = parse_cvsroot(argv[0]);
	
	// Return of NULL from above means new root doesn't parse.  User has been notified.
	if(!new_root)
		return 1;


	if(!strcmp(old_root->original,new_root->original))
	{
		free_cvsroot_t(new_root);
		return 0; // Roots are the same.. nothing to do
	}

	cvs::string dirname;
	dirname.reserve(128);
	switch_recurse(old_root,new_root,dirname);

	free_cvsroot_t(new_root);

	return 0;
}

bool switch_recurse(cvsroot *old_root, cvsroot *new_root, cvs::string& dirname)
{
	cvs::string line;
	CFileAccess acc;

	if(!acc.open("CVS/Root","r"))
		return true; // Not a CVS directory
	acc.getline(line);
	acc.close();

	if(!force && strcmp(line.c_str(),old_root->original))
		return true; // Not part of the current sandbox

	if(!quiet)
		error(0,0,"%s",dirname.size()?dirname.c_str():".");

	if(!acc.open("CVS/Root","w"))
	{
		error(0,0,"Unable to modify CVS/Root");
		return false;
	}
	acc.putline(new_root->original);
	acc.close();

	CDirectoryAccess dir;
	DirectoryAccessInfo inf;

	if(!dir.open("."))
		error(1,0,"Unable to open directory");

	while(dir.next(inf))
	{
		if(inf.isdir)
		{
			if(strcmp(inf.filename.c_str(),".") && strcmp(inf.filename.c_str(),"..") && strcmp(inf.filename.c_str(),"CVS"))
			{
				size_t s = dirname.size();
				if(s) dirname+='/';
				dirname+=inf.filename.c_str();
				CDirectoryAccess::chdir(inf.filename.c_str());
				switch_recurse(old_root,new_root,dirname);
				CDirectoryAccess::chdir("..");
				dirname.resize(s);
			}
		}
	}

	dir.close();

	return 0;
}
