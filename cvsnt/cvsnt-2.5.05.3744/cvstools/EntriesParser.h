/*
	CVSNT Helper application API
    Copyright (C) 2004-6 Tony Hoyle and March-Hare Software Ltd

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
#ifndef ENTRIESPARSER__H
#define ENTRIESPARSER__H

class CEntriesParser
{
public:
	struct entries_t
	{
		char type;
		cvs::filename filename;
		cvs::string version;
		cvs::string date;
		cvs::string expansion;
		cvs::string tag;
	};

	CVSTOOLS_EXPORT CEntriesParser();
	CVSTOOLS_EXPORT virtual ~CEntriesParser();

	CVSTOOLS_EXPORT bool IsCvsControlledDirectory(const char *Directory);
	CVSTOOLS_EXPORT bool Load(const char *Directory);
	CVSTOOLS_EXPORT bool Exists(const char *filename);
	CVSTOOLS_EXPORT bool GetEntry(const char *filename, const entries_t*& entry);
	CVSTOOLS_EXPORT bool Unload();

protected:
	std::map<cvs::filename,entries_t> m_fileMap;

	bool ParseEntriesLine(cvs::string& line, entries_t& ent);
};

#endif

