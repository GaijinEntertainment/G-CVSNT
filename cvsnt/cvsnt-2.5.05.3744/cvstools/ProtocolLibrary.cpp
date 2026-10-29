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
#include <cvsapi.h>
#include "export.h"
#include "GlobalSettings.h"
#include "ProtocolLibrary.h"
#include "plugin_interface.h"
#include "protocol_interface.h"
#include "../cvsgui/cvsgui.h"
#include "../cvsgui/cvsgui_protocol.h"

#ifndef _WIN32
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#endif
#include <ctype.h>

CProtocolLibrary::loaded_protocols_t CProtocolLibrary::m_loaded_protocols;

namespace
{
	struct protocol_interface_data
	{
		void *library;
		int refcount;
	};

	int server_error(const server_interface *server, int fatal, const char *text)
	{
		CServerIo::log(fatal?CServerIo::logError:CServerIo::logNotice,"%s",text);
		CServerIo::error("%s",text);
		if(fatal)
			exit(-1);
		return 0;
	}

	struct server_interface cvs_interface =
	{
		NULL, /* Current root */
		NULL, /* Library directory */
		NULL, /* Config directory */
		NULL, /* cvs command */
		0,	  /* Input FD */
		1,	  /* Output FD */

		server_error,
		NULL, /* getpass */
		NULL, /* yesno */
		NULL, /* set_encrypted_channel */
		NULL  /* enumerate_protocols */
	};
};

bool CProtocolLibrary::SetupServerInterface(cvsroot *root, int io_socket /* = 0 */)
{
	cvs_interface.library_dir = CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDLib);
	cvs_interface.config_dir = CGlobalSettings::GetConfigDirectory();
	cvs_interface.cvs_command = CGlobalSettings::GetCvsCommand();
	cvs_interface.current_root = root;
	if(io_socket)
	{
		cvs_interface.in_fd = io_socket;
		cvs_interface.out_fd = io_socket;
	}
	return true;
}

server_interface *CProtocolLibrary::GetServerInterface()
{
	return &cvs_interface;
}

const protocol_interface *CProtocolLibrary::LoadProtocol(const char *protocol)
{
	cvs::string fn;
	CLibraryAccess lib;

	get_plugin_interface_t get_plugin_interface;
	protocol_interface *proto_interface;
	plugin_interface *plug_interface;
	protocol_interface_data *dat;

	/* Old plugins of other methods call removed callbacks */
	if(strcasecmp(protocol,"pserver"))
		return NULL;

	proto_interface = m_loaded_protocols[protocol];
	if(proto_interface)
	{
		dat = (protocol_interface_data*)proto_interface->plugin.__cvsnt_reserved;
		dat->refcount++;
		return proto_interface;
	}
		
#ifdef _WIN32
	char buf[128];
	cvs::sprintf(fn,128,"%s/protocol_map.ini",CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDProtocols));
	if(GetPrivateProfileStringA("cvsnt",protocol,NULL,buf,sizeof(buf),fn.c_str()))
		fn = buf;
	else
#endif
		cvs::sprintf(fn,128,"%s" SHARED_LIBRARY_EXTENSION,protocol);

	CServerIo::trace(3,"Loading protocol %s as %s",protocol,fn.c_str());
	if(!lib.Load(fn.c_str(),CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDProtocols)))
	{
		CServerIo::trace(3,"Error loading %s",fn.c_str());
		return NULL; // Couldn't find protocol - not supported, or supporting DLLs missing
	}

	get_plugin_interface = (get_plugin_interface_t)lib.GetProc("get_plugin_interface");
	if(!get_plugin_interface)
	{
		CServerIo::error("%s protocol library is missing entry point",protocol);
		return NULL; // Couldn't find protocol - bad DLL
	}

	plug_interface = get_plugin_interface();
	if(!plug_interface)
	{
		CServerIo::error("%s protocol library failed to initialise",protocol);
		return NULL; // Couldn't find protocol - bad DLL
	}

	if(plug_interface->interface_version!=PLUGIN_INTERFACE_VERSION)
	{
		CServerIo::trace(3,"Not loading %s - wrong version",protocol);
		lib.Unload();
		return NULL;
	}

	if(plug_interface->init)
	{
		if(plug_interface->init(plug_interface))
		{
			CServerIo::trace(3,"Not loading %s - initialisation failed",protocol);
			return NULL;
		}
	}

	if(!plug_interface->get_interface || (proto_interface = (protocol_interface*)plug_interface->get_interface(plug_interface,pitProtocol,&cvs_interface))==NULL)
	{
			CServerIo::trace(3,"Library does not support protocol interface.");
			return NULL;
	}

	dat = new protocol_interface_data;
	dat->library = lib.Detach();
	dat->refcount=1;
	plug_interface->__cvsnt_reserved=(void*)dat;
	proto_interface->name=strdup(protocol);
	m_loaded_protocols[protocol]=proto_interface;
	return proto_interface;
}

