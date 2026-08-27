---
id: BUG-server-08
area: locking
file: cvsnt/cvsnt-2.5.05.3744/src/lock.cpp
line: 896
severity: low
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `write_lock` builds the write-lock filename from `CVSRFL` (read-lock prefix) in the short-filename branch

## Summary
In `write_lock`, the `#else` half of the `HAVE_LONG_FILE_NAMES` conditional formats the write-lock filename with `CVSRFL` — the *read* lock prefix — instead of `CVSWFL`. The sibling `Reader_Lock` gets its own pair right, and the `xmalloc` on the line above already sizes the buffer with `sizeof (CVSWFL)`, confirming the intent.

## Code
```cpp
// lock.cpp:889-899  (write_lock)
	writelock = (char*)xmalloc (strlen (hostname) + sizeof (CVSWFL) + 40);
	(void) sprintf (writelock,
#ifdef HAVE_LONG_FILE_NAMES
			"%s.%s.%ld", CVSWFL, hostname,
#else
			"%s.%ld", CVSRFL,          // <-- should be CVSWFL
#endif
			(long) getpid ());
```

Compare `Reader_Lock`, which uses `CVSRFL` in both halves:
```cpp
// lock.cpp:733-741
	readlock = (char*)xmalloc (strlen (hostname) + sizeof (CVSRFL) + 40);
	(void) sprintf (readlock,
#ifdef HAVE_LONG_FILE_NAMES
			"%s.%s.%ld", CVSRFL, hostname,
#else
			"%s.%ld", CVSRFL,
#endif
			(long) getpid ());
```

## Why it is a bug
`CVSRFL` and `CVSWFL` are the two lock-file name prefixes (`#cvs.rfl` / `#cvs.wfl`). `readers_exist` (lock.cpp:952) decides whether a directory is read-locked by matching `CVSRFLPAT` against every directory entry. If `write_lock` writes its lock file under the read-lock prefix:

1. Every writer's own lock file matches `CVSRFLPAT`. `write_lock` calls `readers_exist` *before* creating the file, so it would not self-deadlock on a single pass — but a second writer arriving while the first holds its "write" lock would see the first writer's file as a *reader* and back off with `L_LOCKED` instead of the correct writer-vs-writer handling.
2. Nothing ever matches the write-lock pattern, so any code that distinguishes writers from readers by filename is defeated.
3. `lock_simple_remove` (lock.cpp:654) removes by `lock_name(lock->repository, writelock)`, so the file is still cleaned up — the corruption is silent, not a stuck lock.

## Failure scenario
Build on a platform where `configure` does not define `HAVE_LONG_FILE_NAMES` (short-filename filesystems). Two clients then commit into the same directory concurrently: client A takes the master `#cvs.lck` dir, verifies no readers, creates `#cvs.rfl.<pid>` (should be `#cvs.wfl.<pid>`), and releases the master dir. Client B takes the master dir, runs `readers_exist`, matches A's file against `CVSRFLPAT`, and reports "waiting for <user>'s lock" as if a *reader* held it — logging the wrong lock kind and taking the reader back-off path rather than the writer one. Additionally, `set_lockers_name` on Windows parses the owner out of the filename, so diagnostics attribute the block to the wrong lock type.

## Suggested fix
```cpp
			"%s.%ld", CVSWFL,
#endif
```

## Refutation attempt
* *Is this reachable in shipped builds?* Not currently — `config.h:282` and `windows-NT/config.h:39` both `#define HAVE_LONG_FILE_NAMES 1`, so the `#else` arm is dead in every configuration in the tree. This is why severity is **low**: it is a latent typo, not a live defect.
* *Could `CVSRFL` be intentional here (e.g. a shared namespace on short-filename systems)?* No. The `xmalloc` immediately above sizes the buffer with `sizeof (CVSWFL)`, and the `#ifdef` arm two lines up uses `CVSWFL`. The variable being filled is `writelock`, whose only consumer is `lock_name (lock->repository, writelock)` for the write lock.
* *Would the buffer be too small?* `CVSRFL` and `CVSWFL` are the same length (`"#cvs.rfl"` / `"#cvs.wfl"`), so there is no overflow — only the wrong name.
