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
	\file cvsgui_wire.cpp
	\brief CvsGui wire code implementation
	\author Alexandre Parenteau <aubonbeurre@hotmail.com> --- November 1999
	\note To be used by GUI and CVS client
	\note Derived from gimpwire.c in GIMP
*/

#include <StdAfx.h>
#ifdef HAVE_CONFIG_H
extern "C" {
#include "config.h"
}
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include "cvsgui_protocol.h"
#include "cvsgui_wire.h"

#ifndef WIN32
#	include <unistd.h>
#	include <sys/param.h>
#	include <netinet/in.h>
#else
#	pragma warning (disable : 4786)
#endif

#ifdef WIN32
/*!
	Read the data from pipe
	\param fd Pipe
	\param buf Buffer for data read
	\param count Number of bytes to read
	\return The number of bytes read, -1 on error
*/
static int read(pipe_t fd, void* buf, int count)
{
	DWORD read;
	if( !ReadFile(fd, buf, count, &read, NULL) )
	{
		DWORD err = GetLastError();
		errno = EINVAL;
	
		return -1;
	}

	return read;
}

/*!
	Write the data to the pipe
	\param fd Pipe
	\param buf Buffer for data to write
	\param count Number of bytes to write
	\return The number of bytes written, -1 on error
*/
static int write(pipe_t fd, const void* buf, int count)
{
	DWORD dwrite;
	if( !WriteFile(fd, buf, count, &dwrite, NULL) )
	{
		errno = EINVAL;
		return -1;
	}

	return dwrite;
}
#endif

typedef struct _WireHandler  WireHandler;

/// Wire handler structure
struct _WireHandler
{
	guint32 type;					/*!< Type */
	WireReadFunc read_func;			/*!< Read function */
	WireWriteFunc write_func;		/*!< Write function */
	WireDestroyFunc destroy_func;	/*!< Destroy function */
};

static WireIOFunc wire_read_func = NULL;	  /*!< Read function */
static WireIOFunc wire_write_func = NULL;	  /*!< Write function */
static WireFlushFunc wire_flush_func = NULL;  /*!< Flush function */
static int wire_error_val = FALSE;			  /*!< Error value */

/// Handlers container wrapper
struct CAllHandlers
{
public:
	// Construction
	CAllHandlers() 
	{
	}
	
	~CAllHandlers()
	{
	}

	WireHandler* FindHandler(guint32 type) 
	{
		if( type >= sizeof(m_wireHandlers) / sizeof(*m_wireHandlers) )
			return NULL;

		return &m_wireHandlers[type];
	}

private:
	// Data members
	WireHandler m_wireHandlers[GP_MAX];
} sHandlers;

void wire_register(guint32 type, WireReadFunc read_func, WireWriteFunc write_func, WireDestroyFunc destroy_func)
{
	WireHandler* handler = sHandlers.FindHandler(type);

	if( !handler )
		return;

	handler->type = type;
	handler->read_func = read_func;
	handler->write_func = write_func;
	handler->destroy_func = destroy_func;
}

/*!
	Set the read function
	\param read_func Read function
*/
void wire_set_reader(WireIOFunc read_func)
{
	wire_read_func = read_func;
}

/*!
	Set the write function
	\param write_func Write function
*/
void wire_set_writer(WireIOFunc write_func)
{
	wire_write_func = write_func;
}

/*!
	Set the flush function
	\param flush_func Flush function
*/
void wire_set_flusher(WireFlushFunc flush_func)
{
	wire_flush_func = flush_func;
}

