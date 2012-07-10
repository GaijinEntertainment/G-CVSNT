#ifndef __UFC_CRYPT__H
#define __UFC_CRYPT__H

#ifndef _WIN32
// There isn't a unix in existence that doesn't have a crypt function...
#include <unistd.h>
#else

#ifdef __cplusplus
extern "C" {
#endif

char *crypt(const char *key, const char *salt);

#ifdef __cplusplus
}
#endif

#endif

#endif