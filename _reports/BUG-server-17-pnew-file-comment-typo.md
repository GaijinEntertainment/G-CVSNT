---
id: BUG-server-17
area: import
file: cvsnt/cvsnt-2.5.05.3744/src/import.cpp
line: 1356
severity: low
category: message
status: partially fixed in this slice (the string default; the pnew comment lines remain open)
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `"pnew file"` written into the RCS `comment` field — collateral damage from a `new` -> `pnew` identifier rename

## Summary
When `add_rcs_file` creates an RCS file with no source file, it writes the literal string `"pnew file"` as the RCS `comment` leader. This is a stray `p` left by a blanket rename of the C++ keyword `new` to `pnew`, which also mangled several comments in the same file.

## Code
```cpp
// import.cpp:1352-1358
    if (fprintf (fprcs, "locks    ; strict;\n") < 0 ||
	/* XXX - make sure @@ processing works in the RCS file */
	fprintf (fprcs, "comment  @%s@;\n", userfile?get_comment (userfile):"pnew file") < 0)
    {
	goto write_error;
    }
```

The same rename left these behind in the same file:
```
import.cpp:719:   * A pnew import source file; it doesn't exist as a ,v within the
import.cpp:769: * The RCS file exists; update it by adding the pnew import file to the
import.cpp:798:   * is no need to install the pnew import file as a pnew revision to the
import.cpp:799:   * branch.  Just tag the revision with the pnew import tags.
import.cpp:1145:/* Create a pnew RCS file from scratch.
import.cpp:1160:      the modes to give the pnew RCS file.  */
```
`grep -rn "pnew" *.cpp` shows every other hit is a legitimate local variable (`checkout.cpp`, `import.cpp:1732-1743`), confirming the rename was a mechanical `new` -> `pnew` sweep that caught the comments and this one string literal.

## Why it is a bug
The value is not a diagnostic — it is written verbatim into the repository:
```
comment  @pnew file@;
```
The `comment` field is the RCS comment leader, echoed by `cvs log` and used by RCS-compatible tools. Every RCS file created through a `userfile == NULL` call is permanently stamped with a misspelling. Compare the guarded path, which writes `get_comment (userfile)` (a real comment leader such as `"# "` or `""`).

## Failure scenario
`create_mapping_file` (mapping.cpp:1303) calls `add_rcs_file(message, fn, NULL /* userfile */, "1.1", ...)`. The `userfile?...:` ternary therefore selects the literal, and the generated `.directory_history,v` for every directory that gets a rename or a re-add contains:

```
comment  @pnew file@;
```

`cvs log` on that file, and any `rlog`/RCS tooling pointed at the repository, reports the comment leader as `pnew file`. It is cosmetic but permanent and it propagates through every `RCS_rewrite`, since `RCS_putadmin` copies `rcs->comment` back out verbatim.

## Suggested fix
```cpp
	fprintf (fprcs, "comment  @%s@;\n", userfile?get_comment (userfile):"new file") < 0)
```
(and, separately, revert `pnew` to `new` in the six comment lines listed above).

## Refutation attempt
* *Could `"pnew"` be intentional (e.g. a marker CVSNT looks for)?* No. `grep -rn "pnew file" .` finds exactly this one occurrence — nothing reads it back or compares against it.
* *Is the string ever actually used, or is `userfile` always non-NULL?* mapping.cpp:1303 passes `NULL` for `userfile`, so the branch is taken on every mapping-file creation.
* *Is the comment leader really meant to be a filler string here?* Upstream CVS writes `"# "` or the wrapper-derived leader for the file type; a placeholder is odd but the surrounding `userfile ? get_comment(userfile) : ...` structure shows a placeholder was intended — just spelled `new file`.
