# How to build G-CVSNT

All source lives under `cvsnt/cvsnt-2.5.05.3744/`. Every path below is relative to the repository
root unless stated otherwise.

Two build systems coexist:

* **Autotools** (`configure.in` + per-directory `Makefile.am`) — Linux and macOS.
* **Visual Studio** (`cvsnt.sln` + `*.vcxproj`) — Windows.

They are independent. Changing the file list of a component means editing *both*.

---

## Linux

### Dependencies

Debian/Ubuntu:

```bash
sudo apt-get install build-essential autoconf automake libtool pkg-config \
                     libxml2-dev libpcre3-dev libzstd-dev zlib1g-dev \
                     libssl-dev libpam0g-dev
```

RHEL/CentOS — the list the project itself ships (`cvsnt/install-yum-packages`):

```bash
yum install devtoolset-9-gcc devtoolset-9-gcc-c++
scl enable devtoolset-9 -- bash
yum install dh-autoreconf libxml2-devel pcre-devel libtool-ltdl-devel \
            glibc-static libstdc++-static libzstd-devel zstd pam-devel
```

Hard requirements enforced by `configure.in`: **libxml2 ≥ 2.7** (`configure.in:369`),
**zlib > 1.2.0** (`configure.in:521`), and **libpcre** via `pkg-config`
(`configure.in:376`).

### Build a server

The project's own driver is `cvsnt/build-linux-server`:

```bash
cd cvsnt/cvsnt-2.5.05.3744
autoreconf -i
bash configure \
    --enable-pam \
    --enable-server \
    --enable-pserver \
    --enable-sspi \
    --enable-ext \
    --disable-mysql \
    --disable-postgres \
    --disable-sqlite \
    --enable-64bit

cd zstd && make && sudo make install && cd ..
make
sudo make install
```

Note the ordering: **zstd is built and installed before the main `make`**. The main build links
against the installed `libzstd`, not against the in-tree copy.

### The maintenance tools

On Linux the main `make` **already** builds these four, as `convert_to_blob`, `gc_blobs`,
`repack_blobs` and `blake3_calc`: `tools/` is in the top-level `SUBDIRS` whenever the target is not
macOS (`Makefile.am:42`, gated by `WITH_SERVER_AND_TOOLS` from `configure.in:767`), and
`tools/Makefile.am:12` builds them with the configured compiler at `-std=c++17 -mavx2`.

`tools/build_tools` is an *alternative* clang path that produces the hyphenated names
`cvtblob`, `gc-blobs`, `repack-blobs`, `blake3-calc`:

```bash
cd cvsnt/cvsnt-2.5.05.3744/tools
./build_tools
```

It expects `libblake3` and `libca_blobs_fs` to be installed already (both are `lib_LTLIBRARIES`
installed by the main `make install`), compiles with `clang++ -O3 -msse4.1`, and adds
`-std=c++17 -lz -lzstd -pthread` for the three blob tools. `blake3-calc` needs only `-lblake3`.

### Useful `configure` options

| Option | Effect |
| --- | --- |
| `--enable-server` / `--enable-pserver` | Build the server halves |
| `--enable-pam` | PAM authentication |
| `--enable-sserver` / `--disable-sserver` | The `:sserver:` TLS protocol (on by default when OpenSSL is found, `configure.in:919`) |
| `--with-ssl=DIR` | Where to look for OpenSSL if it is not in the default paths (`configure.in:146`) |
| `--enable-gserver` / `--enable-sspi` | GSSAPI / SSPI methods |
| `--enable-64bit` | 64-bit build |
| `--disable-avx512` | Build BLAKE3 **without** AVX-512 kernels. AVX-512 is **on by default** on non-macOS targets (`configure.in:735`). AMD CPUs older than Zen 4 do not have it and will die with an illegal instruction, so use this flag if any target machine might be one (commit `2cd984a`) |
| `--with-config_dir=DIR` | Where global settings live; defaults to `${sysconfdir}/cvsnt` |
| `--with-protocol_dir=DIR` | Where protocol plugins are searched for |
| `--disable-mysql`, `--disable-postgres`, `--disable-sqlite`, `--disable-odbc` | Skip database back-ends you do not need — this removes most of the optional dependencies |
| `--disable-hfs` | Skip HFS+ support (macOS only) |

Run `bash configure --help` for the complete list.

### Packaging

`debian/` and `redhat/` contain packaging metadata. `cvsnt/build-rhel6` is a minimal
`configure && make && make install` driver, and `cvsnt/build-altlinux` adds the zstd and tools steps.

---

## macOS

Homebrew is required. `cvsnt/build-macosx` does everything:

