G-CVSNT
=======

G-CVSNT Gaijin (and Gamedev) CVSNT version - modified for large amounts of binary data (typically for gamedev)


OSX 10.9+ notes:

* HomeBrew is required (build-macosx will give instruction how to install)

* build cvsnt client package with:
  ./build-macosx
  in  cvsnt  directory

* resulting archive (cvsnt/cvsnt-2.5.05.3744/osx/cvsnt-3.XX.tar.gz) is copied after build
  to cvsnt/cvsnt-3.5.16.7699.tar.gz

* in order to "install" cvsnt client unpack
  cvsnt/cvsnt-3.5.16.7699.tar.gz
  and run
  ./install_copy_cvsnt.sh
  (this will copy relevant files to /usr/local/bin, /usr/local/lib, etc.)
