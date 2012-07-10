/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/* _EXPORT */
#ifndef FILEENTRY__H
#define FILEENTRY__H

#include <map>

#include "cvs_string.h"
#include "cvs_smartptr.h"

struct CvsBasicEntry
{
	typedef std::map<cvs::filename,int> filelist_t;
	CvsBasicEntry() { ignored=false; parse_modules1=parse_modules2=parse_physical=true; isdot=false; old_revision=revision=object_id=0; }
	virtual ~CvsBasicEntry() { }
	cvs::filename logical_name; // Name as seen by user
	cvs::filename physical_name; // Relative path to repository
	cvs::filename module_name; // Module source name
	cvs::string regexp; // Directory regexp
	filelist_t filelist; // File list, if specified
	bool parse_modules1; // reparse with modules1
	bool parse_modules2; // reparse with modules2
	bool parse_physical; // reparse with physical directory
	bool isdot; // Is '.' directory for old code compatibility
	char type; // Entry type

	cvs::string tag;
	cvs::string date;
	bool nonbranch; // tag is non-branch tag

	size_t old_revision; // Current revision before parsing
	size_t revision; // Current revision after parsing
	cvs::filename old_filename; // Renamed_from if rename within directory

	size_t object_id;

	bool ignored; // Used for modules
};

/*
struct CvsDirectoryEntry : public CvsBasicEntry
{
	typedef std::map<cvs::filename,cvs::smartptr<CvsDirectoryEntry> > children_t;
	virtual ~CvsDirectoryEntry() { }

	children_t children;
};

struct CvsFileEntry : public CvsBasicEntry
{
	virtual ~CvsFileEntry() { }

	cvs::string expansion_options;
	cvs::string merge_tag1;
	cvs::string merge_tag2;
	time_t commit_date;
	cvs::string edit_revision;
	cvs::string edit_tag;
	cvs::string bugid;
};
*/

#endif

