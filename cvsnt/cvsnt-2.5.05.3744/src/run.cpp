/* run->c --- routines for executing subprocesses.
   
   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"

/*
 * To exec a program under CVS, first call run->setup() to setup initial
 * arguments.  The argument to run->setup will be parsed into whitespace 
 * separated words and added to the global run->argv list.
 * 
 * Then, optionally call run->arg() for each additional argument that you'd like
 * to pass to the executed program.
 * 
 * Finally, call run->exec() to execute the program with the specified arguments.
 * The execvp() syscall will be used, so that the PATH is searched correctly.
 * File redirections can be performed in the call to run->exec().
 */
CRunFile *run;

void run_setup (const char *prog)
{
  if(!run)
    run = new CRunFile;
    CServerIo::trace(3,"run->setup %s",PATCH_NULL(prog));
	run->resetArgs();
	run->setArgs(prog);
}

void run_arg (const char *s)
{
    CServerIo::trace(3,"run->arg %s",PATCH_NULL(s));

	run->addArg(s);
}

static int run_out(const char *buf,size_t len,void * /*userdata*/)
{
	return cvs_output(buf,len);
}

static int run_err(const char *buf,size_t len,void * /*userdata*/)
{
	return cvs_outerr(buf,len);
}

int run_exec (bool bShow)
{
	int status;

	CServerIo::trace(3,"run->exec()");

	cvs_flusherr();
	cvs_flushout();

	if(!server_active)
	{
		run->setInput(CRunFile::StandardInput,NULL);
		run->setOutput(CRunFile::StandardOutput,NULL);
		run->setError(CRunFile::StandardError,NULL);
	}
	else
	{
		run->setInput(NULL,NULL);
		run->setOutput(run_out,NULL);
		run->setError(run_err,NULL);
	}

	if(!run->run(NULL,bShow))
	{
		CServerIo::trace(3,"run->exec failed");
	}
	run->wait(status);

	cvs_flusherr();
	cvs_flushout();

	return status;
}

void run_print (FILE *fp)
{
    int (*outfn)(const char *, size_t);

    if (fp == stderr)
		outfn = cvs_outerr;
    else if (fp == stdout)
		outfn = cvs_output;
    else
    {
		error (1, 0, "internal error: bad argument to run->print");
		return;
    }

	(*outfn)(run->toString(),0);
}

