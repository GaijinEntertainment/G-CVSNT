AM_CPPFLAGS = @PTHREAD_CPPFLAGS@

cafs_server_SOURCES = \
	cafs_server.cpp \
    blob_file_lib.cpp \
    ../serverLib/blob_push_proc.cpp \
    ../serverLib/blob_push_server.cpp

bin_PROGRAMS=cafs_server

cafs_server_LDADD = -lz -lzstd  ../../ca_blobs_fs/libca_blobs_fs.la @PTHREAD_LIBS@ 

