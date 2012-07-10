/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "getline.h"

#include "../cvsapi/TokenLine.h"
#include "../cvsapi/GetOptions.h"

/*
  Original Author:  athan@morgan.com <Andrew C. Athan> 2/1/94
  Modified By:      vdemarco@bou.shl.com

  This package was written to support the NEXTSTEP concept of
  "wrappers."  These are essentially directories that are to be
  treated as "files."  This package allows such wrappers to be
  "processed" on the way in and out of CVS.  The intended use is to
  wrap up a wrapper into a single tar, such that that tar can be
  treated as a single binary file in CVS.  To solve the problem
  effectively, it was also necessary to be able to prevent rcsmerge
  application at appropriate times.

  ------------------
  Format of wrapper file ($CVSROOT/CVSROOT/cvswrappers or .cvswrappers)

  wildcard	[option value][option value]...

  where option is one of
  -m		update methodology	value: MERGE or COPY
  -k		default -k rcs option to use on import or add

  and value is a single-quote delimited value.

  E.g:
  *.nib		-m 'COPY'
*/

enum WrapMergeMethod
{
	WRAP_MERGE, 
	WRAP_COPY 
};

struct WrapperEntry
{
	WrapperEntry() { isRemote=isTemp=false; mergeMethod=WRAP_MERGE; }
	cvs::string mimeType;
	cvs::wildcard_filename wildCard;
	cvs::string rcsOption;
    WrapMergeMethod mergeMethod;
	bool isRemote;
	bool isTemp;
	bool isDefault;
	cvs::string xdiffWrapper;
};

// Note that *.doc isn't here because it's traditionally meant 'document'
// in the Unix world, not just 'Word Document' so there isn't enough certainty
// that it should be binary
static const char *wrap_default[] =
{
	"*.a -kb",
	"*.avi -kb",
	"*.bin -kb",
	"*.bmp -kb",
	"*.bz2 -kb",
	"*.chm -kb",
	"*.class -kb",
	"*.cur -kb",
	"*.dll -kb",
	"*.doc -kb",
	"*.mpp -kb",
	"*.dvi -kb",
	"*.exe -kb",
	"*.gif -kb",
	"*.gz -kb",
	"*.hqx -kb",
	"*.ico -kb",
	"*.ilk -kb",
	"*.iso -kb",
	"*.lib -kb",
	"*.jar -kb",
	"*.jpg -kb",
	"*.jpeg -kb",
	"*.lnk -kb",
	"*.mpg -kb",
	"*.mpeg -kb",
	"*.mov -kb",
	"*.mp3 -kb",
	"*.ncb -kb",
	"*.o -kb",
	"*.ogg -kb",
	"*.obj -kb",
	"*.pdb -kb",
	"*.pdf -kb",
	"*.png -kb",
	"*.ppt -kb",
	"*.res -kb",
	"*.rpm -kb",
	"*.sit -kb",
	"*.so -kb",
	"*.tar -kb",
	"*.tga -kb",
	"*.tgz -kb",
	"*.tif -kb",
	"*.tiff -kb",
	"*.ttf -kb",
	"*.wav -kb",
	"*.wmv -kb",
	"*.xls -kb",
	"*.zip -kb",
	"*.Z -kb"
};

typedef std::vector<WrapperEntry> wrap_list_t;
static wrap_list_t wrap_list;

static void wrap_kill ();
static void wrap_kill_temp ();

static bool wrap_list_sort(const wrap_list_t::value_type& a,const wrap_list_t::value_type& b)
{
	if(a.isDefault && !b.isDefault)
		return true;
	if(b.isDefault && !a.isDefault)
		return false;
	if(a.isTemp && !b.isTemp)
		return false;
	if(b.isTemp && !a.isTemp)
		return true;
	if(a.isRemote && !b.isRemote)
		return false;
	if(b.isRemote && !a.isRemote)
		return true;
	return false;
}

