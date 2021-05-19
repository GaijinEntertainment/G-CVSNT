#!/bin/bash
. debian/vars
set -e
set -x
export LD_LIBRARY_PATH="$(pwd)/ca_blobs_fs/.libs:$(pwd)/blake3/.libs:$LD_LIBRARY_PATH"
autoreconf -if
./configure --prefix=/usr \
	    --enable-pam \
	    --enable-server \
	    --enable-pserver \
	    --enable-ext \
	    --disable-mdns \
	    --disable-mysql \
	    --disable-postgres \
	    --disable-sqlite \
	    --enable-64bit
/usr/bin/make -j$(nproc)
cd tools
/usr/bin/make -j$(nproc)
g++ -std=c++11 simplelock.cpp -o simplelock
g++ -std=c++11 unlock.cpp -o unlock
cd ..
