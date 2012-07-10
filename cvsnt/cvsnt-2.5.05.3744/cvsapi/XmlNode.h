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
#ifndef XMLNODE__H
#define XMLNODE__H

#include "cvs_smartptr.h"
#include <map>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#define myxmlFree(y) xmlFree(y)

class CXmlNode;
class CXmlTree;

typedef cvs::smartptr<CXmlNode> CXmlNodePtr;


class CXmlNode
{
	friend class CXmlTree;
public:
	CVSAPI_EXPORT CXmlNode(CXmlTree* tree, xmlNodePtr node);
	CVSAPI_EXPORT CXmlNode(const CXmlNode& other);
	CVSAPI_EXPORT virtual ~CXmlNode();
	CVSAPI_EXPORT bool NewAttribute(const char *name, const char *value);
	CVSAPI_EXPORT bool NewNode(const char *name, const char *value = NULL, bool select = true, const char *prefix = NULL);
	CVSAPI_EXPORT void SetValue(const char *value);
	CVSAPI_EXPORT void AppendValue(const char *value);
	CVSAPI_EXPORT const char *GetValue() const; 
	CVSAPI_EXPORT const char *GetName() const;
	CVSAPI_EXPORT const char *GetPrefix() const;
	CVSAPI_EXPORT const char *GetAttrValue(const char *name);
	CVSAPI_EXPORT bool SetAttrValue(const char *name, const char *value);
	CVSAPI_EXPORT bool DeleteAttr(const char *name);
	CVSAPI_EXPORT const char *GetNodeValue(const char *name);
	CVSAPI_EXPORT bool SetNamespace(const char *prefix);
	CVSAPI_EXPORT bool xpathVariable(const char *name, const char *value);
	CVSAPI_EXPORT bool Lookup(const char *path);
	CVSAPI_EXPORT bool LookupF(const char *path, ...);
	CVSAPI_EXPORT bool XPathResultNext();
	CVSAPI_EXPORT bool GetParent();
	CVSAPI_EXPORT bool GetChild(const char *filter = NULL, bool select = true);
	CVSAPI_EXPORT bool GetSibling(const char *filter = NULL, bool select = true);
	CVSAPI_EXPORT unsigned short GetLine();
	CVSAPI_EXPORT CXmlNodePtr Clone();
	CVSAPI_EXPORT bool Delete();
	CVSAPI_EXPORT bool DeleteAllChildren();
	CVSAPI_EXPORT xmlNodePtr Object() { return m_node; };
	CVSAPI_EXPORT CXmlTree* GetTree() { return m_tree; };
	CVSAPI_EXPORT bool WriteXmlFragmentToString(cvs::string& string);
	CVSAPI_EXPORT bool ParseXmlFragmentFromMemory(const char *data);
	CVSAPI_EXPORT bool CopySubtree(CXmlNodePtr tree);
	CVSAPI_EXPORT CXmlNodePtr DuplicateNode();

protected:
	CXmlTree *m_tree;
	xmlNodePtr m_node;
	xmlXPathObjectPtr m_xpathObj; 
	int m_xpathNode;
	std::map<cvs::string,cvs::string> m_xpathVars;

	xmlNodePtr _GetChild(const char *filter);

	static void XpathUsername(xmlXPathParserContextPtr ctxt, int nargs); // evs:username(.,'foo')
	static void XpathFilename(xmlXPathParserContextPtr ctxt, int nargs); // evs:filename(.,'foo')
	
	static int WriteToString(void *context, const char *buffer, int len);
	static int CloseWriteToString(void *context);
};

#endif
