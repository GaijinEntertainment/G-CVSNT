/* expand_path.c -- expand environmental variables in passed in string
 *
 * The main routine is expand_path(), it is the routine that handles
 * the '~' character in four forms: 
 *     ~name
 *     ~name/
 *     ~/
 *     ~
 * and handles environment variables contained within the pathname
 * which are defined by:
 *     ${var_name}   (var_name is the name of the environ variable)
 *     $var_name     (var_name ends w/ non-alphanumeric char other than '_')
 */

#include "cvs.h"
#include <sys/types.h>

static const char *expand_variable(const char *env, const char *file, int line);

/* User variables.  */
variable_list_t variable_list;

/* Currently used by -s option; we might want a way to set user
   variables in a file in the $CVSROOT/CVSROOT directory too.  */

void variable_set (const char *nameval)
{
    const char *p;
    char *name;

    p = nameval;
    while (isalnum ((unsigned char) *p) || *p == '_')
		++p;
    if (*p != '=')
	error (1, 0, "illegal character in user variable name in %s", nameval);
    if (p == nameval)
		error (1, 0, "empty user variable name in %s", nameval);
    name = (char*)xmalloc (p - nameval + 1);
    strncpy (name, nameval, p - nameval);
    name[p - nameval] = '\0';
    /* Make p point to the value.  */
    ++p;
    if (strchr (p, '\n') != NULL)
		error (1, 0, "linefeed in user variable value in %s", nameval);

	variable_list[name]=p;
}

const char *variable_get(const char *nameval)
{
	variable_list_t::const_iterator i = variable_list.find(nameval);
	if(i==variable_list.end())
		return NULL;
	return i->second.c_str();
}

/* This routine will expand the pathname to account for ~ and $
   characters as described above.  Returns a pointer to a newly
   malloc'd string.  If an error occurs, an error message is printed
   via error() and NULL is returned.  FILE and LINE are the filename
   and linenumber to include in the error message.  FILE must point
   to something; LINE can be zero to indicate the line number is not
   known.  */
char *expand_path (const char *name, const char *file, int line)
{
    const char *s;
    char *d;

    char *mybuf = NULL;
    size_t mybuf_size = 0;
    char *buf = NULL;
    size_t buf_size = 0;

    size_t doff;

    char *result;

    /* Sorry this routine is so ugly; it is a head-on collision
       between the `traditional' unix *d++ style and the need to
       dynamically allocate.  It would be much cleaner (and probably
       faster, not that this is a bottleneck for CVS) with more use of
       strcpy & friends, but I haven't taken the effort to rewrite it
       thusly.  */

    /* First copy from NAME to MYBUF, expanding $<foo> as we go.  */
    s = name;
    d = mybuf;
    doff = d - mybuf;
    expand_string (&mybuf, &mybuf_size, doff + 1);
    d = mybuf + doff;
    while ((*d++ = *s))
    {
	if (*s++ == '$')
	{
	    char *p = d;
	    const char *e;
	    int flag = (*s == '{');

	    doff = d - mybuf;
	    expand_string (&mybuf, &mybuf_size, doff + 1);
	    d = mybuf + doff;
	    for (; (*d++ = *s); s++)
	    {
		if (flag
		    ? *s =='}'
		    : isalnum ((unsigned char) *s) == 0 && *s != '_')
		    break;
		doff = d - mybuf;
		expand_string (&mybuf, &mybuf_size, doff + 1);
		d = mybuf + doff;
	    }
	    *--d = '\0';
	    e = expand_variable (&p[flag], file, line);

	    if (e)
	    {
		doff = d - mybuf;
		expand_string (&mybuf, &mybuf_size, doff + 1);
		d = mybuf + doff;
		for (d = &p[-1]; (*d++ = *e++);)
		{
		    doff = d - mybuf;
		    expand_string (&mybuf, &mybuf_size, doff + 1);
		    d = mybuf + doff;
		}
		--d;
		if (flag && *s)
		    s++;
	    }
	    else
		/* expand_variable has already printed an error message.  */
		goto error_exit;
	}
	doff = d - mybuf;
	expand_string (&mybuf, &mybuf_size, doff + 1);
	d = mybuf + doff;
    }
    doff = d - mybuf;
    expand_string (&mybuf, &mybuf_size, doff + 1);
    d = mybuf + doff;
    *d = '\0';

    /* Then copy from MYBUF to BUF, expanding ~.  */
    s = mybuf;
    d = buf;
    /* If you don't want ~username ~/ to be expanded simply remove
     * This entire if statement including the else portion
     */
    if (*s++ == '~')
    {
		const char *t;
		const char *p=s;
		if (*s=='/' || *s==0)
			t = get_homedir ();
		else
		{
#ifdef GETPWNAM_MISSING
			if (line != 0)
				error (0, 0, "%s:%d:tilde expansion not supported on this system", file, line);
			else
				error (0, 0, "%s:tilde expansion not supported on this system", file);
			return NULL;
#else
			struct passwd *ps;
			char *u;
			for (; *p!='/' && *p; p++)
				;
			u=(char*)xmalloc((p-s)+1);
			strncpy(u,s,p-s);
			u[p-s]='\0';
			ps = getpwnam (u);
			if (ps == 0)
			{
				if (line != 0)
					error (0, 0, "%s:%d: no such user %s", file, line, u);
				else
					error (0, 0, "%s: no such user %s", file, u);
				xfree(u);
				return NULL;
			}
			xfree(u);
			t = ps->pw_dir;
	#endif
		}
		if (t == NULL)
			error (1, 0, "cannot find home directory");

		doff = d - buf;
		expand_string (&buf, &buf_size, doff + 1);
		d = buf + doff;
		while ((*d++ = *t++))
		{
			doff = d - buf;
			expand_string (&buf, &buf_size, doff + 1);
			d = buf + doff;
		}
		--d;
		s=p;
    }
    else
	--s;
	/* Kill up to here */
    doff = d - buf;
    expand_string (&buf, &buf_size, doff + 1);
    d = buf + doff;
    while ((*d++ = *s++))
    {
	doff = d - buf;
	expand_string (&buf, &buf_size, doff + 1);
	d = buf + doff;
    }
    doff = d - buf;
    expand_string (&buf, &buf_size, doff + 1);
    d = buf + doff;
    *d = '\0';

    /* OK, buf contains the value we want to return.  Clean up and return
       it.  */
    xfree (mybuf);
    /* Save a little memory with xstrdup; buf will tend to allocate
       more than it needs to.  */
    result = xstrdup (buf);
    xfree (buf);
    return result;

 error_exit:
    if (mybuf != NULL)
	xfree (mybuf);
    if (buf != NULL)
	xfree (buf);
    return NULL;
}

