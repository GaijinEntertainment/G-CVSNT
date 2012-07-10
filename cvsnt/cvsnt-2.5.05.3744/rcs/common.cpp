/*	cvsnt rcs wrapper common code
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

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
#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "common.h"

int rcs_main(const char *command, int argc, char *argv[])
{
#ifdef _WIN32
	std::string s;
	LPSTR szCmd = GetCommandLine();
	char *p=szCmd;
	int inquote=0;

	while(*p && (inquote || !isspace(*p)))
	{
		if(*p=='"')
			inquote=!inquote;
		p++;
	}
	szCmd=*p?p+1:p;
	s="cvs rcsfile ";
	s+=command;
	s+=" ";
	s+=szCmd;

	STARTUPINFO si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi = {0};
	DWORD exit;

	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	if(!CreateProcess(NULL,(LPSTR)s.c_str(),NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))
		return -1;
	CloseHandle(pi.hThread);
	WaitForSingleObject(pi.hProcess,INFINITE);
	GetExitCodeProcess(pi.hProcess,&exit);
	CloseHandle(pi.hProcess);
	return (int)exit;
#else
	int n;
	char **nargv = (char**)malloc((argc+3)*sizeof(char*));

	nargv[0]="cvs";
	nargv[1]="rcsfile";
	nargv[2]=(char*)command;
	for(n=1; n<argc; n++)
		nargv[n+2]=argv[n];
	nargv[n+2]=NULL;

	execvp(nargv[0],nargv);
	perror("Couldn't run cvs");
	return -1;
#endif
}

