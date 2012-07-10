/*----------------------------------------------------------------
   ClockRackCon.c -- Time-zone time conversion for ClockRack 1.1
                     (c) Ziff Davis Media, Inc.
                     All rights reserved.

   First published in PC Magazine, US edition, September 1, 2000.

   Programmed by Charles Petzold.

   The entire function of this module is to implement the 
     MySystemTimeToTzSpecificLocalTime all because Windows 95 & 98
     do not implement the NT function SystemTimeToTzSpecificLocalTime.
  ---------------------------------------------------------------------*/

#include "StdAfx.h"
#include <windows.h>
#include "FixWinDefs.h"

BOOL LocalGreaterThanTransition (const SYSTEMTIME * pstLoc, const SYSTEMTIME * pstTran) ;

void ConvertToAbsolute (const SYSTEMTIME * pstLoc, 
                        const SYSTEMTIME * pstDst, 
                        SYSTEMTIME * pstDstAbs) ;

/*----------------------------------------------------------------- 
   MySystemTimeToTzSpecificLocalTime

   UTC time and time-zone information go in; local time comes out.
  -----------------------------------------------------------------*/

BOOL MySystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION * ptzi, 
                                       const SYSTEMTIME * pstUtc, SYSTEMTIME * pstLoc)
{
     FILETIME      ft ;
     LARGE_INTEGER li ;
     SYSTEMTIME    stDst ;

          // If we're running under NT, just call the real function

     if (!(0x80000000 & GetVersion()))
          return SystemTimeToTzSpecificLocalTime ((TIME_ZONE_INFORMATION *) ptzi, (SYSTEMTIME *) pstUtc, pstLoc) ;

          // Convert time to a LARGE_INTEGER and subtract the bias

     SystemTimeToFileTime (pstUtc, &ft) ;
     li = * (LARGE_INTEGER *) (void *) &ft;
     li.QuadPart -= (LONGLONG) 600000000 * ptzi->Bias ;

          // Convert to a local date/time before application of daylight saving time.
          // The local date/time must be used to determine when the conversion occurs.

     ft = * (FILETIME *) (void *) &li ;
     FileTimeToSystemTime (&ft, pstLoc) ;

          // Find the time assuming Daylight Saving Time

     li.QuadPart -= (LONGLONG) 600000000 * ptzi->DaylightBias ;
     ft = * (FILETIME *) (void *) &li ;
     FileTimeToSystemTime (&ft, &stDst) ;

          // Now put li back the way it was

     li.QuadPart += (LONGLONG) 600000000 * ptzi->DaylightBias ;

     if (ptzi->StandardDate.wMonth)          // ie, daylight savings time
     {
               // Northern hemisphere

          if ((ptzi->DaylightDate.wMonth < ptzi->StandardDate.wMonth) &&

               (stDst.wMonth >= pstLoc->wMonth) &&           // avoid the end of year problem
               
               LocalGreaterThanTransition (pstLoc, &ptzi->DaylightDate) &&
              !LocalGreaterThanTransition (&stDst, &ptzi->StandardDate))
          {
               li.QuadPart -= (LONGLONG) 600000000 * ptzi->DaylightBias ;
          }
               // Southern hemisphere

          else if ((ptzi->StandardDate.wMonth < ptzi->DaylightDate.wMonth) &&
                  (!LocalGreaterThanTransition (&stDst, &ptzi->StandardDate) ||
                    LocalGreaterThanTransition (pstLoc, &ptzi->DaylightDate)))
          {
               li.QuadPart -= (LONGLONG) 600000000 * ptzi->DaylightBias ;
          }
          else
          {
               li.QuadPart -= (LONGLONG) 600000000 * ptzi->StandardBias ;
          }
     }

     ft = * (FILETIME *) (void *) &li ;
     FileTimeToSystemTime (&ft, pstLoc) ;
     return TRUE ;
}

/*----------------------------------------------------------------------
   LocalGreaterThanTransition: Returns TRUE if the local time comes 
          after the daylight saving time (or standard time) transition.
  ----------------------------------------------------------------------*/

