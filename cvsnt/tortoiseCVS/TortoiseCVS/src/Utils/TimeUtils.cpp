// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "StdAfx.h"
#include "TimeUtils.h"
#include "SyncUtils.h"
#include <stdlib.h>

static const char*  const months[] =
{
   "Jan", "Feb", "Mar", "Apr", "May", "Jun",
   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char*  const daysOfWeek[] =
{
   "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

bool asctime_to_SYSTEMTIME(const char* asctime, SYSTEMTIME *lpdate)
{
   // "Wed Jan 02 02:03:55 1980"
   // 0000000000111111111122222
   // 0123456789012345678901234

   char str[26];
   int i;

   int year; 
   int month; 
   int dayOfWeek; 
   int day; 
   int hour; 
   int minute; 
   int second; 

   // check it's really an asctime string
   if (strlen(asctime) < 24 ||
       asctime[3] != ' ' ||
       asctime[7] != ' ' ||
       asctime[10] != ' ' ||
       asctime[13] != ':' ||
       asctime[16] != ':' ||
       asctime[19] != ' ')
      return false;

   // separate fields
   strcpy(str, asctime);
   str[3] = str[7] = str[10] = str[13] = str[16] = str[19] = str[24] = '\0';

   // search for day of week
   dayOfWeek = -1;
   for (i = 0; i < 7; i++)
   {
      if (strcmp(daysOfWeek[i], &str[0]) == 0)
      {
         dayOfWeek = i;
         break;
      }
   }

   // search for month
   month = -1;
   for (i = 0; i < 12; i++)
   {
      if (strcmp(months[i], &str[4]) == 0)
      {
         month = i;
         break;
      }
   }

   // scan numeric fields
   hour = atoi(&str[11]);
   day = atoi(&str[8]);
   minute = atoi(&str[14]);
   second = atoi(&str[17]);
   year = atoi(&str[20]);

   // check consistency
   if (
      dayOfWeek<0 || month<0 ||
      day<1 || day>31 ||
      hour<0 || hour >= 24 ||
      minute<0 || minute >= 60 ||
      second<0 || second >= 60 ||
      year<0 || year>=10000
   )
      return false;

   // save results
   lpdate->wYear = year;
   lpdate->wMonth = month+1;
   lpdate->wDayOfWeek = dayOfWeek;
   lpdate->wHour = hour;
   lpdate->wDay = day;
   lpdate->wMinute = minute;
   lpdate->wSecond = second;
   lpdate->wMilliseconds = 0;

   return true;
}

bool asctime_to_time_t(const char* asctime, time_t* t)
{
   // "Wed Jan 02 02:03:55 1980"
   // 0000000000111111111122222
   // 0123456789012345678901234

   char str[26];
   int i;

   int year; 
   int month; 
   int dayOfWeek; 
   int day; 
   int hour; 
   int minute; 
   int second; 

   // check it's really an asctime string
   if (strlen(asctime) < 24 ||
       asctime[3] != ' ' ||
       asctime[7] != ' ' ||
       asctime[10] != ' ' ||
       asctime[13] != ':' ||
       asctime[16] != ':' ||
       asctime[19] != ' ')
      return false;

   // separate fields
   strcpy(str, asctime);
   str[3] = str[7] = str[10] = str[13] = str[16] = str[19] = str[24] = '\0';

   // search for day of week
   dayOfWeek = -1;
   for (i = 0; i < 7; i++)
   {
      if (strcmp(daysOfWeek[i], &str[0]) == 0)
      {
         dayOfWeek = i;
         break;
      }
   }

   // search for month
   month = -1;
   for (i = 0; i < 12; i++)
   {
      if (strcmp(months[i], &str[4]) == 0)
      {
         month = i;
         break;
      }
   }

   // scan numeric fields
   hour = atoi(&str[11]);
   day = atoi(&str[8]);
   minute = atoi(&str[14]);
   second = atoi(&str[17]);
   year = atoi(&str[20]);

   // check consistency
   if (
      dayOfWeek<0 || month<0 ||
      day<1 || day>31 ||
      hour<0 || hour >= 24 ||
      minute<0 || minute >= 60 ||
      second<0 || second >= 60 ||
      year<0 || year>=10000
   )
      return false;

   // convert
   struct tm tm;
   tm.tm_isdst = 0;
   tm.tm_year = year - 1900;
   tm.tm_mon = month;
   tm.tm_hour = hour;
   tm.tm_mday = day;
   tm.tm_wday = dayOfWeek;
   tm.tm_min = minute;
   tm.tm_sec = second;

   *t = mktime(&tm);
   return true;
}

bool datestring_to_tm(const char* datestring, struct tm *ptm) {
   // Converts date strings in the format: DD-MMM-YY or DD-MMM-YYYY (e.g. "05-Jan-01" or "05-Jan-2001")
   // cvs uses 2 digit years in the Annotate output

   char str[12];
   int i;

   int day;
   int month;
   int year = -1;
   
   // check it's a valid datestring
   if (strlen(datestring) < 9 ||
       datestring[2] != '-' ||
       datestring[6] != '-')
      return false;

   // separate fields
   strcpy(str, datestring);
   str[2] = str[6] = '\0';

   // day
   day = atoi(&str[0]);

   // month
   month = -1;
   for (i=0; i<12; i++) {
      if (strcmp(months[i], &str[3]) == 0) {
         month = i;
         break;
      }
   }

   // year
   if (strlen(&str[7]) == 2) {
       // this logic will break within the next 50 years, but hopefully by then
       // cvs will have figured out that 2 digit years are broken to begin with.
       year = atoi(&str[7]);
       year += year > 50 ? 1900 : 2000;
   } else if (strlen(&str[7]) == 4) {
       year = atoi(&str[7]);
   }   

   // check consistency
   if (
      day<1 || day>31 ||
      month<0 || month>11 ||
      year<0 || year>=10000
   )
      return false;

   // save results
   memset(ptm, 0, sizeof(*ptm));
   ptm->tm_mday = day;
   ptm->tm_mon = month;
   ptm->tm_year = year - 1900;

   return true;
}

bool tm_to_SYSTEMTIME(const struct tm *ptm, SYSTEMTIME *pUtcTime)
{
   pUtcTime->wYear = ptm->tm_year + 1900;
   pUtcTime->wMonth = ptm->tm_mon + 1;
   pUtcTime->wDayOfWeek = ptm->tm_wday;
   pUtcTime->wHour = ptm->tm_hour;
   pUtcTime->wDay = ptm->tm_mday;
   pUtcTime->wMinute = ptm->tm_min;
   pUtcTime->wSecond = ptm->tm_sec;
   pUtcTime->wMilliseconds = 0;
   return true;
}

bool SYSTEMTIME_to_local_SYSTEMTIME(const SYSTEMTIME *pUtcTime, SYSTEMTIME *lpdate)
{
   TIME_ZONE_INFORMATION tzi;
   if (GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID)
      return false;

   if (!MySystemTimeToTzSpecificLocalTime(&tzi, pUtcTime, lpdate))
      return false;

   return true;
}

bool local_SYSTEMTIME_to_local_DateTimeFormatted(const SYSTEMTIME *pLocalTime, wxChar *dateTimeFormatted,
                                                 int fieldSize, bool longDateTime, bool includeTime, bool includeSeconds)
{
   wxChar szTime[20];
   int dateLen;

   GetDateFormat(LOCALE_USER_DEFAULT, longDateTime ? DATE_LONGDATE : DATE_SHORTDATE,
                 pLocalTime, 0L, dateTimeFormatted, fieldSize);

   if (includeTime) {
       GetTimeFormat(LOCALE_USER_DEFAULT, longDateTime ? 0 : (includeSeconds ? 0 : TIME_NOSECONDS),
                 pLocalTime, 0L, szTime, sizeof(szTime));
       dateLen = static_cast<int>(_tcslen(dateTimeFormatted));
       if (fieldSize-dateLen-2 > 0)
       {
           _tcsncat(dateTimeFormatted, wxT(" "), fieldSize-dateLen-1);
           _tcsncat(dateTimeFormatted, szTime, fieldSize-dateLen-2);
       }
   }
   return true;
}

bool asctime_to_local_DateTimeFormatted(const char* asctime, wxChar *dateTimeFormatted, int fieldSize, bool longDateTime)
{
   SYSTEMTIME utcTime;
   SYSTEMTIME localTime;
   return
      asctime_to_SYSTEMTIME(asctime, &utcTime) &&
      SYSTEMTIME_to_local_SYSTEMTIME(&utcTime, &localTime) &&
      local_SYSTEMTIME_to_local_DateTimeFormatted(&localTime, dateTimeFormatted, fieldSize, longDateTime);
}

bool tm_to_local_DateTimeFormatted(const struct tm *ptm, wxChar *dateTimeFormatted, int fieldSize,
                                   bool longDateTime, bool includeSeconds)
{
   SYSTEMTIME utcTime;
   SYSTEMTIME localTime;
   return
      tm_to_SYSTEMTIME(ptm, &utcTime) &&
      SYSTEMTIME_to_local_SYSTEMTIME(&utcTime, &localTime) &&
      local_SYSTEMTIME_to_local_DateTimeFormatted(&localTime, dateTimeFormatted, fieldSize,
                                                  longDateTime, true, includeSeconds);
}

bool tm_to_local_DateFormatted(const struct tm *ptm, wxChar *dateFormatted, int fieldSize, bool longDate)
{
   SYSTEMTIME utcTime;
   SYSTEMTIME localTime;
   return
      tm_to_SYSTEMTIME(ptm, &utcTime) &&
      SYSTEMTIME_to_local_SYSTEMTIME(&utcTime, &localTime) &&
      local_SYSTEMTIME_to_local_DateTimeFormatted(&localTime, dateFormatted, fieldSize, longDate, false);
}

CriticalSection csTimeUtils;


char* asctime_r(const struct tm *tm, char *buffer)
{
   CSHelper csh(csTimeUtils, true);
   char *buf = asctime(tm);
   if (buf)
      strcpy(buffer, buf);
   return buf;
}


char* ctime_r(const time_t *timer, char *buffer)
{
   CSHelper csh(csTimeUtils, true);
   char *buf = ctime(timer);
   if (buf)
      strcpy(buffer, buf);
   return buf;
}


struct tm *gmtime_r(const time_t *clock, struct tm *result)
{
   CSHelper csh(csTimeUtils, true);
   struct tm* tm_p = gmtime(clock);
   if (tm_p)
      *result = *tm_p;
   return tm_p;
}
