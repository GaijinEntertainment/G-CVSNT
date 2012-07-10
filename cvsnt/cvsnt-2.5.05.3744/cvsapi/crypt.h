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
#ifndef CRYPT__H
#define CRYPT__H

class CCrypt
{
public:
	CVSAPI_EXPORT CCrypt();
	virtual CVSAPI_EXPORT ~CCrypt();

	CVSAPI_EXPORT const char *crypt(const char *password, bool md5 = true);
	static CVSAPI_EXPORT int compare(const char *password, const char *crypt_pw);

protected:
	char m_szPw[128];
};

#endif