BOOL LocalGreaterThanTransition (const SYSTEMTIME * pstLoc, const SYSTEMTIME * pstTran)
{
     FILETIME      ftLoc, ftTran ;
     LARGE_INTEGER liLoc, liTran ;
     SYSTEMTIME    stTranAbs ;

          // Easy case: Just compare the two months

     if (pstLoc->wMonth != pstTran->wMonth)
          return (pstLoc->wMonth > pstTran->wMonth) ;

          // Well, we're in a transition month. That requires a bit more work.

          // Check if pstDst is in absolute or day-in-month format.
          //   (See documentation of TIME_ZONE_INFORMATION, StandardDate field.)

     if (pstTran->wYear)       // absolute format (haven't seen one yet!)
     {
          stTranAbs = * pstTran ;
     }
     else                     // day-in-month format
     {
          ConvertToAbsolute (pstLoc, pstTran, &stTranAbs) ;
     }

          // Now convert both date/time structures to large integers & compare
     
     SystemTimeToFileTime (pstLoc, &ftLoc) ;
     liLoc = * (LARGE_INTEGER *) (void *) &ftLoc ;

     SystemTimeToFileTime (&stTranAbs, &ftTran) ;
     liTran = * (LARGE_INTEGER *) (void *) &ftTran ;

     return (liLoc.QuadPart > liTran.QuadPart) ;
}

/*------------------------------------------------------------------------
   ConvertToAbsolute:  Converts day-in-month format to absolute date/time
  ------------------------------------------------------------------------*/

void ConvertToAbsolute (const SYSTEMTIME * pstLoc, const SYSTEMTIME * pstDst, SYSTEMTIME * pstDstAbs)
{
     static int    iDays [12] = { 31, 28, 31, 30, 31, 30, 
                                  31, 31, 30, 31, 30, 31 } ;
     int           iDay ;

          // Set up the aboluste date structure except for wDay, which we must find

     pstDstAbs->wYear         = pstLoc->wYear ;      // Notice from local date/time
     pstDstAbs->wMonth        = pstDst->wMonth ;
     pstDstAbs->wDayOfWeek    = pstDst->wDayOfWeek ;

     pstDstAbs->wHour         = pstDst->wHour ;
     pstDstAbs->wMinute       = pstDst->wMinute ;
     pstDstAbs->wSecond       = pstDst->wSecond ;
     pstDstAbs->wMilliseconds = pstDst->wMilliseconds ;

          // Fix the iDays array for leap years

     if ((pstLoc->wYear % 4 == 0) && ((pstLoc->wYear % 100 != 0) || 
                                      (pstLoc->wYear % 400 == 0)))
     {
          iDays[1] = 29 ;
     }

          // Find a day of the month that falls on the same 
          //   day of the week as the transition.

          // Suppose today is the 20th of the month (pstLoc->wDay = 20)
          // Suppose today is a Wednesday (pstLoc->wDayOfWeek = 3)
          // Suppose the transition occurs on a Friday (pstDst->wDayOfWeek = 5)
          // Then iDay = 31, meaning that the 31st falls on a Friday
          // (The 7 is this formula avoids negatives.)

     iDay = pstLoc->wDay + pstDst->wDayOfWeek + 7 - pstLoc->wDayOfWeek ;

          // Now shrink iDay to a value between 1 and 7.

     iDay = (iDay - 1) % 7 + 1 ;

          // Now iDay is a day of the month ranging from 1 to 7.
          // Recall that the wDay field of the structure can range
          //   from 1 to 5, 1 meaning "first", 2 meaning "second",
          //   and 5 meaning "last".
          // So, increase iDay so it's the proper day of the month.

     iDay += 7 * (pstDst->wDay - 1) ;

          // Could be that iDay overshot the end of the month, so
          //   fix it up using the number of days in each month

     if (iDay > iDays[pstDst->wMonth - 1])
          iDay -= 7 ;

          // Assign that day to the structure. 

     pstDstAbs->wDay = iDay ;
}
