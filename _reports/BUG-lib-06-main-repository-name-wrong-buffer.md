---
id: BUG-lib-06
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/main.cpp
line: 625
severity: high
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# Copy-paste error: trailing-separator strip tests `buffer2` but truncates `buffer`, corrupting every configured repository path

## Summary
`read_global_config()` strips a trailing directory separator from the repository path
(`buffer`) and then from the repository display name (`buffer2`). The second block tests
`buffer2` but writes into `buffer` — the wrong variable of the pair. The result is that the
physical repository path passed to `root_allow_add()` loses **two** characters instead of one
whenever the name ends in a separator, and `buffer2` keeps its trailing separator.

## Code
```cpp
// src/main.cpp:618-626
				char tmp[32];
				int prefixnum = atoi(token+10);
				snprintf(tmp,sizeof(tmp),"Repository%dName",prefixnum);
				if(CGlobalSettings::GetGlobalValue("cvsnt","PServer",tmp,buffer2,sizeof(buffer2)))
					strcpy(buffer2,buffer);
				if(*buffer && ISDIRSEP(buffer[strlen(buffer)-1]))
					buffer[strlen(buffer)-1]='\0';
				if(*buffer2 && ISDIRSEP(buffer2[strlen(buffer2)-1]))
					buffer[strlen(buffer)-1]='\0';        // <-- line 625: buffer, should be buffer2
```
`buffer` is `char buffer[MAX_PATH]` (src/main.cpp:514), `buffer2` is `char buffer2[MAX_PATH]`
(src/main.cpp:575). The pair ends up in
`root_allow_add(buffer,buffer2,...)` at src/main.cpp:648.

## Why it is a bug
The two `if`s are obviously meant to be the same operation applied to each of the two strings —
the guard, the macro and the assignment target should all use the same variable. The second one
mixes them: guard on `buffer2`, write on `buffer`. `root_allow_add()` (src/root.cpp:183) stores
`root` (from `buffer`) as the physical repository directory and `name` (from `buffer2`) as the
alias clients present, so both values matter.

## Failure scenario
The dominant configuration is *no* `Repository<N>Name` key, in which case line 622 copies
`buffer` into `buffer2` **before** the stripping. Take a `cvsnt/PServer/Repository0` value of
`/var/lib/cvs/` (trailing slash — the natural way an admin writes a directory):

1. `GetGlobalValue(...,"Repository0Name",buffer2,...)` fails, so `strcpy(buffer2,buffer)`
   makes `buffer2 == "/var/lib/cvs/"`.
2. Line 623: `buffer` ends in `/`, so it becomes `"/var/lib/cvs"`. Correct.
3. Line 625: `buffer2` still ends in `/`, so the guard is true — and the body truncates
   **`buffer`** again: `buffer` becomes `"/var/lib/cv"`.
4. `root_allow_add("/var/lib/cv", "/var/lib/cvs/", ...)`.

The server now advertises and accepts the root `/var/lib/cv`, which does not exist, while every
client using the documented `/var/lib/cvs` is rejected with "not allowed". If a directory
`/var/lib/cv` happens to exist the server silently serves the wrong tree.

Secondary, memory-unsafe case: with `Repository0` set to the empty string and
`Repository0Name` set to something ending in a separator, line 624's guard `*buffer2` is true
while `*buffer` is `'\0'`, so line 625 evaluates `buffer[strlen(buffer)-1]` =
`buffer[(size_t)-1]` = `buffer[-1]` and writes a zero byte **one byte below the `MAX_PATH`
stack array**.

## Suggested fix
```cpp
				if(*buffer2 && ISDIRSEP(buffer2[strlen(buffer2)-1]))
					buffer2[strlen(buffer2)-1]='\0';
```

## Refutation attempt
- Checked that `buffer` and `buffer2` are genuinely distinct objects with distinct roles at the
  consumer: `root_allow_add(const char *root, const char *name, ...)` (src/root.cpp:183) stores
  them into separate `root_allow_struct` fields, so this is not an aliasing situation where the
  two writes would be equivalent.
- Checked that the first `if` cannot already have removed the separator from `buffer2`: the
  `strcpy(buffer2,buffer)` at line 622 happens *before* line 623, so `buffer2` still carries the
  separator when line 624 tests it. The bug therefore fires on the default, name-less
  configuration, not only on an exotic one.
- Checked `ISDIRSEP` (lib/system.h:471/504) — a plain character test with no side effects, so
  the guard really is just "last char is a separator".
- Confirmed the `isdigit(token[strlen(token)-1])` on line 615 is *not* an underflow: reaching it
  requires `strncasecmp(token,"Repository",10)==0` and `isdigit(token[10])`, so
  `strlen(token) >= 11`.
