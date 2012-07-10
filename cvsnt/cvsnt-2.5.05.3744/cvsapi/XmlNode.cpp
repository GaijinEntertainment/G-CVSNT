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
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <ctype.h>
#include <string.h>
#include <algorithm>
#include <string>

#include "cvs_string.h"
#include "cvs_smartptr.h"
#include "Codepage.h"
#include "ServerIO.h"
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include "XmlTree.h"
#include "XmlNode.h"

CXmlNode::CXmlNode(CXmlTree* tree, xmlNodePtr node) 
{ 
	CServerIo::trace(4,"CXmlNode::CXmlNode(tree,node)");
	m_xpathObj = NULL; m_xpathNode = 0; m_tree = tree; m_node = node; 
}

CXmlNode::CXmlNode(const CXmlNode& other) 
{ 
	CServerIo::trace(4,"CXmlNode::CXmlNode(other)");
	m_xpathObj = NULL; m_xpathNode = 0; m_tree = other.m_tree; m_node = other.m_node; 
}

CXmlNode::~CXmlNode()
{
	if(m_xpathObj)
        xmlXPathFreeObject(m_xpathObj); 
}

bool CXmlNode::NewAttribute(const char *name, const char *value)
{
	xmlAttrPtr newattr = xmlNewProp(m_node, (const xmlChar*)name, (const xmlChar*)value);
	return newattr?true:false;
}

bool CXmlNode::NewNode(const char *name, const char *value /* = NULL */, bool select /* = true */, const char *prefix /* = null */)
{
	xmlNsPtr ns = NULL;
	
	if(ns)
	{
		ns = xmlSearchNs(m_tree->m_doc, m_node, (const xmlChar *)prefix);
		if(!ns)
			return false;
	}
	xmlNodePtr newnode = xmlNewChild(m_node, ns, (const xmlChar*)name, (const xmlChar*)value);
	if(select)
		m_node = newnode;
	return newnode?true:false;
}

const char *CXmlNode::GetValue() const
{
	return (const char *)xmlNodeGetContent(m_node);
}

const char *CXmlNode::GetName() const
{
	return (const char *)m_node->name;
}

const char *CXmlNode::GetPrefix() const
{
	if(!m_node->ns)
		return NULL;
	return (const char *)m_node->ns->prefix;
}

void CXmlNode::SetValue(const char *value)
{
	xmlNodeSetContent(m_node, (const xmlChar*)value);
}

void CXmlNode::AppendValue(const char *value)
{
	xmlNodeAddContent(m_node, (const xmlChar*)value);
}

const char *CXmlNode::GetAttrValue(const char *name)
{
	return (const char *)xmlGetProp(m_node, (const xmlChar *)name);
}

bool CXmlNode::SetAttrValue(const char *name, const char *value)
{
	if(!xmlSetProp(m_node, (const xmlChar *)name, (const xmlChar *)value))
		return false;
	return true;
}

bool CXmlNode::DeleteAttr(const char *name)
{
	if(xmlUnsetProp(m_node, (const xmlChar *)name))
		return false;
	return true;
}

const char *CXmlNode::GetNodeValue(const char *name)
{
	xmlNodePtr node = _GetChild(name);
	if(!node)
		return NULL;
	return (const char *)xmlNodeGetContent(node);
}

bool CXmlNode::SetNamespace(const char *prefix)
{
	xmlNsPtr ns = NULL;
	
	if(prefix)
	{
		ns = xmlSearchNs(m_tree->m_doc, m_node, (const xmlChar *)prefix);
		if(!ns)
			return false;
	}

	xmlSetNs(m_node, ns);
	return true;
}

bool CXmlNode::xpathVariable(const char *variable, const char *value)
{
	m_xpathVars[variable]=value;
	return true;
}

bool CXmlNode::LookupF(const char *path, ...)
{
	cvs::string str;
	va_list va;

	va_start(va,path);
	cvs::vsprintf(str,80,path,va);
	va_end(va);
	return Lookup(str.c_str());
}


