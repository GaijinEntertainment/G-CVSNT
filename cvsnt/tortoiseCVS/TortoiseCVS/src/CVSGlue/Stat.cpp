// Taken wholesale from WinCVS for Daylight Saving fix.
// (apart from GetWinCvsDebugMaskBit commented out, and
// various header file inclusion changes etc.)

#include "StdAfx.h"
#include "Stat.h"
#include <errno.h>
#include <io.h>
#include <windows.h>
#include "../Utils/FixWinDefs.h"
#include <limits.h>
#include "../Utils/TortoiseDebug.h"

/* Try to work out if this is a GMT FS.  There is probably
   a way of doing this that works for all FS's, but this
   detects FAT at least */
#define GMT_FS(_s) (!strstr(_s,"FAT"))


static struct { DWORD err; int dos;} errors[] =
{
   {  ERROR_INVALID_FUNCTION,       EINVAL    },  /* 1 */
   {  ERROR_FILE_NOT_FOUND,         ENOENT    },  /* 2 */
   {  ERROR_PATH_NOT_FOUND,         ENOENT    },  /* 3 */
   {  ERROR_TOO_MANY_OPEN_FILES,    EMFILE    },  /* 4 */
   {  ERROR_ACCESS_DENIED,          EACCES    },  /* 5 */
   {  ERROR_INVALID_HANDLE,         EBADF     },  /* 6 */
   {  ERROR_ARENA_TRASHED,          ENOMEM    },  /* 7 */
   {  ERROR_NOT_ENOUGH_MEMORY,      ENOMEM    },  /* 8 */
   {  ERROR_INVALID_BLOCK,          ENOMEM    },  /* 9 */
   {  ERROR_BAD_ENVIRONMENT,        E2BIG     },  /* 10 */
   {  ERROR_BAD_FORMAT,             ENOEXEC   },  /* 11 */
   {  ERROR_INVALID_ACCESS,         EINVAL    },  /* 12 */
   {  ERROR_INVALID_DATA,           EINVAL    },  /* 13 */
   {  ERROR_INVALID_DRIVE,          ENOENT    },  /* 15 */
   {  ERROR_CURRENT_DIRECTORY,      EACCES    },  /* 16 */
   {  ERROR_NOT_SAME_DEVICE,        EXDEV     },  /* 17 */
   {  ERROR_NO_MORE_FILES,          ENOENT    },  /* 18 */
   {  ERROR_LOCK_VIOLATION,         EACCES    },  /* 33 */
   {  ERROR_BAD_NETPATH,            ENOENT    },  /* 53 */
   {  ERROR_NETWORK_ACCESS_DENIED,  EACCES    },  /* 65 */
   {  ERROR_BAD_NET_NAME,           ENOENT    },  /* 67 */
   {  ERROR_FILE_EXISTS,            EEXIST    },  /* 80 */
   {  ERROR_CANNOT_MAKE,            EACCES    },  /* 82 */
   {  ERROR_FAIL_I24,               EACCES    },  /* 83 */
   {  ERROR_INVALID_PARAMETER,      EINVAL    },  /* 87 */
   {  ERROR_NO_PROC_SLOTS,          EAGAIN    },  /* 89 */
   {  ERROR_DRIVE_LOCKED,           EACCES    },  /* 108 */
   {  ERROR_BROKEN_PIPE,            EPIPE     },  /* 109 */
   {  ERROR_DISK_FULL,              ENOSPC    },  /* 112 */
   {  ERROR_INVALID_TARGET_HANDLE,  EBADF     },  /* 114 */
   {  ERROR_INVALID_HANDLE,         EINVAL    },  /* 124 */
   {  ERROR_WAIT_NO_CHILDREN,       ECHILD    },  /* 128 */
   {  ERROR_CHILD_NOT_COMPLETE,     ECHILD    },  /* 129 */
   {  ERROR_DIRECT_ACCESS_HANDLE,   EBADF     },  /* 130 */
   {  ERROR_NEGATIVE_SEEK,          EINVAL    },  /* 131 */
   {  ERROR_SEEK_ON_DEVICE,         EACCES    },  /* 132 */
   {  ERROR_DIR_NOT_EMPTY,          ENOTEMPTY },  /* 145 */
   {  ERROR_NOT_LOCKED,             EACCES    },  /* 158 */
   {  ERROR_BAD_PATHNAME,           ENOENT    },  /* 161 */
   {  ERROR_MAX_THRDS_REACHED,      EAGAIN    },  /* 164 */
   {  ERROR_LOCK_FAILED,            EACCES    },  /* 167 */
   {  ERROR_ALREADY_EXISTS,         EEXIST    },  /* 183 */
   {  ERROR_FILENAME_EXCED_RANGE,   ENOENT    },  /* 206 */
   {  ERROR_NESTING_NOT_ALLOWED,    EAGAIN    },  /* 215 */
   {  ERROR_NOT_ENOUGH_QUOTA,       ENOMEM    }    /* 1816 */
};

