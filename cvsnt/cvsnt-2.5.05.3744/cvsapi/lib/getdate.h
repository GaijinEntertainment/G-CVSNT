#ifndef GETDATE__H
#define GETDATE__H

#ifdef __cplusplus
extern "C" {
#endif

time_t CVSAPI_EXPORT get_date(char *p, struct timeb *now); 
long CVSAPI_EXPORT difftm (struct tm *a, struct tm *b);

#ifdef __cplusplus
}
#endif

#endif

