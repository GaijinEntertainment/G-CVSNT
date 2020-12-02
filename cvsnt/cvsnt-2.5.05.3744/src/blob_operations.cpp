//#include "cvs.h"
#include "sha_blob_reference.h"
#include <string.h>
#include<stdio.h>

void error (int, int, const char *, ...);

inline bool is_encoded_hash(const char *d, size_t len)
{
  if (len != 64)
    return false;
  for (const char *e = d + len; d != e; ++d)
    if ((*d < '0' || *d > '9') && (*d < 'a' || *d > 'f'))
      return false;
  return true;
}

bool is_blob_reference_data(const void *data, size_t len)
{
  if (len != blob_reference_size || memcmp(data, HASH_TYPE_REV_STRING, hash_type_magic_len) != 0)
    return false;
  if (!is_encoded_hash((const char*)data + hash_type_magic_len, hash_encoded_size))
    return false;
  return true;
}

bool get_blob_reference_content_hash(const unsigned char *ref_file_content, size_t len, char *hash_encoded)//hash_encoded==char[65], encoded 32 bytes + \0
{
  if (len != blob_reference_size || memcmp(&ref_file_content[0], HASH_TYPE_REV_STRING, hash_type_magic_len) != 0)
    //not a blob reference!
    return false;
  //may be check if it is hash encoded
  if (!is_encoded_hash((const char*)ref_file_content + hash_type_magic_len, hash_encoded_size))
    return false;
  memcpy(hash_encoded, ref_file_content + hash_type_magic_len, hash_encoded_size);
  hash_encoded[hash_encoded_size] = 0;
  return true;
}

uint64_t get_user_session_salt();//link time resolved dependency

inline const uint64_t hash_with_salt(const void *data_, size_t len, uint64_t salt)
{
  const uint8_t* data = (const uint8_t* )data_;
  uint64_t result = salt;
  for (size_t i = 0; i < len; ++i)
    result = (result^uint64_t(data[i])) * uint64_t(1099511628211);
  return result;
}
const uint64_t get_session_crypt(const void *data, size_t len)
{
  //make salted hash of content where salt is randomly generated on each session.
  return *(const uint64_t*)((const char*)data + hash_type_magic_len+1);
}

bool gen_session_crypt(void *data, size_t len)
{
  if (len != session_blob_reference_size || memcmp(data, HASH_TYPE_REV_STRING, hash_type_magic_len) != 0)
    return false;
  //write salted hash of hash
  *(uint64_t*)((char*)data + blob_reference_size) = hash_with_salt(data, blob_reference_size, get_user_session_salt());
  return true;
}

bool is_session_blob_reference_data(const void *data, size_t len)
{
  if (len != session_blob_reference_size || memcmp(data, HASH_TYPE_REV_STRING, hash_type_magic_len) != 0)
    return false;
  if (*(const uint64_t*)((const char*)data + blob_reference_size) != hash_with_salt(data, blob_reference_size, get_user_session_salt()))
    return false;
  return true;
}

#include <errno.h>

size_t get_file_size(const char *file);
bool get_session_blob_reference_hash(const char *blob_ref_file_name, char *hash_encoded)//hash_encoded==char[65], encoded 32 bytes + \0
{
  if (get_file_size(blob_ref_file_name) != session_blob_reference_size)
    return false;
  unsigned char session_ref_file_content[session_blob_reference_size];
  FILE* fp;
  fp = fopen(blob_ref_file_name, "rb");
  if (fread(&session_ref_file_content[0],1, session_blob_reference_size, fp) != session_blob_reference_size)
  {
    error(1,errno,"Couldn't read %s", blob_ref_file_name);
    return false;
  }
  fclose(fp);
  if (!is_session_blob_reference_data(session_ref_file_content, session_blob_reference_size))
    return false;
  memcpy(hash_encoded, session_ref_file_content+hash_type_magic_len, hash_encoded_size);
  return is_blob_reference_data(session_ref_file_content, blob_reference_size);
}
