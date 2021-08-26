G-CVSNT
=======

G-CVSNT Gaijin (and Gamedev) CVSNT version - modified for large amounts of binary data (typically for gamedev)


OSX 10.9+ notes:

* HomeBrew is required (build-macosx will give instruction how to install)

* build cvsnt client package with:
  ./build-macosx
  in  cvsnt/  directory

* resulting archive is written to ./cvsnt-3.5.*.tar.gz

* in order to "install" cvsnt client unpack archive
    cvsnt-3.5.*.tar.gz
  and run
    ./install_copy_cvsnt.sh
  (this will copy relevant files to /usr/local/bin, /usr/local/lib, etc.)

* prebuilt package archives are placed to root:
    cvsnt-2.5.05.4452-osx10.13-x64.tar.gz  - built for x86_64, requires OSX 10.13+
    cvsnt-3.5.16.7699-osx10.11-x64.tar.gz  - built for x86_64, requires OSX 10.11+
