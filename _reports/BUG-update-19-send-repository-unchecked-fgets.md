---
id: BUG-update-19
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/client.cpp
line: 3444
severity: medium
category: memory-safety
verdict: PLAUSIBLE
fix_size_loc: 6
behavior_change: yes
---

# `send_repository()` ignores the `fgets()` return value and then indexes `line[strlen(line)-1]` on a possibly-uninitialised stack buffer

## Summary
In the `CVS/VirtualRepository` branch of `send_repository()`, the `CVS/Repository` file is
read with an unchecked `fgets()` and the result is immediately truncated with
`line[strlen(line)-1]='\0'`. If `fgets()` returns NULL (empty file, or a read error) `line`
is never written, so `strlen()` runs over uninitialised stack memory and the store can land
outside the buffer — below it when the garbage starts with a NUL (`line[-1]`), or above it
when no NUL appears within `MAX_PATH` bytes.

## Code
```cpp
/* src/client.cpp:3349, declaration */
    char line[MAX_PATH];                      /* uninitialised automatic */
    ...
/* src/client.cpp:3431-3455 */
	strcat (adm_name, CVSADM_VIRTREPOS);
	if(isfile(adm_name))
	{
	  adm_name[0] = '\0';
	  if (dir[0] != '\0')
	  {
	    strcat (adm_name, dir);
	    strcat (adm_name, "/");
	  }
	  strcat (adm_name, CVSADM_REP);
	  f = CVS_FOPEN (adm_name, "r");
	  if (f == NULL)
		error (1, errno, "reading %s", adm_name);
	  fgets (line, sizeof (line), f);          /* 3444 <-- return value discarded */
	  line[strlen(line)-1]='\0';               /* 3445 <-- strlen/store on garbage */
	  if(!isabsolute(line))
	  {
		send_to_server(current_parsed_root->directory,0);
		send_to_server("/",1);
	  }
	  send_to_server(line,0);
	  ...
	}
```

The `Sticky` block 30 lines below in the same function gets this right:
```cpp
/* src/client.cpp:3486-3494 */
	    while (fgets (line, sizeof (line), f) != NULL)
	    {
		send_to_server (line, 0);
		nl = strchr (line, '\n');
		if (nl != NULL)
		    break;
	    }
```

## Why it is a bug
`fgets()` returns NULL and leaves the buffer untouched when it reads zero characters —
i.e. on immediate EOF (a zero-byte file) or on a read error. `line` is a plain automatic
array with no initialiser, so after a NULL return its contents are whatever the previous
stack frame left behind. Two distinct out-of-bounds accesses follow:

* If the first stale byte is `'\0'`, `strlen(line) == 0` and line 3445 stores to
  `line[-1]` — a one-byte write *below* the array, into the surrounding frame.
* If no `'\0'` occurs within `MAX_PATH` bytes, `strlen()` reads past the end of `line` up
  the stack, and `line[strlen(line)-1] = '\0'` then writes at that out-of-bounds offset.

Even in the benign middle case, the working directory's repository line is replaced by
stack garbage and sent to the server as the `Directory` response's repository
(`send_to_server(line,0)` at client.cpp:3451).

Line 3445 is also wrong for a second, milder reason: it strips the last byte
unconditionally, so a `CVS/Repository` path longer than `MAX_PATH-1` (where `fgets`
returns a full buffer with no `'\n'`) loses a real character of the path.

## Failure scenario
The branch is entered only when `CVS/VirtualRepository` exists (client.cpp:3431-3432), i.e.
a CVSNT working copy using virtual repositories. Within such a directory, a zero-length
`CVS/Repository` reaches line 3444 with an empty stream:

1. A checkout is interrupted (Ctrl-C, full disk, killed connection) between
   `Create_Admin()`'s `CVS_FOPEN (tmp, "w+")` on `CVS/Repository` (create_adm.cpp:85) and the
   `fprintf (fout, "%s\n", cp)` that fills it (create_adm.cpp:125) — the file exists and is
   zero bytes. The same state results from a filesystem that lost the block on power failure,
   or from copying a working tree with a tool that truncates.
2. The next `cvs update` in that directory calls `send_files` -> `send_a_repository` ->
   `send_repository (dir, repository, update_dir)`.
3. `isfile("CVS/VirtualRepository")` is true, `CVS_FOPEN("CVS/Repository","r")` succeeds,
   `fgets` returns NULL, and line 3445 executes against uninitialised `line`.

Outcome ranges from a corrupt `Directory` repository line sent to the server (silently
operating on the wrong repository path) to a stack write outside `line`.

This is filed PLAUSIBLE rather than CONFIRMED because it needs a zero-byte
`CVS/Repository` — a corrupted working copy, not one CVS produces in normal operation.

## Suggested fix
```cpp
	  f = CVS_FOPEN (adm_name, "r");
	  if (f == NULL)
		error (1, errno, "reading %s", adm_name);
	  line[0] = '\0';
	  if (fgets (line, sizeof (line), f) == NULL)
		error (1, 0, "cannot read %s", adm_name);
	  {
	    size_t l = strlen (line);
	    if (l > 0 && line[l - 1] == '\n')
		line[l - 1] = '\0';
	  }
```

## Refutation attempt
* Could `fgets` be guaranteed to succeed because the file was just tested with `isfile`?
  `isfile` was applied to `CVS/VirtualRepository` (client.cpp:3431), not to
  `CVS/Repository`; the latter is only tested for openability, and an existing but empty
  file opens fine.
* Could `CVS/Repository` never be empty? `Create_Admin` writes it in two steps —
  `CVS_FOPEN(tmp,"w+")` at create_adm.cpp:85 and `fprintf` at create_adm.cpp:125 — so the
  zero-byte window exists on disk, and nothing validates the file's size on read.
* Is `line` initialised earlier in the function? No: `char line[MAX_PATH];`
  (client.cpp:3349) has no initialiser, and every prior use in `send_repository` is inside
  this same `if` or in the later `Sticky` block.
* Would the compiler zero it? Not for a plain automatic array; only `/GS`-style cookies or
  a debug runtime's fill pattern would touch it, and neither guarantees a NUL at index 0.
* Is the shorter path guaranteed by `MAX_PATH`? `CVS/Repository` holds an absolute
  repository path that can legitimately approach or exceed `MAX_PATH` on the server-side
  paths CVSNT stores, so the unconditional last-byte strip is a real (if separate) defect.
