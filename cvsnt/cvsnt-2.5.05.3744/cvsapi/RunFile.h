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
#ifndef RUNFILE__H
#define RUNFILE__H

#include "TokenLine.h"

class CRunFile
{
public:
	CVSAPI_EXPORT CRunFile();
	CVSAPI_EXPORT virtual ~CRunFile();

	CVSAPI_EXPORT bool resetArgs();
	CVSAPI_EXPORT bool setArgs(int argc, const char *const*argv);
	CVSAPI_EXPORT bool setArgs(const char *args);
	CVSAPI_EXPORT bool addArg(const char *arg);
	CVSAPI_EXPORT bool addArgs(const char *args);
	CVSAPI_EXPORT const char *toString();
	CVSAPI_EXPORT bool setInput(int (*inputFn)(char *,size_t,void *), void *userData);
	CVSAPI_EXPORT bool setOutput(int (*outputFn)(const char *,size_t,void *), void *userData);
	CVSAPI_EXPORT bool setError(int (*outputFn)(const char *,size_t,void *), void *userData);
	CVSAPI_EXPORT bool setDebug(int (*debugFn)(int type, const char *,size_t, void *), void *userData);
	CVSAPI_EXPORT bool run(const char *path, bool bShow = false);
	CVSAPI_EXPORT bool wait(int& result, int timeout = -1);
	CVSAPI_EXPORT bool terminate();

	CVSAPI_EXPORT static int (*const StandardInput)(char *,size_t, void *);
	CVSAPI_EXPORT static int (*const StandardOutput)(const char *,size_t, void *);
	CVSAPI_EXPORT static int (*const StandardError)(const char *,size_t, void *);

protected:
	CTokenLine* m_args;
	int m_inFd,m_outFd,m_errFd;
	unsigned long m_exitCode;

	int (*m_inputFn)(char *buf, size_t len, void *userData);
	int (*m_outputFn)(const char *buf, size_t len, void *userData);
	int (*m_errorFn)(const char *buf, size_t len, void *userData);
	int (*m_debugFn)(int type, const char *buf, size_t len, void *userData);
	void *m_inputData,*m_outputData,*m_errorData,*m_debugData;

#ifdef _WIN32
	void *m_hProcess;
	void *m_hThread[2];
	static unsigned __stdcall _inputThreadProc(void *param);
	static unsigned __stdcall _outputThreadProc(void *param);
	unsigned inputThreadProc();
	unsigned outputThreadProc();
	void SetBlock(int fd, bool block);
#else
	pid_t m_child;
#endif
};

#endif

