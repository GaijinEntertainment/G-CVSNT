/*
 * Copyright (c) 1992, Mark D. Baushke
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
 * 
 * Name of Root
 * 
 * Determine the path to the CVSROOT and set "Root" accordingly.
 */

#include "cvs.h"
#include "getline.h"
#include "savecwd.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifndef DEBUG

char *Name_Root (const char *dir, const char *update_dir)
{
    FILE *fpin;
    char *ret;
	const char *xupdate_dir;
    char *root = NULL;
    size_t root_allocated = 0;
    char *tmp;
    char *cvsadm;
    char *cp;

    if (update_dir && *update_dir)
		xupdate_dir = update_dir;
    else
		xupdate_dir = ".";

    if (dir != NULL)
    {
		cvsadm = (char*)xmalloc (strlen (dir) + sizeof (CVSADM) + 10);
		sprintf (cvsadm, "%s/%s", dir, CVSADM);
		tmp = (char*)xmalloc (strlen (dir) + sizeof (CVSADM_ROOT) + 10);
		sprintf (tmp, "%s/%s", dir, CVSADM_ROOT);
    }
    else
    {
		cvsadm = xstrdup (CVSADM);
		tmp = xstrdup (CVSADM_ROOT);
    }

    /*
     * Do not bother looking for a readable file if there is no cvsadm
     * directory present.
     *
     * It is possible that not all repositories will have a CVS/Root
     * file. This is ok, but the user will need to specify -d
     * /path/name or have the environment variable CVSROOT set in
     * order to continue.  */
    if ((!isdir (cvsadm)) || (!isreadable (tmp)))
    {
	ret = NULL;
	goto out;
    }

    /*
     * The assumption here is that the CVS Root is always contained in the
     * first line of the "Root" file.
     */
    fpin = open_file (tmp, "r");

    if (getline (&root, &root_allocated, fpin) < 0)
    {
	/* FIXME: should be checking for end of file separately; errno
	   is not set in that case.  */
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, errno, "cannot read %s", CVSADM_ROOT);
	error (0, 0, "please correct this problem");
	ret = NULL;
	goto out;
    }
    (void) fclose (fpin);
    if ((cp = strrchr (root, '\n')) != NULL)
	*cp = '\0';			/* strip the newline */

    /*
     * root now contains a candidate for CVSroot. It must be an
     * absolute pathname or specify a remote server.
     */

    if ((strchr (root, ':') == NULL) && (strchr(root, '@') == NULL) &&
    	! isabsolute (root))
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0,
	       "ignoring %s because it does not contain an absolute pathname.",
	       CVSADM_ROOT);
	ret = NULL;
	goto out;
    }

    {
		const root_allow_struct *found_root = NULL;
		const char *real_root;
		
		if(!server_active || !root_allow_ok(root,found_root,real_root,false))
			real_root = root;
	    if ((strchr (root, ':') == NULL) && (strchr(root, '@') == NULL) && !isdir (real_root))
		{
			error (0, 0, "in directory %s:", xupdate_dir);
			error (0, 0, "ignoring %s because it specifies a non-existent repository %s", CVSADM_ROOT, root);
			ret = NULL;
			goto out;
		}
		if(found_root && !found_root->online)
		{
			error (0, 0, "in directory %s:", xupdate_dir);
			error (0, 0, "ignoring %s because repository %s is offline", CVSADM_ROOT, root);
			ret = NULL;
			goto out;
		}
    }

    /* allocate space to return and fill it in */
    strip_trailing_slashes (root);
    ret = xstrdup (root);
 out:
    xfree (cvsadm);
	xfree (tmp);
	xfree (root);
    return (ret);
}

/*
 * Write the CVS/Root file so that the environment variable CVSROOT
 * and/or the -d option to cvs will be validated or not necessary for
 * future work.
 */
void Create_Root (const char *dir, const char *rootdir)
{
    FILE *fout;
    char *tmp;

    if (noexec)
	return;

    /* record the current cvs root */

    if (rootdir != NULL)
    {
        if (dir != NULL)
	{
	    tmp = (char*)xmalloc (strlen (dir) + sizeof (CVSADM_ROOT) + 10);
	    (void) sprintf (tmp, "%s/%s", dir, CVSADM_ROOT);
	}
        else
	    tmp = xstrdup (CVSADM_ROOT);

        fout = open_file (tmp, "w+");
        if (fprintf (fout, "%s\n", rootdir) < 0)
	    error (1, errno, "write to %s failed", tmp);
        if (fclose (fout) == EOF)
	    error (1, errno, "cannot close %s", tmp);
	xfree (tmp);
    }
}

