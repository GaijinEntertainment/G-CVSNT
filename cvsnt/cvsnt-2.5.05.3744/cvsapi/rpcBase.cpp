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
#include <config.h>
#include "lib/api_system.h"
#include "cvs_string.h"
#include "cvs_smartptr.h"
#include "Codepage.h"
#include "XmlNode.h"
#include "XmlTree.h"
#include "rpcBase.h"

CrpcBase::CrpcBase()
{
}

CrpcBase::~CrpcBase()
{
}

CXmlNodePtr CrpcBase::createNewParams(CXmlTree& tree)
{
	if(!tree.CreateNewTree("params"))
		return NULL;
	return tree.GetRoot();
}

bool CrpcBase::addParam(CXmlNodePtr params, const char *name, const char *value)
{
	CXmlNodePtr param = params->Clone();

	if(!strcmp(param->GetName(),"params"))
		param->NewNode("param", NULL);
	else if(!strcmp(param->GetName(),"struct"))
	{
		param->NewNode("member", NULL);
		if(name)
			param->NewNode("name",name,false);
	}
	param->NewNode("value",NULL);
	param->NewNode("string",value);
	return true;
}

bool CrpcBase::addParam(CXmlNodePtr params, const char *name, int value)
{
	char tmp[32];
	snprintf(tmp,sizeof(tmp),"%d",value);
	CXmlNodePtr param = params->Clone();

	if(!strcmp(param->GetName(),"params"))
		param->NewNode("param", NULL);
	else if(!strcmp(param->GetName(),"struct"))
	{
		param->NewNode("member", NULL);
		if(name)
			param->NewNode("name",name,false);
	}
	param->NewNode("value",NULL);
	param->NewNode("i4",tmp);
	return true;
}

bool CrpcBase::addParam(CXmlNodePtr params, const char *name, rpcObject *obj)
{
	CXmlNodePtr param = params->Clone();

	if(!strcmp(param->GetName(),"params"))
		param->NewNode("param", NULL);
	else if(!strcmp(param->GetName(),"struct"))
	{
		param->NewNode("member", NULL);
		if(name)
			param->NewNode("name",name,false);
	}
	param->NewNode("value",NULL);

	return obj->Marshall(param);
}

bool CrpcBase::rpcInt(CXmlNodePtr param, const char *name, int& value)
{
	cvs::string fnd;
	CXmlNodePtr val;

	val = param->Clone();
	if(!strcmp(val->GetName(),"param")) 
		val->GetChild();
	if(!strcmp(val->GetName(),"struct"))
	{
		if(name)
		{
			cvs::sprintf(fnd,64,"member[@name='%s']",name);
			if(!val->Lookup(fnd.c_str()))
				return false;
			if(!val->XPathResultNext())
				return false;
		}
		else
			val->GetChild();
		val->GetChild("value");
	}
	if(strcmp(val->GetName(),"value"))
		return false;
	if(!val->GetChild())
		return false;
	if(strcmp(val->GetName(),"i4"))
		return false;
	value = atoi(val->GetValue());
	return true;
}

bool CrpcBase::rpcString(CXmlNodePtr param, const char *name, cvs::string& value)
{
	cvs::string fnd;
	CXmlNodePtr val;

	val = param->Clone();
	if(!strcmp(val->GetName(),"param")) 
		val->GetChild();
	if(!strcmp(val->GetName(),"struct"))
	{
		if(name)
		{
			cvs::sprintf(fnd,64,"member[@name='%s']",name);
			if(!val->Lookup(fnd.c_str()))
				return false;
			if(!val->XPathResultNext())
				return false;
		}
		else
			val->GetChild();
		val->GetChild("value");
	}
	if(strcmp(val->GetName(),"value"))
		return false;
	if(!val->GetChild())
		return false;
	if(!strcmp(val->GetName(),"string"))
		return false;
	value = val->GetValue();
	return true;
}

bool CrpcBase::rpcArray(CXmlNodePtr param, const char *name, CXmlNodePtr& node)
{
	CXmlNodePtr val = param->Clone();

	if(!strcmp(val->GetName(),"param"))
		val->GetChild();
	if(!strcmp(val->GetName(),"array"))
		return false;

	if(!node)
	{
		if(!val->GetChild())
			return false;
		if(!strcmp(val->GetName(),"data"))
			return false;
		node = val->Clone();
		return true;
	}
	else
	{
		if(!node->GetParent()) return false;
		if(!node->GetSibling()) return false;
		if(!strcmp(node->GetName(),"data"))
			return false;
		if(!node->GetChild()) return false;
		return true;
	}
}

bool CrpcBase::rpcObj(CXmlNodePtr param, const char *name, rpcObject *rpcObj)
{
	cvs::string fnd;
	CXmlNodePtr val;

	val = param->Clone();
	if(!strcmp(val->GetName(),"param")) 
		val->GetChild();
	if(!strcmp(val->GetName(),"struct"))
	{
		if(name)
		{
			cvs::sprintf(fnd,64,"member[@name='%s']",name);
			if(!val->Lookup(fnd.c_str()))
				return false;
			if(!val->XPathResultNext())
				return false;
		}
		else
			val->GetChild();
		val->GetChild("value");
	}
	if(strcmp(val->GetName(),"value"))
		return false;
	if(!val->GetChild())
		return false;
	if(strcmp(val->GetName(),"struct"))
		return false;
	return rpcObj->Marshall(val);
}

CXmlNodePtr CrpcBase::rpcFault(CXmlTree& tree, int err, const char *error)
{
	if(!tree.CreateNewTree("fault")) return NULL;
	CXmlNodePtr fault = tree.GetRoot();
	fault->NewNode("value", NULL);
	fault->NewNode("struct", NULL);
	addParam(fault,"faultCode",err);
	addParam(fault,"faultString",error);
	fault->GetParent();
	fault->GetParent();
	return fault;
}

CXmlNodePtr CrpcBase::rpcResponse(CXmlNodePtr result)
{
	// Looks dodgy.. probably broken
	// Not sure what this is actually supposed to do right now - fix later
	CXmlTree *tree = result->GetTree();
	if(!tree->CreateNewTree("methodResponse")) return NULL;
	CXmlNodePtr response = tree->GetRoot();
	response->NewNode("params");
	response->CopySubtree(result);
	response->GetParent();
	return response;
}

CXmlNodePtr CrpcBase::rpcCall(const char *method, CXmlNodePtr param)
{
	// Looks dodgy.. probably broken
	// Not sure what this is actually supposed to do right now - fix later
	CXmlTree *tree = param->GetTree();
	if(!tree->CreateNewTree("methodCall")) return NULL;
	CXmlNodePtr methodNode = tree->GetRoot();
	methodNode->NewNode("methodName",method,false);
	methodNode->NewNode("params");
	methodNode->CopySubtree(param);
	methodNode->GetParent();
	return methodNode;
}