static const WORD month_len [12] = 
{
   31, /* Jan */
   28, /* Feb */
   31, /* Mar */
   30, /* Apr */
   31, /* May */
   30, /* Jun */
   31, /* Jul */
   31, /* Aug */
   30, /* Sep */
   31, /* Oct */
   30, /* Nov */
   31  /* Dec */
};

/* One second = 10,000,000 * 100 nsec */
static const ULONGLONG systemtime_second = 10000000L;


static void _dosmaperr(DWORD dwErr)
{
   unsigned int n;
   for(n=0; n<sizeof(errors)/sizeof(errors[0]); n++)
   {
      if(errors[n].err==dwErr)
      {
         errno=errors[n].dos;
         return;
      }
   }
   errno=EFAULT;
}

/* Is the year a leap year?
 * 
 * Use standard Gregorian: every year divisible by 4
 * except centuries indivisible by 400.
 *
 * INPUTS: WORD year the year (AD)
 *
 * OUTPUTS: TRUE if it's a leap year.
 */
BOOL IsLeapYear( WORD year )
{
    return ((year & 3u) == 0)
        && ( ((year % 100u) == 0)
             || ((year % 400u) == 0) );
}

/* A fairly complicated comparison:
 *
 * Compares a test date against a target date.
 * If the test date is earlier, return a negative number
 * If the test date is later, return a positive number
 * If the two dates are equal, return zero.
 *
 * The comparison is complicated by the way we specify
 * TargetDate.
 *
 * TargetDate is assumed to be the kind of date used in 
 * TIME_ZONE_INFORMATION.DaylightDate: If wYear is 0,
 * then it's a kind of code for things like 1st Sunday 
 * of the month. As described in the Windows API docs,
 * wDay is an index of week: 
 *      1 = first of month
 *      2 = second of month
 *      3 = third of month
 *      4 = fourth of month
 *      5 = last of month (even if there are only four such days).
 *
 * Thus, if wYear = 0, wMonth = 4, wDay = 2, wDayOfWeek = 4
 * it specifies the second Thursday of April.
 *
 * INPUTS: SYSTEMTIME * p_test_date     The date to be tested
 *
 *         SYSTEMTIME * p_target_date   The target date. This should be in
 *                                      the format for a TIME_ZONE_INFORMATION
 *                                      DaylightDate or StandardDate.
 *
 * OUTPUT:  -4/+4 if test month is less/greater than target month
 *          -2/+2 if test day is less/greater than target day
 *          -1/+2 if test hour:minute:seconds.milliseconds less/greater than target
 *          0     if both dates/times are equal.
 * 
 */
