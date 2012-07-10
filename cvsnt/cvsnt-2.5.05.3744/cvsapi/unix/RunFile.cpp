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
/* Unix specific */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <system.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>

#include "../RunFile.h"
#include "../ServerIO.h"

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
	m_child = 0;
}

CRunFile::~CRunFile()
{
	delete m_args;
}

bool CRunFile::resetArgs()
{
	return m_args->resetArgs();
}

bool CRunFile::setArgs(const char *args)
{
	return m_args->setArgs(args);
}


bool CRunFile::setArgs(int argc, const char *const*argv)
{
	return m_args->setArgs(argc, argv);
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

bool CRunFile::setInput(int (*inputFn)(char *,size_t,void*), void *userData)
{
	m_inputFn = inputFn;
	m_inputData = userData;
	return true;
}

bool CRunFile::setOutput(int (*outputFn)(const char *,size_t,void*), void *userData)
{
	m_outputFn = outputFn;
	m_inputData = userData;
	return true;
}

bool CRunFile::setError(int (*errorFn)(const char *,size_t,void*), void *userData)
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

/* On Unix, the bShow parameter does nothing */
bool CRunFile::run(const char *path, bool bShow /* = false */)
{
  	int pid;
  	int fd1[2],fd2[2],fd3[2];

	const char* args=m_args->toString();
	CServerIo::trace(3,"CreateProcess(%s,%s)",path?path:"",args?args:"");

	if(m_inputFn && m_inputFn != StandardInput)
  	{
		pipe(fd1);
		m_inFd=fd1[PIPE_WRITE];
    }
    else
      m_inFd=-1;

	if(m_outputFn && m_outputFn != StandardOutput)
    {
        pipe(fd2);
        m_outFd=fd2[PIPE_READ];
    }
    else
        m_outFd=-1;

	if(!m_errorFn)
		m_errorFn = m_outputFn;

	if(m_errorFn && m_errorFn != StandardError)
    {
        pipe(fd3);
        m_errFd=fd3[PIPE_READ];
    }
	else
		m_errFd=-1;

	if(path)
	  m_args->insertArg(0,path);

	pid = fork();
	if(pid<0)
		return false;
	
	signal(SIGPIPE,SIG_IGN);
	if(!pid) /* Child */
	{	
		int nullfd = open("/dev/null",O_RDWR);

		if(m_inFd>=0)
		{
		  close(fd1[PIPE_WRITE]);
		  dup2(fd1[PIPE_READ],0);
		}
		else if(!m_inputFn)
			dup2(nullfd,0);
		if(m_outFd>=0)
		{
		  close(fd2[PIPE_READ]);
		  dup2(fd2[PIPE_WRITE],1);
		}
		else if(!m_outputFn)
			dup2(nullfd,1);
		if(m_errFd>=0)
		{
		  close(fd2[PIPE_READ]);
		  dup2(fd2[PIPE_WRITE],2);
		}
		else if(!m_errorFn)
			dup2(nullfd,2);

		close(nullfd);

		char **myargv = (char**)m_args->toArgv();
		execvp (myargv[0], myargv);
		perror("Exec failed");
		exit(-1);
	}

	if(m_inFd>=0)
		close(fd1[PIPE_READ]);
	if(m_outFd>=0)
		close(fd2[PIPE_WRITE]);
	if(m_errFd>=0)
		close(fd3[PIPE_WRITE]);

	m_child = pid;

	return true;
}

/* Unix gets no I/O until wait() is called - if we had
   reliable threads this wouldn't be necessary... fork()
   isolates the address spaces so is not suitable for this
   task */
bool CRunFile::wait(int& result, int timeout)
{	
	char buf[BUFSIZ],inbuf[BUFSIZ];
	int status,size,wsize,w,l;

	CServerIo::trace(3,"wait() called, m_child=%d",m_child);
	
	if(!m_child)
	  return -1;
		
	if(m_outFd>=0)
		fcntl(m_outFd,F_SETFL, O_NONBLOCK);
	if(m_errFd>=0)
    		fcntl(m_errFd,F_SETFL, O_NONBLOCK);
	if(m_inFd>=0)
    		fcntl(m_errFd,F_SETFL, O_NONBLOCK);

	size = 0;
	if(m_inFd>=0)
	{
		size=l=m_inputFn(inbuf,sizeof(inbuf),m_inputData);
		if(size<=0)
		{
			close(m_inFd);
			m_inFd=-1;
		}
	}
	w = waitpid(m_child,&status,WNOHANG);
	while((timeout==-1 || timeout>0) && ((m_inFd>=0 && size>0) || m_outFd>=0 || m_errFd>=0) && !w)
	{
		if(m_inFd>=0 && size>0)
		{
		  int s = write(m_inFd, inbuf+l-size,size);
		  if(m_debugFn) m_debugFn(0,inbuf+l-size,size,m_debugData);
		  if(s<0)
		  {
		    close(m_inFd);
		    m_inFd=-1;
		  }
		  if(s)
		  {
		    size-=s;
		    if(!size)
		      size=l=m_inputFn(inbuf,sizeof(inbuf),m_inputData);
		    if(size<=0)
		    {
			close(m_inFd);
			m_inFd=-1;
		    }
		  }
		}
		if(!w)
			w = waitpid(m_child,&status,WNOHANG);
		wsize=0;
      	while(!w && m_errFd>=0 && (wsize=read(m_errFd,buf,sizeof(buf)))>0)
		{
       		(m_errorFn?m_errorFn:m_outputFn)(buf,wsize,m_errorFn?m_errorData:m_outputData);
			if(m_debugFn) m_debugFn(2,buf,wsize,m_debugData);
		}
		if(wsize<0 && errno!=EAGAIN)
		{
			close(m_errFd);
			m_errFd=-1;
		}
		if(!w)
			w = waitpid(m_child,&status,WNOHANG);
		wsize=0;
   		while(!w && m_outFd>=0 && (wsize=read(m_outFd,buf,sizeof(buf)))>0)
		{
			m_outputFn(buf,wsize,m_outputData);
			if(m_debugFn) m_debugFn(1,buf,wsize,m_debugData);
		}
		if(wsize<0 && errno!=EAGAIN)
		{
			close(m_outFd);
			m_outFd=-1;
		}
		if(!w)
		{
		  usleep(100);
		  w = waitpid(m_child,&status,WNOHANG);
		}
		if(timeout!=-1)
		{
			timeout-=100;
			if(timeout==-1) timeout--;
		}
	}
	if(!w && timeout!=-1 && timeout<=0) /* timed out */
	   return false;
	if(m_inFd)
	{
		close(m_inFd);
		m_inFd=-1;
	}

	/* If we've fallen through all of the above with the app still running, wait for it */
	if(!w)
	{
		if(timeout==-1)
			waitpid(m_child,&status,0);
		else
		{
			while(!w && timeout>0)
			{
			   w=waitpid(m_child,&status,WNOHANG);
		 	   usleep(100);
			   timeout-=100;
			}
			if(!w)
			   return false;
		}
	}     	
	else /* App died.. soak up stdio */
	{
		CServerIo::trace(3,"Process finished");
      	while(m_errFd>=0 && (wsize=read(m_errFd,buf,sizeof(buf)))>0)
		{
       		(m_errorFn?m_errorFn:m_outputFn)(buf,wsize,m_errorFn?m_errorData:m_outputData);
			if(m_debugFn) m_debugFn(2,buf,wsize,m_debugData);
		}
      	while(m_outFd>=0 && (wsize=read(m_outFd,buf,sizeof(buf)))>0)
		{
       		m_outputFn(buf,wsize,m_outputData);
			if(m_debugFn) m_debugFn(1,buf,wsize,m_debugData);
		}
		if(m_outFd>=0)
		{
			close(m_outFd);	
			m_outFd=-1;
		}
		if(m_errFd>=0)
		{
			close(m_errFd);
			m_errFd=-1;
		}
	}

	m_exitCode=result=WEXITSTATUS(status);
	CServerIo::trace(3,"Exit status is %d",m_exitCode);
	return true;
}

// vi:ts=4:sw=4 
