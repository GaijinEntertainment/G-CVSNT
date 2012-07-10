/*
	CVSNT Helper application API
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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <cvsapi.h>
#include "export.h"
#include "../GlobalSettings.h"

namespace
{
	const char *_cvs_config_dir_default = CVS_CONFIG_DIR;
	const char *cvs_config_dir = NULL;
	const char *_cvs_library_dir_default = CVS_LIBRARY_DIR;
	const char *cvs_library_dir = NULL;
	const char *_cvs_program_name_default = "cvsnt";
	const char *cvs_program_name = NULL;

	int GetCachedPassword(const char *key, char *buffer, int buffer_len)
	{
		CSocketIO sock;

		/* No song and dance.. if there's no server listening, do nothing */
		if(!sock.create("127.0.0.1","32401",false))
			return -1;
		if(!sock.connect())
			return -1;
		if(sock.send(key,(int)strlen(key))<=0)
		{
			CServerIo::trace(1,"Error sending to password agent");
			return -1;
		}
		if(sock.recv(buffer,buffer_len)<=0)
		{
			CServerIo::trace(1,"Error receiving from password agent");
			return -1;
		}
		if(buffer[0]==-1) /* No passwd */
		{
			CServerIo::trace(2,"No password stored in passwd agent");
			return -1;
		}
		sock.close();
		return 0;
	}
	int SetCachedPassword(const char *key, const char *buffer)
	{
		CSocketIO sock;

		/* No song and dance.. if there's no server listening, do nothing */
		if(!sock.create("127.0.0.1","32401",false))
			return -1;
		if(!sock.connect())
			return -1;
		sock.close();
		return 0;
	}
	void GetUserConfigFile(const char *product, const char *key, cvs::filename& fn)
	{
  		struct passwd *pw = getpwuid(getuid());

		if(!product || !strcmp(product,"cvsnt"))
			product = "cvs";

		cvs::sprintf(fn,80,"%s/.%s",pw->pw_dir?pw->pw_dir:"",product);
  		mkdir(fn.c_str(),0777);
		cvs::sprintf(fn,80,"%s/.cvs/%s",pw->pw_dir?pw->pw_dir:"",key?key:"config");

  		CServerIo::trace(2,"Config file name %s",fn.c_str());
	}
	void GetGlobalConfigFile(const char *product, const char *key, cvs::filename& fn)
	{
		if(product && strcmp(product,"cvsnt"))
			CServerIo::error("Global setting for product '%s' not supported",product);
		cvs::sprintf(fn,80,"%s/%s",cvs_config_dir?cvs_config_dir:_cvs_config_dir_default,key?key:"config");
	}
};

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len)
{
	/* Special case for the 'cvspass' key */
	if((!product || !strcmp(product,"cvsnt")) && !strcmp(key,"cvspass") && !GetCachedPassword(value,buffer,buffer_len))
		return 0;

	return _GetUserValue(product,key,value,buffer,buffer_len);
}

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, int& ival)
{
	char tmp[32];
	if(_GetUserValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	ival = atoi(tmp);
	return 0;
}

int CGlobalSettings::GetUserValue(const char *product, const char *key, const char *value, cvs::string& sval)
{
	char tmp[512];
	if(_GetUserValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	sval = tmp;
	return 0;
}

int CGlobalSettings::_GetUserValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len)
{
	cvs::filename fn;
	char line[1024];
	FILE *f;
	char *p;

    GetUserConfigFile(product,key,fn);
    f=fopen(fn.c_str(),"r");
    if(f==NULL)
	{
	   CServerIo::trace(3,"Could not open %s",fn.c_str());
       return -1;
	}

    /* Read keys */
    while(fgets(line,sizeof(line),f))
    {
    	line[strlen(line)-1]='\0';
          	p=strchr(line,'=');
        if(p)
            *p='\0';
        if(!strcasecmp(value,line))
        {
	        if(p)
                strncpy(buffer,p+1,buffer_len);
        	else
          		*buffer='\0';
       		return 0;
       	}
	}
    fclose(f);
    return -1;
}

int CGlobalSettings::SetUserValue(const char *product, const char *key, const char *value, const char *buffer)
{
	if((!product || !strcmp(product,"cvsnt")) && !strcmp(key,"cvspass") && !SetCachedPassword(value,buffer) && buffer)
		return 0;
	return _SetUserValue(product,key,value,buffer);
}

int CGlobalSettings::_SetUserValue(const char *product, const char *key, const char *value, const char *buffer)
{
	cvs::filename fn,fn2;
	char line[1024];
	FILE *f,*o;
	char *p;
	bool found;

    CServerIo::trace(3,"SetUserValue(%s,%s)",key,value?value:"");

	GetUserConfigFile(product,key,fn);
	f=fopen(fn.c_str(),"r");
	if(f==NULL)
	{
		f=fopen(fn.c_str(),"w");
        if(f==NULL)
	    {
	      	CServerIo::trace(1,"Couldn't create config file %s",fn.c_str());
	        return -1;
										                }
            if(buffer)
	        	fprintf(f,"%s=%s\n",value,buffer);
		fclose(f);
		return 0;
	}
	cvs::sprintf(fn2,80,"%s.new",fn.c_str());
    o=fopen(fn2.c_str(),"w");
    if(o==NULL)
    {
	   	CServerIo::CServerIo::trace(1,"Couldn't create temporary file %s",fn2.c_str());
       	fclose(f);
		return -1;
	}

    /* Read keys */
	found=0;
	while(fgets(line,sizeof(line),f))
	{
		line[strlen(line)-1]='\0';
	    p=strchr(line,'=');
	    if(p)
			*p='\0';
        if(!strcasecmp(value,line))
	    {
	      	if(buffer)
	        {
	        	strcat(line,"=");
		        strcat(line,buffer);
		        fprintf(o,"%s\n",line);
			}
			found=1;
		}
	    else
	    {
	        if(p)
	   	    	*p='=';
	        fprintf(o,"%s\n",line);
	    }
	}
    if(!found && buffer)
       	fprintf(o,"%s=%s\n",value,buffer);
    fclose(f);
    fclose(o);
    rename(fn2.c_str(),fn.c_str());
    return 0;
}

int CGlobalSettings::SetUserValue(const char *product, const char *key, const char *value, int ival)
{
	char tmp[32];
	snprintf(tmp,sizeof(tmp),"%d",ival);
	return SetUserValue(product,key,value,tmp);
}

int CGlobalSettings::EnumUserValues(const char *product, const char *key, int value_num, char *value, int value_len, char *buffer, int buffer_len)
{
	cvs::filename fn;
	char line[1024];
  	FILE *f;
  	char *token,*v,*p;

  	GetUserConfigFile(product,key,fn);
  	f=fopen(fn.c_str(),"r");
  	if(f==NULL)
	{
		CServerIo::trace(3,"Could not open %s",fn.c_str());
     	return -1;
	}

  	/* Read keys */
  	while(fgets(line,sizeof(line),f))
  	{
    	line[strlen(line)-1]='\0';
    	if(line[0]=='#' || !strlen(line))
      		continue;
    	if(!value_num--)
    	{
      		for(token=line; isspace(*token); token++)
        		;
      		v=p=strchr(token,'=');
      		if(!p && !strlen(token))
        		continue;
      		if(p)
      		{
        		*p='\0';
        		v++;
      		}
      		for(;isspace(*p); p++)
        		*p='\0';
      		for(;v && isspace(*v); v++)
        		;
      		strncpy(value,token,value_len);
      		if(p && v && strlen(v))
        		strncpy(buffer,v,buffer_len);
      		else
        		*buffer='\0';
      		fclose(f);
      		return 0;
    	}
  	}
  	fclose(f);
  	return -1;
}

int CGlobalSettings::EnumUserKeys(const char *product, const char *key, int value_num, char *value, int value_len)
{
	// TBD
	return -1;
}

int CGlobalSettings::DeleteUserKey(const char *product, const char *key)
{
	cvs::filename fn;
  	GetUserConfigFile(product,key,fn);
  	return remove(fn.c_str());
}

int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, int& ival)
{
	char tmp[32];
	if(GetGlobalValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	ival = atoi(tmp);
	return 0;
}

int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, cvs::string& sval)
{
	char tmp[512];
	if(GetGlobalValue(product,key,value,tmp,sizeof(tmp)))
		return -1;
	sval = tmp;
	return 0;
}

