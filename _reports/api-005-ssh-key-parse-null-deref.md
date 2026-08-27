---
# ssh_connect: strchr result dereferenced before its NULL check when parsing stored key
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/ssh.cpp
- **Line(s):** 200-206
- **Severity:** low
- **Confidence:** high
- **Category:** memory

## Code
```cpp
if(!strncmp(crypt_password,"KEY;",4))
{
    key=strchr(crypt_password+4,';');
    if(!key || !*key)
    {
        /* Something wrong - ignore password */
        server_error(1,"No password or key set.  Try 'cvs login'\n");
    }
    *key++ = '\0';
    key=strchr(key,';');
    *key++ = '\0';                       // <-- line 201: deref BEFORE the check
    if(!key || !*key)                    // <-- line 202: check happens too late
    {
        /* Something wrong - ignore password */
        server_error(1,"No password or key set.  Try 'cvs login'\n");
    }
    version = crypt_password+4;
}
```

## Why this is a bug
The second `strchr` (line 200) can return `NULL` when the stored password value has a
`KEY;` prefix and a first `;` but no second `;` (e.g. a value like `"KEY;a;b"`, which has
exactly one `;` in the tail). Line 201 then executes `*key++ = '\0'` on that `NULL`
pointer, writing to address 0 and crashing, *before* the guard on lines 202-206 gets a
chance to run. The guard is therefore misordered/dead: by the time it runs, `key` has
already been dereferenced and incremented, so `!key` can never be true.

Contrast the first occurrence (lines 193-199), where the `if(!key || !*key)` check is
placed *before* the `*key++`. The second occurrence dropped that ordering.

The input comes from the local `cvspass` store, so this is not directly
attacker-controlled in normal use; it triggers on a corrupted, hand-edited, or
foreign-format stored key entry. Still a genuine NULL-pointer write.

(Also note both `server_error(1,...)` calls here only avoid the subsequent deref if
`server_error` with fatal=1 never returns; if it can return, line 199 has the same
exposure.)

## Suggested fix
Check the pointer before dereferencing, mirroring the first block:
```cpp
key=strchr(key,';');
if(!key || !*key)
{
    server_error(1,"No password or key set.  Try 'cvs login'\n");
    return CVSPROTO_FAIL;   /* or otherwise stop */
}
*key++ = '\0';
```
and make the first block `return`/stop after `server_error` rather than falling through
to `*key++`.
---
