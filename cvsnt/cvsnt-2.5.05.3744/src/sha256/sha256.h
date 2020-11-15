#ifndef SHA256_BLOCK_SHA256_H
#define SHA256_BLOCK_SHA256_H

#include <stdint.h>

#define blk_SHA256_BLKSIZE 64

struct blk_SHA256_CTX {
	uint32_t state[8];
	uint64_t size;
	uint32_t offset;
	uint8_t buf[blk_SHA256_BLKSIZE];
};

typedef struct blk_SHA256_CTX blk_SHA256_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

void blk_SHA256_Init(blk_SHA256_CTX *ctx);
void blk_SHA256_Update(blk_SHA256_CTX *ctx, const void *data, size_t len);
void blk_SHA256_Final(unsigned char *digest, blk_SHA256_CTX *ctx);

#ifdef __cplusplus
}
#endif

#endif
