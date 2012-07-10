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
// Win32 specific

// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <io.h>
#include <Schnlsp.h>

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
static BOOL ClientAuthenticate(const char *name, const char *hostname);
static BOOL ServerAuthenticate(const char *hostname);
static LPCSTR GetErrorString(DWORD dwErr);

static int sserver_printf(char *fmt, ...);

static CredHandle credHandle;
static CtxtHandle contextHandle;
static SecPkgInfo *secPackInfo = NULL;
static SecPkgContext_StreamSizes secSizes;

#define SSL_BUFFER_SIZE 32768
static BYTE *g_sslBufferIn;
static BYTE *g_sslBufferOut;
static int g_sslBufferInPos;
static int g_sslBufferOutPos;
static int g_sslBufferInLen;
static int g_sslBufferOutLen;

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
	if(QuerySecurityPackageInfo( "SChannel", &secPackInfo ) != SEC_E_OK)
		return -1;
	if(!secPackInfo)
		return -1;

	g_sslBufferIn = (BYTE*)malloc(SSL_BUFFER_SIZE);
	g_sslBufferOut = (BYTE*)malloc(SSL_BUFFER_SIZE);

	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	protocol_interface *protocol = (protocol_interface*)plugin;
	free(protocol->auth_username);
	free(protocol->auth_password);
	free(protocol->auth_repository);

	free(g_sslBufferIn);
	free(g_sslBufferOut);

	FreeContextBuffer( secPackInfo );
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
	const char *cert = current_server()->current_root->optional_3;
	int l;
	int sserver_version = 0;
	int strict = 0;
	CScramble scramble;
	bool send_client_version = false;

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

	if(!cert && !CGlobalSettings::GetUserValue("cvsnt","sserver","ClientCert",tmp_keyname,sizeof(tmp_keyname)))
	{
	  cert = tmp_keyname;
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

	if(!ClientAuthenticate(cert,current_server()->current_root->hostname))
		return CVSPROTO_AUTHFAIL;

	QueryContextAttributes(&contextHandle,SECPKG_ATTR_STREAM_SIZES,&secSizes);

	PCERT_CONTEXT sc;
	PCCERT_CHAIN_CONTEXT pcc;
	CERT_SIMPLE_CHAIN  *psc;
	CERT_CHAIN_PARA para = { sizeof(CERT_CHAIN_PARA) };
	DWORD trust,rc;
	
	rc = QueryContextAttributes(&contextHandle,SECPKG_ATTR_REMOTE_CERT_CONTEXT,&sc);
	if(rc)
		server_error(1,"Couldn't get server certificate");

	if(!CertGetCertificateChain(NULL, sc, NULL, NULL, &para, 0, NULL, &pcc))
		server_error(1,"Couldn't get server certificate chain");

    psc = pcc->rgpChain[0];
	trust = psc->TrustStatus.dwErrorStatus;
 
    if (trust)
    {
        if (trust & (CERT_TRUST_IS_PARTIAL_CHAIN | CERT_TRUST_IS_UNTRUSTED_ROOT))
            ; // Seld signed
        else if (trust & (CERT_TRUST_IS_NOT_TIME_VALID))
			server_error(1,"Server certificate expired");
        else
			server_error(1,"Server certificate verification failed - %08x",trust);
    }

	if(strict)
	{
		char certname[256];

		CertGetNameString(sc, CERT_NAME_DNS_TYPE, 0, NULL, certname, sizeof(certname));
		  if(strcasecmp(certname,current_server()->current_root->hostname))
			server_error(1, "Certificate CommonName '%s' does not match server name '%s'\n",certname,current_server()->current_root->hostname);
	}

	CertFreeCertificateChain(pcc);
	FreeContextBuffer(sc);
 
	g_sslBufferInPos=g_sslBufferOutPos=0;
	g_sslBufferInLen=g_sslBufferOutLen=0;

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
	int certonly;
	char *client_version = NULL;
	char keyfile[256];
	const char *hostname = NULL;

    if (!strcmp (auth_string, "BEGIN SSL VERIFICATION REQUEST"))
		sserver_protocol_interface.verify_only = 1;
    else if (!strcmp (auth_string, "BEGIN SSL AUTH REQUEST"))
		sserver_protocol_interface.verify_only = 0;
	else
		return CVSPROTO_NOTME;

	write(current_server()->out_fd,SSERVER_INIT_STRING,sizeof(SSERVER_INIT_STRING)-1);

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificatesOnly",keyfile,sizeof(keyfile)))
		certonly = atoi(keyfile);
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ServerDnsName",keyfile,sizeof(keyfile)))
		hostname = keyfile;

	if(!ServerAuthenticate(hostname))
		return CVSPROTO_AUTHFAIL;

	QueryContextAttributes(&contextHandle,SECPKG_ATTR_STREAM_SIZES,&secSizes);

	g_sslBufferInPos=g_sslBufferOutPos=0;
	g_sslBufferInLen=g_sslBufferOutLen=0;

    set_encrypted_channel(1); /* Error must go through us now */

	PCERT_CONTEXT sc;
	PCCERT_CHAIN_CONTEXT pcc;
	CERT_SIMPLE_CHAIN  *psc;
	CERT_CHAIN_PARA para = { sizeof(CERT_CHAIN_PARA) };
	DWORD trust,rc;
	BOOL cert = FALSE;
	
	rc = QueryContextAttributes(&contextHandle,SECPKG_ATTR_REMOTE_CERT_CONTEXT,&sc);
	if(rc && rc!=SEC_E_NO_CREDENTIALS)
		server_error(1,"Couldn't get client certificate");

	if(rc!=SEC_E_NO_CREDENTIALS) /* The client doesn't have to send us a cert. as cvs uses passwords normally */
	{
		if(!CertGetCertificateChain(NULL, sc, NULL, NULL, &para, 0, NULL, &pcc))
			server_error(1,"Couldn't get client certificate chain");

		psc = pcc->rgpChain[0];
		trust = psc->TrustStatus.dwErrorStatus;
	 
		if (trust)
		{
			if (trust & (CERT_TRUST_IS_PARTIAL_CHAIN | CERT_TRUST_IS_UNTRUSTED_ROOT))
				server_error(1,"Client sent self signed certificate");
			else if (trust & (CERT_TRUST_IS_NOT_TIME_VALID))
				server_error(1,"Client certificate expired");
			else
				server_error(1,"Client certificate verification failed - %08x",trust);
		}

		CertFreeCertificateChain(pcc);
		FreeContextBuffer(sc);

		cert = TRUE;
	}
 
    /* Get the three important pieces of information in order. */
    /* See above comment about error handling.  */

	/* get version, if sent.  1.0 clients didn't have this handshake so we have to handle that. */
	server_getline (protocol, &client_version, MAX_PATH);
	if(strncmp(client_version,"SSERVER-CLIENT ",15))
	{
		sserver_protocol_interface.auth_repository = client_version;
		client_version = NULL;
	}
	else
    	server_getline (protocol, &sserver_protocol_interface.auth_repository, MAX_PATH);
    server_getline (protocol, &sserver_protocol_interface.auth_username, MAX_PATH);
	server_getline (protocol, &sserver_protocol_interface.auth_password, MAX_PATH);

	if(client_version) free(client_version);
	client_version = NULL;

    /* ... and make sure the protocol ends on the right foot. */
    /* See above comment about error handling.  */
    server_getline(protocol, &tmp, MAX_PATH);
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
	int rc = SEC_E_INCOMPLETE_MESSAGE;
	int toRead = SSL_BUFFER_SIZE - g_sslBufferInLen;

	while(rc == SEC_E_INCOMPLETE_MESSAGE && (g_sslBufferOutLen - g_sslBufferOutPos) < length)
	{
		assert((g_sslBufferInLen + toRead) <= SSL_BUFFER_SIZE);

		int len = tcp_read(g_sslBufferIn + g_sslBufferInLen, toRead);
		if(len<0)
			return len;

		BYTE *buf = g_sslBufferIn;

		len += g_sslBufferInLen;
		g_sslBufferInLen = 0;

		while(len)
		{
			SecBuffer rgsb[] =
			{
				{len, SECBUFFER_DATA, buf},
				{0,	SECBUFFER_EMPTY, NULL},
				{0,	SECBUFFER_EMPTY, NULL},
				{0,	SECBUFFER_EMPTY, NULL},
			};
			SecBufferDesc sbd = {SECBUFFER_VERSION, sizeof rgsb / sizeof *rgsb, rgsb};

			rc = DecryptMessage(&contextHandle,&sbd,0,0);

			if(rc == SEC_E_INCOMPLETE_MESSAGE)
			{
				if(g_sslBufferIn!=buf)
					memmove(g_sslBufferIn, buf, len);
				g_sslBufferInLen = len;
				toRead = rgsb[0].cbBuffer; // This isn't documented, but seems to be consistent.  Over-reading is a bad idea especially on a slow network.
				break;
			}

			if(rc)
				return -1;

			if(sbd.pBuffers[1].cbBuffer)
			{
				assert(sbd.pBuffers[1].BufferType == SECBUFFER_DATA); // Should always be true
				if(g_sslBufferOutPos + sbd.pBuffers[1].cbBuffer > SSL_BUFFER_SIZE)
				{
					memmove(g_sslBufferOut, g_sslBufferOut+g_sslBufferOutPos, g_sslBufferOutLen-g_sslBufferOutPos);
					g_sslBufferOutLen-=g_sslBufferOutPos;
					g_sslBufferOutPos=0;
				}
				assert(g_sslBufferOutPos + sbd.pBuffers[1].cbBuffer <= SSL_BUFFER_SIZE); // Taken care of above
				memcpy(g_sslBufferOut + g_sslBufferOutLen, sbd.pBuffers[1].pvBuffer, sbd.pBuffers[1].cbBuffer);
				g_sslBufferOutLen+=sbd.pBuffers[1].cbBuffer;
			}

			// Unprocessed data..
			len = sbd.pBuffers[3].cbBuffer;
			buf = (BYTE*)sbd.pBuffers[3].pvBuffer;
			if(sbd.pBuffers[3].cbBuffer) 
			{
				assert(sbd.pBuffers[3].BufferType == SECBUFFER_EXTRA);
			}
		}
	}

	if(g_sslBufferOutLen - g_sslBufferOutPos <= length)
	{
		if(g_sslBufferOutLen)
			memcpy(data, g_sslBufferOut + g_sslBufferOutPos, g_sslBufferOutLen - g_sslBufferOutPos);
		length = g_sslBufferOutLen - g_sslBufferOutPos;
		g_sslBufferOutLen = g_sslBufferOutPos = 0;
	}
	else
	{
		memcpy(data, g_sslBufferOut+g_sslBufferOutPos, length);
		g_sslBufferOutPos+=length;
	}

	return length;
}

