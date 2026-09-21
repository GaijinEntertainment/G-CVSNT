---
id: BUG-update-13
area: client/update core
file: cvsnt/cvsnt-2.5.05.3744/src/vers_ts.cpp
line: 312
severity: low
category: leak
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# `Version_TS()` leaks the results of `RCS_getexpand()` and `wrap_rcsoption()` — once per file, per command

## Summary
`Version_TS()` obtains two heap strings — the keyword-expansion mode from the RCS file
(`RCS_getexpand`) and the wrapper default (`wrap_rcsoption`) — copies each with `xstrdup()`
into `vers_ts->options` instead of taking ownership, and never frees the originals.
`Version_TS()` runs once for every file in every recursion, so the leak scales with the
size of the checkout.

## Code
```cpp
/* src/vers_ts.cpp:305-317 */
			{
				/* If no keyword expansion was specified on command line,
				use whatever was in the rcs file (if there is one). ... */
				if(vers_ts->vn_rcs)
				{
					char *rcsexpand = RCS_getexpand(rcsdata?rcsdata:finfo->rcs,vers_ts->vn_rcs);
					xfree(vers_ts->options);
					vers_ts->options = xstrdup(rcsexpand);   /* 314 - copies, never frees rcsexpand */
				}
				assign_options(&vers_ts->options,options);
			}
```

```cpp
/* src/vers_ts.cpp:189-200 */
    if (options && *options != '\0')
	{
		TRACE(3,"Version_TS: got an open (eg: -k+x), need to find the 'default' for \"%s\"",PATCH_NULL(finfo->file));
		char *existing_options=wrap_rcsoption(finfo->file);       /* 192 - allocates */
		if ((vers_ts->options==NULL)&&(existing_options!=NULL))
		{
			TRACE(3,"Version_TS: an default options of \"%s\".",PATCH_NULL(existing_options));
			vers_ts->options=xstrdup(existing_options);           /* 196 - copies, never frees */
		}
		TRACE(3,"Version_TS: assign_options(\"%s\",\"%s\")",PATCH_NULL(vers_ts->options),PATCH_NULL(options));
		assign_options(&vers_ts->options,options);
	}                                                             /* 200 - existing_options goes out of scope */
```

## Why it is a bug
Both helpers return caller-owned memory:

```cpp
/* src/rcs.cpp:3691-3709 */
char *RCS_getexpand(RCSNode *rcs, const char *vn_rcs)
{
	...
			ver = xstrdup(v->kopt);
		if(!ver)
			ver = xstrdup(rcs->expand);
	}
	return ver;
}
```
and the other call site in the tree frees it explicitly:
```cpp
/* src/rcs_checkin.cpp:1443-1445 */
    exp = RCS_getexpand (rcs, rev);
    RCS_get_kflags(options?options:exp, false, expand);
    xfree(exp);
```

```cpp
/* src/wrapper.cpp:464-484 */
char *wrap_rcsoption(const char *filename)
{
    ...
	char *options = NULL;
	const char *opt = e->rcsOption.c_str();
	assign_options(&options,opt);     /* xmalloc/xstrdup inside */
	...
	return options;
}
```
`assign_options` (vers_ts.cpp:15-92) always ends by `xstrdup`ing or `xmalloc`ing into
`*existing_options`, so `wrap_rcsoption`'s return value is always heap memory when non-NULL.
Other callers treat it as owned (e.g. `add.cpp:516` assigns it directly into
`vers->options`, which `freevers_ts` frees).

In both spots the code takes a *copy* (`xstrdup`) and drops the original on the floor,
which is what makes the leak easy to miss: the value is not lost, just duplicated.

## Failure scenario
`Version_TS()` is called once per file by `Classify_File` (classify.cpp:41),
`checkout_file` (update.cpp:1749), `patch_file` (update.cpp:2183),
`join_file` (update.cpp:2985), `send_fileproc` (client.cpp:5312) and
`checkout_proc` (checkout.cpp:1256).

* Line 314 leaks whenever the RCS file records a keyword-expansion mode — i.e. every
  `-kb` binary file and every file whose head revision carries a `kopt`. A single
  `cvs update` over a tree with 200,000 such files leaks 200,000 short strings; the
  server-side process handling one `checkout` of a large module accumulates the same.
* Line 192 leaks whenever `-k` is given on the command line *and* the file matches a
  `cvswrappers` entry, once per matching file.

There is no correctness impact — only unbounded growth for the duration of a single
command, which matters most for the server process serving a big checkout.

## Suggested fix
```cpp
				if(vers_ts->vn_rcs)
				{
					char *rcsexpand = RCS_getexpand(rcsdata?rcsdata:finfo->rcs,vers_ts->vn_rcs);
					xfree(vers_ts->options);
					vers_ts->options = rcsexpand;   /* take ownership; NULL is fine here,
					                                   the `if (!vers_ts->options)` at
					                                   vers_ts.cpp:368 restores "" */
				}
```
```cpp
		char *existing_options=wrap_rcsoption(finfo->file);
		if ((vers_ts->options==NULL)&&(existing_options!=NULL))
		{
			vers_ts->options=xstrdup(existing_options);
		}
		assign_options(&vers_ts->options,options);
		xfree(existing_options);
```

## Refutation attempt
* Could `RCS_getexpand` return a borrowed pointer into the RCSNode? No — both return paths
  are `xstrdup(...)` (rcs.cpp:3703 and rcs.cpp:3706), and `rcs_checkin.cpp:1445` `xfree`s
  the result.
* Could `wrap_rcsoption` return a pointer into the static `wrap_list`? No — it builds a
  fresh buffer through `assign_options(&options, opt)` starting from `options = NULL`
  (wrapper.cpp:471-473); the `WrapperEntry`'s own string is only read via `c_str()`.
* Does `freevers_ts` clean these up indirectly? It frees `vers_ts->options`
  (vers_ts.cpp:533) — the *copy*. The originals are unreferenced locals by then.
* Is `rcsexpand` usually NULL, making the leak negligible? It is NULL only when both
  `v->kopt` and `rcs->expand` are NULL, i.e. the RCS file records no expansion mode at all.
  Any `-kb`/`-ku`/`-kv` file has one.
