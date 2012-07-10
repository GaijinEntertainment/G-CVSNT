#ifndef UNICODE_STUFF__H
#define UNICODE_STUFF__H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	const char *encoding;
	int bom;
} diff_encoding_type;

int begin_encoding(const diff_encoding_type *from);
int end_encoding();
int convert_encoding(const char *inbuf, size_t len, char **outbuf, size_t *outlen);
int strip_crlf(char *buf, size_t *len);

extern const diff_encoding_type __diff_encoding_utf8;
#define DIFF_ENCODING_UTF8 &__diff_encoding_utf8

#ifdef __cplusplus
}
#endif

#endif
