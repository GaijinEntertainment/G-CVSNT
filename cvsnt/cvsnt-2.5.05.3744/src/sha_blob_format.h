#pragma once

#define SHA256_REV_STRING "sha256:"
#define BLOBS_SUBDIR "/blobs/"
static constexpr size_t sha256_magic_len = 7;//strlen(SHA256_REV_STRING);
static const size_t sha256_encoded_size = 64;//32*2

enum {BLOB_MAGIC_SIZE = 4};
struct BlobHeader
{
  unsigned char magic[BLOB_MAGIC_SIZE];
  unsigned int  headerSize;
  uint64_t uncompressedLen;
  uint64_t compressedLen;
};
static const unsigned char zlib_magic[BLOB_MAGIC_SIZE] = {'Z','L','I','B'};
static const unsigned char noarc_magic[BLOB_MAGIC_SIZE] = {'N','O','N','E'};
static bool is_accepted_magic(const unsigned char *m)
{
  return memcmp(m, zlib_magic, BLOB_MAGIC_SIZE) == 0 || memcmp(m, noarc_magic, BLOB_MAGIC_SIZE) == 0;
}

static bool is_packed_blob(const BlobHeader& hdr)
{
  return memcmp(hdr.magic, zlib_magic, BLOB_MAGIC_SIZE) == 0;
}

static bool is_zlib_blob(const BlobHeader& hdr)
{
  return memcmp(hdr.magic, zlib_magic, BLOB_MAGIC_SIZE) == 0;
}

BlobHeader get_noarc_header(size_t len)
{
  BlobHeader hdr;
  memcpy(hdr.magic, noarc_magic, BLOB_MAGIC_SIZE);
  hdr.headerSize = sizeof(BlobHeader);
  hdr.uncompressedLen = len;
  hdr.compressedLen = len;
  return hdr;
}
BlobHeader get_zlib_header(size_t len, size_t zlen)
{
  BlobHeader hdr;
  memcpy(hdr.magic, zlib_magic, BLOB_MAGIC_SIZE);
  hdr.headerSize = sizeof(BlobHeader);
  hdr.uncompressedLen = len;
  hdr.compressedLen = zlen;
  return hdr;
}


