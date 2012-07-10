/* CVS sspi auth interface
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

#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#include <Schnlsp.h>
#include <lm.h>

#define MODULE sspi

#include "common.h"
#include "../version.h"

#include "sspi_resource.h"

static void sspi_destroy(const struct protocol_interface *protocol);
static int sspi_connect(const struct protocol_interface *protocol, int verify_only);
static int sspi_disconnect(const struct protocol_interface *protocol);
static int sspi_login(const struct protocol_interface *protocol, char *password);
static int sspi_logout(const struct protocol_interface *protocol);
static int sspi_read_data(const struct protocol_interface *protocol, void *data, int length);
static int sspi_write_data(const struct protocol_interface *protocol, const void *data, int length);
static int sspi_flush_data(const struct protocol_interface *protocol);
static int sspi_shutdown(const struct protocol_interface *protocol);
static int sspi_wrap(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize);
static int sspi_unwrap_buffer(const void *buffer, int size, void *output, int *newsize);
static int sspi_wrap_buffer(int encrypt, const void *buffer, int size, void *output, int *newsize);
static DWORD ServerAuthenticate(const char *proto);
static int ClientAuthenticate(const char *protocol, const char *name, const char *pwd, const char *domain, const char *hostname);
static int sspi_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len);
static int sspi_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password);
static int sspi_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string);
static int sspi_impersonate(const struct protocol_interface *protocol, const char *username, void *reserved);
static int sspi_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value);
static const char *sspi_get_keyword_help(const struct protocol_interface *protocol);
static int InitProtocol(const char *protocol);
static LPCSTR GetErrorString(DWORD dwErr);
int sspi_win32config(const struct plugin_interface *ui, void *wnd);

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

struct protocol_interface sspi_protocol_interface =
{
	{
		PLUGIN_INTERFACE_VERSION,
		":sspi: protocol",CVSNT_PRODUCTVERSION_STRING,"SspiProtocol",
		init,
		destroy,
		get_interface,
	#ifdef _WIN32
		sspi_win32config
	#else
		NULL
	#endif
	},
	"sspi",
	"sspi "CVSNT_PRODUCTVERSION_STRING,
	":sspi[;keyword=value...]:[username[:password]@]host[:port][:]/path",

	elemHostname, /* Required elements */
	elemUsername|elemPassword|elemHostname|elemPort|elemTunnel, /* Valid elements */

	NULL, /* validate */
	sspi_connect,
	sspi_disconnect,
	sspi_login,
	sspi_logout,
	sspi_wrap,
	sspi_auth_protocol_connect,
	sspi_read_data,
	sspi_write_data,
	sspi_flush_data,
	sspi_shutdown,
	sspi_impersonate,
	sspi_validate_keyword, 
	sspi_get_keyword_help, 
	NULL, /* server_read_data */
	NULL, /* server_write_data */
	NULL, /* server_flush_data */
	NULL, /* server_shutdown */
};

static CredHandle credHandle;
static CtxtHandle contextHandle;
static SecPkgInfo *secPackInfo = NULL;
static SecPkgContext_Sizes secSizes;
static bool bNtlm,bKerberos,bSchannel,bNegotiate;

static int init(const struct plugin_interface *plugin)
{
	OSVERSIONINFO osv = { sizeof(OSVERSIONINFO) };
	char value[32];
	
	bNtlm=bKerberos=bNegotiate=true;
	bSchannel=false;

	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiNtlm",value,sizeof(value)))
		bNtlm=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiKerberos",value,sizeof(value)))
		bKerberos=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiNegotiate",value,sizeof(value)))
		bNegotiate=atoi(value)?true:false;
	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiSchannel",value,sizeof(value)))
		bSchannel=atoi(value)?true:false;

	// Win95 or NT4 without Active Directory extensions
	if(bNegotiate && !InitProtocol("Negotiate"))
		bNegotiate=false;

	if(bKerberos && !InitProtocol("Kerberos"))
		bKerberos=false;

	if(bSchannel && !InitProtocol("Schannel"))
		bSchannel=false;

	// Should always exist
	if(bNtlm && !InitProtocol("NTLM"))
		bNtlm=false;

	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	protocol_interface *protocol = (protocol_interface*)plugin;

	free(protocol->auth_username);
	free(protocol->auth_repository);

	FreeContextBuffer( secPackInfo );
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitProtocol)
		return NULL;

	set_current_server((const struct server_interface*)param);
	return (void*)&sspi_protocol_interface;
}

