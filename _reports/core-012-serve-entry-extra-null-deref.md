# serve_entry_extra dereferences NULL when EntryExtra arrives before any Entry

- **File:** cvsnt/cvsnt-2.5.05.3744/src/server.cpp
- **Line(s):** 2370-2376
- **Severity:** medium
- **Confidence:** high
- **Category:** memory / security (remote DoS)

## Code
```cpp
/* This must be sent directly after the Entry line above to work properly */
static void serve_entry_extra(char *arg)
{
    struct an_entry *p = entries;

	xfree(p->entry_extra);        // <-- p is NULL if no Entry was sent first
	p->entry_extra = xstrdup(arg);
}
```

## Why this is a bug
`entries` is the global head of the entry list, populated by `serve_entry`/`serve_is_modified`. `serve_entry_extra` assumes an `Entry` request was received immediately before, but performs no NULL check. The `EntryExtra` request is registered as a normal, non-essential request (server.cpp:4925), so a client — or a malformed/malicious one — can send `EntryExtra` as the very first entry-related request in a directory. `entries` is then NULL and `p->entry_extra` dereferences a NULL pointer, crashing the server process for that connection.

Every other `serve_*` handler in this file carefully checks its allocations and returns via `error()`; this one was written assuming well-behaved ordering. Because the server is reachable by any authenticated (and in anonymous-readonly setups, effectively unauthenticated) client, this is a trivially triggerable remote crash.

## Suggested fix
```cpp
static void serve_entry_extra(char *arg)
{
    struct an_entry *p = entries;
    if (p == NULL)
    {
        error (1, 0, "protocol error: EntryExtra without preceding Entry");
        return;
    }
    xfree(p->entry_extra);
    p->entry_extra = xstrdup(arg);
}
```
