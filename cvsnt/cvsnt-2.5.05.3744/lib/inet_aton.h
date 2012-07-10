#ifndef _FAKE_INET_ATON_H
#define _FAKE_INET_ATON_H


#include <netinet/in.h>

#ifndef HAVE_INET_ATON

int inet_aton(const char *cp, struct in_addr *addr);

#endif

#endif /* _BSD_INET_ATON_H */
