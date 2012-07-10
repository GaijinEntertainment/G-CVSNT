/* $Id: daemon.h,v 1.1.2.2 2004/02/12 21:09:53 tmh Exp $ */

#ifndef _BSD_DAEMON_H
#define _BSD_DAEMON_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_DAEMON
int daemon(int nochdir, int noclose);
#endif /* !HAVE_DAEMON */

#ifdef __cplusplus
}
#endif

#endif /* _BSD_DAEMON_H */
