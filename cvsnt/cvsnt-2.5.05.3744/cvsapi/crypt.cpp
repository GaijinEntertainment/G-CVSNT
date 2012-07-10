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
#include "lib/md5crypt.h"
#include "ServerIO.h"
#include "crypt.h"
#include "../ufc-crypt/crypt.h"

#include <string.h>

#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

CCrypt::CCrypt()
{
}

CCrypt::~CCrypt()
{
}

const char *CCrypt::crypt(const char *password, bool md5)
{
	char salt[9];

	if(!password)
		return NULL;

	for(int n=0; n<8; n++)
		salt[n] = bin_to_ascii(rand()&0x3f);
	salt[8]='\0';
	strcpy(m_szPw,md5?md5_crypt(password, salt): ::crypt(password, salt));
	return m_szPw;
}

int CCrypt::compare(const char *password, const char *crypt_pw)
{
	return compare_crypt(password,crypt_pw);
}
