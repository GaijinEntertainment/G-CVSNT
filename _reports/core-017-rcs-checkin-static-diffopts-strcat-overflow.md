# RCS_checkin strcats encoding onto a static diffopts buffer every call — accumulates and overflows

- **File:** cvsnt/cvsnt-2.5.05.3744/src/rcs_checkin.cpp
- **Line(s):** 1022-1029 (context-diff path) and 1247-1254 (unified-diff path)
- **Severity:** high
- **Confidence:** high
- **Category:** overflow

## Code
```cpp
	else if(kf.flags&KFLAG_ENCODED && !server_active)
	{
		static char __diffopts[64] = "-a -n --binary-output --encoding=";
		strcat(__diffopts,kf.encoding.encoding);      // <-- appends to a *static* buffer
		if(kf.encoding.bom)
			strcat(__diffopts," --bom");
		diffopts = __diffopts;
	}
```
and the near-identical block at 1247-1254:
```cpp
		else if(kf.flags&KFLAG_ENCODED && !server_active)
		{
			static char __diffopts[64] = "-a -u --binary-output --encoding=";
			strcat(__diffopts,kf.encoding.encoding);
			if(kf.encoding.bom)
				strcat(__diffopts," --bom");
			diffopts = __diffopts;
		}
```

## Why this is a bug
`__diffopts` is a **static** buffer initialized once to the ~33-char base string. Each time this branch runs, `strcat` appends the file's encoding name (and optionally `" --bom"`) to whatever the buffer already holds from the previous call. The base string is never restored, so the appended text accumulates:

- 1st encoded file: `"...--encoding=UTF-8"`
- 2nd encoded file: `"...--encoding=UTF-8UTF-8"`
- 3rd: `"...--encoding=UTF-8UTF-8UTF-8"` ...

Two failure modes:
1. **Wrong diff options** from the second encoded file onward — the `--encoding=` value becomes a concatenation of every prior encoding, so `diff_exec` is invoked with a garbage encoding argument, corrupting the delta/diff produced for a committed unicode file.
2. **Static buffer overflow**: the 64-byte buffer overruns after a few accumulations (base 33 + repeated encodings), a classic out-of-bounds write into adjacent static storage.

`RCS_checkin` is called once per file, so committing several `-k`-encoded (unicode) files in a single `cvs commit` against a **local** repository (`server_active` is false — this branch is guarded by `!server_active`) reaches both modes within one process. The same static is also shared between the two blocks' respective buffers (each block has its own static, but each accumulates independently).

## Suggested fix
Rebuild the string from scratch each call into a local (non-static) buffer with a bounds-checked format, e.g.:
```cpp
	char diffbuf[128];
	snprintf(diffbuf, sizeof diffbuf, "-a -n --binary-output --encoding=%s%s",
	         kf.encoding.encoding, kf.encoding.bom ? " --bom" : "");
	diffopts = diffbuf;   /* ensure lifetime spans the diff_exec call */
```
(or `strcpy` the base into the static before each `strcat`, and enlarge the buffer). Apply to both locations.
