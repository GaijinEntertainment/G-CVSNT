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
#ifndef XMLTREE__H
#define XMLTREE__H

#include <map>

#include "XmlNode.h"

class CXmlTree;

typedef cvs::smartptr<CXmlTree> CXmlTreePtr;

class CXmlTree
{
	friend class CXmlNode;

public:
	enum CacheFlags
	{
		cacheDefault = 0,
		cacheFilename = 1,
		cacheUsername = 2
	};

	CVSAPI_EXPORT CXmlTree();
	CVSAPI_EXPORT virtual ~CXmlTree();

	CVSAPI_EXPORT bool ReadXmlFile(const char *name);
	CVSAPI_EXPORT bool WriteXmlFile(const char *name) const;
	CVSAPI_EXPORT bool ParseXmlFromMemory(const char *data);
	CVSAPI_EXPORT bool WriteXmlFileToString(cvs::string& string) const;
	CVSAPI_EXPORT bool CreateNewTree(const char *name, const char *value = NULL);
	CVSAPI_EXPORT bool AddNamespace(const char *prefix, const char *href);
	CVSAPI_EXPORT CXmlNodePtr GetRoot();
	CVSAPI_EXPORT bool Close();

	CVSAPI_EXPORT bool CreateCache(unsigned cacheId, const char *node, const char *attribute = NULL, CacheFlags flags = cacheDefault);
	CVSAPI_EXPORT CXmlNodePtr GetNodeFromCache(unsigned cacheId, const char *value);
	CVSAPI_EXPORT bool AddToCache(unsigned cacheId, const char *key, CXmlNodePtr node);
	CVSAPI_EXPORT bool DeleteFromCache(unsigned cacheId, const char *key);

protected:
	CVSAPI_EXPORT bool DiscardTree();
	xmlDocPtr m_doc;
	struct cache_t
	{
		CacheFlags flags;
		union
		{
			std::map<cvs::string, xmlNodePtr>* standardMap;
			std::map<cvs::filename, xmlNodePtr>* filenameMap;
			std::map<cvs::username, xmlNodePtr>* usernameMap;
		};

		cache_t() { flags=cacheDefault; standardMap=NULL; }
		~cache_t() { if(flags&cacheFilename) delete filenameMap; else if(flags&cacheUsername) delete usernameMap; else delete standardMap; }
	};
	typedef std::map<unsigned, cache_t> cacheMap_t;
	cacheMap_t m_Cache;

	static void errorFunc(void *userData, xmlErrorPtr error);

        static int WriteToString(void *context, const char *buffer, int len);
        static int CloseWriteToString(void *context);
};

#endif
