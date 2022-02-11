#pragma once
#include <string.h>
//32bytes hash -> 64 hex and bake;
bool bin_hash_to_hex_string_s(const unsigned char *blob_hash, char *to_hash_hex_string, size_t hash_hex_string_capacity);
bool hex_string_to_bin_hash_s(const char *from_hash_hex_string, unsigned char *to_blob_hash, size_t blob_hash_capacity);
///
bool bin_hash_to_hex_string(const unsigned char *blob_hash, char *to_hash_hex_string);
bool hex_string_to_bin_hash(const char *from_hash_hex_string, unsigned char *to_blob_hash);

///
//convert hash_type "blake3"/"sha256" + hex_encoded_hash "FEDCBA987654321012345678abcdef" to blob_hash and back (which our packets operates with)
bool encode_hash_str_to_blob_hash_s(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash, size_t blob_hash_capacity);
bool decode_blob_hash_to_hex_hash_s(const unsigned char *blob_hash, size_t blob_hash_len, char *hash_type, size_t hash_type_capacity, char *hash_hex_string, size_t hash_hex_string_capacity);
bool encode_hash_str_to_blob_hash(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash);
bool decode_blob_hash_to_hex_hash(const unsigned char *blob_hash, char *hash_type, char *hash_hex_string);

inline bool bin_hash_to_hex_string_noend(const unsigned char *hash, size_t hash_len, char *to_hash_hex_string, size_t hash_hex_string_capacity)
{
  if (hash_hex_string_capacity < hash_len*2)
    return false;
  static const char *hex_remap ="0123456789abcdef";
  for (int i = 0; i < hash_len; ++i, ++hash, to_hash_hex_string+=2)
  {
    to_hash_hex_string[0] = hex_remap[(*hash)>>4];
    to_hash_hex_string[1] = hex_remap[(*hash)&0xF];
  }
  return true;
}

inline bool bin_hash_to_hex_string_s(const unsigned char *hash, size_t hash_len, char *to_hash_hex_string, size_t hash_hex_string_capacity)
{
  if (hash_hex_string_capacity < hash_len*2 + 1)
    return false;
  if (!bin_hash_to_hex_string_noend(hash, hash_len, to_hash_hex_string, hash_hex_string_capacity))
    return false;
  to_hash_hex_string[hash_len*2] = 0;
  return true;
}

inline bool bin_hash_to_hex_string_s(const unsigned char *blob_hash, char *to_hash_hex_string, size_t hash_hex_string_capacity)
{
  return bin_hash_to_hex_string_s(blob_hash, 32, to_hash_hex_string, hash_hex_string_capacity);
}

inline bool bin_hash_to_hex_string(const unsigned char *blob_hash, char *to_hash_hex_string)
{ return bin_hash_to_hex_string_s(blob_hash, to_hash_hex_string, 65); }

inline int decode_hash_symbol(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

inline bool hex_string_to_bin_hash(const char *from_hash_hex_string, size_t string_len, unsigned char *hash, size_t hash_capacity)
{
  if (hash_capacity < string_len/2)
    return false;
  for (int i = 0; i < string_len/2; ++i, from_hash_hex_string+=2, ++hash)
  {
    int s0 = decode_hash_symbol(from_hash_hex_string[0]);
    if (s0 < 0)
      return false;
    int s1 = decode_hash_symbol(from_hash_hex_string[1]);
    if (s1 < 0)
      return false;
    *hash = (unsigned char)((s1) | (s0<<4));
  }
  return true;
}

inline bool hex_string_to_bin_hash_s(const char *from_hash_hex_string, unsigned char *blob_hash, size_t blob_hash_capacity)
{
  if (blob_hash_capacity < 32)
    return false;
  return hex_string_to_bin_hash(from_hash_hex_string, 64, blob_hash, blob_hash_capacity);
}

inline bool hex_string_to_bin_hash(const char *from_hash_hex_string, unsigned char *to_blob_hash)
{ return hex_string_to_bin_hash_s(from_hash_hex_string, to_blob_hash, 32); }

///======Blob specific Hash manipulation=======
inline bool encode_hash_str_to_blob_hash_s(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash, size_t blob_hash_capacity)
{
  if (blob_hash_capacity < 32+6)
    return false;
  memcpy(blob_hash, hash_type, 6);
  return hex_string_to_bin_hash(hash_hex_string, blob_hash+6);
}

inline bool encode_hash_str_to_blob_hash(const char *hash_type, const char *hash_hex_string, unsigned char *blob_hash)
{ return encode_hash_str_to_blob_hash(hash_type, hash_hex_string, blob_hash); }

//requires: hash_type_capacity >= 8, blob_hash_capacity==32+7, hash_hex_string_capacity>=65
//prints hash_type, and hash_hex_string, including trailing zero
inline bool decode_blob_hash_to_hex_hash_s(const unsigned char *blob_hash, size_t blob_hash_len,
  char *hash_type, size_t hash_type_capacity, char *hash_hex_string, size_t hash_hex_string_capacity)
{
  if (blob_hash_len != 32+6 || hash_type_capacity<7 || hash_hex_string_capacity<65)
    return false;
  memcpy(hash_type, blob_hash, 6);hash_type[6] = 0;
  return bin_hash_to_hex_string(blob_hash+6, hash_hex_string);
}

inline bool decode_blob_hash_to_hex_hash(const unsigned char *blob_hash, char *hash_type, char *hash_hex_string)
{ return decode_blob_hash_to_hex_hash_s(blob_hash, 32+6, hash_type, 7, hash_hex_string, 65); }

inline void memcpy_to(void* &to, const void* from, size_t sz) {memcpy(to, from, sz); to=(char*)to + sz;}
inline void memcpy_from(void* to, const void* &from, size_t sz) {memcpy(to, from, sz); from=(const char*)from + sz;}