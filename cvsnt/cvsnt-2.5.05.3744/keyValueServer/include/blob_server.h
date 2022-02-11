#pragma once
#include "blobs_encryption.h"
//will start (almost) infinite loop, unless should_stop == true
bool start_push_server(int portno, int max_connections, volatile bool *should_stop, const char* encryption_secret, CafsServerEncryption encryption);
void blob_sleep_for_msec(unsigned int msec);
