/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * Copyright (c) 2004, Tony Hoyle
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Rename a file
 */

#include "cvs.h"
#include "getline.h"

static const char *const rename_usage[] =
{
    "Usage: %s %s [-q] <source> <target>\n",
	"\t-q\tQuieter output.\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

static int validate_file(const char *filename, char **root, char **repository, const char **file, const char **returned_dir, int must_exist)
{
	char *dir = (char*)xmalloc(strlen(filename)+sizeof(CVSADM_REP)+sizeof(CVSADM_ROOT)+1),*p;
	size_t repos_len;
	FILE *fp;

	strcpy(dir,filename);
	p=(char*)last_component(dir);
	*file=last_component((char*)filename);
	*p='\0';

	strcat(dir,CVSADM);
	if(!isdir(dir))
		error(1,0,"%s is not part of a checked out repository",filename);
	*p='\0';
	strcat(dir,CVSADM_REP);
	if(!isfile(dir) || isdir(dir))
		error(1,0,"%s is not part of a checked out repository",filename);
	fp = fopen(dir,"r");
	if(!fp)
		error(1,errno,"Couldn't open %s", dir);
	*repository=NULL;
	if((repos_len=getline(repository,&repos_len,fp))<0)
		error(1,errno,"Couldn't read %s", dir);
	fclose(fp);
	(*repository)[repos_len-1]='\0';

	*p='\0';
	strcat(dir,CVSADM_ROOT);
	if(!isfile(dir) || isdir(dir))
		error(1,0,"%s is not part of a checked out repository",filename);
	fp = fopen(dir,"r");
	if(!fp)
		error(1,errno,"Couldn't open %s", dir);
	*root=NULL;
	if((repos_len=getline(root,&repos_len,fp))<0)
		error(1,errno,"Couldn't read %s", dir);
	fclose(fp);
	(*root)[repos_len-1]='\0';

	*p='\0';
	if(!strlen(dir))
		strcpy(dir,"./");

	*returned_dir = dir;

	TRACE(3,"%s is in %s/%s",PATCH_NULL(filename),PATCH_NULL(*root),PATCH_NULL(*repository));

	return 0;
}

int cvsrename(int argc, char **argv)
{
    int c;
    int err = 0;
	char *repos_file1, *repos_file2;
	char *root1, *root2;
	const char *filename1, *filename2, *dir1, *dir2;
	int rootlen;
	List *ent,*ent2;
	Node *node;
	Entnode *entnode;
	const char *cwd;

	if (argc == -1)
		usage (rename_usage);

	quiet = 0;

    optind = 0;
    while ((c = getopt (argc, argv, "+q")) != -1)
    {
	switch (c)
	{
	case 'q':
		quiet = 1;
		break;
    case '?':
    default:
		usage (rename_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

	if(argc!=2)
	{
		usage(rename_usage);
	};

	error(0,0,"Warning: rename is still experimental and may not behave as you would expect");

	if(current_parsed_root->isremote)
	{
		if(!supported_request("Rename"))
			error(1,0,"Remote server does not support rename");
		if(!supported_request("Can-Rename"))
			error(1,0,"Renames are currently disabled");
	}

	if(!strcmp(argv[0],argv[1]))
		return 0;

	rootlen = strlen(current_parsed_root->directory);

	if(!isfile(argv[0]))
		error(1,0,"%s does not exist",argv[0]);

	if(isfile(argv[1]) && fncmp(argv[0],argv[1])) /* We allow case renames (on Unix this is redundant) */
		error(1,0,"%s already exists",argv[1]);

	if(isdir(argv[0]))
		error(1,0,"Directory renames are not currently supported");

	validate_file(argv[0],&root1, &repos_file1, &filename1, &dir1, 1);
	validate_file(argv[1],&root2, &repos_file2, &filename2, &dir2, 0);

	if(strcmp(root1,root2) || strcmp(root1,current_parsed_root->original))
		error(1,0,"%s and %s are in different repositories",argv[0],argv[1]);

	xfree(root1);
	xfree(root2);

	repos_file1 = (char*)xrealloc(repos_file1, strlen(filename1)+strlen(repos_file1)+rootlen+10);
	repos_file2 = (char*)xrealloc(repos_file2, strlen(filename2)+strlen(repos_file2)+rootlen+10);
	memmove(repos_file1+rootlen+1,repos_file1,strlen(repos_file1)+1);
	memmove(repos_file2+rootlen+1,repos_file2,strlen(repos_file2)+1);
	strcpy(repos_file1,current_parsed_root->directory);
	strcpy(repos_file2,current_parsed_root->directory);
	repos_file1[rootlen]='/';
	repos_file2[rootlen]='/';
	strcat(repos_file1,"/");
	strcat(repos_file2,"/");
	strcat(repos_file1,filename1);
	strcat(repos_file2,filename2);

	if(fncmp(argv[0],argv[1]))
		set_mapping(dir2,repos_file2+rootlen+1,""); /* Delete old file */
	if(fncmp(dir1,dir2))
		set_mapping(dir1,repos_file1+rootlen+1,""); 
	set_mapping(dir2,repos_file1+rootlen+1,repos_file2+rootlen+1); /* Rename to new file */

	cwd = xgetwd();
	if(CVS_CHDIR(dir1))
		error(1,errno,"Couldn't chdir to %s",dir1);
	ent = Entries_Open(0, NULL);
	node = findnode_fn(ent, filename1);
	entnode=(Entnode*)node->data;

	if(!node)
	{
		error(1,0,"%s is not listed in CVS/Entries",filename1);
		CVS_CHDIR(cwd);
		xfree(cwd);
		return 1;
	}

	if(!fncmp(dir1,dir2))
		Rename_Entry(ent,filename1,filename2);
	else
	{
		if(CVS_CHDIR(cwd))
			error(1,errno,"Couldn't chdir to %s",cwd);
		if(CVS_CHDIR(dir2))
			error(1,errno,"Couldn't chdir to %s",dir2);
		ent2 = Entries_Open(0, NULL);
		if(entnode->type==ENT_FILE)
			Register(ent2,(char*)filename2,entnode->version,entnode->timestamp,entnode->options,entnode->tag,entnode->date,entnode->conflict,entnode->merge_from_tag_1,entnode->merge_from_tag_2,entnode->rcs_timestamp,entnode->edit_revision,entnode->edit_tag,entnode->edit_bugid,entnode->md5);
		else if(entnode->type==ENT_SUBDIR)
			Subdir_Register(ent2,NULL,filename2);
		else
			error(1,0,"Unknown entry type %d in entries file",node->type);

		Entries_Close(ent2);
		if(CVS_CHDIR(cwd))
			error(1,errno,"Couldn't chdir to %s",cwd);
		if(CVS_CHDIR(dir1))
			error(1,errno,"Couldn't chdir to %s",dir1);

		if(entnode->type==ENT_SUBDIR)
			Subdir_Deregister(ent,NULL,filename1);
		else if(entnode->type==ENT_FILE)
			Scratch_Entry(ent,filename1);
		else
			error(1,0,"Unknown entry type %d in entries file",node->type);
	}
	Entries_Close(ent);

	CVS_RENAME(argv[0],argv[1]);

	if(isdir(argv[1]))
	{
		char *tmp=(char*)xmalloc(strlen(argv[1])+strlen(CVSADM_VIRTREPOS)+10);
		FILE *fp;
		sprintf(tmp,"%s/%s",argv[1],CVSADM_VIRTREPOS);
		fp = fopen(tmp,"w");
		if(!fp)
			error(0,errno,"Couldn't write %s",tmp);
		fprintf(fp,"%s\n",repos_file2+rootlen+1);
		fclose(fp);
	}

	xfree(repos_file1);
	xfree(repos_file2);
	xfree(dir1);
	xfree(dir2);
	CVS_CHDIR(cwd);
	xfree(cwd);

    return (err);
}
