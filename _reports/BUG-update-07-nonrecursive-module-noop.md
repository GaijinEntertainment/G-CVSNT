---
id: BUG-update-07
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/recurse.cpp
line: 779
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `nonrecursive_module()` check in `do_recursion()` is a no-op — non-recursive modules are still recursed into

## Summary
`do_recursion()` guards the "this module is declared non-recursive, so do not descend"
rule with `frame->flags == R_SKIP_DIRS` and then *assigns* `R_SKIP_DIRS`. The assignment can
only run when the flag already holds that value, so the statement can never change
anything. Modules marked `local` in `CVSROOT/modules2` are therefore recursed into on every
repository-side command.

## Code
```cpp
/* src/recurse.cpp:779-786 */
	if (frame->flags == R_SKIP_DIRS && !(frame->which&W_LOCAL) && nonrecursive_module(repository))
		frame->flags = R_SKIP_DIRS;

	/* find sub-directories if we will recurse */
	if (frame->flags != R_SKIP_DIRS)
	    dirlist = Find_Directories (
		process_this_directory ? mapped_repository : NULL,
		frame->which, entries, repository);
```

```cpp
/* src/mapping.cpp:683-689 */
int nonrecursive_module(const char *repository)
{
	modules2_module_struct *dir;
	lookup_module2(relative_repos(repository),NULL,NULL,NULL,&dir);
	TRACE(3,"nonrecursive_module(%s) = %d",PATCH_NULL(repository),dir?dir->local:0);
	return dir?dir->local:0;
}
```

## Why it is a bug
`Dtype` is an enum with distinct values (`cvs.h:394-399`: `R_PROCESS = 1, R_SKIP_FILES,
R_SKIP_DIRS, R_SKIP_ALL, R_ERROR`), and `frame->flags` is initialised to
`local ? R_SKIP_DIRS : R_PROCESS` in `start_recursion()` (recurse.cpp:196) and to
`dir_return` for nested levels (recurse.cpp:1343). So `frame->flags == R_SKIP_DIRS` is true
exactly when recursion is *already* disabled — precisely the case in which setting it to
`R_SKIP_DIRS` accomplishes nothing.

The two-line statement exists solely to consult `nonrecursive_module()`, whose only caller
this is. If the guard were intended as written, the whole statement (and the
`nonrecursive_module()` helper) would be dead weight. The obvious intent — matching the
comment on the following line, "find sub-directories if we will recurse" — is
"if we *were* going to recurse but the module is flagged `local`, stop recursing", i.e.
the condition should be the negation.

Note that `nonrecursive_module()` also has a side effect worth preserving in either fix
(`lookup_module2` populates the modules2 cache), so the correct fix is to flip the
condition, not to delete the statement.

## Failure scenario
`CVSROOT/modules2` containing a module declared non-recursive, e.g.

```
[bigmodule]
  local
```

Then any repository-side (`!(which & W_LOCAL)`) recursive command run without `-l`:

```
cvs rtag -r HEAD RELEASE_1 bigmodule
```

1. `start_recursion` sets `frame.flags = R_PROCESS` (no `-l` given, recurse.cpp:196).
2. `do_recursion` reaches line 779. `frame->flags == R_SKIP_DIRS` is **false**
   (it is `R_PROCESS`), so `nonrecursive_module()` is never even called (short-circuit `&&`),
   and the flag stays `R_PROCESS`.
3. Line 783 `if (frame->flags != R_SKIP_DIRS)` is true, so `Find_Directories()` returns the
   subdirectories and the whole tree below `bigmodule` is tagged.

The `local` declaration is silently ignored: the administrator gets a full-tree `rtag`
where they asked for a single directory. The same applies to `cvs rdiff`, `cvs rls`, and
server-side `export`/`checkout` of that module.

## Suggested fix
```cpp
	if (frame->flags != R_SKIP_DIRS && !(frame->which&W_LOCAL) && nonrecursive_module(repository))
		frame->flags = R_SKIP_DIRS;
```

## Refutation attempt
* Could `frame->flags` be modified between the test and the assignment (e.g. by
  `nonrecursive_module()` itself)? No — `nonrecursive_module()` (mapping.cpp:683-689) only
  calls `lookup_module2()` and reads `dir->local`; `frame` is a local
  `struct recursion_frame` owned by `start_recursion`/`do_dir_proc` and is not reachable
  from mapping.cpp.
* Could `R_SKIP_DIRS` be a bitmask so that re-assigning it means something? No — `Dtype` is
  a plain sequential enum (`R_PROCESS = 1` then unnumbered successors), and every use in
  recurse.cpp is `==`/`!=` comparison, never bit testing.
* Is the statement perhaps intentionally disabled (a deliberate "turn this feature off")?
  If so it would be commented out or `#if 0`'d; instead it is live code whose only purpose
  is to call a helper that exists for no other caller. Either way, `nonrecursive_module()`
  currently has no observable effect anywhere in the tree, which contradicts the presence
  of `dir->local` in the modules2 parser.
* Does something else enforce `local`? `grep -rn "nonrecursive_module" src/` returns only
  `mapping.cpp:683` (definition), `mapping.h:31` (prototype) and `recurse.cpp:779` (this
  call). Nothing else consults it.
