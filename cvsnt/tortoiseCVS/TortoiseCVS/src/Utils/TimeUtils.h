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

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <windows.h>
#include "FixWinDefs.h"
#include <time.h>

bool local_SYSTEMTIME_to_local_DateTimeFormatted(const SYSTEMTIME *lpdate,
                                                 wxChar *dateTimeFormatted,
                                                 int fieldSize,
                                                 bool longDateTime = true,
                                                 bool includeTime = true,
                                                 bool includeSeconds = true);
bool asctime_to_local_DateTimeFormatted(const char* asctime,
                                        wxChar *dateTimeFormatted,
                                        int fieldSize,
                                        bool longDateTime = true);
bool tm_to_local_DateTimeFormatted(const struct tm *ptm,
                                   wxChar* dateTimeFormatted,
                                   int fieldSize,
                                   bool longDateTime = true,
                                   bool includeSeconds = false);

bool tm_to_local_DateFormatted(const struct tm *ptm, 
                               wxChar *dateFormatted, 
                               int fieldSize, 
                               bool longDate);

bool asctime_to_local_SYSTEMTIME(const char* asctime, SYSTEMTIME *lpdate);
bool SYSTEMTIME_to_local_SYSTEMTIME(const SYSTEMTIME *pUtcTime, SYSTEMTIME *lpdate);

bool asctime_to_SYSTEMTIME(const char* asctime, SYSTEMTIME *pUtcTime);
bool asctime_to_time_t(const char* asctime, time_t* t);

bool tm_to_SYSTEMTIME(const struct tm *ptm, SYSTEMTIME *pUtcTime);

bool datestring_to_tm(const char* datestring, struct tm *ptm);

char* asctime_r(const struct tm *tm, char *buffer);

char* ctime_r(const time_t *timer, char *buffer);

struct tm *gmtime_r(const time_t *clock, struct tm *result); 


BOOL MySystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION * ptzi,
                                       const SYSTEMTIME * pstUtc, SYSTEMTIME * pstLoc);

// From getdate.y
extern time_t get_date(const char* p);

#endif
