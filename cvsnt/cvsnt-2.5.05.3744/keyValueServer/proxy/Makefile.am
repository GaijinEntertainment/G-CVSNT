AM_CPPFLAGS = -std=c++17
cafs_proxy_server_SOURCES = \
	free_disk_space.cpp \
    cafs_proxy_server.cpp \
    proxy_file_lib.cpp \
    gc_proc_monitor.cpp \
    ../../ca_blobs_fs/src/fileio.cpp \
    ../serverLib/blob_push_proc.cpp \
    ../serverLib/blob_push_server.cpp

bin_PROGRAMS=cafs_proxy_server

cafs_proxy_server_LDADD = ../clientLib/libkv_client_lib.la

if ALLOW_LIBSTDC_FS
cafs_proxy_server_LDADD += -lstdc++fs
endif
