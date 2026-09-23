#pragma once
#include <atomic>
#include "blobs_encryption.h"
//will start (almost) infinite loop, unless *should_stop becomes true.
//atomic: the flag is written by another thread while this one blocks in
//accept; volatile would not synchronize that.  Semantics are best-effort:
//the flag is observed between connections (accept has no wake path), and
//on the forked (!MULTI_THREADED) builds an established child worker holds
//a post-fork copy and does not see the parent's store.  Threaded workers
//are detached and keep reading the flag after this function returns, so
//the pointed-to atomic must have static (or otherwise process-long)
//storage duration.  The signature changed from volatile bool* (different
//mangled symbol) - consumers of the installed library must rebuild.
bool start_push_server(int portno, int max_connections, std::atomic<bool> *should_stop, const char* encryption_secret, CafsServerEncryption encryption);
void blob_sleep_for_msec(unsigned int msec);
