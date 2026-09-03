---
id: BUG-server-04
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs.cpp
line: 649
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# Two `inflateInit()` calls with no matching `inflateEnd()` leak a full zlib inflate state per compressed revision

## Summary
`RCS_fully_parse()` and `RCS_checkout_raw_value()` each create a `z_stream`, call `inflateInit()`, inflate, and then drop the stream on the floor without `inflateEnd()`. In `RCS_fully_parse` this sits inside the per-revision loop, so `cvs log` on a `-kz` file leaks one zlib inflate state (~7 KB of state plus a 32 KB sliding window) for every revision in the file.

## Code
```cpp
// rcs.cpp:643-661  (inside RCS_fully_parse's "while (1) { ... while (rcsbuf_getkey(...)) }" loop)
						if(vnode->type && STREQ(vnode->type,"compressed_text"))
						{
							uLong zlen;

							z_stream stream = {0};
							inflateInit(&stream);
							zlen = ntohl(*(unsigned long *)value);
							if(zlen)
							{
								stream.avail_in = vallen-4;
								stream.next_in = (Bytef*)value+4;
								stream.avail_out = zlen;
								zbuf = (char*)xmalloc(zlen);
								stream.next_out = (Bytef*)zbuf;
								if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
								{
									error(1,0,"internal error: inflate failed");
								}
							}
							vallen=zlen;
							value = zbuf;
						}                                   // <-- no inflateEnd(&stream)
```

```cpp
// rcs.cpp:4371-4391  (RCS_checkout_raw_value, head-revision fast path)
      if(vers->type && (STREQ(vers->type,"compressed_text") || STREQ(vers->type,"compressed_binary")))
      {
      	uLong zlen;

      	z_stream stream = {0};
      	inflateInit(&stream);
      	zlen = ntohl(*(unsigned long *)value);
      	if(zlen)
      	{
          stream.avail_in = len-4;
          ...
          if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
          {
          	error(1,0,"internal error: inflate failed");
          }
      	  value = (char*)zbuf;
          free_value = 1;
      	}
      	len = zlen;
      }                                                     // <-- no inflateEnd(&stream)
```

The third, structurally identical block in the same file gets it right:

```cpp
// rcs.cpp:5859-5879  (RCS_deltas)
					inflateInit(&stream);
					...
					inflateEnd(&stream);
```

## Why it is a bug
`inflateInit()` allocates `stream.state` (an `inflate_state`, ~7 KB with its code tables) via the stream's allocator, and the first `inflate()` call additionally allocates the 32 KB output window. Only `inflateEnd()` releases them; the `z_stream` itself is a stack object, so once it goes out of scope those heap blocks are unreachable. `grep -n "inflateInit\|inflateEnd" *.cpp` confirms rcs.cpp:649 and rcs.cpp:4376 have no partner, while rcs.cpp:5859 pairs with 5879 and every other zlib user in the tree (zlib.cpp, filesubr.cpp, import.cpp) pairs correctly.

## Failure scenario
`cvs log` on a `-kz` file (log.cpp:924 calls `RCS_fully_parse`). `RCS_fully_parse` loops over every deltatext in the RCS file and enters the `compressed_text` block once per non-head revision. A file with 2 000 revisions leaks 2 000 inflate states — roughly 78 MB of resident memory in a single `cvs log` invocation, all held until the process exits. On a `cvs server` process serving a `cvs log` over a large module (`RCS_fully_parse` is invoked per file), this compounds across files in the same request and can drive the server into the OOM killer or into address-space exhaustion on 32-bit builds.

`RCS_checkout_raw_value` (rcs_checkin.cpp:129, rcs_checkin.cpp:1464) leaks one state per checked-out head revision, so a large `cvs commit` of many `-kz` files accumulates the same way.

## Suggested fix
```cpp
							vallen=zlen;
							value = zbuf;
							inflateEnd(&stream);
						}
```
and likewise before the closing brace of the block at rcs.cpp:4391:
```cpp
      	len = zlen;
      	inflateEnd(&stream);
      }
```

## Refutation attempt
* *Does `inflate()` with `Z_FINISH` implicitly release state?* No. zlib documents that `inflateEnd` is the only way to free the internal state; `inflate` returning `Z_STREAM_END` merely marks the stream complete.
* *Does the `z_stream = {0}` initialisation route allocation through something that gets cleaned up elsewhere?* No — zeroing `zalloc`/`zfree` makes zlib use its default `malloc`/`free`; nothing else in CVSNT tracks those blocks.
* *Is the leak bounded because the process is short-lived?* Not usefully: `RCS_fully_parse` leaks proportionally to the number of revisions *within one command*, so a single `cvs log` on a long-lived file can exhaust memory before the process would have exited. And the pserver/SSH server process handles a whole request (many files) before exiting.
* *Could `zlen == 0` make the block harmless?* No — `inflateInit` is called *before* the `if (zlen)` test, so the state is allocated unconditionally on every entry, even when the payload is empty.