plugin_interface *get_plugin_interface()
{
	return &sspi_protocol_interface.plugin;
}

int sspi_connect(const struct protocol_interface *protocol, int verify_only)
{
	char crypt_password[64];
	const char *password;
	char domain_buffer[128],*domain;
	char user_buffer[128];
	const char *user,*p;
	const char *begin_request = "BEGIN SSPI";
	char protocols[1024];
	const char *proto;
	cvs::string nego;
	CScramble scramble;

	if(!current_server()->current_root->hostname || !current_server()->current_root->directory)
		return CVSPROTO_BADPARMS;

	if(tcp_connect(current_server()->current_root))
		return CVSPROTO_FAIL;

	user = current_server()->current_root->username;
	password = current_server()->current_root->password;
	domain = NULL;

	if(user && !current_server()->current_root->password)
	{
		if(!sspi_get_user_password(user,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,crypt_password,sizeof(crypt_password)))
			password = scramble.Unscramble(crypt_password);
	}

	if(user && strchr(user,'\\'))
	{
		strncpy(domain_buffer,user,sizeof(domain_buffer));
		domain_buffer[sizeof(domain_buffer)-1]='\0';
		domain=strchr(domain_buffer,'\\');
		if(domain)
		{
			*domain = '\0';
			strncpy(user_buffer,domain+1,sizeof(user_buffer));
			domain = domain_buffer;
			user = user_buffer;
		}
	}

	if(current_server()->current_root->optional_1 && *current_server()->current_root->optional_1)
		nego = current_server()->current_root->optional_1;
	else
	{
		// We don't send 'Kerberos' as older servers always used that in preference.. and stuff broke...
		nego="";
		if(bNegotiate) nego+="Negotiate,";
		if(bSchannel) nego+="Schannel,";
		if(bNtlm) nego+="NTLM,";
		if(!nego.size())
		{
			server_error(1,"No protocols available for SSPI connection");
			return CVSPROTO_FAIL;
		}
		nego.resize(nego.size()-1);
	}

	if(tcp_printf("%s\n%s\n",begin_request,nego.c_str())<0)
		return CVSPROTO_FAIL;

	tcp_readline(protocols, sizeof(protocols));
	if((p=strstr(protocols,"[server aborted"))!=NULL)
	{
		server_error(1, p);
		return CVSPROTO_FAIL;
	}

	/* The server won't send us negotiate if we haven't requested it (above) so no need
	   to check here */
	strlwr(protocols);
	if(strstr(protocols,"negotiate"))
		proto="Negotiate";
	else if(strstr(protocols,"kerberos"))
		proto="Kerberos";
	else if(strstr(protocols,"schannel")) 
		proto="Schannel";
	else if(strstr(protocols,"ntlm"))
		proto="NTLM";
	else
	{
	    server_error(1, "Can't authenticate - server and client cannot agree on an authentication scheme (got '%s')",protocols);
		return CVSPROTO_FAIL;
	}

	if(!InitProtocol(proto))
	{
		server_error(1, "Couldn't initialise '%s' package - SSPI broke?",proto);
	}

	if(!ClientAuthenticate(proto,user,password,domain,current_server()->current_root->hostname))
		return CVSPROTO_AUTHFAIL;

	QueryContextAttributes(&contextHandle,SECPKG_ATTR_SIZES,&secSizes);
	
	if(tcp_printf("%s\n",current_server()->current_root->directory)<0)
		return CVSPROTO_AUTHFAIL;
	return CVSPROTO_SUCCESS;
}

int sspi_disconnect(const struct protocol_interface *protocol)
{
	if(tcp_disconnect())
		return CVSPROTO_FAIL;
	return CVSPROTO_SUCCESS;
}

int sspi_login(const struct protocol_interface *protocol, char *password)
{
	const char *user = get_username(current_server()->current_root);
	CScramble scramble;
	
	/* Store username & encrypted password in password store */
	if(sspi_set_user_password(user,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,scramble.Scramble(password)))
	{
		server_error(1,"Failed to store password");
	}

	return CVSPROTO_SUCCESS;
}

int sspi_logout(const struct protocol_interface *protocol)
{
	const char *user = get_username(current_server()->current_root);
	
	if(sspi_set_user_password(user,current_server()->current_root->hostname,current_server()->current_root->port,current_server()->current_root->directory,NULL))
	{
		server_error(1,"Failed to delete password");
	}
	return CVSPROTO_SUCCESS;
}

