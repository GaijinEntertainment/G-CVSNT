---
id: BUG-server-01
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 1308
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 4
behavior_change: no
---

# `rcsbuf_getkey` fails to relocate `pat` and `keystart` after a buffer-growing `rcsbuf_fill`, producing dangling pointers into a freed heap block

## Summary
In the "value which is not a simple `@` string" branch of `rcsbuf_getkey`, the inner `@`-scanning loop calls `rcsbuf_fill()` but passes/updates the wrong pointer. `rcsbuf_fill()` may `xrealloc()` the parse buffer and relocates only the pointers it is handed; `pat` and `keystart` are left pointing into the old (freed) block and are dereferenced immediately afterwards.

## Code
```cpp
// rcs.cpp:1289-1320
		while (1)
		{
			while ((pat = (char*)memchr (ptr, '@', ptrend - ptr)) == NULL)
			{
				ptr = rcsbuf_fill (rcsbuf, ptrend, keyp, valp, NULL);   // <-- keystart NOT relocated
				if (ptr == NULL)
					error (1, 0,
					"EOF while looking for end of string in RCS file %s",
					rcsbuf->filename);
				ptrend = rcsbuf->ptrend;
			}

			/* Handle the special case of an '@' right at the end of
				the known bytes.  */
			if (pat + 1 >= ptrend)
			{
				ptr = rcsbuf_fill (rcsbuf, ptr, keyp, valp, NULL);      // <-- should be `pat = rcsbuf_fill (rcsbuf, pat, ...)`
				if (ptr == NULL)
					error (1, 0, "EOF in value in RCS file %s",
					rcsbuf->filename);
				ptrend = rcsbuf->ptrend;
			}

			if (pat[1] != '@')                                          // <-- read through stale `pat`
			break;

			/* We found an '@' pair in the string.  Keep looking.  */
			ptr = pat + 2;                                              // <-- ptr set into the freed block
		}
```

Compare with the sibling block in the *same function*, which does it correctly:

```cpp
// rcs.cpp:1121-1128
			if (pat + 1 >= ptrend)
			{
				/* Note that we pass PAT, not PTR, here.  */
				pat = rcsbuf_fill (rcsbuf, pat, &keystart, keyp, NULL);
```

## Why it is a bug
`rcsbuf_fill()` (rcs.cpp:1415) grows the buffer with `expand_string()`, computes `poff = rcsbuf->buffer - oldbuf`, and then fixes up: the registered relocation table, `ptr` (its own parameter), `rcsbuf->ptrend`, `rcsbuf->ptr`, and the three optional `ptr1/ptr2/ptr3` out-params. `pat` and `keystart` are plain locals of `rcsbuf_getkey`, are not in the relocation table (only `rcsbuf_valcopy` registers entries there), and are not passed to `rcsbuf_fill` here. When `xrealloc` moves the block, `poff != 0` and both locals become dangling.

Immediately after the fill:
* `pat[1]` (line 1316) is a read of freed memory.
* `ptr = pat + 2` (line 1321) puts `ptr` into the freed block while `ptrend` points into the new block, so the next `memchr (ptr, '@', ptrend - ptr)` gets a wildly wrong length. If the new block is at a lower address than the old one, `ptrend - ptr` is negative and converts to a huge `size_t`, giving an unbounded out-of-bounds read.
* `keystart` (the *value* start in this branch — it is reassigned at rcs.cpp:1085 `if (c != '@') keystart = ptr;`) is used after the loop as `start = keystart; ... vlen = psemi - start; *valp = start;`. A stale `keystart` yields a `*valp` pointing into freed memory and a garbage `vlen`, which `rcsbuf_valcopy` then feeds straight into `memcpy (ret, val, vlen + 1)` after `xmalloc(vlen - embedded_at + 1)`.

## Failure scenario
This branch is entered for any RCS newphrase whose value does *not* begin with `@` but does contain an `@` string. That is exactly what `putprop_proc` (rcs.cpp:6480) emits for the `properties` field:

```
properties
	myprop:@value with a ; or @@ in it@;
```

`properties` is written at file level (`RCS_putadmin`, rcs.cpp:6603) and per-delta (`putdelta`, rcs.cpp:6671), and is set by `cvs admin -p` (admin.cpp:828) and by `rcs_checkin.cpp:674`. On read, `rcsbuf_getkey` skips whitespace, sees `m` (not `@`), takes the composite path, `memchr` finds the trailing `;`, `memchr(start,'@',psemi-start)` finds the opening `@`, and the inner loop runs.

Concretely: set a property whose value contains `@` on a file whose RCS file is larger than the read chunk (`RCSBUF_BUFSIZE = BUFSIZ*10`; on MSVC `BUFSIZ` is 512, so 5120 bytes — a very ordinary RCS file), and arrange for the closing `@` of that property to land on a chunk boundary. `rcsbuf_fill` is then called from inside the inner loop; `xrealloc` on a repeatedly-grown buffer relocates it; and the server reads freed memory, then either crashes or produces a garbage property value that is subsequently written back into the RCS file by `RCS_rewrite`.

## Suggested fix
```cpp
		while (1)
		{
			while ((pat = (char*)memchr (ptr, '@', ptrend - ptr)) == NULL)
			{
				ptr = rcsbuf_fill (rcsbuf, ptrend, &keystart, keyp, valp);
				if (ptr == NULL)
					error (1, 0,
					"EOF while looking for end of string in RCS file %s",
					rcsbuf->filename);
				ptrend = rcsbuf->ptrend;
			}

			/* Handle the special case of an '@' right at the end of
				the known bytes.  */
			if (pat + 1 >= ptrend)
			{
				/* Note that we pass PAT, not PTR, here.  */
				pat = rcsbuf_fill (rcsbuf, pat, &keystart, keyp, valp);
				if (pat == NULL)
					error (1, 0, "EOF in value in RCS file %s",
					rcsbuf->filename);
				ptrend = rcsbuf->ptrend;
			}
```
(`rcsbuf_fill` takes three relocatable out-params; `keystart`, `*keyp` and `*valp` are exactly the three live pointers here.)

## Refutation attempt
* *Could `rcsbuf_fill` never move the buffer?* No — it calls `expand_string(&rcsbuf->buffer, &rcsbuf->buffer_size, rcsbuf->buffer_size + RCSBUF_BUFSIZE)`, which is a `xrealloc`. The function itself acknowledges movement is possible: it computes `poff` and fixes up every pointer it knows about, including an explicit "Movable pointer not within rcs buffer - aborting" bounds check for the relocation table.
* *Could `pat` be in the relocation table?* No. Entries are only added by `rcsbuf_valcopy` (rcs.cpp:1518) for `valp`-style out-params; `pat` and `keystart` are stack locals.
* *Is `pat + 1 >= ptrend` really possible here?* Yes — `memchr` can return the very last buffered byte, which is precisely the case this block exists to handle.
* *Is the branch dead code?* The comment claims "this type of value never arises in a normal RCS file", but that comment predates CVSNT's `properties` newphrase, which `putprop_proc` deliberately emits in exactly this shape. The sibling block at line 1121 handling the same situation correctly shows the divergence is an editing slip, not intent.
* *Does passing `ptr` instead of `pat` at least keep `ptr` valid?* Yes, `ptr` is relocated — but it is then overwritten by `ptr = pat + 2` from the stale `pat`, so that does not save it.
