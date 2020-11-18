#include "zlib.h"
#include "sha256/sha256.h"
#include "sha_blob_format.h"

#define SHA256_LIST(a)\
(a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5],(a)[6],(a)[7],(a)[8],(a)[9],\
(a)[10],(a)[11],(a)[12],(a)[13],(a)[14],(a)[15],(a)[16],(a)[17],(a)[18],(a)[19],\
(a)[20],(a)[21],(a)[22],(a)[23],(a)[24],(a)[25],(a)[26],(a)[27],(a)[28],(a)[29],\
(a)[30],(a)[31]

static void calc_sha256(const char *fn, const void *data, size_t len, bool src_blob, size_t &unpacked_len, unsigned char sha256[])//sha256 char[32]
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

static void get_blob_filename_from_encoded_sha256(const char *root_dir, const char* encoded_sha256, char *sha_file_name, size_t sha_max_len)
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
      error(1, 0, "too long filename <%s> for <%s>", sha_file_name, encoded_sha256);
}

static bool does_blob_exist(const char *sha_file_name)
{
  return get_file_size(sha_file_name) >= sizeof(BlobHeader);
}

static void create_blob_file_name(const char *root, unsigned char sha256[], const char *fn, char *sha_file_name, size_t sha_max_len)//sha256 char[32]
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
    error(1, 0, "too long filename <%s> for <%s>", sha_file_name, fn);
  }
}

static void create_dirs(const char *root, unsigned char sha256[])
{
  char buf[1024];
  if (snprintf(buf, sizeof(buf),"%s" BLOBS_SUBDIR "%02x", root, sha256[0]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (CVS_MKDIR(buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
  if (snprintf(buf, sizeof(buf),"%s" BLOBS_SUBDIR "%02x/%02x", root, sha256[0], sha256[1]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (CVS_MKDIR(buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
}

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
  zbuf = xmalloc(zlen);
  stream.next_out = (Bytef*)((char*)zbuf);
  if (deflate(&stream, Z_FINISH)==Z_STREAM_END && (zlen - stream.avail_out) < len)
  {
    zlen = (zlen - stream.avail_out);
    hdr = get_zlib_header(len, zlen);
  } else
  {
    xfree(zbuf);
    zbuf = nullptr;
    hdr = get_noarc_header(len);
  }
  deflateEnd(&stream);
  return zbuf;
}

static void atomic_write_sha_file(const char *fn, const char *sha_file_name, const void *data, size_t len, bool store_packed, bool src_packed)
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
    if (store_packed)
      compressed_data = compress_zlib_data(data, len, Z_BEST_COMPRESSION-4, hdr);

    const void* writeData = compressed_data ? compressed_data : data;

    if (fwrite(&hdr,1,sizeof(hdr),dest) != sizeof(hdr))
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)sizeof(hdr));
    if (fwrite(writeData,1,hdr.compressedLen,dest) != hdr.compressedLen)
      error(1, 0, "can't write temp_filename <%s> for <%s>(<%s>) of %d len", temp_filename, sha_file_name, fn, (uint32_t)hdr.compressedLen);

    if (compressed_data != nullptr)
      xfree(compressed_data);
  }
  rename_file (temp_filename, sha_file_name);
  fclose(dest);

  xfree (temp_filename);
}

//ideally we should receive already packed data, UNPACK it (for sha computations), and then store packed. That way compression moved to client
//after call to this function binary blob is stored
//returns zero if nothing written
static size_t write_binary_blob(const char *root, unsigned char sha256[],// 32 bytes
  const char *fn,//fn is for context only
  const void *data, size_t len, bool packed, bool src_packed)//fn is just context!
{
  if (src_packed &&
      (len < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->headerSize < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->compressedLen != len + ((const BlobHeader*)data)->headerSize
       || !is_accepted_magic(((const BlobHeader*)data)->magic))
     )
  {
    error(1, 0, "fn <%s> says it's client prepared blob but it's not!", fn, len);
  }
  const size_t clientUnpackedLen = src_packed ? ((const BlobHeader*)data)->uncompressedLen : len;
  size_t unpacked_len = 0;
  calc_sha256(fn, data, len, src_packed, unpacked_len, sha256);
  if (clientUnpackedLen != unpacked_len)
    error(1, 0, "fn <%s> says it has compressed %d of data but we decompressed only %d!", fn, (uint32_t)clientUnpackedLen, (uint32_t)unpacked_len);
  const size_t sha_file_name_len = 1024;
  char sha_file_name[sha_file_name_len];//can be dynamically allocated, as 32+3+1 + strlen(root)
  create_blob_file_name(root, sha256, fn, sha_file_name, sha_file_name_len);
  if (!isreadable(sha_file_name))
  {
    create_dirs(root, sha256);
    atomic_write_sha_file(fn, sha_file_name, data, len, packed, src_packed);
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

static BlobHeader get_binary_blob_hdr(const char *blob_file_name)
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

static size_t decode_binary_blob(const char *blob_file_name, void **data)
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

  *data = xmalloc(hdr.uncompressedLen);
  //((char*)*data)[hdr.uncompressedLen] = 0;
  if (is_packed_blob(hdr))//
  {
    void *tmpbuf = xmalloc(hdr.compressedLen);
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
    } else
    {
      error(1,errno,"Unknown compressor %s", blob_file_name);
      return 0;
    }
    xfree(tmpbuf);
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

static size_t read_binary_blob_directly(const char *blob_file_name, void **data)//allocates memory and read whole file
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
  *data = xmalloc(fileLen);
  if (fread(*data,1,fileLen,fp) != fileLen)
  {
    error(1,errno,"Couldn't read %s", blob_file_name);
    return 0;
  }
  fclose(fp);
  return fileLen;
}

static size_t read_binary_blob(const char *blob_file_name, void **data, bool return_blob_directly)
{
  if (return_blob_directly)//new protocol - just send data to client as is. It is already packed and everything
    return read_binary_blob_directly(blob_file_name, data);
  return decode_binary_blob(blob_file_name, data);
}