static int CompareTargetDate (
    const SYSTEMTIME * p_test_date,
    const SYSTEMTIME * p_target_date
    )
{
    WORD first_day_of_month; /* day of week of the first. Sunday = 0 */
    WORD end_of_month;       /* last day of month */
    WORD temp_date;
    int test_milliseconds, target_milliseconds;

    /* Check that p_target_date is in the correct foramt: */
    if (p_target_date->wYear) 
    {
        return 0;
    }
    if (!p_test_date->wYear) 
    {
        return 0;
    }

    /* Don't waste time calculating if we can shortcut the comparison... */
    if (p_test_date->wMonth != p_target_date->wMonth) 
    {
        return (p_test_date->wMonth > p_target_date->wMonth) ? 4 : -4;
    }

    /* Months equal. Now we neet to do some calculation.
     * If we know that y is the day of the week for some arbitrary date x, 
     * then the day of the week of the first of the month is given by 
     * (1 + y - x) mod 7.
     * 
     * For instance, if the 19th is a Wednesday (day of week = 3), then
     * the date of the first Wednesday of the month is (19 mod 7) = 5.
     * If the 5th is a Wednesday (3), then the first of the month is 
     * four days earlier (it's the first, not the zeroth):
     * (3 - 4) = -1; -1 mod 7 = 6. The first is a Saturday.
     *
     * Check ourselves: The 19th falls on a (6 + 19 - 1) mod 7 
     * = 24 mod 7 = 3: Wednesday, as it should be.
     */
    first_day_of_month = (WORD)( (1u + p_test_date->wDayOfWeek - p_test_date->wDay) % 7u);

    /* If the first of the month comes on day y, then the first day of week z falls on
     * (z - y + 1) mod 7. 
     *
     * For instance, if the first is a Saturday (6), then the first Tuesday (2) falls on a
     * (2 - 6 + 1) mod 7 = -3 mod 7 = 4: The fourth. This is correct (see above).
     *
     * temp_date gets the first <target day of week> in the month.
     */
    temp_date = (WORD)( (1u + p_target_date->wDayOfWeek - first_day_of_month) % 7u);
    /* If we're looking for the third Tuesday in the month, find the date of the first
     * Tuesday and add (3 - 1) * 7. In the example, it's the 4 + 14 = 18th.
     *
     * temp_date now should hold the date for the wDay'th wDayOfWeek of the month.
     * we only need to handle the special case of the last <DayOfWeek> of the month.
     */
    temp_date = (WORD)( temp_date + 7 * p_target_date->wDay );

    /* what's the last day of the month? */
    end_of_month = month_len [p_target_date->wMonth - 1];
    /* Correct if it's February of a leap year? */
    if ( p_test_date->wMonth == 2 && IsLeapYear(p_test_date->wYear) ) 
    {
        ++ end_of_month;
    }

    /* if we tried to calculate the fifth Tuesday of the month
     * we may well have overshot. Correct for that case.
     */
    while ( temp_date > end_of_month)
        temp_date -= 7;

    /* At long last, we're ready to do the comparison. */
    if ( p_test_date->wDay != temp_date ) 
    {
        return (p_test_date->wDay > temp_date) ? 2 : -2;
    }
    else 
    {
        test_milliseconds = ((p_test_date->wHour * 60 + p_test_date->wMinute) * 60 
                                + p_test_date->wSecond) * 1000 + p_test_date->wMilliseconds;
        target_milliseconds = ((p_target_date->wHour * 60 + p_target_date->wMinute) * 60 
                                + p_target_date->wSecond) * 1000 + p_target_date->wMilliseconds;
        test_milliseconds -= target_milliseconds;
        return (test_milliseconds > 0) ? 1 : (test_milliseconds < 0) ? -1 : 0;
    }

}

//
//  Get time zone bias for local time *pst.
//
//  UTC time = *pst + bias.
//
static int GetTimeZoneBias( const SYSTEMTIME * pst )
{
    TIME_ZONE_INFORMATION tz;
    int n, bias;
    
    GetTimeZoneInformation ( &tz );

    /*  I only deal with cases where we look at 
     *  a "last sunday" kind of thing.
     */
    if (tz.DaylightDate.wYear || tz.StandardDate.wYear) 
    {
        return 0;
    }

    bias = tz.Bias;

    n = CompareTargetDate ( pst, & tz.DaylightDate );
    if (n < 0)
        bias += tz.StandardBias;
    else 
    {
        n = CompareTargetDate ( pst, & tz.StandardDate );
        if (n < 0)
            bias += tz.DaylightBias;
        else
            bias += tz.StandardBias;
    }
    return bias;
}

