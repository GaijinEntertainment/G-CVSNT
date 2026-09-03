---
id: BUG-server-02
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 5491
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# `linevector_free` uses post-decrement on the binary refcount, so binary file buffers are never freed

## Summary
`linevector_free()` tests `!vec->binary.bb->refcount--`, which is true only when the refcount was *already zero*. Since every `binbuffer` is created with `refcount = 1`, the release branch is never taken and the whole-file binary buffer plus its `struct binbuffer` are leaked on every binary checkout/update/diff/export.

## Code
```cpp
// rcs.cpp:5485-5501
static void linevector_free (linevector *vec)
{
    unsigned int ln;

	if(vec->is_binary)
	{
		if(vec->binary.bb && !vec->binary.bb->refcount--)   // <-- post-decrement
		{
			xfree(vec->binary.bb->buffer);
			xfree(vec->binary.bb);
		}
		else
			vec->binary.bb=NULL;
	}
	else
	{
		if (vec->text.vector != NULL)
		{
			for (ln = 0; ln < vec->text.nlines; ++ln)
				if (--vec->text.vector[ln]->refcount == 0)   // <-- pre-decrement, correct
				xfree (vec->text.vector[ln]);

			xfree (vec->text.vector);
		}
	}
}
```

The text branch of the very same function uses the correct pre-decrement idiom, which shows the intended semantics.

## Why it is a bug
`refcount` starts at 1 at every creation site:
* `linevector_add`, binary path — rcs.cpp:5343 `vec->binary.bb->refcount=1;`
* `apply_binary_changes`, text→binary conversion — rcs.cpp:5273 `lv.binary.bb->refcount=1;`
* `RCS_deltas`, scratch buffer — rcs.cpp:5919 `binbuf.binary.bb->refcount=1;`

and is bumped by `linevector_copy` (rcs.cpp:5454 `from->binary.bb->refcount++;`).

`x--` yields the *old* value, so `!bb->refcount--` is `!(old_refcount)`, i.e. "was already 0". With a minimum live refcount of 1, the condition is always false; control always goes to `else`, which merely NULLs the local pointer and drops the last reference on the floor. The refcount is left at 0 with no owner, and both `bb->buffer` (a full copy of the file revision) and `bb` itself are leaked.

## Failure scenario
`cvs checkout` / `cvs update` / `cvs export` of a file whose deltas have `deltatype binary` or `compressed_binary`. Walk `RCS_deltas` (rcs.cpp:5745) to the head:

1. `linevector_add(&curlines, value, vallen, NULL, 0, 1)` allocates `curlines.binary.bb` with `refcount = 1`.
2. `linevector_copy(&headlines, &curlines)` (rcs.cpp:5938) raises it to 2 and shares it.
3. Cleanup at rcs.cpp:6133-6136:
   * `linevector_free(&curlines)`: `!2--` → false → pointer NULLed, refcount now 1.
   * `linevector_free(&headlines)`: `!1--` → false → pointer NULLed, refcount now 0, **buffer and struct leaked**.
   * `linevector_free(&binbuf)`: the scratch patch buffer (also refcount 1) is **leaked** the same way.

So each `RCS_deltas` call on a binary file leaks roughly two times the file size. Checking out a module with many large binaries, or a long-lived `cvs server` process handling many requests, grows RSS by the total size of every binary revision materialised. This is exactly the workload this fork is built for (large binary game assets).

## Suggested fix
```cpp
		if(vec->binary.bb && !--vec->binary.bb->refcount)
```
(and then the `else` branch's `vec->binary.bb=NULL;` should be hoisted out so the pointer is cleared in both cases).

## Refutation attempt
* *Maybe the intent is that refcount is 0-based (0 == one owner)?* No. `RCS_deltas` at rcs.cpp:5913 tests `binbuf.binary.bb->refcount>1` to decide whether the buffer is shared, and `linevector_copy` does `from->binary.bb->refcount++` to add a reference — both only make sense with a 1-based count. The text branch in the same function also uses 1-based counting.
* *Maybe some other code frees `bb`?* `xfree` on `bb`/`bb->buffer` appears nowhere else in the file; `grep -n "binary.bb" rcs.cpp` shows all uses, and no other site frees them.
* *Could this be a double-free instead of a leak (i.e. is the condition ever true)?* Only if `refcount` were 0 on entry, which cannot happen while the pointer is still reachable — every path that decrements to 0 also NULLs the local pointer. So the failure mode is strictly a leak, not a double-free.
* *Is the binary delta path actually used?* Yes — `rcs_checkin.cpp` emits `deltatype binary` / `compressed_binary`, and `RCS_deltas` (rcs.cpp:5894, 5905-5923) has the dedicated read path.
