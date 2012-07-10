/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Win32 specific */
#include <config.h>
#include "../lib/api_system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#define SECURITY_WIN32
#include <security.h>

#include "../cvs_string.h"
#include "../HttpSocket.h"
#include "../SSPIHandler.h"
#include "../../version.h"

/*
** Translation Table as described in RFC1113
*/
static const unsigned char cb64[65]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#include <ctype.h>

// in autoproxy
bool GetProxyForHost(const char *url, const char *hostname, cvs::string& proxy, cvs::string& proxy_port);

CHttpSocket::CHttpSocket()
{
	m_bProxy = false;
	m_nAuthStage=0;
}

CHttpSocket::~CHttpSocket()
{
}

bool CHttpSocket::create(const char *urlHost, bool auto_proxy /*= true*/, const char *proxy_address /*= NULL*/, const char *proxy_port /*= NULL*/, const char *username /*= NULL*/, const char *password /*= NULL*/)
{
	if(!_setUrl(urlHost))
		return false;

	m_bAutoProxy = auto_proxy;
	m_proxyUser = username?username:"";
	m_proxyPassword = password?password:"";

	if(proxy_address && proxy_port)
	{
		m_proxyName = proxy_address;
		m_proxyPort = proxy_port;
		m_bProxy = true;
	}

	return _create();
}

bool CHttpSocket::_setUrl(const char *urlHost)
{
	if(!urlHost || strncmp(urlHost,"http://",7))
		return false;

	cvs::string url_string = urlHost;
	char *address,*port,*p;

	address=(char*)(url_string.data()+7);
	p=strpbrk(address,":/");
	if(p && *p==':')
	{
		*p='\0';
		port=p+1;
		p=strchr(port,'/');
	}
	else
		port="80";
	if(p)
		*p='\0';
	// We don't process the URL location here

	m_urlHost = urlHost;
	m_port = port;
	m_address = address;
	return true;
}

bool CHttpSocket::_create()
{
	m_nAuthStage=0;
	if(m_bAutoProxy)
	{
		m_bProxy = GetProxyForHost(m_urlHost.c_str(),m_address.c_str(),m_proxyName,m_proxyPort);
	}
	if(m_bProxy)
		return CSocketIO::create(m_proxyName.c_str(),m_proxyPort.c_str(),false);
	else
		return CSocketIO::create(m_address.c_str(),m_port.c_str(),false);
}