```bash
cd cvsnt
./build-macosx
```

What it does:

1. `brew install autoconf automake pkg-config libtool pcre openssl@3`.
2. Symlinks the Homebrew OpenSSL 3 headers and libraries into `/usr/local/include` and
   `/usr/local/lib`. It handles both the Intel (`/usr/local/opt`) and Apple Silicon
   (`/opt/homebrew/opt`) prefixes.
3. `autoreconf -i --force`.
4. Builds and installs the in-tree `zstd`.
5. Runs `osx/build-mac`, which configures with clang for the chosen architecture and produces
   a `.tar.gz` package in the repository root.

The target architecture and deployment target come from `BUILD_ARCH` at the top of
`cvsnt/build-macosx`. Uncomment the block you want:

| `BUILD_ARCH` | Produces |
| --- | --- |
| `-arch x86_64 -msse4.1 -mmacosx-version-min=10.11` | Intel, macOS 10.11+ (default) |
| `-arch x86_64 -msse4.1 -mavx -mavx2 -mavx512f -mmacosx-version-min=10.15` | Intel with AVX-512, macOS 10.15+ |
| `-arch arm64 -mmacosx-version-min=11.0` | Apple Silicon, macOS 11+ |
| `-arch arm64 -mmacosx-version-min=13.0` | Apple Silicon, macOS 13+ |

The macOS build is a **client-only** build: `osx/build-mac:44` passes `--disable-server`, along with
`--disable-hfs --with-internal-zlib --disable-ltdl --disable-odbc --disable-postgres
--disable-mysql --disable-sspi --disable-sqlite --enable-64bit`.

Two flags in that list are historical no-ops: `configure` recognises neither `--with-internal-zlib`
nor `--disable-ltdl`, and discards both with an "unrecognized options" warning. ltdl is actually
switched off on macOS by target detection (`configure.in:764`), not by a flag.

Install the resulting archive:

```bash
tar xzf cvsnt-3.5.*.tar.gz
./install_copy_cvsnt.sh
```

That copies binaries and libraries into `/usr/local/bin`, `/usr/local/lib`, etc.

`build-macosx` modifies the working tree. When it is done, run `git checkout .` to discard the
generated files, as the script itself reminds you.

