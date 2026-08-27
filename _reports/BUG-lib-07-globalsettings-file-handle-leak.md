---
id: BUG-lib-07
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvstools/unix/GlobalSettings.cpp
line: 154
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `CGlobalSettings::_GetUserValue()` and `GetGlobalValue()` leak the config `FILE*` on every successful lookup

## Summary
Both functions `return 0` from inside the `fgets` loop as soon as the key is found, skipping the
`fclose(f)` that only sits on the not-found path. Every *successful* setting lookup — the common
case — leaks one `FILE*` (an fd plus a stdio buffer). `EnumUserValues()`/`EnumGlobalValues()`
in the same file get this right, which shows the omission is accidental.

## Code
```cpp
// cvstools/unix/GlobalSettings.cpp:142-158  (_GetUserValue; GetGlobalValue at 339-355 is identical)
    while(fgets(line,sizeof(line),f))
    {
    	line[strlen(line)-1]='\0';
          	p=strchr(line,'=');
        if(p)
            *p='\0';
        if(!strcasecmp(value,line))
        {
	        if(p)
                strncpy(buffer,p+1,buffer_len);
        	else
          		*buffer='\0';
       		return 0;            // <-- line 154: f is never closed
       	}
	}
    fclose(f);                   // <-- only reached when the key is NOT found
    return -1;
```

Contrast with `EnumUserValues()` (line 279-284) and `EnumGlobalValues()` (line 469-474), which
do `fclose(f); return 0;` on their early-success path.

## Why it is a bug
`f` is a local; nothing else can close it. The two functions are the only readers of
`~/.cvs/<key>` and `<confdir>/<key>` respectively, and they are the workhorses of the whole
settings API: the `int&` and `cvs::string&` overloads (lines 105-123, 302-321) both funnel into
them, so *every* typed accessor leaks too.

## Failure scenario
`read_global_config()` in src/main.cpp:513-649 calls `CGlobalSettings::GetGlobalValue()` about
twenty times in a row (`Compat0_OldVersion`, `Compat0_OldCheckout`, … `LockServer`,
`LibraryDir`, `Locale`, `ReadOnlyServer`, `AllowedClients`, `EncryptionLevel`,
`CompressionLevel`, `Chroot`, `RunAsUser`, `AllowTrace`, `RemoteInitRoot`), plus one more per
configured repository inside the `EnumGlobalValues` loop at line 613-648
(`Repository%dName`, `Repository%dRemoteServer`, `Repository%dRemoteRepository`,
`Repository%dProxyPhysicalFiles`, `Repository%dRemotePassphrase` — five string lookups per
repository).

Every one of those that finds its key leaks an open `FILE*` on `<confdir>/PServer`. With `N`
configured repositories that is roughly `20 + 5N` leaked descriptors per call.

In the short-lived `cvs` client this merely wastes memory, but `cvsapi`/`cvstools` is linked
into long-running processes — `cvsservice`, `cvsagent`, `lockservice` — and the settings API is
re-read rather than cached. Once the process hits `RLIMIT_NOFILE` every subsequent
`fopen`/`socket`/`accept` fails, and since `GetGlobalValue()` returns `-1` when it cannot open
the file, the server silently falls back to *default* settings — including
`read_only_server`, `encryption_level` and `allowed_clients` — rather than reporting an error.
A settings-dependent security posture that silently reverts to the built-in default on fd
exhaustion is the part that makes this more than a tidiness issue.

## Suggested fix
```cpp
        if(!strcasecmp(value,line))
        {
	        if(p)
                strncpy(buffer,p+1,buffer_len);
        	else
          		*buffer='\0';
                fclose(f);
       		return 0;
       	}
```
(apply the same two-line change at cvstools/unix/GlobalSettings.cpp:154 and :351)

## Refutation attempt
- Checked for an RAII wrapper or an `atexit`-style registry that might close `f`: `f` is a bare
  `FILE *` local, and neither function stores it anywhere.
- Checked whether the caller could close it: the signature returns `int`, not the handle.
- Checked the sibling functions in the same file to make sure this is not a deliberate
  convention: `EnumUserValues` (line 283) and `EnumGlobalValues` (line 473) both call
  `fclose(f)` before their in-loop `return 0`, so the convention is clearly to close.
- Checked the win32 counterpart (cvstools/win32/GlobalSettings.cpp) — it uses the registry via
  `RegOpenKeyEx`/`RegCloseKey` and does not have this shape, so this is a unix-only divergence.
