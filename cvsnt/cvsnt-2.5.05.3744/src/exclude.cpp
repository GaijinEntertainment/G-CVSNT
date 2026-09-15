/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/*
 * .cvsexclude: local-only exclusion of repository content.
 *
 * A .cvsexclude file has the .cvsignore syntax and applies to the working
 * directory it is in; ~/.cvsexclude and $CVSEXCLUDE apply everywhere.  A
 * file or directory whose name matches is kept out of the working copy: it
 * is removed locally unless it has local changes, it is never sent to the
 * server, and nothing the server sends about it is written or shown.  The
 * server never learns about the list.  .cvsexclude itself is always
 * excluded, so it can neither be checked in nor overwritten from the server.
 */

#include "cvs.h"
#include "getline.h"
#include "savecwd.h"
#include <string>
#include <vector>
#include <map>

typedef std::vector<std::string> excl_list;

static excl_list excl_global;
static std::map<std::string, excl_list> excl_dirs;
static bool excl_active;

/* Directory keys must compare equal whatever separator or case a caller used.  */
static std::string excl_key (const char *path)
{
    std::string key;
    for (const char *p = path; *p; ++p)
    {
        char c = ISDIRSEP (*p) ? '/' : *p;
        if (filenames_case_insensitive)
            c = (char) tolower ((unsigned char) c);
        key += c;
    }
    while (key.size () > 1 && key[key.size () - 1] == '/')
        key.erase (key.size () - 1);
    return key;
}

static std::string excl_join (const std::string &dir, const char *name)
{
    if (dir.empty ())
        return name;
    if (ISDIRSEP (dir[dir.size () - 1]))
        return dir + name;
    return dir + "/" + name;
}

/* Position of the last directory separator in PATH, or npos.  */
static size_t excl_last_sep (const std::string &path)
{
    for (size_t i = path.size (); i > 0; --i)
        if (ISDIRSEP (path[i - 1]))
            return i - 1;
    return std::string::npos;
}

static void excl_add_line (excl_list &list, const char *line)
{
    if (line == NULL)
        return;
    while (*line)
    {
        char *tok = ign_next_token (&line);
        if (tok == NULL)
            break;
        if (tok[0] == '!' && tok[1] == 0)
            list.clear ();
        else if (tok[0] != 0)
            list.push_back (tok);
        xfree (tok);
    }
}

static void excl_add_file (excl_list &list, const char *file)
{
    FILE *fp = fopen (file, "r");
    char *line = NULL;
    size_t line_allocated = 0;

    if (fp == NULL)
        return;
    while (getline (&line, &line_allocated, fp) >= 0)
        excl_add_line (list, line);
    if (ferror (fp))
        error (0, errno, "cannot read %s", file);
    fclose (fp);
    xfree (line);
}

/* Patterns in force for the entries of the absolute directory ABSDIR.  */
static const excl_list &excl_patterns (const std::string &absdir)
{
    std::string key = excl_key (absdir.c_str ());
    std::map<std::string, excl_list>::iterator it = excl_dirs.find (key);
    if (it != excl_dirs.end ())
        return it->second;
    excl_list &list = excl_dirs[key];
    list = excl_global;
    excl_add_file (list, excl_join (absdir, CVSDOTEXCLUDE).c_str ());
    return list;
}

static bool excl_match (const excl_list &list, const char *name)
{
    if (fncmp (name, CVSDOTEXCLUDE) == 0)
        return true;
    for (size_t i = 0; i < list.size (); ++i)
        if (CVS_FNMATCH (list[i].c_str (), name, CVS_CASEFOLD) == 0)
            return true;
    return false;
}

static bool excl_name_abs (const std::string &absdir, const char *name)
{
    return excl_match (excl_patterns (absdir), name);
}

static std::string excl_cwd ()
{
    char *wd = xgetwd ();
    if (wd == NULL)
        error (1, errno, "could not get working directory");
    std::string s = wd;
    xfree (wd);
    return s;
}

/* Server pathnames and message paths are relative to the directory the
   command started in, which the client records in toplevel_wd.  */
static std::string excl_top ()
{
    if (toplevel_wd != NULL)
        return toplevel_wd;
    return excl_cwd ();
}

/* Walk PATH component by component below ABS; return true when a component
   is excluded by the directory holding it.  */
