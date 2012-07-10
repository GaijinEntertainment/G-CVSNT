/*	cvsnt temporary key generation
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifdef _WIN32
// Microsoft braindamage reversal.  
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#endif

//#include <config.h>
#include "../cvsapi/lib/api_system.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <netdb.h>
#endif

#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>

/* Posix defines this but Linux doesn't yet */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

/* WARNING*
 
  This app will not work unless linked against an ssl library of the exact same type.
  You can't mix VS.NET and VC6 builds here.
*/

static int mkit(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days);

CVSNT_EXPORT int main(int argc, char *argv[])
{
	BIO *bio_err;
	X509 *x509=NULL;
	EVP_PKEY *pkey=NULL;
	FILE *f;
#ifdef _WIN32
	WSADATA data;
#endif

	if(argc<2)
	{
		fprintf(stderr,"Usage: %s <filename>",argv[0]);
		return -1;
	}

#ifdef _WIN32
    if(WSAStartup (MAKEWORD (1, 1), &data))
	{
		fprintf(stderr,"WSAStartup failed");
		return -1;
	}
#endif

	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

	bio_err=BIO_new_fp(stderr, BIO_NOCLOSE);

	mkit(&x509,&pkey,1024,0,3650); /* Passing 100 years here causes buffer overrun!! */

	f = fopen(argv[1],"w");
	if(!f)
	{
		perror("Couldn't create key file");
		return -1;
	}
	PEM_write_PrivateKey(f,pkey,NULL,NULL,0,NULL, NULL);
	PEM_write_X509(f,x509);
	fclose(f);

	X509_free(x509); 
	EVP_PKEY_free(pkey);

	CRYPTO_mem_leaks(bio_err);
	BIO_free(bio_err);
	return(0);
}

static int get_local_host_name(char *buffer, int len)
{
	char workbuf[HOST_NAME_MAX];
	struct hostent *hp;

	if (buffer == NULL)  /* Check for dumb user */
	  return -2;

	if (gethostname(workbuf, sizeof(workbuf)))
	   return -1;  /* Return an error if not success */

    hp = gethostbyname(workbuf);
	if (hp == 0)
	  return -1;

	/* Get the FQDN in workbuf */
	strncpy(buffer, hp->h_name, len);
	return 0;
}

int mkit(X509 **x509p,EVP_PKEY **pkeyp, int bits, int serial, int days)
{
	X509 *x;
	EVP_PKEY *pk;
	RSA *rsa;
	X509_NAME *name=NULL;
	X509_NAME_ENTRY *ne=NULL;
	X509_EXTENSION *ex=NULL;
	char hostname[HOST_NAME_MAX];

	if ((pkeyp == NULL) || (*pkeyp == NULL))
	{
		if ((pk=EVP_PKEY_new()) == NULL)
		{
			abort(); 
			return(0);
		}
	}
	else
		pk= *pkeyp;

	if ((x509p == NULL) || (*x509p == NULL))
	{
		if ((x=X509_new()) == NULL)
			goto err;
	}
	else
		x= *x509p;

	rsa=RSA_generate_key(bits,RSA_F4,NULL,NULL);
	if (!EVP_PKEY_assign_RSA(pk,rsa))
	{
		abort();
		goto err;
	}
	rsa=NULL;

	X509_set_version(x,3);
	ASN1_INTEGER_set(X509_get_serialNumber(x),serial);
	X509_gmtime_adj(X509_get_notBefore(x),0);
	X509_gmtime_adj(X509_get_notAfter(x),(long)60*60*24*days);
	X509_set_pubkey(x,pk);

	name=X509_get_subject_name(x);

	/* This function creates and adds the entry, working out the
		* correct string type and performing checks on its length.
		* Normally we'd check the return value for errors...
		*/
	X509_NAME_add_entry_by_txt(name,"C",
				MBSTRING_ASC, "UK", -1, -1, 0);

	get_local_host_name(hostname, sizeof(hostname));
	X509_NAME_add_entry_by_txt(name,"CN",
				MBSTRING_ASC, hostname, -1, -1, 0);

	X509_set_issuer_name(x,name);

	if (!X509_sign(x,pk,EVP_md5()))
		goto err;

	*x509p=x;
	*pkeyp=pk;
	return(1);
	err:
	return(0);
}
