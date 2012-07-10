/***
  This file was part of nss-mdns.
  Cross platform support & limited responder support by Tony Hoyle, 2005.
 
  mdnsclient is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  mdnsclient is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef _WIN32
// Microsoft braindamage reversal.  What planet are they on??
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef sun
// Solaris needs this to define the recvmsg calls properly
#define __EXTENSIONS__
#define _XPG4_2
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include "win32/inttypes.h"
#else
#include <stdarg.h> // hpux needs this before inttypes.h
#include <inttypes.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#endif
#include <fcntl.h>
#include <assert.h>
#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#endif

// OSX doesn't define this
#ifndef SOL_IP
#define SOL_IP IPPROTO_IP
#endif

#include "mdnsclient.h"
#include "dns.h"
#include "util.h"

#ifdef _WIN32
#define sock_errno WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
#define sock_strerror gai_strerror
#else
#define sock_errno errno
#define closesocket close
#define sock_strerror strerror
#endif

enum
{
	SQ_TXT = 1,
	SQ_SRV = 2
};

#define MDNS_SERV "_services._dns-sd._udp.local"

static mdns_service_item_t *service_root = NULL;

static void mdns_mcast_group(struct sockaddr_in *ret_sa) {
    assert(ret_sa);
    
    ret_sa->sin_family = AF_INET;
    ret_sa->sin_port = htons(5353);
    ret_sa->sin_addr.s_addr = inet_addr("224.0.0.251");
}

mdnshandle_t mdns_open(void)
{
    struct ip_mreq mreq;
    struct sockaddr_in sa;
	sock_t fd = (sock_t)-1;
    u_char ttl;
    int yes;

    mdns_mcast_group(&sa);
        
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
        fprintf(stderr, "socket() failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }
    
    ttl = 255;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof(ttl)) < 0)
	{
        fprintf(stderr, "IP_MULTICAST_TTL failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }

#ifdef SO_REUSEADDR
    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0)
	{
        fprintf(stderr, "SO_REUSEADDR failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }
#endif

#ifdef SO_REUSEPORT
    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const char *)&yes, sizeof(yes)) < 0)
	{
        fprintf(stderr, "SO_REUSEPORT failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }
#endif

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr = sa.sin_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    sa.sin_addr.s_addr=htons(INADDR_ANY);

    if (bind(fd, (struct sockaddr*) &sa, sizeof(sa)) < 0)
	{
        fprintf(stderr, "bind() failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }
    
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) < 0)
	{
        fprintf(stderr, "IP_ADD_MEMBERSHIP failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }

#ifdef IP_RECVTTL
	/* Linux specific (I think). */
    if (setsockopt(fd, IPPROTO_IP, IP_RECVTTL, (const char *)&yes, sizeof(yes)) < 0)
	{
        fprintf(stderr, "IP_RECVTTL failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }
#endif
   
    if (set_cloexec(fd) < 0) {
        fprintf(stderr, "FD_CLOEXEC failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }
    
    if (set_nonblock(fd) < 0) {
        fprintf(stderr, "O_ONONBLOCK failed: %s\n", sock_strerror(sock_errno));
        goto fail;
    }

    return (mdnshandle_t)fd;

fail:
    if (fd >= 0)
        closesocket(fd);

    return NULL;
}

int mdns_close(mdnshandle_t sock)
{
	return closesocket((sock_t)sock);
}

static int send_dns_packet(sock_t fd, struct dns_packet *p)
{
    struct sockaddr_in sa;

    assert(fd >= 0 && p);
    assert(dns_packet_check_valid(p) >= 0);

    mdns_mcast_group(&sa);
       
    for (;;) {
        
        if (sendto(fd, p->data, (int)p->size, 0, (struct sockaddr *)&sa, sizeof(sa)) >= 0)
            break;

        if (sock_errno != EAGAIN) {
	    fprintf(stderr, "sendto() failed: %s\n", sock_strerror(sock_errno));
            return -1;
        }
        
        if (wait_for_write(fd, NULL) < 0)
            return -1;
    }

	return 1;
}

static int recv_dns_packet(sock_t fd, struct dns_packet **ret_packet, struct timeval *end, struct sockaddr_storage *from, int from_len, int *ttl)
{
    struct dns_packet *p= NULL;
    int ret = -1;
	int err;

    assert(fd >= 0);

	p = dns_packet_new();
   
    for (;;) {
        ssize_t l;
        int r;
        
#if defined (IP_RECVTTL ) && ! defined (__HP_aCC)
		/* In theory anyone that has this also has recvmsg.. */
		struct msghdr msg = {0};
		struct iovec iov[1];
#if defined (_XOPEN_SOURCE_EXTENDED) || ! defined (__digital)
		char control[1024];
#endif

		iov[0].iov_base = p->data;
		iov[0].iov_len = sizeof(p->data);
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		msg.msg_name = from;
		msg.msg_namelen = from_len;
#if defined (_XOPEN_SOURCE_EXTENDED) || ! defined (__digital)
		msg.msg_control = control;
		msg.msg_controllen = sizeof(control);
#endif

		if(( l = recvmsg(fd, &msg, 0)) >= 0)
		{
#if defined (_XOPEN_SOURCE_EXTENDED) || ! defined (__digital)
			struct cmsghdr *cmsg;
			*ttl=255;
			for(cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg,cmsg))
			{
				if(cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_TTL)
				{
					*ttl = *(int*) CMSG_DATA(cmsg);
					break;
				}
			}
#endif

			p->size = (size_t) l;
			*ret_packet = p;
			return 0;
		}
#else
		if ((l = recvfrom(fd, p->data, (int)sizeof(p->data), 0, (struct sockaddr*)from, &from_len)) >= 0)
		{
            p->size = (size_t) l;

			if(ttl) *ttl=255;
            *ret_packet = p;
            return 0;
        }	
#endif
		err = sock_errno;
#ifdef sun
		/* Solaris bug - recvfrom does not set errno correctly (sets it to EBADF) even
		   though the kernel call has returned the correct value - verifiable in truss.. 
		   affects Solaris 9 at least */
		if(err==EBADF)
			err=EAGAIN;
#endif
	/* HPUX doesn't set errno *at all* for recvfrom... */

        if (err && err != EAGAIN && err!=EWOULDBLOCK) {
            fprintf(stderr, "recvfrom() failed: %s\n", sock_strerror(err));
            goto fail;
        }
        
        if ((r = wait_for_read(fd, end)) < 0)
            goto fail;
        else if (r > 0) { /* timeout */
            ret = 1;
            goto fail;
        }
    }

fail:
    if (p)
        dns_packet_free(p);

    return ret;
}

static int send_name_query(sock_t fd, const char *name)
{
    int ret = -1;
    struct dns_packet *p = NULL;
    uint8_t *prev_name = NULL;
    int qdcount = 0;
	int query_ipv4 = 1;
	int query_ipv6 = 1;

    assert(fd >= 0 && name && (query_ipv4 || query_ipv6));

    if (!(p = dns_packet_new())) {
        fprintf(stderr, "Failed to allocate DNS packet.\n");
        goto finish;
    }

    dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    if (query_ipv4) 
	{
        if (!(prev_name = dns_packet_append_name(p, name)))
		{
            fprintf(stderr, "Bad host name\n");
            goto finish;
            
        }
        dns_packet_append_uint16(p, DNS_TYPE_A);
        dns_packet_append_uint16(p, DNS_CLASS_IN);
        qdcount++;
    }
    
    if (query_ipv6)
	{
        if (!dns_packet_append_name_compressed(p, name, prev_name))
		{
            fprintf(stderr, "Bad host name\n");
            goto finish;
        }
        
        dns_packet_append_uint16(p, DNS_TYPE_AAAA);
        dns_packet_append_uint16(p, DNS_CLASS_IN);
        qdcount++;
    }

	dns_packet_set_field(p, DNS_FIELD_QDCOUNT, qdcount);
    
    ret = send_dns_packet(fd, p);
    
finish:
    if (p)
        dns_packet_free(p);

    return ret;
}

static int send_service_query(sock_t fd, const char *service, int flags)
{
    int ret = -1;
    struct dns_packet *p = NULL;
    uint8_t *prev_name = NULL;
    int qdcount = 0;

    assert(fd >= 0);

    if (!(p = dns_packet_new())) {
        fprintf(stderr, "Failed to allocate DNS packet.\n");
        goto finish;
    }

    dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    if (!(prev_name = dns_packet_append_name(p, service))) {
        fprintf(stderr, "Bad host name\n");
        goto finish;
        
    }
    dns_packet_append_uint16(p, DNS_TYPE_PTR);
    dns_packet_append_uint16(p, DNS_CLASS_IN);
    qdcount++;
    
    if (!(prev_name = dns_packet_append_name(p, service))) {
        fprintf(stderr, "Bad host name\n");
        goto finish;
        
    }

	if(flags&SQ_SRV)
	{
		dns_packet_append_uint16(p, DNS_TYPE_SRV);
		dns_packet_append_uint16(p, DNS_CLASS_IN);
		qdcount++;
	}
	else if(flags&SQ_TXT)
	{
		dns_packet_append_uint16(p, DNS_TYPE_TXT);
		dns_packet_append_uint16(p, DNS_CLASS_IN);
		qdcount++;
	}

	dns_packet_set_field(p, DNS_FIELD_QDCOUNT, qdcount);
    
    ret = send_dns_packet(fd, p);
    
finish:
    if (p)
        dns_packet_free(p);

    return ret;
}

static struct dns_packet *begin_response()
{
    struct dns_packet *p;

    if (!(p = dns_packet_new()))
	{
        fprintf(stderr, "Failed to allocate DNS packet.\n");
        return NULL;
    }

    dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(1, 0, 1, 0, 0, 0, 0, 0, 0, 0));
	return p;
}

static int append_ptr_response(struct dns_packet *p, int *ancount, const char *response, const char *name, uint32_t ttl)
{
    int ret = -1;
	uint8_t *datalen;
	uint16_t u;
	size_t size;

    assert(p && response && name);

    if (!(dns_packet_append_name(p, response)))
	{
		fprintf(stderr, "Bad response name\n");
		return -1;           
    }
    dns_packet_append_uint16(p, DNS_TYPE_PTR);
    dns_packet_append_uint16(p, DNS_CLASS_IN);
	dns_packet_append_uint32(p, ttl); // TTL

	datalen = dns_packet_append_uint16(p, 0);
	size = p->size;

    if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad dns name\n");
		return -1;
    }
    (*ancount)++;

	u = htons((uint16_t)(p->size - size));
	memcpy(datalen,&u,sizeof(u));
    
	return 0;
}

