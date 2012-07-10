/* Implementation for file attribute munging features.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "getline.h"
#include "fileattr.h"

static const char *stored_repos; /* Current directory */
static CXmlTree g_tree; /* Global xml context */
static CXmlNodePtr stored_root; /* Node tree of current directory */
static int modified; /* Set when tree changed */

static void fileattr_read();

static void fileattr_convert(CXmlNodePtr & root, const char *oldfile);
static void owner_convert(CXmlNodePtr & root, const char *oldfile);
static void perms_convert(CXmlNodePtr & root, const char *oldfile);
static void fileattr_convert_write(CXmlNodePtr & root, const char *xmlfile);
static void fileattr_convert_line(CXmlNodePtr node, char *line);

static char printf_buf[512];

/* Note that if noone calls fileattr_xxx, this is very cheap.  No stat(),
   no open(), no nothing.  */
void fileattr_startdir (const char *repos)
{
    assert (stored_repos == NULL || !fncmp(repos,stored_repos));
	assert (stored_root == NULL);

	stored_repos = xstrdup (repos);
    modified = 0;
	TRACE(3,"fileattr_startdir(%s)",PATCH_NULL(repos));
}

CXmlNodePtr _fileattr_find(CXmlNodePtr node, const char *exp, ...)
{
	va_list va;
	va_start(va, exp);
	vsnprintf(printf_buf,sizeof(printf_buf),exp,va);
	va_end(va);

	CXmlNodePtr n = node->Clone();
	if(!n->Lookup(printf_buf) || !n->XPathResultNext())
		return NULL;
	return n;
}

/* Search for a given node.  A little like xpath but not nearly as complex */
CXmlNodePtr fileattr_find(CXmlNodePtr root, const char *exp, ...)
{
	TRACE(3,"fileattr_find(%s)",exp);
	if(!stored_root)
		fileattr_read();

	CXmlNodePtr node=root;
	if(!node) node = stored_root;

	va_list va;
	va_start(va, exp);
	vsnprintf(printf_buf,sizeof(printf_buf),exp,va);
	va_end(va);

	CXmlNodePtr n = node->Clone();
	if(!n->Lookup(printf_buf) || !n->XPathResultNext())
		return NULL;
	return n;
}

/* Search for a given node.  A little like xpath but not nearly as complex */
/* This version creates any nodes that aren't in the path */
CXmlNodePtr fileattr_getroot()
{
	TRACE(3,"fileattr_getroot()");
	if(!stored_root)
		fileattr_read();

	return stored_root->Clone();
}

/* Return the next node on this level with this name, for walking lists */
CXmlNodePtr fileattr_next(CXmlNodePtr root)
{
	TRACE(3,"fileattr_next()");
	if(!stored_root)
		fileattr_read();

	if(root->GetSibling(root->GetName()))
		return root;
	return NULL;
}

/* Delete a value under the node.  */
void fileattr_delete(CXmlNodePtr root, const char *exp, ...)
{
	TRACE(3,"fileattr_delete(%s)",exp);
	if(!stored_root)
		fileattr_read();

	CXmlNodePtr node=root;
	if(!node) node = stored_root;

	va_list va;
	va_start(va, exp);
	vsnprintf(printf_buf,sizeof(printf_buf),exp,va);
	va_end(va);

	CXmlNodePtr n = node->Clone();
	if(!n->Lookup(printf_buf))
		return;

	while(n->XPathResultNext())
	{
		n->Delete();
		modified = 1;
	}
}

/* Delete a value under the node.  */
void fileattr_delete_child(CXmlNodePtr parent, CXmlNodePtr child)
{
	TRACE(3,"fileattr_delete_child()");

	if(!parent)
		return;

	if(child)
	{
		child->Delete();
		modified = 1;
	}
}

