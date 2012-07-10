#ifndef _FAKE_GETADDRINFO_H
#define _FAKE_GETADDRINFO_H

#include <stdlib.h>

#ifndef HAVE_GETADDRINFO

#include "gai-errnos.h"

# define AI_PASSIVE        1
# define AI_CANONNAME      2

# define NI_NUMERICHOST    2
# define NI_NAMEREQD       4
# define NI_NUMERICSERV    8

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};

int getaddrinfo(const char *hostname, const char *servname, 
                const struct addrinfo *hints, struct addrinfo **res);

char *gai_strerror(int ecode);

void freeaddrinfo(struct addrinfo *ai);

#endif

#endif /* _FAKE_GETADDRINFO_H */
