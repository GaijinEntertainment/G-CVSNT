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
// Microsoft braindamage reversal. 
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _WIN32
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/socket.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>

#include "mdnsclient.h"
#include "util.h"

/* u_int64_t is sufficiently uncommon to require this */
#ifdef _WIN32
typedef unsigned __int64 local_u_int64_t;
#else
typedef unsigned long long local_u_int64_t;
#endif

/* Calculate the difference between the two specfified timeval
 * timestamsps. */
usec_t timeval_diff(const struct timeval *a, const struct timeval *b) {
    usec_t r;
    assert(a && b);

    /* Check which whan is the earlier time and swap the two arguments if reuqired. */
    if (timeval_cmp(a, b) < 0) {
        const struct timeval *c;
        c = a;
        a = b;
        b = c;
    }

    /* Calculate the second difference*/
    r = ((usec_t) a->tv_sec - b->tv_sec)* 1000000;

    /* Calculate the microsecond difference */
    if (a->tv_usec > b->tv_usec)
        r += ((usec_t) a->tv_usec - b->tv_usec);
    else if (a->tv_usec < b->tv_usec)
        r -= ((usec_t) b->tv_usec - a->tv_usec);

    return r;
}

/* Compare the two timeval structs and return 0 when equal, negative when a < b, positive otherwse */
int timeval_cmp(const struct timeval *a, const struct timeval *b) {
    assert(a && b);

    if (a->tv_sec < b->tv_sec)
        return -1;

    if (a->tv_sec > b->tv_sec)
        return 1;

    if (a->tv_usec < b->tv_usec)
        return -1;

    if (a->tv_usec > b->tv_usec)
        return 1;

    return 0;
}

/* Return the time difference between now and the specified timestamp */
usec_t timeval_age(const struct timeval *tv) {
    struct timeval now;
    assert(tv);
    gettimeofday(&now, NULL);
    return timeval_diff(&now, tv);
}

/* Add the specified time inmicroseconds to the specified timeval structure */
void timeval_add(struct timeval *tv, usec_t v) {
    local_u_int64_t secs;
    assert(tv);
    
    secs = (v/1000000);
    tv->tv_sec += (unsigned long) secs;
    v -= secs*1000000;

    tv->tv_usec += (long)v;

    /* Normalize */
    while (tv->tv_usec >= 1000000) {
        tv->tv_sec++;
        tv->tv_usec -= 1000000;
    }
}

int set_cloexec(sock_t fd) 
{
#ifndef _WIN32
    int n;
    assert(fd >= 0);
    
    if ((n = fcntl(fd, F_GETFD)) < 0)
        return -1;

    if (n & FD_CLOEXEC)
        return 0;

    return fcntl(fd, F_SETFD, n|FD_CLOEXEC);
#else
	return 0;
#endif
}

int set_nonblock(sock_t fd)
{
#ifndef _WIN32
    int n;
    assert(fd >= 0);

    if ((n = fcntl(fd, F_GETFL)) < 0)
        return -1;

    if (n & O_NONBLOCK)
        return 0;

    return fcntl(fd, F_SETFL, n|O_NONBLOCK);
#else
	 int n = 1;
     if (ioctlsocket(fd, FIONBIO, &n) == SOCKET_ERROR)
     {
		 return -1;
     }
	 return 0;
#endif
}

int wait_for_write(sock_t fd, struct timeval *end) {
    struct timeval now;

    if (end)
        gettimeofday(&now, NULL);
    
    for (;;) {
        struct timeval tv;
        fd_set fds;
        int r;
        
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if (end) {
            if (timeval_cmp(&now, end) >= 0)
                return 1;

            tv.tv_sec = tv.tv_usec = 0;
            timeval_add(&tv, timeval_diff(end, &now));
        }

        if ((r = select((int)fd+1, NULL, &fds, NULL, end ? &tv : NULL)) < 0) {
            if (errno != EINTR) {
                fprintf(stderr, "select() failed: %s\n", strerror(errno));
                return -1;
            }
        } else if (r == 0)
            return 1;
        else {
            if (FD_ISSET(fd, &fds))
                return 0;
        }

        if (end)
            gettimeofday(&now, NULL);
    }
}

int wait_for_read(sock_t fd, struct timeval *end) {
    struct timeval now;

    if (end)
        gettimeofday(&now, NULL);

    for (;;) {
        struct timeval tv;
        fd_set fds;
        int r;
        
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if (end) {
            if (timeval_cmp(&now, end) >= 0)
                return 1;
            
            tv.tv_sec = tv.tv_usec = 0;
            timeval_add(&tv, timeval_diff(end, &now));
        }
        
        if ((r = select((int)fd+1, &fds, NULL, NULL, end ? &tv : NULL)) < 0) {
            if (errno != EINTR) {
                fprintf(stderr, "select() failed: %s\n", strerror(errno));
                return -1;
            }
        } else if (r == 0) 
            return 1;
        else {
            
            if (FD_ISSET(fd, &fds))
                return 0;
        }

        if (end)
            gettimeofday(&now, NULL);
    }
}