bool CProtocolLibrary::UnloadProtocol(const protocol_interface *protocol)
{
	if(protocol)
	{
		loaded_protocols_t::iterator i = m_loaded_protocols.find(protocol->name);
		if(i!=m_loaded_protocols.end())
		{
			protocol_interface *proto=i->second;
			protocol_interface_data *dat = (protocol_interface_data *)proto->plugin.__cvsnt_reserved;
			if(!--dat->refcount)
			{
				char protocolname[200];
				strcpy(protocolname,protocol->name);
				if(proto->plugin.destroy)
					proto->plugin.destroy(&proto->plugin);
				CServerIo::trace(3,"Eraseing %s",protocolname);
				m_loaded_protocols.erase(m_loaded_protocols.find(protocolname));
				CServerIo::trace(3,"Freeing %s",protocolname);
				free((void*)((protocol_interface *)protocol)->name);
				CServerIo::trace(3,"Freed %s",protocolname);
				CLibraryAccess lib(dat->library);
				CServerIo::trace(3,"Unloading %s",protocolname);
				lib.Unload();
				CServerIo::trace(3,"Delete %s",protocolname);
				delete dat;
				CServerIo::trace(3,"Deleted %s",protocolname);
			}
		}
	}
	return true;
}

bool CProtocolLibrary::IsEnabled(const protocol_interface *protocol)
{
	char value[64];

	if(!protocol->plugin.key || CGlobalSettings::GetGlobalValue("cvsnt","Plugins",protocol->plugin.key,value,sizeof(value)))
		return true;
	return atoi(value)!=0;
}

const protocol_interface *CProtocolLibrary::FindProtocol(const char *tagline, bool& badauth,int io_socket, const protocol_interface **temp_protocol)
{
	const protocol_interface *protocol;
	int res;

	badauth=false;
	CServerIo::trace(3,"FindPrototocol(%s)",tagline?tagline:"");

	protocol=LoadProtocol("pserver");
	if(!protocol)
		return NULL;
	if(!protocol->auth_protocol_connect || !IsEnabled(protocol))
	{
		CServerIo::trace(3,"pserver is disabled");
		UnloadProtocol(protocol);
		return NULL;
	}
	SetupServerInterface(NULL,io_socket);
	if(temp_protocol)
		*temp_protocol = protocol;
	res = protocol->auth_protocol_connect(protocol,tagline);
	if(res==CVSPROTO_SUCCESS)
		return protocol; /* Correctly authenticated */
	if(res == CVSPROTO_AUTHFAIL)
	{
		badauth=true;
		return protocol;
	}

	if(res !=CVSPROTO_NOTME && res!=CVSPROTO_NOTIMP)
		CServerIo::error("Authentication protocol rejected access\n");
	if(temp_protocol)
		*temp_protocol = NULL;
	UnloadProtocol(protocol);
	return NULL;
}

bool CProtocolLibrary::PromptForPassword(const char *prompt, char *buffer, int max_length)
{
	const char *pw = GetEnvironment("CVS_GETPASS");
	if(!pw)
		pw = __PromptForPassword(prompt);
	if(!pw)
		return false;
	strncpy(buffer,pw,max_length);
	return true;
}

