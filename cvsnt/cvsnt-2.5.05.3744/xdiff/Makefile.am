## Process this file with automake to produce Makefile.in
#
# $(includeopt) is CVS specific and set by configure
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi -I$(top_srcdir)/cvstools $(includeopt)

libdir=$(cvs_library_dir)/xdiff

ext_target = extdiff.la

extdiff_la_SOURCES = \
	ext_xdiff.cpp \
	xdiff.h

extdiff_la_LDFLAGS = -module -avoid-version
extdiff_la_LIBADD = ../cvsapi/libcvsapi.la  -L$(DESTDIR)/$(libdir)

lib_LTLIBRARIES = $(ext_target)

