## Process this file with automake to produce Makefile.in
# Master Makefile for the GNU Concurrent Versions System.
# Copyright (C) 1986, 1988-1992, 1994, 2000 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

AUTOMAKE_OPTIONS = 1.10
ACLOCAL_AMFLAGS = -I m4

if WITH_RCSTOOLS
  RCSTOOLS_SUBDIR = rcs
else
  RCSTOOLS_SUBDIR =
endif

if WITH_MAC_HFS_SUPPORT
  MACOSX_SUBDIR = osx
else
  MACOSX_SUBDIR =
endif

if WITH_MINI_MDNS
  MDNSCLIENT_SUBDIR = mdnsclient
else
  MDNSCLIENT_SUBDIR =
endif
  
cvs_config_dir=@cvs_config_dir@

# All other subdirs:
SUBDIRS = lib $(MDNSCLIENT_SUBDIR) cvsapi diff cvsgui cvstools xdiff \
	  protocols triggers cvsdelta $(MACOSX_SUBDIR) src lockservice $(RCSTOOLS_SUBDIR) man run_script

.PHONY: remotecheck
remotecheck: all
	cd src && $(MAKE) $(AM_MAKEFLAGS) "$@"

## MAINTAINER Targets

## cvsnt.spec  ##
# - cvs.spec needs to be updated only once, since it depends on
#   configure.in, not on the results of a 'configure' run.
# - It is guaranteed (with GNU Make) that when the version in configure.in
#   is changed, acversion.m4 is built only after the new version number is
#   propagated to the Makefile.  (Libtool uses the same guarantee.)
spec: $(srcdir)/redhat/cvsnt.spec.in $(top_srcdir)/configure.in
	sed 's,@VERSION\@,$(VERSION),g' $(srcdir)/redhat/cvsnt.spec.in >$(srcdir)/redhat/cvsnt.spec

# for backwards compatibility with the old makefiles
.PHONY: realclean
realclean: maintainer-clean

install-exec-local:
if INSTALL_SERVER
	$(mkinstalldirs) $(DESTDIR)$(cvs_config_dir)
	$(INSTALL_DATA) $(top_srcdir)/doc/PServer.example $(DESTDIR)$(cvs_config_dir)
	$(INSTALL_DATA) $(top_srcdir)/doc/Plugins.example $(DESTDIR)$(cvs_config_dir)
	if [ -f $(DESTDIR)$(cvs_config_dir)/ca.pem ]; then \
	  $(INSTALL_DATA) $(top_srcdir)/ca.pem $(DESTDIR)$(cvs_config_dir)/ca.pem.example; \
	else \
	  $(INSTALL_DATA) $(top_srcdir)/ca.pem $(DESTDIR)$(cvs_config_dir); \
	fi
endif

testkey:
	sh $(DESTDIR)$(top_srcdir)/contrib/genkey

