/*
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2004 Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "global.h"

#include "md4.h"
#include "des.h"

/*
 * The following code is based on some psuedo-C code from ronald@innovation.ch
 */

static void ntlm_encrypt_answer (unsigned char *hash,
				 const unsigned char *challenge,
				 unsigned char *answer);
static void ntlm_convert_key (unsigned char *key_56, DES_KEY * ks);
static void ntlm_des_set_odd_parity (char *key);

NTLM_STATIC void
SMBencrypt (const char *passwd, const uint8 * challenge, uint8 * answer)
{
#define MAX_PW_SZ 14
  int len;
  int i;
  static const char magic[8] =
    { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
  DES_KEY ks;
  unsigned char hash[24];
  unsigned char passwd_up[MAX_PW_SZ];

  /* convert password to upper and pad to 14 chars */
  memset (passwd_up, 0, MAX_PW_SZ);
  len = strlen (passwd);
  if (len > MAX_PW_SZ)
    len = MAX_PW_SZ;
  for (i = 0; i < len; i++)
    passwd_up[i] = toupper ((unsigned char) passwd[i]);

  /* hash the first 7 characters */
  ntlm_convert_key (passwd_up, &ks);
  ntlm_des_ecb_encrypt (&magic, sizeof (magic), &ks, (hash + 0));

  /* hash the second 7 characters */
  ntlm_convert_key (passwd_up + 7, &ks);
  ntlm_des_ecb_encrypt (&magic, sizeof (magic), &ks, (hash + 8));

  memset (hash + 16, 0, 5);

  ntlm_encrypt_answer (hash, challenge, answer);

  /* with security is best be pedantic */
  memset (&ks, 0, sizeof (ks));
  memset (hash, 0, sizeof (hash));
  memset (passwd_up, 0, sizeof (passwd_up));
}

NTLM_STATIC void
SMBNTencrypt (const char *passwd, const uint8 * challenge, uint8 * answer)
{
  size_t len, i;
  DES_KEY ks;
  unsigned char hash[24];
  unsigned char nt_pw[256];
  MD4_CTX context;

  /* NT resp */
  len = strlen (passwd);
  if (len > 128)
    len = 128;
  for (i = 0; i < len; ++i)
    {
      nt_pw[2 * i] = passwd[i];
      nt_pw[2 * i + 1] = 0;
    }

  MD4Init (&context);
  MD4Update (&context, nt_pw, len * 2);
  MD4Final (&context, hash);

  memset (hash + 16, 0, 5);
  ntlm_encrypt_answer (hash, challenge, answer);

  /* with security is best be pedantic */
  memset (&ks, 0, sizeof (ks));
  memset (hash, 0, sizeof (hash));
  memset (nt_pw, 0, sizeof (nt_pw));
  memset (&context, 0, sizeof (context));
}

/*
* takes a 21 byte array and treats it as 3 56-bit DES keys. The
* 8 byte plaintext is encrypted with each key and the resulting 24
* bytes are stored in the results array.
*/
static void
ntlm_encrypt_answer (unsigned char *hash, const unsigned char *challenge,
		     unsigned char *answer)
{
  DES_KEY ks;

  ntlm_convert_key (hash, &ks);
  ntlm_des_ecb_encrypt (challenge, 8, &ks, answer);

  ntlm_convert_key (&hash[7], &ks);
  ntlm_des_ecb_encrypt (challenge, 8, &ks, &answer[8]);

  ntlm_convert_key (&hash[14], &ks);
  ntlm_des_ecb_encrypt (challenge, 8, &ks, &answer[16]);

  memset (&ks, 0, sizeof (ks));
}

static void
ntlm_des_set_odd_parity (char *key)
{
  int i;
  unsigned char parity;

  for (i = 0; i < 8; i++)
    {
      parity = key[i];

      parity ^= parity >> 4;
      parity ^= parity >> 2;
      parity ^= parity >> 1;

      key[i] = (key[i] & 0xfe) | (parity & 1);
    }
}

/*
* turns a 56 bit key into the 64 bit, odd parity key and sets the key.
* The key schedule ks is also set.
*/
static void
ntlm_convert_key (unsigned char *key_56, DES_KEY * ks)
{
  unsigned char key[8];

  key[0] = key_56[0];
  key[1] = ((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1);
  key[2] = ((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2);
  key[3] = ((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3);
  key[4] = ((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4);
  key[5] = ((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5);
  key[6] = ((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6);
  key[7] = (key_56[6] << 1) & 0xFF;

  ntlm_des_set_odd_parity (key);
  ntlm_des_set_key (ks, key, sizeof (key));

  memset (&key, 0, sizeof (key));
}


/** \@} */
