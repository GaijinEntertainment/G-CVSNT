#pragma once
#include <stdint.h>
#include <stddef.h>
//these functions has to be resolved as link time dependency
void *blob_create_ctx(const char *root);
void blob_destroy_ctx(void *ctx);

//return ~0 if mising
extern uint64_t blob_get_hash_blob_size(const void *ctx, const char* hash_type, const char* hash_hex_string);//hash_type = "blake3" hash_hex_string = "fe12bc56...."
extern bool blob_is_under_attack(bool successful_attempt, void *ctx);
extern bool blob_does_hash_blob_exist(const void *ctx, const char* hash_type, const char* hash_hex_string);//hash_type = "blake3" hash_hex_string = "fe12bc56...."
extern uintptr_t blob_start_push_data(const void *ctx, const char* hash_type, const char* hash_hex_string, uint64_t guessed_size);//hash_type = "blake3" hash_hex_string = "fe12bc56...."
extern bool blob_push_data(const void *data, uint64_t data_size, uintptr_t up);
extern bool blob_end_push_data(uintptr_t up);
extern void blob_destroy_push_data(uintptr_t up);

extern uintptr_t blob_start_pull_data(const void *ctx, const char* hash_type, const char* hash_hex_string, uint64_t &blob_sz);//allows memory mapping. blob_sz is the size of blob. if invalid returns nullptr
extern const char *blob_pull_data(uintptr_t readBlob, uint64_t from, uint64_t &data_pulled);//data_pulled != 0, unless error
extern bool blob_end_pull_data(uintptr_t tempBlob);