/* Delete a value under the node at the next prune.  */
void fileattr_batch_delete(CXmlNodePtr root)
{
	TRACE(3,"fileattr_batch_delete()");
	if(!stored_root)
		fileattr_read();

	CXmlNodePtr node=root;
	if(!node) node = stored_root;

	node->Delete();
	modified = 1;
}

/* Get a single value from a node.  Pass null to get value of this node. */
const char *fileattr_getvalue(CXmlNodePtr root, const char *name)
{
	TRACE(3,"fileattr_getvalue(%s)",name);
	if(!stored_root)
		fileattr_read();

	CXmlNodePtr node=root;
	if(!node) node = stored_root;

	if(name[0]=='@')
	{
		return node->GetAttrValue(name+1);
	}
	else
	{
		CXmlNodePtr n = node->Clone();
		if(!n->GetChild(name))
			return NULL;
		return n->GetValue();
	}
}

/* Set a single value for a node.  Pass null to set value of this node. */
void fileattr_setvalue(CXmlNodePtr root, const char *name, const char *value)
{
	TRACE(3,"fileattr_setvalue(%s,%s)",name,value?value:"");
	if(!stored_root)
		fileattr_read();

	assert(!name || !strchr(name,'/'));

	CXmlNodePtr node=root;
	CXmlNodePtr val;
	if(!node) node = stored_root;

	modified = 1;

	if(name)
	{
		CXmlNodePtr n = node->Clone();
		if(!n->GetChild(name))
		{
			if(name[0]=='@')
				n->NewAttribute(name+1,value);
			else
				n->NewNode(name,value);
		}
		else
			n->SetValue(value);
	}
	else
		node->SetValue(value);
}

void fileattr_newfile (const char *filename)
{
	TRACE(3,"fileattr_newfile(%s)",filename);
	if(!stored_root)
		fileattr_read();

	CXmlNodePtr dir_default = stored_root->Clone();
	if(dir_default->GetChild("directory"))
	{
		if(!dir_default->GetChild("default"))
			dir_default = NULL;
	}
	else
		dir_default = NULL;

	CXmlNodePtr file = stored_root->Clone();
	file->NewNode("file");
	file->NewAttribute("name",filename);
	modified = 1;

	if(dir_default)
		file->CopySubtree(dir_default);
}

void fileattr_write ()
{
	char *fname;

	TRACE(3,"fileattr_write()");
	if(noexec)
		return;
	if(!modified || !stored_root)
		return;

    fname = (char*)xmalloc (strlen (stored_repos)
		     + sizeof (CVSREP_FILEATTR) + 20);

    sprintf(fname,"%s/%s",stored_repos,CVSADM);
    if(!isdir(fname))
      make_directory(fname);

    sprintf(fname,"%s/%s",stored_repos,CVSREP_FILEATTR);

	if(!g_tree.WriteXmlFile(fname))
		error (0, errno, "cannot write %s", fn_root(fname));
	xfree(fname);

	modified = 0;
	TRACE(3,"fileattr_write() return");
}

void fileattr_free ()
{
	TRACE(3,"fileattr_free()");
	xfree(stored_repos);
	stored_root = NULL;
	g_tree.Close();
}

void fileattr_read()
{
	TRACE(3,"fileattr_read()");
	_fileattr_read(stored_root, stored_repos);
	if(!stored_root)
	{
		TRACE(3,"fileattr_read() Malformed fileattr.xml file.");
		error(1,0,"Malformed fileattr.xml file in %s/CVS.  Please fix or delete this file",fn_root(stored_repos));
	}
	TRACE(3,"fileattr_read() return");
}