// Since Win32 can't do getppid from the child, send our pid out so
// scripts can use it
char *cvspid()
{
	static char tmp[32];
	sprintf(tmp,"%08x",(int)getpid());
	return tmp;
}

static const char *expand_variable (const char *name, const char *file, int line)
{
	const char *p;
    if (strcmp (name, CVSROOT_ENV) == 0)
		return current_parsed_root->original;
    else if (strcmp (name, "RCSBIN") == 0)
    {
		error (0, 0, "RCSBIN internal variable is no longer supported");
		return NULL;
    }
    else if (strcmp (name, EDITOR1_ENV) == 0)
		return Editor;
    else if (strcmp (name, EDITOR2_ENV) == 0)
		return Editor;
    else if (strcmp (name, EDITOR3_ENV) == 0)
		return Editor;
    else if (strcmp (name, "USER") == 0)
		return getcaller ();
	else if (strcmp (name, "CVSPID") == 0)
		return cvspid();
	else if (strcmp (name, "SESSIONID") == 0)
		return global_session_id;
	else if (strcmp (name, "COMMITID") == 0)
		return global_session_id;
    else if(isalpha((unsigned char)name[0]))
	{
		if((p=CProtocolLibrary::GetEnvironment(name))!=NULL)
			return p;
		if((p=variable_get(name))!=NULL)
			return p;
		/* These names are reserved for future versions of CVS,
		so that is why it is an error.  */
		if (line != 0)
			error (0, 0, "%s:%d: no such internal variable $%s", file, line, name);
		else
			error (0, 0, "%s: no such internal variable $%s", file, name);
		return NULL;
	}
    else if (name[0] == '=')
    {
		/* Crazy syntax for a user variable.  But we want
		*something* that lets the user name a user variable
		anything he wants, without interference from
		(existing or future) internal variables.  */
		if((p=variable_get(name+1))!=NULL)
			return p;
	    if (line != 0)
			error (0, 0, "%s:%d: no such user variable ${%s}", file, line, name);
	    else
			error (0, 0, "%s: no such user variable ${%s}", file, name);
	    return NULL;
	}
    else
    {
		/* It is an unrecognized character.  We return an error to
		reserve these for future versions of CVS; it is plausible
		that various crazy syntaxes might be invented for inserting
		information about revisions, branches, etc.  */
		if (line != 0)
			error (0, 0, "%s:%d: unrecognized variable syntax %s", file, line, name);
		else
			error (0, 0, "%s: unrecognized variable syntax %s", file, name);
		return NULL;
    }
}
