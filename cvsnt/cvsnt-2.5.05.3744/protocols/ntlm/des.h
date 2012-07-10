#ifndef NTLM_DES_H_
#define NTLM_DES_H_

typedef struct des_key
{
  char kn[16][8];
  word32 sp[8][64];
  char iperm[16][16][8];
  char fperm[16][16][8];
} DES_KEY;

NTLM_STATIC int
ntlm_des_ecb_encrypt (const void *plaintext, int len, DES_KEY * akey,
		      char output[8]);
NTLM_STATIC int ntlm_des_set_key (DES_KEY * dkey, char *user_key, int len);

#endif /*  NTLM_DES_H_ */
