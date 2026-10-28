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
 * file or directory whose name matches is kept out of the working copy:
 * update removes it unless it has local changes or is scheduled for removal,
 * it is never sent to the server, and nothing the server sends about it is
 * written or shown.  The server never learns about the list.  .cvsexclude itself is always
 * excluded, so it can neither be checked in nor overwritten from the server.
 */

#include "cvs.h"
#include "getline.h"
#include "savecwd.h"
#include <string>
#include <vector>
#include <map>

typedef std::vector<std::string> excl_list;

/* What a directory's .cvsexclude contributes: its own patterns, and whether a
   "!" in it cleared the global patterns for that directory.  */
struct excl_dir
{
    bool reset;
    excl_list own;
};

static excl_list excl_global;
static std::map<std::string, excl_dir> excl_dirs;
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

/* A single entry name, as opposed to a path, ".", "..", or nothing.  */
static bool excl_simple (const char *name)
{
    if (name[0] == 0 || strcmp (name, ".") == 0 || strcmp (name, "..") == 0)
        return false;
    for (const char *p = name; *p; ++p)
        if (ISDIRSEP (*p))
            return false;
    return true;
}

static void excl_add_line (excl_list &list, bool *reset, const char *line)
{
    if (line == NULL)
        return;
    while (*line)
    {
        char *tok = ign_next_token (&line);
        if (tok == NULL)
            break;
        if (tok[0] == '!' && tok[1] == 0)
        {
            list.clear ();
            if (reset != NULL)
                *reset = true;
        }
        else if (tok[0] != 0)
            list.push_back (tok);
        xfree (tok);
    }
}

static void excl_add_file (excl_list &list, bool *reset, const char *file)
{
    FILE *fp = fopen (file, "r");
    char *line = NULL;
    size_t line_allocated = 0;

    if (fp == NULL)
        return;
    while (getline (&line, &line_allocated, fp) >= 0)
        excl_add_line (list, reset, line);
    if (ferror (fp))
        error (0, errno, "cannot read %s", file);
    fclose (fp);
    xfree (line);
}

/* The .cvsexclude of the absolute directory ABSDIR, read once.  */
static const excl_dir &excl_dir_at (const std::string &absdir)
{
    std::string key = excl_key (absdir.c_str ());
    std::map<std::string, excl_dir>::iterator it = excl_dirs.find (key);
    if (it != excl_dirs.end ())
        return it->second;
    excl_dir &dir = excl_dirs[key];
    excl_add_file (dir.own, &dir.reset, excl_join (absdir, CVSDOTEXCLUDE).c_str ());
    return dir;
}

static bool excl_match_list (const excl_list &list, const char *name)
{
    for (size_t i = 0; i < list.size (); ++i)
        if (CVS_FNMATCH (list[i].c_str (), name, CVS_CASEFOLD) == 0)
            return true;
    return false;
}

static bool excl_match (const excl_dir &dir, const char *name)
{
    if (fncmp (name, CVSDOTEXCLUDE) == 0)
        return true;
    return (!dir.reset && excl_match_list (excl_global, name)) || excl_match_list (dir.own, name);
}

static bool excl_name_abs (const std::string &absdir, const char *name)
{
    return excl_match (excl_dir_at (absdir), name);
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

/* Walk PATH component by component below ABS; return true when a component
   is excluded by the directory holding it.  Otherwise *END, when given, gets
   the absolute directory PATH leads to.  */
static bool excl_walk (std::string abs, const char *path, std::string *end = NULL)
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
    if (end != NULL)
        *end = abs;
    return false;
}

static std::map<std::string, std::string> excl_inside_memo;

/* The directory that an enclosing working directory excludes and that the
   absolute directory DIR is or lies inside, or an empty string.  */
static const std::string &excl_inside (const std::string &dir)
{
    std::string key = excl_key (dir.c_str ());
    std::map<std::string, std::string>::iterator it = excl_inside_memo.find (key);
    if (it != excl_inside_memo.end ())
        return it->second;
    std::string where;
    size_t slash = excl_last_sep (dir);
    if (slash != std::string::npos && slash > 0)
    {
        std::string up = dir.substr (0, slash);
        std::string name = dir.substr (slash + 1);
        if (!name.empty () && isdir (excl_join (up, CVSADM).c_str ()))
            where = excl_name_abs (up, name.c_str ()) ? dir : excl_inside (up);
    }
    return excl_inside_memo[key] = where;
}

/* The current directory as last looked up, identified by device and inode so
   that it is looked up again only after a change of directory.  Where stat
   reports no inode numbers (st_ino 0) it is looked up every time, and "." is
   not probed again.  */
static bool excl_here_valid;
static bool excl_no_inodes;
static dev_t excl_here_dev;
static ino_t excl_here_ino;
static std::string excl_here_abs;
static const excl_dir *excl_here_dir;
static bool excl_here_inside;

static void excl_here ()
{
    struct stat st;
    bool known = false;
    if (!excl_no_inodes && CVS_STAT (".", &st) == 0)
    {
        known = st.st_ino != 0;
        excl_no_inodes = !known;
    }
    if (known && excl_here_valid && st.st_dev == excl_here_dev && st.st_ino == excl_here_ino)
        return;
    excl_here_abs = excl_cwd ();
    excl_here_dir = &excl_dir_at (excl_here_abs);
    excl_here_inside = !excl_inside (excl_here_abs).empty ();
    excl_here_valid = known;
    if (known)
    {
        excl_here_dev = st.st_dev;
        excl_here_ino = st.st_ino;
    }
}

/* Server pathnames and message paths are relative to the directory the
   command started in, which the client records in toplevel_wd at the first
   server response; until then they are relative to the current directory.  */
