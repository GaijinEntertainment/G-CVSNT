#include "../blob_server_func_deps.h"
#include "file_functions.h"

size_t blob_get_hash_blob_size(const char* htype, const char* hhex) {return file_get_hash_blob_size(htype, hhex);}
bool blob_does_hash_blob_exist(const char* htype, const char* hhex) {return file_does_hash_blob_exist(htype, hhex);}

uintptr_t blob_start_push_data(const char* htype, const char* hhex, uint64_t size){return file_start_push_data(htype, hhex, size);}
bool blob_push_data(const void *data, size_t size, uintptr_t up){return file_push_data(data, size, up);}
bool blob_end_push_data(uintptr_t up, bool ok){return file_end_push_data(up, ok);}

uintptr_t blob_start_pull_data(const char* htype, const char* hhex, uint64_t &sz){return file_start_pull_data(htype, hhex, sz);}
const char *blob_pull_data(uintptr_t up, uint64_t from, size_t &read){return file_pull_data(up, from, read);}
bool blob_end_pull_data(uintptr_t up){return file_end_pull_data(up);}

