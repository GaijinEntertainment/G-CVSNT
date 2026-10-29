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

lib_LTLIBRARIES = $(pserver_target)

EXTRA_DIST = \
	.cvsignore 

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