#endif /* ! DEBUG */


/* The root_allow_* stuff maintains a list of legal CVSROOT
   directories.  Then we can check against them when a remote user
   hands us a CVSROOT directory.  */

static std::vector<root_allow_struct> root_allow_vector;

void root_allow_add(const char *root, const char *name, bool online, bool readwrite, bool proxypasswd, RootType repotype, const char *remote_serv, const char *remote_repos, const char *proxy_repos, const char *remote_pass)
{
	root_allow_struct r;

	r.root = root;
	r.name = name;
	r.online = online;
	r.readwrite = readwrite;
	r.proxypasswd = proxypasswd;
	r.repotype = repotype;
	r.remote_server = remote_serv?remote_serv:"";
	r.remote_repository = remote_repos?remote_repos:"";
	r.proxy_repository = proxy_repos?proxy_repos:"";
	r.remote_passphrase = remote_pass?remote_pass:"";

	root_allow_vector.push_back(r);
}

void root_allow_free ()
{
	root_allow_vector.clear();
}

bool root_allow_ok (const char *root, const root_allow_struct*& found_root, const char *&real_root, bool authuse)
{
    size_t i;
    for (i = 0; i < root_allow_vector.size(); ++i)
	{
		if(!fncmp(root_allow_vector[i].name.c_str(), root))
		{
			found_root = &root_allow_vector[i];
			if ((found_root->proxypasswd)&&(authuse))
			real_root = found_root->proxy_repository.c_str();
			else
			real_root = found_root->root.c_str();
			break;
	
		}
	}

    if(i==root_allow_vector.size())
      return false;
#ifndef _WIN32
    if(chroot_done && chroot_base && real_root)
    {
      if(!fnncmp(real_root,chroot_base,strlen(chroot_base)) && strlen(chroot_base)<=strlen(real_root))
			real_root+=strlen(chroot_base);
    }
#endif
    return true;
}



/* This global variable holds the global -d option.  It is NULL if -d
   was not used, which means that we must get the CVSroot information
   from the CVSROOT environment variable or from a CVS/Root file.  */

char *CVSroot_cmdline;



/* Parse a CVSROOT variable into its constituent parts -- method,
 * username, hostname, directory.  The prototypical CVSROOT variable
 * looks like:
 *
 * :method:user@host:path
 *
 * Some methods may omit fields; local, for example, doesn't need user
 * and host.
 *
 * Returns pointer to new cvsroot on success, NULL on failure. */

cvsroot *current_parsed_root = NULL;



/* allocate and initialize a cvsroot
 *
 * We must initialize the strings to NULL so we know later what we should
 * free
 *
 * Some of the other zeroes remain meaningful as, "never set, use default",
 * or the like
 */
cvsroot *new_cvsroot_t()
{
    cvsroot *newroot;

    /* gotta store it somewhere */
    newroot = (cvsroot*)xmalloc(sizeof(cvsroot));

    newroot->original = NULL;
    newroot->method = NULL;
    newroot->username = NULL;
    newroot->password = NULL;
    newroot->hostname = NULL;
    newroot->port = NULL;
    newroot->directory = NULL;
	newroot->mapped_directory = NULL;
	newroot->unparsed_directory = NULL;
	newroot->optional_1 = NULL;
	newroot->optional_2 = NULL;
	newroot->optional_3 = NULL;
	newroot->optional_4 = NULL;
	newroot->optional_5 = NULL;
	newroot->optional_6 = NULL;
	newroot->optional_7 = NULL;
	newroot->optional_8 = NULL;
	newroot->proxy = NULL;
	newroot->proxyport = NULL;
	newroot->proxyprotocol = NULL;
	newroot->proxyuser = NULL;
	newroot->proxypassword = NULL;
    newroot->isremote = false;
	newroot->password_used = false;
	newroot->type = RootTypeStandard;
	newroot->remote_server = NULL;
	newroot->remote_repository = NULL;
	newroot->proxy_repository_root = NULL;
	newroot->remote_passphrase = NULL;

    return newroot;
}



