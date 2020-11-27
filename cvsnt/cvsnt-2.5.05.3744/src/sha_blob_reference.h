#pragma once
#include "sha_blob_format.h"
#include "sha_blob_operations.h"
//sha256 blob reference has the following format: sha256:<encoded_sha256>:<encoded_fnv1_of_encoded_sha256>
//ofc, all files in repo are sha256 blob reference, but right now kinda trust that client wont commit sha256 reference itself, this make it unlikely
#define SHA256_REV_STRING "blake3:"//we actually use blake3, as it is not-vulnerable to length extension. Also, it is 4 times faster in C, 8 times faster with just sse2 (and also can be implemented with AVX)
static constexpr size_t sha256_magic_len = 7;//strlen(SHA256_REV_STRING);

static const size_t sha256_encoded_size = 64;//32*2
static constexpr size_t blob_file_path_len = sha256_encoded_size + 2 /* / / */ + 7 /*strlen(BLOBS_SUBDIR)*/ + 1/*'\0'*/;

static const size_t blob_reference_size = sha256_magic_len+sha256_encoded_size;

//bool can_be_blob_reference(const char *blob_ref_file_name);
bool is_blob_reference_data(const void *data, size_t len);

bool get_blob_reference_content_sha256(const unsigned char *ref_file_content, size_t len, char *sha256_encoded);//sha256_encoded==char[65], encoded 32 bytes + \0
//bool get_blob_reference_sha256(const char *blob_ref_file_name, char *sha256_encoded);//sha256_encoded==char[65], encoded 32 bytes + \0
//server only, check if blob reference is correct, and referenced blob exists
//bool is_blob_reference(const char *root_dir, const char *blob_ref_file_name, char *sha_file_name, size_t sha_max_len);//returns sha_file_name, which is root/blobs/xx/yy/zzzzzzz*

//writes reference tp file fn. ref_len should be blob_reference_size!
//bool write_direct_blob_reference(const char *fn, const void *ref, size_t ref_len, bool fail_on_error=true);//ref_len == blob_reference_size
//bool write_blob_reference(const char *fn, unsigned char sha256[], bool fail_on_error=true);//sha256[32] == digest

//writes blob reference to fn, and also writes blob itself;
//void write_blob_and_blob_reference(const char *root, const char *fn, const void *data, size_t len, BlobPackType pack, bool src_packed);

static const size_t session_crypt_magic_size = 8;
static const size_t session_blob_reference_size = blob_reference_size + session_crypt_magic_size;

bool is_session_blob_reference_data(const void *data, size_t len);
bool get_session_blob_reference_sha256(const char *blob_ref_file_name, char *sha256_encoded);//sha256_encoded==char[65], encoded 64 bytes + \0
bool gen_session_crypt(void *data, size_t len);//size_t len should be session_blob_reference_size, and first blob_reference_size should be  SHA256_REV_STRING:<encoded_sha256>
