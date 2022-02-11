## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL) -std=c++17

if BUILD_ARM64
else
AM_CPPFLAGS += -msse4.1
endif

libblob_sockets_lib_la_SOURCES = \
  blob_sockets.cpp

libblob_sockets_lib_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 
libblob_sockets_lib_la_LIBADD = -lcrypto

lib_LTLIBRARIES = libblob_sockets_lib.la