int sspi_auth_protocol_connect(const struct protocol_interface *protocol, const char *auth_string)
{
	char *protocols;
	const char *proto;
	SecPkgContext_Names secNames;
	DWORD rc;

    if (!strcmp (auth_string, "BEGIN SSPI"))
		sspi_protocol_interface.verify_only = 0;
	else 
		return CVSPROTO_NOTME;

	server_getline(protocol, &protocols, 1024);

	if(!protocols)
	{
		server_printf("Unsupported protocol\n");
		return CVSPROTO_FAIL;
	}

	if(!(bNtlm||bNegotiate||bKerberos||bSchannel))
	{
		server_printf("No SSPI protocols enabled!\n");
		return CVSPROTO_FAIL;
	}

	strlwr(protocols);
	if(bNegotiate && strstr(protocols,"negotiate"))
		proto="Negotiate";
	else if(bKerberos && (strstr(protocols,"kerberos") || strstr(protocols,"negotiate"))) // For compatibility new clients don't send kerberos
		proto="Kerberos";
	else if(bSchannel && strstr(protocols,"schannel")) 
		proto="Schannel";
	else if(bNtlm && strstr(protocols,"ntlm"))
		proto="NTLM";
	else
	{
		server_printf("Unsupported protocol\n");
		return CVSPROTO_FAIL;
	}
	free(protocols);

	server_printf("%s\n",proto); /* We have negotiated protocol */

	if(!InitProtocol(proto))
		return CVSPROTO_FAIL;

	rc = ServerAuthenticate(proto);
	if(rc==SEC_E_LOGON_DENIED)
		return CVSPROTO_AUTHFAIL;
	else if(rc!=SEC_E_OK)
		return CVSPROTO_FAIL;

	rc = QueryContextAttributes(&contextHandle,SECPKG_ATTR_SIZES,&secSizes);
	if(FAILED(rc))
		return CVSPROTO_FAIL;

	if(!strcmp(secPackInfo->Name,"Schannel"))
	{
		rc = ImpersonateSecurityContext(&contextHandle);
		if(FAILED(rc))
			return CVSPROTO_FAIL;
		TCHAR un[UNLEN];
		DWORD dw = sizeof(un);
		if(!GetUserName(un,&dw))
			return CVSPROTO_FAIL;
		sspi_protocol_interface.auth_username = strdup(un);
		RevertToSelf();
	}
	else
	{
		rc = QueryContextAttributes(&contextHandle, SECPKG_ATTR_NAMES,&secNames);
		if(FAILED(rc))
			return CVSPROTO_FAIL;

		sspi_protocol_interface.auth_username = strdup(secNames.sUserName);
		FreeContextBuffer(secNames.sUserName);
	}


	/* Get the repository details for checking */
    server_getline (protocol, &sspi_protocol_interface.auth_repository, 4096);

	return CVSPROTO_SUCCESS;
}

int sspi_read_data(const struct protocol_interface *protocol, void *data, int length)
{
	return tcp_read(data,length);
}

int sspi_write_data(const struct protocol_interface *protocol, const void *data, int length)
{
	return tcp_write(data,length);
}

int sspi_flush_data(const struct protocol_interface *protocol)
{
	return 0; // TCP/IP is always flushed
}

int sspi_shutdown(const struct protocol_interface *protocol)
{
	return tcp_shutdown();
}

int sspi_wrap(const struct protocol_interface *protocol, int unwrap, int encrypt, const void *input, int size, void *output, int *newsize)
{
	if(unwrap)
		return sspi_unwrap_buffer(input,size,output,newsize);
	else
		return sspi_wrap_buffer(encrypt,input,size,output,newsize);
	return 0;
}

