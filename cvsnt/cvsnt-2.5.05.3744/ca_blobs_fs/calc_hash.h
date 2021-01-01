#pragma once

enum {HASH_CONTEXT_SIZE = 2048};
bool init_blob_hash_context(char *ctx, size_t ctx_size);
void update_blob_hash(char *ctx, const char *data, size_t data_size);
bool finalize_blob_hash(char *ctx, unsigned char *digest, size_t digest_size=32);//digest_size <= 32

inline bool bin_hash_to_hex_string_64(const unsigned char *blob_hash, char *to_hash_hex_string)//copy paste
{
  static const char *hex_remap ="0123456789abcdef";
  for (int i = 0; i < 32; ++i, ++blob_hash, to_hash_hex_string+=2)
  {
    to_hash_hex_string[0] = hex_remap[(*blob_hash)>>4];
    to_hash_hex_string[1] = hex_remap[(*blob_hash)&0xF];
  }
  return true;
}

inline bool is_encoded_hash(const char *d, size_t len)
{
  if (len < 64)//longer lines are accepted
    return false;
  for (const char *e = d + 64; d != e; ++d)
    if ((*d < '0' || *d > '9') && (*d < 'a' || *d > 'f'))
      return false;
  return true;
}
