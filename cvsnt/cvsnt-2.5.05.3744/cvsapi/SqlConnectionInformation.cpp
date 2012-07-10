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
#include <config.h>
#include "lib/api_system.h"
#include "cvs_string.h"
#include "ServerIO.h"
#include "SqlConnectionInformation.h"

/*
CSqlConnectionInformation::CSqlConnectionInformation()
{
}

CSqlConnectionInformation::~CSqlConnectionInformation()
{
}
*/

const char *CSqlConnectionInformation::getVariable(const char *name)
{
	if(!name) return NULL;

	if(!strcmp(name,"hostname"))
		return hostname.c_str();
	else if(!strcmp(name,"database"))
		return database.c_str();
	else if(!strcmp(name,"username"))
		return username.c_str();
	else if(!strcmp(name,"password"))
		return password.c_str();
	return NULL;
}

bool CSqlConnectionInformation::setVariable(const char *name, const char *value)
{
	if(!name) return false;
	if(!value) value="";

	if(!strcmp(name,"hostname"))
		hostname=value;
	else if(!strcmp(name,"database"))
		database=value;
	else if(!strcmp(name,"username"))
		username=value;
	else if(!strcmp(name,"password"))
		password=value;
	return false;
}

const char *CSqlConnectionInformation::enumVariableNames(size_t nVar)
{
	switch(nVar)
	{
		case 0: return "hostname";
		case 1: return "database";
		case 2: return "username";
		case 3: return "password";
		default:
			return NULL;
	}
}

bool CSqlConnectionInformation::connectionDialog(const void *parentWindow)
{
	return false;
}