int CGlobalSettings::GetGlobalValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len)
{
	cvs::filename fn;
	char line[1024];
	FILE *f;
	char *p;

    GetGlobalConfigFile(product,key,fn);
    f=fopen(fn.c_str(),"r");
    if(f==NULL)
	{
	   CServerIo::trace(3,"Could not open %s",fn.c_str());
       return -1;
	}

    /* Read keys */
    while(fgets(line,sizeof(line),f))
    {
    	line[strlen(line)-1]='\0';
          	p=strchr(line,'=');
        if(p)
            *p='\0';
        if(!strcasecmp(value,line))
        {
	        if(p)
                strncpy(buffer,p+1,buffer_len);
        	else
          		*buffer='\0';
       		return 0;
       	}
	}
    fclose(f);
    return -1;
}

int CGlobalSettings::SetGlobalValue(const char *product, const char *key, const char *value, const char *buffer)
{
	cvs::filename fn,fn2;
	char line[1024];
	FILE *f,*o;
	char *p;
	bool found;

    CServerIo::CServerIo::trace(3,"SetUserValue(%s,%s)",key,value?value:"");

	GetGlobalConfigFile(product,key,fn);
	f=fopen(fn.c_str(),"r");
	if(f==NULL)
	{
		f=fopen(fn.c_str(),"w");
        if(f==NULL)
	    {
	      	CServerIo::CServerIo::trace(1,"Couldn't create config file %s",fn.c_str());
	        return -1;
										                }
            if(buffer)
	        	fprintf(f,"%s=%s\n",value,buffer);
		fclose(f);
		return 0;
	}
	cvs::sprintf(fn2,80,"%s.new",fn.c_str());
    o=fopen(fn2.c_str(),"w");
    if(o==NULL)
    {
	   	CServerIo::CServerIo::trace(1,"Couldn't create temporary file %s",fn2.c_str());
       	fclose(f);
		return -1;
	}

    /* Read keys */
	found=0;
	while(fgets(line,sizeof(line),f))
	{
		line[strlen(line)-1]='\0';
	    p=strchr(line,'=');
	    if(p)
			*p='\0';
        if(!strcasecmp(value,line))
	    {
	      	if(buffer)
	        {
	        	strcat(line,"=");
		        strcat(line,buffer);
		        fprintf(o,"%s\n",line);
			}
			found=1;
		}
	    else
	    {
	        if(p)
	   	    	*p='=';
	        fprintf(o,"%s\n",line);
	    }
	}
    if(!found && buffer)
       	fprintf(o,"%s=%s\n",value,buffer);
    fclose(f);
    fclose(o);
    rename(fn2.c_str(),fn.c_str());
    return 0;
}

