#ifndef _FAKE_SOCKET_H
#define _FAKE_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

# define	_SS_MAXSIZE	128	/* Implementation specific max size */
# define       _SS_PADSIZE     (_SS_MAXSIZE - sizeof (struct sockaddr))

struct sockaddr_storage {
	struct	sockaddr ss_sa;
	char		__ss_pad2[_SS_PADSIZE];
};
# define ss_family ss_sa.sa_family

/* Define it to something that should never appear */
#define AF_INET6 AF_MAX

#endif /* !_FAKE_SOCKET_H */

