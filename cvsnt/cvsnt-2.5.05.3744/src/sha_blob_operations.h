#pragma once
#include "sha_blob_format.h"

#define SHA256_LIST(a)\
(a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5],(a)[6],(a)[7],(a)[8],(a)[9],\
(a)[10],(a)[11],(a)[12],(a)[13],(a)[14],(a)[15],(a)[16],(a)[17],(a)[18],(a)[19],\
(a)[20],(a)[21],(a)[22],(a)[23],(a)[24],(a)[25],(a)[26],(a)[27],(a)[28],(a)[29],\
(a)[30],(a)[31]

void encode_sha256(unsigned char sha256[], char sha256_encoded[], size_t enc_len);//sha256 char[32], sha256_encoded[65]
void get_blob_filename_from_encoded_sha256(const char *root_dir, const char* encoded_sha256, char *sha_file_name, size_t sha_max_len);// sha_file_name =root/blobs/xx/yy/zzzzzzz
void get_blob_filename_from_sha256(const char *root, unsigned char sha256[], const char *fn, char *sha_file_name, size_t sha_max_len);//sha256 char[32]

//
void calc_sha256(const char *fn, const void *data, size_t len, bool src_blob, size_t &unpacked_len, unsigned char sha256[]);//sha256 char[32]
bool calc_sha256_file(const char *fn, unsigned char sha256[]);//sha256 char[32]

bool does_blob_exist(const char *sha_file_name);
void create_dirs(const char *root, unsigned char sha256[]);
enum class BlobPackType {NO, FAST, BEST};//try to pack
void atomic_write_sha_file(const char *fn, const char *sha_file_name, const void *data, size_t len, BlobPackType pack, bool src_packed);


//ideally we should receive already packed data, UNPACK it (for sha computations), and then store packed. That way compression moved to client
//after call to this function binary blob is stored
//returns zero if nothing written
bool write_prepacked_binary_blob(const char *root, const char *client_sha256,
  const void *data, size_t len);

size_t write_binary_blob(const char *root, unsigned char sha256[],// 32 bytes
  const char *fn,//fn write to
  const void *data, size_t len, BlobPackType pack, bool src_packed);//fn is just context!


BlobHeader get_binary_blob_hdr(const char *blob_file_name);
size_t decode_binary_blob(const char *blob_file_name, void **data);

//allocates memory and read whole file to memory, but checks if it's size is consistent;
size_t read_binary_blob_directly(const char *blob_file_name, void **data);
size_t read_binary_blob(const char *blob_file_name, void **data, bool return_blob_directly);

//this creates two memory chunks; hdr_  and blob_data (the last should be freed if, and only if allocated_blob_data = true)
void create_binary_blob_to_send(const char *ctx, void *file_content, size_t len, bool guess_packed, BlobHeader **hdr_, void** blob_data, bool &allocated_blob_data, char*sha256_encoded, size_t sha256_encoded_len);
size_t decode_binary_blob(const char *context, const void *data, size_t fileLen, void **out_data, bool &need_free);

//link time resolved dependency!
//files
//FILE *cvs_temp_file(char **filename, const char *mode = nullptr);
void rename_file (const char *from, const char *to);
size_t get_file_size(const char *file);
int isreadable (const char *file);
int blob_mkdir (const char *path, int mode);
//mem
void *blob_alloc(size_t sz);
void blob_free(void *);