/*!
	Read from the pipe
	\param fd Pipe
	\param buf Buffer for data read
	\param count Count of data read
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read(pipe_t fd, guint8 *buf, gulong count, gulong *rcount)
{
	gulong totalcount = count;

	if( wire_read_func )
	{
		if( !(*wire_read_func)(fd, buf, count) )
		{
			//g_print("wire_read: error\n");
			wire_error_val = TRUE;
			return FALSE;
		}
	}
	else
	{
		int bytes;
		
		while( count > 0 )
		{
			do
			{
				bytes = read(fd, (char*)buf, count);
			}while( (bytes == -1) && ((errno == EAGAIN) || (errno == EINTR))) ;
			
			if( bytes == -1 )
			{
				//g_print("wire_read: error\n");
				wire_error_val = TRUE;
				return FALSE;
			}
			
			if( bytes == 0 )
			{
				//g_print("wire_read: unexpected EOF (plug-in crashed?)\n");
				wire_error_val = TRUE;
				return FALSE;
			}
			
			count -= bytes;
			buf += bytes;
		}
	}

	*rcount += totalcount;
	
	return TRUE;
}

/*!
	Write to the pipe
	\param fd Pipe
	\param buf Buffer of data to be written
	\param count Count of data to be written
	\return TRUE on success, FALSE otherwise
*/
int wire_write(pipe_t fd, guint8* buf, gulong count)
{
	if( wire_write_func )
	{
		if( !(*wire_write_func)(fd, buf, count) )
		{
			//g_print("wire_write: error\n");
			wire_error_val = TRUE;
			return FALSE;
		}
	}
	else
	{
		int bytes;

		while( count > 0 )
		{
			do 
			{
				bytes = write(fd, (char*)buf, count);
			}while( (bytes == -1) && ((errno == EAGAIN) || (errno == EINTR)) );

			if( bytes == -1 )
			{
				//g_print("wire_write: error\n");
				wire_error_val = TRUE;
				return FALSE;
			}

			count -= bytes;
			buf += bytes;
		}
	}

	return TRUE;
}

/*!
	Flush the pipe
	\param fd Pipe
	\return TRUE on success, FALSE otherwise
	\note 
*/
int wire_flush(pipe_t fd)
{
	if( wire_flush_func )
		return (*wire_flush_func)(fd);

	return FALSE;
}

/*!
	Get the wire error
	\return The wire error value
*/
int wire_error()
{
	return wire_error_val;
}

/*!
	Clear the wire error
	\note Set the wire error value to FALSE
*/
void wire_clear_error()
{
	wire_error_val = FALSE;
}

/*!
	Read the message from the pipe
	\param fd Pipe
	\param msg Message read from the pipe
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read_msg(pipe_t fd, WireMessage* msg, gulong* rcount)
{
	if( wire_error_val )
		return !wire_error_val;

	if( !wire_read_int32(fd, &msg->type, 1, rcount) )
		return FALSE;

	WireHandler* handler = sHandlers.FindHandler(msg->type);
	if( !handler )
		return FALSE;

	(*handler->read_func)(fd, msg, rcount);

	return !wire_error_val;
}

/*!
	Write the message to the pipe
	\param fd Pipe
	\param msg Message to write to the pipe
	\return TRUE on success, FALSE otherwise
*/
int wire_write_msg(pipe_t fd, WireMessage* msg)
{
	

	if( wire_error_val )
		return !wire_error_val;

	WireHandler* handler = sHandlers.FindHandler(msg->type);
	if( !handler )
		return FALSE;

	if( !wire_write_int32(fd, &msg->type, 1) )
		return FALSE;

	(*handler->write_func)(fd, msg);

	return !wire_error_val;
}

/*!
	Destroy wire message
	\param msg Message to be destroyed
*/
void wire_destroy(WireMessage* msg)
{
	WireHandler* handler = sHandlers.FindHandler(msg->type);
	if( !handler )
		return;

	(*handler->destroy_func)(msg);
}

