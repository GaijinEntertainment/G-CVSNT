/*
	CVSNT Generic API
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
/* Unix specific */
#include <config.h>
#include "../lib/api_system.h"
#include "../cvs_string.h"
#include "../SSPIHandler.h"

#include <sys/socket.h>
#include <unistd.h>

CSSPIHandler::CSSPIHandler()
{
}

CSSPIHandler::~CSSPIHandler()
{
}

bool CSSPIHandler::init(const char *protocol /* = NULL */)
{
	/* sspi_protocol has an implementation of this at least for
	   NTLMv1.  Could also use winbind. */
	return false; /* Not yet implemented for unix */
}

bool CSSPIHandler::ClientStart(bool encrypt, bool& more, const char *name, const char *pwd, const char *tokenSource /* = NULL */)
{
	return false;
}

bool CSSPIHandler::ClientStep(bool& more, const char *inputBuffer, size_t inputSize)
{
	return false;
}