void _fileattr_read(CXmlNodePtr& root, const char *repos)
{
	char *fname=NULL,*ofname=NULL;

	TRACE(3,"_fileattr_read(%s)",repos);
	if (repos==NULL)
		TRACE(3,"_fileattr_read() repos is NULL - will crash!");

	if(root)
	{
		TRACE(3,"_fileattr_read() no root, should never happen!!!");
		return; /* Boilerplate, should never happen */
	}

	TRACE(3,"_fileattr_read() malloc [strlen(%s)=]%d+[sizeof(%s)=]%d+20=%d",repos,(int)strlen(repos),CVSREP_FILEATTR,(int)sizeof (CVSREP_FILEATTR),(int)(strlen (repos)+ sizeof (CVSREP_FILEATTR) + 20));
    fname = (char*)xmalloc ((int)(strlen (repos)
		     + sizeof (CVSREP_FILEATTR) + 20));

	if (fname==NULL)
		TRACE(3,"_fileattr_read() failed to allocate memory for fname - will now crash!");
	else
		TRACE(3,"_fileattr_read() allocated memory for fname - will now sprintf()");

	sprintf(fname,"%s/%s",repos,CVSREP_FILEATTR);

	if(!isfile(fname))
	{
		TRACE(3,"_fileattr_read() no file \"%s\", so allocate ofname of %d bytes.",fname,(int)(strlen (repos)+ sizeof (CVSREP_OLDFILEATTR) + 20));
		ofname = (char*)xmalloc ((int)(strlen (repos)
				+ sizeof (CVSREP_OLDFILEATTR) + 20));
		if (ofname==NULL)
			TRACE(3,"_fileattr_read() failed to allocate memory for ofname - will now crash!");
		sprintf(ofname,"%s/%s",repos,CVSREP_OLDFILEATTR);
		if(isfile(ofname))
		{
			TRACE(3,"_fileattr_read() found old \"%s\".",ofname);
			fileattr_convert(root,ofname);
			CVS_UNLINK(ofname);
		}
		else
			TRACE(3,"_fileattr_read() no old \"%s\".",ofname);

		sprintf(ofname,"%s/%s",repos,CVSREP_OLDOWNER);
		if(isfile(ofname))
		{
			TRACE(3,"_fileattr_read() found old \"%s\".",ofname);
			owner_convert(root,ofname);
			CVS_UNLINK(ofname);
		}
		else
			TRACE(3,"_fileattr_read() no old \"%s\".",ofname);

		sprintf(ofname,"%s/%s",repos,CVSREP_OLDPERMS);
		if(isfile(ofname))
		{
			TRACE(3,"_fileattr_read() found old \"%s\".",ofname);
			perms_convert(root,ofname);
			CVS_UNLINK(ofname);
		}
		else
			TRACE(3,"_fileattr_read() no old \"%s\".",ofname);

		if(root)
		{
			TRACE(3,"_fileattr_read() look for directory(\"%s\")",ofname);
			sprintf(ofname,"%s/%s",repos,CVSADM);
			if(!isdir(ofname))
				make_directory(ofname);
			TRACE(3,"_fileattr_read() fileattr_convert_write(\"%s\")",fname);
			fileattr_convert_write(root,fname);
		}
		else
		{
			TRACE(3,"_fileattr_read() CreateNewTree(\"fileattr\")");
			g_tree.CreateNewTree("fileattr");
			TRACE(3,"_fileattr_read() GetRoot()");
			root = g_tree.GetRoot();
		}

		TRACE(3,"_fileattr_read() free filenames");
		xfree(ofname);
		xfree(fname);
		return;
	}

	TRACE(3,"_fileattr_read() ReadXmlFile(\"%s\")",fname);
	g_tree.ReadXmlFile(fname);
	TRACE(3,"_fileattr_read() get root");
	root = g_tree.GetRoot();
	TRACE(3,"_fileattr_read() free fname");
	xfree(fname);
	TRACE(3,"_fileattr_read() done");
}

