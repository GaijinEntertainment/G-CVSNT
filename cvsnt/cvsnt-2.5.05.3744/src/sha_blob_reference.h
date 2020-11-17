#pragma once
#include "sha_blob_format.h"
#include "sha_blob_operations.h"
static const size_t blob_reference_size = sha256_magic_len+sha256_encoded_size;

static bool can_be_blob_reference(const char *blob_ref_file_name)
{
  return get_file_size(blob_ref_file_name) == blob_reference_size;//blob references is always sha256:encoded_sha_64_bytes
}

static bool is_blob_reference(const char *root_dir, const char *blob_ref_file_name, char *sha_file_name, size_t sha_max_len)//sha256_encode==char[65], encoded 32 bytes + \0
{
  if (!can_be_blob_reference(blob_ref_file_name))
    return false;
  FILE* fp;
  fp = fopen(blob_ref_file_name, "rb");
  char sha256_magic[sha256_magic_len];
  if (fread(&sha256_magic[0],1, sha256_magic_len, fp) != sha256_magic_len)
  {
    error(1,errno,"Couldn't read %s", blob_ref_file_name);
    return false;
  }
  if (memcmp(&sha256_magic[0], SHA256_REV_STRING, sha256_magic_len) != 0)
  {
    //not a blob reference!
    fclose(fp);
    return false;
  }
  char sha256_encoded[sha256_encoded_size+1];
  if (fread(&sha256_encoded[0], 1, sha256_encoded_size, fp) != sha256_encoded_size)
  {
    error(1,errno,"Couldn't read %s", blob_ref_file_name);
    return false;
  }
  sha256_encoded[sha256_encoded_size] = 0;
  get_blob_filename_from_encoded_sha256(root_dir, sha256_encoded, sha_file_name, sha_max_len);
  return does_blob_exist(sha_file_name);
}

static void write_blob_reference(const char *fn, unsigned char sha256[])//sha256[32] == digest
{
  char *temp_filename = NULL;
  FILE *dest = cvs_temp_file(&temp_filename, "wb");
  if (!dest)
    error(1, 0, "can't open write temp_filename <%s> for <%s>", temp_filename, fn);
  if (fprintf(dest,
      SHA256_REV_STRING
	  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	  SHA256_LIST(sha256)) != blob_reference_size)
  {
    error(1, 0, "can't write to temp <%s> for <%s>!", temp_filename, fn);
  } else
    rename_file (temp_filename, fn);
  fclose(dest);
  xfree (temp_filename);
}

static void write_blob_and_blob_reference(const char *root, const char *fn, const void *data, size_t len, bool store_packed, bool src_packed)
{
  unsigned char sha256[32];
  write_binary_blob(root, sha256, fn, data, len, store_packed, src_packed);
  write_blob_reference(fn, sha256);
}
