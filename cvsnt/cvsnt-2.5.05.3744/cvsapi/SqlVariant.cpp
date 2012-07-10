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
#include <config.h>
#include "lib/api_system.h"
#include "cvs_string.h"
#include "SqlVariant.h"

#include <stdio.h>
#include <string.h>

CSqlVariant::CSqlVariant()
{
	m_type = vtNull;
}

CSqlVariant::CSqlVariant(char value)
{
	m_type = vtChar;
	m_char = value;
}

CSqlVariant::CSqlVariant(short value)
{
	m_type = vtShort;
	m_short = value;
}

CSqlVariant::CSqlVariant(int value)
{
	m_type = vtInt;
	m_int = value;
}

CSqlVariant::CSqlVariant(long value)
{
	m_type = vtLong;
	m_long = value;
}

#ifdef _WIN32
CSqlVariant::CSqlVariant(__int64 value)
#else
CSqlVariant::CSqlVariant(long long value)
#endif
{
	m_type = vtLongLong;
	m_longlong = value;
}

CSqlVariant::CSqlVariant(unsigned char value)
{
	m_type = vtUChar;
	m_uchar = value;
}

CSqlVariant::CSqlVariant(unsigned short value)
{
	m_type = vtUShort;
	m_ushort = value;
}

CSqlVariant::CSqlVariant(unsigned int value)
{
	m_type = vtUInt;
	m_uint = value;
}

CSqlVariant::CSqlVariant(unsigned long value)
{
	m_type = vtULong;
	m_ulong = value;
}

#ifdef _WIN32
CSqlVariant::CSqlVariant(unsigned __int64 value)
#else
CSqlVariant::CSqlVariant(unsigned long long value)
#endif
{
	m_type = vtULongLong;
	m_ulonglong = value;
}

CSqlVariant::CSqlVariant(const char *value)
{
	m_type = vtString;
	m_string = value;
}

CSqlVariant::CSqlVariant(const wchar_t *value)
{
	m_type = vtWString;
	m_wstring = value;	
}

CSqlVariant::~CSqlVariant()
{
}

CSqlVariant::operator char()
{
	return numericCast<char>("%c");
}

CSqlVariant::operator short()
{
	return numericCast<short>("%hd");
}

CSqlVariant::operator int()
{
	return numericCast<int>("%d");
}

CSqlVariant::operator long()
{
	return numericCast<long>("%ld");
}

#ifdef _WIN32
CSqlVariant::operator __int64()
{
	return numericCast<__int64>("%I64d");
}
#else
CSqlVariant::operator long long()
{
	return numericCast<long long>("%Ld");
}
#endif

CSqlVariant::operator unsigned char()
{
	return numericCast<unsigned char>("%c");
}

CSqlVariant::operator unsigned short()
{
	return numericCast<unsigned short>("%hu");
}

CSqlVariant::operator unsigned int()
{
	return numericCast<unsigned int>("%u");
}

CSqlVariant::operator unsigned long()
{
	return numericCast<unsigned long>("%lu");
}

#ifdef _WIN32
CSqlVariant::operator unsigned __int64()
{
	return numericCast<unsigned __int64>("%I64u");
}
#else
CSqlVariant::operator unsigned long long()
{
	return numericCast<unsigned long long>("%Lu");
}
#endif

