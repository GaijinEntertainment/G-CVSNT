/*
 * Copyright (c) 2004, Tony Hoyle
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Manage logical->physical directory mapping
 * 
 */

#include "cvs.h"
#include <getline.h>

static int modules2_loaded;
static modules2_struct *modules2_list;
static int modules2_count;
static int global_lookupid;

typedef struct
{
	const char *from;
	const char *to;
} rename_script_entry;

typedef struct
{
	RCSNode *repository_rcsfile;
	const char *repository_map_tag;
	const char *repository_map_date;
	char *repository_buffer;
	List *directory_mappings;
	rename_script_entry *rename_script;
	int rename_script_count,rename_script_size;
	char *virtual_repos;
	char *real_repos;
	const char *directory_version;
} directory_data;

static directory_data *directory_stack=NULL, *current_directory=NULL;
static int directory_stack_size=0, directory_stack_rubbish=0, directory_stack_count=0;

static int modules2_struct_sort(const void *_a, const void *_b)
{
	modules2_struct *a = (modules2_struct*)_a;
	modules2_struct *b = (modules2_struct*)_b;
	TRACE(4,"modules2_struct_sort(%s,%s)",PATCH_NULL(a->name),PATCH_NULL(b->name));
	return fncmp(a->name,b->name);
}

static int modules2_module_struct_sort(const void *_a, const void *_b)
{
	modules2_module_struct *a = (modules2_module_struct*)_a;
	modules2_module_struct *b = (modules2_module_struct*)_b;
	return fncmp(a->directory[0]=='*'?&a->directory[1]:a->directory,b->directory[0]=='*'?&b->directory[1]:b->directory);
}

int free_modules2()
{
	int m,n;

	if(modules2_loaded)
	{
		for(m=0; m<modules2_count; m++)
		{
			for(n=0; n<modules2_list[m].module_count; n++)
			{
				xfree(modules2_list[m].module[n].directory);
				xfree(modules2_list[m].module[n].translation);
				xfree(modules2_list[m].module[n].regex);
			}
			xfree(modules2_list[m].name);
			xfree(modules2_list[m].module);
		}
		xfree(modules2_list);
		modules2_count=0;
		modules2_loaded=0;
	}
	for(n=0; n<directory_stack_size; n++)
	{
		freercsnode(&directory_stack[n].repository_rcsfile);
		xfree(directory_stack[n].repository_map_tag);
		xfree(directory_stack[n].repository_map_date);
		xfree(directory_stack[n].repository_buffer);
		dellist(&directory_stack[n].directory_mappings);
		xfree(directory_stack[n].virtual_repos);
		xfree(directory_stack[n].real_repos);
		xfree(directory_stack[n].directory_version);
		for(m=0; m<directory_stack[n].rename_script_count; m++)
		{
			xfree(directory_stack[n].rename_script[m].from);
			xfree(directory_stack[n].rename_script[m].to);
		}
		xfree(directory_stack[n].rename_script);
	}
	xfree(directory_stack);
	directory_stack_size=0;
	directory_stack_rubbish=0;
	TRACE(3,"free_modules2() directory_stack_size and rubbish set to zero");
	return 0;
}

static void load_modules2()
{
	char *path = (char*)xmalloc(strlen(current_parsed_root->directory)+sizeof(CVSROOTADM)+sizeof(CVSROOTADM_MODULES2)+10);
	FILE *f;
	int n;
	modules2_struct *inmodule;
	modules2_module_struct *line;
	int size = 0,module_size;
	char buf[MAX_PATH*3];
	if(!path)
		error(1,0,"Out of memory");
	sprintf(path,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSROOTADM_MODULES2);

#ifdef _WIN32
	assert(!current_parsed_root->isremote);
#endif
	TRACE(3,"Loading modules2 from %s",PATCH_NULL(path));
	if((f=fopen(path,"r"))==NULL)
	{
		if(!existence_error(errno))
			error(1,errno,"Couldn't read modules2 file");

		xfree(path);
		modules2_loaded = 1;
		modules2_list = NULL;
		return;
	}
	xfree(path);

	inmodule=NULL;
	modules2_count = 0;
	modules2_list = NULL;
	while(fgets(buf,sizeof(buf), f))
	{
		char *p,*q,*dest;
		int inquote;
		p=buf+strlen(buf)-1;
		while(p>=buf && isspace(*p))
			--p;
		*(p+1)='\0';
		p=buf;
		while(isspace(*p))
			p++;
		if(*p=='#' || !*p)
			continue;
		if(*p=='[') /* New module */
		{
			/* We don't allow [] [r [foo]bar] */
			if(*(p+1) == ']' || strchr(p,']')!=p+strlen(p)-1)
			{
				error(0,0,"Malformed line '%s' in modules2 file",p);
				continue;
			}
			p++;
			*(p+strlen(p)-1)='\0';
			if(modules2_count>=size)
			{
				if(!size) size=8;
				else size*=2;
				modules2_list = (modules2_struct *)xrealloc(modules2_list,sizeof(modules2_struct)*size);
				if(!modules2_list)
					error(1,0,"Out of memory");
			}
			inmodule=modules2_list+modules2_count;
			modules2_count++;
			inmodule->module=NULL;
			inmodule->module_count=0;
			inmodule->name=xstrdup(p);
			module_size = 0;
		}
		else
		{
			/* No [...] seen yet,  */
			if(!inmodule) 
			{
				error(0,0,"Malformed line '%s' in modules2 file",p);
				continue;
			}
			if(inmodule->module_count>=module_size)
			{
				if(!module_size) module_size=8;
				else module_size*=2;
				inmodule->module = (modules2_module_struct *)xrealloc(inmodule->module,sizeof(modules2_module_struct)*module_size);
				if(!inmodule->module)
					error(1,0,"Out of memory");
			}
			line = inmodule->module + inmodule->module_count;
			inmodule->module_count++;
			q=p;
			inquote=0;
			line->directory=(char*)xmalloc(strlen(p)+1);
			line->translation=(char*)xmalloc(strlen(p)+1);
			line->regex=NULL;
			dest=(char*)line->directory;
			while(*q && (inquote || (!isspace(*q) && *q!='=')))
			{
				if(inquote=='\\')
				{
					*(dest++)=*q;
					inquote=0;
				}
				else if(*q==inquote)
					inquote=0;
				else if(*q=='"' || *q=='\'')
					inquote=*q;
				else 
					*(dest++)=*q;
				q++;
			}
			*dest='\0';
			if(!strcmp(line->directory,"/"))
				*(char*)line->directory='\0';
			inquote=0;
			while(*q && (isspace(*q) || *q=='='))
			{
				if(*q=='=') inquote='=';
				q++;
			}
			line->local=line->no_recursion=line->lookupid=0;
			if(!inquote)
				strcpy((char*)line->translation,line->directory);
			else
			{
				inquote=0;
				dest=(char*)line->translation;
				while(*q=='!' || *q=='+')
				{
					if(*q=='!')
						line->local=1;
					if(*q=='+')
						line->no_recursion=1;
					q++;
				}

				while(*q)
				{
					if(inquote=='\\')
					{
						*(dest++)=*q;
						inquote=0;
					}
					else if(*q==inquote)
						inquote=0;
					else if(*q=='"' || *q=='\'')
						inquote=*q;
					else 
						*(dest++)=*q;
					q++;
				}
				*dest++='\0';
			}

			if(strlen(line->translation)>1 && line->translation[(strlen(line->translation)-1)==')'])
			{
				int brackets = 1;
				p=(char*)line->translation+strlen(line->translation)-2;
				for(;p>line->translation && brackets;--p)
				{
					if(*p==')')
						brackets++;
					if(*p=='(')
						brackets--;
				}
				if(p>line->translation)
				{
					q=p;
					p++;
					while(isspace(*q))
						*(q--)='\0';
					*p='\0';
					p++;
					if(strlen(p)>1)
					{
						*(p+strlen(p)-1)='\0';
						line->regex=xstrdup(p);
					}
				}
			}

			if(strstr(line->translation,"..") || isabsolute(line->translation))
			{
				error(0,0,"Ignoring invalid module translation '%s'",line->translation);
				inmodule->module_count--;
			}
		}
	}
	fclose(f);
	qsort(modules2_list,modules2_count,sizeof(modules2_struct),modules2_struct_sort);
	for(n=0; n<modules2_count; n++)
		qsort(modules2_list[n].module,modules2_list[n].module_count,sizeof(modules2_module_struct),modules2_module_struct_sort);
	modules2_loaded = 1;
}