bool CHttpSocket::request(const char *command, const char *location, const char *content /* = NULL */, size_t content_length /* = 0 */)
{
	CSSPIHandler sspi;
	cvs::string loc;

	if(!command || !*command || !location || location[0]!='/')
		return false;

	if(!m_activeSocket && !CSocketIO::connect())
		return false;

	if(!content && content_length)
		content_length = 0;

	bool bAgain = false;

	m_requestHeaderList["Proxy-Connection"].clear();
	if(m_bProxy)
		m_requestHeaderList["Proxy-Connection"].push_back("Keep-Alive");

	do
	{
		bAgain = false;
		if(!_request(command,location,content,content_length))
			return false;
		switch(m_responseCode)
		{
		case 302: // Moved
//			_asm int 3
			loc = m_responseHeaderList["Location"][0].c_str();
			location = loc.c_str();
			if(!_setUrl(location))
				return false;
// Do proxies really change across redirects??  I suspect this is a rare case...
//			CSocketIO::close();
//			if(!_create();
//				return false;
//			if(!CSocketIO::connect())
//				return false;
			location=strchr(location+7,'/');
			if(!location) location="/";
			bAgain = true;
			break;
		case 407: // Proxy authentication required
			{
				char *proxy_auth,*p;
				char *auth_type=NULL,*realm;
				cvs::string negotiate,neg_enc;
				std::vector<cvs::string>& auth = m_responseHeaderList["Proxy-Authenticate"];

				if(m_nAuthStage==-1)
					break; // Failed to auth
				if(!m_nAuthStage)
				{
					// NTLM doesn't work - there's some magic voodoo you have to do to use it 
					// that isn't in any of the documentation, and it's impossible to find by
					// experimentation
					//
					// The code is all there and is correct according to the available documentation...
					//
					bool bHasNtlm = sspi.init("NTLM");

					for(size_t n=0; n<auth.size();n++)
					{
						proxy_auth = (char*)auth[n].c_str();
						p=strchr(proxy_auth,' ');
						if(p)
							*(p++)='\0';
						if((!auth_type && !strcmp(proxy_auth,"Basic")) || (bHasNtlm && !strcmp(proxy_auth,"NTLM")))
						{
							auth_type=proxy_auth;
							realm=p;
						}
					}
					if(!auth_type)
						break; // Auth not understood
					if(!strcmp(auth_type,"Basic"))
					{
						cvs::string auth_plain, auth_enc;
						cvs::sprintf(auth_plain,64,"%s:%s",m_proxyUser.c_str(),m_proxyPassword.c_str());
						base64Enc((const unsigned char *)auth_plain.c_str(),auth_plain.length(),auth_enc);
						cvs::sprintf(auth_plain,64,"Basic %s",auth_enc.c_str());
						m_requestHeaderList["Proxy-Authorization"].clear();
						m_requestHeaderList["Proxy-Authorization"].push_back(auth_plain);
						bAgain=true;
						break;
					}
					if(!strcmp(auth_type,"NTLM"))
					{
						CSocketIO::close();
						if(!_create())
							return false;
						if(!CSocketIO::connect())
							return false;
						m_nAuthStage = 1;

						// You must restart the connection to begin authentication... not sure why
						//
						// HTTP clients don't support security (confidentiality, sequence detect, etc. )
						// so we don't use it.
						if(!sspi.ClientStart(false,bAgain,m_proxyUser.c_str(),m_proxyPassword.c_str()))
							return false;

						size_t len;
						const unsigned char *ob = sspi.getOutputBuffer(len);
						base64Enc(ob,len,neg_enc);
						cvs::sprintf(negotiate,256,"NTLM %s",neg_enc.c_str());
						m_requestHeaderList["Proxy-Authorization"].clear();
						m_requestHeaderList["Proxy-Authorization"].push_back(negotiate);
						break;
					}
					else
					{
						// Unknown auth type
						break;
					}
				}
				else
				{
					// NTLM stage 1
					for(size_t n=0; n<auth.size();n++)
					{
						proxy_auth = (char*)auth[n].c_str();
						p=strchr(proxy_auth,' ');
						if(p)
							*(p++)='\0';
						if(!strcmp(proxy_auth,"NTLM"))
						{
							auth_type=proxy_auth;
							realm=p;
						}
					}
					if(!auth_type || !realm)
						return false;
					cvs::string resp;
					base64Dec((const unsigned char *)realm, strlen(realm), resp);
					if(!sspi.ClientStep(bAgain, resp.data(), resp.size()))
						return false;

					size_t len;
					const unsigned char *ob = sspi.getOutputBuffer(len);
					base64Enc(ob,len,neg_enc);
					cvs::sprintf(negotiate,256,"NTLM %s",neg_enc.c_str());
					m_requestHeaderList["Proxy-Authorization"].clear();
					m_requestHeaderList["Proxy-Authorization"].push_back(negotiate);
					if(!bAgain)
					{
						m_nAuthStage=-1;
						bAgain=true; // We must keep going, to send the final packet
					}
					break;
				}
				break;
			}
			break;
		default:
			break;
		}
	} while(bAgain);

	return true;
}

