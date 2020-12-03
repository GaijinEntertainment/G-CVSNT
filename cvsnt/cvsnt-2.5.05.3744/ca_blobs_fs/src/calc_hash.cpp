#include "../blake3/blake3.h"
#include "../calc_hash.h"

bool init_blob_hash_context(char *hctx, size_t ctx_size)
{
  static_assert(sizeof(blake3_hasher) <= HASH_CONTEXT_SIZE, "sizeof hasher context expected to be bigger than defined");
  if (ctx_size < sizeof(blake3_hasher))
    return false;
  blake3_hasher_init((blake3_hasher*)hctx);
  return true;
}

void update_blob_hash(char *hctx, const char *data, size_t data_size)
{
  blake3_hasher_update((blake3_hasher*)hctx, data, data_size);
}

bool finalize_blob_hash(char *hctx, unsigned char *digest, size_t digest_size)
{
  blake3_hasher_finalize((blake3_hasher*)hctx, digest, digest_size);
  return true;
}
