/* From Google.  Original author unknown */
#include <config.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#define BUFSIZE 64

static void getbuf(FILE*, char[], int);

char *cvslib_getpass(const char *prompt)
{ 	
    struct termios otmode, ntmode;
    static char pbuf[BUFSIZE+1];

    FILE *fi = fopen("/dev/tty", "r+");
    if (fi == NULL) return NULL;
    setbuf(fi, NULL);
    tcgetattr(fileno(fi), &otmode);		/* fetch current tty mode */
    ntmode = otmode;				/* copy the struct	  */
    ntmode.c_lflag &= ~ECHO;			/* disable echo		  */
    tcsetattr(fileno(fi), 0, &ntmode);		/* set new mode		  */

    fprintf(stderr, "%s", prompt); 
    fflush(stderr);

    getbuf(fi, pbuf, BUFSIZE);
    putc('\n', stderr);
    tcsetattr(fileno(fi), 0, &otmode);		/* restore previous mode  */
    fclose(fi);
    return pbuf;
}

static void getbuf(FILE *fi, char pbuf[], int max)
{ 
  int k = 0; 
  int ch = getc(fi);
  while(ch != '\n' && ch > 0)
  {
     if (k < max)
        pbuf[k++] = ch;
     ch = getc(fi);
   }
   pbuf[k++] = '\0';
}

