/*	cvsnt xml xdiff
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
#include <config.h>
#include <system.h>
#include <stdarg.h>
#include <stack>
#include "xdiff.h"
#include "../cvsapi/cvsapi.h"
#include "../diff/diffrun.h"
#include <cvstools.h>
#include "../version.h"

static int (*g_outputFn)(const char *text,size_t len);

struct diffStruct_t
{
	char type;
	size_t start1,end1;
	size_t start2,end2;
	int origStart;
	int size;
};

static int compareTree(CXmlNodePtr tree1, CXmlNodePtr tree2,std::vector<std::pair<CXmlNodePtr ,bool> >& changed);
static const char *getPath(const CXmlNodePtr node, const char *prefix);
static bool nodeEqual(const CXmlNodePtr  node1, const CXmlNodePtr  node2);
static bool walk_tree(CXmlNodePtr left, CXmlNodePtr right, int oldline, int& oldmin, int& oldmax, int& newmin, int& newmax);
static bool transform_line_number(CXmlNodePtr left, CXmlNodePtr right, size_t& line);
static bool transform_context(CXmlNodePtr left, CXmlNodePtr right, const char *line, std::vector<diffStruct_t>& diffArray, diffStruct_t& change);
static bool transform_diff(CXmlNodePtr left, CXmlNodePtr right, const char *diff_in, std::vector<diffStruct_t>& diffArray);

static int xdiff_print(const char *fmt, ...)
{
	cvs::string str;
	va_list va;

	va_start(va, fmt);
	cvs::vsprintf(str,256,fmt,va);
	va_end(va);
	g_outputFn(str.c_str(),str.length());
	return str.length();
}

static int xdiff_function(const char *name, const char *file1, const char *file2, const char *label1, const char *label2, int argc, const char *const*argv, int (*output_fn)(const char *,size_t))
{
	std::vector<std::pair<CXmlNodePtr,bool> > changed;
	std::vector<cvs::string> ignore_tag;
	g_outputFn = output_fn;
	bool standard_diff = false;
	CXmlTree tree1,tree2,tree3;

	CTokenLine tok(argc,argv);
	size_t argnum=0;
	CGetOptions opt(tok,argnum,"i:d");
	if(argnum!=argc)
	{
		xdiff_print("Invalid arguments to xdiff.  Aborting.");
		return 1;
	}
	
	for(argnum=0; argnum<opt.count(); argnum++)
	{
		switch(opt[argnum].option)
		{
		case 'i':
			ignore_tag.push_back(opt[argnum].arg);
			break;
		case 'd':
			standard_diff = true;
			break;
		default:
			break;
		}
	}

	if(!tree1.ReadXmlFile(file1))
	{
		xdiff_print("Couldn't read file '%s' as valid XML",file1);
		return 1;
	}

	if(!tree2.ReadXmlFile(file2))
	{
		xdiff_print("Couldn't read file '%s' as valid XML",file2);
		return 1;
	}
	CXmlNodePtr file1Root = tree1.GetRoot();
	CXmlNodePtr file2Root = tree2.GetRoot();

	if(standard_diff)
	{
//#ifdef _DEBUG
//		cvs::string tempfile1 = "d:\\t\\xml1.out";
//		cvs::string tempfile2 = "d:\\t\\xml2.out";
//		cvs::string difftemp1 = "d:\\t\\diff1.out";
//#else
		cvs::string tempfile1 = CFileAccess::tempfilename("xdiff");
		cvs::string tempfile2 = CFileAccess::tempfilename("xdiff");
		cvs::string difftemp1 = CFileAccess::tempfilename("xdiff");
//#endif
		tree1.WriteXmlFile(tempfile1.c_str());
		tree2.WriteXmlFile(tempfile2.c_str());
		CTokenLine diffargs;
		diffargs.addArg("xmldiff");
		diffargs.addArg("-bBNw");
		diffargs.addArg(tempfile1.c_str());
		diffargs.addArg(tempfile2.c_str());
		int ret = diff_run(diffargs.size(),(char**)diffargs.toArgv(),difftemp1.c_str(),NULL);
//#ifndef _DEBUG
		CFileAccess::remove(tempfile1.c_str());
//#endif
		if(!tree3.ReadXmlFile(tempfile2.c_str()))
		{
			xdiff_print("Internal error - couldn't read partial result file");
//#ifndef _DEBUG
			CFileAccess::remove(tempfile2.c_str());
			CFileAccess::remove(difftemp1.c_str());
//#endif
			return 1;
		}
		CXmlNodePtr temp2Root = tree3.GetRoot();

		std::vector<diffStruct_t> diffArray;
		transform_diff(temp2Root,file2Root,difftemp1.c_str(),diffArray);
		temp2Root=NULL;
		file2Root=NULL;
//#ifndef _DEBUG
		CFileAccess::remove(tempfile2.c_str());
		CFileAccess::remove(difftemp1.c_str());
//#endif
		for(size_t n=0; n<diffArray.size(); n++)
		{
			xdiff_print("%c %d %d -> %d %d\n",diffArray[n].type,diffArray[n].start1,diffArray[n].end1,diffArray[n].start2,diffArray[n].end2);
		}
		return ret;
	}
	else
	{
		compareTree(file1Root,file2Root,changed);

		for(size_t i=0; i<changed.size(); i++)
		{
			if(i+1<changed.size() && changed[i].second!=changed[i+1].second && nodeEqual(changed[i].first,changed[i+1].first))
				changed.erase(changed.begin()+i+1);
			else
				xdiff_print("%s",getPath(changed[i].first,changed[i].second?"- ":"+ "));
		}
		return changed.size()?1:0;
	}
}

static int compareTree(CXmlNodePtr tree1, CXmlNodePtr tree2,std::vector<std::pair<CXmlNodePtr ,bool> >& changed)
{
	CXmlNodePtr i = tree1->Clone();
	CXmlNodePtr j = tree2->Clone();

	bool bi = i->GetChild();
	bool bj = j->GetChild();

	while(bi&&bj)
	{
		int diff = strcmp(i->GetName(),j->GetName());
		if(diff<0)
		{
			changed.push_back(std::pair<CXmlNodePtr,bool>(i->Clone(),true));
			bi=i->GetSibling();
		}
		else if(diff>0)
		{
			changed.push_back(std::pair<CXmlNodePtr,bool>(j->Clone(),false));
			bj=j->GetSibling();
		}
		else
		{
			diff = strcmp(i->GetValue(),j->GetValue());
			if(diff<0)
			{
				changed.push_back(std::pair<CXmlNodePtr,bool>(i->Clone(),true));
				bi=i->GetSibling();
			}
			else if(diff>0)
			{
				changed.push_back(std::pair<CXmlNodePtr,bool>(j->Clone(),false));
				bj=j->GetSibling();
			}
			else
			{
				compareTree(i,j,changed);
				bi=i->GetSibling();
				bj=j->GetSibling();
			}
		}
	}		
	while(bi)
	{
		changed.push_back(std::pair<CXmlNodePtr,bool>(i->Clone(),true));
		bi=i->GetSibling();
	}
	while(bj)
	{
		changed.push_back(std::pair<CXmlNodePtr ,bool>(j->Clone(),false));
		bj=j->GetSibling();
	}
	return 0;
}

static const char *getPath(const CXmlNodePtr node, const char *prefix)
{
	std::stack<const CXmlNodePtr > nodes;
	static cvs::string path;
	int lev;
	char tmp[64];
	CXmlNodePtr n = node->Clone();

	do
	{
		nodes.push(n);
	} while(n->GetParent());

	path="";

	lev=0;
	while(nodes.size())
	{
		n = nodes.top();
		snprintf(tmp,10,"%5d",n->GetLine());
		path+=tmp;
		path+=": ";
		path+=prefix;
		path.append(lev,' ');
		path.append("<");
		path+= n->GetName();
		path.append(">");
		nodes.pop();
		if(nodes.size())
			path+= "\n";
		lev++;
	}
	path+=n->GetValue();
	--lev;
	path.append("</");
	path+=n->GetName();
	path.append(">");
	path+="\n";
	while(n->GetParent())
	{
		--lev;
		snprintf(tmp,10,"%5d",n->GetLine());
		path+=tmp;
		path+=": ";
		path+=prefix;
		path.append(lev,' ');
		path.append("</");
		path+=n->GetName();
		path.append(">");
		path+="\n";
	}
	return path.c_str();
}

static bool nodeEqual(const CXmlNodePtr node1, const CXmlNodePtr node2)
{
	if((!node1 && node2) || (node2 && !node1))
		return false;
	if(!node1 && !node2)
		return true;

	if(strcmp(node1->GetName(),node2->GetName()))
		return false;
	if(strcmp(node1->GetValue(),node2->GetValue()))
		return false;
	CXmlNodePtr n1 = node1->Clone();
	CXmlNodePtr n2 = node2->Clone();
	n1->GetParent();
	n2->GetParent();
	return nodeEqual(n1,n2);
}

/* Walk two semantically identical trees and find map source line numbers */
static bool walk_tree(CXmlNodePtr left, CXmlNodePtr right, int oldline, int& oldmin, int& oldmax, int& newmin, int& newmax)
{
	CXmlNodePtr i = left->Clone();
	CXmlNodePtr j = right->Clone();
	bool bi = i->GetChild();
	bool bj = j->GetChild();

	while(bi && bj)
	{
		if(strcmp(i->GetName(),i->GetName()))
		{
			xdiff_print("Internal error - trees not identical");
		}
		if(strcmp(i->GetValue(),j->GetValue()))
		{
//			printf("value different:\n");
//			printf("%s\n-------%s\n",c1->GetValue(),c2->GetValue());
		}
		if(i->GetLine()>=oldmin && i->GetLine()<=oldmax && i->GetLine()<=oldline && i->GetLine()>=oldline)
		{
			oldmin=i->GetLine();
			oldmax=i->GetLine();
			newmin=j->GetLine();
			newmax=j->GetLine();
		}	
		walk_tree(i,j,oldline,oldmin,oldmax,newmin,newmax);
		bi = i->GetSibling();
		bj = j->GetSibling();
	}		
	return 0;
}

