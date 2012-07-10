/* CVS sserver auth interface
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

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <io.h>
#else
#include <netdb.h>
#include <pwd.h>
#include <unistd.h>
#endif

/* Requires openssl installed */
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MODULE sserver

#include "common.h"
#include "../version.h"

#ifdef _WIN32
#include "resource.h"
#endif

static int sserver_connect(const struct protocol_interface *protocol, int verify_only);
static int sserver_disconnect(const struct protocol_interface *protocol);
static int sserver_login(const struct protocol_interface *protocol, char *password);
static int sserver_logout(const struct protocol_interface *protocol);
static int sserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string);
static int sserver_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len);
static int sserver_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password);
static int sserver_read_data(const struct protocol_interface *protocol, void *data, int length);
static int sserver_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int sserver_flush_data(const struct protocol_interface *protocol);
static int sserver_shutdown(const struct protocol_interface *protocol);
static int sserver_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value);
static const char *sserver_get_keyword_help(const struct protocol_interface *protocol);

static int sserver_printf(char *fmt, ...);
static void sserver_error(const char *txt, int err);

static SSL *ssl;
static SSL_CTX *ctx;
static int error_state;
static bool inauth;

#define SSERVER_VERSION "1.1"
#define SSERVER_INIT_STRING "SSERVER "SSERVER_VERSION" READY\n"
#define SSERVER_CLIENT_VERSION_STRING "SSERVER-CLIENT "SSERVER_VERSION" READY\n"

int sserver_win32config(const struct plugin_interface *ui, void *wnd);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface sserver_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":sserver: protocol",CVSNT_PRODUCTVERSION_STRING,"SserverProtocol",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		sserver_win32config
	#else
		NULL
	#endif
	},
	"sserver",
	"sserver "CVSNT_PRODUCTVERSION_STRING,
	":sserver[;keyword=value...]:[username[:password]@]host[:port][:]/path",

	elemHostname, /* Required elements */
	elemUsername|elemPassword|elemHostname|elemPort|elemTunnel|flagAlwaysEncrypted, /* Valid elements */

	NULL, /* validate */
	sserver_connect,
	sserver_disconnect,
	sserver_login,
	sserver_logout,
	NULL, /* wrap */
	sserver_auth_protocol_connect,
	sserver_read_data,
	sserver_write_data,
	sserver_flush_data,
	sserver_shutdown,
	NULL, /* impersonate */
	sserver_validate_keyword,
	sserver_get_keyword_help,
	sserver_read_data,
	sserver_write_data,
	sserver_flush_data,
	sserver_shutdown,
};

static int init(const struct plugin_interface *plugin)
{
	error_state = 0;
	inauth = false;
	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	protocol_interface *protocol = (protocol_interface*)plugin;
	free(protocol->auth_username);
	free(protocol->auth_password);
	free(protocol->auth_repository);
	if(ssl)
	{
	  SSL_free (ssl);
	  ssl=NULL;
	}
	if(ctx)
	{
      SSL_CTX_free (ctx);
	  ctx=NULL;
	}
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitProtocol)
		return NULL;

	set_current_server((const struct server_interface*)param);
	return (void*)&sserver_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &sserver_protocol_interface.plugin;
}

