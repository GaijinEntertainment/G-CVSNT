/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation;
    version 2.1 of the License.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <config.h>
#include "lib/api_system.h"
#include <assert.h>

#include "cvs_string.h"
#include "ServerIO.h"
#include "FileAccess.h"
#include <libxml/parser.h>
#include <libxml/xmlsave.h>
#include "XmlTree.h"
#include "XmlNode.h"

static bool g_bInit;

CXmlTree::CXmlTree()
{
	m_doc = NULL;

	if(!g_bInit)
		xmlInitParser();
}

CXmlTree::~CXmlTree()
{
	Close();
}

void CXmlTree::errorFunc(void *userData, xmlErrorPtr error)
{
	CXmlTree* pthis = (CXmlTree*)userData;
	cvs::string err;

	if(error->level>=XML_ERR_ERROR)
		CServerIo::error("XML error at line %d: %s\n",error->line,error->message);
	else
		CServerIo::trace(3,"XML warning at line %d: %s\n",error->line,error->message);
}

bool CXmlTree::ReadXmlFile(const char *name)
{
	DiscardTree();

    xmlSetStructuredErrorFunc(this, errorFunc);

    xmlKeepBlanksDefault(0);
    xmlLineNumbersDefault(1);

	m_doc = xmlParseFile(name);
	if(!m_doc)
		return false;

	return true;
}

bool CXmlTree::ParseXmlFromMemory(const char *data)
{
	DiscardTree();

    xmlSetStructuredErrorFunc(this, errorFunc);

    xmlKeepBlanksDefault(0);
    xmlLineNumbersDefault(1);

	m_doc = xmlParseMemory(data,(int)strlen(data));
	if(!m_doc)
		return false;
	return true;
}

CXmlNodePtr CXmlTree::GetRoot()
{
	CServerIo::trace(3,"CXmlTree::GetRoot()");
	if(!m_doc)
		return NULL;

	CServerIo::trace(3,"CXmlTree::GetRoot() - xmlDocGetRootElement()");
	xmlNodePtr node = xmlDocGetRootElement(m_doc);
	if(!node)
		return NULL;

	CServerIo::trace(3,"CXmlTree::GetRoot() - CXmlNode(this,node)");
	CXmlNodePtr result = new CXmlNode(this,node);
	CServerIo::trace(3,"CXmlTree::GetRoot() - return");
	return result;
}

bool CXmlTree::Close()
{
	DiscardTree();
	m_doc = NULL;
	return true;
}

bool CXmlTree::WriteXmlFile(const char *file) const
{
	if(xmlSaveFormatFile(file,m_doc,1)<0)
	{
		CServerIo::error("Unable to create %s\n",file);
		return false;
	}
	return true;
}

int CXmlTree::WriteToString(void *context, const char *buffer, int len)
{
        cvs::string& str = *(cvs::string*)context;

        str.append(buffer,len);
        return len;
}

int CXmlTree::CloseWriteToString(void *context)
{
        return 0;
}

bool CXmlTree::WriteXmlFileToString(cvs::string& string) const
{
	xmlBufferPtr buffer = xmlBufferCreate();
	if(!buffer)
		return false;
	xmlSaveCtxtPtr save = xmlSaveToBuffer(buffer, NULL, 0);
	if(!save)
	{
		xmlBufferFree(buffer);
		return false;
	}
	xmlSaveDoc(save, m_doc);
	xmlSaveFlush(save);
	xmlSaveClose(save);
	string = (const char*)xmlBufferContent(buffer);
	xmlBufferFree(buffer);
	return true;
}

bool CXmlTree::DiscardTree(void)
{
  // LIXML2 - crashes all over the place on Vista64, here is an attempt at cleaning it up a tiny little bit.
  //__try
  //{
	if(m_doc)
		xmlFreeDoc(m_doc);
	m_doc=NULL;
  //}
  //__except
  //{
  //	m_doc=NULL;
  //}
	return true;
}


bool CXmlTree::CreateNewTree(const char *name, const char *value /* = NULL */)
{
	CServerIo::trace(3,"CXmlTree::CreateNewTree(%s,%s)",(name)?name:"NULL",(value)?value:"NULL");
	DiscardTree();

	CServerIo::trace(3,"CXmlTree::CreateNewTree() - xmlSetStructuredErrorFunc()");
    xmlSetStructuredErrorFunc(this, errorFunc);

	CServerIo::trace(3,"CXmlTree::CreateNewTree() - xmlKeepBlanksDefault(0)");
    xmlKeepBlanksDefault(0);
	CServerIo::trace(3,"CXmlTree::CreateNewTree() - xmlLineNumbersDefault(1)");
    xmlLineNumbersDefault(1);

	CServerIo::trace(3,"CXmlTree::CreateNewTree() - xmlNewDoc()");
	m_doc = xmlNewDoc((const xmlChar *)"1.0");
	if(!m_doc)
		return false;

	CServerIo::trace(3,"CXmlTree::CreateNewTree() - xmlNewDocNode()");
	xmlNodePtr node = xmlNewDocNode(m_doc, NULL, (const xmlChar *)name, (const xmlChar *)value);
	if(!node)
		return false;
	CServerIo::trace(3,"CXmlTree::CreateNewTree() - xmlDocSetRootElement()");
	xmlDocSetRootElement (m_doc, node);

	CServerIo::trace(3,"CXmlTree::CreateNewTree() - return");
	return true;
}

