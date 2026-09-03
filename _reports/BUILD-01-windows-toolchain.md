---
id: BUILD-01
area: build / windows
status: success
---

# Building G-CVSNT on Windows without Visual Studio

## Result

**Full success.** `cvs.exe` (x64, Release) was built and runs. All 80 translation
units of `cvsnt.vcxproj` compiled to `.obj`, all 14 dependency libraries and both
DLLs (`cvsapi.dll`, `cvstools.dll`) built and linked. A `clean` + full rebuild
takes **~55 s** and finishes with **0 errors** (774 warnings, of which 261 come
from the 80 `cvsnt.vcxproj` TUs; the rest from vendored libxml2/pcre/zlib-ng).

Verified output (`cvs.exe --version`, run from the build's `bin` directory):

```
Concurrent Versions System (CVSNT) 3.5.24 (Gan + [Gaijin -kB/-kBz patch]) Build 7699 (client/server)

CVSNT 3.5.24 (Aug 27 2026) Copyright (c) 2020 Gaijin Games KFT.
see https://github.com/GaijinEntertainment/G-CVSNT


CVS Copyright (c) 1989-2001 Brian Berliner, david d `zoo' zuhn,

Jeff Polk, and other authors
CVSNT Copyright (c) 1999-2008 Tony Hoyle and others
Gaijin Copyright (c) 2008-2020 Nikolay Savichev, Anton Yudintsev and others
CVSNT may be copied only under the terms of the GNU General Public License v2,
a copy of which can be found with the CVS distribution.

The CVSNT Application API is licensed under the terms of the
GNU Library (or Lesser) General Public License.

Specify the --help option for further information about CVS
```

Further smoke tests that passed: `cvs --help-commands` (full command list),
`cvs -H diff` (usage text). `cvs init` fails with
`cvs [init aborted]: Couldn't open default trigger library: No such file or directory`
— that is **not** a build failure: `init` is a server-side operation that
`dlopen`s the `info_triggers` plugin DLL, which is a separate project
(`triggers\info_triggers.vcxproj`) not required to link or run the client.

Artifacts produced (all in the scratchpad, nothing written into the repo):

| output | size |
|---|---|
| `bin\cvs.exe` | 2,378,240 |
| `bin\cvsapi.dll` | 2,849,280 |
| `bin\cvstools.dll` | 426,496 |
| `lib\*.lib` (14 static + 2 import) | see table under *Dependency order* |

Object counts per component: cvsnt 80, libxml2 43, cvsapi 39, zlib 29, zstd 25,
pcre 20, libdiff 14, cvstools 12, gnulib 6, clientLib 5, ca_blobs_fs 4, blake3 3,
cvsgui 3, cvsdelta 2, libsuid 2, ufc-crypt 2, blob_sockets 1. **Total 290 objects.**

`cvs.exe` import table (`dumpbin /dependents`):
`cvsapi.dll, cvstools.dll, libcrypto-1_1-x64.dll, WSOCK32, WS2_32, NETAPI32,
ole32, OLEAUT32, WININET, dbghelp, ADVAPI32, USER32, SHELL32, Secur32,
MSVCP140, KERNEL32, VCRUNTIME140, VCRUNTIME140_1`.

## Environment

| component | path | version |
|---|---|---|
| C/C++ compiler | `D:\devtools\vc2022_17.14.4\bin\Hostx64\x64\cl.exe` | MSVC **19.44.35209** for x64 |
| linker / librarian / make | same directory (`link.exe`, `lib.exe`, `nmake.exe`) | — |
| MSVC headers / libs | `D:\devtools\vc2022_17.14.4\include`, `...\lib\x64` | 14.44 |
| Windows SDK | `D:\devtools\win.sdk.100`, version `10.0.22621.0` | headers `include\10.0.22621.0\{ucrt,shared,um,winrt,cppwinrt}`, libs `lib\10.0.22621.0\{ucrt,um}\x64` |
| `rc.exe`, `mc.exe`, `midl.exe` | `D:\devtools\win.sdk.100\bin\10.0.18362.0\x64` | 10.0.18362.0 |
| OS | Windows 11 Pro 10.0.26200 | — |

No Visual Studio, no MSBuild, no `Microsoft.Cpp.targets`, no `vcvarsall.bat` on
this machine — the `.sln`/`.vcxproj` route is genuinely unavailable. The SDK's
`10.0.22621.0` tree has no `bin\` of its own; the only SDK binaries present are
under `bin\10.0.18362.0`, which is why `rc/mc/midl` come from the older revision.
That mismatch caused no problems.

Prebuilt third-party import libs used as-is from the repo:
`cvsnt\cvsnt-2.5.05.3744\external_libs\x64\{libssl,libcrypto,libiconv,dnssd}.lib`,
runtime DLLs from `external_libs\x64\dll\{libssl,libcrypto}-1_1-x64.dll`.

## Environment setup

`_reports/BUILD-01-env.bat` (identical copy also at
`<scratchpad>\build\env.bat`):

```bat
@echo off
rem ---- Standalone MSVC toolchain (no Visual Studio installed) ----
set VCTOOLS=D:\devtools\vc2022_17.14.4
set WSDK=D:\devtools\win.sdk.100
set SDKVER=10.0.22621.0
set RCDIR=%WSDK%\bin\10.0.18362.0\x64

set PATH=%VCTOOLS%\bin\Hostx64\x64;%RCDIR%;%PATH%

set INCLUDE=%VCTOOLS%\include;%WSDK%\include\%SDKVER%\ucrt;%WSDK%\include\%SDKVER%\shared;%WSDK%\include\%SDKVER%\um;%WSDK%\include\%SDKVER%\winrt;%WSDK%\include\%SDKVER%\cppwinrt

set LIB=%VCTOOLS%\lib\x64;%WSDK%\lib\%SDKVER%\ucrt\x64;%WSDK%\lib\%SDKVER%\um\x64

set LIBPATH=%LIB%
```

Verified with a hello-world that includes both `<stdio.h>` and `<windows.h>`,
compiles with `/MD` and links against the console subsystem.

## Build recipe

Full makefile: `_reports/BUILD-01-Makefile.nmake` (working copy
`<scratchpad>\build\Makefile.nmake`). Driver:

```bat
rem <scratchpad>\build\mk.bat
@echo off
call "%~dp0env.bat"
cd /d "%~dp0"
nmake /NOLOGO /F Makefile.nmake %*
```

Usage: `mk.bat all` (or `mk.bat clean`, `mk.bat cvsapi`, `mk.bat cvsexe`, …).

Structure: one `cl` invocation per project (cl compiles the whole file list in
one process, which is both fast and closest to what MSBuild does), then `lib` or
`link`. Outputs go to `<build>\obj\<project>\`, `<build>\lib\`, `<build>\bin\`.
Generated headers go to `<build>\gen\`. **Nothing is written inside the repo.**

Common flags, transcribed from the `Release|x64` `ItemDefinitionGroup` of
`cvsnt.vcxproj` (`MaxSpeed`→`/O2`, `IntrinsicFunctions`→`/Oi`,
`FavorSizeOrSpeed=Speed`→`/Ot`, `OmitFramePointers`→`/Oy`, `StringPooling`→`/GF`,
`MultiThreadedDLL`→`/MD`, `BufferSecurityCheck=false`→`/GS-`,
`RuntimeTypeInfo=false`→`/GR-`, `WarningLevel=Level3`→`/W3`):

```
/nologo /c /O2 /Oi /Ot /Oy /GF /MD /GS- /GR- /W3 /Zi /FS
/D NDEBUG /D WIN32 /D _CRT_SECURE_NO_WARNINGS /D _WINSOCK_DEPRECATED_NO_WARNINGS
```

plus, for `cvs.exe` itself (`CharacterSet=Unicode` → `/D _UNICODE /D UNICODE`):

```
/D _UNICODE /D UNICODE /EHsc
/D ISOLATION_AWARE_ENABLED /D _CONSOLE /D HAVE_CONFIG_H /D POSIX /D CVSGUI_PIPE
/I zstd /I external_libs /I windows-NT /I src /I lib /I diff /I zlib /I cvsgui
/I xmlapi /I cvsapi\lib /I cvsapi /I cvstools /I libxml\include /I .
```

Deviations from the vcxproj, all confined to the build script:

* **`/EHsc` added** (vcxproj says `<ExceptionHandling>false</ExceptionHandling>`).
  Needed because `windows-NT\setuid.cpp:501` contains a real `try`/`catch(_com_error)`.
  See P6 below — this is a project-file bug, not a toolchain issue.
* **`/GL` (WholeProgramOptimization) and `/LTCG` dropped.** Not needed; only
  affects codegen quality.
* **No `/wd…` suppressions were required.** I initially added a defensive list
  and then removed it: a control run at `/W3` with zero suppressions
  (`<scratchpad>\build\warn.bat`, log `log-warnscan.txt`) compiles all 80 TUs
  cleanly. MSVC 19.44 needs no workarounds for this source tree.
* `_CRT_SECURE_NO_WARNINGS` / `_WINSOCK_DEPRECATED_NO_WARNINGS` added purely to
  cut deprecation noise.
* `secur32.lib` added to the `cvs.exe` link line (see P5).

Generated-file steps that MSBuild would have run as `CustomBuild` / `Midl` items,
redirected out of tree:

```make
# cvsapi: <CustomBuild Include="win32\ServiceMsg.mc"> -> "mc win32/ServiceMsg.mc"
$(GEN)\cvsapi\ServiceMsg.h: dirs
	cd /d "$(S)\cvsapi" && mc -h "$(GEN)\cvsapi" -r "$(GEN)\cvsapi" win32\ServiceMsg.mc

# cvstools: <Midl Include="win32\trigger.idl">
$(GEN)\cvstools\trigger_h.h: dirs
	cd /d "$(S)\cvstools" && midl /nologo /env x64 /W1 /char signed \
		/out "$(GEN)\cvstools" /h trigger_h.h /iid trigger_i.c /proxy trigger_p.c \
		/dlldata trigger_dlldata.c /tlb win32\trigger.tlb \
		/I "$(WSDK)\lib\$(SDKVER)\um\x64" win32\trigger.idl
```

Resource compilation must run with the **project directory as CWD**, because the
`.rc` files use includes that are relative to the CWD, not to the `.rc` file:

```make
cd /d "$(S)"          && rc /nologo /d NDEBUG /I "$(S)\windows-NT" /fo"…\cvsnt.res"    windows-NT\cvsnt.rc
cd /d "$(S)\cvsapi"   && rc /nologo /d NDEBUG /I "$(GEN)\cvsapi" …  /fo"…\cvsapi.res"  win32\cvsapi.rc
cd /d "$(S)\cvstools" && rc /nologo /d NDEBUG /I "$(GEN)\cvstools" … /fo"…\cvstools.res" win32\cvstools.rc
```

Logs kept in the scratchpad:
`log-full.txt` (complete clean rebuild), `log-warnscan.txt` (unsuppressed warning
survey of the 80 client TUs), plus one `log-<component>.txt` per iteration step.
Scratchpad root:
`C:\Users\dark\AppData\Local\Temp\claude\D--another-G-CVSNT\5b443ce4-edac-46ad-9de8-55d91422aefa\scratchpad\build\`

## Dependency order

The real link-time dependency set was derived from unresolved symbols, not from
the 45 `<ProjectReference>` entries in `cvsnt.vcxproj` (most of which are
separate executables/plugins that `cvs.exe` never links against).

1. **Leaf static libs**, no inter-dependencies, any order:
   `zlib` (zlib-ng, 29 obj) · `zstd` (25) · `pcre` (20) · `libxml2` (43) ·
   `blake3` (3) · `ca_blobs_fs` (4) · `clientLib` (5) · `blob_sockets` (1) ·
   `cvsgui` (3) · `gnulib` (6) · `libdiff` (14) · `ufc-crypt` (2) ·
   `cvsdelta` (2) · `libsuid` (2)
2. **`cvsapi.dll`** — needs `libxml2.lib`, `pcre.lib`, `ufccrypt.lib`, plus the
   `mc`-generated `ServiceMsg.h/.rc`. Emits `cvsapi.lib` import lib.
3. **`cvstools.dll`** — needs `cvsapi.lib` and `cvsgui.lib`, plus the
   `midl`-generated `trigger_h.h`, `trigger_i.c` and `trigger.tlb`.
4. **`cvs.exe`** — 80 objects + `cvsnt.res` + all of the above.

Why each non-obvious one is required (symbol that pulled it in):

| library | pulled in by |
|---|---|
| `ufc-crypt` | `crypt` ← `cvsapi\crypt.cpp`, `cvsapi\lib\md5crypt.c` |
| `cvsgui` | `cvsguiglue_init/_close/_getenv`, `gp_console_write`, `_cvsgui_writefd/_readfd` ← `cvstools\Cvsgui.cpp`, `cvstools\ProtocolLibrary.cpp` |
| `cvsdelta` | `cvsdelta_patch` ← `src\rcs_checkin.cpp` |
| `libsuid` | `SuidGetImpersonationTokenW` ← `windows-NT\win32.cpp` (`trysuid`) |
| `secur32.lib` | `LsaRegisterLogonProcess`, `LsaLogonUser`, `LsaDeregisterLogonProcess` ← `windows-NT\setuid.cpp` (`nt_s4u`) |

Projects listed in the task brief that turned out **not** to be needed:
`xdiff\*` (the `xml_xdiff.dll` is loaded at runtime by `src\xdiff.cpp`, never
linked), and the ~30 other `<ProjectReference>` entries (protocol DLLs, triggers,
`cvsservice`, `cvsntcpl`, installers, `plink`, `rcs\*`, …) which are separate
deliverables of the product, not link inputs of `cvs.exe`.

## Problems encountered and how they were solved

### P1: `vcvarsall.bat` does not exist — no environment for `cl.exe`
- **Where:** toolchain, not source.
- **Diagnostic:** `cl.exe` alone reports `fatal error C1034: stdio.h: no include path set`.
- **Cause:** The MSVC and SDK trees are present as loose directories, without the
  VS installer's `Auxiliary\Build\vcvarsall.bat` or `VC\Auxiliary\Build\*.props`.
- **Resolution:** Hand-written `env.bat` (above). The one thing worth noting is
  that the SDK's *tool* binaries live under `bin\10.0.18362.0\x64` while the
  *headers and libs* used are `10.0.22621.0` — those two version numbers do not
  have to match.

### P2: `ServiceMsg.h` is generated, not checked in
- **Where:** `cvsnt\cvsnt-2.5.05.3744\cvsapi\ServerIO.cpp:32`
- **Diagnostic:** `fatal error C1083: Cannot open include file: 'ServiceMsg.h': No such file or directory`
- **Cause:** `cvsapi.vcxproj` line 352 has
  `<CustomBuild Include="win32\ServiceMsg.mc">` running `mc win32/ServiceMsg.mc`.
  The message compiler emits `ServiceMsg.h`, `ServiceMsg.rc` and `MSG00001.bin`
  into the project directory. Only the `.mc` source is in git.
- **Resolution:** Run the SDK's `mc.exe` explicitly, redirected out of tree:
  `mc -h <gen>\cvsapi -r <gen>\cvsapi win32\ServiceMsg.mc` (CWD = `cvsapi\`),
  and add `<gen>\cvsapi` to both the `cl` and the `rc` include paths.

### P3: `trigger_h.h` / `trigger_i.c` are MIDL output, not checked in
- **Where:** `cvstools\TriggerLibrary.cpp:45` and the `ClCompile` entry
  `cvstools\trigger_i.c`.
- **Diagnostic:**
  `fatal error C1083: Cannot open include file: 'trigger_h.h': No such file or directory`
  and
  `c1: fatal error C1083: Cannot open source file: '…\cvstools\trigger_i.c': No such file or directory`
- **Cause:** `cvstools.vcxproj` has `<Midl Include="win32\trigger.idl">` with only
  `TypeLibraryName` overridden. Everything else comes from MSBuild's defaults,
  which are `%(Filename)_h.h` for the header and `%(Filename)_i.c` for the IID
  file — hence the odd-looking `trigger_h.h` name. Without MSBuild these defaults
  have to be re-supplied by hand.
- **Resolution:** the `midl` command shown under *Build recipe*, with
  `/h trigger_h.h /iid trigger_i.c`, output redirected to `<gen>\cvstools`, and
  the makefile compiling `<gen>\cvstools\trigger_i.c` instead of the (nonexistent)
  in-tree path.

### P4: `RC2135` — `win32\trigger.tlb` not found while compiling cvstools resources
- **Where:** `cvstools\win32\cvstools.rc2:6` — `1 TYPELIB "win32\\trigger.tlb"`
- **Diagnostic:** `win32\cvstools.rc2(6) : error RC2135 : file not found: win32\trigger.tlb`
- **Cause:** The `.rc2` hard-codes the type library at a project-relative path,
  which MSBuild satisfies because MIDL writes the `.tlb` into the project tree.
  Building out of tree breaks that assumption.
- **Resolution:** have MIDL write the tlb to `<gen>\cvstools\win32\trigger.tlb`
  (i.e. `/out <gen>\cvstools /tlb win32\trigger.tlb`) and pass
  `/I <gen>\cvstools` to `rc.exe`, so the literal path `win32\trigger.tlb`
  resolves through the include search. No source or repo file touched.

### P5: five unresolved externals at the `cvs.exe` link
- **Where:** link step.
- **Diagnostic:**
  ```
  rcs_checkin.obj : error LNK2019: unresolved external symbol cvsdelta_patch referenced in function "void __cdecl RCS_deltas(...)"
  setuid.obj : error LNK2019: unresolved external symbol LsaRegisterLogonProcess referenced in function "int __cdecl nt_s4u(wchar_t const *,wchar_t const *,void * *)"
  setuid.obj : error LNK2019: unresolved external symbol LsaLogonUser referenced in function "int __cdecl nt_s4u(...)"
  setuid.obj : error LNK2019: unresolved external symbol LsaDeregisterLogonProcess referenced in function "int __cdecl nt_s4u(...)"
  win32.obj  : error LNK2019: unresolved external symbol SuidGetImpersonationTokenW referenced in function "int __cdecl trysuid(struct passwd const *,void * *)"
  …fatal error LNK1120: 5 unresolved externals
  ```
- **Cause:** `<AdditionalDependencies>` in `cvsnt.vcxproj` lists only the external
  libraries; the in-tree static libs arrive via `<ProjectReference>`, and
  `secur32.lib` is inherited from a `.props`/toolset default that does not exist
  outside MSBuild.
- **Resolution:** build `cvsdelta.vcxproj` and
  `windows-NT\setuid\libsuid\libsuid.vcxproj` as static libs and add
  `cvsdelta.lib`, `libsuid.lib` and `secur32.lib` to the link line. Same class of
  problem, earlier: `crypt` (→ `ufc-crypt`) for `cvsapi.dll`, and the
  `cvsguiglue_*` family (→ `cvsgui`) for `cvstools.dll`.

### P6: `windows-NT\setuid.cpp` uses C++ exceptions while the project disables them
- **Where:** `cvsnt\cvsnt-2.5.05.3744\windows-NT\setuid.cpp:501` (the `catch` is
  at line 514: `catch(_com_error e)`).
- **Diagnostic:** `warning C4530: C++ exception handler used, but unwind semantics
  are not enabled. Specify /EHsc`
- **Cause:** the `Release|x64` configuration of `cvsnt.vcxproj` sets
  `<ExceptionHandling>false</ExceptionHandling>` (the single hit, line 188; the other three
  configurations leave the toolset default), yet this TU has a real
  `try`/`catch` around COM calls — see `BUG-build-02` for the single-config fact.
- **Resolution:** compiled with `/EHsc` (build-flag only). The proper fix is a
  **project-file change**: set `<ExceptionHandling>Sync</ExceptionHandling>` in
  `cvsnt.vcxproj` for `Release|x64`. With `ExceptionHandling=false` the
  binary that MSBuild produces today does not run destructors when that `catch`
  fires — see *Source-level issues*.

### P7: driving `cmd` from Git Bash
- **Where:** harness, not the project.
- **Diagnostic:** `cmd /c script.bat` hangs until timeout (an interactive `cmd`
  banner appears in the log).
- **Cause:** MSYS path mangling rewrites the `/c` switch into a Windows path, so
  `cmd` starts interactively and blocks on stdin.
- **Resolution:** use `cmd //c` from Git Bash (or drive everything from
  PowerShell). Worth writing down for anyone reproducing this.