int sserver_connect(const struct protocol_interface *protocol, int verify_only)
{
	char crypt_password[64];
	char server_version[128];
	char tmp_keyname[256];
	const char *begin_request = "BEGIN SSL AUTH REQUEST";
	const char *end_request = "END SSL AUTH REQUEST";
	const char *username = NULL;
	const char *key = current_server()->current_root->optional_3;
	int err,l;
	int sserver_version = 0;
	char certs[4096];
	int strict = 0;
	CScramble scramble;
	bool send_client_version = false;

	snprintf(certs,sizeof(certs),"%s/ca.pem",PATCH_NULL(current_server()->config_dir));

	if(current_server()->current_root->optional_1)
	{
	  sserver_version = atoi(current_server()->current_root->optional_1);
	  if(sserver_version != 0 && sserver_version != 1)
	  {
	    server_error(0,"version must be one of:");
	    server_error(0,"0 - All CVSNT-type servers");
	    server_error(0,"1 - Unix server using Corey Minards' sserver patches");
	    server_error(1,"Please specify a valid value");
	  }
	}

	if(!CGlobalSettings::GetUserValue("cvsnt","sserver","StrictChecking",server_version,sizeof(server_version)))
	{
	  strict = atoi(server_version);
	}

	if(!key && !CGlobalSettings::GetUserValue("cvsnt","sserver","ClientKey",tmp_keyname,sizeof(tmp_keyname)))
	{
	  key = tmp_keyname;
	}

	if(current_server()->current_root->optional_2)
		strict = atoi(current_server()->current_root->optional_2);

	if(sserver_version == 1) /* Old sserver */
	{
	  begin_request = verify_only?"BEGIN SSL VERIFICATION REQUEST":"BEGIN SSL REQUEST";
	  end_request = verify_only?"END SSL VERIFICATION REQUEST":"END SSL REQUEST";
	}
	else if(verify_only)
	{
		begin_request = "BEGIN SSL VERIFICATION REQUEST";
		end_request = "END SSL VERIFICATION REQUEST";
	}
	username = get_username(current_server()->current_root);

	if(!username || !current_server()->current_root->hostname || !current_server()->current_root->directory)
		return CVSPROTO_BADPARMS;

	if(tcp_connect(current_server()->current_root))
		return CVSPROTO_FAIL;
	if(current_server()->current_root->password)
		strncpy(crypt_password,scramble.Scramble(current_server()->current_root->password),sizeof(crypt_password));
	else
	{
		if(sserver_get_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,crypt_password,sizeof(crypt_password)))
		{
			/* Using null password - trace something out here */
			server_error(0,"Using an empty password; you may need to do 'cvs login' with a real password\n");
			strncpy(crypt_password,scramble.Scramble(""),sizeof(crypt_password));
		}
	}


	if(sserver_version == 0) /* Pre-CVSNT had no version check */
	{
	  if(tcp_printf("%s\n",begin_request)<0)
		return CVSPROTO_FAIL;
	  for(;;)
	  {
		*server_version='\0';
		if((l=tcp_readline(server_version,sizeof(server_version))<0))
			return CVSPROTO_FAIL;
		if(*server_version)
			break;
#ifdef _WIN32
		Sleep(10);
#else
		usleep(10);
#endif
	  }
	  if(strncmp(server_version,"SSERVER ",8))
	  {
	  	  server_error(0,"%s\n",server_version);
		  return CVSPROTO_FAIL;
	  }
	  if(strncmp(server_version+8,"1.0 ",4))
		  send_client_version = true;
	}

	SSL_library_init();
	SSL_load_error_strings ();
	ctx = SSL_CTX_new (SSLv3_client_method ());
	SSL_CTX_set_options(ctx,SSL_OP_ALL|SSL_OP_NO_SSLv2);
	SSL_CTX_load_verify_locations(ctx,certs,NULL);
	if(key)
	{
		if((err=SSL_CTX_use_certificate_file(ctx,key,SSL_FILETYPE_PEM))<1)
		{
			sserver_error("Unable to read/load the client certificate", err);
			return CVSPROTO_FAIL;
		}
		if((err=SSL_CTX_use_PrivateKey_file(ctx,key,SSL_FILETYPE_PEM))<1)
		{
			sserver_error("Unable to read/load the client private key", err);
			return CVSPROTO_FAIL;
		}
		if(!SSL_CTX_check_private_key(ctx))
		{
			sserver_error("Client certificate failed verification", err);
			return CVSPROTO_FAIL;
		}
	}
	if(strict)
		SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT,NULL); /* Check verify result below */
	else
		SSL_CTX_set_verify(ctx,SSL_VERIFY_NONE,NULL); /* Check verify result below */

    ssl = SSL_new (ctx);
	SSL_set_fd (ssl, get_tcp_fd());
    if((err=SSL_connect (ssl))<1)
	{
		/* if the SSL connection failed, it is probably because the server hasn't got a valid cert/key so is unable to setup a SSL session */
		sserver_error("SSL connection failed", err);
		return CVSPROTO_FAIL;
	}

	switch(err=SSL_get_verify_result(ssl))
	{
		case X509_V_OK:
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		/* Valid certificate */
		break;
		default:
			server_error(1,"Server certificate verification failed: %s\n",X509_verify_cert_error_string(err));
	}

	{
		X509 *cert;
		char buf[1024];

		if(!(cert=SSL_get_peer_certificate(ssl)))
			server_error(1,"Server did not present a valid certificate\n");

		buf[0]='\0';
		if(strict)
		{
		  X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, buf, sizeof(buf));
		  if(strcasecmp(buf,current_server()->current_root->hostname))
			server_error(1, "Certificate CommonName '%s' does not match server name '%s'\n",buf,current_server()->current_root->hostname);
		}
	}

	if(sserver_version == 1)
	{
	  if(sserver_printf("%s\n",begin_request)<0)
		return CVSPROTO_FAIL;
	}

    // For server versions 1.1+ send CLIENT_VERSION_STRING
	if(send_client_version && sserver_printf(SSERVER_CLIENT_VERSION_STRING)<0)
		return CVSPROTO_FAIL;
	if(sserver_printf("%s\n%s\n",current_server()->current_root->directory,username)<0)
		return CVSPROTO_FAIL;
	if(sserver_printf("%s\n",crypt_password)<0)
		return CVSPROTO_FAIL;
	if(sserver_printf("%s\n",end_request)<0)
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int sserver_disconnect(const struct protocol_interface *protocol)
{
	if(tcp_disconnect())
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int sserver_login(const struct protocol_interface *protocol, char *password)
{
	const char *username = NULL;
	CScramble scramble;

	username = get_username(current_server()->current_root);

	/* Store username & encrypted password in password store */
	if(sserver_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,scramble.Scramble(password)))
	{
		server_error(1,"Failed to store password\n");
	}

	return CVSPROTO_SUCCESS;
}