int CGlobalSettings::SetGlobalValue(const char *product, const char *key, const char *value, int ival)
{
	char tmp[32];
	snprintf(tmp,sizeof(tmp),"%d",ival);
	return SetGlobalValue(product,key,value,tmp);
}

int CGlobalSettings::EnumGlobalValues(const char *product, const char *key, int value_num, char *value, int value_len, char *buffer, int buffer_len)
{
	cvs::filename fn;
	char line[1024];
  	FILE *f;
  	char *token,*v,*p;

  	GetGlobalConfigFile(product,key,fn);
  	f=fopen(fn.c_str(),"r");
  	if(f==NULL)
	{
	    CServerIo::trace(3,"Could not open %s",fn.c_str());
     	return -1;
	}

  	/* Read keys */
  	while(fgets(line,sizeof(line),f))
  	{
    	line[strlen(line)-1]='\0';
    	if(line[0]=='#' || !strlen(line))
      		continue;
    	if(!value_num--)
    	{
      		for(token=line; isspace(*token); token++)
        		;
      		v=p=strchr(token,'=');
      		if(!p && !strlen(token))
        		continue;
      		if(p)
      		{
        		*p='\0';
        		v++;
      		}
      		for(;isspace(*p); p++)
        		*p='\0';
      		for(;v && isspace(*v); v++)
        		;
      		strncpy(value,token,value_len);
      		if(p && v && strlen(v))
        		strncpy(buffer,v,buffer_len);
      		else
        		*buffer='\0';
      		fclose(f);
      		return 0;
    	}
  	}
  	fclose(f);
  	return -1;
}

