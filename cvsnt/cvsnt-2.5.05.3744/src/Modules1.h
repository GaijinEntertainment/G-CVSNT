/*	CVS Recursion processor
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef MODULES1__H
#define MODULES1__H

/* All modules break down to this structure eventually */
struct Module1List_t
{
	Module1List_t() { }
	virtual ~Module1List_t() { }
	cvs::filename module;
	cvs::filename directory;
	std::vector<cvs::filename> modules;
	std::vector<cvs::filename> ignore;
	std::vector<cvs::filename> files;
};

class CModules1
{
public:
	CModules1();
	virtual ~CModules1();

	bool ReadModules1();
	void Translate(CvsBasicEntry& item, std::vector<CvsBasicEntry>& list);

protected:
	typedef std::map<cvs::filename,Module1List_t> modlist_t;
	static bool m_bModules1Loaded;
	static modlist_t m_ModList;

	CvsDirectoryEntry& AddDirectory(CvsDirectoryEntry& root, const char *directory);
};

#endif