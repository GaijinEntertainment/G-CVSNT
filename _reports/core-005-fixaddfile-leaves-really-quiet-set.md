# fixaddfile fails to restore really_quiet (and leaks) when the RCS file is unparseable

- **File:** cvsnt/cvsnt-2.5.05.3744/src/commit.cpp
- **Line(s):** 2038-2061
- **Severity:** low
- **Confidence:** high
- **Category:** logic / error-handling

## Code
```cpp
void fixaddfile (const char *file, const char *repository)
{
    RCSNode *rcsfile;
    char *rcs;
    int save_really_quiet;

    rcs = locate_rcs (file, repository);
	if(isfile(rcs))
	{
		save_really_quiet = really_quiet;
		really_quiet = 1;
		if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
		{
			if (unlink_file (rcs) < 0)
				error (0, errno, "cannot remove %s", rcs);
		}                              // <-- really_quiet not restored, rcs not freed
		else
		{
			freercsnode (&rcsfile);
			really_quiet = save_really_quiet;
			xfree (rcs);
		}
	}                                  // <-- rcs also leaks when !isfile(rcs)
}
```

## Why this is a bug
Upstream CVS/CVSNT restores `really_quiet` and frees `rcs` unconditionally after the if/else. In this refactor the restore and free were moved *into the else branch only*. `fixaddfile` is called on the failure paths of `cvs add`/`cvs commit` of added files (`checkaddfile` failure, `finaladd` failure). If the half-created RCS file cannot be parsed (exactly the situation `fixaddfile` exists to clean up), the global `really_quiet` remains 1 for the remainder of the process. In server mode a single failed add then silently suppresses the normal output (`M`/`U` lines, "new revision" messages, etc.) for every subsequent file in the same commit/session, making the client appear to succeed with no feedback. `rcs` also leaks on both non-else paths.

## Suggested fix
Move `really_quiet = save_really_quiet;` and `xfree (rcs);` out of the else branch so they run on all paths (as in upstream):
```cpp
    if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
    {
        if (unlink_file (rcs) < 0)
            error (0, errno, "cannot remove %s", rcs);
    }
    else
        freercsnode (&rcsfile);
    really_quiet = save_really_quiet;
    xfree (rcs);
```
