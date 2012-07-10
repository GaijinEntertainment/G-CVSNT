/*
 * console.c: various interactive-prompt routines shared between
 * the console PuTTY tools
 */

#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

#include "putty/putty.h"
#include "putty/storage.h"
#include "putty/ssh.h"

#include <process.h>

#include "plink_cvsnt.h"

static int putty_getpass(char *password, int max_length, const char *prompt);
static int putty_prompt(const char *message, const char *title, int withcancel);

putty_callbacks default_callbacks =
{
	putty_getpass,
	putty_prompt
};

putty_callbacks *callbacks = &default_callbacks;

int console_batch_mode = FALSE;
int fatal_exit = FALSE;
int fatal_exit_code;

/*
 * Clean up and exit.
 */
void cleanup_exit(int code)
{
    /*
     * Clean up.
     */
    sk_cleanup();

    random_save_seed();
#ifdef MSCRYPTOAPI
    crypto_wrapup();
#endif

	fatal_exit = TRUE;
	fatal_exit_code = code?code:-1;
	_endthreadex(code);
}

void verify_ssh_host_key(void *frontend, char *host, int port, char *keytype,
          char *keystr, char *fingerprint)
{
    int ret;
    static const char absentmsg_batch[] =
   "The server's host key is not cached in the registry. You\n"
   "have no guarantee that the server is the computer you\n"
   "think it is.\n"
   "The server's key fingerprint is:\n"
   "%s\n"
   "Connection abandoned.\n";
    static const char absentmsg[] =
   "The server's host key is not cached in the registry. You\n"
   "have no guarantee that the server is the computer you\n"
   "think it is.\n"
   "The server's key fingerprint is:\n"
   "%s\n"
   "If you trust this host, hit Yes to add the key to\n"
   "PuTTY's cache and carry on connecting.\n"
   "If you want to carry on connecting just once, without\n"
   "adding the key to the cache, hit No.\n"
   "If you do not trust this host, hit Cancel to abandon the\n"
   "connection.";

    static const char wrongmsg_batch[] =
   "WARNING - POTENTIAL SECURITY BREACH!\n"
   "The server's host key does not match the one PuTTY has\n"
   "cached in the registry. This means that either the\n"
   "server administrator has changed the host key, or you\n"
   "have actually connected to another computer pretending\n"
   "to be the server.\n"
   "The new key fingerprint is:\n"
   "%s\n"
   "Connection abandoned.\n";
    static const char wrongmsg[] =
   "WARNING - POTENTIAL SECURITY BREACH!\n"
   "The server's host key does not match the one PuTTY has\n"
   "cached in the registry. This means that either the\n"
   "server administrator has changed the host key, or you\n"
   "have actually connected to another computer pretending\n"
   "to be the server.\n"
   "The new key fingerprint is:\n"
   "%s\n"
   "If you were expecting this change and trust the new key,\n"
   "hit Yes to update PuTTY's cache and continue connecting.\n"
   "If you want to carry on connecting but without updating\n"
   "the cache, hit No.\n"
   "If you want to abandon the connection completely, hit\n"
   "Cancel. Cancel is the ONLY guaranteed safe choice.";

    static const char mbtitle[] = "Security Alert";

    char message[160 +
                 /* sensible fingerprint max size */
                 (sizeof(absentmsg) > sizeof(wrongmsg) ?
                  sizeof(absentmsg) : sizeof(wrongmsg))];

    /*
     * Verify the key against the registry.
     */
    ret = verify_host_key(host, port, keytype, keystr);

	switch(ret)
	{
	case 0:
		return; /* success - key matched OK */
	case 1:  /* key was absent */
		sprintf(message, absentmsg, fingerprint);
		break;
	case 2: /* key was different */
         sprintf(message, wrongmsg, fingerprint);
		 break;
	}
	switch(callbacks->yesno(message,mbtitle,1))
	{
	case -1: /* Cancel */
		cleanup_exit(0);
		break;
	case 0: /* No */
		break;
	case 1: /* Yes */
		store_host_key(host, port, keytype, keystr);
		break;
	}
}

/*
 * Ask whether the selected cipher is acceptable (since it was
 * below the configured 'warn' threshold).
 * cs: 0 = both ways, 1 = client->server, 2 = server->client
 */
void askcipher(void *frontend, char *ciphername, int cs)
{
   static const char mbtitle[] = "Security Alert";

   static const char msg[] =
   "The first %.35scipher supported by the server\n"
   "is %.64s, which is below the configured\n"
   "warning threshold.\n"
   "Do you want to continue with this connection?";

   /* guessed cipher name + type max length */
   char message[100 + sizeof(msg)];

   sprintf(message, msg,
           (cs == 0) ? "" :
           (cs == 1) ? "client-to-server " : "server-to-client ",
           ciphername);

	switch(callbacks->yesno(message,mbtitle,0))
	{
		case 0:
			cleanup_exit(0);
			break;
	}
}