static int sspi_unwrap_buffer(const void *buffer, int size, void *output, int *newsize)
{
	SecBuffer rgsb[] =
	{
        {size - secSizes.cbSecurityTrailer, SECBUFFER_DATA, output},
        {secSizes.cbSecurityTrailer, SECBUFFER_TOKEN,  ((char*)output)+size - secSizes.cbSecurityTrailer}
    };
    SecBufferDesc sbd = {SECBUFFER_VERSION, sizeof rgsb / sizeof *rgsb, rgsb};
	int rc;

	if(stricmp(secPackInfo->Name,"NTLM"))
	{
		/* None-NTLM needs the value of rgsb[1].cbBuffer passed through to the other end.
		   We don't use this in the NTLM case for compatibility with legacy servers */
		rgsb[1].cbBuffer=*(DWORD*)(((char*)buffer)+size-sizeof(DWORD));

		char *p = (char*)rgsb[1].pvBuffer;
		p-=sizeof(DWORD);
		rgsb[1].pvBuffer=p;
		rgsb[0].cbBuffer-=sizeof(DWORD);
		size-=sizeof(DWORD);
	}

	memcpy(output,buffer,size);
	rc = DecryptMessage(&contextHandle,&sbd,0,0);

	if(rc)
		return -1;

	*newsize = size - secSizes.cbSecurityTrailer;
    return 0;
}

static int sspi_wrap_buffer(int encrypt, const void *buffer, int size, void *output, int *newsize)
{
    SecBuffer rgsb[] =
	{
        {size, SECBUFFER_DATA, output},
        {secSizes.cbSecurityTrailer, SECBUFFER_TOKEN, ((char*)output)+size},
    };
    SecBufferDesc sbd = {SECBUFFER_VERSION, sizeof rgsb / sizeof *rgsb, rgsb};
	int rc;

	/* NTLM doesn't really support wrapping, so we always encrypt */
	/* In theory we could use SignMessage... for the future perhaps */

	memcpy(output, buffer, size);

    // describe our buffer for SSPI
    // encrypt in place

	rc = EncryptMessage(&contextHandle, 0, &sbd, 0);

	if(rc)
		return -1;

    *newsize = size + secSizes.cbSecurityTrailer;

	if(stricmp(secPackInfo->Name,"NTLM"))
	{
		/* None-NTLM needs the value of rgsb[1].cbBuffer passed through to the other end.
		   We don't send this in the NTLM case for compatibility with legacy servers */
		*(DWORD*)(((char*)output)+*newsize)=rgsb[1].cbBuffer;
		(*newsize)+=sizeof(DWORD);
	}

	return 0;
}

int InitProtocol(const char *protocol)
{
	if(QuerySecurityPackageInfo( (char*)protocol, &secPackInfo ) != SEC_E_OK)
		return 0;
	if(!secPackInfo)
		return 0;

	return 1;
}

DWORD ServerAuthenticate(const char *proto)
{
	int rc,rcl;
	BOOL haveToken;
	int bytesReceived = 0, bytesSent = 0;
	TimeStamp useBefore;
	// input and output buffers
	SecBufferDesc obd, ibd;
	SecBuffer ob, ib[2];
	BOOL haveContext = FALSE;
	DWORD ctxReq,ctxAttr;
	char *p;
	int n;
	short len;

	rc = AcquireCredentialsHandle( NULL, (char*)proto, SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &credHandle, &useBefore );
	if ( rc == SEC_E_OK )
		haveToken = TRUE;
	else
		haveToken = FALSE;

	while ( 1 )
	{
		// prepare to get the server's response
		ibd.ulVersion = SECBUFFER_VERSION;
		ibd.cBuffers = 2;
		ibd.pBuffers = ib; // just one buffer
		ib[0].BufferType = SECBUFFER_TOKEN; // preping a token here
		ib[1].cbBuffer = 0;
		ib[1].pvBuffer = NULL;
		ib[1].BufferType = SECBUFFER_EMPTY; // Spare stuff

		// receive the client's POD

		rcl = read( current_server()->in_fd, (char *) &len, sizeof(len) );
		if(rcl<=0 || !len)
		{
			rc = SEC_E_INTERNAL_ERROR;
			break;
		}
		ib[0].cbBuffer = ntohs(len);
		bytesReceived += sizeof ib[0].cbBuffer;
		ib[0].pvBuffer = malloc( ib[0].cbBuffer );

		p = (char *) ib[0].pvBuffer;
		n = ib[0].cbBuffer;
		while ( n )
		{
			rcl = read( current_server()->in_fd, (char *) p, n);
			if(rcl<=0 || !len)
			{
				rc = SEC_E_INTERNAL_ERROR;
				break;
			}
			bytesReceived += rcl;
			n -= rcl;
			p += rcl;
		}
		if(n)
			break;

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

	if(!strcmp(secPackInfo->Name,"Schannel"))
		ctxReq = /*ASC_REQ_ALLOCATE_MEMORY |*/ ASC_REQ_REPLAY_DETECT | ASC_REQ_SEQUENCE_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR | ASC_REQ_MUTUAL_AUTH | ASC_REQ_STREAM;
	else
		ctxReq = /*ASC_REQ_ALLOCATE_MEMORY |*/ ASC_REQ_REPLAY_DETECT | ASC_REQ_SEQUENCE_DETECT | ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR;

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
				len=htons((short)ob.cbBuffer);
				if((n=write(current_server()->out_fd,&len,sizeof(len)))<=0)
					break;
				bytesSent += n;
				if((n=write(current_server()->out_fd,ob.pvBuffer, ob.cbBuffer))<=0)
					break;
				bytesSent += n;
			}
			//FreeContextBuffer(ob.pvBuffer);
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
	{
		if(rc==SEC_E_LOGON_DENIED)
			haveToken = FALSE;
		else
			server_error(0,"[%08x] %s\n",rc, GetErrorString(rc));
	}

	return haveToken?SEC_E_OK:(DWORD)rc;
}

