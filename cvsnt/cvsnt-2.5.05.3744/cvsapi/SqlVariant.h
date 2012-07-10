/*
	CVSNT Generic API
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

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
#ifndef SQLVARIANT__H
#define SQLVARIANT__H

class CSqlVariant
{
public:
	enum vtype
	{
		vtNull,
		vtChar,
		vtShort,
		vtInt,
		vtLong,
		vtLongLong,
		vtUChar,
		vtUShort,
		vtUInt,
		vtULong,
		vtULongLong,
		vtString,
		vtWString
	};

	CVSAPI_EXPORT CSqlVariant();
	CVSAPI_EXPORT CSqlVariant(char value);
	CVSAPI_EXPORT CSqlVariant(short value);
	CVSAPI_EXPORT CSqlVariant(int value);
	CVSAPI_EXPORT CSqlVariant(long value);
#ifdef _WIN32
	CVSAPI_EXPORT CSqlVariant(__int64 value);
#else
	CVSAPI_EXPORT CSqlVariant(long long value);
#endif
	CVSAPI_EXPORT CSqlVariant(unsigned char value);
	CVSAPI_EXPORT CSqlVariant(unsigned short value);
	CVSAPI_EXPORT CSqlVariant(unsigned int value);
	CVSAPI_EXPORT CSqlVariant(unsigned long value);
#ifdef _WIN32
	CVSAPI_EXPORT CSqlVariant(unsigned __int64 value);
#else
	CVSAPI_EXPORT CSqlVariant(unsigned long long value);
#endif
	CVSAPI_EXPORT CSqlVariant(const char *value);
	CVSAPI_EXPORT CSqlVariant(const wchar_t *value);

	CVSAPI_EXPORT virtual ~CSqlVariant();

	CVSAPI_EXPORT operator char();
	CVSAPI_EXPORT operator short();
	CVSAPI_EXPORT operator int();
	CVSAPI_EXPORT operator long();
#ifdef _WIN32
	CVSAPI_EXPORT operator __int64();
#else
	CVSAPI_EXPORT operator long long();
#endif
	CVSAPI_EXPORT operator unsigned char();
	CVSAPI_EXPORT operator unsigned short();
	CVSAPI_EXPORT operator unsigned int();
	CVSAPI_EXPORT operator unsigned long();
#ifdef _WIN32
	CVSAPI_EXPORT operator unsigned __int64();
#else
	CVSAPI_EXPORT operator unsigned long long();
#endif
	CVSAPI_EXPORT operator const char *();
	CVSAPI_EXPORT operator const wchar_t*();

	CVSAPI_EXPORT vtype type() { return m_type; }
	CVSAPI_EXPORT bool isNumeric() { return !(m_type==vtString || m_type==vtWString); }

protected:
	union
	{
		char m_char;
		short m_short;
		int m_int;
		long m_long;
#ifdef _WIN32
		__int64 m_longlong;
#else
		long long m_longlong;
#endif
		unsigned char m_uchar;
		unsigned short m_ushort;
		unsigned int m_uint;
		unsigned long m_ulong;
#ifdef _WIN32
		unsigned __int64 m_ulonglong;
#else
		unsigned long long m_ulonglong;
#endif
		const char *m_string;
		const wchar_t *m_wstring;
	};
	vtype m_type;
	cvs::string m_tmp;
	cvs::wstring m_wtmp;

	template <typename _Ty>
		_Ty numericCast(const char *strfmt);
};

#endif

