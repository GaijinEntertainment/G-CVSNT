/*
	CVSNT Helper application API
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
#ifndef PLUGIN_INTERFACE__H
#define PLUGIN_INTERFACE__H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define CVS_EXPORT __declspec(dllexport)
#elif defined(HAVE_GCC_VISIBILITY)
#define CVS_EXPORT __attribute__ ((visibility("default")))
#else
#define CVS_EXPORT
#endif

enum
{
	pitNone,
	pitProtocol,
	pitTrigger,
	pitXdiff
};

struct plugin_interface
{
public:
	unsigned short interface_version;

	const char *description;
	const char *version;
	const char *key; // key used for enable in 'plugins' section
	int (*init)(const struct plugin_interface *ui);
	int (*destroy)(const struct plugin_interface *ui);
	void *(*get_interface)(const struct plugin_interface *plugin, unsigned interface_type, void *data);
	int (*configure)(const struct plugin_interface *ui, void *data); // For Win32, parent HWND
	void *(*get_command_interface)(const struct plugin_interface *plugin, unsigned interface_type, void *data);

	void *__cvsnt_reserved;
};

typedef plugin_interface* (*get_plugin_interface_t)();

#ifndef _WIN32
#ifdef MODULE
/* This needs an extra level of indirection.  gcc bug..? */
#define __x11433a(__mod,__func) __mod##_LTX_##__func
#define __x11433(__mod,__func) __x11433a(__mod,__func)
#define get_plugin_interface __x11433(MODULE,get_plugin_interface)
#endif
#endif

CVS_EXPORT plugin_interface *get_plugin_interface();

#define PLUGIN_INTERFACE_VERSION 0x510

#undef CVS_EXPORT

#ifdef __cplusplus
}
#endif

#endif
