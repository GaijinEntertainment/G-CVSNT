## Process this file with automake to produce Makefile.in
INCLUDES = -I$(top_srcdir)/src -I$(top_srcdir)/cvsapi -I$(top_srcdir)/cvstools -I$(top_srcdir)/lib

noinst_LIBRARIES = libmachfs.a
libmachfs_a_SOURCES = \
	hqx.c \
	apsingle.c \
	cvs_hqx.cpp \
	mac_copy_file.cpp \
	mac_hfs_stuff.cpp

libmachfs_a_LIBADD = 

EXTRA_DIST = \
	README.txt \
	hqx.h \
	apsingle.h \
	cvs_hqx.h \
	mac_copy_file.h \
	mac_hfs_stuff.h

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
