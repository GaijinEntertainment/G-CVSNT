#pragma once
#include "sha_blob_format.h"
#include "sha_blob_operations.h"
static const size_t blob_reference_size = sha256_magic_len+sha256_encoded_size;

static bool can_be_blob_reference(const char *blob_ref_file_name)
{
  return get_file_size(blob_ref_file_name) == blob_reference_size;//blob references is always sha256:encoded_sha_64_bytes
}

static bool get_blob_reference_content_sha256(const unsigned char *ref_file_content, size_t len, char *sha256_encoded)//sha256_encoded==char[65], encoded 32 bytes + \0
{
  if (len != blob_reference_size || memcmp(&ref_file_content[0], SHA256_REV_STRING, sha256_magic_len) != 0)
    //not a blob reference!
    return false;
  //todo: may be check if it is sha256 encoded
  memcpy(sha256_encoded, ref_file_content + sha256_magic_len, sha256_encoded_size);
  sha256_encoded[sha256_encoded_size] = 0;
  return true;
}

static bool get_blob_reference_sha256(const char *blob_ref_file_name, char *sha256_encoded)//sha256_encoded==char[65], encoded 32 bytes + \0
{
  if (!can_be_blob_reference(blob_ref_file_name))
    return false;
  unsigned char ref_file_content[blob_reference_size];
  FILE* fp;
  fp = fopen(blob_ref_file_name, "rb");
  if (fread(&ref_file_content[0],1, blob_reference_size, fp) != blob_reference_size)
  {
    error(1,errno,"Couldn't read %s", blob_ref_file_name);
    return false;
  }
  fclose(fp);
  return get_blob_reference_content_sha256(ref_file_content, blob_reference_size, sha256_encoded);
}

static bool is_blob_reference(const char *root_dir, const char *blob_ref_file_name, char *sha_file_name, size_t sha_max_len)//sha256_encode==char[65], encoded 32 bytes + \0
{
  char sha256_encoded[sha256_encoded_size+1];
  if (!get_blob_reference_sha256(blob_ref_file_name, sha256_encoded))//sha256_encoded==char[65], encoded 32 bytes + \0
    return false;
  get_blob_filename_from_encoded_sha256(root_dir, sha256_encoded, sha_file_name, sha_max_len);
  return does_blob_exist(sha_file_name);
}

static void write_direct_blob_reference(const char *fn, const void *ref, size_t ref_len)//ref_len == blob_reference_size
{
  if (ref_len != blob_reference_size)
    error(1, 0, "not a reference <%s>", fn);
  char *temp_filename = NULL;
  FILE *dest = cvs_temp_file(&temp_filename, "wb");
  if (!dest)
    error(1, 0, "can't open write temp_filename <%s> for <%s>", temp_filename, fn);
  if (fwrite(ref, 1, ref_len, dest) != ref_len)
  {
    error(1, 0, "can't write to temp <%s> for <%s>!", temp_filename, fn);
  } else
    rename_file (temp_filename, fn);
  fclose(dest);
  xfree (temp_filename);
}

static void write_blob_reference(const char *fn, unsigned char sha256[])//sha256[32] == digest
{
 char sha256_encoded[blob_reference_size+1];
 snprintf(sha256_encoded, sizeof(sha256_encoded), SHA256_REV_STRING
	  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	  SHA256_LIST(sha256));
  write_direct_blob_reference(fn,  sha256_encoded, blob_reference_size);
}

static void write_blob_and_blob_reference(const char *root, const char *fn, const void *data, size_t len, bool store_packed, bool src_packed)
{
  unsigned char sha256[32];
  write_binary_blob(root, sha256, fn, data, len, store_packed, src_packed);
  write_blob_reference(fn, sha256);
}