static int send_response(sock_t fd, struct dns_packet *p, int ancount, int arcount)
{
	int ret;

	assert(fd>=0 && p);

	dns_packet_set_field(p, DNS_FIELD_ANCOUNT, ancount);
	dns_packet_set_field(p, DNS_FIELD_ARCOUNT, arcount);

    ret = send_dns_packet(fd, p);
    
    dns_packet_free(p);

    return ret;
}

static int append_ipv4_response(struct dns_packet *p, int* ancount, const char *name, ipv4_address_t *ipv4, uint32_t ttl)
{
	if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad name\n");
		return -1;           
	}
	dns_packet_append_uint16(p, DNS_TYPE_A);
	dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
	dns_packet_append_uint32(p, ttl);
	dns_packet_append_uint16(p, 4); // IPV4 address length
	dns_packet_append_ipv4(p, ipv4);
	(*ancount)++;
	return 0;
}

static int append_ipv6_response(struct dns_packet *p, int* ancount, const char *name, ipv6_address_t *ipv6, uint32_t ttl)
{
	if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad name\n");
		return -1;           
	}
	dns_packet_append_uint16(p, DNS_TYPE_AAAA);
	dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
	dns_packet_append_uint32(p, ttl);
	dns_packet_append_uint16(p, 16); // IPV4 address length
	dns_packet_append_ipv6(p, ipv6);
	(*ancount)++;
	return 0;
}

