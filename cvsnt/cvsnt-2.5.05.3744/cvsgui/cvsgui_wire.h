/*
** The cvsgui protocol used by WinCvs
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*!
	\file cvsgui_wire.h
	\brief CvsGui wire code header
	\author Alexandre Parenteau <aubonbeurre@hotmail.com> --- November 1999
	\note To be used by GUI and CVS client
	\note Derived from gimpwire.h in GIMP
*/

#ifndef CVSGUI_WIRE_H
#define CVSGUI_WIRE_H

#ifdef WIN32
#	include <Windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __G_LIB_H__
typedef signed char gint8;
typedef unsigned char guint8;
typedef signed short gint16;
typedef unsigned short guint16;
typedef signed int gint32;
typedef unsigned int guint32;

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char	guchar;
typedef unsigned short	gushort;
typedef unsigned long	gulong;
typedef unsigned int	guint;

typedef float	gfloat;
typedef double	gdouble;

typedef void* gpointer;
typedef const void *gconstpointer;

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif
#endif // !__G_LIB_H__

#ifdef WIN32
#	ifdef pid_t
#		undef pid_t // config.h of cvs
#	endif
	typedef HANDLE pid_t;
	typedef HANDLE pipe_t;
#else
	typedef int pipe_t;
#endif

typedef struct _WireMessage WireMessage;

typedef void (*WireReadFunc)	(pipe_t fd, WireMessage* msg);
typedef void (*WireWriteFunc)	(pipe_t fd, WireMessage* msg);
typedef void (*WireDestroyFunc)	(WireMessage* msg);
typedef int  (*WireIOFunc)		(pipe_t fd, guint8* buf, gulong count);
typedef int  (*WireFlushFunc)	(pipe_t fd);

/// Wire message structure
struct _WireMessage
{
	guint32 type;	/*!< Message type */
	gpointer data;	/*!< Data */
};

void wire_register(guint32 type, WireReadFunc read_func, WireWriteFunc write_func, WireDestroyFunc destroy_func);

void wire_set_reader(WireIOFunc read_func);
void wire_set_writer(WireIOFunc write_func);
void wire_set_flusher(WireFlushFunc flush_func);

int wire_read(pipe_t fd, guint8 *buf, gulong count);
int wire_write(pipe_t fd, guint8 *buf, gulong count);
int wire_flush(pipe_t fd);

int wire_error(void);
void wire_clear_error(void);

int wire_read_msg(pipe_t fd, WireMessage* msg);
int wire_write_msg(pipe_t fd, WireMessage* msg);
void wire_destroy(WireMessage* msg);

int wire_read_int32(pipe_t fd, guint32* data, gint count);
int wire_read_int16(pipe_t fd, guint16* data, gint count);
int wire_read_int8(pipe_t fd, guint8* data, gint count);
int wire_read_double(pipe_t fd, gdouble* data, gint count);
int wire_read_string(pipe_t fd, gchar** data, gint count);

int wire_write_int32(pipe_t fd, guint32* data, gint count);
int wire_write_int16(pipe_t fd, guint16* data, gint count);
int wire_write_int8(pipe_t fd, guint8* data, gint count);
int wire_write_double(pipe_t fd, gdouble* data, gint count);
int wire_write_string(pipe_t fd, gchar** data, gint count, int len);

#ifdef __cplusplus
}
#endif

#endif /* CVSGUI_WIRE_H */