int sserver_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	if(!length)
		return 0;

	BYTE *buffer = (BYTE*)malloc(length+secSizes.cbHeader+secSizes.cbTrailer);
	memcpy(buffer+secSizes.cbHeader,data,length);

	SecBuffer rgsb[] =
	{
		{ secSizes.cbHeader, SECBUFFER_STREAM_HEADER, buffer },
        {length, SECBUFFER_DATA, buffer+secSizes.cbHeader},
		{ secSizes.cbTrailer, SECBUFFER_STREAM_TRAILER, buffer+secSizes.cbHeader+length },
		{ 0, SECBUFFER_EMPTY, NULL }
    };
    SecBufferDesc sbd = {SECBUFFER_VERSION, sizeof rgsb / sizeof *rgsb, rgsb};
	int rc;

	rc = EncryptMessage(&contextHandle,0,&sbd,0);

	if(rc)
	{
		free(buffer);
		return -1;
	}

	// In practice only the trailer length changes, otherwise this wouldn't work..
	rc = tcp_write(buffer,rgsb[0].cbBuffer+rgsb[1].cbBuffer+rgsb[2].cbBuffer);
	
	free(buffer);
	return length;
}

int sserver_flush_data(const struct protocol_interface *protocol)
{
	return 0; // TCP/IP is always flushed
}