int sserver_logout(const struct protocol_interface *protocol)
{
	const char *username = NULL;

	username = get_username(current_server()->current_root);
	if(sserver_set_user_password(username,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,NULL))
	{
		server_error(1,"Failed to delete password\n");
	}
	return CVSPROTO_SUCCESS;
}

int sserver_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
	CScramble scramble;
	char *tmp;
	int err;
	char certfile[1024];
	char keyfile[1024];
	char certs[4096];
	int certonly = 0;
	char *client_version = NULL;

	inauth = true;
	snprintf(certs,sizeof(certs),"%s/ca.pem",PATCH_NULL(current_server()->config_dir));

    if (!strcmp (auth_string, "BEGIN SSL VERIFICATION REQUEST"))
		sserver_protocol_interface.verify_only = 1;
    else if (!strcmp (auth_string, "BEGIN SSL AUTH REQUEST"))
		sserver_protocol_interface.verify_only = 0;
	else
		return CVSPROTO_NOTME;

	write(current_server()->out_fd,SSERVER_INIT_STRING,sizeof(SSERVER_INIT_STRING)-1);

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificatesOnly",keyfile,sizeof(keyfile)))
		certonly = atoi(keyfile);
	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificateFile",certfile,sizeof(certfile)))
	{
		server_error(0,"E Configuration Error - CertificateFile not specified\n");
		return CVSPROTO_FAIL;
	}
	if(CGlobalSettings::GetGlobalValue("cvsnt","PServer","PrivateKeyFile",keyfile,sizeof(keyfile)))
		strncpy(keyfile,certfile,sizeof(keyfile));

	SSL_library_init();
	SSL_load_error_strings ();
	ctx = SSL_CTX_new (SSLv23_server_method ());
	SSL_CTX_set_options(ctx,SSL_OP_ALL|SSL_OP_NO_SSLv2);

	SSL_CTX_load_verify_locations(ctx,certs,NULL);

	ERR_get_error(); // Clear error stack
	if((err=SSL_CTX_use_certificate_file(ctx, certfile ,SSL_FILETYPE_PEM))<1)
	{
		sserver_error("Unable to read/load the server certificate", err);
		return CVSPROTO_FAIL;
	}
	if((err=SSL_CTX_use_PrivateKey_file(ctx, keyfile ,SSL_FILETYPE_PEM))<1)
	{
		sserver_error("Unable to read/load the server private key", err);
		return CVSPROTO_FAIL;
	}
	if(!SSL_CTX_check_private_key(ctx))
	{
		sserver_error("Server certificate failed verification", err);
		return CVSPROTO_FAIL;
	}
	SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL); /* Check verify result below */

    ssl = SSL_new (ctx);
#ifdef _WIN32 /* Win32 is stupid... */
    SSL_set_rfd (ssl, _get_osfhandle(current_server()->in_fd));
    SSL_set_wfd (ssl, _get_osfhandle(current_server()->out_fd));
#else
    SSL_set_rfd (ssl, current_server()->in_fd);
    SSL_set_wfd (ssl, current_server()->out_fd);