bool CXmlNode::Lookup(const char *path)
{
	xmlXPathContextPtr xpathCtx; 
    xmlXPathObjectPtr xpathObj; 
	int r1, r2, r3;

	CServerIo::trace(3,"CXmlNode::Lookup(%s)",path);

	if(m_xpathObj)
        xmlXPathFreeObject(m_xpathObj); 
	m_xpathObj = NULL;

    /* Create xpath evaluation context */
    xpathCtx = xmlXPathNewContext(m_tree->m_doc);
    if(!xpathCtx)
	{
		CServerIo::error("Unable to create XPath context\n");
		return false;
    }
#ifdef _DEBUG
	xmlXPathFreeContext(xpathCtx); 
    xpathCtx = xmlXPathNewContext(m_tree->m_doc);
    if(!xpathCtx)
	{
		CServerIo::error("Unable to create XPath context\n");
		return false;
    }
#endif

        xpathCtx->node = m_node;

	// Register evs:filename and evs:username
	r1 = xmlXPathRegisterNs(xpathCtx,(const xmlChar*)"cvs",(const xmlChar*)"http://www.cvsnt.org/namespace/xpath");
	r2 = xmlXPathRegisterFuncNS(xpathCtx,(const xmlChar*)"filename",(const xmlChar*)"http://www.cvsnt.org/namespace/xpath",XpathFilename);
	r3 = xmlXPathRegisterFuncNS(xpathCtx,(const xmlChar*)"username",(const xmlChar*)"http://www.cvsnt.org/namespace/xpath",XpathUsername);

	for(std::map<cvs::string,cvs::string>::const_iterator i = m_xpathVars.begin(); i!=m_xpathVars.end(); ++i)
		xmlXPathRegisterVariable(xpathCtx, (const xmlChar*)i->first.c_str(), xmlXPathNewCString(i->second.c_str()));

	xpathObj = xmlXPathEvalExpression((const xmlChar *)path, xpathCtx);
    if(!xpathObj)
	{
		CServerIo::error("Unable to evaluate xpath expression '%s'\n", path);
        xmlXPathFreeContext(xpathCtx); 
		return false;
    }

	if(xmlXPathNodeSetIsEmpty(xpathObj->nodesetval))
	{
		CServerIo::trace(3,"xpath expression '%s' returned null resultset", path);
	}

	m_xpathObj = xpathObj;
	m_xpathNode = 0;

	xmlXPathFreeContext(xpathCtx); 

	return true;
}

bool CXmlNode::XPathResultNext()
{
	if(!m_xpathObj || !m_xpathObj->nodesetval)
		return false;

	for(;m_xpathNode < m_xpathObj->nodesetval->nodeNr; m_xpathNode++)
	{
		xmlNodePtr cur = m_xpathObj->nodesetval->nodeTab[m_xpathNode];
		if(cur->type==XML_ELEMENT_NODE)
		{
			m_node = cur;
			m_xpathNode++;
			return true;
		}
	}
	return false;
}

void CXmlNode::XpathFilename(xmlXPathParserContextPtr ctxt, int nargs)
{
	xmlXPathObjectPtr fn1,fn2;

    CHECK_ARITY(2);
    CAST_TO_STRING;
    CHECK_TYPE(XPATH_STRING);
	fn1 = valuePop(ctxt);
	CAST_TO_STRING;
    CHECK_TYPE(XPATH_STRING);
	fn2 = valuePop(ctxt);
	if(!fncmp((const char *)fn1->stringval,(const char *)fn2->stringval))
		valuePush(ctxt, xmlXPathNewBoolean(1));
    else
		valuePush(ctxt, xmlXPathNewBoolean(0));
	xmlXPathFreeObject(fn1);
	xmlXPathFreeObject(fn2);
}

void CXmlNode::XpathUsername(xmlXPathParserContextPtr ctxt, int nargs)
{
	xmlXPathObjectPtr fn1,fn2;

    CHECK_ARITY(2);
    CAST_TO_STRING;
    CHECK_TYPE(XPATH_STRING);
	fn1 = valuePop(ctxt);
	CAST_TO_STRING;
    CHECK_TYPE(XPATH_STRING);
	fn2 = valuePop(ctxt);
	if(!usercmp((const char *)fn1->stringval,(const char *)fn2->stringval))
		valuePush(ctxt, xmlXPathNewBoolean(1));
    else
		valuePush(ctxt, xmlXPathNewBoolean(0));
	xmlXPathFreeObject(fn1);
	xmlXPathFreeObject(fn2);
}

bool CXmlNode::GetChild(const char *filter /* = NULL */, bool select /*= true*/)
{
	xmlNodePtr node = _GetChild(filter);

	if(!node)
		return false;

	if(select)
		m_node = node;

	return true;
}

