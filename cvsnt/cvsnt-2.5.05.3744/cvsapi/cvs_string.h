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
/* _EXPORT */
#ifndef CVS_STRING__H
#define CVS_STRING__H

#include <cstdio>
#include <string>
#include <queue>
#include <stdarg.h>
#include <string.h>
#include <cstdio>

#include "ServerIO.h"

#include "cvs_smartptr.h"

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#ifdef HAVE_IDNA_H
#include <idna.h>
#endif

/* If this is a nonstandard compiler (gcc-2.95) we define our own wstring */
#ifndef HAVE_WSTRING
namespace std
{
	typedef basic_string<wchar_t> wstring;
};
#endif

#include "lib/api_system.h"
#include "lib/fnmatch.h"


namespace cvs
{
	/* Define traits for the filenames and usernames in c++ */
	struct filename_char_traits : public std::char_traits<char>
	{
		static CVSAPI_EXPORT bool eq( char c1, char c2 ) { return CompareFileChar(c1,c2)?false:true;  }
		static CVSAPI_EXPORT bool ne( char c1, char c2 ) { return CompareFileChar(c1,c2)?true:false;  }
		static CVSAPI_EXPORT bool lt( char c1, char c2 ) { return CompareFileChar(c1,c2)<0;   }
		static CVSAPI_EXPORT int compare( const char* s1,  const char* s2, size_t n ) { return fnncmp( s1, s2, n ); }
		static CVSAPI_EXPORT const char*find( const char* s, size_t n, char a )
		{
			for(; n>0; n--,s++)
				if(!CompareFileChar(*s,a))
					return s;
			return NULL;
		}
	};

	struct username_char_traits : public std::char_traits<char>
	{
		static CVSAPI_EXPORT bool eq( char c1, char c2 ) { return CompareUserChar(c1,c2)?false:true;  }
		static CVSAPI_EXPORT bool ne( char c1, char c2 ) { return CompareUserChar(c1,c2)?true:false;  }
		static CVSAPI_EXPORT bool lt( char c1, char c2 ) { return CompareUserChar(c1,c2)<0;   }
		static CVSAPI_EXPORT int compare( const char* s1,  const char* s2, size_t n ) { return userncmp( s1, s2, n ); }
		static CVSAPI_EXPORT const char*find( const char* s, size_t n, char a ) { for(; n>0; n--,s++) if(!CompareUserChar(*s,a)) return s; return NULL; }
	};

	typedef std::basic_string<char, filename_char_traits> filename;

	/* Unfortunately you can't do this in char_traits due to a bug in the compare handlers...
	   inheriting from a template is OK only as long as you don't add member variables (so the
	   object is identical in memory) as they don't have virtual destructors */
	class wildcard_filename : public filename
	{
	public:
		CVSAPI_EXPORT wildcard_filename() { }
		CVSAPI_EXPORT wildcard_filename(const char *_Ptr) { assign(_Ptr); }
		CVSAPI_EXPORT wildcard_filename(const filename& oth) { assign(oth); }
		CVSAPI_EXPORT wildcard_filename(const wildcard_filename& oth) { assign(oth); }
		CVSAPI_EXPORT bool operator==(const filename& _Oth) const { return fnmatch(c_str(),_Oth.c_str(),CVS_CASEFOLD|FNM_PATHNAME)==0; }
		CVSAPI_EXPORT bool operator==(const wildcard_filename& _Oth) const { return fnmatch(c_str(),_Oth.c_str(),CVS_CASEFOLD|FNM_PATHNAME)==0; }
		CVSAPI_EXPORT bool operator==(const char *_Ptr) const { return fnmatch(c_str(),_Ptr,CVS_CASEFOLD|FNM_PATHNAME)==0; }
		CVSAPI_EXPORT wildcard_filename& operator=(const filename& oth) { assign(oth); return *this; }
		CVSAPI_EXPORT wildcard_filename& operator=(const wildcard_filename& oth) { assign(oth); return *this; }
		CVSAPI_EXPORT wildcard_filename& operator=(const char *_Ptr) { assign(_Ptr); return *this; }
		CVSAPI_EXPORT bool matches_regexp(const char *regexp, bool extended = true);
	};
	typedef std::basic_string<char, username_char_traits> username;

	typedef std::basic_string<char> string;
	typedef std::basic_string<wchar_t> wstring;

	CVSAPI_EXPORT bool str_prescan(const char *fmt, va_list va);

