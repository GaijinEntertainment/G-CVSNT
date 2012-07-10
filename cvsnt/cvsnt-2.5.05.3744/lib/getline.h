#ifndef _getline_h_
#define _getline_h_ 1

#include <stdio.h>

#ifndef HAVE_GETLINE

#ifdef __cplusplus
extern "C" {
#endif

int getline(char **_lineptr, size_t *_n, FILE *_stream);

#ifdef __cplusplus
}
#endif

#endif

#endif /* _getline_h_ */