xmlNodePtr CXmlNode::_GetChild(const char *filter)
{
	xmlNodePtr node;

	if(!m_node->children)
		return NULL;
	node = m_node->children;
	if(filter && strcmp((const char *)node->name,filter))
	{
		do
		{
			if(!node->next)
				return NULL;
			node = node->next;
		} while(filter && strcmp((const char *)node->name,filter));
	}
	return node;
}

bool CXmlNode::GetSibling(const char *filter /* = NULL */, bool select /*= true*/)
{
	xmlNodePtr node = m_node;

	do
	{
		if(!node->next)
			return false;
		node = node->next;
	} while(filter && strcmp((const char *)node->name,filter));
	if(select)
		m_node = node;
	return true;
}

bool CXmlNode::GetParent()
{
	if(!m_node || !m_node->parent)
		return false;
	m_node = m_node->parent;
	return true;
}

unsigned short CXmlNode::GetLine()
{
	return m_node->line;
}

CXmlNodePtr CXmlNode::Clone()
{
	CXmlNodePtr n = new CXmlNode(m_tree, m_node);
	return n;
}

bool CXmlNode::Delete()
{
	if(!m_node->parent)
	{
		CServerIo::trace(3,"Attempt to delete root node of tree failed");
		return false;
	}
	xmlNodePtr parent = m_node->parent;

	xmlUnlinkNode(m_node);
	myxmlFree(m_node);
	m_node = parent;
	return true;
}

bool CXmlNode::DeleteAllChildren()
{
	if(!m_node->children)
		return true;
	xmlFreeNodeList(m_node->children);
	m_node->children = NULL;
	return true;
}

int CXmlNode::WriteToString(void *context, const char *buffer, int len)
{
	cvs::string& str = *(cvs::string*)context;

	str.append(buffer,len);
	return len;
}

int CXmlNode::CloseWriteToString(void *context)
{
	return 0;
}

bool CXmlNode::WriteXmlFragmentToString(cvs::string& string)
{
	xmlBufferPtr buffer = xmlBufferCreate();
	if(!buffer)
		return false;
	xmlSaveCtxtPtr save = xmlSaveToBuffer(buffer, NULL, XML_SAVE_FORMAT|XML_SAVE_NO_DECL);
	if(!save)
	{
		xmlBufferFree(buffer);
		return false;
	}
	xmlSaveTree(save, m_node);
	xmlSaveClose(save);
	string = (const char*)xmlBufferContent(buffer);
	xmlBufferFree(buffer);
	return true;
}

bool CXmlNode::ParseXmlFragmentFromMemory(const char *data)
{
	xmlNodePtr newnode, node;
	xmlParserCtxtPtr ctxt;

	ctxt = xmlCreateMemoryParserCtxt(data, (int)strlen(data));
	if (!ctxt)
		return false;

	newnode = xmlNewChild(m_node, NULL, (const xmlChar*)"tmpNode", NULL);
	xmlUnlinkNode(newnode);

	ctxt->myDoc = m_node->doc;
	ctxt->node = newnode;
	ctxt->sax->startDocument=NULL;

	xmlParseDocument(ctxt);

	if (!ctxt->wellFormed || !newnode->children)
	{
		xmlFreeParserCtxt(ctxt);
		myxmlFree(newnode);
		return false;
	}

	xmlFreeParserCtxt(ctxt);

	if(!(node = xmlAddChildList(m_node, newnode->children)))
	{
		myxmlFree(newnode);
		return false;
	}

	newnode->children = newnode->last = NULL;
	myxmlFree(newnode);

	m_node = node;

	return true;
}

bool CXmlNode::CopySubtree(CXmlNodePtr tree)
{
	xmlNodePtr nodeList;

	if(!tree->m_node->children)
		return true;

	// For a detached list we simply re-attach it
	if(tree->m_node->doc)
		nodeList = xmlCopyNodeList(tree->m_node->children);
	else
		nodeList = tree->m_node->children;

	if(!nodeList)
		return false;
	
	xmlAddChildList(m_node, nodeList); // Do we need to free nodeList somehow?  Docs are unclear
	return true;
}

CXmlNodePtr CXmlNode::DuplicateNode()
{
	xmlNodePtr node = xmlCopyNode(m_node,1);
	return new CXmlNode(m_tree,node);
}
