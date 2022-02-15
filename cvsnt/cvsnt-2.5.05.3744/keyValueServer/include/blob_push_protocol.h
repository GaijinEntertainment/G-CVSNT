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
//root is not a command, it is part of VERS. it is 1byte of root length, then root of up to 255 bytes.
//"VERS": is first and unencrypted, this is auth prompt. Has to be always followed by "XXX" (client version), then

//A) if client is at auth+ version, then client will sends otp page no of 8 bytes, and then 8 random bytes and DH parameters encrypted with OTP
//   server will send it's 8 random bytes and its DH parameters encrypted with OTP
//   server and client both generate same session keys using 8+8 decrypted random bytes OTP page and DH parameters.
//     they should both get same results
//   client should then immediately send with encrypted (using session keys) it's current 64bit of timestamp, and 64 bits of ones (padding) to finalize handshake.
//     (that's "Client Ready" message)
//   server will answer (using same session keys pair) 64 bit of ones 'HAVE' (yes, encryption will be used), 'NONE' (encryption won't be used anymore), 'ERIO' (invalid timestamp) or EBRD (invalid version)
//     (that's "Server Ready" message)
//   clients always send encrypted root, regardless of if we want to continue negotiotion

//A) if it is auth+ version, then otp page of 8 bytes, and then the answer is two encryption keys (encrypted with OTP)
//   client should immediately respond with encrypted (using provided key) it's current 64bit of timestamp, and 64 bits of ones to finalize handshake.
//   and then encrypted root
//   server will answer (using same encryption key pair) 'HAVE' (yes, encryption will be used), 'NONE' (encryption won't be used anymore), 'ERIO' (invalid timestamp) or EBRD (invalid version)

//B) if it is prototype version, then immediately root should be sent
//  the answer is either NONE (which is OK, if we can work with unencrypted protocol then), or EBRD (invalid version, not allowing prototype clients)

//all these commands can be encrypted (all or nothing):
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