static int append_srv_response(struct dns_packet *p, int* ancount, const char *name, int priority, int weight, int port, const char *host, uint32_t ttl)
{
	uint8_t *datalen;
	uint16_t u;
	size_t size;

	if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad name\n");
		return -1;           
	}
	dns_packet_append_uint16(p, DNS_TYPE_SRV);
	dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
	dns_packet_append_uint32(p, ttl);
	datalen = dns_packet_append_uint16(p, 0);
	size = p->size;
	dns_packet_append_uint16(p, priority); // Priority
	dns_packet_append_uint16(p, weight); 
	dns_packet_append_uint16(p, port);
	if (!(dns_packet_append_name(p, host)))
	{
		fprintf(stderr, "Bad dns name\n");
		return -1;           
	}
	u = htons((uint16_t)(p->size - size));
	memcpy(datalen,&u,sizeof(u));
	(*ancount)++;
	return 0;
}

static int append_txt_response(struct dns_packet *p, int* ancount, const char *name, const char *txt, uint32_t ttl)
{
	uint8_t *datalen;
	uint16_t u;
	size_t size;

	if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad name\n");
		return -1;           
	}
	dns_packet_append_uint16(p, DNS_TYPE_TXT);
	dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
	dns_packet_append_uint32(p, ttl);
	datalen = dns_packet_append_uint16(p, 0);
	size = p->size;
	dns_packet_append_text(p, txt);
	u = htons((uint16_t)(p->size - size));
	memcpy(datalen,&u,sizeof(u));
	(*ancount)++;
	return 0;
}