/*
 * Ask whether to wipe a session log file before writing to it.
 * Returns 2 for wipe, 1 for append, 0 for cancel (don't log).
 */
int askappend(void *frontend, Filename filename)
{
    static const char mbtitle[] = "Log to File";

    static const char msgtemplate[] =
	"The session log file \"%.*s\" already exists.\n"
	"You can overwrite it with a new session log,\n"
	"append your session log to the end of it,\n"
	"or disable session logging for this session.\n"
	"Enter \"y\" to wipe the file, \"n\" to append to it,\n"
	"or just press Return to disable logging.\n"
	"Wipe the log file? (y/n, Return cancels logging) ";

	char message[sizeof(msgtemplate) + FILENAME_MAX];

    sprintf(message, msgtemplate, FILENAME_MAX, filename);

	switch(callbacks->yesno(message,mbtitle,1))
	{
		case 0: /* No */
			return 1;
		case 1: /* Yes */
			return 2;
		default:
		case -1: /* Cancel */
			return 0;
	}
}

/*
 * Warn about the obsolescent key file format.
 */
void old_keyfile_warning(void)
{
   static const char mbtitle[] = "Key File Warning";
    static const char message[] =
   "You are loading an SSH 2 private key which has an\n"
   "old version of the file format. This means your key\n"
   "file is not fully tamperproof. Future versions of\n"
   "PuTTY may stop supporting this private key format,\n"
   "so we recommend you convert your key to the new\n"
   "format.\n"
   "\n"
   "Once the key is loaded into PuTTYgen, you can perform\n"
   "this conversion simply by saving it again.\n";

   callbacks->yesno(message,mbtitle,0);
}

void logevent(void *backend, const char *string)
{
}

int console_get_line(const char *prompt, char *str,
             int maxlen, int is_pw)
{
   if (console_batch_mode) {
      if (maxlen > 0)
         str[0] = '\0';
   } else {
	   return callbacks->getpass(str,maxlen,prompt);
   }
   return 1;
}

static char ReadKey()
{
  INPUT_RECORD buffer;
  DWORD EventsRead;
  char ch;
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
  BOOL CharRead = FALSE;

  /* loop until we find a valid keystroke (a KeyDown event, and NOT a
     SHIFT, ALT, or CONTROL keypress by itself) */
  while(!CharRead)
  {
	while( !CharRead && ReadConsoleInput(hInput, &buffer, 1, &EventsRead ) &&
			EventsRead > 0 )
	{
		if( buffer.EventType == KEY_EVENT &&
			buffer.Event.KeyEvent.bKeyDown )
		{
			ch = buffer.Event.KeyEvent.uChar.AsciiChar;
			if(ch)
				CharRead = TRUE;
		}
	}
  }

  return ch;
}

int putty_getpass(char *password, int max_length, const char *prompt)
{
    int i;
	char c;
	HANDLE hInput=GetStdHandle(STD_INPUT_HANDLE);
	DWORD dwMode;

    fputs (prompt, stderr);
    fflush (stderr);
    fflush (stdout);
	FlushConsoleInputBuffer(hInput);
	GetConsoleMode(hInput,&dwMode);
	SetConsoleMode(hInput,ENABLE_PROCESSED_INPUT);
    for (i = 0; i < max_length - 1; ++i)
    {
		c=0;
		c = ReadKey();
		if(c==27 || c=='\r' || c=='\n')
			break;
		password[i]=c;
		fputc('*',stdout);
		fflush (stderr);
		fflush (stdout);
    }
	SetConsoleMode(hInput,dwMode);
	FlushConsoleInputBuffer(hInput);
    password[i] = '\0';
    fputs ("\n", stderr);
	return c==27?0:1;
}

int putty_prompt(const char *message, const char *title, int withcancel)
{
	char c;

	fflush (stderr);
	fflush (stdout);
	fflush (stdin);

	printf("%s",message);
	fflush(stdout);
	for(;;)
	{
		c=getchar();
		if(tolower(c)=='y' || c=='\n' || c=='\r')
		{
			fflush (stdin);
			return 1;
		}
		if(withcancel && (c==27 || tolower(c)=='c'))
		{
			fflush (stdin);
			return -1;
		}
		if(tolower(c)=='n' || (!withcancel && c==27))
		{
			fflush (stdin);
			return 0;
		}
	}
}

void frontend_keypress(void *handle)
{
    /*
     * This is nothing but a stub, in console code.
     */
    return;
}

void update_specials_menu(void *frontend)
{
}