char CProtocolLibrary::PromptForAnswer(const char *message, const char *title, bool withcancel)
{
	if(_cvsgui_readfd != 0)
	{
		const char *response;
		/* Send to cvsgui in a format that it'll understand */
		fflush (stderr);
		fflush (stdout);

		printf("Question: %s\n",title);
		printf("%s\n",message);
		printf("Enter: Yes/No%s\n",withcancel?"/Cancel":"");
		fflush(stdout);

		response = GetEnvironment("CVSLIB_YESNO");
		if(!response)
		{
			CServerIo::trace(3,"CVSGUI protocol error - null response\n");
			return 'c'; /* Something went wrong... assume Cancel */
		}

		switch(tolower(response[0]))
		{
			case 'y':
			case 'n':
				return tolower(response[0]);
			case 'c':
			case 'q':
				return 'c';
			default:
				CServerIo::trace(3,"CVSGUI protocol error - don't understand '%s\n",response);
				return 'c';
		}
	}
	else
	{
		char c;

		fflush (stderr);
		fflush (stdout);
		fflush (stdin);

		printf("%s",message);
		fflush(stdout);
		for(;;)
		{
			c=getchar();
			if(tolower(c)=='y' || c=='\n' || c=='\r')
			{
				fflush (stdin);
				return 'y';
			}
			if(withcancel && (c==27 || tolower(c)=='c'))
			{
				fflush (stdin);
				return 'c';
			}
			if(tolower(c)=='n' || (!withcancel && c==27))
			{
				fflush (stdin);
				return 'n';
			}
		}
	}
}

#ifndef _WIN32
static void getbuf(FILE *fi, char pbuf[], int max)
{ 
  int k = 0; 
  int ch = getc(fi);
  while(ch != '\n' && ch > 0)
  {
     if (k < max)
        pbuf[k++] = ch;
     ch = getc(fi);
   }
   pbuf[k++] = '\0';
}
#endif

const char *CProtocolLibrary::__PromptForPassword(const char *prompt)
{
#ifdef _WIN32
	// Andrew Innes, via google. Dec. 1997.

	static char input[256];
	HANDLE in;
	HANDLE err;
	DWORD  count;

	in = GetStdHandle (STD_INPUT_HANDLE);
	err = GetStdHandle (STD_ERROR_HANDLE);

	if (in == INVALID_HANDLE_VALUE || err == INVALID_HANDLE_VALUE)
		return NULL;

	if (WriteFile (err, prompt, (DWORD)strlen (prompt), &count, NULL))
	{
		bool istty = (GetFileType (in) == FILE_TYPE_CHAR);
		DWORD old_flags;
		int rc;

		if (istty)
		{
			if (GetConsoleMode (in, &old_flags))
				SetConsoleMode (in, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
			else
				istty = 0;
		}
		rc = ReadFile (in, input, sizeof (input), &count, NULL);
		if (count >= 2 && input[count - 2] == '\r')
			input[count - 2] = '\0';
		else
		{
			char buf[256];
			while (ReadFile (in, buf, sizeof (buf), &count, NULL) > 0)
			{
				if (count >= 2 && buf[count - 2] == '\r')
					break;
			}
		}
		WriteFile (err, "\r\n", 2, &count, NULL);
		if (istty)
			SetConsoleMode (in, old_flags);
		if (rc)
			return input;
	}

	return NULL; 
#else
    struct termios otmode, ntmode;
    static char pbuf[BUFSIZ+1];

    FILE *fi = fopen("/dev/tty", "r+");
    if (fi == NULL) return NULL;
    setbuf(fi, NULL);
    tcgetattr(fileno(fi), &otmode);		/* fetch current tty mode */
    ntmode = otmode;				/* copy the struct	  */
    ntmode.c_lflag &= ~ECHO;			/* disable echo		  */
    tcsetattr(fileno(fi), 0, &ntmode);		/* set new mode		  */

    fprintf(stderr, "%s", prompt); 
    fflush(stderr);

    getbuf(fi, pbuf, BUFSIZ);
    putc('\n', stderr);
    tcsetattr(fileno(fi), 0, &otmode);		/* restore previous mode  */
    fclose(fi);
    return pbuf;
#endif
}

const char *CProtocolLibrary::GetEnvironment(const char *env)
{
	char *e = cvsguiglue_getenv(env);
	if(!e)
		return getenv(env);
	return e;
}
