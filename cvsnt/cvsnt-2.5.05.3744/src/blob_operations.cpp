#include "zlib.h"
#include "zstd.h"
#include "sha256/sha256.h"
//#include "cvs.h"
#include "sha_blob_format.h"
#include "sha_blob_reference.h"
#include "sha_blob_operations.h"

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
void rename_file (const char *from, const char *to);
size_t get_file_size(const char *file);
int isreadable (const char *file);
int blob_mkdir (const char *path, int mode);
//mem
void *blob_alloc(size_t sz);
void blob_free(void *);

#define TRY_ZLIB_AS_WELL_ON_BEST 0//if 1, on Pack::BEST we will try both algoritms to find out whats working best

void encode_sha256(unsigned char sha256[], char sha256_encoded[], size_t enc_len)//sha256 char[32], sha256_encoded[65]
{
  if (enc_len < sha256_encoded_size+1)
    error (1, 0, "too short %d", (int)enc_len);

  snprintf(sha256_encoded, enc_len,
     "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      SHA256_LIST(sha256));
}

void get_blob_filename_from_encoded_sha256(const char *root_dir, const char* encoded_sha256, char *sha_file_name, size_t sha_max_len)
{
  if (snprintf(sha_file_name, sha_max_len,
     "%s"
     BLOBS_SUBDIR
     "%c%c/%c%c/%.*s"
     , root_dir ,
     encoded_sha256[0], encoded_sha256[1],
     encoded_sha256[2], encoded_sha256[3],
     int(sha256_encoded_size)-4, encoded_sha256+4
     )
     >= sha_max_len-1)
      error(1, 0, "get_blob_name:too long filename <%s> for <%s>", sha_file_name, encoded_sha256);
}

void get_blob_filename_from_sha256(const char *root, unsigned char sha256[], const char *fn, char *sha_file_name, size_t sha_max_len)//sha256 char[32]
{
  if (snprintf(sha_file_name, sha_max_len,
     "%s"
     BLOBS_SUBDIR
     "%02x/%02x/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
     , root,
      SHA256_LIST(sha256)
     )
     >= sha_max_len-1)
  {
    error(1, 0, "create_blob_name: too long filename <%s> for <%s>", sha_file_name, fn);
  }
}

void calc_sha256(const char *fn, const void *data, size_t len, bool src_blob, size_t &unpacked_len, unsigned char sha256[])//sha256 char[32]
{
  unpacked_len = len;
  blk_SHA256_CTX ctx;
  blk_SHA256_Init(&ctx);

  if (src_blob)
  {
    //new protocol - client sends data as blob, already packed and everything.
    //we still need to verify it!
    //calc sha256 on the unpacked data!
    const BlobHeader &hdr = *(const BlobHeader*)data;
    unpacked_len = hdr.uncompressedLen;
    const uint64_t len64 = hdr.uncompressedLen;
    blk_SHA256_Update(&ctx, &len64, sizeof(len64));//that would make attack significantly harder. But we dont expect attacks on our repo.
    if (!is_packed_blob(hdr))
    {
      blk_SHA256_Update(&ctx, ((const char*)data + hdr.headerSize), unpacked_len);
    } else if (is_zlib_blob(hdr))
    {
      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = hdr.compressedLen;
      stream.next_in = (Bytef*)((const char*)data + hdr.headerSize);
      char bufOut[32768];
      size_t totalSrcLenLeft = hdr.compressedLen;
      while (stream.avail_in>0)
      {
        stream.avail_out = sizeof(bufOut);
        stream.next_out = (Bytef*)bufOut;
        int result = inflate(&stream, Z_SYNC_FLUSH);
        if (result < 0)
          error(1,0,"internal error: inflate failed");

        blk_SHA256_Update(&ctx, bufOut, sizeof(bufOut) - stream.avail_out);

        if (result == Z_STREAM_END)
          break;
      }
      inflateEnd(&stream);
    } else if (is_zstd_blob(hdr))
    {
      ZSTD_DStream* zds = ZSTD_createDStream();
      ZSTD_initDStream(zds);
      ZSTD_inBuffer_s streamIn;
      streamIn.src = (const char*)((const char*)data + hdr.headerSize);
      streamIn.size = hdr.compressedLen;
      streamIn.pos = 0;
      char bufOut[ZSTD_BLOCKSIZE_MAX];
      ZSTD_outBuffer_s streamOut;
      streamOut.dst = bufOut;
      streamOut.size = sizeof(bufOut);
      streamOut.pos = 0;
      while (streamIn.pos < streamIn.size && streamOut.pos < streamOut.size)
      {
        if (ZSTD_isError(ZSTD_decompressStream(zds, &streamOut, &streamIn)))
          error(1,0,"internal error: inflate failed");

        blk_SHA256_Update(&ctx, bufOut, streamOut.pos);
      };
      ZSTD_freeDStream(zds);
    } else
    {
      error(1,errno,"Unknown compressor %s", fn);
    }
  } else
  {
    uint64_t len64 = len;blk_SHA256_Update(&ctx, &len64, sizeof(len64));//that would make attack significantly harder. But we dont expect attacks on our repo.
    blk_SHA256_Update(&ctx, data, len);
  }
  blk_SHA256_Final(sha256, &ctx);
}

bool calc_sha256_file(const char *fn, unsigned char sha256[])//sha256 char[32]
{
  blk_SHA256_CTX ctx;
  blk_SHA256_Init(&ctx);

  FILE* fp = fopen(fn,"rb");
  if (!fp)
    return false;
  size_t len = get_file_size(fn);
  uint64_t len64 = len;blk_SHA256_Update(&ctx, &len64, sizeof(len64));//that would make attack significantly harder. But we dont expect attacks on our repo.
  char buf[32768];
  while (len > 0)
  {
    size_t sz = len < sizeof(buf) ? len : sizeof(buf);
    if (fread(buf,1,sz,fp) != sz)
    {
      fclose(fp);
      return false;
    }
    blk_SHA256_Update(&ctx, buf, sz);
    len -= sz;
  }
  fclose(fp);

  blk_SHA256_Final(sha256, &ctx);
  return true;
}

bool does_blob_exist(const char *sha_file_name)
{
  return get_file_size(sha_file_name) >= sizeof(BlobHeader);
}

void create_dirs(const char *root, unsigned char sha256[])
{
  char buf[1024];
  if (snprintf(buf, sizeof(buf),"%s" BLOBS_SUBDIR "%02x", root, sha256[0]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (blob_mkdir(buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
  if (snprintf(buf, sizeof(buf),"%s" BLOBS_SUBDIR "%02x/%02x", root, sha256[0], sha256[1]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (blob_mkdir(buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
}


static inline BlobHeader get_header(const uint8_t *m, size_t len, size_t zlen, uint16_t flags)
{
  BlobHeader hdr;
  memcpy(hdr.magic, m, BLOB_MAGIC_SIZE);
  hdr.headerSize = sizeof(BlobHeader);
  hdr.flags = flags;
  hdr.uncompressedLen = len;
  hdr.compressedLen = zlen;
  return hdr;
}
static inline BlobHeader get_noarc_header(size_t len, uint16_t flags = 0) {return get_header(noarc_magic, len, len, flags);}
static inline BlobHeader get_zlib_header(size_t len, size_t zlen, uint16_t flags = 0) {return get_header(zlib_magic, len, zlen, flags);}
static inline BlobHeader get_zstd_header(size_t len, size_t zlen, uint16_t flags = 0) {return get_header(zstd_magic, len, zlen, flags);}

static void* compress_zlib_data(const void *data, size_t len, int compression_level, BlobHeader &hdr)
{
  size_t zlen = 0;
  void *zbuf = nullptr;

  z_stream stream = {0};
  deflateInit(&stream,compression_level);
  zlen = ((deflateBound(&stream, len) - len/10) + 0xFFF) & ~0xFFF;
  stream.avail_in = len;
  stream.next_in = (Bytef*)data;
  stream.avail_out = zlen;
  zbuf = blob_alloc(zlen);
  stream.next_out = (Bytef*)((char*)zbuf);
  if (deflate(&stream, Z_FINISH)==Z_STREAM_END && (zlen - stream.avail_out) < len)
  {
    zlen = (zlen - stream.avail_out);
    hdr = get_zlib_header(len, zlen);
  } else
  {
    blob_free(zbuf);
    zbuf = nullptr;
    hdr = get_noarc_header(len);
  }
  deflateEnd(&stream);
  return zbuf;
}

static const int zstd_fast_comp = 19;//we dont use ultra so everything is fast enough. some server lazy utility can constantly re-pack blobs with smaller compression levels
static void* compress_zstd_data(const void *data, size_t len, BlobPackType pack, BlobHeader &hdr)
{
  const int compression_level = pack == BlobPackType::FAST ? zstd_fast_comp : ZSTD_maxCLevel();
  size_t zlen = ZSTD_compressBound(len);
  void *zbuf = blob_alloc(zlen);
  const uint16_t flags = compression_level == ZSTD_maxCLevel() ? BlobHeader::BEST_POSSIBLE_COMPRESSION : 0;
  size_t compressedSz = ZSTD_compress( zbuf, zlen, data, len, compression_level);
  if (ZSTD_isError(compressedSz) || compressedSz > len - len/20)//should be at least 95% compression, otherwise why bother compressing at all
  {
    blob_free(zbuf);
    zbuf = nullptr;
    hdr = get_noarc_header(len, flags);
  } else
    hdr = get_zstd_header(len, compressedSz, flags);
  return zbuf;
}

void atomic_write_sha_file(const char *fn, const char *sha_file_name, const void *data, size_t len, BlobPackType pack, bool src_packed)
{
  char *temp_filename = NULL;
  FILE *dest = cvs_temp_file(&temp_filename, "wb");
  if (!dest)
  {
    error(1, errno, "can't open write temp_filename <%s> for <%s>(<%s>)", temp_filename, sha_file_name, fn);
  }
  if (src_packed)
  {
    //new protocol - client sends already prepared blobs, just write them down
    if (fwrite(data,1,len,dest) != len)
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)len);
  } else
  {
    BlobHeader hdr = get_noarc_header(len);
    void *compressed_data = nullptr;
    if (pack != BlobPackType::NO)
    {
      compressed_data = compress_zstd_data(data, len, pack, hdr);//on server we use non-ultra method!
      #if TRY_ZLIB_AS_WELL_ON_BEST
      if (pack == BlobPackType::BEST)
      {
        BlobHeader hdr_zlib;
        void *compressed_data_zlib = compress_zlib_data(data, len, Z_BEST_COMPRESSION, hdr_zlib);
        if (hdr_zlib.compressedLen < hdr.compressedLen)
        {
          if (compressed_data)
            blob_free(compressed_data);
          compressed_data = compressed_data_zlib;
          hdr = hdr_zlib;
        } else
        {
          if (compressed_data_zlib)
            blob_free(compressed_data_zlib);
        }
      }
      #endif
    }

    const void* writeData = compressed_data ? compressed_data : data;

    if (fwrite(&hdr,1,sizeof(hdr),dest) != sizeof(hdr))
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)sizeof(hdr));
    if (fwrite(writeData,1,hdr.compressedLen,dest) != hdr.compressedLen)
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)hdr.compressedLen);

    if (compressed_data != nullptr)
      blob_free(compressed_data);
  }
  rename_file (temp_filename, sha_file_name);
  fclose(dest);

  blob_free (temp_filename);
}

//ideally we should receive already packed data, UNPACK it (for sha computations), and then store packed. That way compression moved to client
//after call to this function binary blob is stored
//returns zero if nothing written
bool write_prepacked_binary_blob(const char *root, const char *client_sha256,
  const void *data, size_t len)
{
  if (
      (len < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->headerSize < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->compressedLen + ((const BlobHeader*)data)->headerSize != len
       || !is_accepted_magic(((const BlobHeader*)data)->magic))
     )
  {
    error(0, 0, "Client <%s> says it's client prepared blob of len <%d> but it's not!", client_sha256, (int)len);
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
  get_blob_filename_from_encoded_sha256(root, client_sha256, sha_file_name, sha_file_name_len);
  if (isreadable(sha_file_name))
  {
    //no need to check if client is lying about this sha content
    printf("sha <%s> already exist, deduplication\n", client_sha256);
    return true;
  }

  const size_t clientUnpackedLen = ((const BlobHeader*)data)->uncompressedLen;
  size_t unpacked_len = 0;
  unsigned char sha256[32];
  calc_sha256(client_sha256, data, len, true, unpacked_len, sha256);
  if (clientUnpackedLen != unpacked_len)
  {
    error(0, 0, "Client <%s> says it has compressed %d of data but we decompressed only %d!",client_sha256 , (uint32_t)clientUnpackedLen, (uint32_t)unpacked_len);
    return false;
  }
  char real_sha256_encoded[sha256_encoded_size+1];
  encode_sha256(sha256, real_sha256_encoded, sizeof(real_sha256_encoded));
  if (memcmp(client_sha256, real_sha256_encoded, sha256_encoded_size) != 0)
  {
    error(0, 0, "client-promised sha <%s> and real sha<%s> different!", client_sha256, real_sha256_encoded);
    return false;
  }
  get_blob_filename_from_sha256(root, sha256, client_sha256, sha_file_name, sha_file_name_len);
  if (!isreadable(sha_file_name))
  {
    create_dirs(root, sha256);
    atomic_write_sha_file(client_sha256, sha_file_name, data, len, BlobPackType::NO, true);
    return unpacked_len;
  }//else we already have this blob written. deduplication worked!
  return true;
}

size_t write_binary_blob(const char *root, unsigned char sha256[],// 32 bytes
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
  calc_sha256(fn, data, len, src_packed, unpacked_len, sha256);
  if (clientUnpackedLen != unpacked_len)
    error(1, 0, "fn <%s> says it has compressed %d of data but we decompressed only %d!", fn, (uint32_t)clientUnpackedLen, (uint32_t)unpacked_len);
  const size_t sha_file_name_len = 1024;
  char sha_file_name[sha_file_name_len];//can be dynamically allocated, as 32+3+1 + strlen(root)
  get_blob_filename_from_sha256(root, sha256, fn, sha_file_name, sha_file_name_len);
  if (!isreadable(sha_file_name))
  {
    create_dirs(root, sha256);
    atomic_write_sha_file(fn, sha_file_name, data, len, pack, src_packed);
    return unpacked_len;
  }//else we already have this blob written. deduplication worked!
  else
  {
    //if we are paranoid, that's the place where we can check for collision
    //probability of sha256 collision is way lower then asteroid destroying Earth in next 1000 years
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
    void *tmpbuf = blob_alloc(hdr.compressedLen);
    if (fread(tmpbuf,1,hdr.compressedLen,fp) != hdr.compressedLen)
    {
      error(1,errno,"Couldn't read %s", blob_file_name);
      return 0;
    }
    if (is_zlib_blob(hdr))
    {
      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = hdr.compressedLen;
      stream.next_in = (Bytef*)tmpbuf;
      stream.avail_out = hdr.uncompressedLen;
      stream.next_out = (Bytef*)*data;
      if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
          error(1,0,"internal error: inflate failed");
      inflateEnd(&stream);
    } else if (is_zstd_blob(hdr))
    {
      if (ZSTD_isError(ZSTD_decompress( *data, hdr.uncompressedLen,
                              tmpbuf, hdr.compressedLen)))
          error(1,0,"internal error: zstd decompress failed");
    } else
    {
      error(1,errno,"Unknown compressor %s", blob_file_name);
      return 0;
    }
    blob_free(tmpbuf);
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
    if (is_zlib_blob(hdr))
    {
      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = hdr.compressedLen;
      stream.next_in = (Bytef*)readData;
      stream.avail_out = hdr.uncompressedLen;
      stream.next_out = (Bytef*)*out_data;
      if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
          error(1,0,"internal error: inflate failed");
      inflateEnd(&stream);
    } else if (is_zstd_blob(hdr))
    {
      if (ZSTD_isError(ZSTD_decompress( *out_data, hdr.uncompressedLen,
                              readData, hdr.compressedLen)))
          error(1,0,"internal error: zstd decompress failed");
    } else
    {
      error(1,errno,"Unknown compressor %s", context);
      return 0;
    }
  } else
  {
    *out_data = (void*)readData;
    need_free = false;
  }
  return hdr.uncompressedLen;
}

size_t read_binary_blob(const char *blob_file_name, void **data, bool return_blob_directly)
{
  if (return_blob_directly)//new protocol - just send data to client as is. It is already packed and everything
    return read_binary_blob_directly(blob_file_name, data);
  return decode_binary_blob(blob_file_name, data);
}

bool can_be_blob_reference(const char *blob_ref_file_name)
{
  return get_file_size(blob_ref_file_name) == blob_reference_size;//blob references is always sha256:encoded_sha_64_bytes
}

bool get_blob_reference_content_sha256(const unsigned char *ref_file_content, size_t len, char *sha256_encoded)//sha256_encoded==char[65], encoded 32 bytes + \0
{
  if (len != blob_reference_size || memcmp(&ref_file_content[0], SHA256_REV_STRING, sha256_magic_len) != 0)
    //not a blob reference!
    return false;
  //todo: may be check if it is sha256 encoded
  memcpy(sha256_encoded, ref_file_content + sha256_magic_len, sha256_encoded_size);
  sha256_encoded[sha256_encoded_size] = 0;
  return true;
}

bool get_blob_reference_sha256(const char *blob_ref_file_name, char *sha256_encoded)//sha256_encoded==char[65], encoded 32 bytes + \0
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

bool is_blob_reference(const char *root_dir, const char *blob_ref_file_name, char *sha_file_name, size_t sha_max_len)//sha256_encode==char[65], encoded 32 bytes + \0
{
  char sha256_encoded[sha256_encoded_size+1];
  if (!get_blob_reference_sha256(blob_ref_file_name, sha256_encoded))//sha256_encoded==char[65], encoded 32 bytes + \0
    return false;
  get_blob_filename_from_encoded_sha256(root_dir, sha256_encoded, sha_file_name, sha_max_len);
  return does_blob_exist(sha_file_name);
}

void write_direct_blob_reference(const char *fn, const void *ref, size_t ref_len)//ref_len == blob_reference_size
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
  blob_free (temp_filename);
}

void write_blob_reference(const char *fn, unsigned char sha256[])//sha256[32] == digest
{
 char sha256_encoded[blob_reference_size+1];
 snprintf(sha256_encoded, sizeof(sha256_encoded), SHA256_REV_STRING
	  "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	  SHA256_LIST(sha256));
  write_direct_blob_reference(fn,  sha256_encoded, blob_reference_size);
}

void write_blob_and_blob_reference(const char *root, const char *fn, const void *data, size_t len, BlobPackType store_packed, bool src_packed)
{
  unsigned char sha256[32];
  write_binary_blob(root, sha256, fn, data, len, store_packed, src_packed);
  write_blob_reference(fn, sha256);
}


void create_binary_blob_to_send(const char *ctx, void *file_content, size_t len, bool guess_packed, BlobHeader **hdr_, void** blob_data, bool &allocated_blob_data, char*sha256_encoded, size_t sha_256enc_len)
{
  unsigned char sha256[32];
  size_t unpacked_len;
  calc_sha256(ctx, file_content, len, false, unpacked_len, sha256);
  encode_sha256(sha256, sha256_encoded, sha_256enc_len);//sha256 char[32], sha256_encoded[65]
  *hdr_ = (BlobHeader*)blob_alloc(sizeof(BlobHeader));
  BlobHeader &hdr = **hdr_;
  hdr = get_noarc_header(len);
  void *compressed_data = nullptr;
  if (guess_packed)
    compressed_data = compress_zstd_data(file_content, len, BlobPackType::FAST, hdr);//on client
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