/* Read an old-style fileattr file and convert it to new-style */
void fileattr_convert(CXmlNodePtr& root, const char *oldfile)
{
	FILE *fp;
	char *line, *p;
	size_t linesize;
	CXmlNodePtr node;

	TRACE(3,"fileattr_convert(%s)",oldfile);	
    fp = CVS_FOPEN (oldfile, FOPEN_BINARY_READ);
    if (fp == NULL)
    {
		if (!existence_error (errno))
			error (0, errno, "cannot read %s", fn_root(oldfile));
		return;
    }

	if(!root)
	{
		g_tree.CreateNewTree("fileattr");
		root = g_tree.GetRoot();
	}
	line = NULL;

	while(getline(&line,&linesize,fp)>=0)
	{
		line[linesize-1]='\0';
		if(!*line)
			continue;

		p=line+strlen(line)-1;
		while(isspace(*p))
			*(p--)='\0';

	    p = strchr (line, '\t');
	    if (p == NULL)
			error (1, 0, "file attribute database corruption: tab missing in %s",fn_root(oldfile));
	    *(p++)='\0';

		switch(line[0])
		{
		case 'D':
			if(!root->GetChild("directory")) root->NewNode("directory");
			if(!root->GetChild("default")) root->NewNode("default");
			fileattr_convert_line(root,p);
			root->GetParent();
			root->GetParent();
			break;
		case 'F':
			root->NewNode("file");
			root->NewAttribute("name",line+1);
			fileattr_convert_line(root,p);
			root->GetParent();
			break;
		default:
			error(0,0,"Unrecognized fileattr type '%c'.  Not converting.",line[0]);
			break;
		}
	}
	
	fclose(fp);
	xfree(line);
}

void owner_convert(CXmlNodePtr& root, const char *oldfile)
{
	FILE *fp;
	char *line, *p;
	size_t linesize;

	TRACE(3,"owner_convert(%s)",oldfile);	
    fp = CVS_FOPEN (oldfile, FOPEN_BINARY_READ);
    if (fp == NULL)
    {
		if (!existence_error (errno))
			error (0, errno, "cannot read %s", fn_root(oldfile));
		return;
    }

	if(!root)
	{
		g_tree.CreateNewTree("fileattr");
		root = g_tree.GetRoot();
	}

	line = NULL;
	if(getline(&line,&linesize,fp)>=0)
	{
		line[linesize-1]='\0';
		if(*line)
		{
			p=line+strlen(line)-1;
			while(isspace(*p))
				*(p--)='\0';

			if(!root->GetChild("directory")) root->NewNode("directory");
			if(!root->GetChild("owner")) root->NewNode("owner");
			root->SetValue(line);
			root->GetParent();
			root->GetParent();
		}
	}
	fclose(fp);
	xfree(line);
}

void perms_convert(CXmlNodePtr & root, const char *oldfile)
{
	FILE *fp;
	char *line, *p;
	size_t linesize;

	TRACE(3,"perms_convert(%s)",oldfile);	
    fp = CVS_FOPEN (oldfile, FOPEN_BINARY_READ);
    if (fp == NULL)
    {
		if (!existence_error (errno))
			error (0, errno, "cannot read %s", fn_root(oldfile));
		return;
    }

	if(!root)
	{
		g_tree.CreateNewTree("fileattr");
		root = g_tree.GetRoot();
	}

	line = NULL;
	while(getline(&line,&linesize,fp)>=0)
	{
		const char *user, *branch=NULL, *perms;
		line[linesize-1]='\0';
		if(!*line)
			continue;

		p=line+strlen(line)-1;
		while(isspace(*p))
			*(p--)='\0';

		p = strchr(line,'{');
		if(p)
		{
			*(p++)='\0';
			branch = p;
			p=strchr(p,'}');
			if(!p)
			{
				error(0,0,"malformed ACL for user %s in directory",user);
				continue;
			}
			*(p++)='\0';
		}
		else
			p=line;
		user = p;
		p=strchr(p,':');
		if(!p)
		{
			error(0,0,"malformed ACL for user %s in directory",user);
			continue;
		}
		*(p++)='\0';
		perms = p;

		if(!root->GetChild("directory")) root->NewNode("directory");
		
		root->NewNode("acl",NULL);
		if(strcmp(user,"default"))
			root->NewAttribute("user",user);
		if(branch)
			root->NewAttribute("branch",branch);
		if(strchr(perms,'n'))
		{
			root->NewNode("all");
			root->NewAttribute("deny","1");
			root->GetParent();
		}
		else
		{
			if(strchr(perms,'r'))
				root->NewNode("read",NULL,false);
			if(strchr(perms,'w'))
			{
				root->NewNode("write",NULL,false);
				root->NewNode("tag",NULL,false);
			}
			if(strchr(perms,'c'))
				root->NewNode("create",NULL,false);
		}
		root->GetParent();
		root->GetParent();
	}
	fclose(fp);
	xfree(line);
}

