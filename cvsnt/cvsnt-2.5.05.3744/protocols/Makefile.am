## Process this file with automake to produce Makefile.in
# Makefile for library files used by GNU CVS.
# Copyright (C) 1986, 1988-1994, 2000 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# For now we need to include $(top_srcdir)/src because some systems
# (at least 'AIX rioscpu2 3 4 000030498200',
# 'HP-UX hp60 B.10.20 A 9000/770 hp60 two-user license', &
# 'IRIX64 sgiop110 6.5 07151433 IP30') have trouble finding error.h
# when compiling savecwd.c
#
# $(includeopt) is CVS specific and set by configure
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi -I$(top_srcdir)/cvstools $(includeopt) 

libdir=$(cvs_library_dir)/protocols

if PSERVER_PROTOCOL_SUPPORT

pserver_target = pserver.la

pserver_la_SOURCES = \
	pserver.cpp \
	common.cpp 

pserver_la_LDFLAGS = -module -avoid-version -L$(DESTDIR)/$(libdir)

else

pserver_target = 

endif 

if SSERVER_PROTOCOL_SUPPORT

sserver_target = sserver.la

sserver_la_SOURCES = \
	sserver.cpp \
	common.cpp 

sserver_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir) @SSL_LIBS@

else

sserver_target = 

endif 

#if SYNC_PROTOCOL_SUPPORT
#
#sync_target = sync.la
#
#sync_la_SOURCES = \
#	sync.cpp \
#	common.cpp 
#
#sync_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir) @SSL_LIBS@
#
#else

sync_target = 

#endif 

if SSPI_PROTOCOL_SUPPORT

sspi_target = sspi.la

sspi_la_SOURCES = \
	sspi_unix.cpp \
	common.cpp 

sspi_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir) ntlm/libntlm.la

sspi_ntlm = ntlm

else

sspi_target = 
sspi_ntlm = 

endif 

if EXT_PROTOCOL_SUPPORT

ext_target = ext.la

ext_la_SOURCES = \
	ext.cpp \
	common.cpp 

ext_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)

else

ext_target = 

endif 

if FORK_PROTOCOL_SUPPORT

fork_target = fork.la

fork_la_SOURCES = \
	fork.cpp \
	common.cpp 

fork_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)

else

fork_target = 

endif 

if SERVER_PROTOCOL_SUPPORT

server_target = server.la

server_la_SOURCES = \
	server.cpp \
	common.cpp 

server_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)

else

server_target = 

endif 

if ENUM_PROTOCOL_SUPPORT

enum_target = enum.la

enum_la_SOURCES = \
	enum.cpp \
	common.cpp 

enum_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)

else

enum_target = 

endif 

if GSERVER_PROTOCOL_SUPPORT

gserver_target = gserver.la

gserver_la_SOURCES = \
	gserver.cpp \
	common.cpp

gserver_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir) @GSSAPI_LIBS@

else

gssapi_target = 

endif

SUBDIRS = $(sspi_ntlm)

lib_LTLIBRARIES = $(pserver_target) \
	      $(sserver_target) \
	      $(sync_target) \
	      $(sspi_target) \
	      $(gserver_target) \
	      $(ext_target) \
	      $(fork_target) \
	      $(server_target) \
	      $(enum_target)

EXTRA_DIST = \
	.cvsignore 

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