static modules2_struct *lookup_repository_module(const char *module)
{
	TRACE(3,"lookup_repository_module(%s)",PATCH_NULL(module));
	modules2_struct term = {0};
	term.name=module;
	if(!modules2_list)
		return NULL;
	return (modules2_struct *)bsearch(&term,modules2_list,modules2_count,sizeof(modules2_struct),modules2_struct_sort);
}

static modules2_module_struct *lookup_repository_directory(modules2_struct *module, const char *directory, int *partial_match, int *subdir_match)
{
	modules2_module_struct *dir, *longest_match;
	int l,n;

	modules2_module_struct term = {0};
	term.directory=directory;
	dir = (modules2_module_struct *)bsearch(&term,module->module,module->module_count,sizeof(modules2_module_struct),modules2_module_struct_sort);
	if(dir)
	{
		*partial_match=*subdir_match=0;
		return dir; /* Perfect match */
	}

	if(!directory[0])
	{
		*partial_match = 1;
		*subdir_match = 0;
		return NULL;
	}

	dir = module->module;
	l=strlen(directory);
	longest_match = NULL;
	*subdir_match=0;
	for(n=0; n<module->module_count; n++, dir++)
	{
		if(!strncmp(directory,dir->directory,strlen(dir->directory)) && directory[strlen(dir->directory)]=='/')
		{
			if(!longest_match || strlen(longest_match->directory)>strlen(dir->directory))
				longest_match = dir;
		}
		if(!strncmp(directory,dir->directory,strlen(directory)) && dir->directory[strlen(directory)]=='/')
			*subdir_match = 1;
	}
	if(longest_match) { *partial_match=strlen(longest_match->directory); *subdir_match = 0; }
	else *partial_match = 0;
	return longest_match;
}

static int _lookup_module2(const char *file, char *left, char *right, int lookupid, modules2_struct **pmod, modules2_module_struct **pdir)
{
	char tmp[MAX_PATH],*p;
	modules2_struct *mod;
	modules2_module_struct *dir;
	int partial_match=0,subdir_match=0;
	int l;

	/* Search the modules2 file if required */
	if(!modules2_loaded)
		load_modules2();

	if(pmod) *pmod=NULL;
	if(pdir) *pdir=NULL;

	TRACE(3,"_lookup_module2(%s,%d)",PATCH_NULL(file),lookupid);

	/* Special case.. ls does this */
	if(!*file)
		return 0;

	p=(char*)strchr(file,'/');
	if(!p)
	{
		TRACE(3,"_lookup_module2 !p lookup_repository_module(%s)",PATCH_NULL(file));
		mod = lookup_repository_module(file);
	}
	else
	{
		strncpy(tmp,file,p-file);
		tmp[p-file]='\0';
		TRACE(3,"_lookup_module2 p!=NULL lookup_repository_module(%s)",PATCH_NULL(tmp));
		mod = lookup_repository_module(tmp);
	}

	if(!mod)
	{
		TRACE(3,"lookup_module2() calls to lookup_repository_module() returned nothing");
		if(left) left[0]='\0';
		if(right) strcpy(right,file);
		TRACE(3,"_lookup_module2 !mod return 0 left,right(%s,%s)",PATCH_NULL(left),PATCH_NULL(right));
		return 0;
	}

	if(pmod) *pmod=mod;

	if(p)
		strcpy(tmp,p+1);
	else
		tmp[0]='\0';

	do
	{
		TRACE(3,"_lookup_module2 lookup_repository_directory(%s,%d,%d)",PATCH_NULL(tmp),partial_match, subdir_match);
		dir = lookup_repository_directory(mod, tmp, &partial_match, &subdir_match);
		TRACE(3,"_lookup_module2 lookup_repository_directory results \"%s\",%d,%d",PATCH_NULL(tmp),partial_match, subdir_match);
		if(dir)
		{
			TRACE(3,"lookup_module2() call to lookup_repository_directory() returned something");
			if(dir->lookupid == lookupid)
				error(1,0,"Recursive repository definition for %s",fn_root(file));
			dir->lookupid = lookupid;
		}
		if(!dir && partial_match)
		{
			if(left)
				sprintf(left,"%s%s%s",mod->name,tmp[0]?"/":"",tmp);
			TRACE(3,"lookup_module2() partial match left is %s",PATCH_NULL(left));
			if(right)
			{
				l=strlen(tmp);
				strcpy(right,file+strlen(mod->name)+(l?l+1:0));
			}
			TRACE(3,"lookup_module2() partial match right is %s",PATCH_NULL(right));
			return 3;
		}
		if(subdir_match)
		{
			if(left)
				sprintf(left,"%s%s%s",mod->name,tmp[0]?"/":"",tmp);
			TRACE(3,"lookup_module2() subdir match left is %s",PATCH_NULL(left));
			if(right)
			{
				l=strlen(tmp);
				strcpy(right,file+strlen(mod->name)+(l?l+1:0));
			} 
			TRACE(3,"lookup_module2() subdir match right is %s",PATCH_NULL(right));
			return 4;
		}
		if(dir)
			break;
		l=strlen(tmp);
		p=strrchr(tmp,'/');
		if(!p) tmp[0]='\0';
		else *p='\0';
	} while(l);

	if(!dir)
	{
		TRACE(3,"_lookup_module2 return 0 !dir left,right(%s,%s)",PATCH_NULL(left),PATCH_NULL(right));
		return 0;
	}
	if(pdir)
		*pdir = dir;

	if(partial_match)
		l=partial_match;
	else
		l=strlen(tmp);

	if(left)
		strcpy(left,dir->translation);
	if(right)
		strcpy(right,file+strlen(mod->name)+(l?l+1:0));
	TRACE(3,"lookup_module2() left is %s",PATCH_NULL(left));
	TRACE(3,"lookup_module2() right is %s",PATCH_NULL(right));
	return dir->translation[0]?1:2;
}

