#ifndef _getdelim_h
#define _getdelim_h_ 1

#include <stdio.h>

#ifndef HAVE_GETDELIM

#ifdef __cplusplus
extern "C" {
#endif

int getdelim(char **_lineptr, size_t *_n, int delim, FILE *_stream);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _getline_h_ */
