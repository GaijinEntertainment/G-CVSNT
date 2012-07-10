#ifndef PLINK__CVSNT__H
#define PLINK__CVSNT__H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
	#ifndef PLINK_EXPORT
	#define PLINK_EXPORT __declspec(dllimport)
	#endif
#else
	#define PLINK_EXPORT
#endif

typedef struct
{
	int (*getpass)(char *password, int max_length, const char *prompt); /* return 1 OK, 0 Cancel */
	int (*yesno)(const char *message, const char *title, int withcancel); /* Return -1 cancel, 0 No, 1 Yes */
} putty_callbacks;

int PLINK_EXPORT plink_connect(const char *username, const char *password, const char *keyfile, const char *host, unsigned port, char version, const char *cmd, const char *proxyname, const char *proxyport, const char *proxyuser, const char *proxypassword);
int PLINK_EXPORT plink_write_data(const void *buffer, int length);
int PLINK_EXPORT plink_read_data(void *buffer, int max_length);
putty_callbacks PLINK_EXPORT *plink_set_callbacks(putty_callbacks *new_callbacks);

#ifdef __cplusplus
}
#endif

#endif