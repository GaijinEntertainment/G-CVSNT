/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
#ifndef SSPIHANDLER__H
#define SSPIHANDLER__H

class CSSPIHandler
{
public:
	CVSAPI_EXPORT CSSPIHandler();
	CVSAPI_EXPORT ~CSSPIHandler();

	CVSAPI_EXPORT bool init(const char *protocol = NULL);
	CVSAPI_EXPORT bool ClientStart(bool encrypt, bool& more, const char *name, const char *pwd, const char *tokenSource = NULL);
	CVSAPI_EXPORT bool ClientStep(bool& more, const char *inputBuffer, size_t inputSize);
	CVSAPI_EXPORT const unsigned char *getOutputBuffer(size_t& length) { length=m_outputBuffer.size(); return (const unsigned char *)m_outputBuffer.c_str(); }

protected:
#ifdef _WIN32
	bool m_broken_file_sharing;
	bool m_haveContext;
	const char *m_tokenSource;
	DWORD m_rc;
	CredHandle m_credHandle;
	CtxtHandle m_contextHandle;
	SecPkgInfoA *m_secPackInfo;
	SecPkgContext_Sizes m_secSizes;
	cvs::string m_currentProtocol;
	DWORD m_ctxReq;
	SEC_WINNT_AUTH_IDENTITY_A m_nameAndPwd;

	bool InitProtocol(const char *protocol);
#endif
	cvs::string m_outputBuffer;
};

#endif
