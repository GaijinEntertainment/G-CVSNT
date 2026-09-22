---
id: BUG-build-01
area: build / Windows project files
file: cvsnt/cvsnt-2.5.05.3744/cvsnt.vcxproj
line: 204
severity: low
category: typo
verdict: CONFIRMED
fix_size_loc: 2
behavior_change: no
---

# `cvsnt.vcxproj` x64 configurations point at `..\external_libsx64` — a missing backslash makes the first library search path nonexistent

## Summary
Both x64 configurations (`Debug|x64` at line 204 and `Release|x64` at line 309) list
`..\external_libsx64` as the first `AdditionalLibraryDirectories` entry. The directory separator
between `external_libs` and `x64` is missing, so that path never resolves. The Win32
configurations have the analogous path written correctly.

## Code
```xml
<!-- cvsnt.vcxproj:204  (Debug|x64) -->
<AdditionalLibraryDirectories>..\external_libsx64;.\external_libs\x64;</AdditionalLibraryDirectories>

<!-- cvsnt.vcxproj:309  (Release|x64) -->
<AdditionalLibraryDirectories>..\external_libsx64;.\external_libs\x64;</AdditionalLibraryDirectories>
```

Compare the Win32 configurations, which are correct:
```xml
<!-- cvsnt.vcxproj:151  (Debug|Win32) -->
<AdditionalLibraryDirectories>..\external_libs\Win32;.\external_libs\Win32;</AdditionalLibraryDirectories>

<!-- cvsnt.vcxproj:254  (Release|Win32) -->
<AdditionalLibraryDirectories>..\external_libs\Win32;.\external_libs\Win32;</AdditionalLibraryDirectories>
```

## Why it is a bug
The pattern in all four configurations is a pair of paths: one relative to the solution's parent
(`..\external_libs\<arch>`) and one relative to the project directory
(`.\external_libs\<arch>`). The x64 pair is missing the separator in the first element, so the
`..\` fallback is silently dead. The second entry, `.\external_libs\x64`, exists in this repository
and is what actually satisfies the link — which is exactly why nobody has noticed.

`AdditionalIncludeDirectories` in the same configurations gets it right (`..\external_libs` on
lines 118, 167, 218, 271), confirming that a `..\external_libs`-rooted path is the intended form.

## Failure scenario
Build `Release|x64` in a tree where the per-project `external_libs\x64` directory has been removed
or relocated — for example a source layout that keeps the prebuilt OpenSSL/iconv import libraries
one level up and shared between projects, which is what the `..\` entry exists to support. The
linker searches `..\external_libsx64` (nonexistent), then `.\external_libs\x64` (also gone), and
fails with `LNK1104: cannot open file 'libssl.lib'` even though the libraries are present at
`..\external_libs\x64`. The Win32 configurations in the same solution build fine, which makes the
failure look architecture-specific rather than like a path typo.

## Suggested fix
```xml
<!-- line 204 and line 309 -->
<AdditionalLibraryDirectories>..\external_libs\x64;.\external_libs\x64;</AdditionalLibraryDirectories>
```

## Refutation attempt
Checked whether `external_libsx64` exists anywhere in the tree as a real directory — it does not;
`ls cvsnt/cvsnt-2.5.05.3744/` shows only `external_libs`, containing `Win32/`, `x64/` and
`openssl/`. Checked whether MSBuild might normalise a missing separator — it does not; the string is
passed through to `/LIBPATH:` verbatim. Checked whether the entry might be intentionally dead — the
Win32 configurations prove the intended shape, and the include directories in the same
configurations use `..\external_libs`. The finding stands. Severity is low only because the second
path in the list currently rescues the build in this repository layout.
