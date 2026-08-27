---
# ext_disconnect resets current_in twice, leaving current_out as a stale/closed fd
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/ext.cpp
- **Line(s):** 185-189
- **Severity:** medium
- **Confidence:** high
- **Category:** typo

## Code
```cpp
int ext_disconnect(const struct protocol_interface *protocol)
{
    if(current_in>0)
    {
        close(current_in);
        current_in=-1;
    }
    if(current_out>0)
    {
        close(current_out);
        current_in=-1;        // <-- should be current_out=-1
    }
    return CVSPROTO_SUCCESS;
}
```

## Why this is a bug
After closing `current_out`, the code assigns `current_in=-1` a second time instead of
`current_out=-1`. `current_out` (a module-level `static`) therefore retains the value of
the file descriptor that was just closed.

On a subsequent `ext_disconnect` (or a reconnect that leaves the stale value in place),
`if(current_out>0)` is still true, so `close(current_out)` runs again on an
already-closed descriptor. If that descriptor number has since been reused by another
`open`/`socket`/pipe in the process, this closes an unrelated, live fd — leading to
mysterious I/O failures or, in a server context, closing a descriptor belonging to
another operation. This is a classic double-close / fd-confusion defect.

The identical typo exists in fork.cpp (fork_disconnect) — see api-007.

## Suggested fix
```cpp
    if(current_out>0)
    {
        close(current_out);
        current_out=-1;
    }
```
---
