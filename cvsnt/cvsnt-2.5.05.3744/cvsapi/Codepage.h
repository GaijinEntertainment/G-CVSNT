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
#ifndef CODEPAGE__H
#define CODEPAGE__H

enum LineType
{
	ltLf,
	ltCr,
	ltCrLf,
	ltLfCr
};

class CCodepage
{
public:
	CVSAPI_EXPORT CCodepage();
	virtual CVSAPI_EXPORT ~CCodepage();

	struct Encoding
	{
		Encoding() { encoding=NULL; bom=false; }
		Encoding(const char *enc, bool b) { encoding=enc; bom=b; }
		const char *encoding;
		bool bom;
	};
	static CVSAPI_EXPORT Encoding Utf8Encoding;
	static CVSAPI_EXPORT Encoding NullEncoding;

	static CVSAPI_EXPORT bool ValidEncoding(const char *enc);
	CVSAPI_EXPORT bool BeginEncoding(const Encoding& from, const Encoding& to);
	CVSAPI_EXPORT bool EndEncoding();
	CVSAPI_EXPORT int SetBytestream();
	CVSAPI_EXPORT int ConvertEncoding(const void *inbuf, size_t len, void *&outbuf, size_t& outlen);
	CVSAPI_EXPORT int OutputAsEncoded(int fd, const void *inbuf, size_t len, LineType CrLf);
	CVSAPI_EXPORT bool StripCrLf(void *buf, size_t& len);
	CVSAPI_EXPORT bool AddCrLf(void *&buf, size_t& len);
	static CVSAPI_EXPORT const char *GetDefaultCharset();
	static CVSAPI_EXPORT int TranscodeBuffer(const char *from, const char *to, const void *inbuf, size_t len, void *&outbuf, size_t& olen);

protected:
	void *m_ic;
	int m_blockcount;
	Encoding m_from, m_to;

	static const char *CheckAbbreviations(const char *cp);
	bool GuessEncoding(const char *buf, size_t len, Encoding& type, const Encoding& hint);
};

#endif
