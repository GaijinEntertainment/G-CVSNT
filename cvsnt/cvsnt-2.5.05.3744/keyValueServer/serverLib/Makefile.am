## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL) -msse4.1 -std=c++17

libkv_server_lib_la_SOURCES = \
  blob_push_proc.cpp \
  blob_push_server.cpp

libkv_server_lib_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 
#kv_server_la_LIBADD = ../../ca_blobs_fs/libca_blobs_fs.la

lib_LTLIBRARIES = libkv_server_lib.la