	template<class _Typ>
	void vsprintf(_Typ& str, size_t size_hint, const char *fmt, va_list va)
	{
		str.resize(size_hint?size_hint:strlen(fmt)+256);

		va_list xva;
		va_copy(xva,va);
		str_prescan(fmt,xva);
		va_end(xva);
		do
		{
			va_copy(xva,va);
			int res = ::vsnprintf((char*)str.data(), str.size(), fmt, xva);
			va_end(xva);
			if(res<0) /* SuSv2, Win32 */
				str.resize(str.size()*2);
#ifdef __digital
			else if(res>=(int)str.size()) /* Tru64 */
				str.resize(str.size()*2);
#else
			else if(res>=(int)str.size()) /* C99 */
				str.resize(res+1);
#endif
			else
				break;
		} while(true);
		str.resize(strlen(str.data()));
	}

	template<class _Typ>
	void vswprintf(_Typ& str, size_t size_hint, const wchar_t *fmt, va_list va)
	{
		str.resize(size_hint?size_hint:wcslen(fmt)+256);

		va_list xva;
		do
		{
			va_copy(xva,va);
			int res = ::vswprintf((wchar_t*)str.data(), str.size(), fmt, xva);
			va_end(xva);
			if(res<0) /* SuSv2, Win32 */
				str.resize(str.size()*2);
#ifdef __digital
			else if(res>=(int)str.size()) /* Tru64 */
				str.resize(str.size()*2);
#else
			else if(res>=(int)str.size()) /* C99 */
				str.resize(res+1);
#endif
			else
				break;
		} while(true);
		str.resize(wcslen(str.data()));
	}

	template<class _Typ>
	void sprintf(_Typ& str, size_t size_hint, const char *fmt, ...)
	{
		va_list va;
		va_start(va,fmt);
		cvs::vsprintf(str,size_hint,fmt,va);
		va_end(va);
	}

	template<class _Typ>
	void swprintf(_Typ& str, size_t size_hint, const wchar_t *fmt, ...)
	{
		va_list va;
		va_start(va,fmt);
		cvs::vswprintf(str,size_hint,fmt,va);
		va_end(va);
	}

	template<class _Typ>
	_Typ trim(_Typ& str)
	{
		size_t c=0,d=str.length();
		while(c<d && isspace(str[c]))
			c++;
		while(c<d && isspace(str[--d]))
			;
		if(d<c) d=c-1;
		return str.substr(c,(d-c)+1);
	}

	struct wide
	{
		wide(const char *str) { utf82ucs2(str); }
		operator const wchar_t *() { return w_str.c_str(); }
		std::wstring w_str;
		void utf82ucs2(const char *src)
		{
			unsigned char *p=(unsigned char *)src;
			wchar_t ch;
			// strlen(NULL) causes a crash (VS2008) - so handle it nice
			size_t strlen_src=(src)?strlen(src):0;
			w_str.reserve(strlen_src);
			while(*p)
			{
				if(*p<0x80) { ch = p[0]; p++; }
				else if(*p<0xe0) { ch = ((p[0]&0x3f)<<6)+(p[1]&0x3f); p+=2; }
				else if(*p<0xf0) { ch = ((p[0]&0x1f)<<12)+((p[1]&0x3f)<<6)+(p[2]&0x3f); p+=3; }
				else if(*p<0xf8) { ch = ((p[0]&0x0f)<<18)+((p[1]&0x3f)<<12)+((p[2]&0x3f)<<6)+(p[3]&0x3f); p+=4; }
				else if(*p<0xfc) { ch = ((p[0]&0x07)<<24)+((p[1]&0x3f)<<18)+((p[2]&0x3f)<<12)+((p[3]&0x3f)<<6)+(p[4]&0x3f); p+=5; }
				else if(*p<0xfe) { ch = ((p[0]&0x03)<<30)+((p[1]&0x3f)<<24)+((p[2]&0x3f)<<18)+((p[3]&0x3f)<<12)+((p[4]&0x3f)<<6)+(p[5]&0x3f); p+=6; }
				else { ch = '?'; p++; }
				w_str+=(wchar_t)ch;
			}
		}
	};

