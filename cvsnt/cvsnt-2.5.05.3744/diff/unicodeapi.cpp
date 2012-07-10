#include <config.h>
#include <system.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>

#include <cvsapi.h>

#include "unicodeapi.h"

/* The 'default' encoding is for UTF8-BOM for storage in the RCS files */
const diff_encoding_type __diff_encoding_utf8 = { UTF8_CHARSET ,1 };

static CCodepage g_cp;

int begin_encoding(const diff_encoding_type *from)
{
	CCodepage::Encoding f,t;

	f.encoding = from->encoding;
	f.bom = from->bom?true:false;

	if(g_cp.BeginEncoding(f,CCodepage::Utf8Encoding))
		return 0;
	return -1;
}

int end_encoding()
{
	if(g_cp.EndEncoding())
		return 0;
	return -1;
}

int convert_encoding(const char *inbuf, size_t len, char **outbuf, size_t *outlen)
{
	return g_cp.ConvertEncoding((const void *)inbuf,len,(void*&)*outbuf,*outlen);
}

int strip_crlf(char *buf, size_t *len)
{
	if(g_cp.StripCrLf((void*)buf,*len))
		return 0;
	return -1;
}

