/*
	CVSNT Helper application API
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
#include <config.h>
#include <cvsapi.h>
#include "export.h"
#include "Cvsgui.h"
#include "../cvsgui/cvsgui.h"
#include "../cvsgui/cvsgui_protocol.h"

bool CCvsgui::Init(int& argc, char **&argv)
{
	if(argc<4)
		return false;
	if(strcmp(argv[1],"-cvsgui"))
		return false;
	cvsguiglue_init(argv[2],argv[3]);
	char *p=argv[0];
	argc-=3;
	argv+=3;
	argv[0]=p;
	return true;
}

bool CCvsgui::Active()
{
	if(_cvsgui_writefd>0)
		return true;
	return false;
}

bool CCvsgui::Close(int result)
{
	cvsguiglue_close(result);
	return true;
}

int CCvsgui::write(const char *buf, int len, bool isStderr, bool binary)
{
	return gp_console_write(_cvsgui_writefd,buf,len,isStderr?1:0,binary?1:0);
}


