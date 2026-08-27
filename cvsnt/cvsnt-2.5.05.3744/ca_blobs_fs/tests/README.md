# ca_blobs_fs unit tests

`test_ca_blobs_fs.cpp` is a standalone test for the content-addressed blob
store. It links against the built `ca_blobs_fs` static library and its
dependencies and exercises push/dedup/exists/pull plus the audited
`set_root(ctx, nullptr)` contract.

## Prerequisites

Build the libraries first (they land in `Releasex64/`):

```
cd cvsnt
python build-windows.py --vc <MSVC dir> --sdk <Win10 SDK dir> --projects zlib,zstd,blake3,ca_blobs_fs
```

## Build & run (Windows, bare MSVC toolchain)

From a shell with the MSVC/SDK `INCLUDE`/`LIB` environment set (a
"x64 Native Tools" prompt, or export them as `build-windows.py` does), from
the source root `cvsnt-2.5.05.3744/`:

```
cl /nologo /std:c++17 /EHsc /MD /I ca_blobs_fs ^
   ca_blobs_fs\tests\test_ca_blobs_fs.cpp ^
   Releasex64\ca_blobs_fs.lib Releasex64\blake3.lib Releasex64\zstd.lib Releasex64\zlib.lib ^
   /Fe:test_ca_blobs_fs.exe
test_ca_blobs_fs.exe
```

Exit code 0 means all checks passed; non-zero prints which check failed.

On Linux/macOS the equivalent is a `clang++ -std=c++17` link against the
`libca_blobs_fs`/`libblake3` produced by the autotools build (see
`tools/build_tools` for the analogous link lines).
