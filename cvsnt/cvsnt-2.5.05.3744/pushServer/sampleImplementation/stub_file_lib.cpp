#include "../blob_server_func_deps.h"
#include "file_functions.h"

size_t blob_get_hash_blob_size(const char* htype, const char* hhex) {return 1;}
bool blob_does_hash_blob_exist(const char* htype, const char* hhex) {return true;}

uintptr_t blob_start_push_data(const char* htype, const char* hhex, uint64_t size){return 1;}
bool blob_push_data(const void *data, size_t data_size, uintptr_t up){return true;}
bool blob_end_push_data(uintptr_t up, bool ok){return true;}

uintptr_t blob_start_pull_data(const char* htype, const char* hhex, uint64_t &sz){sz = 1; return 1;}
const char *blob_pull_data(uintptr_t up, uint64_t from, size_t &read){read = 1;return "X";}
bool blob_end_pull_data(uintptr_t up){return true;}

