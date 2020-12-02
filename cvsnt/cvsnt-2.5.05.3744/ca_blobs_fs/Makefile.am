## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL) -msse4.1

libca_blobs_fa_la_SOURCES = \
  src/fileio.cpp \
  src/streaming_compressors.cpp \
  src/content_addressed_fs.cpp 

libca_blobs_fa_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 

libca_blobs_fa_la_LIBADD = $(zlib_lib) 

lib_LTLIBRARIES = libca_blobs_fa.la
