/*
	CVSNT Generic API
    Copyright (C) 2006 Tony Hoyle and March-Hare Software Ltd

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
#ifndef SQLCONNECTIONINFORMATION__H
#define SQLCONNECTIONINFORMATION__H

#include "cvs_smartptr.h"

class CSqlConnectionInformation
{
public:
	static const int firstDriverVariable=4;

	CSqlConnectionInformation() { }
	virtual ~CSqlConnectionInformation() { }

	virtual CVSAPI_EXPORT const char *getVariable(const char *name);
	virtual CVSAPI_EXPORT bool setVariable(const char *name, const char *value);
	virtual CVSAPI_EXPORT const char *enumVariableNames(size_t nVar); 

	virtual CVSAPI_EXPORT bool connectionDialog(const void *parentWindow);

protected:
	cvs::string hostname;
	cvs::string database;
	cvs::string username;
	cvs::string password;
};

#endif