## Source-level issues found (candidate bugs / portability problems)

Warning survey of exactly the 80 `cvsnt.vcxproj` TUs at `/W3` with **no**
suppressions (`log-warnscan.txt`): 261 warnings — 167 × C4267 (`size_t` → smaller
type), 76 × C4005 (macro redefinition, mostly `_CRT_NONSTDC_NO_DEPRECATE`
predefined by the build), 7 × C4101 (unused local), 5 × C4311, 5 × C4302,
1 × C4312. Zero errors. The tree is clean under MSVC 19.44.

The pointer-truncation warnings are the ones worth acting on. On Win64 `long` is
still 32-bit, so every one of these silently drops the top half of a pointer or
handle:

1. **`windows-NT\win32.cpp:1241`** — the most serious.
   ```cpp
   return _open_osfhandle((long)h,_O_RDWR|_O_BINARY);
   ```
   `warning C4311: 'type cast': pointer truncation from 'HANDLE' to 'long'`.
   `_open_osfhandle` takes `intptr_t`; the explicit `(long)` throws away the high
   32 bits of the handle before the call. Fix: `(intptr_t)h`.

2. **`windows-NT\win32.cpp:2079`**
   ```cpp
   buf->st_rdev = buf->st_dev = (_dev_t)hFile;
   ```
   `warning C4302: truncation from 'HANDLE' to '_dev_t'`. `_dev_t` is 32-bit.
   Low impact (the field is informational) but it makes `st_dev` non-unique.

