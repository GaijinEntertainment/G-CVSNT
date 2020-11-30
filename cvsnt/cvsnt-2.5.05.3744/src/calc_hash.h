#pragma once

enum {HASH_CONTEXT_SIZE = 2048};
bool init_blob_hash_context(char *ctx, size_t ctx_size);
void update_blob_hash(char *ctx, const char *data, size_t data_size);
bool finalize_blob_hash(char *ctx, unsigned char *digest, size_t digest_size=32);//digest_size <= 32
