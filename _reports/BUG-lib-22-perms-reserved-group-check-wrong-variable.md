---
id: BUG-lib-22
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/perms.cpp
line: 102
severity: high
category: security
verdict: CONFIRMED
fix_size_loc: 10
behavior_change: yes
---

# The guard against redefining the reserved groups `admin` and `owner` tests the group *member* name instead of the *group* name, so `CVSROOT/group` can silently redefine both

## Summary
`get_valid_groups()` parses `CVSROOT/group` as `groupname:member,member,…`. Inside the loop over
*members* it checks each `name` against `"admin"` and `"owner"` and prints a message about "the
group". The variable holding the group name is `group`, not `name`, so a line
`owner: alice` or `admin: alice` passes with no warning and calls `add_valid_group("owner")` /
`add_valid_group("admin")` — exactly what the check exists to prevent. Symmetrically, a real
*user* called `owner` is silently excluded from every group.

## Code
```cpp
// src/perms.cpp:91-116
		while (getline (&linebuf, &linebuf_len, fp) >= 0)
		{
			group = cvs_strtok(linebuf, ":\n");        /* the GROUP name  */
			if (group == NULL)
				continue;
			names = cvs_strtok(NULL, ":\n");           /* the member list */
			if (names == NULL)
				continue;

			name = cvs_strtok(names, ", \t");          /* each MEMBER     */
			for(;name != NULL; name = cvs_strtok(NULL, ", \t"))
			{
				if(!strcasecmp(name,"admin"))                        // <-- line 102: should be group
				{
					error(0,0,"The group 'admin' is automatically assigned to repository administrators");
				}
				if(!strcasecmp(name,"owner"))                        // <-- line 106: should be group
				{
					error(0,0,"The group 'owner' is automatically assigned to directory owners");
					continue;
				}
				if (!usercmp (CVS_Username, name))
				{
					add_valid_group(group);
					break;
				}
			}
```

## Why it is a bug
The message text names the entity being rejected: "The group 'admin' …", "The group 'owner' …".
The group name in this parse is `group` (the token before the `:`), as the documented format
confirms — doc/cvs.dbk:2775-2781:

> the first is the group name, the second is a list of group members
> `group1: user1 user2 user3`

and doc/cvs.dbk:2784-2785 states the intent explicitly:

> Repository administrators are automatically made a member of the group 'admin'. Don't list this
> group in the group file.

`name`, the variable actually tested, is a *member* — so the guard fires on the wrong entity in
both directions. The `admin` branch additionally lacks the `continue` that the `owner` branch has,
so even for the case it does catch it only warns.

Both names are load-bearing in ACL evaluation:

```cpp
// src/perms.cpp:346-353 (verify_acl)
		if(((!val_user || (!strcmp(val_user,"owner") && verify_owner_acl(acl)) || verify_valid_name(val_user))) && ...
			bool isUser = val_user && CVS_Username && !usercmp(CVS_Username,val_user);
			if(val_user && !strcmp(val_user,"owner")) isUser|=verify_owner_acl(acl);
```
and
```cpp
// src/perms.cpp:160-166
static bool verify_valid_name(const char *name)
{
	if(CVS_Username && !usercmp(name,CVS_Username))
		return true;
	return valid_groups.find(name)!=valid_groups.end();
}
```
`admin` is otherwise only ever added by `if(verify_admin()) add_valid_group("admin");`
(src/perms.cpp:137-138), i.e. after checking `CVSROOT/admin`.

## Failure scenario
Add one line to `CVSROOT/group`:

```
owner: mallory
```

1. `group` = `"owner"`, `names` = `" mallory"`, `name` = `"mallory"`.
2. Line 102: `strcasecmp("mallory","admin")` != 0 — no message.
3. Line 106: `strcasecmp("mallory","owner")` != 0 — no message, no `continue`.
4. Line 111: `usercmp(CVS_Username,"mallory")` == 0 for mallory, so
   `add_valid_group("owner")` runs — `valid_groups` now contains the reserved name `owner`.
5. On any subsequent ACL check, `verify_acl()` evaluates
   `(!strcmp(val_user,"owner") && verify_owner_acl(acl)) || verify_valid_name(val_user)`
   for every `<acl user="owner">` entry. Even where `verify_owner_acl()` correctly says mallory is
   **not** the directory owner, the second disjunct `verify_valid_name("owner")` finds `owner` in
   `valid_groups` and returns true.

Mallory now matches every owner-scoped ACL in the repository — read, write and tag grants that
were meant only for the recorded directory owner — with priority 6 (group match). The identical
line `admin: mallory` yields the reserved `admin` group without going through
`verify_admin()`/`CVSROOT/admin` at all.

The mirror-image effect: a genuine user account named `owner` hits the `continue` on line 109 and
can therefore never be matched into any group in the file, so their group-based ACLs silently
stop applying.

## Suggested fix
Hoist the check out of the member loop and test the group name:

```cpp
			names = cvs_strtok(NULL, ":\n");
			if (names == NULL)
				continue;

			if(!strcasecmp(group,"admin"))
			{
				error(0,0,"The group 'admin' is automatically assigned to repository administrators");
				continue;
			}
			if(!strcasecmp(group,"owner"))
			{
				error(0,0,"The group 'owner' is automatically assigned to directory owners");
				continue;
			}

			name = cvs_strtok(names, ", \t");
			for(;name != NULL; name = cvs_strtok(NULL, ", \t"))
			{
				if (!usercmp (CVS_Username, name))
				{
					add_valid_group(group);
					break;
				}
			}
```

## Refutation attempt
- Confirmed which token is the group name by reading the parse itself (`group` is the first
  `cvs_strtok(linebuf,":\n")`, `names` the second) and cross-checking doc/cvs.dbk:2775-2781.
- Confirmed the reserved names are actually privileged rather than decorative:
  `add_valid_group("admin")` at src/perms.cpp:138 is gated on `verify_admin()`, and `"owner"` is
  special-cased in `verify_acl()` at src/perms.cpp:346 and :351 and in `verify_owner_acl()`
  (src/perms.cpp:305-314).
- Checked whether `verify_valid_name()` might reject reserved names independently — it does not;
  it is a plain `valid_groups` lookup (src/perms.cpp:160-166).
- Checked whether the `||` in `verify_acl`'s user test short-circuits before reaching
  `verify_valid_name` for `"owner"`: it evaluates
  `(!strcmp(val_user,"owner") && verify_owner_acl(acl))` first, and only when that is **false**
  does it try `verify_valid_name(val_user)` — so a non-owner falls through into the poisoned
  group lookup, which is precisely the escalation.
- Considered whether write access to `CVSROOT/group` already implies full control (making this
  moot): it does not — `CVSROOT/group` is committed like any other administrative file and is
  routinely delegated to non-administrators, whereas `CVSROOT/admin` membership and directory
  ownership are deliberately separate mechanisms.