void wrap_setup()
{
	char *homedir;
    char *ptr, *server_line;
    int len;
    int n;

    /* Add default wrappers */
    for(n=0; n<sizeof(wrap_default)/sizeof(wrap_default[0]); n++)
       wrap_add(wrap_default[n],false,false,false,false);

    if (current_parsed_root && !current_parsed_root->isremote)
    {
		cvs::filename file;

		cvs::sprintf(file,strlen(current_parsed_root->directory)+sizeof(CVSROOTADM)+sizeof(CVSROOTADM_WRAPPER)+10,"%s/%s/%s",current_parsed_root->directory,CVSROOTADM,CVSROOTADM_WRAPPER);

		if (isfile (file.c_str()))
			wrap_add_file(file.c_str(),false);
    }
	else if(supported_request("read-cvswrappers"))
	{
		TRACE(1,"Requesting server cvswrappers");
		send_to_server ("read-cvswrappers\n", 0);
		read_line(&server_line);
		if(server_line[0]=='E' && server_line[1]==' ')
		{
			fprintf (stderr, "%s\n", server_line + 2);
			error_exit();
		}
		len = atoi(server_line);
		cvs::smartptr<char,cvs::sp_free<char*> > tmp = (char*)xmalloc(len+1);
		ptr = tmp;
		while(len > 0)
		{
			size_t n;

			n = try_read_from_server (ptr, len);
			len -= n;
			ptr += n;
		}
		*ptr = '\0';
		ptr=strtok(tmp,"\n");
		while(ptr && *ptr)
		{
			wrap_add(ptr,false,true,false,false);
			ptr=strtok(NULL,"\n");
		}
		xfree(server_line);
	}

    /* Then add entries found in home dir, (if user has one) and file
       exists.  */
    homedir = get_homedir ();
    /* If we can't find a home directory, ignore ~/.cvswrappers.  This may
       make tracking down problems a bit of a pain, but on the other
       hand it might be obnoxious to complain when CVS will function
       just fine without .cvswrappers (and many users won't even know what
       .cvswrappers is).  */
    if (homedir != NULL)
    {
		cvs::filename file;

		cvs::sprintf(file,strlen(homedir)+sizeof(CVSDOTWRAPPER)+10,"%s/%s",(char*)homedir,CVSDOTWRAPPER);
		if (isfile (file.c_str()))
		    wrap_add_file (file.c_str(), false);
    }

    /* FIXME: calling wrap_add() below implies that the CVSWRAPPERS
     * environment variable contains exactly one "wrapper" -- a line
     * of the form
     * 
     *    FILENAME_PATTERN	FLAG  OPTS [ FLAG OPTS ...]
     *
     * This may disagree with the documentation, which states:
     * 
     *   `$CVSWRAPPERS'
     *      A whitespace-separated list of file name patterns that CVS
     *      should treat as wrappers. *Note Wrappers::.
     *
     * Does this mean the environment variable can hold multiple
     * wrappers lines?  If so, a single call to wrap_add() is
     * insufficient.
     */

    /* Then add entries found in CVSWRAPPERS environment variable. */
    wrap_add (CProtocolLibrary::GetEnvironment (WRAPPER_ENV), false, false, false, false);

	std::sort(wrap_list.begin(),wrap_list.end(),wrap_list_sort);
}

/* Send -W arguments for the wrappers to the server.  The command must
   be one that accepts them (e.g. update, import).  */
void wrap_send ()
{
    for (size_t i = 0; i < wrap_list.size(); ++i)
    {
		if(wrap_list[i].isRemote) /* Don't resend exising wrappers back to the server */
			continue;

		if (wrap_list[i].mergeMethod == WRAP_COPY)
	    /* For greater studliness we would print the offending option
	       and (more importantly) where we found it.  */
			error (0, 0, "-m wrapper option is not supported remotely; ignored");
		if (wrap_list[i].rcsOption.length())
		{
			send_to_server ("Argument -W\nArgument ", 0);
			send_to_server (wrap_list[i].wildCard.c_str(), 0);
			send_to_server (" -k '", 0);
			send_to_server (wrap_list[i].rcsOption.c_str(), 0);
			send_to_server ("'\n", 0);
		}
    }
}