void fileattr_convert_write(CXmlNodePtr& root, const char *xmlfile)
{
	if(!g_tree.WriteXmlFile(xmlfile))
	{
		error (0, errno, "cannot write %s", fn_root(xmlfile));
		return;
	}
}

void fileattr_convert_line(CXmlNodePtr node, char *line)
{
	char *l = line, *e, *p, *type,*name,*next, *att, *nextatt;
	CXmlNodePtr subnode;
	do
	{
		p = strchr(l,'=');
		if(!p)
			return;
		*(p++)='\0';
		e=strchr(p,';');
		if(e)
			*(e++)='\0';

		if(!strcmp(l,"_watched"))
		{
			node->NewNode("watched",NULL,false);
		}
		else if(!strcmp(l,"_watchers"))
		{
			name=p;
			do
			{
				type=strchr(name,'>');
				if(!type)
					 break;
				*(type++)='\0';
				next=strchr(type,',');
				if(next)
					*(next++)='\0';

				node->NewNode("watcher",NULL);
				node->NewAttribute("name",name);
				att=type;
				do
				{
					nextatt=strchr(att,'+');
					if(nextatt)
						*(nextatt++)='\0';
					node->NewNode(att,NULL,false);
					att=nextatt;
				} while(att);
				name = next;
				node->GetParent();
			} while(name);
		}
		else if(!strcmp(l,"_editors"))
		{
			name=p;
			do
			{
				type=strchr(name,'>');
				if(!type)
					 break;
				*(type++)='\0';
				next=strchr(type,',');
				if(next)
					*(next++)='\0';

				node->NewNode("editor",NULL);
				node->NewAttribute("name",name);
				att=type;

				nextatt=strchr(att,'+');
				if(nextatt)
					*(nextatt++)='\0';
				node->NewNode("time",att,false);

				att=nextatt;
				nextatt=strchr(att,'+');
				if(nextatt)
					*(nextatt++)='\0';
				node->NewNode("hostname",att,false);

				att=nextatt;
				nextatt=strchr(att,'+');
				if(nextatt)
					*(nextatt++)='\0';
				node->NewNode("pathname",att,false);

				node->GetParent();
				att=nextatt;
				name = next;
			} while(name);
		}
		else
		{
			error(0,0,"Unknown fileattr attribute '%s'.  Not converting.",l);
		}
		l=e;
	} while(l);
}

void fileattr_prune(CXmlNodePtr node)
{
	if(!node)
		return;

//	node->Prune();
}

CXmlNodePtr fileattr_copy(CXmlNodePtr root)
{
	if(!root)
		return NULL;
	
	return root->DuplicateNode();
}

void fileattr_paste(CXmlNodePtr root, CXmlNodePtr source)
{
	if(!source)
		return;

	if(!stored_root)
		fileattr_read();

	CXmlNodePtr node=root;
	if(!node) node = stored_root;

	node->CopySubtree(source);
	modified = 1;
}


void fileattr_free_subtree(CXmlNodePtr& root)
{
	root=NULL;
}

void fileattr_modified()
{
	modified = 1;
}
