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
#ifndef MODULES2__H
#define MODULES2__H

struct Module2List_t
{
	cvs::filename directory;
	cvs::string translation;
	cvs::string regexp;
	bool recurse;
	bool no_retranslate;
};

class CModules2
{
public:
	CModules2();
	virtual ~CModules2();

	bool ReadModules2();
	void Translate(CvsBasicEntry& item, std::vector<CvsBasicEntry>& list);

protected:
	typedef std::map<cvs::filename, std::vector<Module2List_t> > modlist_t;
	static bool m_bModules2Loaded;
	static modlist_t m_ModList;
};

#endif