static int send_service_response(sock_t fd, const char *response, const char *name, const char *hostname, uint32_t ttl, ipv4_address_t *ipv4, ipv6_address_t *ipv6, uint16_t port, const char *txt)
{
    int ret = -1;
    struct dns_packet *p = NULL;
    int ancount = 0, arcount = 0;
	uint8_t *datalen;
	uint16_t u;
	size_t size;

    assert(fd >= 0 && response && name);

    if (!(p = dns_packet_new()))
	{
        fprintf(stderr, "Failed to allocate DNS packet.\n");
        goto finish;
    }

    dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(1, 0, 1, 0, 0, 0, 0, 0, 0, 0));

    if (!(dns_packet_append_name(p, response)))
	{
		fprintf(stderr, "Bad response name\n");
		goto finish;           
    }
    dns_packet_append_uint16(p, DNS_TYPE_PTR);
    dns_packet_append_uint16(p, DNS_CLASS_IN);
	dns_packet_append_uint32(p, ttl); // TTL

	datalen = dns_packet_append_uint16(p, 0);
	size = p->size;

    if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad dns name\n");
		goto finish;           
    }
    ancount++;
	dns_packet_set_field(p, DNS_FIELD_ANCOUNT, ancount);

	u = htons((uint16_t)(p->size - size));
	memcpy(datalen,&u,sizeof(u));

	if(ipv4)
	{
		if (!(dns_packet_append_name(p, hostname)))
		{
			fprintf(stderr, "Bad dns name\n");
			goto finish;           
		}
		dns_packet_append_uint16(p, DNS_TYPE_A);
	    dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
		dns_packet_append_uint32(p, 240); // TTL (always 4 minutes?)
		dns_packet_append_uint16(p, 4); // IPV4 address length
		dns_packet_append_ipv4(p, ipv4);
		arcount++;
	}
    
	if(ipv6)
	{
		if (!(dns_packet_append_name(p, hostname)))
		{
			fprintf(stderr, "Bad dns name\n");
			goto finish;           
		}
		dns_packet_append_uint16(p, DNS_TYPE_AAAA);
	    dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
		dns_packet_append_uint32(p, 240); // TTL (always 4 minutes?)
		dns_packet_append_uint16(p, 16); // IPV6 address length
		dns_packet_append_ipv6(p, ipv6);
		arcount++;
	}

	if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad dns name\n");
		goto finish;           
	}
	dns_packet_append_uint16(p, DNS_TYPE_SRV);
	dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
	dns_packet_append_uint32(p, 240); // TTL (always 4 minutes?)
	datalen = dns_packet_append_uint16(p, 0);
	size = p->size;
	dns_packet_append_uint16(p, 0); // Priority
	dns_packet_append_uint16(p, 0); // Weight
	dns_packet_append_uint16(p, port); // Port
	if (!(dns_packet_append_name(p, hostname)))
	{
		fprintf(stderr, "Bad dns name\n");
		goto finish;           
	}
	u = htons((uint16_t)(p->size - size));
	memcpy(datalen,&u,sizeof(u));
	arcount++;

	if (!(dns_packet_append_name(p, name)))
	{
		fprintf(stderr, "Bad dns name\n");
		goto finish;           
	}
	dns_packet_append_uint16(p, DNS_TYPE_TXT);
	dns_packet_append_uint16(p, DNS_CLASS_IN|DNS_CLASS_FLUSH);
	dns_packet_append_uint32(p, 240); // TTL (always 4 minutes?)
	datalen = dns_packet_append_uint16(p, 0);
	size = p->size;
	dns_packet_append_text(p, txt);
	u = htons((uint16_t)(p->size - size));
	memcpy(datalen,&u,sizeof(u));
	arcount++;

	dns_packet_set_field(p, DNS_FIELD_ARCOUNT, arcount);

	ret = send_dns_packet(fd, p);
    