static int lookup_module2(const char *file, char *left, char *right, modules2_struct **pmod, modules2_module_struct **pdir)
{
	int ret,found=0,renamed=0;
	char tmp[MAX_PATH],saved_left[MAX_PATH],saved_right[MAX_PATH];
	modules2_struct *saved_mod;
	modules2_module_struct *saved_dir, *mydir;
	int lookupid = ++global_lookupid;

	TRACE(3,"lookup_module2(%s)",PATCH_NULL(file));
	if(current_directory && current_directory->virtual_repos)
	{
		TRACE(3,"lookup_module2() check between file \"%s\" and virtual_repos \"%s\"",PATCH_NULL(file),PATCH_NULL(current_directory->virtual_repos));
		if(!fncmp(file,current_directory->virtual_repos))
		{
			file = current_directory->real_repos;
			renamed = 1;
		}
		else 
		{
		TRACE(3,"lookup_module2() check between virtual_repos length and file[%d]",strlen(current_directory->virtual_repos));
		if(!fnncmp(file,current_directory->virtual_repos,strlen(current_directory->virtual_repos) && file[strlen(current_directory->virtual_repos)]=='/'))
		{
			sprintf(tmp,"%s%s",current_directory->real_repos,file+strlen(current_directory->virtual_repos));
			file = tmp;
			renamed = 1;
		}
		}
		if (renamed)
			TRACE(3,"lookup_module2() Already determined that the file is renamed");
	}
		
	/* Replay the rename script backwards - rename scripts hold logical filenames in the users' sandbox */
	if(current_directory && current_directory->rename_script)
	{
		int n;

		TRACE(3,"lookup_module2 - Replay the rename script backwards - rename scripts hold logical filenames in the users' sandbox ");
		for(n=current_directory->rename_script_count-1; n>=0 && file; --n)
		{
			TRACE(3,"lookup_module2(%s,%s)",PATCH_NULL(current_directory->rename_script[n].to),PATCH_NULL(current_directory->rename_script[n].from));
			if(current_directory->rename_script[n].to && !fncmp(current_directory->rename_script[n].to,file))
			{
				file = current_directory->rename_script[n].from;
				renamed = 1;
			}
			else 
			{
			TRACE(3,"lookup_module2() check between file \"%s\" and rename_script[%d].from \"%s\"",PATCH_NULL(file),n,PATCH_NULL(current_directory->rename_script[n].from));
			if(!fncmp(current_directory->rename_script[n].from,file))
			{
				file = NULL;
				renamed = 1;
			}
			}
		}
		if (renamed)
			TRACE(3,"lookup_module2() Now determined that the file is renamed");
	}

	TRACE(3,"lookup_module2(%s) after rename?",PATCH_NULL(file));
	if(!file)
		return 2; /* Deleted */

	strcpy(tmp,file);
	do
	{
		TRACE(3,"lookup_module2() call _lookup_module2()");
		ret = _lookup_module2(tmp,left,right,lookupid,pmod,&mydir);
		TRACE(3,"lookup_module2() call _lookup_module2 returned %d",ret);
		if(pdir) *pdir=mydir;
		if(ret!=1 || (mydir && mydir->no_recursion)) 
			TRACE(3,"lookup_module2() no recursion permitted so give up now",ret);
		if(ret!=1 || (mydir && mydir->no_recursion)) break;
		if(left) strcpy(saved_left,left);
		if(right) strcpy(saved_right,right);
		if(pmod) saved_mod=*pmod;
		if(pdir) saved_dir=*pdir;
		sprintf(tmp,"%s%s",PATCH_NULL(left),PATCH_NULL(right));
		found=1;
		TRACE(3,"lookup_module2() found \"%s\"",PATCH_NULL(tmp));
	} while(ret==1);
	if(found && !ret)
	{
		if(left) strcpy(left,saved_left);
		if(right) strcpy(right,saved_right);
		if(pmod) *pmod=saved_mod;
		if(pdir) *pdir=saved_dir;
		TRACE(3,"lookup_module2() return 1");
		ret=1;
	}

	if(current_directory)
		TRACE(3,"lookup_module2() ret=%d, current_directory%s, current_directory->directory_mappings=%s",
			ret,(current_directory)?"!=NULL":"==NULL",(current_directory->directory_mappings)?"!=NULL":"==NULL");
	else
		TRACE(3,"lookup_module2() ret=%d, current_directory==NULL, current_directory->directory_mappings=!!!!",ret);
	if(ret!=2 && current_directory && current_directory->directory_mappings)
	{
		TRACE(3,"lookup_module2() lookup current_directory->directory_mappings");
		Node *head,*p;
		int may_be_deleted = 0;
		sprintf(tmp,"%s%s",PATCH_NULL(left),PATCH_NULL(right));
		head = current_directory->directory_mappings->list;
		if(head)
		{
			for (p = head->next; p != head; p = p->next)
			{
				if(p->data && !fncmp(p->data,tmp))
				{
					left[0]='\0';
					strcpy(right,p->key);
					TRACE(3,"lookup_module2() file appears to be renamed");
					renamed = 1;
					ret = 1;
					break;
				}
				if(!fncmp(p->key,tmp))
					may_be_deleted = 1;
			}
			if(!renamed && may_be_deleted)
				return 2;
		}
	}

	TRACE(3,"lookup_module2() return ret=%d renamed=%d",ret,renamed);
	return ret?ret:renamed;
}

