# Source tree layout

```
D:\G-CVSNT
├── README.md, HOWTOBUILD.md, docs/          this documentation
├── _reports/                                code-audit findings (generated)
└── cvsnt/
    ├── build-windows.py                     Windows build driver (no VS needed)
    ├── build-linux-server, build-macosx,    platform build scripts
    │   build-rhel6, build-altlinux
    ├── make_msi.bat, make_msix64.bat        WiX MSI packaging (msi_tools/, *.wxs)
    ├── cvslockd, cvsnt.xinetd, cvsnt.pam    Unix service integration files
    ├── *.patch                              historical distro patches
    ├── tortoiseCVS/                         TortoiseCVS fork (Windows shell ext)
    └── cvsnt-2.5.05.3744/                   ← the actual source tree (CVSNT 3.5.x)
        ├── cvsnt.sln, *.vcxproj             VS2019 (v142) solution
        ├── configure.in, Makefile.am        autotools build (Unix)
        ├── version_no.h                     product version / build number
        │
        │  ── the cvs program ──
        ├── src/                             all cvs commands, client & server
        │   ├── <command>.cpp                add, admin, annotate, checkout, commit,
        │   │                                diff, edit, import, log, ls, remove,
        │   │                                status, tag, update, watch, ...
        │   ├── client.cpp / server.cpp      wire protocol, two halves
        │   ├── rcs.cpp, rcs_checkin.cpp     RCS ,v read/write
        │   ├── recurse.cpp, RecurseRepository.cpp   tree walking
        │   ├── entries.cpp, vers_ts.cpp, classify.cpp  working-copy state
        │   ├── lock.cpp                     repo locks (files or cvslock daemon)
        │   ├── blob_*.cpp, download_blob_to.cpp, sha_blob_reference.h,
        │   │   zstd_buffer.cpp, sha256/     Gaijin blob machinery
        │   └── httplib.h                    bundled cpp-httplib (HTTP blob pull)
        │
        │  ── Gaijin blob infrastructure ──
        ├── ca_blobs_fs/                     content-addressed store library
        ├── keyValueServer/                  blob TCP server/client/proxy
        │   ├── blob_sockets/, clientLib/, serverLib/
        │   ├── proxy/                       cafs_proxy (write-through cache)
        │   └── sample*/                     reference implementations
        ├── blake3/                          vendored BLAKE3 (C + asm)
        ├── zstd/                            vendored zstd
        │
        │  ── support libraries ──
        ├── cvsapi/                          platform/network/db/unicode API (DLL)
        │   └── db/sqlite, db/odbc, mdns/    plugins
        ├── cvstools/                        settings + trigger glue (DLL)
        ├── protocols/                       pserver, sserver, sspi, ssh, ext,
        │                                    gserver, fork, enum plugins
        ├── triggers/                        info (CVSROOT scripts), audit, email,
        │                                    script, checkout plugins
        ├── lib/ (gnulib), diff/ (libdiff), xdiff/, cvsdelta/, cvsgui/,
        │   pcre/, zlib/, libxml/, ufc-crypt/, mdnsclient/   misc libs
        ├── external_libs/                   prebuilt OpenSSL/iconv (Win32/x64),
        │                                    sqlite3 amalgamation, dns_sd.h
        │
        │  ── services & tools ──
        ├── cvsservice/                      Windows NT service (port 2401/2402)
        ├── lockservice/                     cvslock lock daemon
        ├── windows-NT/                      Win32 compat layer, cvsdiag, setuid,
        │                                    installer helper
        ├── tools/                           blob repo maintenance: cvtblob,
        │                                    gc-blobs, repack-blobs, blake3-calc,
        │                                    simplelock/unlock (build_tools script)
        ├── rcs/ (co, rcsdiff, rlog), su/, extnt/, genkey/, genbuild/,
        │   postinst/, uninsthlp/, control-panel/, cvsntcpl/   utilities
        ├── contrib/, contrib_nt/            scripts, examples
        ├── doc/, man/                       original texinfo/man documentation
        ├── osx/, redhat/, debian/           platform packaging
        └── testcvs/, simcvs/                test harnesses
```

Conventions worth knowing:

* `windows-NT/config.h` vs generated `config.h` — the Windows build uses the
  checked-in config; Unix generates one via `configure`.
* Generated sources not in git: `cvsapi/win32/ServiceMsg.{h,rc}` (from
  `ServiceMsg.mc`), `cvstools/trigger_h.h|trigger_i.c|win32/trigger.tlb`
  (from `win32/trigger.idl`), `triggers/Server_h.h|Server_i.c|
  script_trigger.tlb` (from `server.idl`). `build-windows.py` creates them
  on demand.
* Build outputs land in `Releasex64/` (or `Debug<platform>/`), intermediates
  in `tmp/`.
