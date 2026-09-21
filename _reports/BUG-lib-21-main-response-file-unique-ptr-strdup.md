---
id: BUG-lib-21
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/src/main.cpp
line: 683
severity: medium
category: memory-safety
verdict: CONFIRMED
fix_size_loc: 6
behavior_change: no
---

# Response-file handling stores `strdup()` results in `std::unique_ptr<char[]>`, so every argument is released with `delete[]` on `malloc`'d memory; the vector is then reinterpreted as `char**`

## Summary
The `@responsefile` support builds the replacement `argv` in a
`std::vector<std::unique_ptr<char[]>>` whose elements are all `strdup()` pointers.
`std::unique_ptr<char[]>`'s deleter is `delete[]`, so at process exit every one of those
`malloc`'d blocks is freed with `operator delete[]` — undefined behaviour and an ASan
`alloc-dealloc-mismatch` abort. The vector's buffer is then cast to `char**`, and blank lines in
the response file leak a `strdup()`.

## Code
```cpp
// src/main.cpp:683
static std::vector<std::unique_ptr<char[]>> response_args;

// src/main.cpp:689-717
    if (argc >= 2 && argv[argc-1] && argv[argc - 1][0]=='@')
    {
      const char *respFile = argv[argc-1]+1;
      FILE *f = fopen(respFile, "r");
      ...
        char buf[8192];
        for (int i = 0; i < argc-1; ++i)
  		response_args.emplace_back(strdup(argv[i]));           // <-- malloc'd into unique_ptr<char[]>
		while (const char* s = fgets(buf, sizeof(buf), f))
		{
		  char* copy = strdup(s);                                 // <-- ditto
          size_t len = strlen(copy);
          while (len && (copy[len - 1] == '\r' || copy[len - 1] == '\n'))
            copy[(len--) - 1] = 0;
          if (len)
            response_args.emplace_back(copy);                     // <-- leaked when len == 0
		}
		printf("parsed response file <%s>...\n", respFile);
		fclose(f);
      }
      argc = (int)response_args.size();
      argv = (char**)response_args.data();                        // <-- unique_ptr<char[]>* -> char**
    }
```

## Why it is a bug
`std::default_delete<T[]>::operator()` is specified as `delete[] ptr;`. Pairing `delete[]` with
`malloc()`/`strdup()` is undefined behaviour: the two allocators are only required to be
interchangeable by accident, and hardened builds treat the mismatch as a fatal error. Because
`response_args` has static storage duration, the destructor — and therefore the whole batch of
mismatched frees — runs during static destruction at exit, after `main()` has returned, where a
diagnostic is least useful.

Three further defects in the same block:

* **Leak.** When the trimmed line is empty (`len == 0`) the `strdup()` at line 706 is never stored
  and never freed. A response file padded with blank lines leaks one allocation per blank line.
* **Type-punned `data()`.** `response_args.data()` has type `std::unique_ptr<char[]>*`. The cast
  to `char**` assumes `unique_ptr` is layout-compatible with a bare pointer. That holds for
  libstdc++ and libc++ with the stateless default deleter, but it is not guaranteed, and it breaks
  outright under a debug/checked standard library that adds members to `unique_ptr`.
* **`argv` is no longer NULL-terminated.** C requires `argv[argc] == NULL`. After line 717,
  `argv[argc]` is one element past the vector's buffer. (Nothing in this tree currently reads
  `argv[argc]` — a grep over `src/` and `lib/` finds no such access — but any library handed this
  `argv` may.)

## Failure scenario
```sh
cvs -d :pserver:me@host:/repo @args.rsp
```
with `args.rsp` containing the command and its arguments.

1. Line 703 `strdup`s each of `argv[0..argc-2]` into the vector; lines 706-711 `strdup` each
   response-file line into the same vector.
2. `main()` runs to completion and returns.
3. Static destruction runs `~vector`, which runs `~unique_ptr<char[]>` on every element, each
   executing `delete[] p` where `p` came from `strdup`.

Under `-fsanitize=address` this reports
`alloc-dealloc-mismatch (malloc vs operator delete [])` and aborts with a non-zero exit status
*after* the command has already completed — so a successful `cvs commit` reports failure to the
invoking script. Under MSVC's debug CRT the equivalent check fires. On a plain glibc build the
mismatch is silently survivable today, but it is one allocator change away from a heap crash.

## Suggested fix
Give the vector an ownership type that matches the allocation, e.g.

```cpp
static std::vector<std::string> response_storage;
static std::vector<char*> response_args;
...
        for (int i = 0; i < argc-1; ++i)
            response_storage.emplace_back(argv[i]);
        while (const char* s = fgets(buf, sizeof(buf), f))
        {
            std::string line(s);
            while (!line.empty() && (line.back()=='\r' || line.back()=='\n'))
                line.pop_back();
            if (!line.empty())
                response_storage.emplace_back(std::move(line));
        }
        fclose(f);
        for (auto& a : response_storage)
            response_args.push_back(&a[0]);
        response_args.push_back(nullptr);          /* argv[argc] == NULL */
        argc = (int)response_args.size()-1;
        argv = response_args.data();
```
(or, minimally, keep `strdup` but use
`std::vector<std::unique_ptr<char, decltype(&free)>>` / a plain `std::vector<char*>` freed with
`free`.)

## Refutation attempt
- Confirmed `std::unique_ptr<char[]>`'s deleter really is `delete[]`: `default_delete<T[]>` is
  specified with `delete[]`, and there is no custom deleter template argument at src/main.cpp:683.
- Confirmed the stored pointers really come from `malloc`: both `emplace_back` calls take a
  `strdup()` result (lines 703 and 706-711); there is no `new char[]` anywhere in the block.
- Checked whether the vector might be leaked deliberately (never destroyed, so never mismatched):
  it is a file-scope `static`, so its destructor is registered and does run.
- Checked whether `emplace_back(char*)` even compiles into `unique_ptr` — it does:
  `unique_ptr<T[]>`'s pointer constructor is `explicit`, and `emplace_back` direct-initialises, so
  the explicit constructor is selected. This is not a hypothetical construct; it is what the code
  builds.
- Grepped `src/` and `lib/` for `argv[argc]` to see whether the missing NULL terminator is
  immediately fatal: it is not today, which is why that part is listed as a latent defect rather
  than the headline.
