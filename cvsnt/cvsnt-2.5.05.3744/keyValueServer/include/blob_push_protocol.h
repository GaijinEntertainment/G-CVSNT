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
//"VERS": has to be followed by "XXX" (client version), then 1byte of root length, then root. answers are ERBD or NONE
//"SIZE": has to be followed by "blake3bin_hash_32". Responses are: "SIZE", "NONE", "ERXX" (xx - is error code).
//"CHCK": has to be followed by "blake3bin_hash_32". Responses are: "HAVE", "NONE", "ERXX" (xx - is error code).
//"PUSH": has to be followed by "blake3bin_hash_32bytes_size8". Responses are: "HAVE", "ERXX" (xx - is error code).
//"STRM": has to be followed by "blake3bin_hash_32". Responses are: "HAVE", "ERXX" (xx - is error code).
//"PULL": has to be followed by "blake3bin_hash_32bytes_size8". Responses are: "TAKE", "NONE", "ERXX" (xx - is error code). You can ask for 0:0 if you want whole file

//SIZE response is SIZEbytes_size8
//TAKE response is identical to PULL, except first 4 bytes, TAKEblake3:bin_hash\0bytes_size8bytes_from4

//after PUSH, client will immediately write bytes_size data. This data should be prepared blob, which original unpacked content is hashed to hex_encoded_hash with blake3
//after STRM, client will push chunks till then end of data. The last chunk will be of a zero size - marker of end of stream
//after TAKE response, server will immediately write full bytes_size data (starting from bytes_from<<20 of blob). Allows pull by 1mb chunks.
//bytes_size8,bytes_size4 are binary in little endian

//Stream chunks format is simple 2 bytes of size (uint16_t) + binary data of this size. If size is zero, stream ends

static constexpr size_t pull_chunk_size = 1<<20;//1Mb
static const int command_len = 4, response_len = 4;
static constexpr int vers_command_len = 3;
static constexpr int push_command_len = hash_len/*\0*/ + 8/*bytes_size8*/;
static constexpr int chck_command_len = hash_len;
static constexpr int pull_command_len = hash_len/*\0*/ + 8/*bytes_size8*/ + 4/*bytes_size4*/;
static constexpr int strm_command_len = hash_len;
static constexpr int take_response_len = response_len+pull_command_len;
static constexpr int size_command_len = hash_len;
static constexpr int size_response_len = response_len+8;
static const char* vers_command = "VERS",
                 * chck_command = "CHCK",
                 * size_command = "SIZE",
                 * push_command = "PUSH",
                 * strm_command = "STRM",
                 * pull_command = "PULL";

static const char *have_response = "HAVE",
                  *size_response = "SIZE",
                  *none_response = "NONE",
                  *take_response = "TAKE";
static const char* err_bad_command = "ERBD", *err_file_error = "ERIO";
inline bool is_error_response(const char* response){return response[0] == 'E' && response[1] == 'R';}
}