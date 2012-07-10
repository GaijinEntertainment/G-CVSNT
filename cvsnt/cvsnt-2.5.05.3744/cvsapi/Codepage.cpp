/*
	CVSNT Generic API - Codepage translation
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include "lib/getmode.h"
#else
#include <unistd.h>
#endif
#include <assert.h>
#include "Codepage.h"
#include "ServerIO.h"

/* For Solaris, which has an inconsistent build environment -
   gnu libiconv in 32bit, sun libiconv in 64bit and gnu libiconv headers */
#ifndef HAVE_LIBICONV_OPEN
#define LIBICONV_PLUG
#endif

/* Don't know what to do if there's no iconv.h... probably
   we'll disable all the unicode/codepage conversions */
#ifdef HAVE_ICONV_H
#include <iconv.h>
#else
#error need iconv.h
#endif

#ifdef HAVE_LIBCHARSET_H
#include <libcharset.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LOCALCHARSET_H
#include <localcharset.h>
#endif

#ifndef HAVE_LOCALE_CHARSET
#ifdef HAVE_NL_LANGINFO
const char *locale_charset()
{
	return nl_langinfo(CODESET);
}
#else
#error need nl_langinfo or locale_charset
#endif
#endif

/* The 'default' encoding is for UTF8-BOM for storage in the RCS files */
CCodepage::Encoding CCodepage::Utf8Encoding(UTF8_CHARSET,true);

/* Force detection (source), no encoding (target) */
CCodepage::Encoding CCodepage::NullEncoding;

CCodepage::CCodepage()
{
	m_ic=NULL;
}

CCodepage::~CCodepage()
{
	if(m_ic && m_blockcount>=0)
	  iconv_close((iconv_t)m_ic);
}

/* People sometimes abbreviate character set names... */
const char *CCodepage::CheckAbbreviations(const char *cp)
{
	/* Strcmp first, for speed */
	if(!strcmp(cp,"UTF-8") || !strcmp(cp,"UTF8"))
		return UTF8_CHARSET;
	if(!strcmp(cp,"UCS2"))
		return "UCS-2";
	if(!strcmp(cp,"UCS4"))
		return "UCS-4";
	if(!strcmp(cp,"UTF16"))
		return "UTF-16";
	if(!strcmp(cp,"UTF32"))
		return "UTF-32";
	return cp;
}

bool CCodepage::GuessEncoding(const char *buf, size_t len, Encoding& type, const Encoding& hint)
{
	const unsigned short *c;
	size_t lowchar_count, swap_lowchar_count;

	if(len>2 && (buf[0]==(char)0xef && buf[1]==(char)0xbb && buf[2]==(char)0xbf))
	{
		/* UTF8 */
		type=Utf8Encoding;
		return true;
	}

	if(len<2 || len&1)
	{
		type=NullEncoding;
		return true; // Odd length files (by definition) can't be unicode encoded
	}

	// Check for encoding header
	if(buf[0]==(char)0xff && buf[1]==(char)0xfe)
	{
		type.encoding="UCS-2LE";
		type.bom=true;
		return true;
	}

	// Byteswap encoding header
	if(buf[0]==(char)0xfe && buf[1]==(char)0xff)
	{
		type.encoding="UCS-2BE";
		type.bom=true;
		return true;
	}

	if(hint.encoding)
	{
		type=hint;
		return true;
	}

	// FIXME: Causes a bus error on Solaris (presumably due to buf being unaligned).  Not sure
	// what to do here...
	
	// Into uncertain territory...  For stuff like US-ANSI encodings then we can be fairly
	// certain, but once it gets into arabic and stuff there is no good method of autodetection
	lowchar_count=0;
	swap_lowchar_count=0;
	for(c=(const unsigned short*)buf; ((const char *)c)<(buf+len); c++)
	{
		if((*c)<128) lowchar_count++;
		if(((((*c)>>8)+(((*c)&0xff)<<8)))<128) swap_lowchar_count++;
	}

	// If >80% of the buffer is encoding<128
	if(lowchar_count>(len*8)/10)
	{
		type.encoding="UCS-2LE";
		type.bom=false;
		return true;
	}
	// same for byteswapped
	if(swap_lowchar_count>(len*8)/10)
	{
		type.encoding="UCS-2BE";
		type.bom=false;
		return true;
	}

	type=NullEncoding;
	return true; 
}

bool CCodepage::ValidEncoding(const char *enc)
{
	iconv_t ic;
	if(!strcmp(enc,locale_charset()))
		return true;
	if((ic = iconv_open(enc,locale_charset()))==(iconv_t)-1)
	{
		CServerIo::trace(3,"ValidEncoding(%s,%s) returned false",enc,locale_charset());
		return false;
	}
	iconv_close(ic);
	return true;
}

bool CCodepage::BeginEncoding(const Encoding& from, const Encoding& to)
{
	m_blockcount=0;
	m_from = from;
	m_to = to;
	m_ic = 0;

	return true;
}

