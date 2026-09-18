# CI for G-CVSNT

Two GitHub Actions workflows and the scripts they run.

| Workflow | Trigger | What it does |
| --- | --- | --- |
| `.github/workflows/test.yml` | every pull request; `workflow_dispatch` with a PR list | builds base + PRs, runs every suite on Linux and Windows, runs the `:pserver:` scenario against a fresh server contour |
| `.github/workflows/release.yml` | push of a `v*` tag; `workflow_dispatch` | deb/rpm/macOS/Windows packages, server images, draft GitHub release (from Konstantin Belov's `ci-release` branch, adapted) |

Everything under `ci/` runs in the workflows and can be run by hand; nothing here needs
credentials.

## Fork and enable Actions

1. Fork `GaijinEntertainment/G-CVSNT` to your account.
2. Push this branch to the fork (`git push <fork> ci/autotests`).
3. In the fork: **Settings → Actions → General → Allow all actions**, and under
   *Workflow permissions* leave **Read repository contents**.
4. The `test` workflow appears under **Actions** once the branch containing
   `.github/workflows/test.yml` is the default branch of the fork **or** the workflow file is
   present on the branch you dispatch from. Simplest: make `ci/autotests` the fork's default
   branch while it is a testbed.

Secrets: **none** are needed for the test workflow against a public source repository. If the
source repository is private, add `SOURCE_REPO_TOKEN` (fine-grained PAT, read-only on contents
and pull requests). The release workflow pushes images to `ghcr.io` with the built-in
`GITHUB_TOKEN`; publishing to Nexus is not implemented and would take
`${{ secrets.NEXUS_USER }}` / `${{ secrets.NEXUS_PASSWORD }}` in a new publish job.

## "Build master + PRs 28, 29, 30 and test"

**Actions → test → Run workflow**:

| Input | Value |
| --- | --- |
| `prs` | `28,29,30` — merged onto the base **in this order**; a branch name is accepted in place of a number |
| `base` | `master` |
| `source_repo` | `GaijinEntertainment/G-CVSNT` — where base and PRs are fetched from; the fork itself only supplies the CI tooling |

The `integrate` job fetches the base, merges each PR head with `--no-ff`, and stops at the
first conflict with an annotation naming the PR, the PRs already merged, and the conflicting
files. On success it overlays the CI tooling from the commit the workflow runs from (`ci/`,
`docker/`, `.github/`, `.gitattributes`, `.dockerignore`, and the two build fixes
`longfilenames.xml` and `genbuild.vcxproj`), exports the tree as a tarball and hands it to the
two test jobs, so both build exactly the same sources.

On `pull_request` the same pipeline runs on the PR merge commit with no merging step.

## What the test jobs run

**linux-server-tests** (`ubuntu-latest`):

1. `docker build --target build -f docker/Dockerfile` (Rocky 8, gcc-toolset-9, the production
   configure flags), then `ci/test.Dockerfile` adds python and diffutils on top.
2. `ci/run_linux_suites.sh` inside that image as uid 52 with a tmpfs work dir: starts
   `cvslockd`, checks `cvs --version` reports the expected build and that
   `lib/cvsnt/protocols/ext.so` exists, runs `testcvs/regress.py --libdir` and `testcvs.py`.
3. `authserver`, `cafs-server`, `cvslockd` images from the same build stage.
4. `ci/contour/up.sh`: compose up, a per-run blob secret (`BlobOTP` = `SECRET`), a fresh
   `cvs init -n` repository plus the `blobs/` directory, PAM set to `pam_permit`
   (localhost-only contour, no directory).
5. `ci/smoke_pserver.sh` with the Linux `cvs` from the same build against
   `:pserver:cvs:cvs@127.0.0.1:2401/cvs`.
6. `ci/contour/down.sh` always runs and removes the repository volume: the fixture is recreated
   for every run.

**windows-client** (`windows-2022`): `msbuild cvsnt.sln` Release|x64 with `v143`, then the
staged `Releasex64` tree (OpenSSL DLLs copied next to `cvs.exe`) is checked for the plugins the
suites depend on, `cvs.exe --version` must report the expected build, and `unit_tests`,
`regress.py --libdir Releasex64`, `testcvs.bat` run. The client tree
(`cvs.exe`, `cvsapi.dll`, `cvstools.dll`, OpenSSL DLLs, `protocols/`, `triggers/`) is uploaded as
artifact `cvsnt-<version>-win-x64`.

**Every case must execute.** `ci/check_suite_logs.sh` runs on both platforms and fails the job
when `regress.py` printed a `skipped` note (the `-ku` case does that when `ext` is missing and
still counts as passed), when the `-ku`, binary-by-content or binary round-trip cases did not
pass, or when `testcvs.py` did not reach its last scenario or printed a failure (`testcvs.py`
exits 0 even on failure).

### The `:pserver:` scenario (`ci/smoke_pserver.sh`)

Each mutation is followed by `update -dP` in a second working copy and a sha256 tree compare
of both copies:

import → checkout ×2 → add + commit → **binary** add + commit (blob push through
`cafs_server`) → modify + commit → binary second revision → remove + commit → binary remove +
commit → tag / log / status / history → checkout by tag → 400-file add + commit (crosses the
≥ 8 KiB output batching) → branch switch and back → an independent checkout equals the updated
copy.

After the import and after the mixed add, the working copy is also compared with the **source
files** (`.txt` modulo CR, binaries byte for byte). Two checkouts agreeing proves nothing when
the server stored the wrong bytes at import time. The imported binaries are 5000 and 200000
bytes with a `\0\xff` header under `.dat` names, so the client's automatic `-kB` path is what
runs.

`ROOT` can be overridden in the environment; the script takes the `cvs` binary and a work
directory. It is not idempotent, the repository must be fresh.

### The known import `-kB` defect

`cvs import` of a `-kB` file stores the 79-byte session blob reference (or the raw text body
for a server-forced kopt) as RCS text, so the checkout returns 79 bytes; `import.cpp` has no
blob handling. The smoke compares the first
checkout with the import source in `known-import-defect` mode: mismatches there are
`::warning::` annotations and the job stays green, while a mismatch after `add`/`commit` is
still an error. `ci/repro_import_kB.sh` (standalone reproduction) then runs as a guard that
**fails the job when the defect no longer reproduces**, with the instruction to remove the
warning mode from `ci/smoke_pserver.sh`; the check cannot silently outlive the bug.

## Versioning

`cvs --version` prints `3.5.24 ... Build <n>`; `<n>` is `CVSNT_PRODUCT_BUILD` from `build.h`.

What the tree did before this branch, verified on a Windows build on 2026-09-16: the
`genbuild` project's custom build step declared its output as `build.h.dummy`, a file nothing
creates, so MSBuild considered the step out of date on **every** build and `genbuild.exe`
rewrote `build.h` with `days_since_epoch(now) - 365*30` (the committed 9754 became 9762 that
day). The Linux/autotools path never runs genbuild and reads `build.h` as committed. So a
committed `build.h` pinned the Linux version only, and two platforms built from one commit on
different days disagreed. genbuild also honours an override file `d:\forcebuild.txt`, but that
is a hard-coded path on a drive a runner need not have, so it is not used here.

Two changes make the number a property of the source tree:

- `genbuild.vcxproj`: the command is now
  `if not exist "$(SolutionDir)build.h" "$(TargetPath)" "$(SolutionDir)build.h"`, so genbuild
  writes the file only when it is missing; a committed `build.h` wins on every platform. To get
  a fresh date-based number locally, delete `build.h` and build. The step also runs after
  `Link` (`CustomBuildAfterTargets`), so it can no longer run before `genbuild.exe` exists or
  race the parallel compile under `msbuild -m`.
- `ci/build_number.py` reproduces the same formula from the **committer date of HEAD** instead
  of "now": deterministic per commit, identical on every platform, monotonic across commits made
  on different days.

In the workflows:

- `test.yml`: the integrate job writes `build.h` into the exported tree that both test jobs
  build; both assert `cvs --version` reports that number.
- `release.yml`: the `version` job computes it once, every build job writes `build.h` from that
  output before building.
- Names: artifacts and images are `3.5.24.<build>-<sha7>` (release images additionally carry
  `3.5.24.<build>` and `latest`, moved only after every job succeeded, as in the original).
- The committed `build.h` still matters for anyone building from a plain clone. Bump it with
  `python3 ci/build_number.py --write cvsnt/cvsnt-2.5.05.3744/build.h` before tagging.

## Repository changes the CI depends on

- `docker/` and `.dockerignore`: Konstantin's in-repo server images (`ci-release`, `51e0086`),
  the same build as `buildtools/projects/it/cvsnt/servers/contrib/Dockerfile` plus
  `--disable-avx512`. The test contour and the release images both use it.
- `cvsnt/cvsnt-2.5.05.3744/longfilenames.xml`: the manifest `cvsnt.vcxproj` references but the
  repository never tracked; both earlier CI attempts generated it at build time.
- `genbuild/genbuild.vcxproj`: `<CustomBuildAfterTargets>Link</CustomBuildAfterTargets>`, and
  the step only writes `build.h` when it is missing (see Versioning). The step used to run
  before `Link`, so the first build on a clean tree always failed and the second passed;
  `release.yml`'s `|| msbuild` retry existed for this and is gone.
- `.gitattributes`: LF for `ci/`, `docker/`, `*.sh`, autotools inputs. A CRLF `configure.in`
  breaks `autoreconf`; a CRLF xinetd config makes xinetd load zero services silently.

## Known limitations

- **No `pam_sss`.** The contour authenticates with `pam_permit`; the directory path
  (SSSD → AD) is not exercised. Konstantin's `docker/sssd` sidecar exists but needs a directory.
- **No heavy-repository performance job.** The scenario uses small trees; a perf job needs a
  host with a copy of a large repository and a fixed baseline.
- **`unit_tests` runs on Windows only**; there is no `Makefile.am` entry for it.
- **`:ext:` client/server on Linux** is used by one regression case; the `:sserver:`/`:sspi:`
  protocols are not tested. The test containers run as uid 52 without a passwd entry, which
  is what exposed the `getpwuid()` crash fixed in `cvstools/unix/GlobalSettings.cpp`.
- **Packaging jobs in `release.yml`** (`deb`, `rpm`, `macos`) were written against packaging
  fixes on `ci-release` (`2328008..66423dd`, 11 files under `debian/`, `redhat/`, `osx/`) that
  this branch does not carry. They may fail until those commits are merged.

## To verify on the first run

Nothing below could be checked without a GitHub runner; each is asserted by the workflow, so a
wrong assumption fails loudly rather than silently.

- `windows-2022`: `msbuild cvsnt.sln` for all 58 projects (Konstantin built only
  `cvsnt.vcxproj` + `cafs_proxy.vcxproj`); that `cmd`'s `if not exist` in the genbuild custom
  step leaves the written `build.h` alone (asserted by the `Build <n>` check on `cvs.exe`);
  `genbuild.rc` still needing to be blanked;
  `cvs.exe` finding `protocols/` and `triggers/` next to itself without `-L` (what `testcvs.bat`
  relies on); location of `zlib.lib`/`zstd.lib` for `unit_tests` (searched, not hard-coded).
- `ubuntu-latest`: the default Docker builder sharing the `build` stage cache between the
  successive `docker build --target` calls (otherwise the job is slower, not wrong);
  `--network host` for the client container reaching the published `127.0.0.1` ports.
- Time: the Rocky 8 toolchain build has not been timed on a 2-vCPU runner; `timeout-minutes` is
  90 on both test jobs.