	struct narrow
	{
		narrow(const wchar_t *w_str) { ucs22utf8(w_str); }
		operator const char *() { return str.c_str(); }
		std::string str;
		void ucs22utf8(const wchar_t *src)
		{
			const wchar_t *p=src;
			size_t srclen=wcslen(src);
			str.reserve(srclen*3);
			str="\0";
			while(*p)
			{
#pragma warning(push)
#pragma warning(disable:4333)
				if(*p<0x80) { str+=(char)*p; }
				else if(*p<0x800) { str+=0xc0|(*p>>6); str+=128+(*p&0x3f); }
				else if(*p<0x10000) { str+=0xe0|(*p>>12); str+=0x80|((*p>>6)&0x3f); str+=0x80|(*p&0x3f); }
				else if(*p<0x200000) { str+=0xf0|(*p>>18); str+=0x80|((*p>>12)&0x3f); str+=0x80|((*p>>6)&0x3f); str+=0x80|(*p&0x3f); }
				else if(*p<0x4000000) { str+=0xf8|(*p>>24); str+=0x80|((*p>>18)&0x3f); str+=0x80|((*p>>12)&0x3f); str+=0x80|((*p>>6)&0x3f); str+=0x80|(*p&0x3f); }
				else if(*p<0x80000000) { str+=0xfc|(*p>>30); str+=0x80|((*p>>24)&0x3f); str+=0x80|((*p>>18)&0x3f); str+=0x80|((*p>>12)&0x3f); str+=0x80|((*p>>6)&0x3f); str+=0x80|(*p&0x3f); }
				else { str+='?'; }
				p++;
#pragma warning(pop)
			}
		}
	};

	// converts the wide character string to a strict 2 byte string
	// this is needed because many platforms have wchar_t as 32 bits 
	struct cvsucs2
	{
		cvsucs2(const wchar_t *w_str, const size_t w_len) 
		{ 
			str_length=0;
			ustr_length=0;
			pstr=NULL;
			ustr=NULL;
			pstr=(char *)malloc(((w_len)+10)*2);
			if (pstr)
				wide2ucs2(static_cast<const wchar_t *>(w_str),w_len); 
		}
		cvsucs2(const char *u_str, const size_t u_len) 
		{ 
			str_length=0;
			ustr_length=0;
			pstr=NULL;
			ustr=NULL;
			ustr=(wchar_t *)malloc((u_len+10)*sizeof(wchar_t));
			if (ustr)
				ucs22wide(static_cast<const char *>(u_str),u_len); 
		}
		~cvsucs2(void) 
		{ 
			str_length=0;
			ustr_length=0;
			if (ustr)
				free(ustr);
			if (pstr)
				free(pstr);
		}
		long length(void) 
		{ 
			return (pstr)?str_length:ustr_length;
		}
		operator const char *() 
		{ 
			return pstr; 
		}
		operator const wchar_t *() 
		{ 
			return ustr; 
		}
		operator const long () { return (pstr)?str_length:ustr_length; }
		char *pstr;
		wchar_t *ustr;
		long str_length;
		long ustr_length;
		int ucs2_to_i(void *datain)
		{
			int iresult;
			const unsigned char * instr=(const unsigned char *)datain;
			string xxxstr;
			xxxstr.resize(50);
			sprintf(xxxstr,50," %02u",(int)*(instr));
			iresult=atoi(xxxstr.c_str());
			return iresult;
		}
		int bigendian_like_hp(void)
		{
			wchar_t aword[] = L"A\0";
			int iresult = ucs2_to_i((void *)&aword[0]);
			return (iresult ? 0 : 1);
		}
		void ucs22wide(const char *u_src, const size_t u_len)
		{
			wchar_t wendiantest=L'A';
			char *aendiantest = (char *) &wendiantest;
			const char *p=u_src;
			wchar_t convy;
			wchar_t *ptrstr=ustr;
			if (ptrstr==NULL) return;
			ustr_length=0;
			*ptrstr='\0';
			while(ustr_length<(long)u_len)
			{
				if (bigendian_like_hp())
				{
				convy=(((wchar_t)*p)<<8)&0xff00;
				p++;
				convy|=((wchar_t)*p)&0xff;
				*ptrstr=(wchar_t)convy;
				}
				else
				{
				convy=(((wchar_t)*(p+1))<<8)&0xff00;
				convy|=((wchar_t)*p)&0xff;
				p++;
				*ptrstr=(wchar_t)convy;
				}
				if (!*ptrstr)
					break;
				ptrstr++; ustr_length++;
				p++;
			}
			*(ptrstr)=(wchar_t)'\0';
			ptrstr++;
		}
		void wide2ucs2(const wchar_t *src, const size_t w_len)
		{
			const wchar_t *p=src;
			char convy[3];
			char *ptrstr=pstr;
			if (ptrstr==NULL) return;
			str_length=0;
			*ptrstr='\0';
			while(*p)
			{
				if (bigendian_like_hp())
				{
				convy[0]=((*p)>>8)&0xff;
				convy[1]=(*p)&0xff;
				} else
				{
				convy[1]=((*p)>>8)&0xff;
				convy[0]=(*p)&0xff;
				}
				convy[2]=0;
				*ptrstr=(char)convy[0];
				ptrstr++; str_length++;
				*ptrstr=(char)convy[1];
				ptrstr++; str_length++;
				p++;
			}
			*(ptrstr)=(char)'\0';
			ptrstr++;
			*(ptrstr)=(char)'\0';
			ptrstr++;
		}
	};

#if defined(_WIN32) && defined(LoadLibrary) // We don't do this if there are no win32 headers
	static int (CALLBACK *pIdnToAscii)(DWORD,LPCWSTR,int,LPWSTR,int);
	static int (CALLBACK *pIdnToUnicode)(DWORD,LPCWSTR,int,LPWSTR,int);
	static bool bIdnToAsciiInit = false;
	static bool bIdnToUnicodeInit = false;

