---
# fork_disconnect resets current_in twice, leaving current_out as a stale/closed fd
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/fork.cpp
- **Line(s):** 146-150
- **Severity:** medium
- **Confidence:** high
- **Category:** typo

## Code
```cpp
int fork_disconnect(const struct protocol_interface *protocol)
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
Same copy-paste defect as ext.cpp (api-006). After `close(current_out)` the code sets
`current_in=-1` again instead of `current_out=-1`, so the module-level `static
current_out` keeps the already-closed descriptor value.

A later `fork_disconnect` then sees `current_out>0` and calls `close()` on the stale
descriptor a second time. If that fd number has been reused in the meantime, an
unrelated live descriptor is closed, causing hard-to-diagnose I/O errors. Double-close
of a descriptor is undefined/dangerous.

## Suggested fix
```cpp
    if(current_out>0)
    {
        close(current_out);
        current_out=-1;
    }
```
---
