AM_CPPFLAGS = @PTHREAD_CPPFLAGS@ -std=c++17 -mavx2 -I../src -I../ca_blobs_fs -fopenmp

gc_blobs_SOURCES = gc-blobs.cpp ../src/blob_operations.cpp
repack_blobs_SOURCES = repack-blobs.cpp
convert_to_blob_SOURCES = convert_to_blob.cpp
blake3_calc_SOURCES = blake3-calc.cpp

bin_PROGRAMS=gc_blobs repack_blobs convert_to_blob blake3_calc

convert_to_blob_LDADD = -lstdc++fs -lblake3 -lca_blobs_fs -lz -lzstd @PTHREAD_LIBS@ 
gc_blobs_LDADD = -lstdc++fs -lblake3 -lca_blobs_fs -lz -lzstd @PTHREAD_LIBS@ 
repack_blobs_LDADD = -lstdc++fs -lblake3 -lca_blobs_fs -lz -lzstd @PTHREAD_LIBS@ 
blake3_calc_LDADD = -lblake3

