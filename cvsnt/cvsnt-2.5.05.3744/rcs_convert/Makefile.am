zlib_inc =
zlib_lib = @ZLIB_LIBS@
	
if WITH_PAM
  pam_lib = @PAM_LIBS@
else
  pam_lib =
endif

if WITH_MAC_HFS_SUPPORT
  mac_hfs_inc = -I$(top_srcdir)/osx
  mac_hfs_lib = ../osx/libmachfs.a
else
  mac_hfs_inc =
  mac_hfs_lib =
endif

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/cvsapi -I$(top_srcdir)/cvstools -I$(top_srcdir)/diff -I$(top_srcdir)/cvsapi $(zlib_inc) $(includeopt) $(mac_hfs_inc) -pthread -msse4.1
AM_LDFLAGS = -pthread

bin_PROGRAMS = rcs_bin_convert

# The cvs executable
rcs_bin_convert_SOURCES = \
    rcs_convert.cpp \
    stub.cpp \
    ../src/hash.cpp \
    ../src/lock.cpp \
    ../src/subr.cpp \
    ../src/xgetwd.cpp \
    ../src/filesubr.cpp

rcs_bin_convert_LDADD = \
	-L$(DESTDIR)/$(libdir) \
	../lib/libcvs.a \
	../cvsdelta/libcvsdelta.a \
	../cvsapi/libcvsapi.la \
	../cvstools/libcvstools.la \
    ../blake3/libblake3.la \
    ../ca_blobs_fs/libca_blobs_fs.la \
	$(zlib_lib) \
	$(mac_hfs_lib)

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/cvs

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/cvs
	$(LN_S) cvsnt $(DESTDIR)$(bindir)/cvs

