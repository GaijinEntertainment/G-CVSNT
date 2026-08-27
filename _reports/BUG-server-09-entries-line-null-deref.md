---
id: BUG-server-09
area: server
file: cvsnt/cvsnt-2.5.05.3744/src/server.cpp
line: 2232
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 18
behavior_change: yes
---

# Client-supplied `Entry` line with a missing `/` makes the server dereference address `0x1` (`strchr(...) + 1` with no NULL check)

## Summary
`serve_unchanged`, `serve_is_modified` and `serve_file_kopt` locate the timestamp field of a stored `Entries` line with `timefield = strchr (cp + 1, '/') + 1;` and never test the `strchr` result. `serve_entry` stores the client's `Entry` argument verbatim with no format validation, so an `Entry` line containing only one `/` after the file name makes `timefield` equal `(char *)1`, which is then dereferenced.

## Code
```cpp
// server.cpp:2222-2244  (serve_unchanged)
    for (p = entries; p != NULL; p = p->next)
    {
	name = p->entry + 1;
	cp = strchr (name, '/');
	if (cp != NULL
	    && strlen (arg) == cp - name
	    && strncmp (arg, name, cp - name) == 0)
	{
	    timefield = strchr (cp + 1, '/') + 1;          // <-- NULL + 1
	    if (*timefield != UNCHANGED_CHAR && *timefield!=MODIFIED_CHAR && *timefield!=DATE_CHAR)
```

Identical at server.cpp:2271 (`serve_is_modified`) and, doubly, at server.cpp:2435-2436 (`serve_file_kopt`):

```cpp
		    timefield = strchr (cp + 1, '/') + 1;
			optfield = timefield?strchr(timefield,'/')+1:NULL;   // timefield==(char*)1 is "true"
```

And the producer performs no validation at all:

```cpp
// server.cpp:2334-2354  (serve_entry)
    cp = (char*)xmalloc (strlen (arg) + 2);
    ...
    strcpy (cp, arg);
    p->next = entries;
    p->entry = cp;
```

## Why it is a bug
`strchr` returns `NULL` when the character is absent. `NULL + 1` is undefined behaviour and in practice yields the address `0x1`; `*timefield` then faults. In `serve_file_kopt` the guard `timefield ? ... : NULL` is worse than useless — `(char *)1` is non-null, so it proceeds to `strchr((char*)1, '/')`, which walks from address 1.

A well-formed CVS `Entries` line is `/name/version/timestamp/options/tagdate`, so the loop's `cp` is the `/` after `name` and `strchr(cp + 1, '/')` is expected to find the `/` after `version`. Nothing enforces that a *third* `/` exists. Note that the entry-matching condition only inspects the text up to the first `/` after the name, so a two-field line matches happily.

Upstream CVS added exactly this guard in the 1.11.17 security round; this fork is missing it:

```c
	    if (!(timefield = strchr (cp + 1, '/')) || *++timefield == '\0')
	      {
		error (0, 0, "Invalid Entries line: %s", p->entry);
		return;
	      }
```

## Failure scenario
Any client (including an anonymous/read-only pserver login) that reaches the request loop can send:

```
Root /repo
Directory .
/repo
Entry /foo/
Unchanged foo
```

* `serve_entry("/foo/")` stores `p->entry = "/foo/"`.
* `serve_unchanged("foo")`: `outside_dir("foo")` passes; `name = "foo/"`; `cp` points at the `/` at index 3; `strlen("foo") == 3 == cp - name` and `strncmp` matches, so the body runs.
* `cp + 1` is `""`, `strchr("", '/')` returns `NULL`, `timefield = (char *)1`.
* `*timefield` → SIGSEGV.

The same line reached via `Is-modified foo` hits server.cpp:2271, and via `Modified foo` (which calls `receive_file (size, arg, !(serve_file_kopt(arg).flags & ...))`, server.cpp:2045) hits server.cpp:2435-2436, where `strchr((char*)1, '/')` scans from address 1.

The result is a crash of the server process handling that connection — a trivially triggered denial of service requiring only the access level needed to run `cvs update`.

## Suggested fix
Add the missing NULL check at each of the three sites, e.g. for `serve_unchanged`:
```cpp
	    if (!(timefield = strchr (cp + 1, '/')) || *++timefield == '\0')
	    {
		error (0, 0, "Invalid Entries line: %s", p->entry);
		return;
	    }
	    if (*timefield != UNCHANGED_CHAR && *timefield!=MODIFIED_CHAR && *timefield!=DATE_CHAR)
```
and for `serve_file_kopt`:
```cpp
		    timefield = strchr (cp + 1, '/');
		    optfield = timefield ? strchr (timefield + 1, '/') : NULL;
		    if (optfield)
		    {
			++optfield;
			...
```

## Refutation attempt
* *Does something validate `Entry` before it reaches these functions?* No. `serve_entry` (server.cpp:2334) copies `arg` with `strcpy` and stores it; there is no format check anywhere between the request dispatcher (`REQ_LINE("Entry", serve_entry, RQ_ESSENTIAL)`, server.cpp:4924) and these consumers.
* *Would the entry-match test reject a short line first?* No — the test is `strlen (arg) == cp - name && strncmp (arg, name, cp - name) == 0`, which only examines the name field. `"/foo/"` matches `arg == "foo"`.
* *Does `outside_dir` block it?* `outside_dir` rejects names escaping the directory (`../`, absolute paths); a plain name like `foo` passes.
* *Is `NULL + 1` benign on the target platforms?* It is UB, and on every mainstream implementation it produces `(char *)1`, whose dereference faults. Even the `timefield ? ...` test at server.cpp:2436 shows the author expected a NULL to be possible here but wrote the check one operation too late.
* *Could the shifting loop below overflow the buffer instead?* No — `serve_entry` deliberately allocates `strlen(arg) + 2` ("Leave space for serve_unchanged to write '=' if it wants"), and the shift adds exactly one byte, so that part is correct. The defect is solely the missing NULL check.
