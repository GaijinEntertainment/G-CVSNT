## Process this file with automake to produce Makefile.in

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# $(includeopt) is CVS specific and set by configure
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi -I$(top_srcdir)/cvstools $(includeopt) \
        -DCVS_CONFIG_DIR=\"$(cvs_config_dir)\" \
        -DCVS_LIBRARY_DIR=\"$(cvs_library_dir)\" 

libdir = $(cvs_library_dir)/triggers

default_target = info.la

info_la_SOURCES = info_trigger.cpp 

info_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
info_la_LIBADD = ../cvsapi/libcvsapi.la ../cvstools/libcvstools.la

email_target = email.la

email_la_SOURCES = email_trigger.cpp 

email_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
email_la_LIBADD = ../cvsapi/libcvsapi.la ../cvstools/libcvstools.la

audit_target = audit.la

audit_la_SOURCES = audit_trigger.cpp 

audit_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
audit_la_LIBADD = ../cvsapi/libcvsapi.la ../cvstools/libcvstools.la

checkout_target = checkout.la

checkout_la_SOURCES = checkout_trigger.cpp 

checkout_la_LDFLAGS = -module -avoid-version  -L$(DESTDIR)/$(libdir)
checkout_la_LIBADD = ../cvsapi/libcvsapi.la ../cvstools/libcvstools.la

lib_LTLIBRARIES = $(default_target) $(email_target) $(audit_target) $(checkout_target)

EXTRA_DIST = \
	.cvsignore 

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