finish:
    if (p)
        dns_packet_free(p);

    return ret;
}

static int domain_cmp(const char *a, const char *b)
{
    size_t al, bl;

    al = strlen(a);
    bl = strlen(b);

    if (al > 0 && a[al-1] == '.')
        al --;

    if (bl > 0 && b[bl-1] == '.')
        bl --;

    if (al != bl)
        return al > bl ? 1 : (al < bl ? -1 : 0);

    return strncasecmp(a, b, al);
}

static int domain_prefix_cmp(const char *a, const char *b)
{
    size_t al, bl;

    al = strlen(a);
    bl = strlen(b);

    if (al > 0 && a[al-1] == '.')
        al --;

    if (bl > 0 && b[bl-1] == '.')
        bl --;

    return strncasecmp(a, b, al);
}

static int process_response(sock_t fd, const char *prefix, uint64_t timeout, struct mdns_callback *callback, void *userdata)
{
    struct dns_packet *p = NULL;
    int done = 0;
    struct timeval end;
	struct sockaddr_storage from;
	int prefix_seen = 0;
    
    assert(fd >= 0 && callback);

	gettimeofday(&end, NULL);
	timeval_add(&end, timeout);
    while (!done)
	{
        int r,ttl;

        if ((r = recv_dns_packet(fd, &p, &end, &from, (int)sizeof(from), &ttl)) < 0)
            return -1;
        else if (r > 0) /* timeout */
            return 1;

        /* Ignore corrupt packets */
        if ((ttl == 255 || ttl == 1) && dns_packet_check_valid_response(p) >= 0)
		{
			/* Reset timeout */
			gettimeofday(&end, NULL);
			timeval_add(&end, timeout);

            for (;;)
			{
                char pname[256];
                uint16_t type, dnsclass;
                uint32_t rr_ttl;
                uint16_t rdlength;
                
                if (dns_packet_consume_name(p, pname, sizeof(pname)) < 0 ||
                    dns_packet_consume_uint16(p, &type) < 0 ||
                    dns_packet_consume_uint16(p, &dnsclass) < 0 ||
                    dns_packet_consume_uint32(p, &rr_ttl) < 0 ||
                    dns_packet_consume_uint16(p, &rdlength) < 0)
				{
                    break;
                }
	
                /* Remove mDNS cache flush bit */
                dnsclass &= ~DNS_CLASS_FLUSH;
                
				if(type == DNS_TYPE_PTR && dnsclass == DNS_CLASS_IN && !domain_cmp(MDNS_SERV, pname))
				{
					char name[256];

					if (dns_packet_consume_name(p, name, sizeof(name)) < 0)
						break;

					if(!prefix || (!prefix_seen && !domain_prefix_cmp(prefix,name)))
					{
						prefix_seen = 1;
						send_service_query(fd, name, 0);
					}
				}
				else if(type == DNS_TYPE_PTR && dnsclass == DNS_CLASS_IN)
				{
					char name[256];

					if (dns_packet_consume_name(p, name, sizeof(name)) < 0)
						break;
                
					if(callback->name_func)
						callback->name_func(name, userdata);
				}
				else if(type == DNS_TYPE_SRV && dnsclass == DNS_CLASS_IN)
				{
					uint16_t priority,weight,port;
					char target[256];
					// srv record
					// Priority Weight Port Target

					dns_packet_consume_uint16(p,&priority);
					dns_packet_consume_uint16(p,&weight);
					dns_packet_consume_uint16(p,&port);
					dns_packet_consume_name(p,target,sizeof(target));
					
					if(strcmp(target,".") && callback->srv_func)
						callback->srv_func(pname, port, target, userdata);

					// All mdns responders seem to send this stuff anyway
//					if(callback->ipv4_func || callback->ipv6_func)
//						send_name_query(fd, target);
//					if(callback->txt_func)
//						send_service_query(fd, pname, SQ_TXT);
				}
				else if(type == DNS_TYPE_TXT && dnsclass == DNS_CLASS_IN)
				{
					char buf[1024];

					if (dns_packet_consume_text(p, buf, sizeof(buf)) < 0)
						break;

					if(callback->txt_func)
						callback->txt_func(pname, buf, userdata);
				}
                else if (type == DNS_TYPE_A &&
						dnsclass == DNS_CLASS_IN &&
						rdlength == sizeof(ipv4_address_t))
				{
                    ipv4_address_t ipv4;
                    
                    if (dns_packet_consume_bytes(p, &ipv4, sizeof(ipv4)) < 0)
                        break;

					if(callback->ipv4_func)
						callback->ipv4_func(pname, &ipv4, userdata);                   
                }
                else if (type == DNS_TYPE_AAAA && dnsclass == DNS_CLASS_IN && rdlength == sizeof(ipv6_address_t))
				{
                    ipv6_address_t ipv6;
                    
                    if (dns_packet_consume_bytes(p, &ipv6, sizeof(ipv6)) < 0)
                        break;
                    
					if(callback->ipv6_func)
						callback->ipv6_func(pname, &ipv6, userdata);                   
                }
				else
				{
                    /* Step over */
                    if (dns_packet_consume_seek(p, rdlength) < 0)
                        break;
                }
            }
        }

        if (p)
            dns_packet_free(p);
    }

    return 0;
}

