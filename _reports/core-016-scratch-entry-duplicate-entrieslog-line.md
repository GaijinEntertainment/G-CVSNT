# Scratch_Entry/Rename_Entry write a duplicate un-prefixed (implicit-Add) line into Entries.Log

- **File:** cvsnt/cvsnt-2.5.05.3744/src/entries.cpp
- **Line(s):** write_ent_ex_proc 120-135 (the `fputentent(entfile,...)` at 129); callers Scratch_Entry 257-258, Rename_Entry 299-300 & 310-311, Register 422-423
- **Severity:** low
- **Confidence:** medium
- **Category:** logic

## Code
```cpp
static int write_ent_ex_proc (Node *node, void *closure)
{
    Entnode *entnode = (Entnode *) node->data;
    if (closure != NULL && entnode->type != ENT_FILE)
	*(int *) closure = 1;
    if (fputentent(entfile, entnode))          // <-- writes the Entries line to entfile
		error (1, errno, "cannot write %s", entfilename);
    if (fputententex(entexfile, entnode))      // writes the Entries.Extra line to entexfile
		error (1, errno, "cannot write %s", entexfilename);
    return (0);
}
```
Scratch_Entry appends to `CVSADM_ENTLOG` (entfile) like this:
```cpp
    if (fprintf (entfile, "R ") < 0) ...
    if (fprintf (entexfile, "R ") < 0) ...
    write_ent_proc (node, NULL);      // entfile gets: "R /user/vn/ts/opts/td\n"
    write_ent_ex_proc (node, NULL);   // entfile ALSO gets a second: "/user/vn/ts/opts/td\n"
```

## Why this is a bug
`write_ent_ex_proc` writes the plain Entries line to `entfile` **and** the extra line to `entexfile`. That is correct for the full-rewrite path (`write_entries` walks only `write_ent_ex_proc`). But the incremental log-append paths (`Scratch_Entry`, `Rename_Entry`, `Register`) call **both** `write_ent_proc` (which already wrote the entry to `entfile`) and `write_ent_ex_proc`, so every such operation appends a **second, command-prefix-less copy** of the entry to `Entries.Log`.

When `Entries.Log` is later replayed (`fgetentent` with a `cmd`), a line whose second character is not a space is treated as an implicit **Add** (`if (l[1] != ' ') *cmd = 'A';`, entries.cpp:473). So for `Scratch_Entry` the log records:
```
R /user/...      (remove)
/user/...        (implicit ADD of the very file just removed)
```
Replaying this removes then re-adds the file — the scratch is undone.

In normal operation this is masked because `write_entries` runs at command end, rewrites `CVSADM_ENT` from the in-memory list, and unlinks `Entries.Log` before it can be replayed. But if the process terminates between the `Scratch_Entry` and the closing `write_entries` (fatal `error()`, signal, crash, `kill`), the stale `Entries.Log` survives and the **next** command's `Entries_Open` replays the bogus Add, resurrecting a scratched entry (and, for `Rename_Entry`, resurrecting the old name). It also doubles the size of every Entries.Log write.

## Suggested fix
Give the incremental paths a helper that writes only the Entries.Extra line (e.g. a `write_entex_only_proc` calling just `fputententex(entexfile,...)`), and use `write_ent_proc` + that helper in `Scratch_Entry`/`Rename_Entry`/`Register`, leaving `write_ent_ex_proc` (both files) only for the `write_entries` full rewrite.
