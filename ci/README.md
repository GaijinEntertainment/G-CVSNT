# CI for G-CVSNT

Two GitHub Actions workflows and the scripts they run. Everything under `ci/` can also be run
by hand; nothing here needs credentials.

| Workflow | Trigger | What it does |
| --- | --- | --- |
| `.github/workflows/test.yml` | pull requests; `workflow_dispatch` with a PR list | builds base + PRs, runs every suite on Linux and Windows, runs the `:pserver:` scenario |
| `.github/workflows/release.yml` | push of a `v*` tag; `workflow_dispatch` | builds deb/rpm/macOS/Windows packages and server images, drafts a GitHub release |

## Running a PR set

**Actions → test → Run workflow**:

| Input | Meaning |
| --- | --- |
| `prs` | PR numbers and/or branch names, comma-separated, merged onto `base` **in this order**; empty = base only |
| `base` | Base branch, default `master` |
| `source_repo` | `owner/repo` to fetch `base` and `prs` from; defaults to this repository, override only to test against a different source |

Example: `prs: 28,29,30` with `base: master` merges those three PR heads onto `master` in order
and runs the full suite on the result; `integrate` stops at the first conflict, naming the item
and the conflicting files. `SOURCE_REPO_TOKEN` (read-only PAT) is only needed when `source_repo`
is overridden to a private repository.

## What it does

- **`integrate`** — the exact source tree the other jobs build: base + `prs`, or the PR's own
  merge commit. Overlays the CI tooling from the commit the workflow runs from, and writes a
  deterministic build number into `build.h`.
- **`linux-server-tests`** (`ubuntu-latest`) — builds the server, runs the suites, brings up a
  throwaway `:pserver:` contour, runs the smoke scenario.
- **`windows-client`** (`windows-2022`) — builds the client, runs `unit_tests`, `regress.py`,
  `testcvs.bat`, uploads the client tree as an artifact.

A failing suite does not stop its job: every later step still runs, so one broken case cannot
hide the rest of the result. Each step is gated on the step it actually needs — the suites gate
nothing, the contour gates the smoke scenario. `regress.log` and `testcvs.log` are uploaded as
the `suite-logs-*` artifact from both platforms, failed run or not, and the failing step still
marks the run red.

`ci/` scripts:

- `build_number.py` — build number from the commit's committer date.
- `check_suite_logs.sh` — fails the job if a required case was skipped or a suite didn't reach
  its last scenario (`testcvs.py` exits 0 even on failure, so this is the only thing that
  catches that).
- `run_linux_suites.sh` — runs the Linux suites; `cvslockd` must already be running (or every
  locking call hangs), and CVSNT refuses to commit as root, so they run as an unprivileged uid.
- `smoke_pserver.sh` — the `:pserver:` functional scenario, below.
- `repro_import_kB.sh` — standalone reproduction of the known `import -kB` defect, below.
- `contour/` — a throwaway `cvslockd` + `authserver` + `cafs-server` stack; `pam-cvsnt` is
  `pam_permit.so` only (no directory service in CI, localhost-only); the blob secret is
  generated per run and masked in the log.
- `test.Dockerfile` — the runtime stage of `docker/Dockerfile` plus `python3`/`diffutils` and `testcvs/`; the `cvs` user is homed at `/work`, the tmpfs every step mounts.

## The `:pserver:` scenario

import → checkout ×2 → binary add + commit (blob push through `cafs_server`) → modify → binary
modify → remove → binary remove → tag / log / status / history → checkout by tag → 400-file
commit → branch switch. Each mutation is followed by `update -dP` in a second working copy and
a tree compare between the two.

Then, in working copies of its own so the tree compare above keeps its meaning: `-kb` and `-kB`
side by side through a **fresh checkout**, a second revision of each, `update -r 1.1` and back
with `update -A`, a sticky tag on a binary, remove followed by a checkout of an older revision,
and a branch/merge round trip.

Those last scenarios are here because their local-mode counterparts in `testcvs.py` and
`regress.py` cannot answer for the assembled stack: local mode never points the blob store at the
repository (`caddressed_fs::set_root` is called on the server path only), so it exercises a code
path no client here uses. The local suites still carry the breadth of commands; this carries the
same ground over the protocol people actually work on.

19 scenarios in all. Each one is numbered `[k/19]` as it starts and the run ends with how many
passed; the total is counted out of the script itself, so adding a scenario cannot leave it
behind. `set -e` aborts on the first failed check, so an `EXIT` trap names the scenario that
failed and how many passed before it — otherwise the log just stops on whatever command failed.

Two non-obvious properties. After the import and after the mixed add, the working copy is also
compared against the **import source**, not just the second checkout — two checkouts agreeing
proves nothing if the server stored the wrong bytes at import time. And the binary scenarios
compare after a *fresh checkout*, not an update, because an update can be satisfied out of a
working copy that already holds the bytes.

## The known `import -kB` defect

`cvs import` of a `-kB` file stores the 79-byte session blob reference as RCS text instead of
the file content. The smoke scenario downgrades that one mismatch to a `::warning::` instead of
failing the job. `repro_import_kB.sh` runs separately as a guard that **fails the job the day
the defect stops reproducing**, telling the maintainer to remove the warning mode from
`smoke_pserver.sh` — the check cannot silently outlive the bug. It fails just as loudly when it
cannot judge: a client that crashes, or an import that never produces a checkout, is not evidence
that the defect is still there, and is reported as inconclusive rather than as the known defect.

## Versioning

The build number (`CVSNT_PRODUCT_BUILD` in `build.h`, printed by `cvs --version`) comes from the
commit's committer date, not the build time, so Linux and Windows agree for the same commit.

## Prerequisites

- **No suites on bare `master` yet** — `testcvs/regress.py` and `testcvs/unit/` arrive with the
  audit branches (first `audit/02-analysis-reports-and-fixes`).
- **`audit/02-analysis-reports-and-fixes` does not merge onto current `master`** — it predates
  #38 and conflicts with it in `README.md` and `docs/README.md`; a hand resolution is needed
  before `prs=audit/02-...` can be used against `master` directly.

## Known limitations

- No macOS job.
- `unit_tests` runs on Windows only; there is no Linux/`Makefile.am` entry for it.
