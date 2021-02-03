#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <memory>
#include "fileio.h"
#include "../../blake3/blake3.h"
#include "../content_addressed_fs.h"
#include "../ca_blob_format.h"
#include "../calc_hash.h"
#include "../streaming_blobs.h"

//we can implement remapping of hashes, or even different hash structure (hash_type->root_folder) with symlinks to same content
namespace caddressed_fs
{
#define BLOBS_SUB_FOLDER "blobs"

static const char *blobs_sub_folder = BLOBS_SUB_FOLDER;
static bool allow_trust = true;
void set_allow_trust(bool p){allow_trust = p;}
const char* blobs_subfolder(){return blobs_sub_folder;}

std::string dir_for_roots = "";
void set_dir_for_roots(const char* s){dir_for_roots = s ? s : "";}

std::string default_tmp_dir = "";
void set_temp_dir(const char *p){default_tmp_dir = p ? p : "";}

static struct context
{
  std::string root_path = "./" BLOBS_SUB_FOLDER "/";
} def_ctx;

context *get_default_ctx() {return &def_ctx;}
context *create() {return new context;}
void destroy(context *ctx) {if (ctx && ctx != &def_ctx) delete ctx;}
std::string blobs_dir_path(const context *ctx){return ctx->root_path;}

void set_root(context *ctx, const char *p)
{
  //better use std::format, but it is c++20
  ctx->root_path = dir_for_roots;
  if (ctx->root_path.length() && ctx->root_path[ctx->root_path.length()-1] != '/' && p[0] != '/')
    ctx->root_path += "/";
  ctx->root_path += p;
  if (ctx->root_path[ctx->root_path.length()-1] != '/')
    ctx->root_path += "/";
  ctx->root_path += blobs_sub_folder;
  ctx->root_path += "/";
}

inline bool make_blob_dirs(std::string &fp)
{
  if (fp.length()<69)
    return false;
  if (fp[fp.length()-65] != '/' || fp[fp.length()-68] != '/')
    return false;
  fp[fp.length()-68] = 0;
  blob_fileio_ensure_dir(fp.c_str());
  fp[fp.length()-68] = '/'; fp[fp.length()-65] = 0;
  blob_fileio_ensure_dir(fp.c_str());
  fp[fp.length()-65] = '/';
  return true;
}

inline std::string get_file_path(const std::string &root_path, const char* hash_hex_string)
{
  char buf[74];
  std::snprintf(buf,sizeof(buf), "%.2s/%.2s/%.64s", hash_hex_string, hash_hex_string+2, hash_hex_string);
  return root_path + buf;
}

std::string get_file_path(const context *ctx, const char* hash_hex_string)
{
  return get_file_path(ctx->root_path, hash_hex_string);
}

size_t get_size(const context *ctx, const char* hash_hex_string)
{
  return blob_fileio_get_file_size(get_file_path(ctx->root_path, hash_hex_string).c_str());
}

bool exists(const context *ctx, const char* hash_hex_string)
{
  return blob_fileio_is_file_readable(get_file_path(ctx->root_path, hash_hex_string).c_str());
}

class PushData
{
public:
  FILE *fp;
  std::string root;
  std::string temp_file_name;
  char provided_hash[64];
  DownloadBlobInfo info;
  blake3_hasher hasher;
};

PushData* start_push(const context *ctx, const char* hash_hex_string)
{
  if (hash_hex_string && blob_fileio_is_file_readable(get_file_path(ctx->root_path, hash_hex_string).c_str()))
  {
    //magic number. we dont need to do anything if such exists, but that's not an error.
    //we save cpu by not decoding/encoding data and calc hash again. even if real hash is different, no real issue, we won't overwrite correct one. Only liars are 'harmed'
    auto r = new PushData{(FILE*)uintptr_t(1)};
    memcpy(r->provided_hash, hash_hex_string, 64);
    return r;
  }
  std::string temp_file_name;
  FILE * tmpf = blob_fileio_get_temp_file(temp_file_name, default_tmp_dir.length()>0 ? default_tmp_dir.c_str() : ctx->root_path.c_str());
  if (!tmpf)
    return nullptr;
  auto r = new PushData{tmpf, ctx->root_path, std::move(temp_file_name)};
  if (hash_hex_string)
    memcpy(r->provided_hash, hash_hex_string, 64);
  else
    r->provided_hash[0] = 0;
  if (!allow_trust || !r->provided_hash[0])
    blake3_hasher_init(&r->hasher);
  return r;
}

bool stream_push(PushData *fp, const void *data, size_t data_size)
{
  if (fp == nullptr)
    return false;
  if (uintptr_t(fp->fp) == 1)
    return true;
  if (!fp->fp || data_size == invalid_size)
    return false;
  if (fwrite(data, 1, data_size, fp->fp) != data_size)//we write packed data as is
    return false;
  if (allow_trust && fp->provided_hash[0])//we trusted cache.
    return true;
  //but we always unpack to calculate hash, unless hash is provided so we can trust it
  if (!decode_stream_blob_data(fp->info, (const char*)data, data_size, [&](const void *unpacked_data, size_t sz)
    {blake3_hasher_update(&fp->hasher, unpacked_data, sz);return true;}))
  {
    fclose(fp->fp); fp->fp = nullptr;
    blob_fileio_unlink_file(fp->temp_file_name.c_str());
    return false;
  }
  return true;
}

void destroy(PushData *fp)
{
  if (fp == nullptr)
    return;
  std::unique_ptr<PushData> kill(fp);
  if (!fp->fp || uintptr_t(fp->fp) == 1)
    return;
  fclose(fp->fp);fp->fp = nullptr;
  blob_fileio_unlink_file(fp->temp_file_name.c_str());
}

PushResult finish(PushData *fp, char *actual_hash_str)
{
  if (fp == nullptr)
    return PushResult::EMPTY_DATA;
  std::unique_ptr<PushData> kill(fp);
  if (uintptr_t(fp->fp) == 1)
  {
    if (actual_hash_str)
      memcpy(actual_hash_str, fp->provided_hash, 64);
    return PushResult::DEDUPLICATED;
  }
  if (!fp->fp)
    return PushResult::IO_ERROR;
  fclose(fp->fp);fp->fp = nullptr;

  char final_hash[64];
  char *final_hash_p = actual_hash_str ? actual_hash_str : final_hash;
  if (!allow_trust || !fp->provided_hash[0])
  {
    unsigned char digest[32];
    blake3_hasher_finalize(&fp->hasher, digest, sizeof(digest));
    bin_hash_to_hex_string_64(digest, final_hash_p);
    if (fp->provided_hash[0] && memcmp(fp->provided_hash, final_hash_p, 64) != 0)
      return PushResult::WRONG_HASH;
  } else
    memcpy(final_hash_p, fp->provided_hash, 64);
  std::string filepath = get_file_path(fp->root, final_hash_p);
  if (blob_fileio_is_file_readable(filepath.c_str()))//was pushed by someone else
  {
    blob_fileio_unlink_file(fp->temp_file_name.c_str());
    return PushResult::DEDUPLICATED;
  }
  make_blob_dirs(filepath);
  return blob_fileio_rename_file(fp->temp_file_name.c_str(), filepath.c_str()) ? PushResult::OK : PushResult::IO_ERROR;
}

PullData* start_pull(const context *ctx, const char* hash_hex_string, size_t &blob_sz)
{
  return (PullData*)blobe_fileio_start_pull(get_file_path(ctx->root_path, hash_hex_string).c_str(), blob_sz);
}
const char *pull(PullData* fp, uint64_t from, size_t &data_pulled){return blobe_fileio_pull((BlobFileIOPullData*)fp, from, data_pulled);}
bool destroy(PullData* up) { return blobe_fileio_destroy((BlobFileIOPullData*)up);}

bool get_file_content_hash(const char *filename, char *hash_hex_str, size_t hash_len)
{
  if (hash_len < 64)
    return false;
  hash_hex_str[0] = 0;hash_hex_str[hash_len-1] = 0;

  size_t blob_sz;
  BlobFileIOPullData* pd = blobe_fileio_start_pull(filename, blob_sz);
  if (!pd)
    return false;
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  uint64_t at = 0;
  bool ok = true;

  do {
    size_t data_pulled = 0;
    const char *data = blobe_fileio_pull(pd, at, data_pulled);
    if (!data)
    {
      ok = false;
      break;
    }
    blake3_hasher_update(&hasher, data, data_pulled);
    at += data_pulled;
  } while(at < blob_sz);

  unsigned char digest[32];
  blake3_hasher_finalize(&hasher, digest, sizeof(digest));
  bin_hash_to_hex_string_64(digest, hash_hex_str);
  if (hash_len > 64)
   hash_hex_str[64] = 0;

  if (!blobe_fileio_destroy(pd))
    return false;
  return ok;
}

int64_t repack(const context *ctx, const char* hash_hex_string, bool repack_unpacked)
{
  using namespace streaming_compression;
  size_t curSize = 0;
  std::unique_ptr<PullData, bool(*)(PullData*)> in(start_pull(ctx, hash_hex_string, curSize), caddressed_fs::destroy);
  if (!in)
    return 0;
  //check if it is already repacked.
  BlobHeader wasHdr;
  {
    size_t sz = 0;
    const BlobHeader & hdr = *(const BlobHeader *)pull(in.get(), 0, sz);
    if (sz < sizeof(BlobHeader))
      return 0;
    wasHdr = hdr;
  }
  if ((wasHdr.flags&BlobHeader::BEST_POSSIBLE_COMPRESSION) != 0)
    return 0;
  const bool is_unpacked_blob = !is_packed_blob(wasHdr);
  if (!repack_unpacked && is_unpacked_blob)
    return 0;
  BlobHeader hdr = get_header(zstd_magic, wasHdr.uncompressedLen, BlobHeader::BEST_POSSIBLE_COMPRESSION);
  std::string temp_file_name;
  FILE * tmpf = blob_fileio_get_temp_file(temp_file_name, default_tmp_dir.length()>0 ? default_tmp_dir.c_str() : ctx->root_path.c_str());
  if (!tmpf)
    return 0;
  char cctx[CTX_SIZE];
  if (fwrite(&hdr, 1, sizeof(hdr), tmpf) != sizeof(hdr) || !init_compress_stream(cctx, sizeof(cctx), 19, StreamType::ZSTD))
  {
    fclose(tmpf);
    blob_fileio_unlink_file(temp_file_name.c_str());
    return 0;
  }

  uint64_t at = 0;
  char buf[65536];
  DownloadBlobInfo info;
  size_t compressedSize = 0, uncompressedSize = 0;
  do {// with mmap that's one iteration
    size_t sz = 0;
    const char *got = pull(in.get(), at, sz);
    if (!sz)
      break;
    if (!decode_stream_blob_data(info, got, sz, [&](const char *unpacked_data, size_t unpacked_sz)
      {
        size_t src_pos = 0;
        while (src_pos < unpacked_sz)
        {
          size_t dst_pos = 0;
          size_t wasSrcPos = src_pos;
          if (compress_stream(cctx, unpacked_data, src_pos, unpacked_sz, buf, dst_pos, sizeof(buf)) == StreamStatus::Error)
            return false;
          compressedSize += dst_pos;
          uncompressedSize += src_pos-wasSrcPos;
          if (fwrite(buf, 1, dst_pos, tmpf) != dst_pos)
            return false;
          //if already tried to encode 1mb of data, without any success - stop trying
          if (is_unpacked_blob && uncompressedSize > (1<<20) && compressedSize >= uncompressedSize)
            return false;
        }
        return true;
      }))
      break;
    at += sz;
  } while (at < curSize);
  in.reset();

  StreamStatus st = StreamStatus::Continue;
  while (at == curSize && st == StreamStatus::Continue)
  {
    size_t dst_pos = 0;
    st = finalize_compress_stream(cctx, buf, dst_pos, sizeof(buf));
    if ( fwrite(buf, 1, dst_pos, tmpf) != dst_pos )
      break;
  }
  if (st != StreamStatus::Finished)
    kill_compress_stream(cctx);
  fclose(tmpf);

  size_t finalSize = blob_fileio_get_file_size(temp_file_name.c_str());
  if (finalSize == invalid_blob_file_size ||
      at < curSize || st != StreamStatus::Finished || (finalSize > curSize && !is_zlib_blob(wasHdr)))//if it was unpacked file and not zlib, then keep it unpacked. Unpacked files are faster to update
  {
    blob_fileio_unlink_file(temp_file_name.c_str());
    return 0;
    //we probably should write BEST_POSSIBLE_COMPRESSION flag in current header, if it is not compressing
  }
 if (blob_fileio_rename_file(temp_file_name.c_str(), get_file_path(ctx->root_path, hash_hex_string).c_str()))
    return int64_t(curSize) - int64_t(finalSize);
  return 0;
}

}