## Makefile.am for CVS GUI Interface
## Copyright (C) 2001 Free Software Foundation, Inc.
##
## This file is part of CVS GUI Interface.
##
## CVS GUI Interface is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## CVS GUI Interface is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/src

noinst_LTLIBRARIES = libcvsgui.la

libcvsgui_la_SOURCES = \
	cvsgui_process.cpp \
	cvsgui_wire.cpp \
	cvsgui.cpp \
	cvsgui.h \
	cvsgui_process.h \
	cvsgui_protocol.h \
	cvsgui_wire.h

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean

