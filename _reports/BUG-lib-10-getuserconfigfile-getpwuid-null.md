---
id: BUG-lib-10
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvstools/unix/GlobalSettings.cpp
line: 80
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: no
---

# `GetUserConfigFile()` dereferences the result of `getpwuid()` without checking it for NULL

## Summary
`getpwuid()` returns NULL when the calling uid has no passwd entry. The code checks
`pw->pw_dir` for NULL but never checks `pw` itself, so the very expression that performs the
check crashes. The rest of the tree guards this call correctly (src/subr.cpp:472,
src/filesubr.cpp:1295), so this is a local omission.

## Code
```cpp
// cvstools/unix/GlobalSettings.cpp:78-87
	void GetUserConfigFile(const char *product, const char *key, cvs::filename& fn)
	{
  		struct passwd *pw = getpwuid(getuid());

		if(!product || !strcmp(product,"cvsnt"))
			product = "cvs";

		cvs::sprintf(fn,80,"%s/.%s",pw->pw_dir?pw->pw_dir:"",product);      // <-- pw may be NULL
  		mkdir(fn.c_str(),0777);
		cvs::sprintf(fn,80,"%s/.cvs/%s",pw->pw_dir?pw->pw_dir:"",key?key:"config");
```

Compare src/subr.cpp:470-476, which handles exactly this case:
```c
    if ((pw = (struct passwd *) getpwuid (uid)) == NULL)
    { ... }
```

## Why it is a bug
`getpwuid()` is documented to return NULL both for "no matching entry" and for a lookup error,
with the two distinguished by `errno`. The `pw->pw_dir?pw->pw_dir:""` guard shows the author
was thinking about missing data, but applied it one level too deep — the dereference `pw->pw_dir`
happens before the ternary can help.

## Failure scenario
Run any cvs client/tool under a uid that has no passwd entry. This is routine, not exotic:

* inside a container started with `docker run --user 1234:1234` against an image whose
  `/etc/passwd` has no uid 1234;
* under a systemd unit with `DynamicUser=yes`;
* when NSS is misconfigured or the LDAP/SSSD backend is unreachable (in which case `getpwuid`
  returns NULL with `errno` set even for a uid that "exists").

The first settings lookup — `CGlobalSettings::GetUserValue("cvsnt","cvspass",…)` on the
`:pserver:` path — calls `_GetUserValue` -> `GetUserConfigFile` and the process segfaults at
line 84 before it can print any diagnostic. `DeleteUserKey()` (line 292) and
`EnumUserValues()` (line 247) reach the same function.

## Suggested fix
```cpp
	void GetUserConfigFile(const char *product, const char *key, cvs::filename& fn)
	{
  		struct passwd *pw = getpwuid(getuid());
		const char *home = (pw && pw->pw_dir) ? pw->pw_dir : "";

		if(!product || !strcmp(product,"cvsnt"))
			product = "cvs";

		cvs::sprintf(fn,80,"%s/.%s",home,product);
  		mkdir(fn.c_str(),0777);
		cvs::sprintf(fn,80,"%s/.cvs/%s",home,key?key:"config");
```

## Refutation attempt
- Checked whether some earlier initialisation guarantees a passwd entry exists: `GetUserConfigFile`
  is a file-static helper called directly from `_GetUserValue`, `_SetUserValue`,
  `EnumUserValues` and `DeleteUserKey`, none of which validate the uid first.
- Checked whether `$HOME` is consulted as a fallback anywhere on this path — it is not; only
  `pw->pw_dir` is used, so there is no alternate route that would make the NULL case unreachable.
- Confirmed the same file's win32 counterpart uses `HKEY_CURRENT_USER` and has no analogue, so
  this is a unix-only exposure.
- Noted in passing (not part of this finding) that the `mkdir` on line 85 creates
  `~/.<product>` while line 86 then reads `~/.cvs/<key>` — for any `product` other than NULL or
  `"cvsnt"` the directory created is not the directory used.
