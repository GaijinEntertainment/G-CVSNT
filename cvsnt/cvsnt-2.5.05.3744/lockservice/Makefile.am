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

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi -I$(top_srcdir)/cvstools $(includeopt) @PTHREAD_CPPFLAGS@

cvs_targets = cvslockd

cvslockd_SOURCES = \
	cvslockd.cpp \
	LockParse.cpp \
	server.cpp 

bin_PROGRAMS=$(cvs_targets)

cvslockd_LDADD = \
	-L$(DESTDIR)/$(libdir) \
	../lib/libcvs.a ../cvsapi/libcvsapi.la ../cvstools/libcvstools.la \
	@PTHREAD_LIBS@

EXTRA_DIST = \
	.cvsignore 

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