int ClientAuthenticate(const char *protocol, const char *name, const char *pwd, const char *domain, const char *hostname)
{
	int rc, rcISC;
	SEC_WINNT_AUTH_IDENTITY nameAndPwd = {0};
	int bytesReceived = 0, bytesSent = 0;
	char myTokenSource[DNLEN*4];
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
	char *p;
	int n;
	short len;

	if(strcmp(secPackInfo->Name,"Schannel"))
	{
		if ( name )
		{
			nameAndPwd.Domain = (BYTE*)domain;
			nameAndPwd.DomainLength = domain?strlen( domain ):0;
			nameAndPwd.User = (BYTE*)name;
			nameAndPwd.UserLength = name?strlen( name ):0;
			nameAndPwd.Password = (BYTE*)(pwd?pwd:"");
			nameAndPwd.PasswordLength = pwd? strlen(pwd):0;
			nameAndPwd.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
		}

		rc = AcquireCredentialsHandle( NULL, (char*)protocol, SECPKG_CRED_OUTBOUND, NULL, name?&nameAndPwd:NULL, NULL, NULL, &credHandle, &useBefore );

		ctxReq = /*ISC_REQ_ALLOCATE_MEMORY |*/ ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR;
	    sprintf (myTokenSource, "cvs/%s", hostname);
	}
	else
	{
		HANDLE hMy = CertOpenSystemStore(0,"MY");
		if(!hMy)
		{
			rcISC = SEC_E_NO_CREDENTIALS;
			server_error(1,"[%08x] %s\n",rcISC,GetErrorString(rcISC));
			return FALSE;
        }

		if(name)
		{
			cert = CertFindCertificateInStore(hMy, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_NAME, name,	NULL);
			if(!cert)
			{
				rcISC = SEC_E_NO_CREDENTIALS;
				server_error(1,"[%08x] %s\n",rcISC,GetErrorString(rcISC));
				return FALSE;
			}
		}

		cred.dwVersion = SCHANNEL_CRED_VERSION;
		cred.dwFlags = SCH_CRED_USE_DEFAULT_CREDS | SCH_CRED_AUTO_CRED_VALIDATION;

		if(cert)
		{
			cred.cCreds     = 1;
			cred.paCred     = &cert;
		}

		rc = AcquireCredentialsHandle( NULL, (char*)protocol, SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &credHandle, &useBefore );

		ctxReq = /*ISC_REQ_ALLOCATE_MEMORY |*/ ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR | ISC_REQ_MUTUAL_AUTH | ISC_REQ_STREAM;
		strncpy(myTokenSource,hostname,sizeof(myTokenSource));

		CertCloseStore(hMy,0);
	}

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
			if(rcISC==SEC_E_LOGON_DENIED)
				return FALSE;
			else
				server_error(1,"[%08x] %s\n",rcISC,GetErrorString(rcISC));
		}

		// send the output buffer off to the server
		if ( ob.cbBuffer != 0 )
		{
			len=htons((short)ob.cbBuffer);
			if((dwWritten=tcp_write( (const char *) &len, sizeof len))<=0)
				break;
			bytesSent += dwWritten;
			if((dwWritten=tcp_write( (const char *) ob.pvBuffer, ob.cbBuffer))<=0)
				break;
			bytesSent += dwWritten;
		}
		//FreeContextBuffer(ob.pvBuffer);
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
		ib[1].cbBuffer = 0;
		ib[1].pvBuffer = NULL;
		ib[1].BufferType = SECBUFFER_EMPTY; // Spare stuff

		// receive the server's response
		if((dwRead=tcp_read((char *) &len, sizeof len))<=0)
			break;
		ib[0].cbBuffer=ntohs(len);
		if(!len)
			break;
		bytesReceived += sizeof ib[0].cbBuffer;
		ib[0].pvBuffer = malloc( ib[0].cbBuffer );

		p = (char *) ib[0].pvBuffer;
		n = ib[0].cbBuffer;
		while ( n )
		{
			if((dwRead=tcp_read(p,n))<=0)
				break;
			bytesReceived += dwRead;
			n -= dwRead;;
			p += dwRead;
		}

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