3. **`windows-NT\waitpid.cpp:19`**
   ```cpp
   return (pid_t)_cwait (statusp, (int)pid, _WAIT_CHILD);
   ```
   `warning C4311: pointer truncation from 'void *' to 'int'`. On Windows `pid_t`
   carries a process HANDLE; `_cwait` takes `intptr_t`. Fix: `(intptr_t)pid`.

4. **`src\commit.cpp:1207`**
   ```cpp
   if(((long)closure)==1) /* dll call */
   ```
   `warning C4311: pointer truncation from 'void *' to 'long'`. Works today only
   because the sentinel is the small constant `1`. Fix: compare against
   `(intptr_t)closure`.

5. **`src\rcs.cpp:7369`** (compiled into `rcs_checkin.obj` — see below)
   ```cpp
   // Cast pointer to in to int is an error in gcc
   // This is probably masking another bug - the data values are pointers
   // to allocated memory, not integer values
   rv = (int)(unsigned long)(void*)info.rev_list->list->next->data;
   ```
   The comment is the authors' own; the C4311/C4302 pair confirms it. Related:
   `src\rcs.cpp:7485` stores a small integer into `Node::data` (`char*`), giving
   `warning C4312: conversion from 'int' to 'char *' of greater size`.

**Project-configuration bugs (not compiler bugs):**

