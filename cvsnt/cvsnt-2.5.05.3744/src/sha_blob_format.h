#pragma once
#include <stdint.h>
#include <string.h>

#define BLOBS_SUBDIR_BASE "blobs"
#define BLOBS_SUBDIR "/blobs/"

enum {BLOB_MAGIC_SIZE = 4};
struct BlobHeader
{
  enum {BEST_POSSIBLE_COMPRESSION = 0x01};//we have tried our best. That's done only by server maintainance utility, clients and server on the fly tries to perform faster
  unsigned char magic[BLOB_MAGIC_SIZE];
  uint16_t  headerSize;
  uint16_t  flags;
  uint64_t uncompressedLen;
  uint64_t compressedLen;
};
static const unsigned char zlib_magic[BLOB_MAGIC_SIZE] = {'Z','L','I','B'};
static const unsigned char zstd_magic[BLOB_MAGIC_SIZE] = {'Z','S','T','D'};
static const unsigned char noarc_magic[BLOB_MAGIC_SIZE] = {'N','O','N','E'};
static bool is_accepted_magic(const unsigned char *m)
{
  return memcmp(m, zstd_magic, BLOB_MAGIC_SIZE) == 0 ||
         memcmp(m, zlib_magic, BLOB_MAGIC_SIZE) == 0 ||
         memcmp(m, noarc_magic, BLOB_MAGIC_SIZE) == 0;
}

static inline bool is_zlib_blob(const BlobHeader& hdr)
{
  return memcmp(hdr.magic, zlib_magic, BLOB_MAGIC_SIZE) == 0;
}
static inline bool is_zstd_blob(const BlobHeader& hdr)
{
  return memcmp(hdr.magic, zstd_magic, BLOB_MAGIC_SIZE) == 0;
}

static bool is_packed_blob(const BlobHeader& hdr)
{
  return is_zstd_blob(hdr) || is_zlib_blob(hdr);
}
