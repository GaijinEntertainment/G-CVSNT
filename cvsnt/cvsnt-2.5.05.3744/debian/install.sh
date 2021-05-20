#!/bin/bash
. debian/vars
set -e
set -x
rm -rf $BUILD_ROOT
install -d $BUILD_ROOT{/etc/{pam.d,xinetd.d,default,systemd/system},/var/lib/cvs}
/usr/bin/make install -j1 \
  DESTDIR=$BUILD_ROOT
cd tools
/usr/bin/make install -j1 \
  DESTDIR=$BUILD_ROOT
cd ..
install -m 755 -d $BUILD_ROOT/usr/sbin
install -m 755 tools/unlock $BUILD_ROOT/usr/sbin/cvsnt-unlock
install -m 755 tools/simplelock $BUILD_ROOT/usr/sbin/cvsnt-simplelock
grep -v 'LD_LIBRARY_PATH' redhat/cvsnt.xinetd > $BUILD_ROOT/etc/xinetd.d/cvsnt
install redhat/cvsnt-cvslockd.service $BUILD_ROOT/etc/systemd/system/cvsnt-cvslockd.service
install redhat/cvsnt.pam $BUILD_ROOT/etc/pam.d/cvsnt
install redhat/cvsnt-cafs $BUILD_ROOT/etc/default/cvsnt-cafs
sed -i 's/etc\/sysconfig\/cvsnt-cafs/etc\/default\/cvsnt-cafs/' redhat/cvsnt-cafs.service
install redhat/cvsnt-cafs.service $BUILD_ROOT/etc/systemd/system/cvsnt-cafs.service
install redhat/cvsnt-proxy $BUILD_ROOT/etc/default/cvsnt-proxy
sed -i 's/etc\/sysconfig\/cvsnt-proxy/etc\/default\/cvsnt-proxy/' redhat/cvsnt-proxy.service
install redhat/cvsnt-proxy.service $BUILD_ROOT/etc/systemd/system/cvsnt-proxy.service
mv $BUILD_ROOT/etc/cvsnt/PServer{.example,}
mv $BUILD_ROOT/etc/cvsnt/Plugins{.example,}