int sserver_shutdown(const struct protocol_interface *protocol)
{
	return tcp_shutdown();
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
	if(!strcasecmp(keyword,"cert"))
	{
		root->optional_3 = strdup(value);
		return CVSPROTO_SUCCESS;
	}
   return CVSPROTO_FAIL;
}

const char *sserver_get_keyword_help(const struct protocol_interface *protocol)
{
	return "cert\0Name of certificate to use\0version\0Server implementation (default: 0) (alias: ver)\0strict\0Strict certificate checks (default: 0)\0";
}

BOOL ClientAuthenticate(const char *name, const char *hostname)
{
	int rc, rcISC;
	SEC_WINNT_AUTH_IDENTITY nameAndPwd = {0};
	int bytesReceived = 0, bytesSent = 0;
	char myTokenSource[256];
	TimeStamp useBefore;
	DWORD ctxReq, ctxAttr;
	int dwRead,dwWritten;
	// input and output buffers
	SecBufferDesc obd, ibd;
	SecBuffer ob, ib[2];
	BOOL haveInbuffer = FALSE;
	BOOL haveContext = FALSE;
	SCHANNEL_CRED cred = {0};
	PCCERT_CONTEXT cert = NULL;

	HANDLE hMy = CertOpenSystemStore(0,"MY");
	if(!hMy)
	{
		rcISC = SEC_E_NO_CREDENTIALS;
		server_error(1,"[%08x] %s\n",rcISC,GetErrorString(rcISC));
		return FALSE;
    }

	if(name)
	{
		cert = CertFindCertificateInStore(hMy, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, (const wchar_t *)cvs::wide(name),	NULL);
		if(!cert)
		{
			rcISC = SEC_E_NO_CREDENTIALS;
			server_error(1,"No certificate for '%s': %s\n",name,GetErrorString(rcISC));
			return FALSE;
		}
	}

	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.dwFlags = SCH_CRED_USE_DEFAULT_CREDS;

	if(cert)
	{
		cred.cCreds     = 1;
		cred.paCred     = &cert;
	}

	rc = AcquireCredentialsHandle( NULL, "SChannel", SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &credHandle, &useBefore );

	ctxReq = ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_INTEGRITY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS;
	strncpy(myTokenSource,hostname,sizeof(myTokenSource));

	CertCloseStore(hMy,0);

	ib[0].pvBuffer = NULL;

	while ( 1 )
	{
		obd.ulVersion = SECBUFFER_VERSION;
		obd.cBuffers = 1;
		obd.pBuffers = &ob; // just one buffer
		ob.BufferType = SECBUFFER_TOKEN; // preping a token here
		ob.cbBuffer = secPackInfo->cbMaxToken;
		ob.pvBuffer = malloc(secPackInfo->cbMaxToken);

		rcISC = InitializeSecurityContext( &credHandle, haveContext? &contextHandle: NULL,
			myTokenSource, ctxReq, 0, SECURITY_NATIVE_DREP, haveInbuffer? &ibd: NULL,
			0, &contextHandle, &obd, &ctxAttr, &useBefore );

		if ( ib[0].pvBuffer != NULL )
		{
			free(ib[0].pvBuffer);
			ib[0].pvBuffer = NULL;
		}

		if ( rcISC == SEC_I_COMPLETE_AND_CONTINUE || rcISC == SEC_I_COMPLETE_NEEDED )
		{
			CompleteAuthToken( &contextHandle, &obd );
			if ( rcISC == SEC_I_COMPLETE_NEEDED )
				rcISC = SEC_E_OK;
			else if ( rcISC == SEC_I_COMPLETE_AND_CONTINUE )
				rcISC = SEC_I_CONTINUE_NEEDED;
		}

		if(rcISC<0)
		{
			server_error(1,"[%08x] %s\n",rcISC,GetErrorString(rcISC));
		}

		// send the output buffer off to the server
		if ( ob.cbBuffer != 0 )
		{
			if((dwWritten=tcp_write( (const char *) ob.pvBuffer, ob.cbBuffer))<=0)
				break;
			bytesSent += dwWritten;
		}
		free(ob.pvBuffer);
		ob.pvBuffer = NULL;
		ob.cbBuffer = 0;

		if ( rcISC != SEC_I_CONTINUE_NEEDED )
			break;

		// prepare to get the server's response
		ibd.ulVersion = SECBUFFER_VERSION;
		ibd.cBuffers = 2;
		ibd.pBuffers = ib; // just one buffer
		ib[0].BufferType = SECBUFFER_TOKEN; // preping a token here
		ib[0].cbBuffer = secPackInfo->cbMaxToken;
		ib[0].pvBuffer = malloc(secPackInfo->cbMaxToken);
		ib[1].cbBuffer = 0;
		ib[1].pvBuffer = NULL;
		ib[1].BufferType = SECBUFFER_EMPTY; // Spare stuff

		// receive the server's response
		if((dwRead=tcp_read(ib[0].pvBuffer,ib[0].cbBuffer))<=0)
			break;
		bytesReceived += dwRead;

		// by now we have an input buffer and a client context

		haveInbuffer = TRUE;
		haveContext = TRUE;
	}

	// we arrive here as soon as InitializeSecurityContext()
	// returns != SEC_I_CONTINUE_NEEDED.

	if ( rcISC != SEC_E_OK )
		haveContext = FALSE;
	else
		haveContext = TRUE; /* Looopback kerberos needs this */

	return haveContext;
}