static int process_server(sock_t fd)
{
    struct dns_packet *p = NULL;
    int done = 0;
    struct timeval end;
	struct sockaddr_storage from;

	uint64_t timeout = 1000;
    
    assert(fd >= 0);

	gettimeofday(&end, NULL);
	timeval_add(&end, timeout);
    while (!done)
	{
        int r,ttl;

        if ((r = recv_dns_packet(fd, &p, &end, &from, (int)sizeof(from), &ttl)) < 0)
            return -1;
        else if (r > 0) /* timeout */
            return 1;

        /* Ignore corrupt packets */
        if ((ttl == 255 || ttl == 1) && dns_packet_check_valid_request(p) >= 0)
		{
			/* Reset timeout */
			gettimeofday(&end, NULL);
			timeval_add(&end, timeout);

            for (;;)
			{
                char pname[256];
                uint16_t type, dnsclass;
                uint32_t rr_ttl;
                uint16_t rdlength;
                
                if (dns_packet_consume_name(p, pname, sizeof(pname)) < 0 ||
                    dns_packet_consume_uint16(p, &type) < 0 ||
                    dns_packet_consume_uint16(p, &dnsclass) < 0)
				{
                    break;
                }

				rr_ttl=0; rdlength=0;

                (dns_packet_consume_uint32(p, &rr_ttl) >=0) &&
                (dns_packet_consume_uint16(p, &rdlength) >=0);

                /* Remove mDNS cache flush bit */
                dnsclass &= ~DNS_CLASS_FLUSH;
                
				if(type == DNS_TYPE_PTR && dnsclass == DNS_CLASS_IN && !domain_cmp(MDNS_SERV, pname))
				{
					mdns_service_item_t *serv = service_root;
					char *service_list[1024], *last;
					int count = 0;
					int n,ancount;
				    struct dns_packet *outp = NULL;

					//printf("dns-sd global service request\n");
					while(serv)
					{
						service_list[count++]=(char*)serv->Service;
						serv=serv->next;
						if(count==1024) break;
					}
					if(count)
					{
						qsort(service_list,count,sizeof(service_list[0]),strcmp);
						last = NULL;
						outp = begin_response();
						ancount=0;
						for(n=0; n<count; n++)
						{
							char tmp[128];
							if(last && !strcmp(last,service_list[n]))
								continue;
							strncpy(tmp,service_list[n],sizeof(tmp)-7);
							strcat(tmp,".local");
							append_ptr_response(outp,&ancount,pname,tmp,3600);
							last = service_list[n];
						}
						send_response(fd,outp,ancount,0);
					}
				}
				else if(type == DNS_TYPE_PTR && dnsclass == DNS_CLASS_IN)
				{
					mdns_service_item_t *serv = service_root;
					int count = 0;
					int ancount,arcount;
				    struct dns_packet *outp = NULL;

					//printf("dns-sd specific service request (%s)\n",pname);
					while(serv)
					{
						if(!domain_prefix_cmp(serv->Service,pname))
							count++;
						serv=serv->next;
					}
					if(count)
					{
						serv = service_root;
						outp = begin_response();
						ancount=arcount=0;
						while(serv)
						{
							if(!domain_prefix_cmp(serv->Service,pname))
							{
								char tmp[256];
								snprintf(tmp,sizeof(tmp),"%s.%s",serv->Instance,pname);
								append_ptr_response(outp,&ancount,pname,tmp,3600);
								if(serv->ipv4)
									append_ipv4_response(outp,&arcount,serv->Location,serv->ipv4,240);
								if(serv->ipv6)
									append_ipv6_response(outp,&arcount,serv->Location,serv->ipv6,240);
								append_srv_response(outp,&arcount,tmp,0,0,serv->Port,serv->Location,240);
								append_txt_response(outp,&arcount,tmp,"",240);
							}
							serv=serv->next;
						}
						send_response(fd,outp,ancount,arcount);
					}
				}
				else if(dnsclass == DNS_CLASS_IN)
				{
					mdns_service_item_t *serv = service_root;
					int count = 0;
					int ancount,arcount;
				    struct dns_packet *outp = NULL;

					//printf("dns-sd specific item request (%s, %d)\n",pname,type);
					while(serv)
					{
						char tmp[256];
						snprintf(tmp,sizeof(tmp),"%s.%s.local",serv->Instance,serv->Service);

						if((type==DNS_TYPE_A && !domain_prefix_cmp(serv->Location,pname) && serv->ipv4) ||
						   (type==DNS_TYPE_AAAA && !domain_prefix_cmp(serv->Location,pname) && serv->ipv6) ||
							((type==DNS_TYPE_SRV || type==DNS_TYPE_TXT) && !domain_cmp(tmp,pname)))
						{
							count++;
						}
						serv=serv->next;
					}
					if(count)
					{
						serv = service_root;
						outp = begin_response();
						ancount=arcount=0;
						while(serv)
						{
							char tmp[256];
							snprintf(tmp,sizeof(tmp),"%s.%s.local",serv->Instance,serv->Service);

							if(type==DNS_TYPE_A && !strcmp(serv->Location,pname) && serv->ipv4)
							{
								if(!strcmp(serv->Location,pname))
									append_ipv4_response(outp,&ancount,pname,serv->ipv4,3600);
							}
							else if(type==DNS_TYPE_AAAA && !strcmp(serv->Location,pname) && serv->ipv6)
							{
								if(!strcmp(serv->Location,pname))
									append_ipv6_response(outp,&ancount,pname,serv->ipv6,3600);
							}
							else if(!strcmp(tmp,pname))
							{
								if(type==DNS_TYPE_SRV)
								{
									append_srv_response(outp,&ancount,tmp,0,0,serv->Port,serv->Location,3600);
									if(serv->ipv4)
										append_ipv4_response(outp,&arcount,serv->Location,serv->ipv4,240);
									if(serv->ipv6)
										append_ipv6_response(outp,&arcount,serv->Location,serv->ipv6,240);
								}
								else if(type==DNS_TYPE_TXT)
									append_txt_response(outp,&ancount,tmp,"",3600);
							}
							serv=serv->next;
						}
						send_response(fd,outp,ancount,arcount);
					}
				}
				else
				{
                    /* Step over */
                    if (dns_packet_consume_seek(p, rdlength) < 0)
                        break;
                }
            }
        }

        if (p)
            dns_packet_free(p);
    }

    return 0;
}

