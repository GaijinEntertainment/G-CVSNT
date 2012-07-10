/* CVS auth common routines
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __COMMON__H
#define __COMMON__H

#include <config.h>
#include <cvsapi.h>
#include <cvstools.h>

#ifndef HAVE_GETADDRINFO
#include "socket.h"
#include "getaddrinfo.h"
#endif

#ifndef HAVE_INET_ATON
#include "inet_aton.h"
#endif

#ifdef	sun
/* solaris has a poor implementation of vsnprintf() which is not able to handle null pointers for %s */
# define PATCH_NULL(x) ((x)?(x):"<NULL>")
#else
# define PATCH_NULL(x) x
#endif

void set_current_server(const struct server_interface *_cs);
const struct server_interface *current_server();

const char *enum_protocols(int *context, enum proto_type type);
int server_error(int fatal, const char *fmt, ...);
int server_getc(const struct protocol_interface *protocol);
int server_getline(const struct protocol_interface *protocol, char** buffer, int buffer_max);
int server_printf(const char *fmt, ...);
const char *get_default_port(const cvsroot *root);
int set_encrypted_channel(int encrypt);

/* TCP/IP helper functions */
int get_tcp_fd();
int tcp_printf(char *fmt, ...);
int tcp_connect(const cvsroot *cvsroot);
int tcp_connect_bind(const char *servername, const char *remote_port, int min_local_port, int max_local_port);
int tcp_disconnect();
int tcp_read(void *data, int length);
int tcp_write(const void *data, int length);
int tcp_shutdown();
int tcp_readline(char* buffer, int buffer_len);

int run_command(const char *cmd, int* in_fd, int* out_fd, int* err_fd);
const char* get_username(const cvsroot* current_root);

int base64enc(const unsigned char *in, unsigned char *out, int len);
int base64dec(const unsigned char *in, unsigned char *out, int len);

#ifdef _WIN32
extern HMODULE g_hInst;

int win32config(const struct plugin_interface *ui, void *wnd);
#endif

#endif
