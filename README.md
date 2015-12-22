G-CVSNT
=======

G-CVSNT Gaijin (and Gamedev) CVSNT version - modified for large amounts of binary data (typically for gamedev)


OSX 10.9+ notes:

* pcre headers are required
  (unpack cvsnt/cvsnt-2.5.05.3744/osx/usr_local_include_pcre.tar.gz to /usr/local/include)

* build cvsnt client package with:
  ./build-mac
  in  cvsnt/cvsnt-2.5.05.3744/osx  directory

* resulting archive (cvsnt/cvsnt-2.5.05.3744/osx/cvsnt-2.XX.tar.gz) is prebuilt and added to repository
  (cvsnt/cvsnt-2.5.05.3744/osx/cvsnt-2.5.05.4452.tar.gz built with Xcode7 on OSX 10.11)
  it can be directly used for installing without code rebuild

* in order to "install" cvsnt client unpack
  cvsnt/cvsnt-2.5.05.3744/osx/cvsnt-2.XX.tar.gz
  and run
  ./install_copy_cvsnt.sh
  (this will copy relevant files to /usr/local/bin, /usr/local/lib, etc.)
