---
# nt_setuid: user-rights loop always reads lsaUserRights[0] instead of [n] when building token privileges
- **File:** cvsnt/cvsnt-2.5.05.3744/windows-NT/setuid.cpp
- **Line(s):** 350-355 (bug at 353)
- **Severity:** medium
- **Confidence:** high
- **Category:** typo

## Code
```cpp
if(LsaEnumerateAccountRights(hLsa,UserSid,&lsaUserRights,&NumUserRights)==ERROR_SUCCESS)
{
    TokenPrivs->PrivilegeCount=NumUserRights;
    TokenPrivs=(PTOKEN_PRIVILEGES)realloc(TokenPrivs,sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)*TokenPrivs->PrivilegeCount);

    for(n=0,j=0; n<(int)NumUserRights; n++)
    {
        TokenPrivs->Privileges[j].Attributes=SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
        LookupPrivilegeValueW(wszMachine,lsaUserRights->Buffer,&TokenPrivs->Privileges[j].Luid);  // <-- lsaUserRights[0]
        j++;
    }
    NetApiBufferFree(lsaUserRights);
}
```

## Why this is a bug
`LsaEnumerateAccountRights` returns an array of `LSA_UNICODE_STRING` in `lsaUserRights`
with `NumUserRights` elements. The loop iterates `n` over that array but always
dereferences `lsaUserRights->Buffer`, i.e. `lsaUserRights[0].Buffer`, so it looks up the
LUID of the *first* privilege name on every iteration. The result is that
`TokenPrivs->Privileges[0..NumUserRights-1]` are all filled with the LUID of the first
directly-assigned right, and every other right the account holds is dropped.

The correct index is used just below, in the group-rights loop (line 369 uses
`lsaUserRights[p].Buffer`), confirming the intended form.

This code builds the privilege set handed to `NtCreateToken` for the impersonation token
(`nt_setuid`). The defect means the impersonated user's token does not carry the
privileges actually granted to that account — a security-relevant correctness bug in
token construction (privileges silently altered/lost; duplicate LUIDs may also make
`NtCreateToken` behave unexpectedly). The `LookupPrivilegeValueW` return value is also
unchecked here (contrast line 373), so a lookup failure leaves the `Luid` field
whatever the reallocated (uninitialized) memory held.

## Suggested fix
```cpp
for(n=0,j=0; n<(int)NumUserRights; n++)
{
    LUID luid;
    if(!LookupPrivilegeValueW(wszMachine,lsaUserRights[n].Buffer,&luid))
        continue;
    TokenPrivs->Privileges[j].Attributes=SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
    TokenPrivs->Privileges[j].Luid=luid;
    j++;
}
```
---