	struct idn
	{
		idn(const char *str) { idntoascii(str); }
		operator const char *() { return str.c_str(); }
		std::string str;
		void idntoascii(const char *src)
		{
			if(!bIdnToAsciiInit)
			{
				HMODULE hLib = LoadLibraryA("Normaliz.dll");
				if(!hLib)
					pIdnToAscii = NULL;
				else
					pIdnToAscii = (int (CALLBACK *)(DWORD,LPCWSTR,int,LPWSTR,int))GetProcAddress(hLib,"IdnToAscii");
				bIdnToAsciiInit = true;
			}
			if(!pIdnToAscii)
				str = src;
			else
			{
				cvs::wide s = src;
				cvs::wstring wstr;
				int c = pIdnToAscii(0,s,lstrlenW(s),NULL,0);
				if(!c)
					str = src;
				else
				{
					wstr.resize(c);
					pIdnToAscii(0,s,lstrlenW(s),(wchar_t *)wstr.data(),wstr.size());
					str = cvs::narrow(wstr.c_str());
				}
			}
		}
	};

	struct decode_idn
	{
		decode_idn(const char *str) { asciitoidn(str); }
		operator const char *() { return str.c_str(); }
		std::string str;
		void asciitoidn(const char *src)
		{
			if(!bIdnToUnicodeInit)
			{
				HMODULE hLib = LoadLibraryA("Normaliz.dll");
				if(!hLib)
					pIdnToUnicode = NULL;
				else
					pIdnToUnicode = (int  (CALLBACK *)(DWORD,LPCWSTR,int,LPWSTR,int))GetProcAddress(hLib,"IdnToUnicode");
				bIdnToUnicodeInit = true;
			}
			if(!pIdnToUnicode)
				str = src;
			else
			{
				cvs::wide s = src;
				cvs::wstring wstr;
				int c = pIdnToUnicode(0,s,lstrlenW(s),NULL,0);
				if(!c)
					str = src;
				else
				{
					wstr.resize(c);
					pIdnToUnicode(0,s,lstrlenW(s),(wchar_t *)wstr.data(),wstr.size());
					str = cvs::narrow(wstr.c_str());
				}
			}
		}
	};

#elif defined(HAVE_IDNA_H) && defined(HAVE_LIBIDN)
	struct idn
	{
		idn(const char *_str) { utf8topunycode(_str); }
		~idn() { if(dst) free(dst); }
		operator const char *() { return dst?dst:src; }
		const char *src;
		char *dst;
		void utf8topunycode(const char *_str)
		{
			src = _str;
			dst = NULL;
		 	if(idna_to_ascii_lz(src, &dst, 0)!=IDNA_SUCCESS)
			{
				if(dst) free(dst);
				dst = NULL;
			}
		}
	};

	struct decode_idn
	{
		decode_idn(const char *_str) { punycodetoutf8(_str); }
		~decode_idn() { if(dst) free(dst); }
		operator const char *() { return dst?dst:src; }
		const char *src;
		char *dst;
		void punycodetoutf8(const char *_str)
		{
			src = _str;
			dst = NULL;
		 	if(idna_to_unicode_lzlz(src, &dst, 0)!=IDNA_SUCCESS)
			{
				if(dst) free(dst);
				dst = NULL;
			}
		}
	};
#else
	struct idn
	{
		idn(const char *_str) { str = _str; }
		operator const char *() { return str; }
		const char *str;
	};

	struct decode_idn
	{
		decode_idn(const char *_str) { str = _str; }
		operator const char *() { return str; }
		const char *str;
	};
#endif

	class cache_static_string
	{
	public:
		cache_static_string(const char *string)
		{ 
			if(string)
			{ 
				global_string_cache.push(string);
				m_ptr = global_string_cache.back().c_str();
				while(global_string_cache.size()>global_string_cache_max)
					global_string_cache.pop();
			}
			else 
				m_ptr=NULL;
		}
		virtual ~cache_static_string() { }
		operator const char *() const { return m_ptr; }
	protected:
		const char *m_ptr;

		static CVSAPI_EXPORT std::queue<std::string> global_string_cache;
		static int const global_string_cache_max = 30;
	};
};


#endif