/* Dispose of a cvsroot and its component parts */
void free_cvsroot_t (cvsroot *root)
{
	if(!root)
		return;

	if (root->method)
		xfree(root->method);
	xfree (root->original);
	xfree (root->username);
	/* I like to be paranoid */
	if(root->password)
		memset ((char*)root->password, 0, strlen (root->password));
	xfree (root->password);
	xfree (root->hostname);
	xfree (root->port);
	xfree (root->directory);
	xfree(root->unparsed_directory);
	xfree(root->optional_1);
	xfree(root->optional_2);
	xfree(root->optional_3);
	xfree(root->optional_4);
	xfree(root->optional_5);
	xfree(root->optional_6);
	xfree(root->optional_7);
	xfree(root->optional_8);
	xfree(root->proxy);
	xfree(root->proxyport);
	xfree(root->proxyprotocol);
	xfree(root->proxyuser);
	xfree(root->proxypassword);
	xfree(root->mapped_directory);
	xfree(root->remote_server);
	xfree(root->remote_repository);
	xfree(root->proxy_repository_root);
	xfree(root->remote_passphrase);
    xfree (root);
}

static int get_keyword(char *buffer, int buffer_len, char **ptr)
{
	int len = 0;
	int in_quote = 0; 
	while(**ptr && len<buffer_len && (in_quote || (isprint(**ptr) && **ptr!='=' && **ptr!=';' && **ptr!=',')))
	{
		if(**ptr==in_quote)
		{
			in_quote = 0;
			(*ptr)++;
			continue;
		}
		else if(!in_quote && (**ptr=='"' || **ptr=='\''))
		{
			in_quote = **ptr;
			(*ptr)++;
			continue;
		}
		buffer[len++]=*(*ptr)++;
	}
	if(in_quote)
		error(0,0,"Nonterminated quote in cvsroot string");
	buffer[len]='\0';
	return len;
}

/* Parse a keyword in a cvsroot string, either old format (:method;foo=x,bar=x:/stuff) */
static int parse_keyword(char *keyword, char **p, cvsroot *newroot)
{
	char value[256];

	get_keyword(value,sizeof(value),p);
	if(!strcasecmp(keyword,"method") || !strcasecmp(keyword,"protocol"))
	{
		CProtocolLibrary lib;
		if(*value && strcasecmp(value,"local"))
			strcpy(newroot->method,xstrdup(value));
		if(newroot->method)
		{
			client_protocol = lib.LoadProtocol(newroot->method);
			if(client_protocol==NULL)
			{
				error (0, 0, "the :%s: access method is not available on this system", newroot->method);
				return -1;
			}
			if(!client_protocol->connect)
			{
				error (0, 0, "the :%s: access method cannot be used to connect to other servers", newroot->method);
				return -1;
			}
			lib.SetupServerInterface(newroot,server_io_socket);
		}

		newroot->isremote = (newroot->method==NULL)?0:1;
	}
	else if(!strcasecmp(keyword,"username") || !strcasecmp(keyword,"user"))
	{
		TRACE(1,"Depreciated keyword 'username' used");
		if(*value)
			newroot->username = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"password") || !strcasecmp(keyword,"pass"))
	{
		TRACE(1,"Depreciated keyword 'password' used");
		newroot->password = xstrdup(value); // Empty password is valid
	}
	else if(!strcasecmp(keyword,"hostname") || !strcasecmp(keyword,"host"))
	{
		TRACE(1,"Depreciated keyword 'hostname' used");
		if(*value)
			newroot->hostname = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"port"))
	{
		TRACE(1,"Depreciated keyword 'port' used");
		if(*value)
			newroot->port = xstrdup(value);
		if(newroot->port)
		{
			char *q = value;
			while (*q)
			{
				if (!isdigit(*q++))
				{
					error(0, 0, "CVSROOT (\"%s\")", newroot->original);
					error(0, 0, "may only specify a positive, non-zero, integer port (not \"%s\").",
						newroot->port);
					return -1;
				}
			}
		}
	}
	else if(!strcasecmp(keyword,"directory") || !strcasecmp(keyword,"path"))
	{
		TRACE(1,"Depreciated keyword 'directory' used");
		if(*value)
			newroot->directory = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"proxy"))
	{
		if(*value)
			newroot->proxy = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"proxyport") || !strcasecmp(keyword,"proxy_port"))
	{
		if(*value)
			newroot->proxyport = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"tunnel") || !strcasecmp(keyword,"proxy_protocol") ||
			!strcasecmp(keyword,"proxyprotocol"))
	{
		if(*value)
			newroot->proxyprotocol = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"proxyuser") || !strcasecmp(keyword,"proxy_user"))
	{
		if(*value)
			newroot->proxyuser = xstrdup(value);
	}
	else if(!strcasecmp(keyword,"proxypassword") || !strcasecmp(keyword,"proxy_password"))
	{
		if(*value)
			newroot->proxypassword = xstrdup(value);
	}
	else if(client_protocol && client_protocol->validate_keyword)
	{
		int res = client_protocol->validate_keyword(client_protocol,newroot,keyword,value);
		if(res==CVSPROTO_FAIL || res==CVSPROTO_BADPARMS || res==CVSPROTO_AUTHFAIL)
		{
			error(0,0,"Bad CVSROOT: Unknown keyword '%s'",keyword);
			return -1;
		}
	}
	else
	{
		error(0,0,"Bad CVSROOT: Unknown keyword '%s'",keyword);
		return -1;
	}
	return 0;
}
/*
 * parse a CVSROOT string to allocate and return a new cvsroot structure
 */
