#ifndef CLIENT__H
#define CLIENT__H
/* Interface between the client and the rest of CVS.  */

/* Stuff shared with the server.  */
extern char *mode_to_string(mode_t mode);
extern int change_mode(char *filename, char *mode_string, int respect_umask);

extern int gzip_level;
extern int file_gzip_level;

/* Whether the connection should be encrypted.  */
extern int cvsencrypt;

/* Whether the connection should be authenticated.  */
extern int cvsauthenticate;

struct buffer *cvs_encrypt_wrap_buffer_initialize (const struct protocol_interface *protoocol, struct buffer *buf,
     int input, int encrypt, void (*memory)(struct buffer *));

/*
 * Flag variable for seeing whether the server has been started yet.
 * As of this writing, only edit.c:notify_check() uses it.
 */
extern int server_started;

/* Is the -P option to checkout or update specified?  */
extern int client_prune_dirs;

#if defined (SERVER_SUPPORT) 
extern void server_authenticate_connection();
#endif

/* Talking to the server. */
void send_to_server(char const *str, size_t len);
void send_to_server_untranslated(char const *str, size_t len);
void read_from_server(char *buf, size_t len);

/* Internal functions that handle client communication to server, etc.  */
int supported_request(const char *name);
void option_with_arg(const char *option, const char *arg);

/* Get the responses and then close the connection.  */
extern int get_responses_and_close();

extern int get_server_responses();
extern int get_server_responses_noproxy();

/* close after finished */
extern int cleanup_and_close_server();

/* Start up the connection to the server on the other end.  */
int start_server(int verify_only);

/* Send the names of all the argument files to the server.  */
void
send_file_names(int argc, char **argv, unsigned int flags);

/* Flags for send_file_names.  */
/* Expand wild cards?  */
#define SEND_EXPAND_WILD 1

/*
 * Send Repository, Modified and Entry.  argc and argv contain only
 * the files to operate on (or empty for everything), not options.
 * local is nonzero if we should not recurse (-l option).
 */
void
send_files(int argc, char **argv, int local, int aflag, unsigned int flags);

/* Flags for send_files.  */
#define SEND_BUILD_DIRS			0x001
#define SEND_FORCE				0x002
#define SEND_NO_CONTENTS		0x004
#define BACKUP_MODIFIED_FILES	0x008
#define SEND_DIRECTORIES_ONLY	0x010
#define SEND_CASE_SENSITIVE		0x020

/* Send an argument to the remote server.  */
void send_arg(const char *string);

/* Send a string of single-char options to the remote server, one by one.  */
void
send_option_string(char *string);

extern void send_a_repository(const char *dir, const char *repository, const char *update_dir);

	/*
     * ok and error are special; they indicate we are at the end of the
     * responses, and error indicates we should exit with nonzero
     * exitstatus.
     */
enum response_type {response_type_normal, response_type_ok, response_type_error};

    /* Used by the server to indicate whether response is supported by
       the client, as set by the Valid-responses request.  */
enum response_status {
      /*
       * Failure to implement this response can imply a fatal
       * error.  This should be set only for responses which were in the
       * original version of the protocol; it should not be set for new
       * responses.
       */
      rs_essential,

      /* Some clients might not understand this response.  */
      rs_optional,

      /*
       * Set by the server to one of the following based on what this
       * client actually supports.
       */
      rs_supported,
      rs_not_supported
};

/*
 * This structure is used to catalog the responses the client is
 * prepared to see from the server.
 */

struct response
{
    /* Name of the response.  */
    char *name;

    /*
     * Function to carry out the response.  ARGS is the text of the
     * command after name and, if present, a single space, have been
     * stripped off.  The function can scribble into ARGS if it wants.
     * Note that although LEN is given, ARGS is also guaranteed to be
     * '\0' terminated.
     */
    void (*func)(char *args, int len);
    void (*proxy)(char *cmd);

	enum response_type type;
	enum response_status status;
};

/* Table of responses ending in an entry with a NULL name.  */

extern struct response responses[];

extern void client_senddate(const char *date);
extern void client_expand_modules(int argc, char **argv, int local);
extern void client_send_expansions(int local, char *where, int build_dirs);

extern char **failed_patches;
extern int failed_patches_count;
extern char *toplevel_wd;
extern void client_import_setup(char *repository);
extern int client_process_import_file(const char *message, const char *vfile, const char *vtag, int targc, const char **targv, const char *repository, const char *options, int modtime);
extern void client_import_done();
extern void client_notify(char *repository, char *update_dir, char *filename, int notif_type, char *val, char *user);

extern void from_server_buffer_read(char **line, int *lenp);
extern void to_server_buffer_flush();

#endif /* CLIENT__H */

