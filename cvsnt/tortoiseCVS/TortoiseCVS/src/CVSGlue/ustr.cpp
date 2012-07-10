/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Author : Alexandre Parenteau <aubonbeurre@hotmail.com> --- January 1997
 */

#include "StdAfx.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "ustr.h"
//#include "uconsole.h"

#if defined(macintosh) && !defined(qMacAPP)
#	include <sys/errno.h>
#endif

#if qMacAPP
#	include <Errors.h>
#endif

static const unsigned int kMaxChar = 255;

void UPStr::flush(void)
{
	if(str == 0L)
		return;
	free(str);
	str = 0L;
}

const char *UPStr::operator=(const char *newstr)
{
	if(newstr == 0L)
		newstr = "";

	size_t l = strlen(newstr);
	if(l > kMaxChar)
		l = kMaxChar;

	char *newvalue = (char *)malloc((l + 2) * sizeof(char));
	if(newvalue == 0L)
		return NULL;
		//UTHROW(ENOMEM);

	strncpy(newvalue + 1, newstr, l);
	newvalue[0] = static_cast<char>(l);
	newvalue[l + 1] = '\0';
	
	flush();
	str = newvalue;
	
	return *this;
}

const unsigned char *UPStr::operator=(const unsigned char *newstr)
{
	if(newstr == 0L)
		newstr = (unsigned char *)"";

	int l = newstr[0];
	char *newvalue = (char *)malloc((l + 2) * sizeof(char));
	if(newvalue == 0L)
		return NULL;
//		UTHROW(ENOMEM);

	memcpy(newvalue + 1, (char *)newstr + 1, l * sizeof(char));
	newvalue[0] = l;
	newvalue[l + 1] = '\0';

	flush();
	str = newvalue;

	return *this;
}

const UPStr & UPStr::operator=(const UPStr & newstr)
{
	*this = (const char *)newstr;
	return *this;
}

const UPStr & UPStr::set(const char *buf, unsigned int len)
{
	flush();
	if(len == 0)
		return *this;
	if(len > kMaxChar)
		len = kMaxChar;

	str = (char *)malloc((len + 2) * sizeof(char));
	if(str == 0L)
	{
//		UAppConsole("Impossible to allocate %d bytes !\n", (len + 2) * sizeof(char));
		return *this;
	}

	memcpy(str + 1, buf, len * sizeof(char));
	str[0] = len;
	str[len + 1] = '\0';

	return *this;
}

const UPStr & UPStr::replace(char which, char bywhich)
{
	if(str == 0L)
		return *this;
	
	char *buf = str + 1;
	while(*buf)
	{
		if(*buf == which)
			*buf++ = bywhich;
		else
			buf++;
	}
	return *this;
}

UPStr & UPStr::operator<<(const char *addToStr)
{
	if(addToStr == 0L)
		return *this;

	if(str == 0L)
	{
		*this = addToStr;
		return *this;
	}

	size_t len = strlen(addToStr);
	unsigned curlen = length();
	if(len + length() > kMaxChar)
		len = kMaxChar - length();
	if(len == 0)
		return *this;

	str = (char *)realloc(str, (len + curlen + 2) * sizeof(char));
	memcpy(str + curlen + 1, addToStr, len * sizeof(char));
	str[0] = static_cast<char>(len + curlen);
	str[len + curlen + 1] = '\0';

	return *this;
}

UPStr & UPStr::operator<<(char addToStr)
{
	char astr[2] = {'\0', '\0'};
	astr[0] = addToStr;
	return *this << astr;
}

UPStr & UPStr::operator<<(int addToStr)
{
	char astr[50];
	sprintf(astr, "%d", addToStr);
	return *this << astr;
}

void UStr::flush(void)
{
	if(str == 0L)
		return;
	free(str);
	str = 0L;
}

const char *UStr::operator=(const char *newstr)
{
	if(newstr == 0L)
		newstr = "";

	int l = static_cast<int>(strlen(newstr));
	char *newvalue = (char *)malloc((l + 1) * sizeof(char));
	if(newvalue == 0L)
		return NULL;
		//UTHROW(ENOMEM);

	strcpy(newvalue, newstr);
	
	flush();
	str = newvalue;

	return *this;
}

const char *UStr::operator=(const unsigned char *newstr)
{
	if(newstr == 0L)
		newstr = (unsigned char *)"";

	int l = newstr[0];
	char *newvalue = (char *)malloc((l + 1) * sizeof(char));
	if(newvalue == 0L)
		return NULL;
		//UTHROW(ENOMEM);

	memcpy(newvalue, newstr + 1, l * sizeof(char));
	newvalue[l] = '\0';

	flush();
	str = newvalue;

	return *this;
}

const UStr & UStr::operator=(const UStr & newstr)
{
	*this = (const char *)newstr;
	return *this;
}

const UStr & UStr::set(const char *buf, unsigned int len)
{
	flush();
	if(len == 0)
		return *this;

	str = (char *)malloc((len + 1) * sizeof(char));
	if(str == 0L)
	{
//		UAppConsole("Impossible to allocate %d bytes !\n", (len + 1) * sizeof(char));
		return *this;
	}

	memcpy(str, buf, len * sizeof(char));
	str[len] = '\0';

	return *this;
}

const UStr & UStr::replace(char which, char bywhich)
{
	if(str == 0L)
		return *this;
	
	char *buf = str;
	while(*buf)
	{
		if(*buf == which)
			*buf++ = bywhich;
		else
			buf++;
	}
	return *this;
}

UStr & UStr::operator<<(const char *addToStr)
{
	if(addToStr == 0L)
		return *this;

	if(str == 0L)
	{
		*this = addToStr;
		return *this;
	}

	unsigned int len = static_cast<unsigned int>(strlen(addToStr));
	if(len == 0)
		return *this;

	unsigned int curlen = length();
	char *newstr = (char *)malloc((len + curlen + 1) * sizeof(char));
	if(newstr == 0L)
		return *this;
		//UTHROW(ENOMEM);
	memcpy(newstr, str, curlen * sizeof(char));
	memcpy(newstr + curlen, addToStr, len * sizeof(char));
	newstr[len + curlen] = '\0';

	if(str != 0L)
		free(str);
	str = newstr;

	return *this;
}

UStr & UStr::operator<<(char addToStr)
{
	char astr[2] = {'\0', '\0'};
	astr[0] = addToStr;
	return *this << astr;
}

UStr & UStr::operator<<(int addToStr)
{
	char astr[50];
	sprintf(astr, "%d", addToStr);
	return *this << astr;
}