cvsroot *parse_cvsroot (const char *root_in)
{
    cvsroot *newroot;			/* the new root to be returned */
    char *cvsroot_save;			/* what we allocated so we can dispose
					 * it when finished */
    char *firstslash;			/* save where the path spec starts
					 * while we parse
					 * [[user][:password]@]host[:[port]]
					 */
	char char_at_slash; /* Character might be '/', '\', or a drive letter */
    char *cvsroot_copy, *p, *p1, *q, *keywords;		/* temporary pointers for parsing */
    int check_hostname, no_port, no_password;
	int password_begin=0, password_end=0;

    /* allocate some space */
    newroot = new_cvsroot_t();

    /* save the original string */
    newroot->original = xstrdup (root_in);

    /* and another copy we can munge while parsing */
    cvsroot_save = cvsroot_copy = xstrdup (root_in);

	if (*cvsroot_copy == '[')
	{
		char keyword[256];
		/* Comma separated cvsroot, depreciated. */
		TRACE(1,"Depreciated cvsroot format [...] used.\n");
		p=cvsroot_copy;
		p++;
		while(get_keyword(keyword,sizeof(keyword),&p))
		{
			if(*p!='=')
			{
				error(0,0,"Bad CVSROOT: Specify comma separated keyword=value pairs");
				goto error_exit;
			}
			p++;
			if(parse_keyword(keyword, &p, newroot)<0)
				goto error_exit;
			if(*p==']')
				break;
			if(*p!=',')
			{
				error(0,0,"Bad CVSROOT: Specify comma separated keyword=value pairs");
				goto error_exit;
			}
			p++;
		}
		if(*p!=']')
		{
			error(0,0,"Bad CVSROOT: Specify comma separated keyword=value pairs");
			goto error_exit;
		}
		goto new_root_ok;
	}

	if (*cvsroot_copy == ':' || (!isabsolute(cvsroot_copy) && strchr(cvsroot_copy,'@')))
    {
		char *method;
		char in_quote,escape;

		/* Access method specified, as in
		* "cvs -d :(gserver|kserver|pserver)[;params...]:[[user][:password]@]host[:[port]]/path",
		* "cvs -d [:(ext|server)[;params...]:]{access_method}[[user]@]host[:]/path",
		* "cvs -d :local[;params...]:e:\path",
		* "cvs -d :fork[;params...]:/path".
		* We need to get past that part of CVSroot before parsing the
		* rest of it.
		*/

		if(cvsroot_copy[0]==':')
		{
			method=++cvsroot_copy;
			p=method;
			in_quote=0;
			escape=0;
			while((in_quote || *p!=':') && *p)
			{
				if(escape)
				{
					escape=0;
					p++;
					continue;
				}
				if(in_quote == *p)
					in_quote=0;
				else if(!in_quote && (*p=='"' || *p=='\''))
					in_quote=*p;
				else if(!in_quote && *p=='\\')
					escape=1;
				p++;
			}
			if (!*p)
			{
				error (0, 0, "bad CVSroot: %s", root_in);
				xfree (cvsroot_save);
				goto error_exit;
			}
			*p = '\0';
			cvsroot_copy = ++p;
		}
		else
			method="ext";

		keywords = strchr(method,';');
		if(keywords)
			*(keywords++) = '\0';

		newroot->method = xstrdup(method);

		if(newroot->method && !strcasecmp(newroot->method,"local"))
		{
			xfree(newroot->method);
			newroot->method = NULL;
			newroot->isremote=false;
		}
		else if(newroot->method && !*newroot->method)
		{
			xfree(newroot->method);
			newroot->method = NULL;
			newroot->isremote=true;
		}
		else if(newroot->method)
		{
			CProtocolLibrary lib;
			client_protocol = lib.LoadProtocol(newroot->method);
			if(client_protocol==NULL)
			{
				error (1, 0, "the :%s: access method is not available on this system", newroot->method);
			}
			if(!client_protocol->connect)
			{
				error (1, 0, "the :%s: access method cannot be used to connect to other servers", newroot->method);
			}
			lib.SetupServerInterface(newroot,server_io_socket);
			newroot->isremote=true;
		}

		/* Process extra parameters */
		if(keywords)
		{
			char keyword[256];

			p=keywords;

			while(*p && get_keyword(keyword,sizeof(keyword),&p))
			{
				if(*p!='=')
				{
					error(0,0,"Bad CVSROOT: Specify semicolon separated keyword=value pairs");
					goto error_exit;
				}
				p++;
				if(parse_keyword(keyword, &p, newroot)<0)
					goto error_exit;
				if(!*p)
					break;
				if(*p!=';')
				{
					error(0,0,"Bad CVSROOT: Specify semicolon separated keyword=value pairs");
					goto error_exit;
				}
				p++;
			}

			if(*p)
			{
				error(0,0,"Bad CVSROOT: Specify semicolon separated keyword=value pairs");
				goto error_exit;
			}
		}
	}
    else
    {
		newroot->method = NULL;
		newroot->isremote = false;
    }

    if (newroot->method)
    {
	/* split the string into {optional_data}[[user][:password]@]host[:[port]] & /path
	 *
	 * this will allow some characters such as '@' & ':' to remain unquoted
	 * in the path portion of the spec
	 */

	/* Optional data that goes before everything else.  The format is defined only by
	   the protocol. For ext, it's an override for the CVS_RSH variable */
	if(cvsroot_copy[0]=='{')
	{
		p = strchr(cvsroot_copy+1,'}');
		if(p)
		{
			int len = p-cvsroot_copy-1;
			char *s = (char*)xmalloc(len+1);
			newroot->optional_1 = s;
			strncpy(s,cvsroot_copy+1,len);
			s[len]='\0';
			cvsroot_copy = p+1;
		}
	}

	p1=strchr(cvsroot_copy,'@');
	if(!p1) p1=cvsroot_copy;
	if (((p = strchr (p1, '/')) == NULL) && (p = strchr (p1, '\\')) == NULL)
	{
	    error (0, 0, "CVSROOT (\"%s\")", root_in);
	    error (0, 0, "requires a path spec");
	    if(client_protocol)
	    	error (0, 0, "%s",client_protocol->syntax);

		xfree (cvsroot_save);
	    goto error_exit;
	}
	if(p>cvsroot_copy+2)
	{
		if(*(p-1)==':' && *(p-3)==':') // host:drive:/path
			p-=2;
	}
	else if(p==cvsroot_copy+2)
	{
		if(*(p-1)==':') // drive:/path
			p-=2;
	}
	firstslash = p;		/* == NULL if '/' not in string */
	char_at_slash = *p;
	*p = '\0';

	/* Check to see if there is a username[:password] in the string. */
	if ((p = strchr (cvsroot_copy, '@')) != NULL)
	{
	    *p = '\0';
	    /* check for a password */
	    if ((q = strchr (cvsroot_copy, ':')) != NULL)
	    {
            password_begin = q - cvsroot_save;
            password_end = (q+strlen(q)) - cvsroot_save;
			*q = '\0';
			newroot->password = xstrdup (++q);
			/* Don't check for *newroot->password == '\0' since
			* a user could conceivably wish to specify a blank password
			* (newroot->password == NULL means to use the
			* password from .cvspass)
			*/
	    }

	    /* copy the username */
	    if (*cvsroot_copy && strcmp(cvsroot_copy,"."))
			/* a blank username is impossible, so leave it NULL in that
			* case so we know to use the default username
			*/
			newroot->username = xstrdup (cvsroot_copy);

	    cvsroot_copy = ++p;
	}

	if(newroot->isremote)
	{
		/* now deal with host[:[port]] */

		/* the port */
		p=cvsroot_copy;
		if(p[0]=='[')
		{
			p=strchr(cvsroot_copy,']');
			if(!p)
			{
				error(0, 0, "CVSROOT (\"%s\")", root_in);
				error(0, 0, "Invalid hostname");
				xfree (cvsroot_save);
				goto error_exit;
			}
		}		
		if ((p = strchr (p, ':')) != NULL)
		{
			
			*p++ = '\0';
			if (strlen(p))
			{
				q = p;
				if (*q == '-') q++;
				while (*q && !(*q==':' && !*(q+1)) && !(isalpha(*q) && *(q+1)==':' && !*(q+2)))
				{
					if (!isdigit(*q++))
					{
						error(0, 0, "CVSROOT (\"%s\")", root_in);
						error(0, 0, "may only specify a positive, non-zero, integer port (not \"%s\").",
							p);
						error(0, 0, "perhaps you entered a relative pathname?");
						xfree (cvsroot_save);
						goto error_exit;
					}
				}
				*q='\0';
				if (atoi(p) <= 0)
				{
					error (0, 0, "CVSROOT (\"%s\")", root_in);
					error(0, 0, "may only specify a positive, non-zero, integer port (not \"%s\").",
						p);
					error(0, 0, "perhaps you entered a relative pathname?");
					xfree (cvsroot_save);
					goto error_exit;
				}
				newroot->port = xstrdup(p);
			}
		}

		/* copy host */
		if (*cvsroot_copy != '\0')
			/* blank hostnames are invalid, but for now leave the field NULL
			 * and catch the error during the sanity checks later
			 */
			 newroot->hostname = xstrdup (cvsroot_copy);
	}

	/* restore the '/' */
	cvsroot_copy = firstslash;
	*cvsroot_copy = char_at_slash;
    }

    /* parse the path for all methods */
	if(newroot->isremote)
	{
		if(!newroot->method)
		{
			newroot->hostname = xstrdup (cvsroot_copy);
			cvsroot_copy=NULL;
		}
		else
			newroot->directory = xstrdup (cvsroot_copy);
	}
	else
	{
		const root_allow_struct *found_root = NULL;
		const char *real_root;
		
		if(!root_allow_ok(cvsroot_copy,found_root,real_root,false))
			real_root = cvsroot_copy;
		newroot->directory = xstrdup(real_root);
	}

new_root_ok:
	// Wipe the password from the stored cvsroot string
	if(password_begin && password_end && password_begin<password_end)
	{
		strcpy((char*)newroot->original+password_begin,newroot->original+password_end);
		newroot->password_used = true;
	}

	newroot->unparsed_directory = xstrdup(cvsroot_copy);
    xfree (cvsroot_save);

	/* Get mapped directory, so that we're always talking the root as understood by the 
	   client, not the server */
	if(!newroot->isremote)
	{
	    struct saved_cwd cwd;
		save_cwd(&cwd);
		CVS_CHDIR(newroot->directory);
		newroot->mapped_directory = xgetwd();
		if(!fncmp(newroot->mapped_directory,newroot->directory))
			xfree(newroot->mapped_directory); /* Saves a step if there's no mapping required */
		else
			TRACE(3,"mapping %s -> %s",PATCH_NULL(newroot->mapped_directory),PATCH_NULL(newroot->directory));
		restore_cwd(&cwd, NULL);
		free_cwd(&cwd);
	}

	/* If we are local, set CVS_Username for later */
	if(!newroot->method)
	{
		xfree(CVS_Username);
		CVS_Username=xstrdup(getcaller());
	}

    /*
     * Do various sanity checks.
     */

    check_hostname = 0;
    no_password = 0;
    no_port = 0;
    if(!newroot->method)
    {
		if (newroot->isremote && (newroot->username || newroot->port || newroot->password))
		{
			error (0, 0, "can't specify username or password in CVSROOT");
			error (0, 0, "(\"%s\")", root_in);
			error (0, 0, "when using anonymous access method");
			goto error_exit;
		}

		if (!newroot->isremote && (newroot->username || newroot->hostname || newroot->port || newroot->password))
		{
			error (0, 0, "can't specify hostname and username in CVSROOT");
			error (0, 0, "(\"%s\")", root_in);
			error (0, 0, "when using local access method");
			goto error_exit;
		}
	}

	/* Anonymous access.  Fill in all the parameters from the remote server configuration */
	if(newroot->isremote && !newroot->method)
	{
		CServerInfo si;
		if(strchr(newroot->hostname,'/'))
		{
			// Global server
			const char *root = si.getGlobalServerInfo(newroot->hostname);
			if(!root)
			{
				error(0, 0, "Remote server '%s' not found",newroot->hostname);
				goto error_exit;
			}
			free_cvsroot_t(newroot);
			newroot = parse_cvsroot(root); // Recursive call to parse the new root
		}
		else
		{
			CServerInfo::remoteServerInfo rsi;
			if(!si.getRemoteServerInfo(newroot->hostname, rsi))
				goto error_exit;
			if(!rsi.anon_protocol.size() || !rsi.anon_username.size() ||
				!rsi.default_repository.size())
			{
				error(0, 0, "Remote server '%s' does not allow anonymous login",newroot->hostname);
				goto error_exit;
			}
			if (strlen(rsi.anon_protocol.c_str())>20)
			{
				error(0, 0, "Remote server '%s' protocol too long",rsi.anon_protocol.c_str());
				goto error_exit;
			}
			newroot->method = xstrdup(rsi.anon_protocol.c_str());
			newroot->directory = xstrdup(rsi.default_repository.c_str());
			newroot->username = xstrdup(rsi.anon_username.c_str());
			newroot->password = xstrdup("");

			CProtocolLibrary lib;
			client_protocol = lib.LoadProtocol(newroot->method);
			if(client_protocol==NULL)
			{
				error (0, 0, "Anonymous connection failed");
				error (0, 0, "the :%s: access method is not available on this system", newroot->method);
				goto error_exit;
			}
			if(!client_protocol->connect)
			{
				error (0, 0, "Anonymous connection failed");
				error (0, 0, "the :%s: access method cannot be used to connect to other servers", newroot->method);
				goto error_exit;
			}
			lib.SetupServerInterface(newroot,server_io_socket);
		}
	}

	if (!newroot->directory || *newroot->directory == '\0')
    {
		error (0, 0, "missing directory in CVSROOT: %s", root_in);
		goto error_exit;
    }

	/* cvs.texinfo has always told people that CVSROOT must be an
	   absolute pathname.  Furthermore, attempts to use a relative
	   pathname produced various errors (I couldn't get it to work),
	   so there would seem to be little risk in making this a fatal
	   error.  */
	if (!isabsolute_remote (newroot->directory))
	{
	    error (0, 0, "CVSROOT \"%s\" must be an absolute pathname",
		   newroot->directory);
	    goto error_exit;
	}

	if(client_protocol)
	{
		if((client_protocol->required_elements&elemUsername) && !newroot->username)
		{
			error(0,0,"bad CVSROOT - Username required: %s",root_in);
			goto error_exit;
		}
		if((client_protocol->required_elements&elemHostname) && !newroot->hostname)
		{
			error(0,0,"bad CVSROOT - Hostname required: %s",root_in);
			goto error_exit;
		}
		if((client_protocol->required_elements&elemPort) && !newroot->port)
		{
			error(0,0,"bad CVSROOT - Port required: %s",root_in);
			goto error_exit;
		}
		if((client_protocol->required_elements&elemTunnel) && !(newroot->proxyprotocol||newroot->proxy))
		{
			error(0,0,"bad CVSROOT - Proxy required: %s",root_in);
			goto error_exit;
		}
		if(!(client_protocol->valid_elements&elemUsername) && newroot->username)
		{
			error(0,0,"bad CVSROOT - Cannot specify username: %s",root_in);
			goto error_exit;
		}
		if(!(client_protocol->valid_elements&elemHostname) && newroot->hostname)
		{
			error(0,0,"bad CVSROOT - Cannot specify hostname: %s",root_in);
			goto error_exit;
		}
		if(!(client_protocol->valid_elements&elemPort) && newroot->port)
		{
			error(0,0,"bad CVSROOT - Cannot specify port: %s",root_in);
			goto error_exit;
		}
		if(!(client_protocol->valid_elements&elemTunnel) && (newroot->proxyprotocol||newroot->proxy))
		{
			error(0,0,"bad CVSROOT - Cannot specify proxy: %s",root_in);
			goto error_exit;
		}
		if(client_protocol->validate_details && client_protocol->validate_details(client_protocol, newroot))
		{
			error (0, 0, "bad CVSROOT: %s", root_in);
			goto error_exit;
		}
	}

    /* Hooray!  We finally parsed it! */
    return newroot;

error_exit:
    free_cvsroot_t (newroot);
    return NULL;
}