char *map_repository(const char *repository)
{
	char left[MAX_PATH],right[MAX_PATH];
	int res;
	char *r = NULL;

	if(strlen(repository)>1)
	{
		const char *p=repository+strlen(repository)-2;
		if(!strcmp(p,"/."))
		{
			r=xstrdup(repository);
			r[p-repository]='\0';
			repository=r;
		}
	}
	else if(!*repository || strcmp(repository,"."))
	   repository = "/";

	TRACE(3,"map_repository(%s)",repository);
	if(!strcmp(repository,current_parsed_root->directory))
	{
		if(r) return r;
		else xstrdup(repository);
	}

	res = lookup_module2(relative_repos(repository),left,right, NULL, NULL);
	if(r) TRACE(3,"map_repository - lookup_module2 returned %d ",res);

	if(res==1) 
	{
		TRACE(3,"map_repository - lookup_module2 returned res==1");
		char *ret = (char*)xmalloc(strlen(current_parsed_root->directory)+strlen(left)+strlen(right)+10);
		sprintf(ret,"%s/%s%s",current_parsed_root->directory,PATCH_NULL(left),PATCH_NULL(right));
		xfree(r);
		TRACE(3,"map_repository - return(ret) \"%s\"",PATCH_NULL(ret));
		return ret;
	}

	if(res==2) /* Deleted */
	{
		xfree(r);
		TRACE(3,"map_repository - return(NULL)");
		return NULL;
	}

	if((res==3 || res==4) && !*right)
	{
		TRACE(3,"map_repository - lookup_module2 returned res==1||4 (partial translation)");
		/* Partial translation - use emptydir, or if the file/directory exists, use that */
		char *ret = (char*)xmalloc(strlen(current_parsed_root->directory)+sizeof(CVSROOTADM)+sizeof(CVSNULLREPOS)+strlen(left)+strlen(right)+10);
		sprintf(ret,"%s/%s%s",current_parsed_root->directory,PATCH_NULL(left),PATCH_NULL(right));
		TRACE(3,"map_repository - look for %s",PATCH_NULL(ret));
		if(!isfile(ret))
			sprintf(ret,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSNULLREPOS);
		xfree(r);
		TRACE(3,"map_repository - return(ret) \"%s\"",PATCH_NULL(ret));
		return ret;
	}

	if(r) TRACE(3,"map_repository - return(r) \"%s\"",PATCH_NULL(r));
	else TRACE(3,"map_repository - return(repository) \"%s\"",PATCH_NULL(repository));
	if(r) return r;
	else return xstrdup(repository);
}

char *map_filename(const char *repository, const char *name, const char **repository_out)
{
	char tmp[MAX_PATH];
	char *p;
	
	TRACE(3,"map_filename(%s,%s)",PATCH_NULL(repository),PATCH_NULL(name));

	/* Empty filename, assumed deleted */
	if(!*name)
	{
		TRACE(3,"map_filename - * Empty filename, assumed deleted ");
		if(repository_out) *repository_out=NULL;
		return NULL;
	}
	snprintf(tmp,sizeof(tmp),"%s/%s",PATCH_NULL(repository),PATCH_NULL(name));
	TRACE(3,"map_filename - call map_repository(%s)",PATCH_NULL(tmp));
	*repository_out = map_repository(tmp);
	if(!*repository_out)
		return NULL;
	TRACE(3,"map_filename - map_repository() returns %s",PATCH_NULL(*repository_out));
	p=(char*)strrchr(*repository_out,'/'); /* This cannot fail... */
	*p='\0';
	return xstrdup(p+1);
}

int nonrecursive_module(const char *repository)
{
	modules2_module_struct *dir;
	lookup_module2(relative_repos(repository),NULL,NULL,NULL,&dir);
	TRACE(3,"nonrecursive_module(%s) = %d",PATCH_NULL(repository),dir?dir->local:0);
	return dir?dir->local:0;
}

const char *lookup_regex(const char *repository)
{
	modules2_module_struct *dir;
	lookup_module2(relative_repos(repository),NULL,NULL,NULL,&dir);
	return dir&&dir->regex?xstrdup(dir->regex):NULL;
}

int regex_filename_match(const char *regex, const char *filename)
{
	cvs::wildcard_filename fn(filename);

	bool regex_match = fn.matches_regexp(regex)?1:0;
	TRACE(3,"Match %s to %s = %d",PATCH_NULL(regex),PATCH_NULL(filename),regex_match?1:0);
	return regex_match?1:0;
}

int find_virtual_dirs (const char *repository, List *list)
{
	char tmp[MAX_PATH], *q, *fn;
	static char left[MAX_PATH],right[MAX_PATH];
	modules2_struct *mod;
	modules2_module_struct *dir;
	int n,l;
	
	TRACE(3,"find_virtual_dirs(%s)",PATCH_NULL(repository));

	if(!strcmp(repository,current_parsed_root->directory) || !strcmp(repository+strlen(current_parsed_root->directory),"/"))
	{
		/* Special case... basically just list the modules */
		mod=modules2_list;
		for(n=0; n<modules2_count; n++,mod++)
		{
			Node *node;
			strcpy(tmp,mod->name);
			q=strchr(tmp,'/');
			if(q) *q='\0';
			/* put it in the list */
			node = getnode ();
			node->type = DIRS;
			node->key = xstrdup (tmp);
			if (addnode (list, node) != 0)
				freenode (node);
		}
		return 0;
	}

	if(!lookup_module2(relative_repos(repository),left,right,&mod,NULL))
		return 0;

	/* Special case - virtually renamed physical directory */
	if(!mod)
		return 0;

	/* At this point (I think) everything is normalised so don't have to take into account case, etc. */
	strcat(left,right);
	dir = mod->module;
	fn=left+strlen(mod->name);
	if(*fn) fn++;
	l=strlen(fn);

	for(n=0; n<mod->module_count; n++, dir++)
	{
		if(((!l && *dir->directory) || (l && !strncmp(fn,dir->directory,l) && dir->directory[l]=='/')))
		{
			Node *node;
			int isfile = dir->directory[0]=='*'?1:0;
			strcpy(tmp,dir->directory+isfile+(l?l+1:0));
			q=strchr(tmp,'/');
			if(q) *q='\0';
			else if(isfile) continue; /* Don't store files in dir list */
			if(dir->translation[0])
			{
				/* put it in the list */
				node = getnode ();
				node->type = DIRS;
				node->key = xstrdup (tmp);
				if (addnode (list, node) != 0)
					freenode (node);
			}
			else if(!q) 
			{
				node = findnode_fn(list, tmp);
				if(node)
					delnode(node);
			}
		}
	}
	return 0;
}

char *map_fixed_rename(const char *repos, char *name)
{
	char tmp[MAX_PATH];
	if(current_directory && current_directory->directory_mappings)
	{
		Node *n;

		sprintf(tmp,"%s/%s",relative_repos(repos),PATCH_NULL(name));
		n = findnode_fn(current_directory->directory_mappings, tmp);
		if(n)
		{
			char *q=strrchr(n->data,'/');
			if(!q) q=n->data;
			else q++;
			return q;
		}
	}
	return name;
}

