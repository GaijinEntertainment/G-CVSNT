#include <zlib.h>
#ifdef _WIN32
//typedef __int64 int64_t;
#endif
#include "sha256/sha256.h"

#define SHA256_LIST(a)\
(a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5],(a)[6],(a)[7],(a)[8],(a)[9],\
(a)[10],(a)[11],(a)[12],(a)[13],(a)[14],(a)[15],(a)[16],(a)[17],(a)[18],(a)[19],\
(a)[20],(a)[21],(a)[22],(a)[23],(a)[24],(a)[25],(a)[26],(a)[27],(a)[28],(a)[29],\
(a)[30],(a)[31]

#define SHA256_REV_STRING "sha256:"
#define BLOBS_SUBDIR "/blobs/"
static const char sha256_rev_string[] = SHA256_REV_STRING;
static const size_t sha256_encoded_size = 64;
static const size_t blob_reference_size = sizeof(sha256_rev_string)-1+sha256_encoded_size;

enum {BLOB_MAGIC_SIZE = 8};
struct BlobHeader
{
  unsigned char magic[BLOB_MAGIC_SIZE];
  uint64_t uncompressedLen;
  uint64_t compressedLen;
};
static const char zlib_magic[BLOB_MAGIC_SIZE+1] = "ZLIBCOMP";
static const char noarc_magic[BLOB_MAGIC_SIZE+1] = "NONCOMPR";
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
  hdr.uncompressedLen = len;
  hdr.compressedLen = len;
  return hdr;
}


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
    if (is_zlib_blob(hdr))
    {
      unpacked_len = hdr.uncompressedLen;
      uint64_t len64 = hdr.uncompressedLen;
      blk_SHA256_Update(&ctx, &len64, sizeof(len64));//that would make attack significantly harder. But we dont expect attacks on our repo.
      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = hdr.compressedLen;
      stream.next_in = (Bytef*)((const char*)data + sizeof(BlobHeader));
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
      error(1,errno,"Unknown compressor %s", fn);
  } else
  {
    uint64_t len64 = len;blk_SHA256_Update(&ctx, &len64, sizeof(len64));//that would make attack significantly harder. But we dont expect attacks on our repo.
    blk_SHA256_Update(&ctx, data, len);
  }
  blk_SHA256_Final(sha256, &ctx);
}

static void get_blob_filename_from_encoded_sha256(const char* encoded_sha256, char *sha_file_name, size_t sha_max_len)
{
  if (snprintf(sha_file_name, sha_max_len,
     "%s"
     BLOBS_SUBDIR
     "%c/%c/*.%s"
     , current_parsed_root->directory,
     encoded_sha256[0],
     encoded_sha256[1],
     int(sha256_encoded_size)-2, encoded_sha256+2
     )
     >= sha_max_len-1)
      error(1, 0, "too long filename <%s> for <%s>", sha_file_name, encoded_sha256);
}

static bool does_blob_exist(const char *sha_file_name)
{
  return get_file_size(sha_file_name) >= sizeof(BlobHeader);
}

static void create_blob_file_name(unsigned char sha256[], const char *fn, char *sha_file_name, size_t sha_max_len)//sha256 char[32]
{
  if (snprintf(sha_file_name, sha_max_len,
     "%s"
     BLOBS_SUBDIR
     "%02x/%02x/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
     , current_parsed_root->directory,
      SHA256_LIST(sha256)
     )
     >= sha_max_len-1)
  {
    error(1, 0, "too long filename <%s> for <%s>", sha_file_name, fn);
  }
}

