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

* prebuilt package archives are available in Releases (for x64 and arm64 arch):
  https://github.com/GaijinEntertainment/G-CVSNT/releases