static const char *get_default_client_port(const struct protocol_interface *protocol)
{
	struct servent *ent;
	static char p[32];

	if((ent=getservbyname("cvspserver","tcp"))!=NULL)
	{
		sprintf(p,"%u",ntohs(ent->s_port));
		return p;
	}

	return "2401";
}

/* Use root->username, root->hostname, root->port, and root->directory
 * to create a normalized CVSROOT fit for the .cvspass file
 *
 * username defaults to the result of getcaller()
 * port defaults to the result of get_cvs_port_number()
 *
 * FIXME - we could cache the canonicalized version of a root inside the
 * cvsroot, but we'd have to un'const the input here and stop expecting the
 * caller to be responsible for our return value
 */
char *normalize_cvsroot (const cvsroot *root)
{
    char *cvsroot_canonical;
    char *p, *hostname;
    const char *username;
    char port_s[64];

    /* get the appropriate port string */
	if(!root->port)
		sprintf (port_s, get_default_client_port(client_protocol));
	else
		strcpy(port_s,root->port);

    /* use a lower case hostname since we know hostnames are case insensitive */
    /* Some logic says we should be tacking our domain name on too if it isn't
     * there already, but for now this works.  Reverse->Forward lookups are
     * almost certainly too much since that would make CVS immune to some of
     * the DNS trickery that makes life easier for sysadmins when they want to
     * move a repository or the like
     */
    p = hostname = xstrdup(root->hostname);
    while (*p)
    {
	*p = tolower(*p);
	p++;
    }

    /* get the username string */
    username = root->username ? root->username : getcaller();
    cvsroot_canonical = (char*)xmalloc ( strlen(root->method) + strlen(username)
				+ strlen(hostname) + strlen(port_s)
				+ strlen(root->directory) + 12);
	sprintf (cvsroot_canonical, ":%s:%s@%s:%s:%s",
		 root->method, username, hostname, port_s, root->directory);

    xfree (hostname);
    return cvsroot_canonical;
}

