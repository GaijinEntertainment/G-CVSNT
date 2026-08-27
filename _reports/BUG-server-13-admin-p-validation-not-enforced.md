---
id: BUG-server-13
area: admin
file: cvsnt/cvsnt-2.5.05.3744/src/admin.cpp
line: 823
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: yes
---

# `cvs admin -p`: property-name validation prints an error but does not stop the write, letting `;` and `@` into the RCS `properties` newphrase

## Summary
The two validity checks on a `-p` property name print an error via `error(0, 0, ...)` and then fall straight through to `RCS_setprop`. They set no status and do not `continue`, so `status` stays 0, `RCS_rewrite` runs, and a property name containing `;` or `@` is written unescaped into the `properties` field of the RCS file — corrupting it.

## Code
```cpp
// admin.cpp:819-832
			if(!isalpha((unsigned char)prop[0]))
			{
				error(0, 0, "%s: numeric properties not allowed '%s'", fn_root(rcs->path), prop);
			}                                    // <-- no status = 1, no continue
			if(strpbrk (prop, "$,:;@"))
			{
				error(0, 0, "%s: Invalid characters in property name '%s'", fn_root(rcs->path), prop);
			}                                    // <-- no status = 1, no continue

			if(!RCS_setprop(rcs,rev,prop,val))   // <-- runs anyway
			{
				error(0, 0, "%s: unable to set property %s", fn_root(rcs->path), prop);
				status = 1;
			}
```

Every other failure in this switch does it correctly, e.g. eleven lines above:
```cpp
// admin.cpp:791-796
			if (p == NULL)
			{
				error (0, 0, "%s: -p option needs [rev:]prop=value", fn_root(rcs->path));
			    status = 1;
			    continue;
			}
```

And `status` is what gates the write:
```cpp
// admin.cpp:846-852
    if (status == 0)
    {
	RCS_rewrite (rcs, NULL, NULL, 0);
```

## Why it is a bug
`RCS_setprop` (rcs.cpp:3049) does no validation of its own — it `xstrdup`s `prop` straight into the property list — so admin.cpp is the only gate.

The writer does not escape property *names*:
```cpp
// rcs.cpp:6480-6495  (putprop_proc)
    putc ('\n', fp);
    putc ('\t', fp);
    fputs (propnode->key, fp);              // <-- key written raw, never @-quoted
    putc (':', fp);
	if(!strpbrk (propnode->data, "$,:;@"))  // <-- only the *value* is checked
		fputs (propnode->data, fp);
	else
	{
	    putc ('@', fp);
	    expand_at_signs (propnode->data, strlen (propnode->data), fp);
	    putc ('@', fp);
	}
```

`;` terminates a newphrase in the RCS grammar, so a `;` inside the key splits the `properties` field in two. The `strpbrk(prop, "$,:;@")` check exists precisely because the writer cannot escape the key — it is just never enforced.

## Failure scenario
```
cvs admin -p 'x;y=1' somefile
```

1. `p = strchr(arg,'=')` splits off `val = "1"`; `prop = "x;y"`; `rev = NULL`.
2. `strpbrk("x;y", "$,:;@")` matches `;` — an error line is printed, and nothing else happens.
3. `RCS_setprop(rcs, NULL, "x;y", "1")` adds the node.
4. `status` is still 0, so `RCS_rewrite` runs and `RCS_putadmin` (rcs.cpp:6603-6612) emits

   ```
   properties
        x;y:1;
   ```

5. On the next parse, `rcsbuf_getkey` reads key `properties`, then scans for the terminating `;` with `memchr` — and finds the one *inside the key*. The `properties` value becomes `x`, and the parser then reads `y` as a brand-new admin keyword with value `1`. The property `x;y` is gone, a bogus `y` field now exists in the RCS header, and every subsequent `RCS_rewrite` preserves the damage.

The user sees an error message and reasonably assumes nothing was written — the command even prints `done` afterwards, because `status == 0`.

`cvs admin` is typically restricted to the `cvsadmin` group, so this is a foot-gun for a privileged user rather than an unprivileged attack; but it silently and permanently damages the repository file, and it is server-side (`admin_fileproc` runs after the `current_parsed_root->isremote` early return at admin.cpp:435).

## Suggested fix
```cpp
			if(!isalpha((unsigned char)prop[0]))
			{
				error(0, 0, "%s: numeric properties not allowed '%s'", fn_root(rcs->path), prop);
				status = 1;
				*(val-1)='=';
				xfree (rev);
				continue;
			}
			if(strpbrk (prop, "$,:;@"))
			{
				error(0, 0, "%s: Invalid characters in property name '%s'", fn_root(rcs->path), prop);
				status = 1;
				*(val-1)='=';
				xfree (rev);
				continue;
			}
```

## Related defect in the same block
The `rev == NULL` path at admin.cpp:804-809 `continue`s *without* restoring the two bytes it overwrote (`*p=':'` at line 811 and `*(val-1)='='` at line 833). `arg` is an element of `admin_data.av[]`, which `admin_fileproc` re-reads for **every file** in the recursion, so one unresolvable revision leaves the `-p` argument permanently truncated at the `:` and every remaining file fails with the unrelated message `-p option needs [rev:]prop=value`.

## Refutation attempt
* *Does `RCS_setprop` reject the bad name?* No — rcs.cpp:3049-3101 only looks up/creates/deletes the node; there is no character check anywhere in it.
* *Does `putprop_proc` escape the key?* No. It `@`-quotes only `propnode->data`; `propnode->key` goes out via a bare `fputs`.
* *Does `error(0, ...)` abort?* No — status 0 means "warning, keep going"; only `error(1, ...)` exits. Compare the `status = 1; continue;` used by every neighbouring check in the same switch.
* *Could `status` be non-zero from an earlier option, saving us?* Only by luck, and only if the *same* `cvs admin` invocation had already failed something else. In the single-option case `cvs admin -p 'x;y=1'`, `status` is 0.
* *Is the `xfree(p->key); xfree(p->data); delnode(p);` sequence in `RCS_setprop` a double free?* No — `xfree` is `xfree_s`, which NULLs the member, so `freenode_mem`'s own frees see NULL and skip. Checked, not a finding.
