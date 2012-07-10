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
 * Author : Alexandre Parenteau <aubonbeurre@hotmail.com> --- May 1999
 */

#ifndef USTR_H
#define USTR_H

#ifdef __GNUC__
#	define EGCS_CONST const
#else
#	define EGCS_CONST
#endif

#include <string.h>

// up to 255 characters
class UPStr
{
public :
	inline UPStr() : str(0L) {}
	inline UPStr(const char *newstr)
	{
		str = 0L;
		*this = newstr;
	}
	inline UPStr(const unsigned char *newstr)
	{
		str = 0L;
		*this = newstr;
	}
	inline UPStr(const UPStr & newstr)
	{
		str = 0L;
		*this = newstr;
	}
	virtual ~UPStr() {flush();}

	inline bool empty(void) const {return str == 0L || str[0] == '\0';}
	inline unsigned int length(void) const {return str == 0L ? 0 : (unsigned char)str[0];}

	const char *operator=(const char *newstr);
		// set to a new C String (0L is OK)
	const unsigned char *operator=(const unsigned char *newstr);
		// set to a new P String (0L is OK)
	const UPStr & operator=(const UPStr & newstr);
		// set according to another UPStr
	const UPStr & set(const char *buf, unsigned int len);
		// set from a buffer

	inline operator const char *() const
		{return str == 0L ? "" : str + 1;}
	inline operator char *() EGCS_CONST
		{return str == 0L ? 0L : str + 1;}

		// as a C string
	inline operator const unsigned char *() const
		{return str == 0L ? (unsigned char *)"" : (unsigned char *)str;}
	inline operator unsigned char *() EGCS_CONST
		{return str == 0L ? (unsigned char *)"" : (unsigned char *)str;}
		// as a P string

	UPStr & operator<<(const char *addToStr);
	UPStr & operator<<(char addToStr);
	UPStr & operator<<(int addToStr);
		// concatenate

	inline bool endsWith(char c) const {return str == 0L ? false : str[str[0]] == c;}
	
	const UPStr & replace(char which, char bywhich);
		// replace a character
	
protected :
	void flush(void);
	char *str; // the String
};

// above 255 characters, no-Pascal string support
class UStr
{
public :
	inline UStr() : str(0L) {}
	inline UStr(const char *newstr)
	{
		str = 0L;
		*this = newstr;
	}
	inline UStr(const unsigned char *newstr)
	{
		str = 0L;
		set((const char *)newstr + 1, newstr[0]);
	}
	inline UStr(const UStr & newstr)
	{
		str = 0L;
		*this = newstr;
	}
	virtual ~UStr() {flush();}

	inline bool empty(void) const {return str == 0L || str[0] == '\0';}
	inline unsigned int length(void) const {return str == 0L ? 0 : static_cast<unsigned int>(strlen(str));}

	const char *operator=(const char *newstr);
		// set from a C String (0L is OK)
	const char *operator=(const unsigned char *newstr);
		// set from a P String (0L is OK)
	const UStr & operator=(const UStr & newstr);
		// set according to another UStr
	const UStr & set(const char *buf, unsigned int len);
		// set from a buffer

#ifdef WIN32
	bool operator<(const UStr & str) const;
		// not implemented : only provided to compile
		// with STL
#endif /* WIN32 */

#if defined(macintosh) || defined(__GNUC__)
	inline char operator[](int index) const { return str[index]; }
	inline char & operator[](int index) { return str[index]; }
#endif /* macintosh */

	inline operator const char *() const { return str == 0L ? "" : str; }
	inline operator char *() EGCS_CONST { return str == 0L ? 0L : str; }
		// as a C string

	UStr & operator<<(const char *addToStr);
	UStr & operator<<(char addToStr);
	UStr & operator<<(int addToStr);
		// concatenate

	inline int compare(const char *thestr) const { return strcmp(*this, thestr); }
		// compare

	inline bool endsWith(char c) const {return str == 0L ? false : str[length()-1] == c;}
	
	const UStr & replace(char which, char bywhich);
		// replace a character
	
protected :
	void flush(void);
	char *str; // the String
};

#endif /* USTR_H */
