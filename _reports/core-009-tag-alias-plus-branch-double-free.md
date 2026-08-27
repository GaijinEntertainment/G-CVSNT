# cvs tag/rtag with -A -b double-frees the revision string (rev aliases version)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/tag.cpp
- **Line(s):** rtag_fileproc 740/767 with 816-830; tag_fileproc 1131 with 1188-1198 and 1214-1218
- **Severity:** medium
- **Confidence:** high
- **Category:** memory

## Code
rtag_fileproc:
```cpp
    rev = (!alias_branch && branch_mode) ? RCS_magicrev (rcsfile, version) : version;
    ...
    retcode = RCS_settag(rcsfile, symtag, rev, current_date);
    ...
    if (branch_mode)
		xfree (rev);          // frees; sets only `rev` to NULL
    xfree (version);          // <-- same pointer freed again when alias_branch && branch_mode
```
tag_fileproc has the identical pattern:
```cpp
    rev = (!alias_branch && branch_mode) ? RCS_magicrev (vers->srcfile, version) : version;
    ...
    if (branch_mode)
	xfree (rev);
    ...
    if (nversion != NULL)
    {
        xfree (nversion);     // <-- version == nversion on the -r path: double free
    }
    freevers_ts (&vers);      // <-- or frees vers->vn_user again on the no -r path
```

## Why this is a bug
`rev` receives a *fresh* allocation from `RCS_magicrev` only when `!alias_branch && branch_mode`. When **both** `-A` (alias_branch) and `-b` (branch_mode) are given — a combination the option parser does not reject (`cvstag()` only rejects mixing `-M` with `-A`) — `rev` is a mere alias of `version`, yet the cleanup code still executes `if (branch_mode) xfree (rev);` *and* separately frees `version`/`nversion` (or lets `freevers_ts` free `vers->vn_user`, which `version` aliases in `tag_fileproc` when no `-r` was given). `xfree` NULLs only the variable it is passed, so the second free operates on an already-freed pointer:

- `cvs rtag -A -b -r <branch> <tag> <module>`: `xfree(rev)` then `xfree(version)` → double free per file.
- `cvs tag -A -b -r <branch> <tag>`: `xfree(rev)` then `xfree(nversion)` → double free per file.
- `cvs tag -A -b <tag>` (no `-r`; the "-A requires -r" rule is only documented, never enforced): `xfree(rev)` frees `vers->vn_user` in place; `freevers_ts(&vers)` then frees it again.

Heap corruption in the server process (these fileprocs run server-side), repeated once per file being tagged.

## Suggested fix
Track ownership explicitly, e.g.:
```cpp
    int rev_allocated = (!alias_branch && branch_mode);
    rev = rev_allocated ? RCS_magicrev (rcsfile, version) : version;
    ...
    if (rev_allocated)
        xfree (rev);
```
or reject the `-A -b` combination in `cvstag()` the way `-A -M` is rejected.
