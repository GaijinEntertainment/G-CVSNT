## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL) -msse4.1 -std=c++17

libca_blobs_fs_la_SOURCES = \
  src/fileio.cpp \
  src/calc_hash.cpp \
  src/streaming_compressors.cpp \
  src/content_addressed_fs.cpp 

libca_blobs_fs_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 

libca_blobs_fs_la_LIBADD = -lz -lzstd ../blake3/libblake3.la

lib_LTLIBRARIES = libca_blobs_fs.la