/* Convert a system time from local time to UTC time, correctly
 * taking DST into account (sort of).
 *
 * INPUTS:
 *      const SYSTEMTIME * p_local: 
 *              A file time. It may be in UTC or in local 
 *              time (see local_time, below, for details).
 *
 *      SYSTEMTIME * p_utc:
 *              The destination for the converted time.
 *
 * OUTPUTS:
 *      SYSTEMTIME * p_utc:         
 *              The destination for the converted time.
 */

void LocalSystemTimeToUtcSystemTime( const SYSTEMTIME * p_local, SYSTEMTIME * p_utc)
{
    TIME_ZONE_INFORMATION tz;
    FILETIME ft;
    ULARGE_INTEGER itime, delta;
    int bias;

    * p_utc = * p_local;

    GetTimeZoneInformation(&tz);
    bias = tz.Bias;
    if ( CompareTargetDate(p_local, & tz.DaylightDate) < 0
            || CompareTargetDate(p_local, & tz.StandardDate) >= 0)
        bias += tz.StandardBias;
    else
        bias += tz.DaylightBias;

    SystemTimeToFileTime(p_local, & ft);
    itime.QuadPart = ((ULARGE_INTEGER *) &ft)->QuadPart;
    delta.QuadPart = systemtime_second;
    delta.QuadPart *= 60;   // minute
    delta.QuadPart *= bias;
    itime.QuadPart += delta.QuadPart;
    ((ULARGE_INTEGER *) & ft)->QuadPart = itime.QuadPart;
    FileTimeToSystemTime(& ft, p_utc);
}


/* Convert a file time to a Unix time_t structure. This function is as 
 * complicated as it is because it needs to ask what time system the 
 * filetime describes.
 * 
 * INPUTS:
 *      const FILETIME * ft: A file time. It may be in UTC or in local 
 *                           time (see local_time, below, for details).
 *
 *      time_t * ut:         The destination for the converted time.
 *
 *      BOOL local_time:     TRUE if the time in *ft is in local time 
 *                           and I need to convert to a real UTC time.
 *
 * OUTPUTS:
 *      time_t * ut:         Store the result in *ut.
 */
static BOOL FileTimeToUnixTime ( const FILETIME* pft, time_t* put, BOOL local_time )
{
    BOOL success = FALSE;

   /* FILETIME = number of 100-nanosecond ticks since midnight 
    * 1 Jan 1601 UTC. time_t = number of 1-second ticks since 
    * midnight 1 Jan 1970 UTC. To translate, we subtract a
    * FILETIME representation of midnight, 1 Jan 1970 from the
    * time in question and divide by the number of 100-ns ticks
    * in one second.
    */

    SYSTEMTIME base_st = 
    {
        1970,   /* wYear            */
        1,      /* wMonth           */
        0,      /* wDayOfWeek       */
        1,      /* wDay             */
        0,      /* wHour            */
        0,      /* wMinute          */
        0,      /* wSecond          */
        0       /* wMilliseconds    */
    };
    
    ULARGE_INTEGER itime;
    FILETIME base_ft;
    int bias = 0;

    if (local_time)
    {
        SYSTEMTIME temp_st;
        success = FileTimeToSystemTime(pft, & temp_st);
        bias =  GetTimeZoneBias(& temp_st);
    }

    success = SystemTimeToFileTime ( &base_st, &base_ft );
    if (success) 
    {
        itime.QuadPart = ((ULARGE_INTEGER *)pft)->QuadPart;

        itime.QuadPart -= ((ULARGE_INTEGER *)&base_ft)->QuadPart;
        itime.QuadPart /= systemtime_second;	// itime is now in seconds.
        itime.QuadPart += bias * 60;    // bias is in minutes.

        *put = itime.LowPart;
    }

    if (!success)
    {
        *put = -1;   /* error value used by mktime() */
    }
    return success;
}

