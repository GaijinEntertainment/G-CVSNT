---
id: BUG-update-16
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/ignore.cpp
line: 276
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: yes
---

# `ign_add()`: the temporary-reset `else if` is attached to the wrong `if`, so a lone `!` in `.cvsignore` does nothing and any `!xxx` token wipes the ignore list

## Summary
The "temporarily reset the ignore list" branch is written as an `else` of the *outer*
`if ((ptr[0]=='!' || ptr[0]=='*') && !ptr[1])` instead of as an `else` of the inner
`if (!hold)`. The branch therefore fires on the wrong condition: it never runs for the lone
`!` it was written for, and it runs for every token that merely *starts* with `!`.

## Code
```cpp
/* src/ignore.cpp:256-294 */
		if((ptr[0]=='!' || ptr[0]=='*') && !ptr[1])          /* 256 */
		{
		    if (!hold)                                       /* 258 */
		    {
				/* permanently reset the ignore list */
				int i;

				for (i = 0; i < ign_count; i++)
					xfree (ign_list[i]);
				ign_count = 0;
				ign_list[0] = NULL;

				/* if we are doing a '!', continue; otherwise add the '*' */
				if (ptr[0] == '!')
				{
					ign_inhibit_server = 1;
					continue;
				}
		    }
		}                                                    /* 275 closes the OUTER if */
	    else if (ptr[0] == '!')                              /* 276 <-- else of line 256, should be else of line 258 */
	    {
			/* temporarily reset the ignore list */
			int i;

			if (ign_hold >= 0)
			{
				for (i = ign_hold; i < ign_count; i++)
				xfree (ign_list[i]);
				ign_hold = -1;
			}
			s_ign_list = (const char **) xmalloc (ign_count * sizeof (char *));
			for (i = 0; i < ign_count; i++)
				s_ign_list[i] = ign_list[i];
			s_ign_count = ign_count;
			ign_count = 0;
			ign_list[0] = NULL;
			continue;
	    }

		/* If we have used up all the space, add some more */
		...
		ign_list[ign_count++] = ptr;
```

## Why it is a bug
Two wrong behaviours follow directly from the misattached `else`:

**(a) `hold != 0` with a lone `!` does nothing at all.** Line 256 is true, line 258 is false,
the outer `if` body ends, the `else if` at 276 is skipped (its `if` was taken), and control
falls through to line 296 which appends `"!"` to `ign_list` as an ordinary glob pattern.
The list is not reset and, from then on, CVS ignores files literally named `!`.

**(b) Any token starting with `!` but longer than one character triggers the temporary
reset.** For `!foo`, line 256 is false (`ptr[1]` is `'f'`), so the `else if` at 276 is taken:
the entire ignore list is moved into `s_ign_list`, `ign_count` is zeroed, and the token is
dropped with `continue`. Nothing is ignored until the next `ign_add_file()` restores the
saved list.

That the branch is meant to be the `hold` counterpart of the `!hold` branch is not a guess —
the save/restore machinery only makes sense that way:

```cpp
/* src/ignore.cpp:149-158, top of ign_add_file() */
    /* restore the saved list (if any) */
    if (s_ign_list != NULL)
    {
	int i;
	for (i = 0; i < s_ign_count; i++)
	    ign_list[i] = s_ign_list[i];
	ign_count = s_ign_count;
	...
    }
```
`ign_add_file()` restores `s_ign_list` at the start of each load. Saving the list is only
meaningful for a *temporary* (`hold`) reset; a permanent reset (`!hold`, line 258) must not
be undone, and indeed that branch frees the entries outright instead of saving them.

The file's own header states the intended contract (ignore.cpp:20-23):
```
 *	"!" may be included any time to reset the list (i.e. ignore nothing);
```

`hold != 0` is exactly the per-directory `.cvsignore` case: `ignore_files()` calls
`ign_add_file (CVSDOTIGNORE, 1)` (ignore.cpp:412) and `import.cpp:551` does the same; both
funnel each line into `ign_add (line, hold)` (ignore.cpp:191).

## Failure scenario
**(a)** A working directory containing a `.cvsignore` whose only line is `!`
(the documented way to say "ignore nothing here" — e.g. a directory of `*.o` fixtures that
must show up as unknown files):

```
$ echo '!' > testdata/.cvsignore
$ touch testdata/junk.o testdata/core
$ cvs update
```
Expected: `? testdata/junk.o` and `? testdata/core`, because the ignore list was reset.
Actual: `ign_add_file("...", 1)` -> `ign_add("!", 1)` -> line 256 true, line 258 false ->
`"!"` is appended to `ign_list` -> `*.o` and `core` are still in the default list
(ignore.cpp:35-41) and both files stay silently hidden. The user's `.cvsignore` is inert,
and the only thing newly ignored is a file named `!`.

**(b)** Any of `cvs update -I '!foo'`, a `CVSROOT/cvsignore` line `!foo`, or a `.cvsignore`
line `!foo` (a natural thing to write, since that is gitignore's negation syntax): line 276
fires, `ign_count` is set to 0, and every remaining file in the directory is reported as
`? name` until the list is restored.

## Suggested fix
Move the `else` inside, as the `hold` counterpart of the `!hold` branch:

```cpp
		if((ptr[0]=='!' || ptr[0]=='*') && !ptr[1])
		{
		    if (!hold)
		    {
				/* permanently reset the ignore list */
				...
				if (ptr[0] == '!')
				{
					ign_inhibit_server = 1;
					continue;
				}
		    }
		    else if (ptr[0] == '!')
		    {
			/* temporarily reset the ignore list */
			...
			continue;
		    }
		}
```

## Refutation attempt
* Am I reading the braces right? Line 275 is the closing brace of the block opened at 257
  (the outer `if`); the inner `if (!hold)` block opened at 259 closes at 274. So the
  `else if` at 276 binds to the `if` at 256. Confirmed by indentation as well: 276 is at the
  same level as 256, not 258.
* Could case (b) be intended as "any `!`-prefixed token resets"? Then the outer test's
  `&& !ptr[1]` would be pointless, and the reset would be *temporary* (saved to
  `s_ign_list`) for `-I '!foo'` on the command line while being *permanent* for `-I '!'` —
  an incoherent pair. Also `next_token()` (ignore.cpp:199-234) already strips quoting, so
  `!foo` reaches here as a genuine user pattern.
* Could case (a) be intended, i.e. `!` inside a per-directory `.cvsignore` is deliberately
  not supported? Then `s_ign_list`/`s_ign_count` and the restore block at ignore.cpp:149-158
  would be dead code — nothing else in the file ever assigns `s_ign_list`, and `ign_add_file`
  is the only reader.
* Is `hold=1` actually used? Yes: ignore.cpp:412 (`ignore_files`, the `? file` reporting used
  by `cvs update`) and import.cpp:551.
* Secondary, not part of this report's claim: lines 266 and 292 write `ign_list[0] = NULL`
  without checking `ign_list != NULL`. `ign_list` is a zero-initialised static grown only at
  ignore.cpp:297-302, and `ign_add_file` already guards the analogous write with
  `if(ign_list)` (ignore.cpp:175-176). In practice `ign_setup()` populates the list first
  (ignore.cpp:64-65), so this is latent rather than live.
