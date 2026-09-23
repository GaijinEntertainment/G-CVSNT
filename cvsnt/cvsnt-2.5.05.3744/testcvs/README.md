# Tests

Three suites, in increasing order of what they need to run.

| Suite | What it covers | Needs |
| --- | --- | --- |
| `unit/unit_tests.cpp` | Header-resident blob code: header format, the streaming header accumulator, wire hash encoding | A C++17 compiler, zlib, zstd. No repository, no server, no socket |
| `regress.py` | End-to-end behaviour against a **local** repository: import, checkout, commit, tag, branch, sticky tags, `-C`/`-n` backups, pruning | A built `cvs` and its plugin directory |
| `testcvs.py` | The original CVSNT acceptance suite: 17 scenarios including `-kB` binary delta and the `*info` triggers | A built `cvs`, and `test_data/` |

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

## `testcvs.py`

The suite that shipped with CVSNT, in its original scenarios and order. It works in `work_<instance>/`
beside itself and removes that directory on entry and on exit (`--keep` to inspect it).

```bash
python3 testcvs.py --cvs /usr/local/bin/cvs
python3 testcvs.py --cvs ../Releasex64/cvs.exe --libdir ../Releasex64 -v
python3 testcvs.py -v                      # cvs from PATH
```

or, on Windows, `testcvs.bat`, which just forwards its arguments.

The scenarios run in five groups, each with a repository of its own. The order inside a group is
fixed, because the golden outputs in `test_data` pin revision numbers, branch numbers and tag sets
that only the whole sequence produces — `info_test_output.txt`, for one, names
`new revision: 1.6`. A scenario that fails is reported `FAIL` with the failing command, the exit
code (a death by signal is named as such, not reported as an error code) and the captured stderr;
the rest of *its* group is then reported `blocked` and the other groups still run. A scenario two
groups both need is listed in both.

Every command has a timeout (`--timeout`, 300 s by default), so a client that hangs fails its
scenario instead of the whole job. `--cvs` and `--libdir` mean the same as in `regress.py`, and
`-i/--instance` separates the scratch directories of parallel runs.

Exit status is 0 only if every scenario passed.

## What to test when changing things

* **A bugfix** should come with a test that fails before it and passes after. Verify that directly:
  reintroduce the bug, watch the test fail, then restore.
* **A performance change** must not change behaviour, so the relevant `regress.py` case should exist
  and pass *before* the optimization goes in. See
  [suggested_optimizations.md](../../../suggested_optimizations.md).
* **Anything that rewrites a `,v`** needs a byte-exact round-trip check against real repository
  files, not just a functional test. Corruption there is silent and permanent.
