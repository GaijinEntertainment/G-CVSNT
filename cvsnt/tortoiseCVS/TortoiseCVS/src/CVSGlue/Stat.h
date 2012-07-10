// Taken from WinCVS's stdafx.h file

#ifndef TORTOISECVS_STAT_H
#define TORTOISECVS_STAT_H

#include <sys/stat.h>

#ifdef __BORLANDC__
  #define _dev_t dev_t
  #define _ino_t ino_t
  #define _off_t off_t
#endif

// Replacement stat() routine
struct wnt_stat {
        _dev_t st_dev;
        _ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        _dev_t st_rdev;
        _off_t st_size;
        time_t st_atime;
        time_t st_mtime;
        time_t st_ctime;
        unsigned int dwFileAttributes;
        };
int wnt_stat(const char *name, struct wnt_stat *buf);
int wnt_fstat (int fildes, struct wnt_stat *buf);
int wnt_lstat (const char *name, struct wnt_stat *buf);

#define fstat wnt_fstat
#define lstat wnt_lstat
#define stat wnt_stat

#endif
