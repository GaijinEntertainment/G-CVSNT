//#include "cvs.h"
#include "sha_blob_format.h"
#include "sha_blob_reference.h"
#include "sha_blob_operations.h"
#include "streaming_compressors.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(Spec) /* empty */
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif
void error (int, int, const char *, ...)  __attribute__ ((__format__ (__printf__, 3, 4)));
//files
FILE *cvs_temp_file(char **filename, const char *mode = nullptr);
bool rename_file (const char *from, const char *to, bool fail);
size_t get_file_size(const char *file);
int isreadable (const char *file);
int blob_mkdir (const char *path, int mode);
bool change_file_mode(const char *file, int mode);
//mem
void *blob_alloc(size_t sz);
void blob_free(void *);

#define TRY_ZLIB_AS_WELL_ON_BEST 0//if 1, on Pack::BEST we will try both algoritms to find out whats working best

inline bool is_encoded_hash(const char *d, size_t len)
{
  if (len != 64)
    return false;
  for (const char *e = d + len; d != e; ++d)
    if ((*d < '0' || *d > '9') && (*d < 'a' || *d > 'f'))
      return false;
  return true;
}

void encode_hash(unsigned char hash[], char hash_encoded[], size_t enc_len)//hash char[32], hash_encoded[65]
{
  if (enc_len < hash_encoded_size+1)
    error (1, 0, "too short %d", (int)enc_len);

  snprintf(hash_encoded, enc_len,
     "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      HASH_LIST(hash));
}

void get_blob_filename_from_encoded_hash(const char *root_dir, const char* encoded_hash, char *sha_file_name, size_t sha_max_len)
{
  if (snprintf(sha_file_name, sha_max_len,
     "%s"
     BLOBS_SUBDIR
     "%.2s/%.2s/%.60s"
     , root_dir ,
     encoded_hash,
     encoded_hash+2,
     encoded_hash+4
     )
     >= sha_max_len-1)
      error(1, 0, "get_blob_name:too long filename <%s> for <%s>", sha_file_name, encoded_hash);
}

void get_blob_filename_from_hash(const char *root, unsigned char hash[], const char *fn, char *sha_file_name, size_t sha_max_len)//hash char[32]
{
  if (snprintf(sha_file_name, sha_max_len,
     "%s"
     BLOBS_SUBDIR
     "%02x/%02x/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
     , root,
      HASH_LIST(hash)
     )
     >= sha_max_len-1)
  {
    error(1, 0, "create_blob_name: too long filename <%s> for <%s>", sha_file_name, fn);
  }
}

void calc_hash(const char *fn, const void *data, size_t len, bool src_blob, size_t &unpacked_len, unsigned char hash[])//hash char[32]
{
  unpacked_len = len;
  char hctx[HASH_CONTEXT_SIZE];
  init_blob_hash_context(hctx, sizeof(hctx));

  if (src_blob)
  {
    //new protocol - client sends data as blob, already packed and everything.
    //we still need to verify it!
    //calc hash on the unpacked data!
    const BlobHeader &hdr = *(const BlobHeader*)data;
    unpacked_len = hdr.uncompressedLen;
    if (!is_packed_blob(hdr))
    {
      update_blob_hash(hctx, ((const char*)data + hdr.headerSize), unpacked_len);
    } else if (is_zlib_blob(hdr) || is_zstd_blob(hdr))
    {
      char cctx[BLOB_STREAM_CTX_SIZE];
      if (!init_decompress_blob_stream(cctx, sizeof(cctx), is_zlib_blob(hdr) ? BlobStreamType::ZLIB : BlobStreamType::ZSTD))
        error(1,0,"Can't init ctx");
      const char *src = (const char *)((const char*)data + hdr.headerSize);
	  size_t src_pos = 0;
      char bufOut[32768];
      unpacked_len = 0;

      while (src_pos < len)
      {
        size_t dest_pos = 0;
        BlobStreamStatus status = decompress_blob_stream(cctx, src, src_pos, hdr.uncompressedLen, bufOut, dest_pos, sizeof(bufOut));
        update_blob_hash(hctx, bufOut, dest_pos);
        unpacked_len += dest_pos;
        if (status != BlobStreamStatus::Continue)
          break;
      }
      finish_decompress_blob_stream(cctx);
    } else if (is_zstd_blob(hdr))
    {
    } else
    {
      error(1,0,"Unknown compressor %s", fn);
    }
  } else
  {
    update_blob_hash(hctx, (const char*)data, len);
  }
  finalize_blob_hash(hctx, hash);
}

bool calc_hash_file(const char *fn, unsigned char hash[])//hash char[32]
{
  char hctx[HASH_CONTEXT_SIZE];
  init_blob_hash_context(hctx, sizeof(hctx));

  FILE* fp = fopen(fn,"rb");
  if (!fp)
    return false;
  size_t len = get_file_size(fn);
  char buf[32768];
  while (len > 0)
  {
    size_t sz = len < sizeof(buf) ? len : sizeof(buf);
    if (fread(buf,1,sz,fp) != sz)
    {
      fclose(fp);
      return false;
    }
    update_blob_hash(hctx, buf, sz);
    len -= sz;
  }
  fclose(fp);

  finalize_blob_hash(hctx, hash);
  return true;
}

bool does_blob_exist(const char *sha_file_name)
{
  return get_file_size(sha_file_name) >= sizeof(BlobHeader);
}

void create_dirs(const char *root, unsigned char hash[])
{
  char buf[1024];
  if (snprintf(buf, sizeof(buf),"%s" BLOBS_SUBDIR "%02x", root, hash[0]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (blob_mkdir(buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
  if (snprintf(buf, sizeof(buf),"%s" BLOBS_SUBDIR "%02x/%02x", root, hash[0], hash[1]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (blob_mkdir(buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
}


static inline BlobHeader get_noarc_header(size_t len, uint16_t flags = 0) {return get_header(noarc_magic, len, len, flags);}
static inline BlobHeader get_zlib_header(size_t len, size_t zlen, uint16_t flags = 0) {return get_header(zlib_magic, len, zlen, flags);}
static inline BlobHeader get_zstd_header(size_t len, size_t zlen, uint16_t flags = 0) {return get_header(zstd_magic, len, zlen, flags);}

static const int zstd_fast_comp = 3;
static const int zstd_best_comp = 19;//we dont use ultra as it consumes too much memory
static const int zlib_fast_comp = 3;
static const int zlib_best_comp = 9;//we dont use ultra as it consumes too much memory

static void* compress_blob_data(const void *data, size_t len, BlobPackType pack, BlobHeader &hdr, BlobStreamType type = BlobStreamType::ZSTD)
{
  const int compression_level = type == BlobStreamType::ZSTD ? (pack == BlobPackType::FAST ? zstd_fast_comp : zstd_best_comp) :
                                                               (pack == BlobPackType::FAST ? zlib_fast_comp : zlib_best_comp);
  char cctx[BLOB_STREAM_CTX_SIZE];
  size_t zlen = 0, zbound = 0;
  if (init_compress_blob_stream(cctx, sizeof(cctx), compression_level, type) && ((zbound = compress_blob_bound(cctx, len)) != 0))
  {
    void *zbuf = blob_alloc(zbound);
    if (compress_blob_stream_and_finish(cctx, (const char *)data, len, (char*)zbuf, zlen, zbound) == BlobStreamStatus::Finished && zlen < len - len/20)
    {
      hdr = get_header(type == BlobStreamType::ZSTD ? zstd_magic : zlib_magic, len, zlen, pack == BlobPackType::FAST ? 0 : BlobHeader::BEST_POSSIBLE_COMPRESSION);
      return zbuf;
    } else
      blob_free(zbuf);
  }
  hdr = get_header(noarc_magic, len, len, 0);
  return nullptr;
}


size_t atomic_write_sha_file(const char *fn, const char *sha_file_name, const void *data, size_t len, BlobPackType pack, bool src_packed)
{
  char *temp_filename = NULL;
  FILE *dest = cvs_temp_file(&temp_filename, "wb");
  if (!dest)
  {
    error(1, errno, "can't open write temp_filename <%s> for <%s>(<%s>)", temp_filename, sha_file_name, fn);
  }
  size_t written = 0;
  if (src_packed)
  {
    //new protocol - client sends already prepared blobs, just write them down
    if (fwrite(data,1,len,dest) != len)
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)len);
    written = len;
  } else
  {
    BlobHeader hdr = get_noarc_header(len);
    void *compressed_data = nullptr;
    if (pack != BlobPackType::NO)
      compressed_data = compress_blob_data(data, len, pack, hdr);//we use non-ultra method, unless we repack!

    const void* writeData = compressed_data ? compressed_data : data;

    if (fwrite(&hdr,1,sizeof(hdr),dest) != sizeof(hdr))
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)sizeof(hdr));
    if (fwrite(writeData,1,hdr.compressedLen,dest) != hdr.compressedLen)
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)hdr.compressedLen);
    written = hdr.compressedLen + hdr.headerSize;
    if (compressed_data != nullptr)
      blob_free(compressed_data);
  }
  change_file_mode(temp_filename, 0666);
  bool ret = rename_file (temp_filename, sha_file_name, false);//we dont care if blob is written independently
  fclose(dest);

  blob_free (temp_filename);
  return ret ? written : 0;
}

//ideally we should receive already packed data, UNPACK it (for sha computations), and then store packed. That way compression moved to client
//after call to this function binary blob is stored
//returns zero if nothing written
bool write_prepacked_binary_blob(const char *root, const char *client_hash,
  const void *data, size_t len)
{
  if (
      (len < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->headerSize < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->compressedLen + ((const BlobHeader*)data)->headerSize != len
       || !is_accepted_magic(((const BlobHeader*)data)->magic))
     )
  {
    error(0, 0, "Client <%s> says it's client prepared blob of len <%d> but it's not!", client_hash, (int)len);
    if (len >= sizeof(BlobHeader))
    {
      if (((const BlobHeader*)data)->headerSize < sizeof(BlobHeader))
        error(0, 0, "Blob header size = %d and should be at least <%d>!", ((const BlobHeader*)data)->headerSize, (int)sizeof(BlobHeader));
      if (((const BlobHeader*)data)->compressedLen + ((const BlobHeader*)data)->headerSize != len)
        error(0, 0, "Blob total size is %d and promised is %d!", (int)(((const BlobHeader*)data)->compressedLen + ((const BlobHeader*)data)->headerSize), (int)len);
      if (!is_accepted_magic(((const BlobHeader*)data)->magic))
        error(0, 0, "unaccepted magic <%.*s>!", (int)BLOB_MAGIC_SIZE, ((const BlobHeader*)data)->magic);
    }
    return false;
  }
  const size_t sha_file_name_len = 1024;
  char sha_file_name[sha_file_name_len];//can be dynamically allocated, as 64+3+1 + strlen(root). Yet just don't put it that far!
  get_blob_filename_from_encoded_hash(root, client_hash, sha_file_name, sha_file_name_len);
  if (isreadable(sha_file_name))
  {
    //no need to check if client is lying about this sha content
    printf("sha <%s> already exist, deduplication\n", client_hash);
    return true;
  }

  const size_t clientUnpackedLen = ((const BlobHeader*)data)->uncompressedLen;
  size_t unpacked_len = 0;
  unsigned char hash[32];
  calc_hash(client_hash, data, len, true, unpacked_len, hash);
  if (clientUnpackedLen != unpacked_len)
  {
    error(0, 0, "Client <%s> says it has compressed %d of data but we decompressed only %d!",client_hash , (uint32_t)clientUnpackedLen, (uint32_t)unpacked_len);
    return false;
  }
  char real_hash_encoded[hash_encoded_size+1];
  encode_hash(hash, real_hash_encoded, sizeof(real_hash_encoded));
  if (memcmp(client_hash, real_hash_encoded, hash_encoded_size) != 0)
  {
    error(0, 0, "client-promised sha <%s> and real sha<%s> different!", client_hash, real_hash_encoded);
    return false;
  }
  get_blob_filename_from_hash(root, hash, client_hash, sha_file_name, sha_file_name_len);
  if (!isreadable(sha_file_name))
  {
    create_dirs(root, hash);
    atomic_write_sha_file(client_hash, sha_file_name, data, len, BlobPackType::NO, true);
    return true;
  }//else we already have this blob written. deduplication worked!
  return true;
}

size_t write_binary_blob(const char *root, unsigned char hash[],// 32 bytes
  const char *fn,//write to
  const void *data, size_t len, BlobPackType pack, bool src_packed)//fn is just context!
{
  if (src_packed &&
      (len < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->headerSize < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->compressedLen + ((const BlobHeader*)data)->headerSize != len
       || !is_accepted_magic(((const BlobHeader*)data)->magic))
     )
  {
    error(1, 0, "fn <%s> says it's client prepared blob of len <%d> but it's not!", fn, (int)len);
  }
  const size_t clientUnpackedLen = src_packed ? ((const BlobHeader*)data)->uncompressedLen : len;
  size_t unpacked_len = 0;
  calc_hash(fn, data, len, src_packed, unpacked_len, hash);
  if (clientUnpackedLen != unpacked_len)
    error(1, 0, "fn <%s> says it has compressed %d of data but we decompressed only %d!", fn, (uint32_t)clientUnpackedLen, (uint32_t)unpacked_len);
  const size_t sha_file_name_len = 1024;
  char sha_file_name[sha_file_name_len];//can be dynamically allocated, as 32+3+1 + strlen(root)
  get_blob_filename_from_hash(root, hash, fn, sha_file_name, sha_file_name_len);
  if (!isreadable(sha_file_name))
  {
    create_dirs(root, hash);
    return atomic_write_sha_file(fn, sha_file_name, data, len, pack, src_packed);
  }//else we already have this blob written. deduplication worked!
  else
  {
    //if we are paranoid, that's the place where we can check for collision
    //probability of hash collision is way lower then asteroid destroying Earth in next 1000 years
    // in addition, it won't crash our repo, merely will result in incorrect commit (you will update something else, not what you have commited, some other file)
    //so we won't check for it.
    return 0;
  }
}

BlobHeader get_binary_blob_hdr(const char *blob_file_name)
{
  size_t fileLen = get_file_size(blob_file_name);
  if (fileLen < sizeof(BlobHeader))
  {
    error(1,errno,"Couldn't read %s of %d len", blob_file_name, (int)fileLen);
    return BlobHeader{0};
  }
  FILE* fp;
  fp = fopen(blob_file_name,"rb");
  if (!fp)
  {
    error(1,errno,"Couldn't read %s len", blob_file_name);
    return BlobHeader{0};
  }
  BlobHeader hdr;
  if (fread(&hdr,1,sizeof(hdr),fp) != sizeof(hdr))
  {
    error(1,errno,"Couldn't read %s", blob_file_name);
    return hdr;
  }
  if (!is_accepted_magic(hdr.magic) || hdr.compressedLen != fileLen - hdr.headerSize)
  {
    error(1,errno,"<%s> is not a blob (%d bytes stored in file, vs file len = %d)", blob_file_name, (int)hdr.compressedLen, int(fileLen - sizeof(hdr)));
    return BlobHeader{0};
  }
  fclose(fp);
  return hdr;
}

size_t read_binary_blob_directly(const char *blob_file_name, void **data)//allocates memory and read whole file
{
  size_t fileLen = get_file_size(blob_file_name);
  if (fileLen < sizeof(BlobHeader))
  {
    error(1,errno,"Couldn't read %s of %d len", blob_file_name, (int)fileLen);
    return 0;
  }
  FILE* fp;
  fp = fopen(blob_file_name, "rb");
  if (!fp)
  {
    error(1,errno,"Couldn't read %s len", blob_file_name);
    return 0;
  }
  *data = blob_alloc(fileLen);
  if (fread(*data,1,fileLen,fp) != fileLen)
  {
    error(1,errno,"Couldn't read %s", blob_file_name);
    return 0;
  }
  const BlobHeader &hdr = *((const BlobHeader*)*data);
  if (hdr.compressedLen + hdr.headerSize != fileLen || !is_accepted_magic(hdr.magic))
  {
    error(1,errno,"<%s> is not a blob", blob_file_name);
    fclose(fp);
  }
  fclose(fp);
  return fileLen;
}

size_t decode_binary_blob(const char *context, const void *data, size_t fileLen, void **out_data, bool &need_free)
{
  if (fileLen < sizeof(BlobHeader))
  {
    error(1,errno,"Couldn't read %s of %d len", context, (int)fileLen);
    return 0;
  }

  const BlobHeader &hdr = *(const BlobHeader*)data;
  if (!is_accepted_magic(hdr.magic) || hdr.compressedLen + hdr.headerSize != fileLen)
  {
    error(1,errno,"<%s> is not a blob (%d bytes stored in file, vs file len = %d)", context, (int)hdr.compressedLen, int(fileLen - sizeof(hdr)));
    return 0;
  }
  const char* readData = ((const char*)data) + hdr.headerSize;

  //((char*)*data)[hdr.uncompressedLen] = 0;
  if (is_packed_blob(hdr))//
  {
    *out_data = blob_alloc(hdr.uncompressedLen);
    need_free = true;
    if (decompress_blob(readData, hdr.compressedLen, (char*)*out_data, hdr.uncompressedLen, is_zlib_blob(hdr) ? BlobStreamType::ZLIB : BlobStreamType::ZSTD) != BlobStreamStatus::Finished)
        error(1,0,"internal error: decompress failed");
  } else
  {
    *out_data = (void*)readData;
    need_free = false;
  }
  return hdr.uncompressedLen;
}

size_t decode_binary_blob(const char *blob_file_name, void **data)
{
  size_t fileLen = get_file_size(blob_file_name);
  if (fileLen < sizeof(BlobHeader))
  {
    error(1,errno,"Couldn't read %s of %d len", blob_file_name, (int)fileLen);
    return 0;
  }
  FILE* fp;
  fp = fopen(blob_file_name,"rb");
  if (!fp)
  {
    error(1,errno,"Couldn't read %s len", blob_file_name);
    return 0;
  }
  BlobHeader hdr;
  if (fread(&hdr,1,sizeof(hdr),fp) != sizeof(hdr))
  {
    error(1,errno,"Couldn't read %s", blob_file_name);
    return 0;
  }
  if (!is_accepted_magic(hdr.magic) || hdr.compressedLen != fileLen - sizeof(hdr))
  {
    error(1,errno,"<%s> is not a blob (%d bytes stored in file, vs file len = %d)", blob_file_name, (int)hdr.compressedLen, int(fileLen - sizeof(hdr)));
    return 0;
  }
  fseek(fp, hdr.headerSize, SEEK_SET);

  *data = blob_alloc(hdr.uncompressedLen);
  //((char*)*data)[hdr.uncompressedLen] = 0;
  if (is_packed_blob(hdr))//
  {
    char cctx[BLOB_STREAM_CTX_SIZE];
    if (!init_decompress_blob_stream(cctx, sizeof(cctx), is_zlib_blob(hdr) ? BlobStreamType::ZLIB : BlobStreamType::ZSTD))
      error(1,0,"Can't init ctx");
    char bufIn[32768];
    size_t dest_pos = 0;
	size_t totalRead = 0;
    while (totalRead < hdr.compressedLen)
    {
      size_t src_pos = 0;
      int dataRead = fread(bufIn, 1, sizeof(bufIn), fp);
      if (dataRead < 0)
      {
        error(1,errno,"Couldn't read %s", blob_file_name);
        return 0;
      }
	  totalRead += dataRead;
      BlobStreamStatus status = decompress_blob_stream(cctx, bufIn, src_pos, dataRead, (char*)*data, dest_pos, hdr.uncompressedLen);
      if (status == BlobStreamStatus::Finished)
        break;
      if (dest_pos > hdr.uncompressedLen)
      {
        error(1,errno,"Couldn't decode %s, corrupted", blob_file_name);
        return 0;
      }
    };
    finish_decompress_blob_stream(cctx);
  } else
  {
    if (fread(*data,1,hdr.uncompressedLen,fp) != hdr.uncompressedLen)
    {
      error(1,errno,"Couldn't read %s", blob_file_name);
      return 0;
    }
  }
  fclose(fp);
  return hdr.uncompressedLen;
}


size_t read_binary_blob(const char *blob_file_name, void **data, bool return_blob_directly)
{
  if (return_blob_directly)//new protocol - just send data to client as is. It is already packed and everything
    return read_binary_blob_directly(blob_file_name, data);
  return decode_binary_blob(blob_file_name, data);
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


void create_binary_blob_to_send(const char *ctx, void *file_content, size_t len, bool guess_packed, BlobHeader **hdr_, void** blob_data, bool &allocated_blob_data, char*hash_encoded, size_t sha_256enc_len)
{
  unsigned char hash[32];
  size_t unpacked_len;
  calc_hash(ctx, file_content, len, false, unpacked_len, hash);
  encode_hash(hash, hash_encoded, sha_256enc_len);//hash char[32], hash_encoded[65]
  *hdr_ = (BlobHeader*)blob_alloc(sizeof(BlobHeader));
  BlobHeader &hdr = **hdr_;
  hdr = get_noarc_header(len);
  void *compressed_data = nullptr;
  if (guess_packed)
    compressed_data = compress_blob_data(file_content, len, BlobPackType::FAST, hdr);//on client
  if (compressed_data)
  {
    *blob_data = compressed_data;
    allocated_blob_data = true;
  }
  else
  {
    *blob_data = file_content;
    allocated_blob_data = false;
  }
}

bool is_blob_reference_data(const void *data, size_t len)
{
  if (len != blob_reference_size || memcmp(data, HASH_TYPE_REV_STRING, hash_type_magic_len) != 0)
    return false;
  if (!is_encoded_hash((const char*)data + hash_type_magic_len, hash_encoded_size))
    return false;
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
  return get_blob_reference_content_hash(session_ref_file_content, blob_reference_size, hash_encoded);
}