CSqlVariant::operator const char *()
{
	switch(m_type)
	{
		case vtNull:
			return "";
		case vtChar:
			cvs::sprintf(m_tmp,32,"%hd",(short)m_char);
			return m_tmp.c_str();
		case vtShort:
			cvs::sprintf(m_tmp,32,"%hd",m_short);
			return m_tmp.c_str();
		case vtInt:
			cvs::sprintf(m_tmp,32,"%d",m_int);
			return m_tmp.c_str();
		case vtLong:
			cvs::sprintf(m_tmp,32,"%ld",m_long);
			return m_tmp.c_str();
		case vtLongLong:
#ifdef _WIN32
			cvs::sprintf(m_tmp,32,"%I64d",m_longlong);
#else
			cvs::sprintf(m_tmp,32,"%Ld",m_longlong);
#endif
			return m_tmp.c_str();
		case vtUChar:
			cvs::sprintf(m_tmp,32,"%hu",(unsigned short)m_uchar);
			return m_tmp.c_str();
		case vtUShort:
			cvs::sprintf(m_tmp,32,"%hu",m_ushort);
			return m_tmp.c_str();
		case vtUInt:
			cvs::sprintf(m_tmp,32,"%u",m_uint);
			return m_tmp.c_str();
		case vtULong:
			cvs::sprintf(m_tmp,32,"%lu",m_ulong);
			return m_tmp.c_str();
		case vtULongLong:
#ifdef _WIN32
			cvs::sprintf(m_tmp,32,"%I64u",m_longlong);
#else
			cvs::sprintf(m_tmp,32,"%Lu",m_longlong);
#endif
			return m_tmp.c_str();
		case vtString:
			return m_string;
		case vtWString:
			m_tmp = cvs::narrow(m_wstring);
			return m_tmp.c_str();
		default:
			return 0;
	}
}

CSqlVariant::operator const wchar_t*()
{
	switch(m_type)
	{
		case vtNull:
			return L"";
		case vtChar:
			cvs::swprintf(m_wtmp,32,L"%hd",(short)m_char);
			return m_wtmp.c_str();
		case vtShort:
			cvs::swprintf(m_wtmp,32,L"%hd",m_short);
			return m_wtmp.c_str();
		case vtInt:
			cvs::swprintf(m_wtmp,32,L"%d",m_int);
			return m_wtmp.c_str();
		case vtLong:
			cvs::swprintf(m_wtmp,32,L"%ld",m_long);
			return m_wtmp.c_str();
		case vtLongLong:
#ifdef _WIN32
			cvs::swprintf(m_wtmp,32,L"%I64d",m_longlong);
#else
			cvs::swprintf(m_wtmp,32,L"%Ld",m_longlong);
#endif
			return m_wtmp.c_str();
		case vtUChar:
			cvs::swprintf(m_wtmp,32,L"%hu",(unsigned short)m_uchar);
			return m_wtmp.c_str();
		case vtUShort:
			cvs::swprintf(m_wtmp,32,L"%hu",m_ushort);
			return m_wtmp.c_str();
		case vtUInt:
			cvs::swprintf(m_wtmp,32,L"%u",m_uint);
			return m_wtmp.c_str();
		case vtULong:
			cvs::swprintf(m_wtmp,32,L"%lu",m_ulong);
			return m_wtmp.c_str();
		case vtULongLong:
#ifdef _WIN32
			cvs::swprintf(m_wtmp,32,L"%I64u",m_longlong);
#else
			cvs::swprintf(m_wtmp,32,L"%Lu",m_longlong);
#endif
			return m_wtmp.c_str();
		case vtString:
			m_wtmp = cvs::wide(m_string);
			return m_wtmp.c_str();
		case vtWString:
			return m_wstring;
		default:
			return 0;
	}
}

template <typename _Ty>
_Ty CSqlVariant::numericCast(const char *strfmt)
{
	switch(m_type)
	{
		case vtNull:
			return (_Ty)0;
		case vtChar:
			return (_Ty)m_char;
		case vtShort:
			return (_Ty)m_short;
		case vtInt:
			return (_Ty)m_int;
		case vtLong:
			return (_Ty)m_long;
		case vtLongLong:
			return (_Ty)m_longlong;
		case vtUChar:
			return (_Ty)m_uchar;
		case vtUShort:
			return (_Ty)m_ushort;
		case vtUInt:
			return (_Ty)m_uint;
		case vtULong:
			return (_Ty)m_ulong;
		case vtULongLong:
			return (_Ty)m_ulonglong;
		case vtString:
			{
				_Ty t;
				sscanf(m_string,strfmt,&t);
				return t;
			}
		case vtWString:
			{
				wchar_t fmt[16], *q;
				const char *p;
				_Ty t;

				for(p=strfmt, q=fmt; *p; p++)
					*(q++)=*p;
				swscanf(m_wstring,fmt,&t);
				return t;
			}
		default:
			return 0;
	}
}