int find_virtual_rcs (const char *repository, List *list)
{
	char tmp[MAX_PATH], *q;
	static char left[MAX_PATH],right[MAX_PATH];
	modules2_struct *mod;
	modules2_module_struct *dir;
	int n,l;
	const char *fn;
	
	TRACE(3,"find_virtual_rcs(%s)",PATCH_NULL(repository));

	if(!lookup_module2(relative_repos(repository),left,right,&mod,NULL))
		return 0;

	/* Special case - a virtually renamed directory won't have any module entry */
	if(!mod)
		return 0;

	/* At this point (I think) everything is normalised so don't have to take into account case, etc. */
	strcat(left,right);
	dir = mod->module;
	fn=left+strlen(mod->name);
	if(*fn) fn++;
	l=strlen(fn);
	for(n=0; n<mod->module_count; n++, dir++)
	{
		if(((!l && *dir->directory) || (l && !strncmp(fn,&dir->directory[1],l) && dir->directory[l]=='/')) && dir->directory[0]=='*')
		{
			Node *node;
			strcpy(tmp,dir->directory+1+(l?l+1:0));
			q=strchr(tmp,'/');
			if(q) continue; /* Not at this level */
			if(dir->translation[0])
			{
				/* put it in the list */
				q=map_fixed_rename(repository,tmp);
				if(q)
				{
					node = getnode ();
					node->type = FILES;
					node->key = xstrdup (q);
					if (addnode (list, node) != 0)
						freenode (node);
				}
			}
			else
			{
				node=findnode_fn(list, tmp);
				if(node)
					delnode(node);
			}
		}
	}
	return 0;
}

int find_rename_rcs (const char *repository, List *list)
{
	const char *file,*repos,*p;
	int n;
	Node *node;
	
	TRACE(3,"find_rename_rcs(%s)",PATCH_NULL(repository));

	if(!current_directory || (!current_directory->rename_script && !current_directory->directory_mappings))
		return 0;

	repos=relative_repos(repository);
	if(current_directory->directory_mappings)
	{
		Node *n;

		n=current_directory->directory_mappings->list->next;
		while(n!=current_directory->directory_mappings->list)
		{
			file=(char*)n->data;
			if(file && *file)
			{
				p=strrchr(file,'/');
				if(!p) p=file;
				else p++;
				node = findnode_fn(list,p);
				if(!node)
				{
					node=getnode();
					node->type = FILES;
					node->key = xstrdup (p);
					if (addnode (list, node) != 0)
						freenode (node);
				}
			}
			n=n->next;
		}
	}

	if(current_directory->rename_script)
	{
		for(n=0; n<current_directory->rename_script_count; n++)
		{
			file=current_directory->rename_script[n].from;
			p=strrchr(file,'/');
			if(!p) p=file;
			else p++;
			node = findnode_fn(list,p);
			if(node)
				delnode(node);

			if(current_directory->rename_script[n].to)
			{
				file=current_directory->rename_script[n].to;
				p=strrchr(file,'/');
				if(!p) p=file;
				else p++;

				node=getnode();
				node->type = FILES;
				node->key = xstrdup (p);
				if (addnode (list, node) != 0)
					freenode (node);
			}
		}
	}

	return 0;
}

int find_rename_dirs(const char *repository, List *list)
{
	const char *file,*repos,*p;
	int n;
	Node *node;
	
	TRACE(3,"find_rename_dirs(%s)",PATCH_NULL(repository));

	if(!current_directory || !current_directory->rename_script)
		return 0;

	repos=relative_repos(repository);
	for(n=0; n<current_directory->rename_script_count; n++)
	{
		file=current_directory->rename_script[n].from;
		p=strrchr(file,'/');
		if(!p) p=file;
		else p++;
		node = findnode_fn(list,p);
		if(node)
		{
			delnode(node);

			if(current_directory->rename_script[n].to)
			{
				file=current_directory->rename_script[n].to;
				p=strrchr(file,'/');
				if(!p) p=file;
				else p++;

				node=getnode();
				node->type = DIRS;
				node->key = xstrdup (p);
				if (addnode (list, node) != 0)
					freenode (node);
			}
		}
	}

	return 0;
}

static void repository_checkoutproc (void *callerdat, const char *buffer, size_t len)
{
	const char *p,*from,*to;
	Node *node;

	if(current_directory->directory_mappings)
		dellist(&current_directory->directory_mappings);

	current_directory->directory_mappings = getlist();
	
	p = buffer;
	while(p-buffer<len && *p)
	{
		from=p;
		to=p=strchr(p,'\n');
		if(!p)
			break;
		to++; p++;
		p=strchr(p,'\n');
		if(!p)
			break;
		p++;
		node = getnode();
		node->type=FILES;
		node->key=(char*)xmalloc(to-from);
		strncpy(node->key,from,(to-from)-1);
		node->key[(to-from)-1]='\0';
		node->data=(char*)xmalloc(p-to);
		strncpy(node->data,to,(p-to)-1);
		node->data[(p-to)-1]='\0';
		if (addnode (current_directory->directory_mappings, node) != 0)
			freenode (node);
	}
}



/* Repository versioning support.
   As each cvs enters each directory, it calls this which determines which
   directory revision is used */