6. `cvsnt.vcxproj` sets `<ExceptionHandling>false</ExceptionHandling>` in its
   `Release|x64` configuration (line 188; the other three leave the default)
   while `windows-NT\setuid.cpp:501-514` uses `try`/`catch`. The shipped x64
   release binary therefore has no unwind semantics on that path. See P6 and
   `BUG-build-02`.

7. `cvsnt.vcxproj` lines **204** and **309** (`Release|x64` and `Debug|x64`):
   ```xml
   <AdditionalLibraryDirectories>..\external_libsx64;.\external_libs\x64;</AdditionalLibraryDirectories>
   ```
   `..\external_libsx64` is missing a backslash — compare the Win32 configs which
   correctly read `..\external_libs\Win32`. Harmless only because the second entry
   happens to be the one that works.

8. `cvsapi.vcxproj` compiles with `XML_STATIC` (an *expat*-era macro) but never
   defines `LIBXML_STATIC`, even though `libxml2.vcxproj` builds a static lib with
   `LIBXML_STATIC`. Consumers therefore see libxml symbols as `__declspec(dllimport)`
   and the linker reports
   `LNK4217: symbol 'xmlFree' defined in 'libxml2.lib(globals.obj)' is imported by 'XmlNode.obj'`.
   Benign (the linker fixes it up through a thunk) but it costs an indirection on
   every libxml call and would be fixed by adding `LIBXML_STATIC` to
   `cvsapi.vcxproj`'s preprocessor definitions.

