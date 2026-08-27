# checkout_file frees RCS delta-owned version string when resurrecting during a join

- **File:** cvsnt/cvsnt-2.5.05.3744/src/update.cpp
- **Line(s):** 1630-1641 (aliasing), 1812-1821 (the free)
- **Severity:** medium
- **Confidence:** medium
- **Category:** memory

## Code
```cpp
	file_is_dead = RCS_isdead (vers_ts->srcfile, vers_ts->vn_rcs);
	if(file_is_dead && resurrect)
	{
		Node *p = findnode(finfo->rcs->versions, vers_ts->vn_rcs);
		RCSVers *vers = (RCSVers *) p->data;

		file_is_dead = 0;
		adding = 1;
		vn_rcs = vers->version;          // <-- aliases string owned by the RCSVers node
	}
	else
		vn_rcs = vers_ts->vn_rcs;
...
		/* fix up the vers structure, in case it is used by join */
		if (join_rev1)
		{
			TRACE(3,"fix up the vers structure, in case it is used by join.");
			if (vers_ts->vn_user != NULL)
				xfree (vers_ts->vn_user);
			if (vn_rcs != NULL)
				xfree (vn_rcs);          // <-- frees RCSVers::version in the resurrect case
			vers_ts->vn_user = xstrdup (xvers_ts->vn_rcs);
			vers_ts->vn_rcs = xstrdup (xvers_ts->vn_rcs);
		}
```

## Why this is a bug
Upstream CVSNT freed `vers_ts->vn_rcs` here (a string owned by the `Vers_TS`), then replaced it. The Gaijin resurrect feature introduced the local alias `vn_rcs`, which in the `file_is_dead && resurrect` case points at `RCSVers::version` — the revision-number string owned by the in-memory RCS delta list (`finfo->rcs->versions`). Freeing it:

1. Leaves the RCS node with a dangling `version` pointer. The RCS structure is used afterwards (e.g. by `join_file` via `vers->srcfile`, by `RCS_*` lookups, and finally by `freercsnode`, which frees `RCSVers::version` again) → use-after-free / double free.
2. Leaks the real `vers_ts->vn_rcs` string, which is overwritten two lines later without being freed.

Trigger path: `checkout_file` called with `resurrect != 0` while `join_rev1` is set and the requested revision is dead. `resurrect=1` callers are add.cpp:607 and update.cpp:897 (`resurrect = is_rcs`, i.e. the `cvsrcs` emulation, where `rcs_update_fileproc` can set `join_rev1` from its `xjoin_rev1` argument). The combination is narrow, which is why severity is medium rather than high, but when hit it corrupts the shared RCS structure and typically crashes or double-frees at `freercsnode` time.

## Suggested fix
Free the `Vers_TS`-owned string explicitly and never the alias:
```cpp
if (vers_ts->vn_rcs != NULL)
    xfree (vers_ts->vn_rcs);
```
(and keep `vn_rcs` purely as a read-only alias for the checkout revision).