int open_directory(const char *repository, const char *dir, const char *tag, const char *date, int nonbranch, const char *version, int remote)
{
	char *tmp,*pt;
	TRACE(3,"open_directory(%s,%s,%s,%d)",PATCH_NULL(repository),PATCH_NULL(tag),PATCH_NULL(date),remote);
	if (current_directory)
	{
		TRACE(3,"current_directory is already set");
		if ( current_directory->directory_mappings )
		{
			TRACE(3,"current_directory->directory_mappings is already set");
		}
	}

	if ((current_directory)&&(!repository))
	{
		if ( current_directory->directory_mappings )
		{
			/* this is to handle the case where we are trying to checkout a renamed file to stdout
			   it works around the case where open_directory() is called with all NULL params from
			   unroll_files_proc() in recurse.cpp */
			TRACE(3,"open_directory has been passed rubbish - but I've already got something better so I'm ignoring it");
			directory_stack_rubbish++;
			return -1;
		}
	}
	if(directory_stack_size == directory_stack_count)
	{
		directory_stack_count *= 2;
		if(directory_stack_count < 50)
			directory_stack_count = 50;
		directory_stack = (directory_data *)xrealloc(directory_stack,sizeof(directory_data)*directory_stack_count);
		if(!directory_stack)
			error(1,errno,"Out of memory");
		if(current_directory)
			current_directory = directory_stack + directory_stack_size;
	}

	if(!current_directory)
		current_directory = directory_stack;
	else
		current_directory++;
	directory_stack_size++;
	TRACE(3,"open_directory() directory_stack_size increased by one to %d (rubbish %d)",directory_stack_size,directory_stack_rubbish);
	memset(current_directory,0,sizeof(directory_data));

	if(!remote)
	{
		current_directory->repository_rcsfile = RCS_parse(RCSREPOVERSION,repository);
		if(!current_directory->repository_rcsfile)
			TRACE(3,"No mapping file in this directory.");
		else
			TRACE(3,"Opened mapping file %s",PATCH_NULL(current_directory->repository_rcsfile->path));

		if(current_directory->repository_rcsfile)
		{
			int retcode;
			const char *rev = NULL;;
			
			TRACE(3,"Reading mapping file %s version=%s",
						PATCH_NULL(current_directory->repository_rcsfile->path),
						PATCH_NULL(version));
			if(version && numdots(version))
			{
				rev = xstrdup(version);
				TRACE(3,"Mapping file rev=%s",PATCH_NULL(rev));
			}
			else if(date || nonbranch || (version && !strcmp(version,"_H_")))
			{
				TRACE(3,"Mapping file get rev using RCS_getversion tag=%s, date=%s",PATCH_NULL(tag),PATCH_NULL(date));
				rev = RCS_getversion(current_directory->repository_rcsfile,(char*)tag,(char*)date,1,NULL);
				TRACE(3,"Mapping file rev=%s",PATCH_NULL(rev));
			}
			else
				TRACE(3,"Mapping file cannot get rev using tag=%s or date=%s",PATCH_NULL(tag),PATCH_NULL(date));
			if(!rev)
			{

				int userevone=1;


				if ((userevone)&&(tag||date))
				{
					// this seems to happen in Bo's test set - not really sure why
					// - something about it causes nonbranch to be set...
					//if(date || nonbranch || (version && !strcmp(version,"_H_")))
					if(date || nonbranch )
					{
						TRACE(3,"Could not get rev using version or tag tag=%s or date=%s, so try not forcing tag match.",PATCH_NULL(tag),PATCH_NULL(date));
						rev = RCS_getversion(current_directory->repository_rcsfile,(char*)tag,(char*)date,0,NULL);
						TRACE(3,"Mapping file rev=%s",PATCH_NULL(rev));
					}
					if(!rev&&!date)
					{
						TRACE(3,"Could not get rev using version or tag or date, so use 1.1 because option set in server and tag=\"%s\".",PATCH_NULL(tag));
						rev = xstrdup("1.1");
					}
				}
				if(!rev)
				{
					TRACE(3,"Could not get rev using version or tag or date, so use HEAD");
					rev = RCS_head(current_directory->repository_rcsfile);
				}
			}

			retcode = RCS_checkout(current_directory->repository_rcsfile, NULL, (char*)rev, (char*)tag, NULL, NULL, repository_checkoutproc, NULL, NULL);
			if (retcode != 0)
				error (1, 0,
				"failed to check out %s file", RCSREPOVERSION);
			TRACE(3,"open_directory has successfully completed RCS_checkout.");
			current_directory->directory_version = rev;
		}
		TRACE(3,"open_directory copy the tag and date.");
		current_directory->repository_map_tag = xstrdup(tag);
		current_directory->repository_map_date = xstrdup(date);
		TRACE(3,"open_directory copied the tag and date.");
	}

	tmp = (char*)xmalloc(strlen(dir)+strlen(CVSADM_RENAME)+256);
	if(dir && dir[0])
	{
		sprintf(tmp,"%s/",dir);
		pt=tmp+strlen(tmp);
	}
	else
		pt=tmp;
	TRACE(3,"Look for rename script file %s",CVSADM_RENAME);
	strcpy(pt,CVSADM_RENAME);
	if(isfile(tmp))
	{
		FILE *fp;
		size_t n;
		char *from, *to;

		TRACE(3,"Rename script file exists so open it");
		fp = CVS_FOPEN(tmp, "r");
		if(!fp)
			error(1,errno,"Couldn't open %s",CVSADM_RENAME);

		current_directory->rename_script_size = 50;
		current_directory->rename_script = (rename_script_entry *)xmalloc(sizeof(rename_script_entry)*current_directory->rename_script_size);
		from=to=NULL;
		while(getline(&from, &n, fp)>0 && getline(&to, &n, fp)>0)
		{
			from[strlen(from)-1]='\0';
			to[strlen(to)-1]='\0';
			TRACE(3,"Rename script file from: %s",PATCH_NULL(from));
			TRACE(3,"Rename script file to: %s",PATCH_NULL(to));
			if(!to[0])
				xfree(to);
			if(current_directory->rename_script_count==current_directory->rename_script_size)
			{
				current_directory->rename_script_size*=2;
				current_directory->rename_script = (rename_script_entry *)xrealloc(current_directory->rename_script,sizeof(rename_script_entry)*current_directory->rename_script_size);
			}
			current_directory->rename_script[current_directory->rename_script_count].from=from;
			current_directory->rename_script[current_directory->rename_script_count].to=to;
			current_directory->rename_script_count++;
			from=to=NULL;
		}
		TRACE(3,"Rename script file exists so close it");
		xfree(from);
		xfree(to);
		fclose(fp);
	}
	else
		TRACE(3,"Rename script file does not exist");

	strcpy(pt,CVSADM_VIRTREPOS);
    if(isfile(tmp))
	{
		FILE *fp = fopen(tmp,"r");
		size_t len;
		if(!fp)
			error(1,errno,"Couldn't open %s",CVSADM_VIRTREPOS);
		if(getline(&current_directory->virtual_repos,&len,fp)<1)
			error(1,errno,"Couldn't read %s",CVSADM_VIRTREPOS);
		current_directory->virtual_repos[strlen(current_directory->virtual_repos)-1]='\0';
		fclose(fp);

		strcpy(pt,CVSADM_REP);
		fp = fopen(tmp,"r");
		if(!fp)
			error(1,errno,"Couldn't open %s",CVSADM_REP);
		if(getline(&current_directory->real_repos,&len,fp)<1)
			error(1,errno,"Couldn't read %s",CVSADM_REP);
		current_directory->real_repos[strlen(current_directory->real_repos)-1]='\0';
		if(isabsolute(current_directory->real_repos))
			memmove(current_directory->real_repos,relative_repos(current_directory->real_repos),strlen(relative_repos(current_directory->real_repos))+1);
		fclose(fp);
	} 
	xfree(tmp);

	TRACE(3,"directory opened");
			
	return 0;
}

static int directory_commitproc(const char *filename, char **buffer, size_t *buflen, char **displayname)
{
	Node *node;
	Node *p, *head;
	size_t len,count;
	char *buf,*q;

	if(!current_directory->directory_mappings)
		current_directory->directory_mappings = getlist();

	TRACE(3,"directory_commitproc(%s)",PATCH_NULL(filename));
	/* Convert the final rename script into a direct file mapping */
	if(current_directory->rename_script)
	{
		int n;

		for(n=0; n<current_directory->rename_script_count; n++)
		{
			const char *from = current_directory->rename_script[n].from;
			const char *to = current_directory->rename_script[n].to;

		    head = current_directory->directory_mappings->list;
			if(head)
			{
				for (p = head->next; p != head; p = p->next)
				{
					if(p->data && !fncmp(p->data,from))
					{
						from = p->key;
						break;
					}
				}
			}

			if((node=findnode_fn(current_directory->directory_mappings,from))!=NULL)
			{
				if(to && !fncmp(to,from))
					delnode(node);
				else
					node->data = (char*)to;
			}
			else if(!to || fncmp(to,from))
			{
				node=getnode();
				node->key = (char*)from;
				node->data = (char*)to;
				node->type = FILES;
				if (addnode (current_directory->directory_mappings, node) != 0)
					freenode (node);
			}
		}
	    sortlist (current_directory->directory_mappings, fsortcmp);
	}

	head = current_directory->directory_mappings->list;
	len = 0;
	count = 0;
	buf = NULL;
	if(head)
	{
		for (p = head->next; p != head; p = p->next)
		{
			count++;
			len+=strlen(p->key)+1;
			if(p->data)
				len+=strlen(p->data);
			len++;
		}
		buf = (char*)xmalloc(len+20);
		q=buf;
		for (p = head->next; p != head; p = p->next)
		{
			sprintf(q,"%s\n%s\n",p->key,p->data?p->data:"");
			q+=strlen(q);
		}
		assert(q-buf==len);
		*(q++)='\0';
	}

	/* Reset the rename script as it's no longer needed */
	xfree(current_directory->rename_script);
	current_directory->rename_script_count = current_directory->rename_script_size = 0;

	*buffer = buf;
	*buflen = len;
	*displayname=xstrdup("directory update");
	return 0;
}

