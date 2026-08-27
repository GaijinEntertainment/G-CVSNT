# G-CVSNT documentation

G-CVSNT is Gaijin Entertainment's fork of CVSNT, adapted for repositories that hold hundreds of
thousands of large binary files.

| Document | What it covers |
| --- | --- |
| [01-overview.md](01-overview.md) | What this fork is, why it exists, what it changes and what it leaves alone |
| [02-architecture.md](02-architecture.md) | Processes, connection methods, library layers, request flow |
| [03-content-addressed-storage.md](03-content-addressed-storage.md) | The blob store: hashing, on-disk format, dedup, trust model |
| [04-protocols.md](04-protocols.md) | CVS protocol extensions and the `blob_push` protocol |
| [05-repository-layout.md](05-repository-layout.md) | What is on disk, server side and client side |
| [06-server-operations.md](06-server-operations.md) | Running the CVS server, lock server, blob server and proxy; maintenance tools |
| [07-client-usage.md](07-client-usage.md) | Commands and options that matter in this fork |
| [08-source-map.md](08-source-map.md) | Directory-by-directory guide to the source tree |
| [../HOWTOBUILD.md](../HOWTOBUILD.md) | Building on Windows, Linux and macOS |

## The one-paragraph version

Classic CVS stores every revision of a binary file inside its `,v` file and rewrites that whole file
to add a tag. For a game repository this is fatal. G-CVSNT adds a keyword mode `-kB` under which a
file's content is stored once, keyed by its BLAKE3 hash, in a separate content-addressed store
(CAFS), while the `,v` file keeps only a 71-byte reference per revision. Blob bytes move over their
own TCP protocol that can be served by a dumb caching proxy near the client. Everything else — the
command set, the RCS format, ACLs, triggers — is unchanged.

## Beyond the reference documentation

| Document | What it covers |
| --- | --- |
| [../known_issues.md](../known_issues.md) | The defects still open, ordered by severity, and — more usefully — the ones where the obvious fix is wrong and why |
| [../suggested_optimizations.md](../suggested_optimizations.md) | Why `update` and `tag` scale with file count rather than data volume, and a ranked plan |
| [../cvsnt/cvsnt-2.5.05.3744/testcvs/README.md](../cvsnt/cvsnt-2.5.05.3744/testcvs/README.md) | The three test suites and what each needs to run |

## Analysis reports

The `_reports/` directory at the repository root holds the individual analysis findings behind the
documents above: 87 bug findings (`BUG-*.md`), the performance analyses of the update and tag paths
(`PERF-*.md`), the Windows build write-up (`BUILD-*.md`), and two design plans (`PLAN-*.md`).

Each bug file is self-contained: the offending code, why it is wrong, a concrete failure scenario, a
suggested fix, and a record of what was checked that could have made it a false positive.
