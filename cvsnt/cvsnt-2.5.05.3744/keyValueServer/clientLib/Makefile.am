## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL) -msse4.1

libkv_client_lib_la_SOURCES = \
  blob_push_pull_client.cpp \
  blob_strm_client_cmd.cpp \
  blob_chck_client_cmd.cpp \
  blob_pull_client_cmd.cpp \
  blob_push_client_cmd.cpp 

libkv_client_lib_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 
libkv_client_lib_la_LIBADD = ../../ca_blobs_fs/libca_blobs_fs.la

##libblake3_la_LIBADD = $(LIBLTDL)

lib_LTLIBRARIES = libkv_client_lib.la
