#ifndef _FAKE_GETNAMEINFO_H
#define _FAKE_GETNAMEINFO_H

int getnameinfo(const struct sockaddr *sa, size_t salen, char *host, 
                size_t hostlen, char *serv, size_t servlen, int flags);

# define NI_MAXSERV 32
# define NI_MAXHOST 1025

#endif /* _FAKE_GETNAMEINFO_H */
