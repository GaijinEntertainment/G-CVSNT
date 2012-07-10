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
#ifndef HTTPSOCKET__H
#define HTTPSOCKET__H

#include "SocketIO.h"
#include <map>
#include <vector>

class CHttpSocket : public CSocketIO
{
public:
	CVSAPI_EXPORT CHttpSocket();
	CVSAPI_EXPORT virtual ~CHttpSocket();

	CVSAPI_EXPORT bool create(const char *urlHost, bool auto_proxy = true, const char *proxy_address = NULL, const char *proxy_port = NULL, const char *username = NULL, const char *password = NULL);
	CVSAPI_EXPORT bool addRequestHeader(const char *header, const char *value) { m_requestHeaderList[header].push_back(value); return true; }
	CVSAPI_EXPORT bool request(const char *command, const char *location, const char *content = NULL, size_t content_length = 0);
	CVSAPI_EXPORT int responseCode() { return m_responseCode; }
	CVSAPI_EXPORT const char *responseProtocol() { return m_responseProtocol.c_str(); }
	CVSAPI_EXPORT const char *responseString() { return m_responseString.c_str(); }
	CVSAPI_EXPORT const char *responseHeader(const char *header) { if(m_responseHeaderList.find(header)!=m_responseHeaderList.end()) return m_responseHeaderList[header][0].c_str(); else return NULL; }
	CVSAPI_EXPORT const std::vector<cvs::string>* responseHeaders(const char *header) { if(m_responseHeaderList.find(header)!=m_responseHeaderList.end()) return &m_responseHeaderList[header]; else return NULL; }
	CVSAPI_EXPORT const char *responseData(size_t& responseLength) { responseLength = m_content.size(); return m_content.data(); }

protected:
	int m_nAuthStage;
	cvs::string m_port, m_address, m_urlHost;
	cvs::string m_proxyName, m_proxyPort, m_proxyUser, m_proxyPassword;
	cvs::string m_responseProtocol,m_responseString;
	int m_responseCode;
	cvs::string m_content;
	bool m_bProxy,m_bAutoProxy;
	typedef	std::map<cvs::string, std::vector<cvs::string> > headerList_t;
	headerList_t m_requestHeaderList;
	headerList_t m_responseHeaderList;

	bool _create();
	bool _setUrl(const char *urlHost);
	bool _request(const char *command, const char *location, const char *content, size_t content_length);
	bool base64Enc(const unsigned char *from, size_t len, cvs::string& to);
	bool base64Dec(const unsigned char *from, size_t len, cvs::string& to);
};

#endif

