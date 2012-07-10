/*
	CVSNT Generic API
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
/* Unix specific */
#include <config.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ltdl.h>
#include <glob.h>

#include <config.h>
#include "../lib/api_system.h"

#include "../cvs_string.h"
#include "../FileAccess.h"
#include "../ServerIO.h"
#include "../LibraryAccess.h"

namespace
{
	static unsigned initcount;
	static char save_nls_lang[200], new_nls_lang[200];
	static char save_nls_nchar[200], new_nls_nchar[200];
	static char save_oracle_home[200], new_oracle_home[200];
	static void *save_m_lib;

	static int dlref()
	{
	if(!initcount++)
		lt_dlinit();
	}

	static int dlunref()
	{
	if(!--initcount)
		lt_dlexit();
	}
};

CLibraryAccess::CLibraryAccess(const void *lib /*= NULL*/)
{
	m_lib = (void*)lib;
	save_m_lib = (void*)NULL;
	new_nls_lang[0]='\0';
	new_nls_nchar[0]='\0';
	new_oracle_home[0]='\0';
	save_nls_lang[0]='\0';
	save_nls_nchar[0]='\0';
	save_oracle_home[0]='\0';
}

CLibraryAccess::~CLibraryAccess()
{
	Unload();
}

bool CLibraryAccess::Load(const char *name, const char *directory)
{
	if(m_lib)
		Unload();

	if (strncmp(name,"oracle",6)==0)
	{
		/* this is very messy, but the server crashes/hangs
		   when the oracle library is unloaded due to the
	      	   putenv ... */

	      /* For this kind of thing better to put a load/unload hook in the
		 library itself, otherwise it just gets too messy hardcoding
		 everything. */

	      CServerIo::trace(3,"It is ORACLE so save environment.");
	      strcpy(save_nls_lang,getenv("NLS_LANG"));
	      strcpy(save_nls_nchar,getenv("NLS_NCHAR"));
	      strcpy(save_oracle_home,getenv("ORACLE_HOME"));
	      CServerIo::trace(3," NLS_LANG=%s",save_nls_lang);
	      CServerIo::trace(3," NLS_NCHAR=%s",save_nls_nchar);
	      CServerIo::trace(3," ORACLE_HOME=%s",save_oracle_home);
	}

	cvs::filename fn;
	if(directory && *directory)
		cvs::sprintf(fn,256,"%s/%s",directory,name);
	else
		fn = name;

	VerifyTrust(fn.c_str(),false);

	dlref();	
	m_lib = (void*)lt_dlopenext(fn.c_str());

	if(!m_lib)
	{
		CServerIo::trace(3,"LibraryAccess::Load failed for '%s', error = %d %s",fn.c_str(),errno, lt_dlerror());
		dlunref();
		return false;
	}

	if (strncmp(name,"oracle",6)==0)
	      save_m_lib=m_lib;

	return true;
}

bool CLibraryAccess::Unload()
{
	if(!m_lib)
		return true;

	if (m_lib==save_m_lib)
	{
		/* this is very messy, but the server crashes/hangs
		   when the oracle library is unloaded due to the
	      	   putenv ... 

		   Since I am not sure if oracle.la ever "really" gets 
		   unloaded, I'm also resetting the environment when
		   info.la gets unloaded...
		   */
	      CServerIo::trace(3,"It is ORACLE so restore environment.");
	      strcpy(new_nls_lang,"NLS_LANG=");
	      strcpy(new_nls_nchar,"NLS_NCHAR=");
	      strcpy(new_oracle_home,"ORACLE_HOME=");
	      strcat(new_nls_lang,save_nls_lang);
	      strcat(new_nls_nchar,save_nls_nchar);
	      strcat(new_oracle_home,save_oracle_home);
	      putenv(new_nls_lang);
	      putenv(new_nls_nchar);
	      putenv(new_oracle_home);
	      if (m_lib==save_m_lib) 
		CServerIo::trace(3,"Unloading oracle");
	      CServerIo::trace(3," NLS_LANG=%s",save_nls_lang);
	      CServerIo::trace(3," NLS_NCHAR=%s",save_nls_nchar);
	      CServerIo::trace(3," ORACLE_HOME=%s",save_oracle_home);
	}
	lt_dlclose((lt_dlhandle)m_lib);
	dlunref();
	m_lib = NULL;
	return true;
}

void *CLibraryAccess::GetProc(const char *name)
{
	if(!m_lib)
		return NULL;

	return lt_dlsym((lt_dlhandle)m_lib,name);
}

void *CLibraryAccess::Detach()
{
	void *lib = m_lib;
	m_lib = NULL;
	return lib;
}

void CLibraryAccess::VerifyTrust(const char *module, bool must_exist)
{
	// 
}
