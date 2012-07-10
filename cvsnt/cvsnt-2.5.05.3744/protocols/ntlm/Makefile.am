## Process this file with automake to produce Makefile.in
# Copyright (C) 2002, 2003, 2004, 2005 Simon Josefsson.
#
# This file is part of Libntlm.
#
# Libntlm is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Libntlm is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Libntlm; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA

SUBDIRS = lib

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = libntlm.pc.in test.txt libntlm.spec

#pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = libntlm.pc

noinst_LTLIBRARIES = libntlm.la

#include_HEADERS = ntlm.h

AM_CPPFLAGS = -I$(srcdir)/lib -DNTLM_SRCDIR=\"$(srcdir)\"

libntlm_la_SOURCES = des.h des.c md4.h md4.c \
	smbutil.c smbencrypt.c smbencrypt.h \
	global.h
libntlm_la_LIBADD = lib/libgnu.la

# test

TESTS = test_ntlm
check_PROGRAMS = $(TESTS)
LDADD = libntlm.la
CLEANFILES = test.out
test_ntlm_LDFLAGS = -no-install

# Maintainer targets

ChangeLog:
	cvs2cl --FSF --fsf --usermap .cvsusers -I ChangeLog -I .cvs \
		-I autogen.sh && \
	cat .cvscopying >> ChangeLog

indent:
	indent $(SOURCES)

htmldir = ../www-$(PACKAGE)
tag = $(PACKAGE)-`echo $(VERSION) | sed 's/\./-/g'`

release:
	cvs upd -p -r HEAD NEWS > /dev/null
	if cvs upd -p -r $(tag) NEWS > /dev/null; then false else true; fi
	rm -f ChangeLog && cvs upd ChangeLog
	cvs commit
	rm -f ChangeLog
	$(MAKE) ChangeLog distcheck
	cvs commit -m "" ChangeLog
	cvs rtag $(tag) $(PACKAGE)
	gpg -b $(distdir).tar.gz
	gpg --verify $(distdir).tar.gz.sig
	cp -v $(distdir).tar.gz{,.sig} $(htmldir)/releases/
	cd $(htmldir) && cvs add -kb releases/$(distdir).tar.gz{,.sig} && \
		cvs commit -m "Update." releases/
