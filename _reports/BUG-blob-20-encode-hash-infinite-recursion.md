---
id: BUG-blob-20
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/include/blob_hash_util.h
line: 96
severity: low
category: typo
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `encode_hash_str_to_blob_hash()` calls itself instead of the `_s` variant — unconditional infinite recursion

## Summary
The convenience wrapper that is supposed to forward to `encode_hash_str_to_blob_hash_s(..., 32+6)`
forwards to *itself* with the identical three arguments. Any call to this declared public API
recurses until the stack is exhausted.

## Code
```cpp
// keyValueServer/include/blob_hash_util.h:87-96
inline bool encode_hash_str_to_blob_hash_s(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash, size_t blob_hash_capacity)
{
  if (blob_hash_capacity < 32+6)
    return false;
  memcpy(blob_hash, hash_type, 6);
  return hex_string_to_bin_hash(hash_hex_string, blob_hash+6);
}

inline bool encode_hash_str_to_blob_hash(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash)
{ return encode_hash_str_to_blob_hash(hash_type, hash_hex_string, blob_hash); }   // 96 <-- calls itself
```

## Why it is a bug
The three-argument overload takes `(const char*, const char*, unsigned char*)` and the call
expression at line 96 supplies exactly those three types, so overload resolution picks the function
itself — the four-argument `_s` variant is not a candidate. There is no base case and no argument
changes between calls, so the recursion is unbounded.

Every sibling wrapper in the same header gets this right by naming the `_s` function explicitly:

```cpp
// :234-235
inline bool bin_hash_to_hex_string(const unsigned char *blob_hash, char *to_hash_hex_string)
{ return bin_hash_to_hex_string_s(blob_hash, to_hash_hex_string, 65); }
// :272-273
inline bool hex_string_to_bin_hash(const char *from_hash_hex_string, unsigned char *to_blob_hash)
{ return hex_string_to_bin_hash_s(from_hash_hex_string, to_blob_hash, 32); }
// :298-299
inline bool decode_blob_hash_to_hex_hash(const unsigned char *blob_hash, char *hash_type, char *hash_hex_string)
{ return decode_blob_hash_to_hex_hash_s(blob_hash, 32+6, hash_type, 7, hash_hex_string, 65); }
```
so line 96 is a copy-paste that lost its `_s` suffix.

The function is part of the header's advertised API — it is forward-declared at `:14`
(`bool encode_hash_str_to_blob_hash(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash);`)
right next to the three other wrappers that all work.

## Failure scenario
Any consumer of `blob_hash_util.h` that uses the short form — which the header presents as the
normal spelling, and which is the natural thing to reach for since the buffer size is fixed at
`hash_len` — e.g.

```cpp
  unsigned char blob_hash[blob_push_proto::hash_len];
  encode_hash_str_to_blob_hash("blake3", hex, blob_hash);
```

immediately recurses. With optimisation off, each frame consumes ~48 bytes on x86-64, so an 8 MiB
default stack is exhausted in under 200 000 calls — a few milliseconds — and the process dies with
`SIGSEGV` on the guard page. With optimisation on, the compiler turns it into an infinite loop via
tail-call elimination and the process hangs at 100 % CPU instead.

No in-tree caller uses it today (all five call sites — `blob_chck_client_cmd.cpp:20` and `:49`,
`blob_pull_client_cmd.cpp:20`, `blob_push_client_cmd.cpp:23`, `blob_strm_client_cmd.cpp:26` — use
`encode_hash_str_to_blob_hash_s`), which is why the defect has survived.

## Suggested fix
```cpp
inline bool encode_hash_str_to_blob_hash(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash)
{ return encode_hash_str_to_blob_hash_s(hash_type, hash_hex_string, blob_hash, 32+6); }
```

## Refutation attempt
I checked for a differently-typed overload that could win resolution and break the cycle — the
header declares only the 3-argument and the 4-argument (`_s`) forms, and the 4-argument form has a
different name, so no other candidate exists. I checked whether `inline` without an odr-use means
the body is never instantiated: that is true, which is precisely why the code still builds and why
I rate this low rather than high — the defect is latent, but it is unconditional the moment anyone
calls the function, and it sits in a public header of a library that is installed
(`lib_LTLIBRARIES = libkv_client_lib.la`). The finding stands.
