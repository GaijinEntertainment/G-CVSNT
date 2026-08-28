---
id: BUG-blob-21
area: blob/CAFS subsystem, local (non-server) mode
file: cvsnt/cvsnt-2.5.05.3744/src/rcs_checkin.cpp
line: 512
severity: critical
category: correctness
verdict: CONFIRMED
fix_size_loc: 20
behavior_change: yes
status: partially fixed - parts 1 and the missing-blobs-dir case; parts 2 and 3 remain open
---

# In local mode, committing a second revision of a binary file either aborts or silently destroys the content

## Summary
Three separate behaviours combine into silent, permanent data loss on any repository used without a
server:

1. `RCS_checkin` rewrites every `b` in the keyword options to `B` before parsing them
   (`src/rcs_checkin.cpp:512`), so **every** commit of a `-kb` file is forced onto the
   content-addressed blob path, whatever the file was registered as.
2. `caddressed_fs::set_root()` is called from exactly one place in the whole tree —
   `src/server.cpp:5375` — so in local mode the blob root is never configured and keeps its default
   `"./blobs/"` (`ca_blobs_fs/src/content_addressed_fs.cpp:31`), which is relative to the **current
   working directory**, i.e. the user's working copy.
3. `RCS_read_binary_rev_data` cannot report failure, so a later checkout that cannot find the blob
   reports success and writes a zero-length file (tracked separately as `BUG-server-12`).

So a local `cvs commit` of a binary file either **aborts** — if the working directory has no
`blobs/` subdirectory — or **appears to succeed** while writing the repository's content into the
user's working copy, leaving the `,v` referencing a blob that is not in the repository. A later
checkout then silently produces an empty file.

## Reproduction

Verified end to end against a build of this tree. No server, no lock server, no CAFS — a plain
local repository, which is what `cvs -d /path/to/repo` gives you.

```console
$ cvs -d $REPO init -n
$ printf 'BINARY-v1-\0\1\2\377' > b.dat
$ cvs -d $REPO import -m bin -kb mb VENDOR REL0      # fine
$ cvs -d $REPO checkout mb                           # fine
$ printf 'BINARY-v2-\0\1\2\377\376' > mb/b.dat
$ cd mb && cvs -d $REPO commit -m second
Checking in b.dat;
$REPO/mb/b.dat,v  <--  b.dat
cvs [commit aborted]: Couldn't write blob of $REPO/mb/b.dat,v: No such file or directory
```

That is the *good* outcome. Now create the directory the code is implicitly looking for:

```console
$ mkdir blobs                                        # in the working copy
$ cvs -d $REPO commit -m second
Checking in b.dat;
new revision: 1.2; previous revision: 1.1
done
$ find blobs -type f
blobs/7b/4b/7b4b68681a126173f62655198ee59a9d6231545fc751c804b12acc249c4b41da
```

The commit succeeded and the blob was written **into the working copy**, not the repository. The
`,v` records the new revision as a blob reference:

```console
$ grep kopt $REPO/mb/b.dat,v
kopt	B;      <- revision 1.2, rewritten from b to B by rcs_checkin.cpp:512
kopt	b;
kopt	b;
```

Check out somewhere else:

```console
$ cd /elsewhere && cvs -d $REPO checkout mb
cvs checkout: Updating mb
U mb/b.dat
$ ls -l mb/b.dat
-rw-r--r-- 1 user user 0 ... mb/b.dat
```

`U` — reported as a successful update — and the file is **zero bytes**. The committed content is
gone. Deleting the working copy that happened to hold `blobs/` destroys it permanently.

## Code

The forced conversion:
```cpp
// src/rcs_checkin.cpp:512-515
    if (options)
      while (char*s = strstr(const_cast<char*>(options), "b"))//on checkin do not allow old binary files
        *s = 'B';
	RCS_get_kflags(options, true, kf);
```

This is deliberate — the comment says so — but it is unconditional, and it mutates the caller's
string through a `const_cast`. Any `-kb` file in any existing repository becomes `-kB` on its next
commit.

The blob path is then taken because `KFLAG_BINARY_DELTA` is now set:
```cpp
// src/rcs_checkin.cpp:942-946
        if(kf.flags&KFLAG_BINARY_DELTA)
        {
          RCS_write_binary_rev_data(rcs->path, dtext->text, dtext->len, kf.flags&KFLAG_COMPRESS_DELTA, true);
          bufsize = dtext->len+1;
        }
```