static std::string excl_top ()
{
    if (toplevel_wd != NULL)
        return toplevel_wd;
    excl_here ();
    return excl_here_abs;
}

/* The patterns for the entries of the directory REL, relative to the top, or
   NULL when that directory is excluded itself; remembered per directory for
   as long as the top stays the same.  */
static std::map<std::string, const excl_dir *> excl_rel_memo;
static std::string excl_rel_top;

static const excl_dir *excl_rel (const char *rel)
{
    std::string top = excl_top ();
    if (top != excl_rel_top)
    {
        excl_rel_memo.clear ();
        excl_rel_top = top;
    }
    std::string key = excl_key (rel);
    std::map<std::string, const excl_dir *>::iterator it = excl_rel_memo.find (key);
    if (it != excl_rel_memo.end ())
        return it->second;
    std::string abs;
    const excl_dir *dir = excl_walk (top, rel, &abs) ? NULL : &excl_dir_at (abs);
    return excl_rel_memo[key] = dir;
}

/* NAME, a path relative to the current directory, is excluded, or the
   current directory itself is inside an excluded directory.  */
int excl_name_here (const char *name)
{
    if (!excl_active || name == NULL)
        return 0;
    excl_here ();
    if (excl_here_inside)
        return 1;
    if (excl_simple (name))
        return excl_match (*excl_here_dir, name);
    return excl_walk (excl_here_abs, name);
}

int excl_name_in (const char *dir, const char *name)
{
    if (!excl_active || name == NULL)
        return 0;
    return excl_path (excl_join (dir != NULL ? dir : "", name).c_str ());
}

int excl_path (const char *path)
{
    if (!excl_active || path == NULL)
        return 0;
    std::string p (path);
    while (!p.empty () && ISDIRSEP (p[p.size () - 1]))
        p.erase (p.size () - 1);
    size_t slash = excl_last_sep (p);
    std::string name = slash == std::string::npos ? p : p.substr (slash + 1);
    if (!excl_simple (name.c_str ()))
        return excl_walk (excl_top (), path);
    const excl_dir *dir = excl_rel (slash == std::string::npos ? "" : p.substr (0, slash).c_str ());
    return dir == NULL || excl_match (*dir, name.c_str ());
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

static void excl_report_kept (const char *what, const char *why)
{
    error (0, 0, "%s is excluded by %s but %s; not removed", what, CVSDOTEXCLUDE, why);
}

/* A file `cvs remove` scheduled for removal, not committed yet.  */
static bool excl_scheduled_for_removal (const Vers_TS *vers)
{
    return vers->vn_user != NULL && vers->vn_user[0] == '-';
}

/* During update, take the excluded file FINFO out of the working copy.  A
   file the server knows nothing about is left alone; a modified file or one
   scheduled for removal is kept and reported.  */
void excl_remove_file (struct file_info *finfo, Vers_TS *vers)
{
    if (!excl_active || strcmp (command_name, "update") != 0 || noexec || vers->vn_user == NULL
        || finfo->entries == NULL)
        return;
    if (excl_scheduled_for_removal (vers))
    {
        excl_report_kept (finfo->fullname, "is scheduled for removal");
        return;
    }
    if (vers->ts_user == NULL || fncmp (finfo->file, CVSDOTEXCLUDE) == 0)
    {
        Scratch_Entry (finfo->entries, finfo->file);
        return;
    }
    if (excl_file_is_modified (finfo, vers))
    {
        excl_report_kept (finfo->fullname, "has local changes");
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
   version-controlled file at a time.  Files with local changes, files
   scheduled for removal and files not under version control are kept, and then so is
   the directory.
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
            if (excl_scheduled_for_removal (vers))
            {
                excl_report_kept (full.c_str (), "is scheduled for removal");
                kept = 1;
            }
            else if (excl_file_is_modified (&finfo, vers))
            {
                excl_report_kept (full.c_str (), "has local changes");
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

/* During update, take the excluded directory DIR (UPDATE_DIR for messages)
   out of the working copy; ENTRIES is the list of the directory holding it.  */
void excl_remove_dir (const char *dir, const char *update_dir, List *entries)
{
    if (!excl_active || strcmp (command_name, "update") != 0 || noexec)
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
        error (0, 0, "%s is excluded by %s but was not removed: it has local changes, files scheduled for removal or files not under version control", update_dir, CVSDOTEXCLUDE);
}

/* A command started inside a directory that an enclosing working directory
   excludes would update excluded content, so refuse it.  */
static void excl_check_ancestors ()
{
    if (!isdir (CVSADM))
        return;
    std::string where = excl_inside (excl_cwd ());
    if (where.empty ())
        return;
    std::string parent = where.substr (0, excl_last_sep (where));
    if (parent.size () == 2 && parent[1] == ':')
        parent += "/";
    error (1, 0, "%s is excluded by %s in %s", where.c_str (), CVSDOTEXCLUDE, parent.c_str ());
}

void excl_setup (int uses_work_dir)
{
    excl_close ();
    excl_active = !server_active;
    if (!excl_active)
        return;

    char *home_dir = get_homedir ();
    if (home_dir)
        excl_add_file (excl_global, NULL, excl_join (home_dir, CVSDOTEXCLUDE).c_str ());
    excl_add_line (excl_global, NULL, CProtocolLibrary::GetEnvironment (EXCLUDE_ENV));

    if (uses_work_dir)
        excl_check_ancestors ();
}

void excl_close ()
{
    excl_global.clear ();
    excl_dirs.clear ();
    excl_inside_memo.clear ();
    excl_rel_memo.clear ();
    excl_rel_top.clear ();
    excl_here_valid = false;
    excl_active = false;
}