/*!
	Read 32 bit integers from the pipe
	\param fd Pipe
	\param data Data to be read
	\param count Count of integers to be read
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read_int32(pipe_t fd, guint32* data, gint count, gulong* rcount)
{
	if( count > 0 )
    {
		if( !wire_read_int8(fd, (guint8*)data, count * 4, rcount) )
			return FALSE;

		while( count-- )
		{
			*data = ntohl(*data);
			data++;
		}
    }

	return TRUE;
}

/*!
	Read 16 bit integers from the pipe
	\param fd Pipe
	\param data Data to be read
	\param count Count of integers to be read
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read_int16(pipe_t fd, guint16* data, gint count, gulong* rcount)
{
	if( count > 0 )
    {
		if( !wire_read_int8(fd, (guint8*)data, count * 2, rcount) )
			return FALSE;

		while( count-- )
		{
			*data = ntohs(*data);
			data++;
		}
    }

	return TRUE;
}

/*!
	Read 8 bit integers from the pipe
	\param fd Pipe
	\param data Data to be read
	\param count Count of integers to be read
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read_int8(pipe_t fd, guint8* data, gint count, gulong* rcount)
{
	return wire_read(fd, data, count, rcount);
}

/*!
	Read double values from the pipe
	\param fd Pipe
	\param data Data to be read
	\param count Count of double values to be read
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read_double(pipe_t fd, gdouble* data, gint count, gulong* rcount)
{
	char* str;
	int i;

	for(i = 0; i < count; i++)
    {
		if( !wire_read_string(fd, &str, 1, rcount) )
			return FALSE;

		sscanf(str, "%le", &data[i]);
		free(str);
    }

	return TRUE;
}

/*!
	Read strings from the pipe
	\param fd Pipe
	\param data Data to be read
	\param count Count of strings to be read
	\param rcount Received data count
	\return TRUE on success, FALSE otherwise
*/
int wire_read_string(pipe_t fd, gchar** data, gint count, gulong* rcount)
{
	guint32 tmp;
	int i;

	for(i = 0; i < count; i++)
    {
		if( !wire_read_int32(fd, &tmp, 1, rcount) )
			return FALSE;

		if( tmp > 0 )
		{
			data[i] = (gchar*)malloc(tmp * sizeof(gchar));
			if( !wire_read_int8(fd, (guint8*)data[i], tmp, rcount) )
			{
				free(data[i]);
				return FALSE;
			}
		}
		else
		{
			data[i] = NULL;
		}
    }

	return TRUE;
}

/*!
	Write 32 bit integers to the pipe
	\param fd Pipe
	\param data Data to be written
	\param count Count of integers to be written
	\return TRUE on success, FALSE otherwise
*/
int wire_write_int32(pipe_t fd, guint32* data, gint count)
{
	guint32 tmp;
	int i;

	if( count > 0 )
    {
		for(i = 0; i < count; i++)
		{
			tmp = htonl(data[i]);
			if( !wire_write_int8(fd, (guint8*)&tmp, 4) )
				return FALSE;
		}
    }

	return TRUE;
}

/*!
	Write 16 bit integers to the pipe
	\param fd Pipe
	\param data Data to be written
	\param count Count of integers to be written
	\return TRUE on success, FALSE otherwise
*/
int wire_write_int16(pipe_t fd, guint16* data, gint count)
{
	guint16 tmp;
	int i;

	if( count > 0 )
    {
		for(i = 0; i < count; i++)
		{
			tmp = htons(data[i]);
			if( !wire_write_int8(fd, (guint8*)&tmp, 2) )
				return FALSE;
		}
    }

	return TRUE;
}

/*!
	Write 8 bit integers to the pipe
	\param fd Pipe
	\param data Data to be written
	\param count Count of integers to be written
	\return TRUE on success, FALSE otherwise
*/
int wire_write_int8(pipe_t fd, guint8* data, gint count)
{
	return wire_write(fd, data, count);
}

/*!
	Write double values to the pipe
	\param fd Pipe
	\param data Data to be written
	\param count Count of double values to be written
	\return TRUE on success, FALSE otherwise
*/
int wire_write_double(pipe_t fd, gdouble* data, gint count)
{
	gchar* t, buf[128];
	int i;

	t = buf;
	for(i = 0; i < count; i++)
    {
		sprintf(buf, "%0.50e", data[i]);
		if( !wire_write_string(fd, &t, 1, -1) )
			return FALSE;
    }

	return TRUE;
}

/*!
	Write strings from the pipe
	\param fd Pipe
	\param data Data to be written
	\param count Count of strings to be written
	\param len Strings length, if -1 then it's NULL-terminated string, otherwise it's a binary data
	\return TRUE on success, FALSE otherwise
*/
int wire_write_string(pipe_t fd, gchar** data, gint count, int len)
{
	guint32 tmp;
	int i;

	for(i = 0; i < count; i++)
    {
		if( data[i] )
			tmp = static_cast<guint32>(len == -1 ? strlen(data[i]) + 1 : len + 1);
		else
			tmp = 0;

		if( !wire_write_int32(fd, &tmp, 1) )
			return FALSE;

		if( tmp > 0 )
			if( !wire_write_int8(fd, (guint8*)data[i], tmp) )
				return FALSE;
    }

	return TRUE;
}
