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
	\file cvsgui_protocol.h
	\brief CvsGui protocol code header
	\author Alexandre Parenteau <aubonbeurre@hotmail.com> --- January 2000
	\note To be used by GUI and CVS client
	\note Derived from gimpprotocol.h in GIMP
*/

#ifndef CVSGUI_PROTOCOL_H
#define CVSGUI_PROTOCOL_H

#include "cvsgui_wire.h"

/// Wire message type enum
enum {
	GP_QUIT,	/*!< Exit code */
	GP_GETENV,	/*!< Environment variable */
	GP_CONSOLE	/*!< Data */
};

#ifdef __cplusplus
extern "C" {
#endif

/// Exit code structure
typedef struct
{
	gint32 code;	/*!< Exit code */
} GPT_QUIT;

/// Environment variable structure
typedef struct
{
	guint8 empty;	/*!< Flag to indicate the empty variable */
	char* str;		/*!< Variable name */
} GPT_GETENV;

/// Data structure
typedef struct
{
	guint8 isStderr;	/*!< Flag to indicate <b>stdout</b> or <b>stderr</b> */
	guint32 len;		/*!< Data length */
	char* str;			/*!< Data */
} GPT_CONSOLE;

int gp_quit_write(pipe_t fd, int code);
int gp_getenv_write(pipe_t fd, const char* env);
int gp_console_write(pipe_t fd, const char* str, int len, int isStderr, int binary);
char* gp_getenv_read(pipe_t fd);

extern pipe_t _cvsgui_readfd;
extern pipe_t _cvsgui_writefd;

#ifdef __cplusplus
}
#endif

#endif /* CVSGUI_PROTOCOL_H */
