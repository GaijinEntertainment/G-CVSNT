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
/* WIN32 Specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <errno.h>

#include "../ServerIo.h"
#include "../RunFile.h"
#include "../FileAccess.h"

int (*const CRunFile::StandardInput)(char *,size_t, void *) = (int (*const)(char *,size_t, void *))-1;
int (*const CRunFile::StandardOutput)(const char *,size_t, void *) = (int (*const)(const char *,size_t, void *))-1;
int (*const CRunFile::StandardError)(const char *,size_t, void *) = (int (*const)(const char *,size_t, void *))-1;

enum enPipes
{
	PIPE_READ = 0,
	PIPE_WRITE = 1 
};

CRunFile::CRunFile()
{
	m_args = new CTokenLine;

	m_inputFn=NULL;
	m_outputFn=NULL;
	m_errorFn=NULL;
	m_debugFn=NULL;
	m_hProcess=NULL;
}

CRunFile::~CRunFile()
{
	delete m_args;

	if(m_hProcess)
		CloseHandle(m_hProcess);
}

bool CRunFile::resetArgs()
{
	return m_args->resetArgs();
}

bool CRunFile::setArgs(int argc, const char *const*argv)
{
	return m_args->setArgs(argc, argv);
}

bool CRunFile::setArgs(const char *args)
{
	return m_args->setArgs(args);
}

bool CRunFile::addArg(const char *arg)
{
	return m_args->addArg(arg);
}

bool CRunFile::addArgs(const char *args)
{
	return m_args->addArgs(args);
}

const char *CRunFile::toString()
{
	return m_args->toString(0);
}

bool CRunFile::setInput(int (*inputFn)(char *,size_t, void *), void *userData)
{
	m_inputFn = inputFn;
	m_inputData = userData;
	return true;
}

bool CRunFile::setOutput(int (*outputFn)(const char *,size_t, void *), void *userData)
{
	m_outputFn = outputFn;
	m_outputData = userData;
	return true;
}

bool CRunFile::setError(int (*errorFn)(const char *,size_t, void *), void *userData)
{
	m_errorFn = errorFn;
	m_errorData = userData;
	return true;
}

bool CRunFile::setDebug(int (*debugFn)(int type, const char *,size_t, void *), void *userData)
{
	m_debugFn = debugFn;
	m_debugData = userData;
	return true;
}

bool CRunFile::run(const char *path, bool bShow)
{
	STARTUPINFOW si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi;
	BOOL status;
	int fd1[2],fd2[2], fd3[2];
	int fdcp1, fdcp2, fdcp3;

	if(m_inputFn != StandardInput)
	{
		_pipe(fd1,0,_O_BINARY | _O_NOINHERIT);
		if(m_inputFn)
			m_inFd=fd1[PIPE_WRITE];
		else
			m_inFd=-1;
		fdcp1 = _dup(fd1[PIPE_READ]);
	}
	else
		m_inFd=-1;

	if(m_debugFn || (m_outputFn && m_outputFn != StandardOutput))
	{
		_pipe(fd2,0,_O_BINARY | _O_NOINHERIT);
		m_outFd=fd2[PIPE_READ];
		fdcp2 = _dup(fd2[PIPE_WRITE]);
	}
	else
		m_outFd=-1;

	if(!m_errorFn)
	{
		m_errorFn = m_outputFn;
		m_errorData = m_outputData;
	}

	if(m_debugFn || (m_errorFn && m_errorFn != StandardError))
	{
		_pipe(fd3,0,_O_BINARY | _O_NOINHERIT);
		m_errFd=fd3[PIPE_READ];
		fdcp3 = _dup(fd3[PIPE_WRITE]);
	}
	else 
		m_errFd=-1;

	/* The STARTUPINFO structure can specify handles to pass to the
	 child as its standard input, output, and error.  */
	si.hStdInput  = (m_inputFn==StandardInput)?GetStdHandle(STD_INPUT_HANDLE):(HANDLE)_get_osfhandle(fdcp1);
	si.hStdOutput = (m_outputFn==StandardOutput)?GetStdHandle(STD_OUTPUT_HANDLE):(HANDLE)(m_outFd>=0?_get_osfhandle(fdcp2):NULL);
	si.hStdError  = (m_errorFn==StandardError)?GetStdHandle(STD_ERROR_HANDLE):(HANDLE)(m_errFd>=0?_get_osfhandle (fdcp3):NULL);
	si.wShowWindow = bShow?SW_SHOWNORMAL:SW_HIDE;
	si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;

	LPSTR args=(LPSTR)m_args->toString();
	CServerIo::trace(3,"CreateProcess(%s,%s)",path?path:"",args?args:"");

	if(m_debugFn)
	{
		cvs::string tmp;
		if(path)
			cvs::sprintf(tmp,80,"\nExecuting: %s %s\n\n",path,args);
		else
			cvs::sprintf(tmp,80,"\nExecuting: %s\n\n",args);
		m_debugFn(4,tmp.c_str(),tmp.length(),m_debugData);
	}

	status = CreateProcessW(CFileAccess::Win32Wide(path), (LPWSTR)(LPCWSTR)CFileAccess::Win32Wide(args), NULL, NULL, TRUE, 0, 0, 0, &si, &pi);

	if (! status)
	{
		DWORD error_code = GetLastError ();
		switch (error_code)
		{
			case ERROR_NOT_ENOUGH_MEMORY:
			case ERROR_OUTOFMEMORY:
				errno = ENOMEM;
				break;
			case ERROR_BAD_EXE_FORMAT:
				errno = ENOEXEC;
				break;
			case ERROR_ACCESS_DENIED:
				errno = EACCES;
				break;
			case ERROR_NOT_READY:
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
			default:
				errno = ENOENT;
				break;
			}
			return false;
	}

	m_hProcess = pi.hProcess;

	m_hThread[0]=m_hThread[1]=NULL;
	if(m_outFd>=0 || m_errFd>=0)
		m_hThread[0]=(void*)_beginthreadex(NULL,0,_outputThreadProc,this,0,NULL);
	if(m_inFd>=0)
		m_hThread[1]=(void*)_beginthreadex(NULL,0,_inputThreadProc,this,0,NULL);
	else if(!m_inputFn)
		_close(fd1[PIPE_WRITE]);

	WaitForInputIdle(pi.hProcess,INFINITE);
	CloseHandle(pi.hThread);

	if(m_inFd>=0 || !m_inputFn)
	{
		_close(fd1[PIPE_READ]);
		_close(fdcp1);
	}

	if(m_outFd>=0)
	{
		_close(fd2[PIPE_WRITE]);
		_close(fdcp2);
	}
	if(m_errFd>=0)
	{	
		if(fd3[PIPE_WRITE]>=0)
			_close(fd3[PIPE_WRITE]);
		_close(fdcp3);
	}

	return true;
}

