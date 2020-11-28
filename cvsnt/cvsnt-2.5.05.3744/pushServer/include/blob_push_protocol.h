#pragma once
//first, server sends BLOBPUSH_SERVER_Vxxx
#define BLOB_PUSH_GREETING_STR "BLOBPUSH_SERVER_V"
namespace blob_push_proto
{
static const int greeting_prefix_length = 17;//strlen(BLOBPUSH_SERVER_V)
static const int greeting_length = greeting_prefix_length+3;//including XXX version!
static constexpr int bin_hash_len = 32;
static constexpr int hash_len = bin_hash_len+6;
//allows to transfer up to 4TB (4Gb<<20) per file

//then, server waits for commands
//all commands are 4 bytes. Currently we have just 4: "VERS", "CHCK", "PUSH", "PULL", "SIZE"
//"VERS": has to be followed by "XXX" (client version). answers are ERBD or NONE
//"SIZE": has to be followed by "blake3bin_hash_32". Responces are: "SIZE", "NONE", "ERXX" (xx - is error code).
//"CHCK": has to be followed by "blake3bin_hash_32". Responces are: "HAVE", "NONE", "ERXX" (xx - is error code).
//"PUSH": has to be followed by "blake3bin_hash_32bytes_size8". Responces are: "HAVE", "ERXX" (xx - is error code).
//"PULL": has to be followed by "blake3bin_hash_32bytes_size8". Responces are: "TAKE", "NONE", "ERXX" (xx - is error code). You can ask for 0:0 if you want whole file

//SIZE responce is SIZEbytes_size8
//TAKE responce is identical to PULL, except first 4 bytes, TAKEblake3:bin_hash\0bytes_size8bytes_from4

//after PUSH, client will immediately write bytes_size data. This data should be prepared blob, which original unpacked content is hashed to hex_encoded_hash with blake3
//after TAKE responce, server will immediately write full bytes_size data (starting from bytes_from<<20 of blob). Allows pull by 1mb chunks.
//bytes_size8,bytes_size4 are binary in little endian
static constexpr size_t pull_chunk_size = 1<<20;//1Mb
static const int command_len = 4, responce_len = 4;
static constexpr int vers_command_len = 3;
static constexpr int push_command_len = hash_len/*\0*/ + 8/*bytes_size8*/;
static constexpr int chck_command_len = hash_len;
static constexpr int pull_command_len = hash_len/*\0*/ + 8/*bytes_size8*/ + 4/*bytes_size4*/;
static constexpr int take_responce_len = responce_len+pull_command_len;
static constexpr int size_command_len = hash_len;
static constexpr int size_responce_len = responce_len+8;
static const char* vers_command = "VERS";
static const char* chck_command = "CHCK";
static const char* size_command = "SIZE";
static const char* push_command = "PUSH";
static const char* pull_command = "PULL";
static const char* have_responce = "HAVE", *size_responce = "SIZE", *none_responce = "NONE", *take_responce = "TAKE";
static const char* err_bad_command = "ERBD", *err_file_error = "ERIO";
inline bool is_error_responce(const char* responce){return responce[0] == 'E' && responce[1] == 'R';}
}