**Structural observations (informational):**

9. `src\rcs_checkin.cpp:11` is `#include "rcs.cpp"` — a 7,615-line unity-build
   include. That is why `rcs.cpp` is absent from `cvsnt.vcxproj`'s `ClCompile`
   list and why `rcs_checkin.obj` is by far the largest object. Similarly
   `src\rcs.cpp:7615` is `#include "rcs_cvt_kB.cpp"`.

10. Three files under `src\` are in neither `cvsnt.vcxproj` nor `src\Makefile.am`
    and are not `#include`d anywhere — dead code:
    `src\Modules1.cpp`, `src\Modules2.cpp`, `src\RecurseRepository.cpp`
    (with their headers `Modules1.h`, `Modules2.h`, `RecurseRepository.h`).
    (`src\filesubr.cpp` and `src\stripslash.cpp` are *not* dead — they are the
    POSIX variants used by `Makefile.am`; Windows uses the `windows-NT\` versions.)

11. `cvsnt.vcxproj` `<Manifest><AdditionalManifestFiles>longfilenames.xml`
    references a file that does not exist anywhere in the tree. The link succeeds
    without it (a default manifest is generated), so the MSBuild build is
    presumably also silently ignoring it.

## Notes for HOWTOBUILD.md

Building the Windows client **without Visual Studio**, using only a standalone
MSVC toolchain plus a Windows SDK.

**Prerequisites**

* An MSVC toolset directory containing `bin\Hostx64\x64\{cl,link,lib,nmake}.exe`,
  `include\` and `lib\x64\`. Verified with 19.44 (VS 2022 17.14); 16.11 / 17.9
  should work too.
* A Windows 10/11 SDK with `include\<ver>\{ucrt,shared,um,winrt,cppwinrt}`,
  `lib\<ver>\{ucrt,um}\x64`, and **`rc.exe`, `mc.exe`, `midl.exe`** somewhere
  under `bin\<any-ver>\x64`. The tool version does not need to match the
  header/lib version.

**Steps**

1. Copy `_reports/BUILD-01-env.bat` and `_reports/BUILD-01-Makefile.nmake` into an
   empty build directory as `env.bat` and `Makefile.nmake`. Edit the four `set`
   lines at the top of `env.bat` (`VCTOOLS`, `WSDK`, `SDKVER`, `RCDIR`) and the
   `S = …` line at the top of `Makefile.nmake` (absolute path of
   `cvsnt\cvsnt-2.5.05.3744`).
2. Add a one-line `mk.bat`:
   ```bat
   @echo off
   call "%~dp0env.bat"
   cd /d "%~dp0"
   nmake /NOLOGO /F Makefile.nmake %*
   ```
3. `mk.bat all` — about a minute. Outputs land in `bin\` and `lib\` of the build
   directory; nothing is written into the source tree.
4. Copy `external_libs\x64\dll\libssl-1_1-x64.dll` and `libcrypto-1_1-x64.dll`
   next to `cvs.exe`, then `cvs.exe --version`.

Single components can be rebuilt with `mk.bat zlib | zstd | pcre | libxml2 |
blake3 | cablobs | clientlib | blobsock | cvsgui | gnulib | libdiff | ufccrypt |
cvsapi | cvstools | cvsexe`. `mk.bat clean` removes `obj\`, `lib\` and `bin\`.

**Caveats**

* The makefile compiles a whole project in one `cl` invocation and has no
  per-file dependency tracking: a library is rebuilt only if its `.lib` is
  missing. Delete the `.lib` (or run `clean`) after editing sources. The 80
  client TUs are always recompiled.
* Two generated files are *not* in git and must be produced before compiling —
  `cvsapi\…\ServiceMsg.h` (message compiler) and `cvstools\…\trigger_h.h` +
  `trigger_i.c` + `trigger.tlb` (MIDL). The makefile does this; a hand-rolled
  build must too. Note the MSBuild MIDL naming convention `%(Filename)_h.h` /
  `%(Filename)_i.c`.
* `rc.exe` **must** run with the project directory as its current directory —
  `windows-NT\cvsnt.rc` needs CWD = `cvsnt-2.5.05.3744\`, `cvsapi\win32\cvsapi.rc`
  needs CWD = `cvsapi\`, `cvstools\win32\cvstools.rc` needs CWD = `cvstools\`.
  Their `#include "../version_no.h"` / `res\cvsnt.rc2` /
  `..\windows-NT\VersionInfoCommon.rc2` lines are resolved against the CWD, not
  against the `.rc` file.
* `cvs.exe` links against `cvsapi.dll` and `cvstools.dll`; both must be on the
  `PATH` or beside the executable, together with the two OpenSSL 1.1 DLLs.
* This builds the **client**. Server-side operations (`cvs init`, running as a
  server) additionally need the trigger and protocol plugin DLLs
  (`triggers\*.vcxproj`, `protocols\*.vcxproj`), which are separate projects and
  are not part of this recipe.
* From Git Bash, invoke batch files as `cmd //c foo.bat` — a single slash is
  rewritten by MSYS and `cmd` hangs waiting on stdin.
