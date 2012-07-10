/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/*
 * Ported from FreeBSD to Linux, only minimal changes.  --marekm
 */

/*
 * Adapted from shadow-19990607 by Tudor Bosman, tudorb@jm.nu
 */

#include <config.h>
#include "api_system.h"
#include "../../ufc-crypt/crypt.h"

#include <string.h>
#include "md5.h"
#include "md5crypt.h"

static unsigned char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static char	*magic = "$1$";	/*
                                 * This string is magic for
                                 * this algorithm.  Having
                                 * it this way, we can get
                                 * get better later on
                                 */

static void to64(char *s, unsigned long v, int n)
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

static int is_md5_salt(const char *salt)
{
        return (!strncmp(salt, magic, strlen(magic)));
}

/*
 * UNIX password
 *
 * Use MD5 for what it is best at...
 */

char *md5_crypt(const char *pw, const char *salt)
{
	static char     passwd[120], *p;
	static const char *sp,*ep;
	unsigned char	final[16];
	size_t sl,i,j;
	int pl;
	struct cvs_MD5Context	ctx,ctx1;
	unsigned long l;

	/* Refine the Salt first */
	sp = salt;

	/* If it starts with the magic string, then skip that */
	if(!strncmp(sp,magic,strlen(magic)))
		sp += strlen(magic);

	/* It stops at the first '$', max 8 chars */
	for(ep=sp;*ep && *ep != '$' && ep < (sp+8);ep++)
		continue;

	/* get the length of the true salt */
	sl = ep - sp;

	cvs_MD5Init(&ctx);

	/* The password first, since that is what is most unknown */
	cvs_MD5Update(&ctx,pw,strlen(pw));

	/* Then our magic string */
	cvs_MD5Update(&ctx,magic,strlen(magic));

	/* Then the raw salt */
	cvs_MD5Update(&ctx,sp,sl);

	/* Then just as many characters of the MD5(pw,salt,pw) */
	cvs_MD5Init(&ctx1);
	cvs_MD5Update(&ctx1,pw,strlen(pw));
	cvs_MD5Update(&ctx1,sp,sl);
	cvs_MD5Update(&ctx1,pw,strlen(pw));
	cvs_MD5Final(final,&ctx1);
	for(pl = (int)strlen(pw); pl > 0; pl -= 16)
		cvs_MD5Update(&ctx,final,pl>16 ? 16 : pl);

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	/* Then something really weird... */
	for (j=0,i = strlen(pw); i ; i >>= 1)
		if(i&1)
		    cvs_MD5Update(&ctx, final+j, 1);
		else
		    cvs_MD5Update(&ctx, pw+j, 1);

	/* Now make the output string */
	strcpy(passwd,magic);
	strncat(passwd,sp,sl);
	strcat(passwd,"$");

	cvs_MD5Final(final,&ctx);

	/*
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i=0;i<1000;i++) {
		cvs_MD5Init(&ctx1);
		if(i & 1)
			cvs_MD5Update(&ctx1,pw,strlen(pw));
		else
			cvs_MD5Update(&ctx1,final,16);

		if(i % 3)
			cvs_MD5Update(&ctx1,sp,sl);

		if(i % 7)
			cvs_MD5Update(&ctx1,pw,strlen(pw));

		if(i & 1)
			cvs_MD5Update(&ctx1,final,16);
		else
			cvs_MD5Update(&ctx1,pw,strlen(pw));
		cvs_MD5Final(final,&ctx1);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12]; to64(p,l,4); p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13]; to64(p,l,4); p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14]; to64(p,l,4); p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15]; to64(p,l,4); p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[ 5]; to64(p,l,4); p += 4;
	l =                    final[11]                ; to64(p,l,2); p += 2;
	*p = '\0';

	/* Don't leave anything around in vm they could use. */
	memset(final,0,sizeof final);

	return passwd;
}

int compare_crypt(const char *text, const char *crypt_pw)
{
	const char *test_pw;

	if(is_md5_salt(crypt_pw))
		test_pw = md5_crypt(text,crypt_pw+strlen(magic));
	else
		test_pw = crypt(text,crypt_pw);

	return strcmp(test_pw,crypt_pw);
}
