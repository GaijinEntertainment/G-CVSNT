---
id: BUG-server-06
area: locking
file: cvsnt/cvsnt-2.5.05.3744/src/lock.cpp
line: 263
severity: low
category: leak
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# Shadowed local `ob` in `do_lock_server` defeats all three `xfree(ob)` calls, leaking a path buffer per lock

## Summary
`do_lock_server` declares `char *ob = NULL;` at function scope and calls `xfree(ob)` on all three exit paths, but the block that actually allocates the buffer re-declares `char *ob;` locally. The inner declaration shadows the outer one, so every `xfree(ob)` frees the outer pointer — which is always `NULL` — and the allocation is leaked.

## Code
```cpp
// lock.cpp:254-276
static size_t do_lock_server(const char *object, const char *directory, const char *flags, int wait)
{
	char line[MAX_PATH*4],*p,*q, *ob = NULL;      // outer ob, stays NULL forever
	int bWaited;
	size_t id;
	unsigned helper;

	if(filenames_case_insensitive && object)
	{
		char *ob;                                  // <-- shadows the outer ob

		if(directory)
		{
			ob = (char*)xmalloc(strlen(directory)+strlen(object)+10);
			sprintf(ob,"%s/%s",directory,object);
		}
		else
			ob = (char*)xstrdup(object);
		ob = normalize_path(ob);
		object = last_component(ob);
		if(object>ob)
		{
			((char*)object)[-1]='\0';
			directory=ob;
		}
		else if(directory)
			directory="";
	}                                              // inner ob dies here; buffer unreachable
```

The three cleanup sites all operate on the outer, still-`NULL` pointer:
```
lock.cpp:307   xfree(ob);   // success return
lock.cpp:314   xfree(ob);   // no-wait return 0
lock.cpp:358   xfree(ob);   // tail
```

## Why it is a bug
`normalize_path` is a one-line identity function (`return arg;`, filesubr.cpp:1317-1320), so `ob` still holds the `xmalloc`/`xstrdup` result at the end of the block. `object` and `directory` are set to point *into* that buffer and are used for the remainder of the function, so it must stay alive until the function returns — which is exactly why the author added the function-scope `ob` and the three `xfree(ob)` calls. The inner `char *ob;` turns all of that into dead code: `xfree(NULL)` is a no-op and the real pointer is lost when the block ends.

## Failure scenario
On any case-insensitive-filenames configuration (`filenames_case_insensitive`, i.e. every Windows server and any server configured with case-insensitive names), `do_lock_server` runs once per lock acquisition via `do_lock_file` (lock.cpp:362), which `rcsbuf_open` (rcs.cpp:903) calls for every RCS file it opens. A `cvs commit`, `cvs update` or `cvs checkout` touching 10 000 files leaks 10 000 path-sized buffers (`strlen(directory)+strlen(object)+10` bytes each, typically 60-200 bytes) — a few MB per large operation, held for the life of the server process.

## Suggested fix
Delete the inner declaration so the block assigns the function-scope `ob`:
```cpp
	if(filenames_case_insensitive && object)
	{
		if(directory)
		{
			ob = (char*)xmalloc(strlen(directory)+strlen(object)+10);
```

## Refutation attempt
* *Does something else own the buffer?* No. `object`/`directory` are `const char *` parameters reassigned to interior pointers; nothing copies or frees them. `grep -n "xfree(ob)" lock.cpp` finds only the three outer-scope calls.
* *Could `normalize_path` be freeing or transferring ownership?* No — it is `return arg;` and nothing else.
* *Is the outer `ob` perhaps intended for something else?* It is initialised to `NULL` and never assigned anywhere else in the function, so `xfree(ob)` on it is unconditionally a no-op. Owning this allocation is its only possible purpose.
* *Would the compiler have caught it?* Only with `-Wshadow`, which is not enabled in this tree's build flags.