bool CRunFile::wait(int& result, int timeout /* = -1 */)
{
	DWORD exit = 0;

	if(m_hProcess)
	{
		if(WaitForSingleObject(m_hProcess,timeout)==WAIT_TIMEOUT)
			return false;
		if(!GetExitCodeProcess(m_hProcess,&exit))
		{
			DWORD dwErr = GetLastError();
			exit = dwErr;
		}
	}
	if(m_hThread[0])
	{
		WaitForSingleObject(m_hThread[0],INFINITE);
		CloseHandle(m_hThread[0]);
	}
	if(m_hThread[1])
	{
		WaitForSingleObject(m_hThread[1],INFINITE);
		CloseHandle(m_hThread[1]);
	}
	if(m_hProcess)
	{
		CloseHandle(m_hProcess);
		m_hProcess = NULL;
	}

	result=(int)exit;
	if(m_debugFn)
	{
		cvs::string tmp;
		cvs::sprintf(tmp,40,"\nProcess exited with code %d\n",result);
		m_debugFn(4,tmp.c_str(),tmp.length(),m_debugData);
	}
	return true;
}

void CRunFile::SetBlock(int fd, bool block)
{
	DWORD mode = block?PIPE_WAIT:PIPE_NOWAIT;
	SetNamedPipeHandleState((HANDLE)_get_osfhandle(fd),&mode,NULL,NULL);
}

unsigned CRunFile::_outputThreadProc(void *param)
{
	return ((CRunFile*)param)->outputThreadProc();
}

unsigned CRunFile::_inputThreadProc(void *param)
{
	return ((CRunFile*)param)->inputThreadProc();
}

unsigned CRunFile::outputThreadProc()
{
	char buf[BUFSIZ*10];
	int size;

	if(m_outFd>=0)
		SetBlock(m_outFd,false);
	if(m_errFd>=0)
		SetBlock(m_errFd,false);

	do
	{
		while(m_errFd>=0 && (size=read(m_errFd,buf,BUFSIZ*10))>0)
		{
			if(m_errorFn && m_errorFn != StandardError) m_errorFn(buf,size,m_errorData);
			if(m_debugFn) m_debugFn(2,buf,size,m_debugData);
		}
		while(m_outFd>=0 && (size=read(m_outFd,buf,BUFSIZ*10))>0)
		{
			if(m_outputFn && m_outputFn != StandardOutput) m_outputFn(buf,size,m_outputData);
			if(m_debugFn) m_debugFn(1,buf,size,m_debugData);
		}
	} while(WaitForSingleObject(m_hProcess,100)==WAIT_TIMEOUT);

	while(m_errFd>=0 && (size=read(m_errFd,buf,BUFSIZ*10))>0)
	{
		if(m_errorFn && m_errorFn != StandardError) m_errorFn(buf,size,m_errorData);
		if(m_debugFn) m_debugFn(2,buf,size,m_debugData);
	}
	while(m_outFd>=0 && (size=read(m_outFd,buf,BUFSIZ*10))>0)
	{
		if(m_outputFn && m_outputFn != StandardOutput) m_outputFn(buf,size,m_outputData);
		if(m_debugFn) m_debugFn(1,buf,size,m_debugData);
	}

	if(m_outFd>=0)
		close(m_outFd);
	if(m_errFd>=0)
		close(m_errFd);
	return 0;
}

unsigned CRunFile::inputThreadProc()
{
	char buf[BUFSIZ];
	int size=0,l;

	SetBlock(m_inFd,false);

	do
	{
		if(!size)
			size=l=m_inputFn(buf,sizeof(buf),m_inputData);
		if(size<=0)
			break;

		if(m_debugFn) m_debugFn(0,buf+l-size,size,m_debugData);
	
		int s = write(m_inFd,buf+l-size,size);
		if(s<=0)
			break;
		size-=s;
		if(!size)
			size=l=m_inputFn(buf,sizeof(buf),m_inputData);
	} while(size>=0 && WaitForSingleObject(m_hProcess,100)==WAIT_TIMEOUT);

	close(m_inFd);

	return 0;
}

bool CRunFile::terminate()
{
	if(m_hProcess)
		TerminateProcess(m_hProcess,-1);
	if(m_hThread[0])
	{
		WaitForSingleObject(m_hThread[0],INFINITE);
		CloseHandle(m_hThread[0]);
	}
	if(m_hThread[1])
	{
		WaitForSingleObject(m_hThread[1],INFINITE);
		CloseHandle(m_hThread[1]);
	}
	if(m_hProcess)
	{
		CloseHandle(m_hProcess);
		m_hProcess = NULL;
	}
	return true;
}
