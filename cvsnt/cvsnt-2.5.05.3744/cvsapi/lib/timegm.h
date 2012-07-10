#ifndef __TIMEGM__H
#define __TIMEGM__H

#ifndef HAVE_TIMEGM

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

time_t CVSAPI_EXPORT timegm(struct tm *tm);

#ifdef __cplusplus
}
#endif

#endif

#endif

