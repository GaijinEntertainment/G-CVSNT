---
id: BUG-server-03
area: rcs
file: cvsnt/cvsnt-2.5.05.3744/src/rcs_checkin.cpp
line: 1165
severity: high
category: correctness
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `-kz` compressed delta length is set to `deflateBound()` instead of `stream.total_out`, writing uninitialized heap into the RCS file

## Summary
When re-compressing the previous head's change text for a `-kz` (compressed-delta) file, `RCS_checkin` stores the *allocation upper bound* returned by `deflateBound()` as the delta length instead of the number of bytes deflate actually produced. The tail of the `xmalloc`'d buffer is never written, so `deflateBound(len) - total_out` bytes of uninitialized heap are `expand_at_signs()`-escaped into the repository's RCS file.

## Code
```cpp
// rcs_checkin.cpp:1143-1166
		if((kf.flags & (KFLAG_BINARY_DELTA|KFLAG_COMPRESS_DELTA)) == KFLAG_COMPRESS_DELTA)
		{
			/* We need to compress the delta here, because it won't be compressed by RCS_rewrite */
			uLong zlen;
			void *zbuf;

			z_stream stream = {0};
			deflateInit(&stream,Z_DEFAULT_COMPRESSION);
			zlen = deflateBound(&stream, commitpt->text->len);
			stream.avail_in = commitpt->text->len;
			stream.next_in = (Bytef*)commitpt->text->text;
			stream.avail_out = zlen;
			zbuf = xmalloc(zlen+4);
			stream.next_out = (Bytef*)((char*)zbuf)+4;
			*(unsigned long *)zbuf=htonl(commitpt->text->len);
			if(deflate(&stream, Z_FINISH)!=Z_STREAM_END)
			{
				error(1,0,"internal error: deflate failed");
			}
			deflateEnd(&stream);
			xfree(commitpt->text->text);
			commitpt->text->text = (char*)zbuf;
			commitpt->text->len = zlen+4;          // <-- BUG: zlen is deflateBound(), not the compressed size
		}
```

The two other copies of this identical compression block get it right:

```cpp
// rcs.cpp:6788  (putdeltatext)
			expand_at_signs ((const char *)zbuf, stream.total_out+4, fp);
// import.cpp:1574
				expand_at_signs ((const char *)zbuf, stream.total_out+4, fprcs);
```

## Why it is a bug
`deflateBound()` returns a conservative *upper bound* on the compressed size — roughly `len + len/16 + 64 + 5`. `stream.total_out` is the number of bytes deflate actually wrote. `xmalloc` does not zero its result, so bytes `zbuf[4 + total_out .. 4 + zlen - 1]` hold whatever was previously in that heap block.

`commitpt->text` is the `Deltatext` hung off the `RCSVers` for the previous head. `RCS_rewrite` → `RCS_copydeltas` (rcs.cpp:6869-6874) transplants it into the outgoing delta:

```cpp
	    if (dadmin->text->text != NULL)
	    {
		dtext->text = dadmin->text->text;
		dtext->len  = dadmin->text->len;
		dadmin->text->text = NULL;
	    }
	}
	putdeltatext (fout, dtext, 0);
```

and `putdeltatext` with `compress == 0` does `expand_at_signs (d->text, d->len, fp)` — writing all `zlen + 4` bytes verbatim into the new RCS file.

Two consequences:
1. **Information disclosure into the repository.** Up to ~6% of the diff size (plus 69 bytes) of uninitialized server heap is committed into the RCS file and is subsequently served to every client that checks out that revision. For a server process this heap can contain other users' file contents, log messages, passwords read during authentication, or `.cvspass`-style material.
2. **Repository bloat.** Every `-kz` revision permanently carries the padding.

Decompression on read still succeeds — `RCS_deltas` (rcs.cpp:5850-5875) sets `avail_out = zlen` (the *uncompressed* length stored in the 4-byte header) and `inflate(..., Z_FINISH)` returns `Z_STREAM_END` as soon as it reaches the end of the deflate stream, ignoring the trailing garbage — which is exactly why this has gone unnoticed.

## Failure scenario
```
cvs add -kz bigfile.txt          # KFLAG_COMPRESS_DELTA, no KFLAG_BINARY_DELTA
cvs commit -m "rev 1" bigfile.txt
# edit bigfile.txt
cvs commit -m "rev 2" bigfile.txt
```
On the second commit `commitpt` is rev 1.1 (the old head), `commitpt->text->text` is the RCS-format diff read from `changefile` (rcs_checkin.cpp:1085). Say that diff is 1 MB; `deflateBound` returns ~1 062 000 and the diff compresses to ~50 KB. `commitpt->text->len` is then set to ~1 062 004 and `putdeltatext` writes ~1 012 000 bytes of uninitialized heap (@-escaped) into `bigfile.txt,v`. `cvs log`/`cvs co -r 1.1` still work; the garbage is invisible but permanent, and `strings bigfile.txt,v` on the server exposes it.

## Suggested fix
```cpp
			commitpt->text->len = stream.total_out+4;
```
(and, for tidiness, move the `deflateEnd(&stream);` call after this line, or capture `total_out` into a local before `deflateEnd`).

## Refutation attempt
* *Maybe `deflateBound` == `total_out` in practice?* No; `deflateBound` is documented as an upper bound for the *worst case* (incompressible input plus header/trailer overhead). Real deltas are text and compress well, so the gap is large.
* *Maybe some later step trims the buffer to the real length?* No. `commitpt->text->len` is only read again in `RCS_copydeltas` (`dtext->len = dadmin->text->len`) and then passed straight to `expand_at_signs`. `freedeltatext` only frees.
* *Maybe `xmalloc` zeroes memory?* It does not — `lib/xmalloc.c`'s `xmalloc` is a checked `malloc` wrapper; only `xcalloc`/explicit `memset` zero.
* *Maybe the branch is unreachable?* `-kz` is a documented CVSNT kflag: rcs.cpp:68 registers `{ 'z', ..., KFLAG_COMPRESS_DELTA, KFLAG_CVSNT, 0 }`, and rcs_checkin.cpp:1364 passes the same predicate to `RCS_rewrite`. `cvs add -kz` / `cvs import -kz` / `cvs admin -kz` all reach it.
* *Does the extra data break readback (making this "just" a crash)?* No — verified against `RCS_deltas` rcs.cpp:5856-5872, which bounds `avail_out` by the stored uncompressed length and accepts `Z_STREAM_END`. The corruption is silent, which makes it worse, not better.
