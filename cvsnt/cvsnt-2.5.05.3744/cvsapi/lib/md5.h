/* See md5.c for explanation and copyright information.  */

#ifndef MD5_H
#define MD5_H

#ifdef __cplusplus
extern "C" {
#endif

/* Unlike previous versions of this code, uint32 need not be exactly
   32 bits, merely 32 bits or more.  Choosing a data type which is 32
   bits instead of 64 is not important; speed is considerably more
   important.  ANSI guarantees that "unsigned long" will be big enough,
   and always using it seems to have few disadvantages.  */
typedef unsigned long md5_uint32;

struct cvs_MD5Context
{
	md5_uint32 buf[4];
	md5_uint32 bits[2];
	unsigned char in[64];
};

void cvs_MD5Init (struct cvs_MD5Context *context);
void cvs_MD5Update (struct cvs_MD5Context *context, unsigned char const *buf, size_t len);
void cvs_MD5Final (unsigned char digest[16], struct cvs_MD5Context *context);
void cvs_MD5Transform (md5_uint32 buf[4], const unsigned char in[64]);

#ifdef __cplusplus
}
#endif

#endif /* !MD5_H */