static bool transform_line_number(CXmlNodePtr left, CXmlNodePtr right, size_t& line)
{
	int oldmin,oldmax,newmin,newmax;

	oldmin=-1;
	oldmax=0x7fffffff;

	walk_tree(left,right,line,oldmin,oldmax,newmin,newmax);
	line=(line-oldmin)+newmin;
	return true;
}

static bool transform_context(CXmlNodePtr left, CXmlNodePtr right, const char *line, std::vector<diffStruct_t>& diffArray, diffStruct_t& change)
{
	const char *p;
	/* Line is <start>[,<end>]<type><start>[,end] */

	memset(&change,0,sizeof(change));

	p=line;
	while(isdigit(*p)) p++;
	change.start1=atoi(line);
	if(*p==',')
	{
		line=++p;
		while(isdigit(*p)) p++;
		change.end1=atoi(line);
	}
	change.type=*p;
	line=++p;
	while(isdigit(*p)) p++;
	change.start2=atoi(line);
	if(*p==',')
	{
		line=++p;
		while(isdigit(*p)) p++;
		change.end2=atoi(line);
	}

	change.origStart=change.start2;
/*
	for(size_t n=0; n<diffArray.size(); n++)
	{
		if(change.start2>=diffArray[n].origStart)
			change.start2+=diffArray[n].size;
		if(change.end2>=diffArray[n].origStart)
			change.end2+=diffArray[n].size;
	}
*/
	transform_line_number(left,right,change.start1);
	if(change.end1)
		transform_line_number(left,right,change.end1);
	transform_line_number(left,right,change.start2);
	if(change.end2)
		transform_line_number(left,right,change.end2);
/*
	for(size_t n=0; n<diffArray.size(); n++)
	{
		if(change.start2>=diffArray[n].start2)
			change.start2-=diffArray[n].size;
		if(change.end2>=diffArray[n].start2)
			change.end2-=diffArray[n].size;
	}
*/
	return true;
}

