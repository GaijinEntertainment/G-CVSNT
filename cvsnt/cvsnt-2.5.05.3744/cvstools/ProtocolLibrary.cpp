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
	CProtocolLibrary m_lib;

	int server_error(const server_interface *server, int fatal, const char *text)
	{
		CServerIo::log(fatal?CServerIo::logError:CServerIo::logNotice,"%s",text);
		CServerIo::error("%s",text);
		if(fatal)
			exit(-1);
		return 0;
	}
	int server_getpass(const server_interface *server, char *password, int max_length, const char *prompt) /* return 1 OK, 0 Cancel */
	{
		if(m_lib.PromptForPassword(prompt,password,max_length))
			return 1;
		return 0;
	}
	int server_yesno(const server_interface *server, const char *message, const char *title, int withcancel) /* Return -1 cancel, 0 No, 1 Yes */
	{
		switch(m_lib.PromptForAnswer(message,title,withcancel?true:false))
		{
			case 'y':
				return 1;
			case 'n':
				return 0;
			case 'c':
			default:
				return -1;
		}
	}
	int server_set_encrypted_channel(const server_interface *server, int encrypt)
	{
		return 0;
	}
	const char *server_enumerate_protocols(const server_interface *server, int *context, enum proto_type type)
	{
		CServerIo::trace(3,"server_enumerate_protocols(%d,%d)",context?*context:0,(int)type);
		do
		{
			const char *proto = m_lib.EnumerateProtocols(context);
			if(proto && type!=ptAny)
			{
				const protocol_interface *protocol=m_lib.LoadProtocol(proto);
				if(!protocol)
					continue;
				if(type == ptServer)
				{
					if((!protocol->auth_protocol_connect || !protocol->connect))
					{
						CServerIo::trace(3,"%s has no server component",proto);
						m_lib.UnloadProtocol(protocol);
						continue;
					}
					if(protocol->auth_protocol_connect && protocol->plugin.key)
					{
						char value[64];
						int val = 1;

						CServerIo::trace(3,"Checking key %s",protocol->plugin.key);

						if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins",protocol->plugin.key,value,sizeof(value)))
							val = atoi(value);
						if(!val)
						{
							CServerIo::trace(3,"%s is disabled",proto);
							m_lib.UnloadProtocol(protocol);
							continue;
						}
					}
				}
				if(type == ptClient && !protocol->connect)
				{
					CServerIo::trace(3,"$s has no client component",proto);
					m_lib.UnloadProtocol(protocol);
					continue;
				}
				m_lib.UnloadProtocol(protocol);
			}
			if(proto)
				CServerIo::trace(3,"Returning protocol :%s:",proto);
			else
				CServerIo::trace(3,"No more protocols");
			return proto;
		} while(1);
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
		server_getpass,
		server_yesno,
		server_set_encrypted_channel,
		server_enumerate_protocols
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
		cvs::sprintf(fn,128,"%s"SHARED_LIBRARY_EXTENSION,protocol);

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

const protocol_interface *CProtocolLibrary::FindProtocol(const char *tagline, bool& badauth,int io_socket, bool secure, const protocol_interface **temp_protocol)
{
	int context;
	const char *proto;
	const protocol_interface *protocol;
	int res;
	
	badauth=false;
	CServerIo::trace(3,"FindPrototocol(%s)",tagline?tagline:"");

	context=0;
	while((proto=EnumerateProtocols(&context))!=NULL)
	{
		protocol=LoadProtocol(proto);
		if(!protocol)
			continue;
		CServerIo::trace(3,"Checking protocol %s",proto);
		if(secure && !protocol->wrap && !(protocol->valid_elements&flagAlwaysEncrypted))
		{
			CServerIo::trace(3,"%s protocol disabled as it does not support encryption.",proto);
			UnloadProtocol(protocol);
			continue;
		}
		if(protocol->auth_protocol_connect)
		{
			if(protocol->plugin.key)
			{
				char value[64];
				int val = 1;

				CServerIo::trace(3,"Checking key %s",protocol->plugin.key);

				if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins",protocol->plugin.key,value,sizeof(value)))
					val = atoi(value);
				if(!val)
				{
					CServerIo::trace(3,"%s is disabled",proto);
					UnloadProtocol(protocol);
					continue;
				}
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
			{
				CServerIo::error("Authentication protocol rejected access\n");
				if(temp_protocol)
					*temp_protocol = NULL;
				UnloadProtocol(protocol);
				return NULL; /* Protocol was recognised, but failed */
			}
			if(temp_protocol)
				*temp_protocol = NULL;
			UnloadProtocol(protocol);
		}
		else
			UnloadProtocol(protocol);
	}
	return NULL;
}

const char *CProtocolLibrary::EnumerateProtocols(int *context)
{
	if(!*context)
	{
		CServerIo::trace(3,"EnumerateProtocols: %s",CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDProtocols));
		m_acc.close();
		if(!m_acc.open(CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDProtocols),"*"SHARED_LIBRARY_EXTENSION))
		{
			CServerIo::trace(3,"EnumeratePrototocols failed");
			return NULL;
		}
		*context=1;
	}

	if(!m_acc.next(m_inf))
	{
		*context=2;
		m_acc.close();
		return NULL;
	}

	m_inf.filename.resize(m_inf.filename.find_last_of('.'));
	return m_inf.filename.c_str();
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