BOOL ServerAuthenticate(const char *hostname)
{
	int rc, rcISC, rcl;
	BOOL haveToken;
	int bytesReceived = 0, bytesSent = 0;
	TimeStamp useBefore;
	// input and output buffers
	SecBufferDesc obd, ibd;
	SecBuffer ob, ib[2];
	BOOL haveContext = FALSE;
	DWORD ctxReq,ctxAttr;
	int n;
	short len;
	SCHANNEL_CRED cred = {0};
	char host[256];
	struct addrinfo *ai=NULL, hints = {0};
	PCCERT_CONTEXT cert;

	HANDLE hMy = CertOpenSystemStore(0,"MY");
	if(!hMy)
	{
		rcISC = SEC_E_NO_CREDENTIALS;
		server_error(1,"[%08x] %s\n",rcISC,GetErrorString(rcISC));
		return FALSE;
	}

	if(!hostname)
	{
		gethostname (host, sizeof host);
		hints.ai_flags=AI_CANONNAME;
		if(getaddrinfo(cvs::idn(host),NULL,&hints,&ai))
			server_error (1, "can't get canonical hostname");
		hostname = ai->ai_canonname;
		cert = CertFindCertificateInStore(hMy, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, (const wchar_t*)cvs::wide(cvs::decode_idn(hostname)),	NULL);
	}
	else
		cert = CertFindCertificateInStore(hMy, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, (const wchar_t*)cvs::wide(hostname),	NULL);

	if(!cert)
	{
		rcISC = SEC_E_NO_CREDENTIALS;
		server_error(1,"No certificate for '%s': %s\n",hostname,GetErrorString(rcISC));
		return FALSE;
	}

	cred.cCreds     = 1;
	cred.paCred     = &cert;

	if(ai)
		freeaddrinfo(ai);

	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.dwFlags = SCH_CRED_USE_DEFAULT_CREDS;

	rc = AcquireCredentialsHandle( NULL, "SChannel", SECPKG_CRED_INBOUND, NULL, &cred, NULL, NULL, &credHandle, &useBefore );
	if ( rc == SEC_E_OK )
		haveToken = TRUE;
	else
		haveToken = FALSE;

	CertCloseStore(hMy,0);

	while ( 1 )
	{
		// prepare to get the server's response
		ibd.ulVersion = SECBUFFER_VERSION;
		ibd.cBuffers = 2;
		ibd.pBuffers = ib; // just one buffer
		ib[0].BufferType = SECBUFFER_TOKEN; // preping a token here
		ib[0].cbBuffer = secPackInfo->cbMaxToken;
		ib[0].pvBuffer = malloc(ib[0].cbBuffer);
		ib[1].cbBuffer = 0;
		ib[1].pvBuffer = NULL;
		ib[1].BufferType = SECBUFFER_EMPTY; // Spare stuff

		// receive the client's POD

		rcl = read( current_server()->in_fd, ib[0].pvBuffer, ib[0].cbBuffer);
		if(rcl<=0)
		{
			rc = SEC_E_INTERNAL_ERROR;
			break;
		}

		// by now we have an input buffer

		obd.ulVersion = SECBUFFER_VERSION;
		obd.cBuffers = 1;
		obd.pBuffers = &ob; // just one buffer
		ob.BufferType = SECBUFFER_TOKEN; // preping a token here
		ob.cbBuffer = secPackInfo->cbMaxToken;
		ob.pvBuffer = malloc(secPackInfo->cbMaxToken);

		if(rc<0)
		{
			len=0;
			if((n=write(current_server()->out_fd,&len,sizeof(len)))<=0)
				break;
			break;
		}

		ctxReq = ASC_REQ_INTEGRITY | ASC_REQ_CONFIDENTIALITY | ASC_REQ_REPLAY_DETECT | ASC_REQ_SEQUENCE_DETECT | ASC_REQ_STREAM;

		rc = AcceptSecurityContext( &credHandle, haveContext? &contextHandle: NULL,
				&ibd, ctxReq, SECURITY_NATIVE_DREP, &contextHandle, &obd, &ctxAttr,
				&useBefore );

		if ( ib[0].pvBuffer != NULL )
		{
			free( ib[0].pvBuffer );
			ib[0].pvBuffer = NULL;
		}

		if ( rc == SEC_I_COMPLETE_AND_CONTINUE || rc == SEC_I_COMPLETE_NEEDED )
		{
			CompleteAuthToken( &contextHandle, &obd );
			if ( rc == SEC_I_COMPLETE_NEEDED )
				rc = SEC_E_OK;
			else if ( rc == SEC_I_COMPLETE_AND_CONTINUE )
				rc = SEC_I_CONTINUE_NEEDED;
		}

		// send the output buffer off to the server
		// warning -- this is machine-dependent! FIX IT!
		if ( rc == SEC_E_OK || rc == SEC_I_CONTINUE_NEEDED )
		{
			if ( ob.cbBuffer != 0 )
			{
				if((n=write(current_server()->out_fd,ob.pvBuffer, ob.cbBuffer))<=0)
					break;
				bytesSent += n;
			}
			free(ob.pvBuffer);
			ob.pvBuffer = NULL;
			ob.cbBuffer = 0;
		}
		else
		{
			break;
		}

		if ( rc != SEC_I_CONTINUE_NEEDED )
			break;

		haveContext = TRUE;
	}

	// we arrive here as soon as InitializeSecurityContext()
	// returns != SEC_I_CONTINUE_NEEDED.

	if ( rc != SEC_E_OK )
	{
		haveToken = FALSE;
	}

	if(rc<0)
		server_error(0,"[%08x] %s\n",rc, GetErrorString(rc));

	return haveToken?TRUE:FALSE;
}