int CGlobalSettings::EnumGlobalKeys(const char *product, const char *key, int value_num, char *value, int value_len)
{
	// TBD
	return -1;
}

int CGlobalSettings::DeleteGlobalKey(const char *product, const char *key)
{
	cvs::filename fn;
  	GetGlobalConfigFile(product,key,fn);
  	return remove(fn.c_str());
}

const char *CGlobalSettings::GetConfigDirectory()
{
	return cvs_config_dir?cvs_config_dir:_cvs_config_dir_default;
}

const char *CGlobalSettings::GetLibraryDirectory(GLDType type)
{
	const char *cvsDir = cvs_library_dir?cvs_library_dir:_cvs_library_dir_default;
	/* At some point replace this with something more complex.  For now
	   it serves the purpose */
	static const char *cvsDirProtocols = NULL;
	static const char *cvsDirTriggers = NULL;
	static const char *cvsDirXdiff = NULL;
	static const char *cvsDirMdns = NULL;
	static const char *cvsDirDatabase = NULL;
	switch(type)
	{
		case GLDLib:
			return cvsDir;
		case GLDProtocols:
			if(!cvsDirProtocols)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/protocols";
				cvsDirProtocols = strdup(cv.c_str());
			}
			return cvsDirProtocols;
		case GLDTriggers:
			if(!cvsDirTriggers)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/triggers";
				cvsDirTriggers = strdup(cv.c_str());
			}
			return cvsDirTriggers;
		case GLDXdiff:
			if(!cvsDirXdiff)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/xdiff";
				cvsDirXdiff = strdup(cv.c_str());
			}
			return cvsDirXdiff;
		case GLDMdns:
			if(!cvsDirMdns)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/mdns";
				cvsDirMdns = strdup(cv.c_str());
			}
			return cvsDirMdns;
		case GLDDatabase:
			if(!cvsDirDatabase)
			{
				cvs::string cv;
				cv = cvsDir;
				cv += "/database";
				cvsDirDatabase = strdup(cv.c_str());
			}
			return cvsDirDatabase;
		default:
			return cvsDir;
	}
}

bool CGlobalSettings::SetConfigDirectory(const char *directory)
{
	CServerIo::trace(1,"Config directory changed to %s",directory?directory:_cvs_config_dir_default);
	if(cvs_config_dir && cvs_config_dir!=_cvs_config_dir_default)
    	free((void*)cvs_config_dir);
    cvs_config_dir=directory?strdup(directory):NULL;
    return true;
}

bool CGlobalSettings::SetLibraryDirectory(const char *directory)
{
	CServerIo::trace(1,"Library directory changed to %s",directory?directory:_cvs_library_dir_default);
	if(cvs_library_dir && cvs_library_dir!=_cvs_library_dir_default)
    	free((void*)cvs_library_dir);
	cvs_library_dir=directory?strdup(directory):NULL;
	return true;
}

const char *CGlobalSettings::GetCvsCommand()
{
	return cvs_program_name?cvs_program_name:_cvs_program_name_default;
}

bool CGlobalSettings::SetCvsCommand(const char *directory)
{
	CServerIo::trace(1,"CVS program name set to %s",directory?directory:_cvs_program_name_default);
	if(cvs_program_name && cvs_program_name!=_cvs_program_name_default)
    	free((void*)cvs_program_name);
	cvs_program_name=directory?strdup(directory):NULL;
	return true;
}


// vi:ts=4:sw=4
