#include "../blob_server_func_deps.h"
#include "../../ca_blobs_fs/content_addressed_fs.h"

size_t blob_get_hash_blob_size(const char* htype, const char* hhex) {return caddressed_fs::get_size(hhex);}
bool blob_does_hash_blob_exist(const char* htype, const char* hhex) {return caddressed_fs::exists(hhex);}

uintptr_t blob_start_push_data(const char* htype, const char* hhex, uint64_t size) {return uintptr_t(caddressed_fs::start_push(hhex));}
bool blob_push_data(const void *data, size_t size, uintptr_t up) { return caddressed_fs::stream_push((caddressed_fs::PushData *)up, data, size); }
bool blob_end_push_data(uintptr_t up) { return caddressed_fs::is_ok(caddressed_fs::finish((caddressed_fs::PushData *)up, nullptr)); }
void blob_destroy_push_data(uintptr_t up) {caddressed_fs::destroy((caddressed_fs::PushData *)up);}

uintptr_t blob_start_pull_data(const char* htype, const char* hhex, uint64_t &sz){return uintptr_t(caddressed_fs::start_pull(hhex, sz));}
const char *blob_pull_data(uintptr_t up, uint64_t from, size_t &read){return caddressed_fs::pull((caddressed_fs::PullData*)up, from, read);}
bool blob_end_pull_data(uintptr_t up){return caddressed_fs::destroy((caddressed_fs::PullData*)up);}