static bool excl_walk (std::string abs, const char *path)
{
    const char *p = path;
    while (*p)
    {
        const char *e = p;
        while (*e && !ISDIRSEP (*e))
            ++e;
        std::string comp (p, e - p);
        if (comp == "..")
        {
            size_t slash = excl_last_sep (abs);
            if (slash != std::string::npos && slash > 0)
                abs.erase (slash);
        }
        else if (!comp.empty () && comp != ".")
        {
            if (excl_name_abs (abs, comp.c_str ()))
                return true;
            abs = excl_join (abs, comp.c_str ());
        }
        p = *e ? e + 1 : e;
    }
    return false;
}

int excl_name_here (const char *name)
{
    if (!excl_active || name == NULL)
        return 0;
    return excl_name_abs (excl_cwd (), name);
}

int excl_name_in (const char *dir, const char *name)
{
    if (!excl_active || name == NULL)
        return 0;
    std::string abs = excl_top ();
    if (dir != NULL && excl_walk (abs, dir))
        return 1;
    for (const char *p = dir ? dir : ""; *p; )
    {
        const char *e = p;
        while (*e && !ISDIRSEP (*e))
            ++e;
        std::string comp (p, e - p);
        if (!comp.empty () && comp != ".")
            abs = excl_join (abs, comp.c_str ());
        p = *e ? e + 1 : e;
    }
    return excl_name_abs (abs, name);
}

int excl_path (const char *path)
{
    if (!excl_active || path == NULL)
        return 0;
    return excl_walk (excl_top (), path);
}

/* The same test send_fileproc uses: timestamps first, then the stored md5.  */
int excl_file_is_modified (struct file_info *finfo, Vers_TS *vers)
{
    if (vers->ts_user == NULL)
        return 0;
    if (vers->ts_rcs != NULL && strcmp (vers->ts_user, vers->ts_rcs) == 0)
        return 0;
    if (vers->entdata == NULL || vers->entdata->md5 == NULL || !vers->entdata->md5[0])
        return 1;

    char *buf = NULL;
    size_t bufsize = 0, len = 0;
    kflag kf;
    int modified = 1;

    RCS_get_kflags (vers->options, false, kf);
    get_file (finfo->file, vers->entdata->user, "r", &buf, &bufsize, &len, kf);
    if (len)
    {
        CMD5Calc md5;
        md5.Update (buf, len);
        modified = strcmp (md5.Final (), vers->entdata->md5) != 0;
    }
    xfree (buf);
    return modified;
}

static void excl_report_removed (const char *what)
{
    if (!quiet)
        error (0, 0, "%s is excluded by %s; removed from the working copy", what, CVSDOTEXCLUDE);
}

static void excl_report_kept (const char *what)
{
    error (0, 0, "%s is excluded by %s but has local changes; not removed", what, CVSDOTEXCLUDE);
}

/* Take the excluded file FINFO out of the working copy.  A file the server
   knows nothing about is left alone; a modified file is kept and reported.  */
void excl_remove_file (struct file_info *finfo, Vers_TS *vers)
{
    if (!excl_active || noexec || vers->vn_user == NULL || finfo->entries == NULL)
        return;
    if (vers->ts_user == NULL || fncmp (finfo->file, CVSDOTEXCLUDE) == 0)
    {
        Scratch_Entry (finfo->entries, finfo->file);
        return;
    }
    if (excl_file_is_modified (finfo, vers))
    {
        excl_report_kept (finfo->fullname);
        return;
    }
    if (unlink_file (finfo->file) < 0)
    {
        error (0, errno, "cannot remove %s", finfo->fullname);
        return;
    }
    Scratch_Entry (finfo->entries, finfo->file);
    excl_report_removed (finfo->fullname);
}

/* Remove DIR, an excluded subdirectory of the current directory, one
   version-controlled file at a time.  Files with local changes and files
   not under version control are kept, and then so is the directory.
   Returns nonzero when the directory is gone.  */