int CCodepage::SetBytestream()
{
	if(m_blockcount)
	  return 0;

	if(((!m_from.encoding && !m_to.encoding) || !strcmp(m_from.encoding?m_from.encoding:locale_charset(), m_to.encoding?m_to.encoding:locale_charset())))
	{
	    m_blockcount=-1;
		return 0;
	}

	if((m_ic = iconv_open(m_to.encoding?m_to.encoding:locale_charset(),m_from.encoding?m_from.encoding:locale_charset()))==(iconv_t)-1)
	{
		CServerIo::trace(3,"SetBytestream(%s,%s) failed",m_to.encoding?m_to.encoding:locale_charset(),m_from.encoding?m_from.encoding:locale_charset());
		return -1;
	}
	m_blockcount++;
	return 1;
}

bool CCodepage::EndEncoding()
{
	if(m_ic && m_blockcount>=0)
	  iconv_close((iconv_t)m_ic);
	m_ic = 0;
	return true;
}

int CCodepage::ConvertEncoding(const void *inbuf, size_t len, void *&outbuf, size_t& outlen)
{
	size_t in_remaining;
	size_t out_remaining;
	const char *inbufp = (const char *)inbuf;
	char *outbufp = (char *)outbuf;

	if(!len)
		return 0;

	if(m_blockcount<0)
	  return 0; /* A previous encoding failed, so we've stopped */

	if(!m_blockcount)
	{
		GuessEncoding((const char *)inbuf,len,m_from, m_from);

		if(((!m_from.encoding && !m_to.encoding) || !strcmp(m_from.encoding?m_from.encoding:locale_charset(), m_to.encoding?m_to.encoding:locale_charset())) && m_from.bom==m_to.bom)
		{
			m_blockcount=-1;
			return 0;
		}

		if((m_ic = iconv_open(m_to.encoding?m_to.encoding:locale_charset(),m_from.encoding?m_from.encoding:locale_charset()))==(iconv_t)-1)
		{
			CServerIo::trace(3,"ConvertEncoding(%s,%s) failed",m_to.encoding?m_to.encoding:locale_charset(),m_from.encoding?m_from.encoding:locale_charset());
			return -1;
		}
	}

	if(!outbuf)
	{
		outlen = (len * 4) + 4; /* Enough for ansi -> ucs4-le + a BOM */
		outbuf = malloc(outlen);
		outbufp= (char*)outbuf;
	}

	in_remaining = len;
	out_remaining = outlen;
	if(!m_blockcount)
	{
		if(m_from.bom)
		{
			if(!strcmp(m_from.encoding,UTF8_CHARSET)) { if(in_remaining>2 && (((const char *)inbuf)[0]==(char)0xef && ((const char *)inbuf)[1]==(char)0xbb && ((const char *)inbuf)[2]==(char)0xbf)) { inbufp+=3; in_remaining-=3; } }
			else if(!strcmp(m_from.encoding,"UCS-2LE")) { if(((const char *)inbuf)[0]==(char)0xff && ((const char *)inbuf)[1]==(char)0xfe) { inbufp+=2; in_remaining-=2; } }
			else if(!strcmp(m_from.encoding,"UCS-2BE")) { if(((const char *)inbuf)[0]==(char)0xfe && ((const char *)inbuf)[1]==(char)0xff) { inbufp+=2; in_remaining-=2; } }
		}

		if(m_to.bom)
		{
			if(!strcmp(m_to.encoding,UTF8_CHARSET)) { ((char *)outbuf)[0]=(char)0xef; ((char *)outbuf)[1]=(char)0xbb; ((char *)outbuf)[2]=(char)0xbf; outbufp+=3; out_remaining-=3; }
			else if(!strcmp(m_to.encoding,"UCS-2LE")) { ((char *)outbuf)[0]=(char)0xff; ((char *)outbuf)[1]=(char)0xfe; outbufp+=2; out_remaining-=2; }
			else if(!strcmp(m_to.encoding,"UCS-2BE")) { ((char *)outbuf)[0]=(char)0xfe; ((char *)outbuf)[1]=(char)0xff; outbufp+=2; out_remaining-=2; }
		}
	}
	m_blockcount++;

	if(iconv((iconv_t)m_ic,(iconv_arg2_t)&inbufp,&in_remaining,&outbufp,&out_remaining)<0)
		return -1;
	outlen-= out_remaining;
	return 1;
}

/* The input will either be UTF-8 or some kind of codepage from the server.
   Expand the buffer to the dest, and also expand a CR and CRLF buffer (so
   we don't have to worry about platform specifics).  Then add the cr/lf pairs
   as required.

   For Unix we don't have to any of this, of course :) */