int sspi_get_user_password(const char *username, const char *server, const char *port, const char *directory, char *password, int password_len)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::GetUserValue("cvsnt","cvspass",tmp,password,password_len))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int sspi_set_user_password(const char *username, const char *server, const char *port, const char *directory, const char *password)
{
	char tmp[1024];

	if(port)
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s:%s",username,server,port,directory);
	else
		snprintf(tmp,sizeof(tmp),":sspi:%s@%s:%s",username,server,directory);
	if(!CGlobalSettings::SetUserValue("cvsnt","cvspass",tmp,password))
		return CVSPROTO_SUCCESS;
	else
		return CVSPROTO_FAIL;
}

int sspi_impersonate(const struct protocol_interface *protocol, const char *username, void *reserved)
{
	if(ImpersonateSecurityContext(&contextHandle)==SEC_E_OK)
		return CVSPROTO_SUCCESS;
	return CVSPROTO_FAIL;
}

int sspi_validate_keyword(const struct protocol_interface *protocol, cvsroot *root, const char *keyword, const char *value)
{
   if(!strcasecmp(keyword,"force"))
   {
      root->optional_1 = strdup(value);
      return CVSPROTO_SUCCESS;
   }
   return CVSPROTO_FAIL;
}

const char *sspi_get_keyword_help(const struct protocol_interface *protocol)
{
	return "force\0Force authentication type (Default: Negotiate)\0";
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
	
static BOOL CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int nSel;
	char value[64];

	switch(uMsg)
	{
	case WM_INITDIALOG:
		nSel = 1;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","Plugins","SspiProtocol",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK1,BM_SETCHECK,nSel,0);
		if(!nSel)
		{
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),FALSE);
		}
		nSel = 1;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiNegotiate",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK2,BM_SETCHECK,nSel,0);
		nSel = 1;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiKerberos",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK3,BM_SETCHECK,nSel,0);
		nSel = 1;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiNtlm",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK4,BM_SETCHECK,nSel,0);
		nSel = 0;
		if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","SspiSchannel",value,sizeof(value)))
			nSel = atoi(value);
		SendDlgItemMessage(hWnd,IDC_CHECK5,BM_SETCHECK,nSel,0);

		if(!InitProtocol("Negotiate"))
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),FALSE);

		if(!InitProtocol("Kerberos"))
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),FALSE);

		if(!InitProtocol("Ntlm"))
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),FALSE);

		if(!InitProtocol("Schannel"))
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),FALSE);
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_CHECK1:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK2),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK3),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK4),nSel?TRUE:FALSE);
			EnableWindow(GetDlgItem(hWnd,IDC_CHECK5),nSel?TRUE:FALSE);
			return TRUE;
		case IDOK:
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","Plugins","SspiProtocol",value);
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK2,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","PServer","SspiNegotiate",value);
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK3,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","PServer","SspiKerberos",value);
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK4,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","PServer","SspiNtlm",value);
			nSel=SendDlgItemMessage(hWnd,IDC_CHECK5,BM_GETCHECK,0,0);
			snprintf(value,sizeof(value),"%d",nSel);
            CGlobalSettings::SetGlobalValue("cvsnt","PServer","SspiSchannel",value);
		case IDCANCEL:
			EndDialog(hWnd,LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

int sspi_win32config(const struct plugin_interface *ui, void *wnd)
{
	HWND hWnd = (HWND)wnd;
	int ret = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, ConfigDlgProc);
	return ret==IDOK?0:-1;
}