static bool transform_diff(CXmlNodePtr left, CXmlNodePtr right, const char *diff_in, std::vector<diffStruct_t>& diffArray)
{
	CFileAccess in;
	CFileAccess out;
	size_t n;

	if(!in.open(diff_in,"r"))
		return false;

	cvs::string line;

	while(in.getline(line))
	{
		diffStruct_t change;
		transform_context(left,right,line.c_str(),diffArray,change);
		switch(change.type)
		{
		case 'd':
			n=0;
			in.getline(line);
			if(change.end1)
				for(; n<(change.end1-change.start1) && in.getline(line); n++)
					;
			change.size=-(int)(n+1);
			break;
		case 'a':
			n=0;
			in.getline(line);
			if(change.end2)
				for(; n<(change.end2-change.start2) && in.getline(line); n++)
					;
			change.size=n+1;
			break;
		case 'c':
			n=0;
			in.getline(line);
			if(change.end1)
				for(; n<(change.end1-change.start1) && in.getline(line); n++)
					;
			change.size=-(int)(n+1);
			in.getline(line); // Separator
			n=0;
			in.getline(line);
			if(change.end2)
				for(; n<(change.end2-change.start2) && in.getline(line); n++)
					;
			change.size+=n+1;
			break;
		default:
			xdiff_print("Unknown change type '%c'?",change.type);
			break;
		}
		diffArray.push_back(change);
	}
	return true;
}

static int init(const struct plugin_interface *plugin);
static int destroy(const struct plugin_interface *plugin);
static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param);

static xdiff_interface xml_xdiff =
{
	{
		PLUGIN_INTERFACE_VERSION,
		"XML XDiff handler",CVSNT_PRODUCTVERSION_STRING,"XmlXDiff",
		init,
		destroy,
		get_interface,
		NULL /* Configure */
	},
	xdiff_function
};

static int init(const struct plugin_interface *plugin)
{
	return 0;
}

static int destroy(const struct plugin_interface *plugin)
{
	return 0;
}

static void *get_interface(const struct plugin_interface *plugin, unsigned interface_type, void *param)
{
	if(interface_type!=pitXdiff)
		return NULL;

	return (void*)&xml_xdiff;
}

plugin_interface *get_plugin_interface()
{
	return (plugin_interface*)&xml_xdiff;
}