int CCodepage::OutputAsEncoded(int fd, const void *inbuf, size_t len, LineType CrLf)
{
	void *outbuf;

#ifdef _WIN32
	setmode(fd,_O_BINARY); // Don't want CRLF default stuff
#endif

	if(CrLf!=ltLf)
	{
		char *p = (char*)inbuf, *o, *pinbuf=(char*)inbuf;
		size_t l;
		static const char crlf[] = { '\r', '\n' };
		static const char lfcr[] = { '\n', '\r' };
		static const char cr[] = { '\r' };

		const char *line_end;
		size_t line_end_len;

		switch(CrLf)
		{
			case ltCr: line_end = cr; line_end_len=sizeof(cr); break;
			case ltCrLf: line_end = crlf; line_end_len=sizeof(crlf); break;
			case ltLfCr: line_end = lfcr; line_end_len=sizeof(lfcr); break;
			default:
				assert(0);
				line_end = crlf; line_end_len=sizeof(crlf); break;
		};

		while((len-(pinbuf-(char*)inbuf))>0 && (p=(char*)memchr(p,'\n',len-(p-(char*)inbuf)))!=NULL)
		{
			o=pinbuf;
			l=p-pinbuf;
			p++;
			pinbuf=p;
			outbuf=NULL;
			if(l)
			{
				if(ConvertEncoding(o,l,outbuf,l))
					o=(char*)outbuf;

				if(write(fd,o,(unsigned)l)<(int)l)
				{
					if(outbuf)
						free(outbuf);
					/* error - reported by caller */
					return 1;
				}
			}
			o=(char*)line_end;
			if(l<8 && outbuf)
			{
				free(outbuf);
				outbuf=NULL;
			}
			if(ConvertEncoding(o,line_end_len,outbuf,l))
				o=(char*)outbuf;
			else
				l=line_end_len;
			if(write(fd,o,(unsigned)l)<(int)l)
			{
				if(outbuf)
					free(outbuf);
				/* error - reported by caller */
				return 1;
			}
			if(outbuf)
				free(outbuf);
		}
		l=len-(pinbuf-(char*)inbuf);
		if(l>0)
		{
			outbuf=NULL;
			o=pinbuf;
			if(ConvertEncoding(o,l,outbuf,len))
			{
				o=(char*)outbuf;
				l=len;
			}
			if(write(fd,o,(unsigned)l)<(int)l)
			{
				if(outbuf)
					free(outbuf);
				/* error - reported by caller */
				return 1;
			}
			if(outbuf)
				free(outbuf);
		}
		return 0;
	}
	else
	{
		outbuf = NULL;
		char *p = (char*)inbuf, *o = (char*)inbuf;
		size_t l = len;
		if(ConvertEncoding(o,l,outbuf,l))
			o=(char*)outbuf;
		if(write(fd,o,(unsigned)l)<(int)l)
		{
			if(outbuf)
				free(outbuf);
			/* error - reported by caller */
			return 1;
		}
		if(outbuf)
			free(outbuf);
		return 0;
	}
}

bool CCodepage::StripCrLf(void *buf, size_t& len)
{
	char *p = (char*)buf;
	while(len-(p-(char*)buf)>0 && (p=(char*)memchr(p,'\r',len-(p-(char*)buf)))!=NULL)
	{
		// CR/LF or LF/CR (windows text)
		if((p>buf && *(p-1)=='\n') || (len-(p-(char*)buf)>1 && *(p+1)=='\n'))
		{
			if(len-(p-(char*)buf)>1)
				memmove(p,p+1,(len-(p-(char*)buf))-1);
			len--;
		}
		else // CR only (mac text)
			*p='\n';
	}
	return true;
}

const char *CCodepage::GetDefaultCharset()
{
	return locale_charset();
}

/* Fast transcode, used by the client to cover filenames etc. */
int CCodepage::TranscodeBuffer(const char *from, const char *to, const void *inbuf, size_t len, void *&outbuf, size_t& olen)
{
	const char *inbufp=(const char *)inbuf;
	size_t in_remaining = len?len:strlen(inbufp)+1;
	char *outbufp;
	size_t outlen, out_remaining = outlen = in_remaining*4;
	iconv_t ic;
	int chars_deleted = 0;

	outbuf=malloc(in_remaining*4);
	outbufp=(char*)outbuf;

	to = CheckAbbreviations(to);
	from = CheckAbbreviations(from);

	if(!strcmp(from,to) || (ic = iconv_open(to,from))==(iconv_t)-1)
	{
		CServerIo::trace(3,"TranscodeBuffer(%s,%s) failed",to,from);
		strcpy((char *)outbuf,(const char *)inbuf);
		return -1;
	}
//	CServerIo::trace(4,"Transcode %s",inbuf);
	do
	{

		if(iconv(ic,(iconv_arg2_t)&inbufp,&in_remaining,&outbufp,&out_remaining)<0)
		{
			CServerIo::trace(3,"Transcode between %s and %s failed",from,to);
			strcpy((char *)outbuf,(const char *)inbuf);
			return -1;
		}
		if(in_remaining)
		{
			inbufp++; in_remaining--;
			chars_deleted++;
		}
	} while(in_remaining);
	if(chars_deleted)
		CServerIo::trace(3,"Transcode: %d characters deleted",chars_deleted);
	iconv_close(ic);
	olen = outlen - out_remaining;
	if(!len)
		olen--; /* Compensate for NULL */
//	if(olen)
//		CServerIo::trace(4,"Transcode returned %-*.*s",olen,olen,outbuf);
	return chars_deleted;
}