/* allocate and return a cvsroot structure set up as if we're using the local
 * repository DIR.  */
cvsroot *local_cvsroot (const char *dir, const char *real_dir,bool readwrite, RootType type, const char *remote_serv, const char *remote_repos, const char *proxy_repos, const char *remote_pass)
{
    cvsroot *newroot = new_cvsroot_t();

	newroot->original = xstrdup(dir?dir:"");

	newroot->directory = xstrdup(real_dir&&*real_dir?real_dir:newroot->original);

	newroot->unparsed_directory = xstrdup(newroot->original);

	newroot->readwrite = readwrite;
	newroot->type = type;
	newroot->remote_server = xstrdup(remote_serv);
	newroot->remote_repository = xstrdup(remote_repos);
	newroot->proxy_repository_root = xstrdup(proxy_repos);
	newroot->remote_passphrase = xstrdup(remote_pass);

	if(real_dir && *real_dir)
    {
		struct saved_cwd cwd;
		save_cwd(&cwd);
		if(CVS_CHDIR(newroot->directory)<0)
			error(1,errno,"Repository directory %s does not exist",newroot->directory);
		newroot->mapped_directory = xgetwd();
		if(!fncmp(newroot->mapped_directory,newroot->directory))
			xfree(newroot->mapped_directory); /* Saves a step if there's no mapping required */
		else
			TRACE(3,"mapping %s -> %s",PATCH_NULL(newroot->mapped_directory),PATCH_NULL(newroot->directory));
		restore_cwd(&cwd, NULL);
    }

    return newroot;
}