static void create_dirs(unsigned char sha256[])
{
  char buf[1024];
  if (snprintf(buf, sizeof(buf),"%s/%02x", current_parsed_root->directory, sha256[0]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (CVS_MKDIR (buf, 0777) != 0 && errno != EEXIST)
    error (1, errno, "cannot make directory %s", buf);
  if (snprintf(buf, sizeof(buf),"%s/%02x/%02x", current_parsed_root->directory, sha256[0], sha256[1]) >= sizeof(buf)-1)
  {
    error(1, 0, "too long dirname <%s>", buf);
  }
  if (CVS_MKDIR (buf, 0777) != 0 && errno != EEXIST)
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
    memcpy(hdr.magic, zlib_magic, BLOB_MAGIC_SIZE);
    hdr.uncompressedLen = len;
    hdr.compressedLen = zlen;
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
static size_t write_binary_blob(unsigned char sha256[],// 32 bytes
  const char *fn, const void *data, size_t len, bool packed, bool src_packed)//fn is just context!
{
  if (src_packed &&
      (len < sizeof(BlobHeader)
       || ((const BlobHeader*)data)->compressedLen != len + sizeof(BlobHeader)
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
  char sha_file_name[sha_file_name_len];//can be dynamically allocated, as 32+3+1 + strlen(current_parsed_root->directory)
  create_blob_file_name(sha256, fn, sha_file_name, sha_file_name_len);
  if (!isreadable(sha_file_name))
  {
    create_dirs(sha256);
    atomic_write_sha_file(fn, sha_file_name, data, len, packed, src_packed);
  }//else we already have this blob written. deduplication worked!
  else
  {
    //if we are paranoid, that's the place where we can check for collision
    //probability of sha256 collision is way lower then asteroid destroying Earth in next 1000 years
    // in addition, it won't crash our repo, merely will result in incorrect commit (you will update something else, not what you have commited, some other file)
    //so we won't check for it.
  }
  return unpacked_len;
}

static size_t decode_binary_blob(const char *blob_file_name, void **data)
{
  size_t fileLen = get_file_size(blob_file_name);
  if (fileLen < sizeof(BlobHeader))
  {
    error(1,errno,"Couldn't read %s of %d len", blob_file_name, (int)fileLen);
    return 0;
  }
  FILE* fp = CVS_FOPEN(blob_file_name,"rb");
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

  *data = xmalloc(hdr.uncompressedLen);
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
  FILE* fp = CVS_FOPEN(blob_file_name, "rb");
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

static void RCS_write_binary_rev_data_blob(const char *fn, const void *data, size_t len, bool store_packed, bool src_packed)
{
  unsigned char sha256[32];
  size_t srcSize = write_binary_blob(sha256, fn, data, len, store_packed, src_packed);

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

static bool can_be_blob_reference(const char *blob_file_name)
{
  return get_file_size(blob_file_name) == blob_reference_size;//blob references is always sha256:encoded_sha_64_bytes
}

static bool is_blob_reference(const char *blob_file_name, char *sha_file_name, size_t sha_max_len)//sha256_encode==char[65], encoded 32 bytes + \0
{
  if (!can_be_blob_reference(blob_file_name))
    return false;
  FILE* fp = CVS_FOPEN(blob_file_name,"rb");
  if (!fp)
    return false;
  char sha256_magic[sizeof(sha256_rev_string)-1];
  if (fread(sha256_magic,1,sizeof(sha256_magic),fp) != sizeof(sha256_magic))
  {
    error(1,errno,"Couldn't read %s", blob_file_name);
    return false;
  }
  if (memcmp(sha256_magic, sha256_rev_string, sizeof(sha256_magic) != 0))
  {
    //not a blob reference!
    fclose(fp);
    return false;
  }
  char sha256_encoded[sha256_encoded_size+1];
  if (fread(sha256_encoded, 1, sha256_encoded_size, fp) != sha256_encoded_size)
  {
    error(1,errno,"Couldn't read %s", blob_file_name);
    return false;
  }
  sha256_encoded[sha256_encoded_size] = 0;
  get_blob_filename_from_encoded_sha256(sha256_encoded, sha_file_name, sha_max_len);
  return does_blob_exist(sha_file_name);
}

static bool RCS_read_binary_rev_data_direct(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool packed, int64_t *cmp_other_sz);

static bool RCS_read_binary_rev_data_blob(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool return_packed, bool supposed_packed, int64_t *cmp_other_sz)
{
  if (cmp_other_sz)
  {
    if (*inout_data_allocated && *out_data)
      xfree (*out_data);
    *out_data = NULL;
    *out_len = 0;
    *inout_data_allocated = 0;
    return false;
  }
  char sha_file_name[1024];
  if (is_blob_reference(fn, sha_file_name, sizeof(sha_file_name)))
  {
    if (*inout_data_allocated && *out_data)
      xfree (*out_data);
    *out_data = NULL;
    *out_len = 0;
    *inout_data_allocated = 0;
    *out_len = read_binary_blob(sha_file_name, (void**)out_data, false);//
    if (*out_data)
      *inout_data_allocated = 1;
	return true;
  }
  else
  {
    return RCS_read_binary_rev_data_direct(fn, out_data, out_len, inout_data_allocated, supposed_packed, cmp_other_sz);
    //read like it used to be
  }
}

static void RCS_write_binary_rev_data_direct(const char *fn, void *data, size_t len, bool packed)
{
  char sbuf[512];

  char *fnpk = (char*)xmalloc(strlen(fn)+4);
  strcpy(fnpk, fn);
  strcat(fnpk, "#z");

  const char *path = packed ? fnpk : fn;
  FILE *fp = CVS_FOPEN(path,"wb");

  if (packed && fp)
  {
    uLong zlen;
    void *zbuf;

    z_stream stream = {0};
    deflateInit(&stream,Z_BEST_COMPRESSION-4);
    zlen = ((deflateBound(&stream, len) - len/10) + 0xFFF) & ~0xFFF;
    stream.avail_in = len;
    stream.next_in = (Bytef*)data;
    stream.avail_out = zlen;
    zbuf = xmalloc(zlen+4);
    stream.next_out = (Bytef*)((char*)zbuf)+4;
    *(unsigned*)zbuf=htonl(len);
    if(deflate(&stream, Z_FINISH)==Z_STREAM_END)
    {
      zlen = 4 + (zlen - stream.avail_out);
      snprintf(sbuf, sizeof(sbuf), "CVS: commit compressed %s (%+d Kb)\n", path, ((int)zlen-(int)len)/1024); sbuf[sizeof(sbuf)-1] = 0;
      cvs_output (sbuf, 0);

      if(fwrite(zbuf,1,zlen,fp)!=zlen)
      {
        fclose(fp); fp = NULL;
        unlink(path);
        error(1,errno,"Couldn't write to %s", path);
      }
    }
    else
    {
      snprintf(sbuf, sizeof(sbuf), "CVS: commit failed to compress %s, store uncompressed\n", path); sbuf[sizeof(sbuf)-1] = 0;
      cvs_output (sbuf, 0);

      fclose(fp);
      unlink(path);
      packed = false;
      path = fn;
      fp = CVS_FOPEN(path,"wb");
    }
    deflateEnd(&stream);
    xfree(zbuf);
  }

  if (!packed && fp)
    if(fwrite(data,1,len,fp)!=len)
    {
      fclose(fp); fp = NULL;
      unlink(path);
      error(1,errno,"Couldn't write to %s", path);
    }

  if (fp)
    fclose(fp);
  else
    error(1,errno,"Couldn't create %s", path);

  xfree(fnpk);
}

static bool RCS_read_binary_rev_data_direct(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool packed, int64_t *cmp_other_sz)
{
  char *fnpk = NULL;
  struct stat s;
  int data_sz = 0;
  FILE *fp = NULL;

  if (!packed)
  {
    if (CVS_STAT (fn, &s) >= 0)
      data_sz = s.st_size;
    else
    {
      fnpk = (char*)xmalloc(strlen(fn)+4); strcpy(fnpk, fn); strcat(fnpk, "#z");
      if (CVS_STAT (fnpk, &s) >= 0 && s.st_size >= 4)
        fn = fnpk, packed = true;
      else
      {
        xfree(fnpk); fnpk = NULL;
        s.st_size = 0;
        cvs_output ("warning: missing ", 0);
        cvs_output (fn, 0);
        cvs_output ("\n", 1);
      }
    }
  }
  else
  {
    fnpk = (char*)xmalloc(strlen(fn)+4); strcpy(fnpk, fn); strcat(fnpk, "#z");
    if (CVS_STAT (fnpk, &s) >= 0)
      fn = fnpk;
    else
    {
      xfree(fnpk); fnpk = NULL;
      if (CVS_STAT (fn, &s) >= 0)
        data_sz = s.st_size, packed = false;
      else
      {
        s.st_size = 0;
        cvs_output ("warning: missing ", 0);
        cvs_output (fn, 0);
        cvs_output ("\n", 1);
      }
    }
  }

  if (packed && s.st_size)
  {
    fp = CVS_FOPEN(fn,"rb");
    data_sz = (fread(&data_sz,1,4,fp) == 4) ? ntohl(data_sz) : -1;
  }

  if (cmp_other_sz && data_sz != *cmp_other_sz)
  {
    *cmp_other_sz = -1;
    if (fp)
      fclose(fp);
    if (fnpk)
      xfree(fnpk);

    if (*inout_data_allocated && *out_data)
      xfree (*out_data);
    *out_data = NULL;
    *out_len = 0;
    *inout_data_allocated = 0;
    return false;
  }

  if (*inout_data_allocated && *out_data)
    xfree (*out_data);
  *out_data = NULL;
  *out_len = 0;
  *inout_data_allocated = 0;

  if (!fp)
    fp = CVS_FOPEN(fn,"rb");
  if (fnpk)
    xfree(fnpk);

  if (data_sz)
  {
    *out_data = (char*)xmalloc(data_sz+1);
    *out_len = data_sz;
    *inout_data_allocated = 1;
    out_data[0][data_sz] = 0;

    if (!packed)
    {
      if (fread(*out_data, 1, data_sz, fp) != data_sz)
        error(1,errno,"Couldn't read from to %s", fn);
    }
    else
    {
      int zlen = s.st_size-4;
      void *tmpbuf = xmalloc(zlen);

      if (fread(tmpbuf, 1, zlen, fp) != zlen)
        error(1,errno,"Couldn't read from %s#z", fn);

            z_stream stream = {0};
            inflateInit(&stream);
            stream.avail_in = zlen;
            stream.next_in = (Bytef*)tmpbuf;
            stream.avail_out = data_sz;
            stream.next_out = (Bytef*)*out_data;
            if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
                error(1,0,"internal error: inflate failed");
            xfree(tmpbuf);
    }
  }
  if (fp)
    fclose(fp);
  return true;
}

#define CVS_DEDUPLICATION 0

static void RCS_write_binary_rev_data(const char *fn, void *data, size_t len, bool packed)
{
  #if CVS_DEDUPLICATION
  RCS_write_binary_rev_data_blob(fn, data, len, packed, false);//we should rely on packed sent by client. it know not only is src_packed but also if it was reasonable to pack
  #else
  RCS_write_binary_rev_data_direct(fn, data, len, packed);
  #endif
  //todo: establish protocol
}

static bool RCS_read_binary_rev_data(const char *fn, char **out_data, size_t *out_len, int *inout_data_allocated, bool packed, int64_t *cmp_other_sz)
{
  #if CVS_DEDUPLICATION
  return RCS_read_binary_rev_data_blob(fn, out_data, out_len, inout_data_allocated, false, packed, cmp_other_sz);
#else
  return RCS_read_binary_rev_data_direct(fn, out_data, out_len, inout_data_allocated, packed, cmp_other_sz);
  #endif
}

struct RevStringList
{
  RCSVers **data;
  int count, total;

public:
  RevStringList(): data(NULL), count(0), total(0) {}
  ~RevStringList() { clear(); }

  void clear() { xfree(data); data = NULL, count = total = 0; }
  void add(RCSVers *s, int q=64)
  {
    if (count+1 > total)
      data = (RCSVers**)xrealloc(data, (total+=q)*sizeof(RCSVers*));
    data[count] = s;
    count++;
  }
  bool replaceWithNull(RCSVers *p)
  {
    for (int i = 0; i < count; i ++)
      if (data[i] == p)
      {
        data[i] = NULL;
        return true;
      }
    return false;
  }

  void sort() { qsort(data, count, sizeof(RCSVers*), cmp_rev_up); }
  static int cmp_rev_up(const void *a, const void *b)
  {
    RCSVers *r1 = *(RCSVers**)a, *r2 = *(RCSVers**)b;
    int nd1 = numdots(r1->version), nd2 = numdots(r2->version);
    if (nd1 != nd2)
      return nd1-nd2;
    char *p1 = strchr(r1->version, '.'), *p2 = strchr(r2->version, '.');
    while (p1 && p2)
    {
      int diff = atoi(p1+1)-atoi(p2+1);
      if (diff)
        return nd1 < 2 ? diff : -diff;
      p1 = strchr(p1+1, '.');
      p2 = strchr(p2+1, '.');
    }
    return strcmp(r1->version, r2->version);
  }
};

static void RCS_get_rev_list(RevStringList &lst, RCSNode *rcs, char *rev)
{
  RCSVers *versp;
  Node *p, *q, *branch;
  List *revs = getlist();

  do
  {
    /* Find the delta node for this revision. */
    p = findnode (rcs->versions, rev);
    if (p == NULL)
      error (1, 0, "error parsing repository file %s, file may be corrupt.", rcs->path);

    versp = (RCSVers *) p->data;
    lst.add(versp);

    if(versp->branches)
    {
      p = getnode();
      p->key = xstrdup(rev);
      p->data = (char*)versp;
      p->delproc = no_del;
      addnode_at_front(revs,p);
    }

    rev = versp->next;
  } while(rev);

  for(p=revs->list->next; p!=revs->list;p=p->next)
  {
    versp = (RCSVers *) p->data;

    /* Recurse into this routine to do the branches.  This will eventually fail,
       but a lot later than the original implementation that blew after 988 versions */
    branch = versp->branches->list;
    for (q = branch->next; q != branch; q = q->next)
      RCS_get_rev_list (lst, rcs, q->key);
  }
  dellist(&revs);
}

static void RCS_extract_rev_data(RCSNode *rcs, RCSVers *rev, RevStringList &lst, bool packed, int64_t &new_sz)
{
  char *branchversion;
  char *cpversion;
  char *key;
  char *value;
  size_t vallen;
  RCSVers *vers;
  RCSVers *prev_vers;
  RCSVers *trunk_vers;
  char **next;
  int ishead, isnext, isversion, onbranch;
  Node *node;
  linevector headlines;
  linevector curlines;
  linevector trunklines;
  linevector binbuf;
  int foundhead;
  const char **deltatype;
  char *zbuf=NULL;
  size_t zbuflen=0;
  static const char *dt_text="text";

  //printf("--- RCS_extract_rev_data(%s)\n",rev->version);

  struct rcsbuffer *rcsbuf = &rcs->rcsbuf;

  rcsbuf_setpos_to_delta_base(rcs);

  ishead = 1;
  vers = NULL;
  prev_vers = NULL;
  trunk_vers = NULL;
  next = NULL;
  onbranch = 0;
  foundhead = 0;

  linevector_init (&curlines);
  linevector_init (&headlines);
  linevector_init (&trunklines);
  linevector_init (&binbuf);
  binbuf.is_binary=1;

  /* We set BRANCHVERSION to the version we are currently looking
     for.  Initially, this is the version on the trunk from which
     VERSION branches off.  If VERSION is not a branch, then
     BRANCHVERSION is just VERSION.  */
  branchversion = xstrdup (rev->version);
  cpversion = strchr (branchversion, '.');
  if (cpversion != NULL)
    cpversion = strchr (cpversion + 1, '.');
  if (cpversion != NULL)
    *cpversion = '\0';

  do {
    if (! rcsbuf_getrevnum (rcsbuf, &key))
      error (1, 0, "unexpected EOF reading RCS file %s", fn_root(rcs->path));

    if (next != NULL && ! STREQ (*next, key))
    {
      /* This is not the next version we need.  It is a branch
          version which we want to ignore.  */
      isnext = 0;
      isversion = 0;
    }
    else
    {
      isnext = 1;

      /* look up the revision */
      node = findnode (rcs->versions, key);
      if (node == NULL)
        error (1, 0,"mismatch in rcs file %s between deltas and deltatexts", fn_root(rcs->path));

      /* Stash the previous version.  */
      prev_vers = vers;

      vers = (RCSVers *) node->data;
      next = vers->next?&vers->next:NULL;
      deltatype = vers->type?(const char **)&vers->type:&dt_text;

      /* Hack */
      if(STREQ(*deltatype,"(null)"))
        deltatype=&dt_text;

      /* Compare key and trunkversion now, because key points to
      storage controlled by rcsbuf_getkey.  */
      if (STREQ (branchversion, key))
        isversion = 1;
      else
        isversion = 0;
    }

    while (1)
    {
      if (! rcsbuf_getkey (rcsbuf, &key, &value))
        error (1, 0, "%s does not appear to be a valid rcs file", fn_root(rcs->path));

      if (STREQ (key, "text"))
      {
        rcsbuf_valpolish (rcsbuf, value, 0, &vallen);
        if(STREQ(*deltatype,"compressed_text") || STREQ(*deltatype,"compressed_binary"))
        {
          uLong zlen;

          z_stream stream = {0};
          inflateInit(&stream);
          zlen = ntohl(*(unsigned long *)value);
          if(zlen)
          {
            stream.avail_in = vallen-4;
            stream.next_in = (Bytef*)(value+4);
            stream.avail_out = zlen;
            if(zlen>zbuflen)
            {
              zbuf=(char*)xrealloc(zbuf,zlen);
              zbuflen=zlen;
            }
            stream.next_out = (Bytef*)zbuf;
            if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
            {
              error(1,0,"internal error: inflate failed");
            }
          }
          vallen = zlen;
          value = zbuf;
          inflateEnd(&stream);
        }
        if (ishead)
        {
          if(STREQ(*deltatype,"text") || STREQ(*deltatype,"compressed_text"))
          {
            if (! linevector_add (&curlines, value, vallen, NULL, 0, 0))
              error (1, 0, "invalid rcs file %s", fn_root(rcs->path));
          }
          else if(STREQ(*deltatype,"binary") || STREQ(*deltatype,"compressed_binary"))
          {
            if (!linevector_add(&curlines, value, vallen, NULL, 0, 1))
              error (1, 0, "invalid rcs file %s", fn_root(rcs->path));
          }
          else
            error(1,0,"invalid rcs delta type %s in %s",*deltatype,fn_root(rcs->path));

          ishead = 0;
        }
        else if (isnext)
        {
          if(STREQ(*deltatype,"text") || STREQ(*deltatype,"compressed_text"))
          {
            if (! apply_rcs_changes (&curlines, value, vallen,
                    rcs->path, onbranch ? vers : NULL, onbranch ? NULL : prev_vers))
            {
              error (1, 0, "invalid change text in %s", fn_root(rcs->path));
            }
          }
          else if(STREQ(*deltatype,"binary") || STREQ(*deltatype,"compressed_binary"))
          {
            /* If we've been copied, disconnect the copy */
            if(!binbuf.binary.bb || binbuf.binary.bb->refcount>1)
            {
              if(binbuf.binary.bb)
                binbuf.binary.bb->refcount--;
              binbuf.binary.bb=(struct binbuffer*)xmalloc(sizeof(struct binbuffer));
              memset(binbuf.binary.bb,0,sizeof(struct binbuffer));
              binbuf.binary.bb->refcount=1;
            }
            if (vallen && !apply_binary_changes(&curlines, &binbuf, value, vallen))
              error(1,0,"invalid binary delta text in %s", fn_root(rcs->path));
          }
          else
            error(1,0,"invalid rcs delta type %s in %s",*deltatype,fn_root(rcs->path));
        }

        if (lst.replaceWithNull(vers))
        {
          /* Now print out or return the data we have just computed.  */
          char *p;
          size_t n;
          unsigned int ln;

          char *text = NULL; size_t len;
          linevector_copy (&headlines, &curlines);

          if(headlines.is_binary)
          {
            p = (char*)xmalloc(headlines.binary.bb->length);
            text = p;
            len = headlines.binary.bb->length;
            memcpy(p, headlines.binary.bb->buffer,headlines.binary.bb->length);
          }
          else
          {
            n = 0;
            for (ln = 0; ln < headlines.text.nlines; ++ln)
              /* 1 for \n */
              n += headlines.text.vector[ln]->len + 1;
            p = (char*)xmalloc (n);
            text = p;
            for (ln = 0; ln < headlines.text.nlines; ++ln)
            {
              memcpy (p, headlines.text.vector[ln]->text,
                  headlines.text.vector[ln]->len);
              p += headlines.text.vector[ln]->len;
              if (headlines.text.vector[ln]->has_newline)
              *p++ = '\n';
            }
            len = p - text;
            assert (len <= n);
          }

          char *sp = strrchr(rcs->path, '/');
          if (sp) sp ++; else { sp = rcs->path+strlen(rcs->path)-2; }

          char *path = (char*)xmalloc(strlen(rcs->path)+strlen(vers->version)+4+4);
          sprintf(path, "%.*sCVS/%.*s", int(sp-rcs->path), rcs->path, int(strlen(sp)-2), sp );
          make_directories(path);
          sprintf(path+strlen(path), "/#%s", vers->version);

          cvs_output("  export ", 0); cvs_output(path, 0); cvs_output("\n", 1);

          if (text)
          {
            RCS_write_binary_rev_data(path, text, len, packed);
            xfree(text);

            struct stat s;
            if (CVS_STAT(path, &s) >= 0)
              new_sz += s.st_size;
          }
          xfree(text);
        }
        break;
      }
    }

    if (isversion)
    {
      /* This is either the version we want, or it is the
          branchpoint to the version we want.  */
      if (STREQ (branchversion, rev->version))
      {
        /* This is the version we want.  */
        linevector_copy (&headlines, &curlines);
        foundhead = 1;
      }
      else
      {
        Node *p;

        /* We need to look up the branch.  */
        onbranch = 1;

        if (numdots (branchversion) < 2)
        {
          unsigned int ln;

          /* We are leaving the trunk; save the current
                  lines so that we can restore them when we
                  continue tracking down the trunk.  */
          trunk_vers = vers;
          linevector_copy (&trunklines, &curlines);

          if(!curlines.is_binary)
          {
            /* Reset the version information we have
                        accumulated so far.  It only applies to the
                        changes from the head to this version.  */
            for (ln = 0; ln < curlines.text.nlines; ++ln)
              curlines.text.vector[ln]->vers = NULL;
          }
        }

        /* The next version we want is the entry on
            VERS->branches which matches this branch.  For
            example, suppose VERSION is 1.21.4.3 and
            BRANCHVERSION was 1.21.  Then we look for an entry
            starting with "1.21.4" and we'll put it (probably
            1.21.4.1) in NEXT.  We'll advance BRANCHVERSION by
            two dots (in this example, to 1.21.4.3).  */

        if (vers->branches == NULL)
          error (1, 0, "missing expected branches in %s", fn_root(rcs->path));
        *cpversion = '.';
        ++cpversion;
        cpversion = strchr (cpversion, '.');
        if (cpversion == NULL)
          error (1, 0, "version number confusion in %s", fn_root(rcs->path));
        for (p = vers->branches->list->next; p != vers->branches->list; p = p->next)
          if (strncmp (p->key, branchversion, cpversion - branchversion) == 0)
            break;
        if (p == vers->branches->list)
          error (1, 0, "missing expected branch in %s", fn_root(rcs->path));

        next = p->key?&p->key:NULL;

        cpversion = strchr (cpversion + 1, '.');
        if (cpversion != NULL)
          *cpversion = '\0';
      }
    }
    if (foundhead)
      break;
    /* We've finished with this delta */
    rcsbuf_reuse_delta_buffer(rcs);
  } while (next != NULL);

  xfree (branchversion);

  if (! foundhead)
    error (1, 0, "could not find desired version %s in %s", rev->version, fn_root(rcs->path));

  linevector_free (&curlines);
  linevector_free (&headlines);
  linevector_free (&trunklines);
  linevector_free (&binbuf);
  xfree(zbuf);

  return;
}

static void RCS_convert_to_new_binary(RCSNode *rcs)
{
  bool packed = false;

  if (rcs->head)
  {
    RCSVers *vers;
    kflag expand;

    Node *vp = findnode (rcs->versions, rcs->head);
    if (vp == NULL)
      error (1, 0, "internal error: no revision information for %s", rcs->head);
    vers = (RCSVers *) vp->data;
    char *options = vers->kopt;

    if ((options == NULL || options[0] == '\0') && rcs->expand == NULL)
      expand.flags = KFLAG_TEXT|KFLAG_KEYWORD|KFLAG_VALUE;
    else
    {
      const char *ouroptions;

      if (options != NULL && options[0] != '\0')
        ouroptions = options;
      else
        ouroptions = rcs->expand;

      TRACE(3,"RCS_checkout options = \"%s\"",ouroptions);
      RCS_get_kflags(ouroptions, false, expand); // Don't signal an error here.. corrupt kopt in the RCS file would make checkout blow up
    }

    if((expand.flags&KFLAG_BINARY) && !(expand.flags&KFLAG_BINARY_DELTA))
    {
      struct stat s;
      if (CVS_STAT (rcs->path, &s) < 0 || s.st_size <= 0)
        return; // no gain in converting

      if (expand.flags & KFLAG_COMPRESS_DELTA)
        packed = true;
      else
        if (Node *vers = findnode (rcs->versions, rcs->head))
        {
          RCSVers *vnode = ((RCSVers *)vers->data);
          Node *fname = findnode(((RCSVers *)vers->data)->other_delta,"filename");
          if (char *opt = (fname ? wrap_rcsoption(fname->data) : NULL))
          {
            packed = strchr(opt, 'z') != NULL;
            xfree(opt);
          }
        }
      goto do_convert;
    }
  }
  return;

do_convert:
  cvs_output (packed ? "CVS: converting to -kBz: " : "CVS: converting to -kB: ", 0);
  cvs_output (rcs->path, 0);
  cvs_output ("\n", 1);

  for (;;)
  {
    char *key, *value;
    Node *vers;
    RCSVers *vnode;

    /* Rather than try to keep track of how much information we have read, just read to the end of the file.  */
    if (! rcsbuf_getrevnum (&rcs->rcsbuf, &key))
      break;

    vers = findnode (rcs->versions, key);
    if (vers == NULL)
      error (1, 0, "mismatch in rcs file %s between deltas and deltatexts", fn_root(rcs->path));

    vnode = (RCSVers *) vers->data;

    while (rcsbuf_getkey (&rcs->rcsbuf, &key, &value))
    {
      if (! STREQ (key, "text"))
      {
        Node *kv;

        if (vnode->other == NULL)
          vnode->other = getlist ();
        kv = getnode ();
        kv->type = rcsbuf_valcmp (&rcs->rcsbuf) ? RCSCMPFLD : RCSFIELD;
        kv->key = xstrdup (key);
        rcsbuf_valcopy (&kv->data, &rcs->rcsbuf, value, kv->type == RCSFIELD, (size_t *) NULL, true);
        if (addnode (vnode->other, kv) != 0)
        {
          error (0, 0, "warning: duplicate key `%s' in version `%s' of RCS file `%s'",
            key, vnode->version, fn_root(rcs->path));
          freenode (kv);
        }
        continue;
      }
      break;
    }
    rcsbuf_reuse_delta_buffer(rcs);
  }

  struct stat s;
  int64_t old_sz = 0;
  int64_t new_sz = 0;
  if (CVS_STAT(rcs->path, &s) >= 0)
    old_sz = s.st_size;

  RevStringList lst;
  RCS_get_rev_list(lst, rcs, rcs->head);
  lst.sort();
  for (int i = 0; i < lst.count; i ++)
    if (lst.data[i])
      RCS_extract_rev_data(rcs, lst.data[i], lst, packed, new_sz);

  lst.count = 0;
  RCS_get_rev_list(lst, rcs, rcs->head);
  for (int i = 0; i < lst.count; i ++)
  {
    if (lst.data[i]->kopt)
      xfree(lst.data[i]->kopt);
    lst.data[i]->kopt = xstrdup(packed ? "Bz" : "B");
    lst.data[i]->type = (char*)"text";
  }

  size_t lockId_temp;
  FILE *fout = rcs_internal_lockfile(rcs->path, &lockId_temp);

  RCS_putadmin (rcs, fout);
  RCS_putdtree (rcs, rcs->head, fout);
  RCS_putdesc (rcs, fout);

  fflush(fout);
  for (int i = 0; i < lst.count; i ++)
  {
    char *sp = strrchr(rcs->path, '/');
    if (sp) sp ++; else { sp = rcs->path+strlen(rcs->path)-2; }

    char *delta = (char*)xmalloc(16+strlen(sp)+strlen(lst.data[i]->version));
    sprintf(delta, "%s%.*s/#%s",
      STREQ(lst.data[i]->version, rcs->head) ? "" : "d1 1\na1 1\n",
      int(strlen(sp)-2), sp, lst.data[i]->version);

    Deltatext d;
    memset(&d, 0, sizeof(d));
    d.version = lst.data[i]->version;
    d.log = findnode(lst.data[i]->other, "log")->data;
    //d.other = lst.data[i]->other;
    d.text = delta;
    d.len = strlen(delta);
    putdeltatext (fout, &d, 0);
    xfree(delta);
  }

  rcsbuf_close (&rcs->rcsbuf);
  rcs_internal_unlockfile (fout, rcs->path, lockId_temp);

  free_rcsnode_contents(rcs);
  RCS_reparsercsfile(rcs);

  if (CVS_STAT(rcs->path, &s) >= 0)
    new_sz += s.st_size;

  char sbuf[256];
  snprintf(sbuf, sizeof(sbuf), "CVS: conversion done (%+lld Kb)\n", (new_sz-old_sz)/1024);
  sbuf[255] = 0;
  cvs_output (sbuf, 0);
}
