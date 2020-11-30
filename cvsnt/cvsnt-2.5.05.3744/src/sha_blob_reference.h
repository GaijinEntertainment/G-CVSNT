#pragma once
#include "sha_blob_format.h"
#include "sha_blob_operations.h"
//session hash blob reference has the following format: blake3:<encoded_hash>:<encoded_fnv1_of_encoded_hash>
#define HASH_TYPE_REV_STRING "blake3:"//we actually use blake3, as it is not-vulnerable to length extension. Also, it is 4 times faster in C, 8 times faster with just sse2 (and also can be implemented with AVX)
static constexpr size_t hash_type_magic_len = 7;//strlen(HASH_TYPE_REV_STRING);

static const size_t hash_encoded_size = 64;//32*2
static constexpr size_t blob_file_path_len = hash_encoded_size + 2 /* / / */ + 7 /*strlen(BLOBS_SUBDIR)*/ + 1/*'\0'*/;

static const size_t blob_reference_size = hash_type_magic_len+hash_encoded_size;

//bool can_be_blob_reference(const char *blob_ref_file_name);
bool is_blob_reference_data(const void *data, size_t len);

bool get_blob_reference_content_hash(const unsigned char *ref_file_content, size_t len, char *hash_encoded);//hash_encoded==char[65], encoded 32 bytes + \0

static const size_t session_crypt_magic_size = 8;
static const size_t session_blob_reference_size = blob_reference_size + session_crypt_magic_size;

bool is_session_blob_reference_data(const void *data, size_t len);
bool get_session_blob_reference_hash(const char *blob_ref_file_name, char *hash_encoded);//hash_encoded==char[65], encoded 64 bytes + \0
bool gen_session_crypt(void *data, size_t len);//size_t len should be session_blob_reference_size, and first blob_reference_size should be  HASH_TYPE_REV_STRING:<encoded_hash>
