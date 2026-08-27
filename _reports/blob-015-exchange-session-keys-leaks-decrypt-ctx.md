# exchange_session_keys frees the wrong cipher context, leaking the decrypt EVP_CIPHER_CTX per handshake

- **File:** cvsnt/cvsnt-2.5.05.3744/keyValueServer/blob_sockets/blob_sockets.cpp
- **Line(s):** 397-422 (esp. 417-421)
- **Severity:** medium
- **Confidence:** high
- **Category:** memory

## Code
```cpp
EVP_CIPHER_CTX *enc = init_cipher_from_otp(otp_page, true);
uint8_t encrypted[sizeof(ourDhAuthData)];
int outLen = 0;
if (1 != EVP_EncryptUpdate(enc, encrypted, &outLen, ourDhAuthData, sizeof(ourDhAuthData)) || outLen != sizeof(encrypted))
  return nullptr;
EVP_CIPHER_CTX_free(enc); enc = NULL;                 // enc freed here
...
EVP_CIPHER_CTX *dec = init_cipher_from_otp(otp_page, false);   // new context
if (1 != EVP_DecryptUpdate(dec, otherDhAuthData, &outLen, encrypted, sizeof(ourDhAuthData)) || outLen != sizeof(encrypted))
  return nullptr;
memset(encrypted, 0, sizeof(encrypted));
EVP_CIPHER_CTX_free(enc); enc = NULL;                 // BUG: frees enc (already NULL); dec is never freed
return otherDhAuthData;
```

## Why this is a bug
On the success path, `dec` (an `EVP_CIPHER_CTX` allocated inside `init_cipher_from_otp` via `EVP_CIPHER_CTX_new`) is never freed: line 421 frees `enc`, which was already freed and set to `NULL` at line 402, so `EVP_CIPHER_CTX_free(enc)` is a harmless no-op while `dec` leaks. It is clearly a copy/paste typo — the line should free `dec`.

`exchange_session_keys` runs once per authenticated connection (server side via `connect_to_client_blob_socket`, client side via `connect_to_server_blob_socket`). Each successful handshake therefore leaks one cipher context plus its underlying AES state. On the long-lived multithreaded CAFS server this is an unbounded per-connection memory leak driven by remote clients.

Secondary defects in the same block: `init_cipher_from_otp` can return `nullptr` (its `EVP_*Init_ex` may fail), but `enc` (line 400) and `dec` (line 418) are passed to `EVP_EncryptUpdate`/`EVP_DecryptUpdate` with no NULL check — a null-context dereference/crash. Also the early `return nullptr` at line 401 leaks `enc`, and at line 419 leaks `dec`.

## Suggested fix
```cpp
EVP_CIPHER_CTX_free(dec); dec = NULL;     // free the decrypt context, not enc
```
and null-check the results of `init_cipher_from_otp` before use, freeing `enc`/`dec` on every early-return error path.
