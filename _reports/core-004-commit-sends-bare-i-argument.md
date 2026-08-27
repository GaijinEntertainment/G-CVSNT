# commit client sends bare "i" instead of "-i" for ignore-keywords option

- **File:** cvsnt/cvsnt-2.5.05.3744/src/commit.cpp
- **Line(s):** 574-575
- **Severity:** medium
- **Confidence:** high
- **Category:** typo

## Code
```cpp
	if(commit_keep_edits)
		send_arg("-e");

	if(slide_tags)
		send_arg("-T");

	if(ignore_keywords)
		send_arg("i");        // <-- missing '-'; every sibling sends "-X"
```

## Why this is a bug
When the user runs `cvs commit -i` (the "ignore keyword differences" option, still accepted by the client's getopt string `"+cnlRm:M:fF:Db:B:eTi"` at line 401 even though it is commented out of the usage text), the client sends the literal argument `i` to the server instead of `-i`.

On the server, commit's option string starts with `+` (POSIX mode: stop option parsing at the first non-option argument). The bare `i` therefore:
1. is never interpreted as the ignore-keywords option — the feature silently does nothing remotely; and
2. terminates option parsing, so every argument the client sends *after* it (`-l`, `-f`, `-n`, `-c`, `--`) is treated as a file name to commit rather than as an option. The commit then fails with "nothing known about `i'" / "nothing known about `-l'" etc., or in the worst case operates on a real working file that happens to be named `i`.

## Suggested fix
```cpp
	if(ignore_keywords)
		send_arg("-i");
```
