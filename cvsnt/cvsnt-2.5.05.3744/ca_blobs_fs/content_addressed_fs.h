#pragma once
#include <stdint.h>
//content-addressed-file-storage
namespace caddressed_fs
{
  enum class PushResult {OK, DEDUPLICATED, IO_ERROR, WRONG_HASH, EMPTY_DATA};
  inline bool is_ok(PushResult pr){return pr == PushResult::OK || pr == PushResult::DEDUPLICATED;}

  struct context;
  context *create();
  void destroy(context *);

  //"" or nullptr is accepted and means "/" (on Linux, on Windows you have to pass correct dir)
  // this is folder where all roots will be located. usually it is "/" or nullptr
  void set_dir_for_roots(const char*);
  //set root folder (fullpath) where 'blobs' of hash type hash_type will be stored. This folder can be anywhere, but temp should be in set system.
  // the library will create subfolders of name xx/yy/ inside that root
  // but in a future we can add support of different hashes easily. Content itself isn't aware of it's hashing method.
  //root will be used
  context *get_default_ctx();
  void set_root(context *ctx, const char*);//if passed root is nullptr, we wil use default root

  //it is ok to pass nullptr or "". In that case root/blobs will be used as temp dir
  void set_temp_dir(const char *p);

  //trust client provided hashes (if any)
  //actually, there is nothing that really bad with trusting client on their hash, actually
  //in the worst case scenario that would be "bad" blob just to be removed in gc phase
  //we can't spoil repo that way - overwriting existing blobs are not allowed (unless in a possible race, but then you have to guess the current hash)
  //it is kinda possible to 'spam' with incorrect hashed blobs - but then again, not much harder to spam with correct one
  //so, we enable trust by default
  void set_allow_trust(bool);

  class PushData;
  class PullData;

  static constexpr size_t invalid_size = size_t(~size_t(0));
  size_t get_size(const context *ctx, const char* hash_hex_string);
  bool exists(const context *ctx, const char* hash_hex_string);

  //if we provide hash_hex_string, we will trust it. this is for tools, networking server never trusts
  PushData* start_push(const context *ctx, const char* hash_hex_string = nullptr);
  bool stream_push(PushData *pd, const void *data, size_t data_size);
  PushResult finish(PushData *pd, char *actual_hash_str);//will destroy it. if actual_hash_str = null, hash won't be returned
  void destroy(PushData *pd);

  //pull allows random access
  //will do memory mapping. blob_sz is the size of whole blob. if invalid returns nullptr
  PullData *start_pull(const context *ctx, const char* hash_hex_string, size_t &blob_sz);
  const char *pull(PullData *pd, uint64_t from, size_t &data_pulled);//returned data_pulled != 0, unless error
  bool destroy(PullData *pd);//will destroy it

  //
  //only for compatibility
  bool get_file_content_hash(const char *filename, char *hash_hex_str, size_t hash_len);
};