bool CHttpSocket::_request(const char *command, const char *location, const char *content, size_t content_length)
{
	cvs::string line;

	if(m_bProxy)
	{
		if(CSocketIO::printf("%s http://%s%s HTTP/1.1\r\nHost: %s\r\nContent-Length: %d\r\n",command,m_address.c_str(),location,m_address.c_str(),content_length)<0)
			return false;
	}
	else
	{
		if(CSocketIO::printf("%s %s HTTP/1.1\r\nHost: %s\r\nContent-Length: %d\r\n",command,location,m_address.c_str(),content_length)<0)
			return false;
	}
	if(m_requestHeaderList.find("User-Agent")==m_requestHeaderList.end())
		m_requestHeaderList["User-Agent"].push_back("Cvsapi "CVSNT_PRODUCTVERSION_SHORT" (Win32)");
	for(headerList_t::const_iterator i = m_requestHeaderList.begin(); i!=m_requestHeaderList.end(); i++)
	{
		if(!strcmp(i->first.c_str(),"Content-Length") || !strcmp(i->first.c_str(),"Host"))
			continue;
		for(size_t j=0; j<i->second.size(); j++)
		{
			if(CSocketIO::printf("%s: %s\r\n",i->first.c_str(),i->second[j].c_str())<0)
				return false;
		}
	}
	CSocketIO::printf("\r\n");
	if(content_length)
		if(CSocketIO::send(content,(int)content_length)<0)
			return false;
	CSocketIO::getline(line);
	char *p,*l=(char*)line.c_str();
	p=strchr(l,' ');
	if(p)
		*p='\0';
	m_responseProtocol = l;
	if(p)
	{
		l=++p;
		p=strchr(p,' ');
		if(p)
			*p='\0';
		m_responseCode = atoi(l);
	}
	if(p)
		m_responseString = p+1;
	m_responseHeaderList.clear();
	while(CSocketIO::getline(line))
	{
		if(!line.length())
			break;
		l=(char*)line.c_str();
		p=strchr(l,':');
		if(p)
		{
			*(p++)='\0';
			while(*p && isspace((unsigned char)*p))
				p++;
			m_responseHeaderList[l].push_back(p);
		}
		else
			m_responseHeaderList[l].push_back("");
	}
	if(m_responseHeaderList.find("Content-Length")!=m_responseHeaderList.end())
	{
		size_t len = atoi(m_responseHeaderList["Content-Length"][0].c_str());
		m_content.resize(len);
		if(len)
		{
			if(CSocketIO::recv((char*)m_content.data(),(int)len)<0)
				return false;
		}
	}
	else if(m_responseHeaderList.find("Transfer-Encoding")!=m_responseHeaderList.end() && 
		m_responseHeaderList["Transfer-Encoding"][0]=="chunked")
	{
		m_content.clear();
		while(CSocketIO::getline(line))
		{
			if(!line.length())
				continue;

			size_t len;
			sscanf(line.c_str(),"%x",&len);

			if(!len)
				break;
			size_t pos = m_content.size();
			m_content.resize(pos+len);
			CSocketIO::recv((char*)m_content.data()+pos,(int)len);
			CSocketIO::getline(line); // CRLF
		}
	}
	else
		m_content="";
	return true;
}

/************************************************************
 *    uuencode/decode functions
 ************************************************************/

//
//  Taken from NCSA HTTP and wwwlib.
//
//  NOTE: These conform to RFC1113, which is slightly different then the Unix
//        uuencode and uudecode!
//

static const int pr2six[256]={
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};

static const char six2pr[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};

bool CHttpSocket::base64Enc(const unsigned char *from, size_t len, cvs::string& to)
{
   unsigned char *outptr;
   unsigned int i;

   //
   //  Resize the buffer to 133% of the incoming data
   //

   to.resize(len + ((len + 3) / 3) + 4);

   outptr = (unsigned char *)to.data();

   for (i=0; i<len; i += 3) {
      *(outptr++) = six2pr[*from >> 2];            /* c1 */
      *(outptr++) = six2pr[((*from << 4) & 060) | ((from[1] >> 4) & 017)]; /*c2*/
      *(outptr++) = six2pr[((from[1] << 2) & 074) | ((from[2] >> 6) & 03)];/*c3*/
      *(outptr++) = six2pr[from[2] & 077];         /* c4 */

      from += 3;
   }

   /* If len was not a multiple of 3, then we have encoded too
    * many characters.  Adjust appropriately.
    */
   if(i == len+1) {
      /* There were only 2 bytes in that last group */
      outptr[-1] = '=';
   } else if(i == len+2) {
      /* There was only 1 byte in that last group */
      outptr[-1] = '=';
      outptr[-2] = '=';
   }

   *outptr = '\0';

   return true;
}

bool CHttpSocket::base64Dec(const unsigned char *from, size_t len, cvs::string& to)
{
    int nbytesdecoded;
    const unsigned char *bufin = from;
	unsigned char *bufout;
    int nprbytes;

    /* Figure out how many characters are in the input buffer.
     * If this would decode into more bytes than would fit into
     * the output buffer, adjust the number of input bytes downwards.
     */
    while(pr2six[*(bufin++)] <= 63)
		;
    nprbytes = (int)(bufin - from - 1);
    nbytesdecoded = ((nprbytes+3)/4) * 3;

	to.resize(nbytesdecoded+4);

    bufout = (unsigned char *)to.data();

    bufin = from;

    while (nprbytes > 0) {
        *(bufout++) =
            (unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        *(bufout++) =
            (unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    if(nprbytes & 03) {
        if(pr2six[bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }

	to.resize(nbytesdecoded);

    return true;
}