#endif
    set_encrypted_channel(1); /* Error must go through us now */

	if((err=SSL_accept(ssl))<1)
	{
		sserver_error("SSL connection failed", err);
		return CVSPROTO_FAIL;
	}

	switch(err=SSL_get_verify_result(ssl))
	{
		case X509_V_OK:
		/* Valid (or no) certificate */
		break;
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		/* Self signed certificate */
		server_error(0,"E Client sent self-signed certificate.\n");
		return CVSPROTO_FAIL;
		default:
			server_error(0,"E Server certificate verification failed: %s\n",X509_verify_cert_error_string(err));
			return CVSPROTO_FAIL;
	}

	X509 *cert = SSL_get_peer_certificate(ssl);

    /* Get the three important pieces of information in order. */
    /* See above comment about error handling.  */

	/* get version, if sent.  1.0 clients didn't have this handshake so we have to handle that. */
	server_getline (protocol, &client_version, PATH_MAX);
	if(strncmp(client_version,"SSERVER-CLIENT ",15))
	{
		sserver_protocol_interface.auth_repository = client_version;
		client_version = NULL;
	}
	else
    	server_getline (protocol, &sserver_protocol_interface.auth_repository, PATH_MAX);
    server_getline (protocol, &sserver_protocol_interface.auth_username, PATH_MAX);
	server_getline (protocol, &sserver_protocol_interface.auth_password, PATH_MAX);

	if(client_version) free(client_version);
	client_version = NULL;

    /* ... and make sure the protocol ends on the right foot. */
    /* See above comment about error handling.  */
    server_getline(protocol, &tmp, PATH_MAX);
    if (strcmp (tmp,
		sserver_protocol_interface.verify_only ?
		"END SSL VERIFICATION REQUEST" : "END SSL AUTH REQUEST")
	!= 0)
    {
		server_printf ("bad auth protocol end: %s\n", tmp);
		free(tmp);
		return CVSPROTO_FAIL;
    }

	strcpy(sserver_protocol_interface.auth_password, scramble.Unscramble(sserver_protocol_interface.auth_password));

	free(tmp);

	switch(certonly)
	{
	case 0:
		break;
	case 1:
		if(!cert)
		{
			server_error(0,"E Login requires a valid client certificate.\n");
			return CVSPROTO_AUTHFAIL;
		}
		free(sserver_protocol_interface.auth_password);
		sserver_protocol_interface.auth_password = NULL;
		break;
	case 2:
		if(!cert)
		{
			server_error(0,"E Login requires a valid client certificate.\n");
			return CVSPROTO_AUTHFAIL;
		}
		break;
	};

	inauth = false;

	return CVSPROTO_SUCCESS;
}

int sserver_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":sserver:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":sserver:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::GetUserValue("cvsnt","cvspass",tmp,password,password_len))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int sserver_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":sserver:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":sserver:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::SetUserValue("cvsnt","cvspass",tmp,password))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int sserver_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	int r,e;
	//if(error_state)
	//	return -1;

	r=SSL_read(ssl,data,length);
	switch(e=SSL_get_error(ssl,r))
	{
		case SSL_ERROR_NONE:
			return r;
		case SSL_ERROR_ZERO_RETURN:
			return 0;
		case SSL_ERROR_SYSCALL:
		default:
			error_state = 1;
			sserver_error("Read data failed", e);
			return -1;
	}
}

int sserver_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	int offset=0,r,e;

	if(!ssl)
		return length;  // Error handling - stop recursion

	//if(error_state)
	//	return -1;
	while(length)
	{
		r=SSL_write(ssl,((const char *)data)+offset,length);
		switch(e=SSL_get_error(ssl,r))
		{
		case SSL_ERROR_NONE:
			length -= r;
			offset += r;
			break;
		case SSL_ERROR_WANT_WRITE:
			break;
		default:
			error_state = 1;
			sserver_error("Write data failed", e);
			return -1;
		}
	}
	return offset;
}

int sserver_flush_data(const struct protocol_interface *protocol)
{
	return 0; // TCP/IP is always flushed
}

int sserver_shutdown(const struct protocol_interface *protocol)
{
	SSL_shutdown(ssl);
	return 0;
}

int sserver_printf(char *fmt, ...)
{
	char temp[1024];
	va_list va;

	va_start(va,fmt);

	vsnprintf(temp,sizeof(temp),fmt,va);

	va_end(va);

	return sserver_write_data(NULL,temp,strlen(temp));
}