static int create_mapping_file(const char *repository, const char *message)
{
	TRACE(3,"create_mapping_file current_directory->repository_rcsfile=%d",(!current_directory->repository_rcsfile)?0:1);
	if(!current_directory->repository_rcsfile)
	{
		char *fn = (char*)xmalloc(strlen(repository)+strlen(RCSREPOVERSION)+10);
		TRACE(3,"Creating new directory mapping file");
		sprintf(fn,"%s/%s%s",repository,RCSREPOVERSION,RCSEXT);
		add_rcs_file(message?message:"created",fn,NULL,"1.1",NULL,NULL,NULL,0,NULL,"mapping file",12,NULL,NULL);
		xfree(fn);
		current_directory->repository_rcsfile = RCS_parse(RCSREPOVERSION,repository);
		if(!current_directory->repository_rcsfile)
			error(1,errno,"Couldn't create %s",RCSREPOVERSION);
		else
			TRACE(3,"Created new directory mapping file %s",RCSREPOVERSION);
	}
	return 0;
}
int commit_directory(const char *update_dir, const char *repository, const char *message)
{
	char *newver = NULL;
	const char *rev;

	TRACE(3,"commit_directory current_directory->rename_script=%d",(!current_directory->rename_script)?0:1);

	if(!current_directory->rename_script)
		return 0;

	TRACE(3,"commit_directory(%s,%s,%s)",PATCH_NULL(update_dir),PATCH_NULL(repository),PATCH_NULL(message));
	TRACE(3,"commit_directory current_directory->repository_rcsfile=%d",(!current_directory->repository_rcsfile)?0:1);

	if(!current_directory->repository_rcsfile)
		create_mapping_file(repository,message);

	if(current_directory->repository_map_tag && strcmp(current_directory->repository_map_tag,"HEAD"))
	{
		rev = RCS_whatbranch(current_directory->repository_rcsfile, current_directory->repository_map_tag);
		if(current_directory->repository_map_date || (rev && !RCS_isbranch(current_directory->repository_rcsfile, current_directory->repository_map_tag)))
		{
			if(current_directory->repository_map_date)
				error (0, 0, "cannot commit directory '%s' which has a sticky date of '%s'", "foo", current_directory->repository_map_date);
			else
				error (0, 0, "cannot commit directory '%s' which has a sticky tag of '%s'", "foo", current_directory->repository_map_tag);
			xfree(rev);
			return 1;
		}

		if(!rev)
		{
			char *cp,*ver;
			ver = RCS_getversion(current_directory->repository_rcsfile, current_directory->repository_map_tag, current_directory->repository_map_date, 0,  (int *) NULL);
			rev = RCS_magicrev(current_directory->repository_rcsfile,ver);
			RCS_settag(current_directory->repository_rcsfile,current_directory->repository_map_tag,rev,NULL);
			cp = (char*)strrchr(rev,'.');
			for(--cp;*cp!='.';--cp)
				;
			strcpy(cp,cp+2);
			xfree(ver);
		}
	}
	else
	{
		if(current_directory->repository_map_date)
		{
			error (0, 0, "cannot commit directory '%s' which has a sticky date of '%s'", "foo", current_directory->repository_map_date);
			return 1;
		}
		rev = RCS_head(current_directory->repository_rcsfile);
		*(char*)strchr(rev,'.')='\0';
		
	}

	if(RCS_checkin(current_directory->repository_rcsfile,NULL,message?(char*)message:"no message",rev,NULL,0,NULL,NULL,directory_commitproc, &newver, NULL, NULL))
		error(1,errno,"RCS checkin to mapping file failed");

	xfree(current_directory->directory_version);
	current_directory->directory_version = newver;

	CVS_UNLINK(CVSADM_RENAME);

#ifdef SERVER_SUPPORT
	if(server_active)
		reset_client_mapping(update_dir, repository);
#endif

	return 0;
}

int close_directory()
{
	TRACE(3,"close_directory() directory_stack_size %d, rubbish %d",directory_stack_size,directory_stack_rubbish);

	if(!directory_stack_size)
		error(1,0,"Directory stack overrun -- directory_stack_size is zero");

	if(current_directory->repository_rcsfile)
		freercsnode(&current_directory->repository_rcsfile);
	dellist(&current_directory->directory_mappings);
	xfree(current_directory->rename_script);
	current_directory->rename_script_count = current_directory->rename_script_size = 0;
	xfree(current_directory->directory_version);

	directory_stack_size--;
	TRACE(3,"close_directory() directory_stack_size decreased by one to %d (rubbish %d)",directory_stack_size,directory_stack_rubbish);
	if(!directory_stack_size)
		current_directory = NULL;
	else
		current_directory--;

	return 0;
}

int free_directory()
{
	TRACE(3,"free_directory() directory_stack_size = %d, rubbish = %d",directory_stack_size,directory_stack_rubbish);
	while(directory_stack_size)
		close_directory();
	xfree(directory_stack);
	return 0;
}

int set_mapping(const char *directory, const char *oldfile, const char *newfile)
{
	char *ren = (char*)xmalloc(strlen(directory)+sizeof(CVSADM_RENAME)+20);
	char *rennew = (char*)xmalloc(strlen(directory)+sizeof(CVSADM_RENAME)+20);
	char *of = (char*)xmalloc(MAX_PATH+strlen(oldfile));
	FILE *fpin,*fpout;
	char from[MAX_PATH],to[MAX_PATH];

	strcpy(of,oldfile);
	TRACE(3,"set_mapping(%s,%s,%s)",PATCH_NULL(directory), PATCH_NULL(oldfile), PATCH_NULL(newfile));
	sprintf(ren,"%s%s",directory,CVSADM_RENAME);
	fpin = CVS_FOPEN(ren,"r");
	if(!fpin)
	{
		if(!existence_error(errno))
			error(1,errno,"Couldn't open %s",ren);
	}
	strcpy(rennew,ren);
	strcat(rennew,".New");
	fpout = CVS_FOPEN(rennew,"w");
	if(!fpout)
		error(1,errno,"Couldn't create %s",rennew);

	if(fpin)
	{
		while(fgets(from,sizeof(from),fpin) && fgets(to,sizeof(to),fpin))
		{
			from[strlen(from)-1]='\0';
			to[strlen(to)-1]='\0';
			fprintf(fpout,"%s\n%s\n",from,to);
		}
		fclose(fpin);
	}
	fprintf(fpout,"%s\n%s\n",oldfile,newfile);
	fclose(fpout);
	if(CVS_RENAME(rennew,ren))
		error(1,errno,"Error renaming %s to %s",rennew,ren);
	xfree(ren);
	xfree(rennew);
	xfree(of);
	return 0;
}

