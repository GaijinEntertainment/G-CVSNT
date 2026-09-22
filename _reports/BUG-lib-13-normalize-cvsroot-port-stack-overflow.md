---
id: BUG-lib-13
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/root.cpp
line: 1059
severity: high
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: no
---

# `normalize_cvsroot()` `strcpy`s the CVSROOT port into a 64-byte stack buffer, and `sprintf`s a non-literal format string into it

## Summary
`normalize_cvsroot()` copies `root->port` into `char port_s[64]` with an unbounded `strcpy`. The
port is taken verbatim from the CVSROOT string; both parsers that produce it accept an
arbitrarily long run of digits (leading zeros pass every validity check), so a long port smashes
the stack frame. The `else` branch on the line above uses `sprintf(port_s, <non-literal>)`,
which is a format-string construct.

## Code
```cpp
// src/root.cpp:1048-1060
char *normalize_cvsroot (const cvsroot *root)
{
    char *cvsroot_canonical;
    char *p, *hostname;
    const char *username;
    char port_s[64];

    /* get the appropriate port string */
	if(!root->port)
		sprintf (port_s, get_default_client_port(client_protocol));   // <-- non-literal format
	else
		strcpy(port_s,root->port);                                    // <-- line 1059, unbounded
```

## Why it is a bug
Neither of the two places that set `root->port` bounds its length:

```cpp
// src/root.cpp:421-425 — the ";port=" keyword form
	else if(!strcasecmp(keyword,"port"))
	{
		if(*value)
			newroot->port = xstrdup(value);        /* value is char value[256] */
		if(newroot->port)
		{
			char *q = value;
			while (*q) { if (!isdigit(*q++)) { ...error...; return -1; } }
		}
	}

// src/root.cpp:770-796 — the ":host:port/path" form
				while (*q && !(*q==':' && !*(q+1)) && !(isalpha(*q) && *(q+1)==':' && !*(q+2)))
					if (!isdigit(*q++)) { ...error...; goto error_exit; }
				*q='\0';
				if (atoi(p) <= 0) { ...error...; goto error_exit; }
				newroot->port = xstrdup(p);
```

Both checks are "every character is a digit" plus, in the second, "`atoi()` is positive". Neither
is a length check, and neither rejects leading zeros — `atoi("000…0002401")` is `2401`, so a
60-, 200- or 2000-character port sails through.

`port_s` is 64 bytes on the stack of `normalize_cvsroot`, directly below `cvsroot_canonical`,
`p`, `hostname`, `username` and the saved return address.

## Failure scenario
`cvs login` calls `normalize_cvsroot(current_parsed_root)` (src/login.cpp:56) before anything
else touches the port; `cvs logout` does the same at src/login.cpp:116.

```
cvs -d ":pserver;port=000000000000000000000000000000000000000000000000000000000000000000002401:me@host:/repo" login
```

1. `parse_keyword("port", …)` (src/root.cpp:421) copies the 70-digit value out of the 256-byte
   `value` buffer; every character is a digit, so the validation loop passes and
   `newroot->port` becomes a 70-character string.
2. `login()` calls `normalize_cvsroot()`.
3. `strcpy(port_s, root->port)` writes **71 bytes into a 64-byte stack array**, overwriting
   `hostname`/`username`/the saved frame. With a `port=` value up to the 255-byte `value` limit
   this is a 192-byte overwrite of fully attacker-chosen bytes (restricted to ASCII digits).
   The `:host:port/path` form (src/root.cpp:796) has no 256-byte cap at all, so the overwrite
   length there is bounded only by the length of the CVSROOT string.

CVSROOT reaches this code from `-d`, from `$CVSROOT`, and from a sandbox `CVS/Root` file
(`Name_Root()`, src/root.cpp:78) — so checking out a hostile tarball that ships its own `CVS/Root`
and then running `cvs login` in it is enough.

Secondary defect on line 1057: `sprintf(port_s, get_default_client_port(...))` passes a runtime
string as the format. `get_default_client_port()` (src/root.cpp:1023-1035) currently returns
either `"2401"` or a `"%u"`-formatted number, so no `%` reaches the format today — but it is a
`-Wformat-security` violation one edit away from being exploitable, and it also means the
`/etc/services` value is copied into `port_s` with no length check (the static buffer there is
`char p[32]`, so that one is safe by construction).

## Suggested fix
```cpp
    char port_s[64];

    /* get the appropriate port string */
	if(!root->port)
		snprintf (port_s, sizeof(port_s), "%s", get_default_client_port(client_protocol));
	else
	{
		strncpy(port_s,root->port,sizeof(port_s)-1);
		port_s[sizeof(port_s)-1]='\0';
	}
```
(better still, reject ports longer than 5 digits in the two parsers).

## Refutation attempt
- Checked whether the digit validation implicitly bounds the length — it does not; it is a
  character-class test only, run character by character with no counter.
- Checked whether `atoi(p) <= 0` rejects long inputs: `atoi` of a long run of zeros followed by a
  valid port returns that port. (Even a value that overflows `int` is only *undefined*, not
  guaranteed to be `<= 0`.)
- Checked whether `normalize_cvsroot` might be unreachable for a remote root with a port: its two
  callers are `login()` and `logout()`, both of which run precisely on `:pserver:`-style roots
  where a port is normal.
- Checked whether some earlier sanity check caps the port: src/root.cpp:979 and :999 only test
  whether a port is *present* against `client_protocol->required_elements`/`valid_elements`.
- Confirmed `port_s` is a stack array, not a heap block: `char port_s[64];` at src/root.cpp:1053.
