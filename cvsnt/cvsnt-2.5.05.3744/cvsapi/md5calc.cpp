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
#include <config.h>
#include "lib/api_system.h"
#include "lib/md5.h"

#include <stdio.h>

#include "md5calc.h"

CMD5Calc::CMD5Calc()
{
	context = NULL;
	Init();
}

CMD5Calc::~CMD5Calc()
{
	if(context)
		delete context;
}

bool CMD5Calc::Init()
{
	if(context)
		return false;

	context = new cvs_MD5Context();
	cvs_MD5Init(context);
	return true;
}

bool CMD5Calc::Update(const void *buf, size_t len)
{
	if(!context)
		return false;
	cvs_MD5Update(context, (const unsigned char *)buf, len);
	return true;
}

const char *CMD5Calc::Final()
{
	if(!context)
		return chk;

	cvs_MD5Final (checksum, context);
	for (size_t i = 0; i < 16; i++)
		sprintf(chk+i*2,"%02x",(unsigned)checksum[i]);
	delete context;
	context = NULL;
	return chk;
}
