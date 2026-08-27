# How to build G-CVSNT

G-CVSNT is the Gaijin fork of CVSNT 2.5.05, versioned as **CVSNT 3.5.x**. The
actual source tree lives in [`cvsnt/cvsnt-2.5.05.3744/`](cvsnt/cvsnt-2.5.05.3744)
(the directory name kept the original upstream version). It has two build
systems:

| Platform | Build system | Entry point |
|----------|--------------|-------------|
| Windows (client + server) | Visual C++ solution (`cvsnt.sln`, toolset v142) | `cvsnt/build-windows.py` or Visual Studio |
| Linux (server, mostly) | autotools | `cvsnt/build-linux-server` |
| macOS (client) | autotools + osx packaging script | `cvsnt/build-macosx` |

All third-party dependencies needed on Windows are vendored in the tree
(`external_libs/` contains OpenSSL and iconv headers + prebuilt `.lib`s, the
sqlite3 amalgamation; `zlib/`, `pcre/`, `libxml/`, `zstd/`, `blake3/` are full
vendored sources built as projects of the solution).

---

## Windows

### Option A: `build-windows.py` (no Visual Studio installation required) — verified

[`cvsnt/build-windows.py`](cvsnt/build-windows.py) is a self-contained driver
that parses the `.vcxproj` files and invokes `cl.exe` / `ml64.exe` / `rc.exe` /
`mc.exe` / `midl.exe` / `lib.exe` / `link.exe` directly, so **no MSBuild and no
VS installation is needed** — a bare MSVC toolchain package plus a Windows 10
SDK is enough (e.g. the Gaijin devtools packages).

Requirements:

* Python 3
* MSVC toolchain directory (contains `bin\Hostx64\x64\cl.exe`, `include`, `lib`,
  `atlmfc`) — e.g. `D:\devtools\vc2019_16.11.34` (VS2019/v142, matches the
  solution) or `vc2022_*`
* Windows 10 SDK directory (contains `include\<ver>`, `lib\<ver>`, and
  `bin\<ver>\x64\rc.exe`) — e.g. `D:\devtools\win.sdk.100`

```bash
cd cvsnt
python build-windows.py --vc D:\devtools\vc2019_16.11.34 --sdk D:\devtools\win.sdk.100
```

Or, if you have real Visual Studio, from a *x64 Native Tools Command Prompt*:

```bash
cd cvsnt
python build-windows.py
```

Useful switches: `--config Release|Debug`, `--platform x64|Win32`,
`--projects name1,name2` (partial rebuild), `--with-optional` (also builds
`co`/`rcsdiff`/`rlog`, `cvsdiag`, `extnt`, `su`, `genkey`, installer helpers…),
`--jobs N`.

Output lands in `cvsnt/cvsnt-2.5.05.3744/Releasex64/`:

```
cvs.exe                  the client/server binary
cvsapi.dll  cvstools.dll core libraries
cvsservice.dll           NT service
lockservice.exe          cvslock daemon
plink.dll                bundled PuTTY link (used by :ssh:)
protocols\*.dll          pserver, sserver, sspi, ssh, ext, enum, fork, server
triggers\*.dll           info, audit, email, script, checkout plugins
database\sqlite_database.dll, odbc_database.dll
mdns\mdns_mini.dll, mdnsclient.dll
xdiff\xml_xdiff.dll
```

Notes on generated sources (the script runs these automatically; Visual Studio
runs them as custom build steps):

* `cvsapi/win32/ServiceMsg.mc` → `ServiceMsg.h`/`.rc` via `mc.exe`
  (needed by `cvsapi` and `cvsservice`)
* `cvstools/win32/trigger.idl` → `trigger_h.h`, `trigger_i.c`,
  `win32/trigger.tlb` via `midl.exe`
* `triggers/server.idl` → `Server_h.h`, `Server_i.c`, `script_trigger.tlb`

### Option B: Visual Studio

Open `cvsnt/cvsnt-2.5.05.3744/cvsnt.sln` with Visual Studio 2019 or newer
(the projects use PlatformToolset **v142**; install the "MSVC v142" component
or retarget). Build configuration `Release|x64`. The `Desktop development with
C++` workload plus **ATL** (for `script_trigger` and the control panel) and the
Windows 10 SDK are required.

### Running the freshly built cvs.exe

