## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL) -msse4.1

libblake3_la_SOURCES = \
  blake3_avx2_x86-64_unix.S \
  blake3_avx512_x86-64_unix.S \
  blake3_sse2_x86-64_unix.S \
  blake3_sse41_x86-64_unix.S \
  blake3.c \
  blake3_portable.c \
  blake3_dispatch.c

libblake3_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 

##libblake3_la_LIBADD = $(LIBLTDL)

lib_LTLIBRARIES = libblake3.la