And the root was never set:
```cpp
// ca_blobs_fs/src/content_addressed_fs.cpp:29-32
static struct context
{
  std::string root_path = "./" BLOBS_SUB_FOLDER "/";
} def_ctx;
```
```console
$ grep -rn 'set_root' src/*.cpp
src/server.cpp:5375:              caddressed_fs::set_root(caddressed_fs::get_default_ctx(), current_parsed_root->directory);
```

One call site, inside the server path only.

## Why it is a bug
Whatever the merits of forcing binaries onto blob storage, a client operating on a local repository
has no blob store, and the code does not notice. Two independent failures follow:

* **The blob root defaults to a relative path.** `"./blobs/"` is meaningless for a repository — it
  resolves against wherever the user happened to `cd`. Even in the "successful" case the content is
  written outside the repository, so the repository is left referencing a blob it does not contain.
* **Nothing detects the resulting dangling reference.** The commit reports success, and the checkout
  reports `U` while producing an empty file, because `RCS_read_binary_rev_data` has no way to
  signal failure (`BUG-server-12`).

The combination is worse than either part. An abort is recoverable; a commit that reports success
and loses the data is not.

## Failure scenario
Any use of a local repository with binary files. `cvs -d /srv/cvs commit` on a machine where the
working directory happens to contain a `blobs/` directory — for example because a previous aborted
commit prompted someone to create one, or because the tree legitimately has a directory of that
name — commits binary content into the working copy. The developer sees `new revision: 1.2; done`,
deletes the working copy a week later, and the revision is gone. A fresh checkout yields an empty
file with no error, so the loss is not noticed until the file is opened.

The `testcvs/regress.py` case "binary file survives a commit/checkout round trip byte for byte"
does not catch it because it only *imports* — `import` does not go through `RCS_checkin`, so the
`b`→`B` rewrite never fires.

## Status

Parts of this are fixed on this branch:

* the blob root is now set for local mode in `main.cpp`, next to `parse_config`, mirroring the
  server's per-request call — so local commits write into `<repos>/blobs/`;
* `RCS_write_binary_rev_data_blob` now creates the `blobs/` directory before pushing, so a
  repository that has never held a blob works instead of aborting (this also covers the server's
  old-client checkin path, where a fresh `cvs init` repository had the same gap).

Verified by the regression case "second commit of a binary file round trips byte for byte", which
fails against the previous build (commit aborted) and passes now, including a byte-exact fresh
checkout and `update -r` back to the first revision.

Still open: the default root should fail loudly rather than silently resolving to a relative path
(part 2 below), and the read path still cannot report a missing blob (`BUG-server-12`, part 3) — a
repository already poisoned by the old behaviour still checks out empty files without an error.

## Suggested fix
Three parts, smallest first:

1. **Set the blob root in local mode.** Call `caddressed_fs::set_root(get_default_ctx(),
   current_parsed_root->directory)` on the local path as well, alongside the existing server-mode
   call. That alone turns the data loss into correct behaviour for a local repository, since the
   blob then lands in `<repos>/blobs/`.
2. **Fail loudly instead of defaulting to a relative path.** `"./blobs/"` is never a correct
   repository blob root. Make `get_default_ctx()` start unset and have `push`/`pull` refuse with a
   clear diagnostic if no root was configured, rather than silently resolving against the process
   working directory.
3. **Make the read path able to fail** (`BUG-server-12`), so a dangling reference is an error at
   checkout rather than an empty file.

Fixing only (1) removes the data loss for the common case; (2) and (3) turn the remaining cases from
silent corruption into diagnostics.

Whether the unconditional `b`→`B` rewrite at `:512` should stay is a policy question for the
maintainers, but note that it silently migrates existing `-kb` history in any repository this build
touches, and that the `const_cast` write into the caller's `options` string is a separate hazard.

## Refutation attempt
Checked whether the file really was `-kb` and not `-kB` — `CVS/Entries` records `/b.dat/1.1.1.1/.../-kb/`
and the `,v` shows `kopt b` for the imported revisions, so the conversion happens at commit, not at
import. Checked whether some other code sets the blob root for local mode — `grep -rn 'set_root'`
over `src/` returns the single server-mode call. Checked whether the empty checkout might be a
working-copy artefact rather than real loss — the checkout was done into a *different* directory
with no `blobs/`, and the blob exists only under the original working copy, so the repository
genuinely cannot serve the revision. Checked whether this is already covered by `BUG-server-12` —
that finding is about the read path reporting success on a failed pull; it does not identify why the
blob is missing in the first place, which is what this report adds. The finding stands.