int mdns_query_name(mdnshandle_t handle, const char *name, struct mdns_callback *callback, void *userdata, uint64_t timeout)
{
	int n;

	sock_t fd = (sock_t)handle;
    assert(fd >= 0 && name && callback);

	if(!timeout)
		timeout = 2000000;
      
    if (send_name_query(fd, name) < 0)
		return -1;

	if ((n = process_response(fd, NULL, timeout, callback, userdata)) < 0)
		return -1;

    if (n == 0)
        return 0;

	// Timeout
    return -1;
}

int mdns_query_services(mdnshandle_t handle, const char *prefix, struct mdns_callback *callback, void *userdata, uint64_t timeout)
{
	sock_t fd = (sock_t)handle;
	int n;
    assert(fd >= 0 && callback);

    if(!timeout)
		timeout = 2000000;

	if (send_service_query(fd, MDNS_SERV, 0) < 0)
        return -1;

    if ((n = process_response(fd, prefix, timeout, callback, userdata)) < 0)
        return -1;

    if (n == 0)
       return 0;

	/* Timeout.. normal */
    return 0;
}

static int send_reverse_query(sock_t fd, const char *name)
{
    int ret = -1;
    struct dns_packet *p = NULL;

    assert(fd >= 0 && name);

    if (!(p = dns_packet_new())) {
        fprintf(stderr, "Failed to allocate DNS packet.\n");
        goto finish;
    }

    dns_packet_set_field(p, DNS_FIELD_FLAGS, DNS_FLAGS(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    if (!dns_packet_append_name(p, name))
	{
        fprintf(stderr, "Bad host name\n");
        goto finish;
    }

    dns_packet_append_uint16(p, DNS_TYPE_PTR);
    dns_packet_append_uint16(p, DNS_CLASS_IN);

    dns_packet_set_field(p, DNS_FIELD_QDCOUNT, 1);
    
    ret = send_dns_packet(fd, p);
    
finish:
    if (p)
        dns_packet_free(p);

    return ret;
}

static int query_reverse(mdnshandle_t handle, const char *name, struct mdns_callback *callback, void *userdata, uint64_t timeout)
{
	sock_t fd = (sock_t)handle;
	int n;
    assert(fd >= 0 && callback);

    if(!timeout)
		timeout = 2000000;
        
    if (send_reverse_query(fd, name) <= 0) /* error or no interface to send data on */
        return -1;

    if ((n = process_response(fd, NULL, timeout, callback, userdata)) < 0)
        return -1;

    if (n == 0)
        return 0;

	/* Timeout */
    return -1;
}

int mdns_query_ipv4(mdnshandle_t handle, const ipv4_address_t *ipv4, struct mdns_callback *callback, void *userdata, uint64_t timeout)
{
    char name[256];
    assert(handle && callback && ipv4);

    snprintf(name, sizeof(name), "%u.%u.%u.%u.in-addr.arpa", ipv4->address[0],ipv4->address[1],ipv4->address[2],ipv4->address[3]);

    return query_reverse(handle, name, callback, userdata, timeout);
}

int mdns_query_ipv6(mdnshandle_t handle, const ipv6_address_t *ipv6, struct mdns_callback *callback, void *userdata, uint64_t timeout)
{
    char name[256];
    assert(handle && ipv6 && callback);

    snprintf(name, sizeof(name), "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa",
             ipv6->address[15] & 0xF, ipv6->address[15] >> 4,
             ipv6->address[14] & 0xF, ipv6->address[14] >> 4,
             ipv6->address[13] & 0xF, ipv6->address[13] >> 4,
             ipv6->address[12] & 0xF, ipv6->address[12] >> 4,
             ipv6->address[11] & 0xF, ipv6->address[11] >> 4,
             ipv6->address[10] & 0xF, ipv6->address[10] >> 4,
             ipv6->address[9] & 0xF, ipv6->address[9] >> 4,
             ipv6->address[8] & 0xF, ipv6->address[8] >> 4,
             ipv6->address[7] & 0xF, ipv6->address[7] >> 4,
             ipv6->address[6] & 0xF, ipv6->address[6] >> 4,
             ipv6->address[5] & 0xF, ipv6->address[5] >> 4,
             ipv6->address[4] & 0xF, ipv6->address[4] >> 4,
             ipv6->address[3] & 0xF, ipv6->address[3] >> 4,
             ipv6->address[2] & 0xF, ipv6->address[2] >> 4,
             ipv6->address[1] & 0xF, ipv6->address[1] >> 4,
             ipv6->address[0] & 0xF, ipv6->address[0] >> 4);
    
    return query_reverse(handle, name, callback, userdata, timeout);
}

int mdns_server_step(mdnshandle_t handle)
{
	sock_t fd = (sock_t)handle;
	int n;
    assert(fd >= 0);

	if ((n = process_server(fd)) < 0)
		return -1;

	if (n == 0)
	    return 1;
	
	return 0;
}

int mdns_add_service(mdnshandle_t handle, mdns_service_item_t *item)
{
	item->next = service_root;
	service_root = item;
	return 0;
}
