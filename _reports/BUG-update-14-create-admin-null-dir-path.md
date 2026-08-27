---
id: BUG-update-14
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/create_adm.cpp
line: 166
severity: low
category: typo
verdict: PLAUSIBLE
fix_size_loc: 2
behavior_change: no
---

# `Create_Admin()`'s documented `dir == NULL` path is doubly broken: `strlen(NULL)` at entry and `CVSADM_ENT` written where `CVSADM_ENTEXT` is meant

## Summary
`Create_Admin()` carries four `if (dir != NULL) ... else ...` pairs that exist specifically
to support `dir == NULL` ("operate in the current directory"). That path can never work:
the very first statement calls `strlen (dir)` before the NULL test, and — if that were
fixed — the `Entries.Extra` branch would create `CVS/Entries` a second time because the
`else` arm names `CVSADM_ENT` instead of `CVSADM_ENTEXT`.

## Code
```cpp
/* src/create_adm.cpp:38-42 — strlen before the NULL test */
    tmp = (char*)xmalloc (strlen (dir) + 100);      /* 38 <-- dereferences dir */
    if (dir != NULL)                                /* 39 <-- ...then checks it for NULL */
		sprintf (tmp, "%s/%s", dir, CVSADM);
    else
		strcpy (tmp, CVSADM);
```

```cpp
/* src/create_adm.cpp:162-166 — the copy-paste slip */
	/* CVS/Entries.Extra */
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENTEXT);
    else
	(void) strcpy (tmp, CVSADM_ENT);        /* 166 <-- should be CVSADM_ENTEXT */
    fout = CVS_FOPEN (tmp, "w+");
```

Compare the immediately preceding `CVS/Entries` block (create_adm.cpp:136-140), which is
consistent:
```cpp
	/* CVS/Entries */
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENT);
    else
	(void) strcpy (tmp, CVSADM_ENT);
```

`cvs.h:139-140` define them as distinct paths: `CVSADM_ENT "CVS/Entries"` and
`CVSADM_ENTEXT "CVS/Entries.Extra"`.

## Why it is a bug
Two independent defects on the same code path:

1. `strlen (dir)` at line 38 runs unconditionally. Any `Create_Admin (NULL, ...)` call
   segfaults there, before reaching the `if (dir != NULL)` on the very next line. The
   presence of that test one line later is proof the NULL case is meant to be supported.
2. Line 166 names the wrong constant. Were defect 1 fixed, `Create_Admin (NULL, ...)`
   would open `CVS/Entries` with `"w+"` for a second time (truncating the file it created
   twelve lines earlier) and would never create `CVS/Entries.Extra` at all — leaving a
   working directory that `Entries_Open()` reads without the merge-tag / edit-revision /
   md5 side-car data (entries.cpp:864-880), silently dropping `merge_from_tag_1/2`,
   `edit_revision`, `edit_tag`, `edit_bugid` and `md5` for every file in that directory.

## Failure scenario
No caller currently passes `dir == NULL` — `grep -rn "Create_Admin" src/` shows every call
site passes `"."`, `dir`, `p`, or `cwd` (add.cpp:316 and add.cpp:931, checkout.cpp:636/1116/1134,
client.cpp:965/1102/1151, import.cpp:637, modules.cpp:596, update.cpp:1109/1286) — so the
branch is dead today. This is why the verdict is PLAUSIBLE rather than CONFIRMED: the
defects are unambiguous but currently unreachable.

The failure appears the moment a caller uses the documented NULL form, which is easy to do
because the signature and the four `if (dir != NULL)` guards advertise it:
`Create_Admin (NULL, ".", repo, NULL, NULL, 0, 0)` crashes at create_adm.cpp:38; with that
one line fixed, the resulting working directory silently lacks `CVS/Entries.Extra`, and
the first `cvs update` in it loses all Entries.Extra metadata.

## Suggested fix
```cpp
    tmp = (char*)xmalloc ((dir ? strlen (dir) : 0) + 100);
    if (dir != NULL)
		sprintf (tmp, "%s/%s", dir, CVSADM);
    else
		strcpy (tmp, CVSADM);
```
```cpp
	/* CVS/Entries.Extra */
    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_ENTEXT);
    else
	(void) strcpy (tmp, CVSADM_ENTEXT);
```

## Refutation attempt
* Could `CVSADM_ENT` and `CVSADM_ENTEXT` be the same string? No — `cvs.h:139-140`:
  `"CVS/Entries"` vs `"CVS/Entries.Extra"`.
* Could writing `CVS/Entries` twice be intentional (e.g. to guarantee truncation)? No —
  the block is introduced by the comment `/* CVS/Entries.Extra */` and every error message
  inside it reports `CVSADM_ENTEXT` (create_adm.cpp:171, 178), so only the `strcpy` target
  is wrong.
* Is `dir == NULL` genuinely intended, or vestigial? The function contains four separate
  `if (dir != NULL) ... else ...` pairs (create_adm.cpp:39, 78, 136, 163) plus a
  `PATCH_NULL(dir)` in its `TRACE` (create_adm.cpp:32-34), and passes `dir` straight through
  to `Create_Root (dir, ...)` and `WriteTag (dir, ...)`, both of which explicitly accept
  NULL. The support is deliberate; only line 38 breaks it.
* Does something else create `CVS/Entries.Extra` on the client if this misses it?
  `grep -n "CVSADM_ENTEXT" src/*.cpp` gives create_adm.cpp:164 (this site),
  entries.cpp:865 and 870 (opened for reading), entries.cpp:219 (renamed into place from
  `CVSADM_ENTEXTBAK` by `write_entries`, which only runs when the file is already being
  rewritten), and server.cpp:837/1489 (server side only). Nothing else creates it in a
  fresh client working directory.