Prebuilt macOS packages for x64 and arm64 are published under
[Releases](https://github.com/GaijinEntertainment/G-CVSNT/releases).

---

## Windows

### Prerequisites

* Visual Studio 2019 or later with the **Desktop development with C++** workload.
  The projects specify `PlatformToolset` `v142` (VS 2019) and
  `WindowsTargetPlatformVersion` `10.0`. Newer toolsets work if you retarget the solution.
* No external dependency downloads are needed: prebuilt import libraries and headers for OpenSSL,
  iconv and Bonjour ship in `cvsnt/cvsnt-2.5.05.3744/external_libs/` (`x64/` and `Win32/`).
  Only the OpenSSL redistributables are included, under `dll/`; `libiconv.lib` links statically and
  `dnssd.lib` resolves against the Bonjour service installed on the machine.

### Build

```
cvsnt\cvsnt-2.5.05.3744\cvsnt.sln
```

Open it, pick `Release|x64` (or `Release|Win32`), and build the solution. All 58 projects in the
solution are marked to build; `cvsnt` produces `cvs.exe`.

The main client/server project (`cvsnt.vcxproj`) is configured with:

* Preprocessor: `ISOLATION_AWARE_ENABLED;NDEBUG;_CONSOLE;WIN32;HAVE_CONFIG_H;POSIX;CVSGUI_PIPE`
* Character set: Unicode
* Include directories: `zstd`, `external_libs`, `windows-NT`, `src`, `lib`, `diff`, `zlib`,
  `cvsgui`, `expat\lib`, `xmlapi`, `cvsapi\lib`, `cvsapi`, `cvstools`, `libxml\include`
* Libraries: `libssl.lib libcrypto.lib comctl32.lib wsock32.lib netapi32.lib mpr.lib ole32.lib
  libiconv.lib wininet.lib dbghelp.lib`

### Installer

```
cd cvsnt
make_msix64.bat        (64-bit)
make_msi.bat           (32-bit)
```

Both must be run **from the `cvsnt/` directory** — they invoke `msi_toolsin\candle.exe` and
`msi_toolsin\light.exe` by relative path and read the `.wxs` from the current directory. The WiX
toolset ships in `cvsnt/msi_tools/bin/`; nothing needs to be on `PATH`.

### Building without Visual Studio

`.vcxproj` files require MSBuild's C++ targets, which only ship with Visual Studio or the Visual
Studio Build Tools. With only a standalone MSVC toolchain — `cl.exe`, `link.exe`, `lib.exe`,
`nmake.exe` and a Windows SDK, but no MSBuild integration — the solution cannot be opened, and you
have to drive the compiler yourself.

**This has been done and verified.** A hand-written `nmake` makefile plus an environment batch file
(replacing the absent `vcvarsall.bat`) builds `cvs.exe` and its dependencies with MSVC 19.44 and
Windows SDK 10.0.22621.0. The resulting binary runs:

```
Concurrent Versions System (CVSNT) 3.5.24 (Gan + [Gaijin -kB/-kBz patch]) Build 7699 (client/server)
```

The dependency order that works is: `blake3`, `zlib`, `zstd`, `pcre`, `libxml2`, `ufc-crypt`,
`cvsgui`, `ca_blobs_fs`, `blob_sockets`, `clientLib`, `gnulib`, `libdiff`, `cvsdelta`, `libsuid` →
`cvsapi.dll` → `cvstools.dll` → `cvs.exe`. (`cvsapi.dll` needs `libxml2`, `pcre` and `ufc-crypt`;
`cvstools.dll` needs `cvsapi.dll` and `cvsgui`; the rest link into `cvs.exe`.)

Two things to know:

* `libcrypto-1_1-x64.dll` from `external_libs/x64/dll/` must sit next to `cvs.exe` for it to start —
  it is the only one of the two the binary actually imports. Copy `libssl-1_1-x64.dll` as well; a
  build that enables `:sserver:` will need it.
* `cvs init` needs the trigger plugin (`triggers/info_triggers.vcxproj`); without it you get
  `Couldn't open default trigger library`. Client commands work without it.

`_reports/BUILD-01-windows-toolchain.md` has the full environment setup, the makefile, the exact
dependency graph and every problem encountered along the way.

---

## Versioning

Three files decide what `cvs --version` prints:

| File | Contents |
| --- | --- |
| `version_no.h` | `CVSNT_PRODUCT_MAJOR` / `MINOR` / `PATCHLEVEL`, and the product-name suffix |
| `build.h` | `CVSNT_PRODUCT_BUILD` — a single integer, bumped per build |
| `version_fu.h` | Assembles the strings |

`genbuild/` contains a helper that regenerates `build.h`.

Bump `CVSNT_PRODUCT_PATCHLEVEL` or `CVSNT_PRODUCT_BUILD` when you change anything that affects
client/server capability negotiation — the server uses the client version to decide which requests
and responses it may use.

---

## Verifying a build

```bash
cvs --version
```

should print something like:

```
Concurrent Versions System (CVSNT) 3.5.24 (Gan + [Gaijin -kB/-kBz patch]) Build 7699 (client/server)
```

`(client/server)` confirms that server support was compiled in; a client-only build (macOS, and any
build configured with `--disable-server`) says `(client)`.

A quick functional smoke test without any server:

```bash
export CVSROOT=/tmp/testcvs
cvs init
cvs -d /tmp/testcvs checkout CVSROOT
```

For the blob path you need a running `cafs_server`; see
[docs/06-server-operations.md](docs/06-server-operations.md).

---

## Common problems

| Symptom | Cause and fix |
| --- | --- |
| `configure: error: requires libxml 2.7 or greater` | Install `libxml2-dev`/`libxml2-devel`. The check is `xmlSchemaValidCtxtGetParserCtxt` (`configure.in:365`) |
| `configure: error: requires zlib > 1.2.0` | Install a zlib whose headers provide `deflateBound()` (`zlib1g-dev` / `zlib-devel`). The check at `configure.in:512` link-tests that symbol |
| `No package 'libpcre' found` | Install `libpcre3-dev`/`pcre-devel`; the check goes through `pkg-config` |
| Link errors for `ZSTD_*` | zstd was not built and installed before the main `make`. Do `cd zstd && make && sudo make install` first |
| Illegal instruction at startup on an AMD CPU | AVX-512 is enabled by default and AMD CPUs before Zen 4 lack it. Reconfigure with `--disable-avx512` (commit `2cd984a`) |
| macOS: `could not detect openssl@3 location` | `brew install openssl@3`; `build-macosx` looks in `/usr/local/opt/openssl@3` and `/opt/homebrew/opt/openssl@3` only |
| Windows: `Microsoft.Cpp.Default.props was not found` | Visual Studio (or the Build Tools) is not installed, or the toolset in the projects is not installed. See "Building without Visual Studio" above |
| The build leaves modified files in the tree | `build-macosx` and `autoreconf` generate files in place. `git checkout .` after building |