LPCSTR GetErrorString(DWORD dwErr)
{
	static char ErrBuf[1024];

	FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM |
	FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    dwErr,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPSTR) ErrBuf,
    sizeof(ErrBuf),
    NULL );
	return ErrBuf;
};

#ifdef _WIN32
static BOOL CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int nSel;
	char value[64];
/*	char certfile[MAX_PATH]; */

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
/*		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","CertificateFile",certfile,sizeof(certfile)))
			SetDlgItemText(hWnd,IDC_EDIT2,certfile);
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","PrivateKeyFile",certfile,sizeof(certfile)))
			SetDlgItemText(hWnd,IDC_EDIT3,certfile); */

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
/*			{
				OPENFILENAME ofn = { sizeof(OPENFILENAME), hWnd, NULL, "Certificate files\0*.pem\0", NULL, 0, 0, certfile, sizeof(certfile), NULL, 0, NULL, NULL, OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY, 0, 0, "pem" };

				GetDlgItemText(hWnd,IDC_EDIT2,certfile,sizeof(certfile));
				if(GetOpenFileName(&ofn))
					SetDlgItemText(hWnd,IDC_EDIT2,certfile);
			} */
			return TRUE;
		case IDC_PRIVATEKEY:
/*			{
				OPENFILENAME ofn = { sizeof(OPENFILENAME), hWnd, NULL, "Private key files\0*.pem\0", NULL, 0, 0, certfile, sizeof(certfile), NULL, 0, NULL, NULL, OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY, 0, 0, "pem" };

				GetDlgItemText(hWnd,IDC_EDIT3,certfile,sizeof(certfile));
				if(GetOpenFileName(&ofn))
					SetDlgItemText(hWnd,IDC_EDIT3,certfile);
			} */
			return TRUE;
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","Plugins","SserverProtocol",value);
/*			GetDlgItemText(hWnd,IDC_EDIT2,certfile,sizeof(certfile));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","CertificateFile",certfile);
			GetDlgItemText(hWnd,IDC_EDIT3,certfile,sizeof(certfile));
			CGlobalSettings::SetGlobalValue("cvsnt","PServer","PrivateKeyFile",certfile);
			nSel=SendDlgItemMessage(hWnd,IDC_COMBO1,CB_GETCURSEL,0,0); */
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

