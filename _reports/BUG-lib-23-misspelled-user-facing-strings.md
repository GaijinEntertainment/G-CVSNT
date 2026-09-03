---
id: BUG-lib-23
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/buffer.cpp
line: 1567
severity: low
category: message
verdict: CONFIRMED
fix_size_loc: 10
behavior_change: no
---

# Misspelled user-facing strings: "recieved", "Depreciated", "Eraseing", "FindPrototocol"

## Summary
Ten literal strings in the audited scope are misspelled. One of them
(`src/buffer.cpp:1567`) is a fatal `error(1,0,…)` message shown directly to the user; the rest are
trace/diagnostic output that ends up in server trace files and support tickets.

## Code
```cpp
// src/buffer.cpp:1566-1567 — fatal error shown to the user
	if (tcount > count)
		error (1, 0, "Input failure: data packet recieved is too short");
                                          /* recieved -> received */

// src/root.cpp:406,412,417,423,443,519 — "Depreciated" should be "Deprecated"
		TRACE(1,"Depreciated keyword 'username' used");
		TRACE(1,"Depreciated keyword 'password' used");
		TRACE(1,"Depreciated keyword 'hostname' used");
		TRACE(1,"Depreciated keyword 'port' used");
		TRACE(1,"Depreciated keyword 'directory' used");
		TRACE(1,"Depreciated cvsroot format [...] used.\n");

// cvstools/ProtocolLibrary.cpp:262 — "Eraseing" should be "Erasing"
				CServerIo::trace(3,"Eraseing %s",protocolname);

// cvstools/ProtocolLibrary.cpp:287,358 — "Prototocol" should be "Protocol"
	CServerIo::trace(3,"FindPrototocol(%s)",tagline?tagline:"");
			CServerIo::trace(3,"EnumeratePrototocols failed");
```

## Why it is a bug
"recieved", "Depreciated", "Eraseing" and "Prototocol" are not words. Beyond presentation, the
`Prototocol` spellings break grep-ability: the trace line does not match the function name
`FindProtocol`/`EnumerateProtocols` it is reporting on, so searching a trace file for the function
being diagnosed does not find its own log line.

"Depreciate" (to reduce in value) and "deprecate" (to mark as obsolete) are different words; the
intended meaning here is clearly the latter — these messages fire when a legacy CVSROOT keyword
form is used (src/root.cpp:404-444) and when the obsolete `[key=value,…]` root syntax is parsed
(src/root.cpp:519).

## Failure scenario
`error(1,0,"Input failure: data packet recieved is too short")` at src/buffer.cpp:1567 is a
fatal, user-visible message printed by `packetizing_buffer_input()` whenever a compressed or
encrypted packet's translated length exceeds its raw length — i.e. exactly the message an
administrator sees and searches for when a `-z` or `:sserver:` session fails. The remaining nine
appear in `cvs -t -t -t` output and in the server trace file selected by the
`ServerTraceFile` setting (src/main.cpp:1084).

## Suggested fix
```cpp
		error (1, 0, "Input failure: data packet received is too short");
...
		TRACE(1,"Deprecated keyword 'username' used");
		TRACE(1,"Deprecated keyword 'password' used");
		TRACE(1,"Deprecated keyword 'hostname' used");
		TRACE(1,"Deprecated keyword 'port' used");
		TRACE(1,"Deprecated keyword 'directory' used");
		TRACE(1,"Deprecated cvsroot format [...] used.\n");
...
				CServerIo::trace(3,"Erasing %s",protocolname);
...
	CServerIo::trace(3,"FindProtocol(%s)",tagline?tagline:"");
			CServerIo::trace(3,"EnumerateProtocols failed");
```

## Refutation attempt
- Checked that these are literal output strings rather than identifiers or protocol tokens whose
  spelling is fixed by the wire format: all ten are `error()`/`TRACE()`/`CServerIo::trace()`
  format strings, none is compared against or sent over the protocol.
- Checked `src/root.cpp:518` — `/* Comma separated cvsroot, depreciated. */` is a comment, not
  output, so it is excluded from the fix list above (though it has the same error).
- Restricted the list to the files in this audit's scope; identical misspellings elsewhere in the
  tree (e.g. doc/cvs.dbk:2776 "seperated") are out of scope and not listed.
