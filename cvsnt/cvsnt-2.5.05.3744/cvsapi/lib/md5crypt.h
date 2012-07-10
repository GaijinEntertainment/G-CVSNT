#ifndef MD5CRYPT__H
#define MD5CRYPT__H

#ifdef __cplusplus
extern "C" {
#endif

char *md5_crypt(const char *pw, const char *salt);
int compare_crypt(const char *text, const char *crypt_pw);

#ifdef __cplusplus
}
#endif

#endif