int add_mapping(const char *directory, const char *oldfile, const char *newfile)
{
	TRACE(3,"add_mapping(%s,%s,%s)",PATCH_NULL(directory), PATCH_NULL(oldfile), PATCH_NULL(newfile));
	if(current_directory->rename_script_count==current_directory->rename_script_size)
	{
		current_directory->rename_script_size*=2;
		if(current_directory->rename_script_size<50) current_directory->rename_script_size=50;
		current_directory->rename_script = (rename_script_entry *)xrealloc(current_directory->rename_script,sizeof(rename_script_entry)*current_directory->rename_script_size);
		if(!current_directory->rename_script)
			error(1,errno,"Out of memory");
	}
	current_directory->rename_script[current_directory->rename_script_count].from=xstrdup(oldfile);
	current_directory->rename_script[current_directory->rename_script_count].to=xstrdup(newfile);
	current_directory->rename_script_count++;

#ifdef SERVER_SUPPORT
	if(server_active)
		send_rename_to_client(oldfile,newfile);
#endif
	return 0;
}

const char *get_directory_version()
{
	if (current_directory)
		TRACE(3,"get_directory_version() current_directory%sNULL, directory_version=%s",(current_directory)?"!=":"==",PATCH_NULL(current_directory->directory_version));
	else
		TRACE(3,"get_directory_version() current_directory%sNULL",(current_directory)?"!=":"==");
	return (current_directory && current_directory->directory_version)?current_directory->directory_version:NULL;
}

int get_directory_finfo(const char *repository, const char *dir, const char *update_dir, struct file_info *finfo)
{
	assert(!current_parsed_root->isremote);
	assert(current_directory);

 	if(!current_directory->repository_rcsfile)
		return -1;

	memset(finfo,0,sizeof(struct file_info));
	finfo->mapped_file=finfo->file=finfo->fullname=RCSREPOVERSION;
	finfo->rcs=current_directory->repository_rcsfile;
	finfo->update_dir=(char*)update_dir;
	finfo->repository=(char*)repository;
	finfo->entries=getlist();
	Fast_Register(finfo->entries,RCSREPOVERSION,current_directory->directory_version,"","",current_directory->repository_map_tag,current_directory->repository_map_date,"","","",0,"","","", NULL);

	return 0;
}

int upgrade_entries(const char *repository, const char *dir, List **entries, List **renamed_files)
{
	/* This is called with two open directories on the stack (old/new).  We scan
	   through the entries files looking for translations in (old) and making them
	   equal to (new), provided they're in the same directory */
	directory_data *new_directory = current_directory;
	directory_data *old_directory = current_directory-1;
	Node *head, *p, *q;
	List *newlist;
	int modified = 0;
	int client_supports_rename = 1;
	
#ifdef SERVER_SUPPORT
	if(server_active)
		client_supports_rename = client_can_rename();
#endif

	if((!old_directory->directory_version && !new_directory->directory_version) || !strcmp(old_directory->directory_version,new_directory->directory_version))
		return 0;

	TRACE(3,"Old directory version = %s\n",PATCH_NULL(old_directory->directory_version));
	TRACE(3,"New directory version = %s\n",PATCH_NULL(new_directory->directory_version));

    head = (*entries)->list;
	*renamed_files = getlist();
	if(client_supports_rename)
		newlist = getlist();

    for (p = head->next; p != head; p = p->next)
	{
		const char *oldfile;
		char *oldrepos;
		Node *dirhead=new_directory->directory_mappings->list;
		Entnode *ent = (Entnode*)p->data;
		char *name = ent->user;
		Node *newnode;

		TRACE(3,"Remapping file %s\n", PATCH_NULL(name));

		current_directory = old_directory;
		oldfile=map_filename(repository,name,(const char**)&oldrepos);
		current_directory = new_directory;

		if(oldfile)
		{
			oldrepos=(char*)xrealloc(oldrepos,strlen(oldfile)+strlen(oldrepos)+4);
			strcat(oldrepos,"/");
			strcat(oldrepos,oldfile);
			memmove(oldrepos,oldrepos+strlen(current_parsed_root->directory)+1,strlen(oldrepos+strlen(current_parsed_root->directory)+1)+1);
			
			if(client_supports_rename)
			{
				newnode=getnode();
				newnode->key = xstrdup(p->key);
				newnode->data = (char*)ent;
				newnode->delproc = p->delproc;
				p->data = NULL;
				p->delproc = NULL;
			}

			for(q = dirhead->next; q!=dirhead; q = q->next)
			{
				const char *from = (const char *)q->key;
				const char *to = (const char*)q->data;

				if(!strcmp(from,oldrepos))
				{
					Node *nod = getnode();
					if(client_supports_rename)
					{
						if(to && *to)
						{
							char *xfrom, *xto;

							xfree(newnode->key);
							xfree(ent->user);
							newnode->key=xstrdup(last_component(to));
							ent->user=xstrdup(last_component(to));

							xfrom = (char*)xmalloc(strlen(dir)+strlen(from)+10);
							sprintf(xfrom,"%s/%s",dir,last_component(from));
							xto = (char*)xmalloc(strlen(dir)+strlen(to)+10);
							sprintf(xto,"%s/%s",dir,last_component(to));
							if(isfile(xfrom))
								CVS_RENAME(xfrom,xto);
							xfree(xfrom);	
							xfree(xto);
						}
						else
						{
							freenode(newnode);
							newnode=NULL;
						}
						modified = 1; 
					}
					nod->key=xstrdup(last_component(from));
					nod->data=xstrdup(last_component(to));
					addnode(*renamed_files,nod);
					break;
				}
			}
			xfree(oldfile);
			xfree(oldrepos);

			if(client_supports_rename && newnode)
				addnode(newlist,newnode);
		}
	}

	/* If modifed, create an entries.log */
	/* This isn't really the best way to do it, but it's the simplest at the moment */
	if(client_supports_rename && modified)
	{
		char *log = (char*)xmalloc(strlen(dir)+sizeof(CVSADM_ENTLOG)+10);
		FILE *f;
		sprintf(log,"%s/%s",dir,CVSADM_ENTLOG);
		f=fopen(log,"a");
		fclose(f);
	}

	if(client_supports_rename)
	{
		dellist(entries);
		*entries = newlist;
	}
	
	return 0;
}