int sserver_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value)
{
   if(!strcasecmp(keyword,"version") || !strcasecmp(keyword,"ver"))
   {
      root->optional_1 = strdup(value);
      return CVSPROTO_SUCCESS;
   }
   if(!strcasecmp(keyword,"strict"))
   {
      root->optional_2 = strdup(value);
      return CVSPROTO_SUCCESS;
   }
	if(!strcasecmp(keyword,"privatekey") || !strcasecmp(keyword,"key") || !strcasecmp(keyword,"rsakey"))
	{
		root->optional_3 = strdup(value);
		return CVSPROTO_SUCCESS;
	}
   return CVSPROTO_FAIL;
}

const char *sserver_get_keyword_help(const struct protocol_interface *protocol)
{
	return "privatekey\0Use file as private key (aliases: key, rsakey)\0version\0Server implementation (default: 0) (alias: ver)\0strict\0Strict certificate checks (default: 0)\0";
}

void sserver_error(const char *txt, int err)
{
	char errbuf[1024];
	int e = ERR_get_error();
	if(e)
		ERR_error_string_n(e, errbuf, sizeof(errbuf));
	else
		strcpy(errbuf,"Server dropped connection.");
	if(inauth)
		server_error(0, "E %s (%d): %s\n",txt,err,errbuf);
	else
		server_error(0, "%s (%d): %s\n",txt,err,errbuf);
}

#ifdef _WIN32
static BOOL CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int nSel;
	char value[64];
	char certfile[PATH_MAX];

	switch(uMsg)
	{
	case WM_INITDIALOG:
		nSel = 1;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","SserverProtocol",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,nSel,0);
		if(!nSel)
		{
			EnableWindow(GetDlgItem(hWnd,IDC_EDIT2),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_EDIT3),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SSLCERT),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_PRIVATEKEY),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_COMBO1),FALSE);
		}
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificateFile",certfile,sizeof(certfile)))
			SetDlgItemText(hWnd,IDC_EDIT2,certfile);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","PrivateKeyFile",certfile,sizeof(certfile)))
			SetDlgItemText(hWnd,IDC_EDIT3,certfile);

		SendDlgItemMessage(hWnd,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)"Passwords Only");
		SendDlgItemMessage(hWnd,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)"Certificates Only");
		SendDlgItemMessage(hWnd,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)"Certificates And Passwords");
		nSel = 0;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificatesOnly",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_COMBO1,CB_SETCURSEL,nSel,0);
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_CHECK1:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0);
			EnableWindow(GetDlgItem(hWnd,IDC_EDIT2),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_EDIT3),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_SSLCERT),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_PRIVATEKEY),nSel?TRUE:FALSE);
			return TRUE;
		case IDC_SSLCERT:
			{
				OPENFILENAME ofn = { sizeof(OPENFILENAME), hWnd, NULL, "Certificate files\0*.pem\0", NULL, 0, 0, certfile, sizeof(certfile), NULL, 0, NULL, NULL, OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY, 0, 0, "pem" };

				GetDlgItemText(hWnd,IDC_EDIT2,certfile,sizeof(certfile));
				if(GetOpenFileName(&ofn))
					SetDlgItemText(hWnd,IDC_EDIT2,certfile);
			}
			return TRUE;
		case IDC_PRIVATEKEY:
			{
				OPENFILENAME ofn = { sizeof(OPENFILENAME), hWnd, NULL, "Private key files\0*.pem\0", NULL, 0, 0, certfile, sizeof(certfile), NULL, 0, NULL, NULL, OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY, 0, 0, "pem" };

				GetDlgItemText(hWnd,IDC_EDIT3,certfile,sizeof(certfile));
				if(GetOpenFileName(&ofn))
					SetDlgItemText(hWnd,IDC_EDIT3,certfile);
			}
			return TRUE;
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","Plugins","SserverProtocol",value);
			GetDlgItemText(hWnd,IDC_EDIT2,certfile,sizeof(certfile));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","CertificateFile",certfile);
			GetDlgItemText(hWnd,IDC_EDIT3,certfile,sizeof(certfile));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","PrivateKeyFile",certfile);
			nSel=SendDlgItemMessage(hWnd,IDC_COMBO1,CB_GETCURSEL,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","CertificatesOnly",value);
		case IDCANCEL:
			EndDialog(hWnd,LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

int sserver_win32config(const struct plugin_interface *ui, void *wnd)
{
	HWND hWnd = (HWND)wnd;
	int ret = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, ConfigDlgProc);
	return ret==IDOK?0:-1;
}
#endif