bool CXmlTree::AddNamespace(const char *prefix, const char *href)
{
	xmlNodePtr node = xmlDocGetRootElement(m_doc);
	if(!node)
		return false;
	if(!href)
		return false;
	xmlNsPtr ns = xmlNewNs(node, (const xmlChar *)href, (const xmlChar *)prefix);
	if(!ns)
		return false;
	return true;
}

bool CXmlTree::CreateCache(unsigned cacheId, const char *path, const char *attribute /*= NULL*/, CacheFlags flags /*= cacheDefault*/)
{
	CXmlNodePtr node = GetRoot();
	if(!node->Lookup(path))
	{
		CServerIo::trace(3,"CreateCache node lookup failed");
		return false;
	}

	cache_t& cache = m_Cache[cacheId];
	cache.flags = flags;
	if(flags&cacheFilename)
	{
		if(cache.filenameMap) delete cache.filenameMap;
		cache.filenameMap = new std::map<cvs::filename, xmlNodePtr>;
	}
	else if(flags&cacheUsername)
	{
		if(cache.usernameMap) delete cache.usernameMap;
		cache.usernameMap = new std::map<cvs::username, xmlNodePtr>;
	}
	else
	{
		if(cache.standardMap) delete cache.standardMap;
		cache.standardMap = new std::map<cvs::string, xmlNodePtr>;
	}
	while(node->XPathResultNext())
	{
		const char *value;
		if(attribute)
		{
			if(attribute[0]=='@')
				value = node->GetAttrValue(attribute+1);
			else
				value = node->GetNodeValue(attribute);
		}
		else
			value = node->GetValue();
		if(value)
		{
			if(flags&cacheFilename)
				(*cache.filenameMap)[value]=node->Object();
			else if(flags&cacheUsername)
				(*cache.usernameMap)[value]=node->Object();
			else
				(*cache.standardMap)[value]=node->Object();
		}
	}	
	return true;
}

CXmlNodePtr CXmlTree::GetNodeFromCache(unsigned cacheId, const char *value)
{
	cacheMap_t::const_iterator i = m_Cache.find(cacheId);
	if(!value || i==m_Cache.end())
		return NULL;
	const cache_t& cache = i->second;

	if(cache.flags&cacheFilename)
	{
		if(cache.filenameMap->find(value)!=cache.filenameMap->end())
			return new CXmlNode(this,(*cache.filenameMap)[value]);
		else
			return NULL;
	}
	else if(cache.flags&cacheUsername)
	{
		if(cache.usernameMap->find(value)!=cache.usernameMap->end())
			return new CXmlNode(this,(*cache.usernameMap)[value]);
		else
			return NULL;
	}
	else 
	{
		if(cache.standardMap->find(value)!=cache.standardMap->end())
			return new CXmlNode(this,(*cache.standardMap)[value]);
		else
			return NULL;
	}
}

bool CXmlTree::AddToCache(unsigned cacheId, const char *key, CXmlNodePtr node)
{
	cacheMap_t::const_iterator i = m_Cache.find(cacheId);
	if(!node || !key || i==m_Cache.end())
		return false;
	const cache_t& cache = i->second;

	assert(node->m_tree == this);

	if(cache.flags&cacheFilename)
		(*cache.filenameMap)[key]=node->Object();
	else if(cache.flags&cacheUsername)
		(*cache.usernameMap)[key]=node->Object();
	else
		(*cache.standardMap)[key]=node->Object();
	return true;
}

bool CXmlTree::DeleteFromCache(unsigned cacheId, const char *key)
{
	cacheMap_t::const_iterator i = m_Cache.find(cacheId);
	if(!key || i==m_Cache.end())
		return false;

	const cache_t& cache = i->second;

	if(cache.flags&cacheFilename)
	{
		std::map<cvs::filename, xmlNodePtr>::iterator i = cache.filenameMap->find(key);
		if(i!=cache.filenameMap->end())
			cache.filenameMap->erase(i);
	}
	else if(cache.flags&cacheUsername)
	{
		std::map<cvs::username, xmlNodePtr>::iterator i = cache.usernameMap->find(key);
		if(i!=cache.usernameMap->end())
			cache.usernameMap->erase(i);
	}
	else
	{
		std::map<cvs::string, xmlNodePtr>::iterator i = cache.standardMap->find(key);
		if(i!=cache.standardMap->end())
			cache.standardMap->erase(i);
	}
	return true;
}