#ifdef DEBUG
/* This is for testing the parsing function.  Use

     gcc -I. -I.. -I../lib -DDEBUG root.c -o root

   to compile.  */

#include <stdio.h>

char *program_name = "testing";
char *command_name = "parse_cvsroot";		/* XXX is this used??? */

/* Toy versions of various functions when debugging under unix.  Yes,
   these make various bad assumptions, but they're pretty easy to
   debug when something goes wrong.  */

void
error_exit ()
{
    exit (1);
}

int isabsolute (const char *dir)
{
    return (dir && (ISDIRSEP(*dir) || (*(dir+1)==':'));
}

void main (int argc, char *argv[])
{
    program_name = argv[0];

    if (argc != 2)
    {
	fprintf (stderr, "Usage: %s <CVSROOT>\n", program_name);
	exit (2);
    }
  
    if ((current_parsed_root = parse_cvsroot (argv[1])) == NULL)
    {
	fprintf (stderr, "%s: Parsing failed.\n", program_name);
	exit (1);
    }
    printf ("CVSroot: %s\n", argv[1]);
    printf ("current_parsed_root->method: %s\n", method_names[current_parsed_root->method]);
    printf ("current_parsed_root->username: %s\n",
	    current_parsed_root->username ? current_parsed_root->username : "NULL");
    printf ("current_parsed_root->hostname: %s\n",
	    current_parsed_root->hostname ? current_parsed_root->hostname : "NULL");
    printf ("current_parsed_root->directory: %s\n", current_parsed_root->directory);

   exit (0);
   /* NOTREACHED */
}
#endif