`cvs.exe` locates its protocol/trigger DLLs through the CVSNT install
directory (registry `HKLM\SOFTWARE\CVS\Pserver` / the directory of an installed
CVSNT). If you run the fresh `cvs.exe` in place next to its `protocols\`
subdirectory it works standalone for `--version` and local operations; to use
it as your day-to-day client, either replace the files of an existing CVSNT
installation (e.g. `C:\Program Files (x86)\CVSNT\`) or build & install the MSI.

### MSI installer

WiX sources and tools are vendored:

```bash
cd cvsnt
make_msix64.bat     # candle.exe + light.exe on cvsnt-x64-3.5.05.<build>.wxs
```

`make_msi.bat` is the Win32 variant. The `.wxs` files reference the binaries
produced by the Release build; bump the version/build number in the `.wxs`
file name and content when releasing (see *Versioning* below).

---

## Linux (server)

Scripted in [`cvsnt/build-linux-server`](cvsnt/build-linux-server). On
RHEL-family distros install the packages from
[`cvsnt/install-yum-packages`](cvsnt/install-yum-packages) first
(devtoolset-9 gcc/g++, dh-autoreconf, libxml2-devel, pcre-devel,
libtool-ltdl-devel, glibc-static, libstdc++-static, libzstd-devel, zstd,
pam-devel):

```bash
cd cvsnt/cvsnt-2.5.05.3744
autoreconf -i
bash configure \
    --enable-pam --enable-server --enable-pserver --enable-sspi --enable-ext \
    --disable-mysql --disable-postgres --disable-sqlite --enable-64bit
( cd zstd && make && sudo make install )   # vendored zstd first
make
sudo make install
cd tools && ./build_tools                  # blob utilities, see below
```

`build-rhel6` and `build-altlinux` are older distro-specific variants of the
same sequence (`build-altlinux` applies `cvsnt-2.5.05-alt-system-pcre.patch`).

`tools/build_tools` builds the Gaijin blob-server maintenance utilities with
clang++ (`cvtblob` — convert an RCS repository to content-addressed blobs,
`gc-blobs`, `repack-blobs`, `blake3-calc`, `sha256_calc`, `simplelock`,
`unlock`). They link against the already-installed `libblake3`/`libca_blobs_fs`
that `make install` produced.

---

## macOS (client)

Per the top-level [README.md](README.md) — Homebrew is required:

```bash
cd cvsnt
./build-macosx
```

The script installs autoconf/automake/pkg-config/libtool/pcre/openssl@3 via
brew, symlinks openssl@3 into `/usr/local`, runs `autoreconf`, builds the
vendored zstd, then delegates to `osx/build-mac` which produces
`cvsnt-3.5.*.tar.gz`. Unpack the archive and run `./install_copy_cvsnt.sh` to
copy into `/usr/local`. Edit `BUILD_ARCH`/`PKG_BUILD_SUFFIX` at the top of the
script to select x86_64 vs arm64. After the build, discard the tree changes the
build made: `git checkout .`

---

## Versioning

The product version lives in `cvsnt-2.5.05.3744/version_no.h`
(`CVSNT_PRODUCT_MAJOR/MINOR/PATCH` and the build number). On Windows,
`genbuild.exe` (built from `genbuild/`) regenerates the build number/date; the
MSI `.wxs` files carry the version in their file names and contents and must be
kept in sync manually.

## Troubleshooting

* **`Cannot open include file: 'ServiceMsg.h'` / `'trigger_h.h'` /
  `'Server_h.h'`** — the `.mc`/`.idl` code generation step has not run; use
  `build-windows.py` (it runs `mc.exe`/`midl.exe` on demand) or build the full
  solution once in VS.
* **`file not found: win32\trigger.tlb`** in `cvstools.rc2` — same cause, the
  MIDL type library was not generated.
* **`atlbase.h` not found** — ATL is missing (install VS ATL component, or use
  a toolchain package that ships `atlmfc/`).
* **`the :pserver: access method is not available`** — the protocol DLLs were
  not found at run time (see *Running the freshly built cvs.exe* above).
* **Unresolved OpenSSL/iconv symbols** — link inputs come prebuilt from
  `external_libs/x64` (`libssl.lib`, `libcrypto.lib`, `libiconv.lib`); make
  sure that directory is intact.
