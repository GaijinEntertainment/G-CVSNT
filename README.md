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

Documentation
-------------

* [docs/](docs/) — architecture, blob storage, protocols, repository layout, server operations,
  client usage and a source map
* [HOWTOBUILD.md](HOWTOBUILD.md) — building on Windows, Linux and macOS
* [known_issues.md](known_issues.md) — open defects, with the reasoning behind the ones that are
  deliberately not fixed
* [suggested_optimizations.md](suggested_optimizations.md) — why update and tag scale with file
  count, and the ranked plan to fix it
* [cvsnt/cvsnt-2.5.05.3744/testcvs/](cvsnt/cvsnt-2.5.05.3744/testcvs/) — unit, regression and
  acceptance test suites
* [_reports/](_reports/) — the individual analysis findings behind all of the above