static int _statcore(HANDLE hFile, const char *filename, struct stat *buf)
{
    BY_HANDLE_FILE_INFORMATION bhfi;
	int isdev;
	char szFs[32];
	int is_gmt_fs = -1;

	if(hFile)
	{
		/* Find out what kind of handle underlies filedes
		*/
		isdev = GetFileType(hFile) & ~FILE_TYPE_REMOTE;

		if ( isdev != FILE_TYPE_DISK )
		{
			/* not a disk file. probably a device or pipe
			 */
			if ( (isdev == FILE_TYPE_CHAR) || (isdev == FILE_TYPE_PIPE) )
			{
				/* treat pipes and devices similarly. no further info is
				 * available from any API, so set the fields as reasonably
				 * as possible and return.
				 */
				if ( isdev == FILE_TYPE_CHAR )
					buf->st_mode = _S_IFCHR;
				else
					buf->st_mode = _S_IFIFO;

				buf->st_rdev = buf->st_dev = (_dev_t)hFile;
				buf->st_nlink = 1;
				buf->st_uid = buf->st_gid = buf->st_ino = 0;
				buf->st_atime = buf->st_mtime = buf->st_ctime = 0;
				if ( isdev == FILE_TYPE_CHAR )
					buf->st_size = 0;
				else
				{
					unsigned long ulAvail;
					int rc;
					rc = PeekNamedPipe(hFile,NULL,0,NULL,&ulAvail,NULL);
					if (rc)
						buf->st_size = (_off_t)ulAvail;
					else 
						buf->st_size = (_off_t)0;
				}

				return 0;
			}
			else if ( isdev == FILE_TYPE_UNKNOWN )
			{
				// Network device
				errno = EBADF;
				return -1;
			}
		}
	}

    /* set the common fields
     */
    buf->st_ino = buf->st_uid = buf->st_gid = buf->st_mode = 0;
    buf->st_nlink = 1;

	if(hFile)
	{
		/* use the file handle to get all the info about the file
		 */
		if ( !GetFileInformationByHandle(hFile, &bhfi) )
		{
			_dosmaperr(GetLastError());
			return -1;
		}
	}
	else
	{
		WIN32_FIND_DATAA fd;
		HANDLE hFind;
		
		hFind=FindFirstFileA(filename,&fd);
		memset(&bhfi,0,sizeof(bhfi));
		if(hFind==INVALID_HANDLE_VALUE)
		{
			// If we don't have permissions to view the directory content (required by FindFirstFile,
			// maybe we can open the file directly
			hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, 0);

			if(hFile != INVALID_HANDLE_VALUE)
			{
				GetFileInformationByHandle(hFile, &bhfi);
				CloseHandle(hFile);
			}
			else
			{
				// Can't do anything about the root directory...
				// Win2k and Win98 can return info, but Win95 can't, so
				// it's best to do nothing.
				bhfi.dwFileAttributes=GetFileAttributesA(filename);
				if(bhfi.dwFileAttributes==0xFFFFFFFF)
				{
					_dosmaperr(GetLastError());
					return -1;
				}
				bhfi.nNumberOfLinks=1;
 			}
		}
		else
		{
			FindClose(hFind);
			bhfi.dwFileAttributes=fd.dwFileAttributes;
			bhfi.ftCreationTime=fd.ftCreationTime;
			bhfi.ftLastAccessTime=fd.ftLastAccessTime;
			bhfi.ftLastWriteTime=fd.ftLastWriteTime;
			bhfi.nFileSizeLow=fd.nFileSizeLow;
			bhfi.nFileSizeHigh=fd.nFileSizeHigh;
			bhfi.nNumberOfLinks=1;
		}
	}

   buf->dwFileAttributes = bhfi.dwFileAttributes;
    if ( bhfi.dwFileAttributes & FILE_ATTRIBUTE_READONLY )
        buf->st_mode |= (_S_IREAD + (_S_IREAD >> 3) + (_S_IREAD >> 6));
    else
        buf->st_mode |= ((_S_IREAD|_S_IWRITE) + ((_S_IREAD|_S_IWRITE) >> 3)
          + ((_S_IREAD|_S_IWRITE) >> 6));
	if (bhfi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        buf->st_mode |= (_S_IEXEC + (_S_IEXEC >> 3) + (_S_IEXEC >> 6));

    // Potential, but unlikely problem... casting DWORD to short.
    // Reported by Jerzy Kaczorowski, addressed by Jonathan Gilligan
    // if the number of links is greater than 0x7FFF
    // there would be an overflow problem.
    // This is a problem inherent in the struct stat, and hence
    // in the design of the C library.
    if (bhfi.nNumberOfLinks > SHRT_MAX) {
        buf->st_nlink = SHRT_MAX;
        }
    else {
	    buf->st_nlink=(short)bhfi.nNumberOfLinks;
        }

	if(!filename)
	{
		// We have to assume here that the repository doesn't span drives, so the
		// current directory is correct.
		*szFs='\0';
		GetVolumeInformationA(NULL,NULL,0,NULL,NULL,NULL,szFs,32);
		
		is_gmt_fs = GMT_FS(szFs);
	}
	else
	{
		if(filename[1]!=':')
		{
			if((filename[0]=='\\' || filename[0]=='/') && (filename[1]=='\\' || filename[1]=='/'))
			{
				// UNC pathname: Extract server and share and pass it to GVI
				char szRootPath[MAX_PATH + 1] = "\\\\";
				const char *p = &filename[2];
				char *q = &szRootPath[2];
				for (int n = 0; n < 2; n++)
				{
					// Get n-th path element
					while (*p != 0 && *p != '/' && *p != '\\')
					{
						*q = *p;
						p++;
						q++;
					}
					// Add separator
					if (*p != 0)
					{
						*q = '\\';
						p++;
						q++;
					}
				}
				*q = 0;

				*szFs='\0';
				GetVolumeInformationA(szRootPath,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
				is_gmt_fs = GMT_FS(szFs);
			}
			else
			{
				// Relative path, treat as local
				GetVolumeInformationA(NULL,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
				is_gmt_fs = GMT_FS(szFs);
			}
		}
		else
		{
			// Drive specified...
			char szRootPath[4] = "?:\\";
			szRootPath[0]=filename[0];
			*szFs='\0';
			GetVolumeInformationA(szRootPath,NULL,0,NULL,NULL,NULL,szFs,sizeof(szFs));
			is_gmt_fs = GMT_FS(szFs);
		}
	}

	if(is_gmt_fs) // NTFS or similar - everything is in GMT already
	{
		FileTimeToUnixTime ( &bhfi.ftLastAccessTime, &buf->st_atime, FALSE );
		FileTimeToUnixTime ( &bhfi.ftLastWriteTime, &buf->st_mtime, FALSE );
		FileTimeToUnixTime ( &bhfi.ftCreationTime, &buf->st_ctime, FALSE );
	}
	else 
	{
        // FAT - timestamps are in incorrectly translated local time.
        // translate them back and let FileTimeToUnixTime() do the
        // job properly.

        FILETIME At,Wt,Ct;

		FileTimeToLocalFileTime ( &bhfi.ftLastAccessTime, &At );
		FileTimeToLocalFileTime ( &bhfi.ftLastWriteTime, &Wt );
		FileTimeToLocalFileTime ( &bhfi.ftCreationTime, &Ct );
		FileTimeToUnixTime ( &At, &buf->st_atime, TRUE );
		FileTimeToUnixTime ( &Wt, &buf->st_mtime, TRUE );
		FileTimeToUnixTime ( &Ct, &buf->st_ctime, TRUE );
	}

    buf->st_size = bhfi.nFileSizeLow;
	if(bhfi.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
		buf->st_mode |= _S_IFDIR;
	else
		buf->st_mode |= _S_IFREG;

    /* On DOS, this field contains the drive number, but
     * the drive number is not available on this platform.
     * Also, for UNC network names, there is no drive number.
     */
    buf->st_rdev = buf->st_dev = 0;
	return 0;
}

int wnt_stat(const char *name, struct wnt_stat *buf)
{
	return _statcore(NULL,name,buf);
}

int wnt_fstat (int fildes, struct wnt_stat *buf)
{
	return _statcore((HANDLE)_get_osfhandle(fildes),NULL,buf);
}

int wnt_lstat (const char *name, struct wnt_stat *buf)
{
	return _statcore(NULL,name,buf);
}

