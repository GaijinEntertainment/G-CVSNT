/*
 * fake getaddrinfo library taken from openssh distribution
 *
 * This file includes getaddrinfo(), freeaddrinfo() and gai_strerror().
 * These funtions are defined in rfc2133.
 *
 * But these functions are not implemented correctly. The minimum subset
 * is implemented for ssh use only. For exapmle, this routine assumes
 * that ai_family is AF_INET. Don't use it for another purpose.
 */

#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include "socket.h"
#include "getaddrinfo.h"
#include "getnameinfo.h"
#include "gai-errnos.h"


char *gai_strerror(int ecode)
{
	switch (ecode) {
		case EAI_NODATA:
			return "no address associated with hostname.";
		case EAI_MEMORY:
			return "memory allocation failure.";
		default:
			return "unknown error.";
	}
}    

void freeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *next;

	do {
		next = ai->ai_next;
		free(ai->ai_canonname);
		free(ai);
	} while (NULL != (ai = next));
}

static struct addrinfo *malloc_ai(int port, int type, unsigned long addr)
{
	struct addrinfo *ai;

	ai = malloc(sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
	if (ai == NULL)
		return(NULL);
	
	memset(ai, 0, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
	
	ai->ai_addr = (struct sockaddr *)(ai + 1);
	/* XXX -- ssh doesn't use sa_len */
	ai->ai_addrlen = sizeof(struct sockaddr_in);
	ai->ai_addr->sa_family = ai->ai_family = AF_INET;
	ai->ai_socktype = type;

	((struct sockaddr_in *)(ai)->ai_addr)->sin_port = port;
	((struct sockaddr_in *)(ai)->ai_addr)->sin_addr.s_addr = addr;
	
	return(ai);
}

int getaddrinfo(const char *hostname, const char *servname, 
                const struct addrinfo *hints, struct addrinfo **res)
{
	struct addrinfo *cur, *prev = NULL;
	struct hostent *hp;
	struct in_addr in;
	int i,port,type;

	if (servname)
		port = htons((short)atoi(servname));
	else
		port = 0;

	if (hints && hints->ai_socktype)
	  type = hints->ai_socktype;
	else
	  type = SOCK_STREAM;

	if (hints && hints->ai_flags & AI_PASSIVE) {
		if (NULL != (*res = malloc_ai(port, type, htonl(0x00000000))))
			return 0;
		else
			return EAI_MEMORY;
	}
		
	if (!hostname) {
		if (NULL != (*res = malloc_ai(port, type, htonl(0x7f000001))))
			return 0;
		else
			return EAI_MEMORY;
	}
	
	if (inet_aton(hostname, &in)) {
		if (NULL != (*res = malloc_ai(port, type, in.s_addr)))
			return 0;
		else
			return EAI_MEMORY;
	}
	
	hp = gethostbyname(hostname);	
	if (hp && hp->h_name && hp->h_name[0] && hp->h_addr_list[0]) {
		for (i = 0; hp->h_addr_list[i]; i++) {
			cur = malloc_ai(port, type, ((struct in_addr *)hp->h_addr_list[i])->s_addr);
			if (cur == NULL) {
				if (*res)
					freeaddrinfo(*res);
				return EAI_MEMORY;
			}
			if (hints && hints->ai_flags & AI_CANONNAME)
				cur->ai_canonname = strdup(hp->h_name);
			
			if (prev)
				prev->ai_next = cur;
			else
				*res = cur;

			prev = cur;
		}
		return 0;
	}
	
	return EAI_NODATA;
}

