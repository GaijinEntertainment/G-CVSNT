## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(includeopt) $(INCLTDL)

libblake3_la_SOURCES = \
  blake3.c \
  blake3_portable.c \
  blake3_dispatch.c

if BUILD_ARM64
AM_CFLAGS = -DBLAKE3_USE_NEON
libblake3_la_SOURCES += blake3_neon.c
else
AM_CFLAGS = -msse4.1
libblake3_la_SOURCES += blake3_avx2_x86-64_unix.S blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S
if BUILD_AVX512
AM_CFLAGS += -mavx -mavx2 -mavx512f
libblake3_la_SOURCES += blake3_avx512_x86-64_unix.S
else
AM_CFLAGS += -DBLAKE3_NO_AVX512=1
endif
endif

libblake3_la_LDFLAGS = -release $(VERSION) -L$(DESTDIR)/$(libdir) 

##libblake3_la_LIBADD = $(LIBLTDL)

lib_LTLIBRARIES = libblake3.la
