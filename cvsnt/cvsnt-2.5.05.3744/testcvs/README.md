# Tests

Three suites, in increasing order of what they need to run.

| Suite | What it covers | Needs |
| --- | --- | --- |
| `unit/unit_tests.cpp` | Header-resident blob code: header format, the streaming header accumulator, wire hash encoding | A C++17 compiler, zlib, zstd. No repository, no server, no socket |
| `regress.py` | End-to-end behaviour against a **local** repository: import, checkout, commit, tag, branch, sticky tags, `-C`/`-n` backups, pruning | A built `cvs` and its plugin directory |
| `testcvs.py` | The original CVSNT acceptance suite: 18 scenarios including `-kB` binary delta and the `*info` triggers | A built `cvs` on `PATH`, and `test_data/` |

## `unit/unit_tests.cpp`

Pure functions only, so it builds and runs anywhere the tree does.

```bash
cd testcvs/unit
c++ -std=c++17 -I.. -I../../ca_blobs_fs -I../../src -I../../zstd -I../../zlib \
    -I../../keyValueServer/include \
    unit_tests.cpp ../../ca_blobs_fs/src/streaming_compressors.cpp \
    -lz -lzstd -o unit_tests
./unit_tests
```

On Windows with a standalone MSVC toolchain (see [HOWTOBUILD.md](../../../HOWTOBUILD.md)):

```
cl /nologo /EHsc /MD /std:c++17 /I.. /I..\..\ca_blobs_fs /I..\..\src ^
   /I..\..\zstd /I..\..\zlib /I..\..\keyValueServer\include ^
   unit_tests.cpp ..\..\ca_blobs_fs\src\streaming_compressors.cpp ^
   /Fe:unit_tests.exe /link zlib.lib zstd.lib
```

**`/MD` is required.** The in-tree zlib and zstd are built against the dynamic CRT; compiling the
test with the default `/MT` gives unresolved `__imp__aligned_malloc` / `__imp__aligned_free`.

Exit status is 0 when every check passes; the count is printed at the end.

## `regress.py`

Runs against a local repository, so it needs no server, no lock server and no blob store. Every test
gets its own throwaway repository and working copy in a temporary directory.

```bash
python3 regress.py --cvs /usr/local/bin/cvs
python3 regress.py --cvs ../../build/bin/cvs.exe --libdir ../../build/bin -v
```

`--libdir` becomes the global `-L` option and is needed when running `cvs` out of a build tree
rather than an installation — that is where it looks for the protocol and trigger plugins. The
`info` trigger must be present as `<libdir>/triggers/info.<so|dll>`, or `cvs init` aborts with
`Couldn't open default trigger library`.

Note that the tests initialise repositories with `cvs init -n`. Without `-n`, `init` also tries to
register the repository in the machine-global settings, which needs privileges a test should not
require.

One case, the `-ku` line-ending test, has to go through a real client/server session because only
the client-side write path is affected. It uses the `:ext:` protocol with `CVS_EXT` set to a Python
pass-through and `CVS_SERVER` set to `<cvs> --allow-root=<repo> server`, so it needs the `ext`
protocol plugin as `<libdir>/protocols/ext.<so|dll>` and nothing registered anywhere. When the
plugin is missing the case prints a skip note instead of failing. On Windows the standalone
makefile in `_reports/BUILD-01-Makefile.nmake` builds it with `mk.bat plugins`.

## `testcvs.py`

The suite that shipped with CVSNT. It expects `cvs` on `PATH` and creates `tree/`, `tree_0/`,
`repos/` and `repos_0/` beside itself.

```bash
python3 testcvs.py -v
```

or, on Windows, `testcvs.bat`, which clears those directories first.

It stops at the first failure and prints the captured stderr.

## What to test when changing things

* **A bugfix** should come with a test that fails before it and passes after. Verify that directly:
  reintroduce the bug, watch the test fail, then restore.
* **A performance change** must not change behaviour, so the relevant `regress.py` case should exist
  and pass *before* the optimization goes in. See
  [suggested_optimizations.md](../../../suggested_optimizations.md).
* **Anything that rewrites a `,v`** needs a byte-exact round-trip check against real repository
  files, not just a functional test. Corruption there is silent and permanent.
