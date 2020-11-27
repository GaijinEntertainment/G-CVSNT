#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>
#include "sha256/sha256.h"
# ifdef _MSC_VER
  #include <intrin.h>
# else
  #include <x86intrin.h>
# endif

#define SHA256_LIST(a)\
(a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5],(a)[6],(a)[7],(a)[8],(a)[9],\
(a)[10],(a)[11],(a)[12],(a)[13],(a)[14],(a)[15],(a)[16],(a)[17],(a)[18],(a)[19],\
(a)[20],(a)[21],(a)[22],(a)[23],(a)[24],(a)[25],(a)[26],(a)[27],(a)[28],(a)[29],\
(a)[30],(a)[31]

void calc_hash(const void *data, size_t len, unsigned char sha256[], unsigned &a)
{
  blk_SHA256_CTX ctx;
  blk_SHA256_Init(&ctx);
  blk_SHA256_Update(&ctx, data, len);//that would make attack significantly harder. But we dont expect attacks on our repo.
  blk_SHA256_Final(sha256, &ctx);
  a += sha256[0];
}

int main(int ac, const char* argv[])
{
  if (ac < 2)
  {
    printf("filename");
    return 1;
  }
  FILE *f = fopen(argv[1], "rb");
  fseek(f, 0, SEEK_END);
  unsigned fsz = ftell(f);
  char *data = (char*)malloc(fsz);
  fseek(f, 0, SEEK_SET);
  fread(data, 1, fsz, f);
  fclose(f);
  unsigned char sha256[32];
  unsigned a = 0;
  uint64_t best = ~uint64_t(0);
  for (int i = 0; i < 500; ++i)
  {
    int64_t reft = __rdtsc();
    calc_hash(data, fsz, sha256, a);
    int64_t result = __rdtsc() - reft;
    if (result < best)
      best = result;
  }
  printf("a=%d total ticks %lld\nSHA256="
     "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    a, best, SHA256_LIST(sha256));
  free(data);
  return 0;
}