/* Output wrapper entries in the format of cvswrappers lines.
 *
 * This is useful when one side of a client/server connection wants to
 * send its wrappers to the other; since the receiving side would like
 * to use wrap_add() to incorporate the wrapper, it's best if the
 * entry arrives in this format.
 *
 * The entries are stored in `line'
 *
 * If first_call is true, then start afresh.  */
bool wrap_unparse_rcs_options (cvs::string& line, bool& first_call)
{
    /* FIXME-reentrancy: we should design a reentrant interface, like
       a callback which fgets handed each wrapper (a multithreaded
       server being the most concrete reason for this, but the
       non-reentrant interface is fairly unnecessary/ugly).  */
    static size_t i;

    if (first_call)
		i=0;

	first_call=false;

    for (; i < wrap_list.size(); ++i)
    {
		if (wrap_list[i].rcsOption.length())
		{
			cvs::sprintf(line,256,"%s -k '%s'",wrap_list[i].wildCard.c_str(),wrap_list[i].rcsOption.c_str());

            /* We're going to miss the increment because we return, so
               do it by hand. */
            ++i;

            return true;
		}
    }
	return false;
}

/*
 * Open a file and read lines, feeding each line to a line parser. Arrange
 * for keeping a temporary list of wrappers at the end, if the "temp"
 * argument is set.
 */
void wrap_add_file (const char *file, bool isTemp)
{
    FILE *fp;
	char *line = NULL;
	size_t line_allocated;

    wrap_kill_temp ();

    /* Load the file.  */
    fp = fopen (file, "r");
    if (fp == NULL)
    {
		if (!existence_error (errno))
			error (0, errno, "cannot open %s", file);
		return;
    }

    while (getline (&line, &line_allocated, fp) >= 0)
		wrap_add (line, isTemp, false, false, false);

    if (ferror (fp))
		error (0, errno, "cannot read %s", file);
    if (fclose (fp) == EOF)
		error (0, errno, "cannot close %s", file);
	xfree(line);

	std::sort(wrap_list.begin(), wrap_list.end(), wrap_list_sort);
}

void wrap_kill()
{
    wrap_kill_temp();
	wrap_list.clear();
}

bool wrap_is_temp(const wrap_list_t::value_type& i)
{
	return i.isTemp;
}

void wrap_kill_temp()
{
	wrap_list.erase(std::remove_if(wrap_list.begin(), wrap_list.end(), wrap_is_temp),wrap_list.end());
}

void wrap_close()
{
	wrap_kill();
}