static int excl_remove_tree (const char *dir, const char *update_dir, List *parent_entries)
{
    struct saved_cwd cwd;
    List *entries;
    int kept = 0;

    if (save_cwd (&cwd))
        error_exit ();
    if (CVS_CHDIR (dir) < 0)
    {
        error (0, errno, "cannot change directory to %s", update_dir);
        free_cwd (&cwd);
        return 0;
    }

    entries = Entries_Open (0, update_dir);
    if (entries != NULL)
    {
        std::vector<std::string> subdirs, files;
        for (Node *p = entries->list->next; p != entries->list; p = p->next)
            (((Entnode *) p->data)->type == ENT_SUBDIR ? subdirs : files).push_back (p->key);

        for (size_t i = 0; i < subdirs.size (); ++i)
        {
            std::string sub_update = excl_join (update_dir, subdirs[i].c_str ());
            if (!isdir (subdirs[i].c_str ()))
                Subdir_Deregister (entries, (char *) NULL, subdirs[i].c_str ());
            else if (!excl_remove_tree (subdirs[i].c_str (), sub_update.c_str (), entries))
                kept = 1;
        }

        for (size_t i = 0; i < files.size (); ++i)
        {
            struct file_info finfo;
            std::string full = excl_join (update_dir, files[i].c_str ());

            memset (&finfo, 0, sizeof finfo);
            finfo.file = files[i].c_str ();
            finfo.update_dir = update_dir;
            finfo.fullname = full.c_str ();
            finfo.entries = entries;

            Vers_TS *vers = Version_TS (&finfo, NULL, NULL, NULL, 0, 0, 0);
            if (vers->ts_user != NULL && excl_file_is_modified (&finfo, vers))
            {
                excl_report_kept (full.c_str ());
                kept = 1;
            }
            else if (vers->ts_user != NULL && unlink_file (finfo.file) < 0)
            {
                error (0, errno, "cannot remove %s", full.c_str ());
                kept = 1;
            }
            else
                Scratch_Entry (entries, finfo.file);
            freevers_ts (&vers);
        }
        Entries_Close (entries);
    }

    if (restore_cwd (&cwd, NULL))
        error_exit ();
    free_cwd (&cwd);

    if (kept || !isemptydir (dir, 0))
        return 0;
    if (unlink_file_dir (dir) < 0)
    {
        error (0, errno, "cannot remove %s", update_dir);
        return 0;
    }
    Subdir_Deregister (parent_entries, (char *) NULL, dir);
    return 1;
}

/* Take the excluded directory DIR (UPDATE_DIR for messages) out of the
   working copy; ENTRIES is the list of the directory holding it.  */
void excl_remove_dir (const char *dir, const char *update_dir, List *entries)
{
    if (!excl_active || noexec)
        return;
    if (!isdir (dir))
    {
        /* Removed by hand earlier: forget the directory too.  */
        if (entries != NULL && findnode_fn (entries, dir) != NULL)
            Subdir_Deregister (entries, (char *) NULL, dir);
        return;
    }
    if (excl_remove_tree (dir, update_dir, entries))
        excl_report_removed (update_dir);
    else
        error (0, 0, "%s is excluded by %s but was not removed: it has local changes or files not under version control", update_dir, CVSDOTEXCLUDE);
}

/* A command started inside a directory that an enclosing working directory
   excludes would update excluded content, so refuse it.  */
static void excl_check_ancestors ()
{
    if (!isdir (CVSADM))
        return;
    std::string cur = excl_cwd ();
    for (;;)
    {
        size_t slash = excl_last_sep (cur);
        if (slash == std::string::npos || slash == 0)
            break;
        std::string parent = cur.substr (0, slash);
        std::string name = cur.substr (slash + 1);
        if (parent.size () == 2 && parent[1] == ':')
            parent += "/";
        if (name.empty () || !isdir (excl_join (parent, CVSADM).c_str ()))
            break;
        if (excl_name_abs (parent, name.c_str ()))
            error (1, 0, "%s is excluded by %s in %s", cur.c_str (), CVSDOTEXCLUDE, parent.c_str ());
        cur = parent;
    }
}

void excl_setup (int uses_work_dir)
{
    excl_close ();
    excl_active = !server_active;
    if (!excl_active)
        return;

    char *home_dir = get_homedir ();
    if (home_dir)
        excl_add_file (excl_global, excl_join (home_dir, CVSDOTEXCLUDE).c_str ());
    excl_add_line (excl_global, CProtocolLibrary::GetEnvironment (EXCLUDE_ENV));

    if (uses_work_dir)
        excl_check_ancestors ();
}

void excl_close ()
{
    excl_global.clear ();
    excl_dirs.clear ();
    excl_active = false;
}
