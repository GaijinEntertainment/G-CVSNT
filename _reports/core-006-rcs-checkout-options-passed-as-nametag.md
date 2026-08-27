# Keyword-expansion options passed in the nametag slot of RCS_checkout (remove_file / import delete path)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/commit.cpp (and same pattern in src/import.cpp)
- **Line(s):** commit.cpp 1848-1856; import.cpp 1909-1916
- **Severity:** low
- **Confidence:** high
- **Category:** logic / typo

## Code
commit.cpp, `remove_file()`:
```cpp
	char *real_rev = RCS_branch_head(finfo->rcs,rev?corev:NULL);
	options = RCS_getexpand(finfo->rcs, real_rev);

    /* check something out.  Generally this is the head. ... */
    bool is_ref = false;
    retcode = RCS_checkout (finfo->rcs, finfo->file, rev ? corev : NULL,
			    options, (char *) NULL, RUN_TTY,      // <-- options in the *nametag* slot
			    (RCSCHECKOUTPROC) NULL, (void *) NULL, NULL, &is_ref);
```

The declaration (src/rcs.h:358, definition rcs_checkin.cpp:106):
```cpp
int RCS_checkout (RCSNode *rcs, const char *workfile, const char *rev, const char *nametag,
                  const char *options, const char *sout, RCSCHECKOUTPROC pfn, void *callerdat,
                  mode_t *pmode, bool *is_ref);
```

import.cpp has the identical copy-pasted call at 1914-1916.

## Why this is a bug
The 4th parameter of `RCS_checkout` is `nametag` (the symbolic tag used to expand `$Name$`), the 5th is `options` (the `-k` expansion mode). Upstream passed `(char*) NULL, (char*) NULL` here. The Gaijin change fetches the per-revision expansion mode via `RCS_getexpand()` — clearly intending it to control checkout expansion — but passes it as `nametag` and leaves `options` NULL.

Consequences when committing a file removal (`cvs remove` + `cvs ci`) or when `import` marks a file dead:
1. The intended explicit expansion mode is not applied. (It mostly self-heals because `RCS_checkout` falls back to `RCS_getexpand(rcs, rev)` internally when options is NULL, but for a branch removal `real_rev` and the checked-out rev can differ, so the wrong revision's kopt can be used.)
2. The kopt string (e.g. `kv`, `B`, `z9`) is used as the *tag name* for `$Name$` keyword expansion, so the temporary working file — whose content becomes the base for the dead revision check-in — gets `$Name: kv $` (or similar) baked into it for any file using the `$Name$` keyword.

## Suggested fix
Swap the arguments so the expansion mode lands in the options slot:
```cpp
    retcode = RCS_checkout (finfo->rcs, finfo->file, rev ? corev : NULL,
			    (char *) NULL, options, RUN_TTY,
			    (RCSCHECKOUTPROC) NULL, (void *) NULL, NULL, &is_ref);
```
(same fix in import.cpp:1914).