bool wrap_add(const char *line, bool isTemp, bool isRemote, bool Sort, bool fromCommand)
{
    WrapperEntry e;

    if (!line || line[0] == '#')
		return true;

    TRACE(3,"wrap_add(%s, %d, %d, %d, %d)",PATCH_NULL(line),(int)isTemp,(int)isRemote, (int)Sort, (int)fromCommand);

	/* Search for the wild card */
	CTokenLine tokenLine(line);
	size_t argnum = 0;

	while(argnum < tokenLine.size())
	{
		/* A wrapper entry of "!" clears the default list */
		if(!strcmp(tokenLine[argnum],"!"))
		{
			wrap_list.clear();
			argnum++;
			continue;
		}

		e.wildCard = tokenLine[argnum++];
		if(!strcmp(e.wildCard.c_str(),"*") || !strcmp(e.wildCard.c_str(),"*.*"))
			e.isDefault = true;
		else
			e.isDefault = false;

		e.mimeType = CFileAccess::mimetype(e.wildCard.c_str());

		CGetOptions getOpt(tokenLine,argnum,"+k:x:m:t:");
		
		if(getOpt.error())
		{
			error(0,0,"Bad cvswrappers line %s",line);
			break;
		}

		for(size_t n=0; n<getOpt.count(); n++)
		{
			switch(getOpt[n].option)
			{
				case 'm':
					if(tolower(getOpt[n].arg[0])=='c')
						e.mergeMethod=WRAP_COPY;
					else
						e.mergeMethod=WRAP_MERGE;
					break;
				case 'k':
					if(fromCommand && server_active && compat[compat_level].ignore_client_wrappers)
						break;
					e.rcsOption = getOpt[n].arg;
					break;
				case 'x':
					e.xdiffWrapper = getOpt[n].arg;
					break;
				case 't':
					e.mimeType = getOpt[n].arg;
					break;
				default:
					TRACE(3,"Bad option from cvswrappers?: %c\n",getOpt[n].option);
					break;
			}
		}

		e.isRemote=isRemote;
		e.isTemp = isTemp;

		wrap_list.push_back(e);
	}

	if(Sort)
		std::sort(wrap_list.begin(), wrap_list.end(), wrap_list_sort);

	return true;
}

static WrapperEntry *wrap_matching_entry (const char *name)
{
	for(size_t n = wrap_list.size(); n>0; --n)
	{
		if(wrap_list[n-1].wildCard == name)
			return &wrap_list[n-1];
	}
	return NULL;
}

/* Return 1 if the given filename is a wrapper filename */
bool wrap_name_has(const char *name, WrapMergeHas  has)
{
    WrapperEntry *e = wrap_matching_entry (name);

	if(!e)
		return false;

	switch(has)
	{
		case WRAP_RCSOPTION:
			return !e->rcsOption.empty();
		case WRAP_XDIFFWRAPPER:
			return !e->xdiffWrapper.empty();
	}

    return false;
}

/* Return the RCS options for FILENAME in a newly malloc'd string.  If
   ASFLAG, then include "-k" at the beginning (e.g. "-kb"), otherwise
   just give the option itself (e.g. "b").  */
char *wrap_rcsoption(const char *filename)
{
    WrapperEntry *e = wrap_matching_entry (filename);

	if(!e || !e->rcsOption.size())
		return NULL;

	char *options = NULL;
	const char *opt = e->rcsOption.c_str();
	assign_options(&options,opt);

	// If the *.* entry is -k+ or -k-, modify the value above
	if(!e->isDefault && wrap_list[0].isDefault)
	{
		opt = wrap_list[0].rcsOption.c_str();
		if(opt[0]=='+' || opt[0]=='-')
			assign_options(&options,opt);
	}					
	return options;
}

const char *wrap_xdiffwrapper(const char *filename)
{
    WrapperEntry *e=wrap_matching_entry(filename);

	if(!e || !e->xdiffWrapper.length())
		return NULL;
	return e->xdiffWrapper.c_str();
}

bool wrap_merge_is_copy (const char *fileName)
{
    WrapperEntry *e=wrap_matching_entry(fileName);

    if(e==NULL || e->mergeMethod==WRAP_MERGE)
		return false;
    return true;
}

void wrap_display()
{
    for(size_t n=0; n<wrap_list.size(); n++)
	{
		if(wrap_list[n].rcsOption.empty())
		{
			TRACE(3,"%c%c%s (ignored)\n",wrap_list[n].isRemote?'R':'L',wrap_list[n].isTemp?'T':'P',wrap_list[n].wildCard.c_str());
			continue;
		}

		TRACE(3,"%c%c:%s -k%s\n",wrap_list[n].isRemote?'R':'L',wrap_list[n].isTemp?'T':'P',wrap_list[n].wildCard.c_str(),wrap_list[n].rcsOption.c_str());
	    printf("%s -k%s\n",wrap_list[n].wildCard.c_str(),wrap_list[n].rcsOption.c_str());
	}
}
