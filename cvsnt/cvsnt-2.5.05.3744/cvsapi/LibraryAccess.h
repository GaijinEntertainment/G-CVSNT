/*
	CVSNT Generic API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

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
#ifndef LIBRARYACCESS__H
#define LIBRARYACCESS__H

class CLibraryAccess
{
public:
	CVSAPI_EXPORT CLibraryAccess(const void *lib = NULL);
	CVSAPI_EXPORT virtual ~CLibraryAccess();

	CVSAPI_EXPORT bool Load(const char *name, const char *directory);
	CVSAPI_EXPORT bool Unload();
	CVSAPI_EXPORT void *GetProc(const char *name);
	CVSAPI_EXPORT void *Detach();
	static CVSAPI_EXPORT void VerifyTrust(const char *module, bool must_exist);

protected:
	void *m_lib;
};

#endif
