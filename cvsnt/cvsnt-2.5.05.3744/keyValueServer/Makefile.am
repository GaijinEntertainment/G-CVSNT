AM_CPPFLAGS = @PTHREAD_CPPFLAGS@ -DCAFS

kv_server_SOURCES = \
	sample/sample_server.cpp \
    sampleImplementation/blob_file_lib.cpp

bin_PROGRAMS=kv_server

kv_server_LDADD = -lz -lzstd  ../ca_blobs_fs/libca_blobs_fs.la ./serverLib/libkv_server_lib.la	@PTHREAD_LIBS@ 
