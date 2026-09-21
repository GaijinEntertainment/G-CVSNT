---
id: BUG-blob-04
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/keyValueServer/blob_sockets/blob_sockets.cpp
line: 421
severity: medium
category: leak
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: no
---

# Handshake frees `enc` twice and never frees `dec` — one `EVP_CIPHER_CTX` leaked per authenticated connection

## Summary
Inside the ECDH key-exchange lambda, the second cleanup line repeats
`EVP_CIPHER_CTX_free(enc)` (a no-op, `enc` is already `NULL`) where it should free `dec`.
The OTP decryption context allocated at line 417 is never released, so every successful
authenticated handshake leaks an `EVP_CIPHER_CTX`. Two error paths leak as well.

## Code
```cpp
// keyValueServer/blob_sockets/blob_sockets.cpp:397-422
          EVP_CIPHER_CTX *enc = init_cipher_from_otp(otp_page, true);
          uint8_t encrypted[sizeof(ourDhAuthData)];
          int outLen = 0;
          if (1 != EVP_EncryptUpdate(enc, encrypted, &outLen, ourDhAuthData, sizeof(ourDhAuthData)) || outLen != sizeof(encrypted))
            return nullptr;                                  // leaks enc
          EVP_CIPHER_CTX_free(enc); enc = NULL;               // 402: enc freed here
          ...
          EVP_CIPHER_CTX *dec = init_cipher_from_otp(otp_page, false);   // 417
          if (1 != EVP_DecryptUpdate(dec, otherDhAuthData, &outLen, encrypted, sizeof(ourDhAuthData)) || outLen != sizeof(encrypted))
            return nullptr;                                  // leaks dec
          memset(encrypted, 0, sizeof(encrypted));
          EVP_CIPHER_CTX_free(enc); enc = NULL;               // 421: BUG - should be dec
          return otherDhAuthData;
```

## Why it is a bug
`enc` is set to `NULL` at line 402, so line 421 is `EVP_CIPHER_CTX_free(NULL)` — a documented
no-op. Nothing else in the translation unit ever sees `dec`: it is a lambda-local raw pointer, not
stored in the `BlobSocket` (`encrypted_socket()` only receives the pair produced later by
`create_encryption_pair`), and `blob_close_encryption()` only frees `BlobSocket::encrypt` and
`BlobSocket::decrypt`. So `dec` is unreachable and unfreed on every path through this lambda.

`init_cipher_from_otp` allocates via `EVP_CIPHER_CTX_new()` and initialises an AES-128-CTR key
schedule; each leaked context is a few hundred bytes of heap that OpenSSL will not reclaim. It also
keeps the OTP-derived key material alive in the heap indefinitely, which is at odds with the
careful `memset`-on-exit style used everywhere else in this function.

Additionally, `init_cipher_from_otp` may return `nullptr` (if `EVP_CIPHER_CTX_new()` or
`EVP_*Init_ex` fails; it returns `nullptr` explicitly at `blob_sockets.cpp:217`) and neither line
397 nor line 417 checks the result before dereferencing it in
`EVP_EncryptUpdate`/`EVP_DecryptUpdate`.

## Failure scenario
`cafs_server` started with `encryption <secret>`. Every incoming connection runs
`authenticate_client` -> `connect_to_client_blob_socket` -> `encrypted_socket` ->
`exchange_session_keys` -> this lambda, exactly once. A build farm doing 50 000 checkouts a day
against the server leaks 50 000 `EVP_CIPHER_CTX` objects per day; the process is long-lived
(`start_push_server` loops until shutdown), so RSS grows monotonically until the server is
restarted. The proxy leaks the same way on its client side, once per `ClientConnection::start()` /
`restart()` — and `restart()` sits inside a retry loop of up to 100 iterations in
`PullThroughTemp::start` (`proxy_file_lib.cpp:315-332`), so a flapping master multiplies the leak.

## Suggested fix
```cpp
          EVP_CIPHER_CTX_free(dec); dec = NULL;
```
(and, for completeness, free `enc`/`dec` before each `return nullptr`, plus a null check on both
`init_cipher_from_otp` results).

## Refutation attempt
I searched the whole file for any other reference to `dec` — there is none; it is declared, used
once, and goes out of scope. I checked whether OpenSSL contexts are reclaimed by some global
cleanup: `blob_close_sockets()` only calls `kill_locks()` and `raw_close_sockets()`. I confirmed
`EVP_CIPHER_CTX_free(NULL)` is a documented no-op rather than a crash, so line 421 is a silent leak
and not a double free. The finding stands.
