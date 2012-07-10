/* smbutil.c --- Main library functions.
 * Copyright (C) 2002, 2004, 2005 Simon Josefsson
 * Copyright (C) 1999-2001 Grant Edwards
 * Copyright (C) 2004 Frediano Ziglio
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifdef NTLM_UNIQUE_MODULE
#define NTLM_STATIC static
#endif
#include "global.h"
#include <assert.h>

#ifdef NTLM_UNIQUE_MODULE
# include "des.c"
# include "md4.c"
# include "smbencrypt.c"
#else
# include "des.h"
# include "md4.h"
# include "smbencrypt.h"
#endif

char versionString[] = PACKAGE_STRING;

/* Utility routines that handle NTLM auth structures. */

/*
 * Must be multiple of two
 * We use a statis buffer of 1024 bytes for message
 * At maximun we but 48 bytes (ntlm responses) and 3 unicode strings so
 * NTLM_BUFSIZE * 3 + 48 <= 1024
 */
#define NTLM_BUFSIZE 320

/*
 * all bytes in our structures are aligned so just swap bytes so 
 * we have just to swap order 
 */
#ifdef WORDS_BIGENDIAN
# define UI16LE(n) byteswap16(n)
# define UI32LE(n) byteswap32(n)
#else
# define UI16LE(n) (n)
# define UI32LE(n) (n)
#endif

/* I am not crazy about these macros -- they seem to have gotten
 * a bit complex.  A new scheme for handling string/buffer fields
 * in the structures probably needs to be designed
 */
/* The special handling for zero length items is necessary so the offset
 * does not point past end of message if the last item of the message
 * has zero length (otherwise server responds with "The parameter is
 * incorrect."). However if the item length is zero it doesn't matter
 * where exactly the offset points to, I think. We just set it to zero.
 */
#define AddBytes(ptr, header, buf, count) \
{ \
  ptr->header.len = ptr->header.maxlen = UI16LE(count); \
  if (buf && count) \
  { \
  ptr->header.offset = UI32LE((ptr->buffer - ((uint8*)ptr)) + ptr->bufIndex); \
  memcpy(ptr->buffer+ptr->bufIndex, buf, count); \
  ptr->bufIndex += count; \
  } \
  else \
  ptr->header.offset = UI32LE(0); \
}

#define AddString(ptr, header, string) \
{ \
const char *p = (string); \
size_t len = p ? strlen(p) : 0; \
AddBytes(ptr, header, p, len); \
}

#define AddUnicodeStringLen(ptr, header, string, len) \
{ \
unsigned char buf[NTLM_BUFSIZE]; \
unsigned char *b = strToUnicode(string, len, buf); \
AddBytes(ptr, header, b, len*2); \
}

#define AddUnicodeString(ptr, header, string) \
{ \
size_t len = strlen(string); \
AddUnicodeStringLen(ptr, header, string, len); \
}

#define GetUnicodeString(structPtr, header, output) \
getUnicodeString(UI32LE(structPtr->header.offset), UI16LE(structPtr->header.len), ((char*)structPtr), (structPtr->buffer - (uint8*) structPtr), sizeof(structPtr->buffer), output)
#define GetString(structPtr, header, output) \
getString(UI32LE(structPtr->header.offset), UI16LE(structPtr->header.len), ((char*)structPtr), (structPtr->buffer - (uint8*) structPtr), sizeof(structPtr->buffer), output)
#define DumpBuffer(fp, structPtr, header) \
dumpBuffer(fp, UI32LE(structPtr->header.offset), UI16LE(structPtr->header.len), ((char*)structPtr), (structPtr->buffer - (uint8*) structPtr), sizeof(structPtr->buffer))


static void
dumpRaw (FILE * fp, const unsigned char *buf, size_t len)
{
  size_t i;

  for (i = 0; i < len; ++i)
    fprintf (fp, "%02x ", buf[i]);

  fprintf (fp, "\n");
}

static inline void
dumpBuffer (FILE * fp, uint32 offset, uint32 len, char *structPtr,
	    size_t buf_start, size_t buf_len)
{
  /* prevent buffer reading overflow */
  if (offset < buf_start || offset > buf_len + buf_start
      || offset + len > buf_len + buf_start)
    len = 0;
  dumpRaw (fp, structPtr + offset, len);
}

static char *
unicodeToString (const char *p, size_t len, char *buf)
{
  size_t i;

  if (len >= NTLM_BUFSIZE)
    len = NTLM_BUFSIZE - 1;

  for (i = 0; i < len; ++i)
    {
      buf[i] = *p & 0x7f;
      p += 2;
    }

  buf[i] = '\0';
  return buf;
}

static inline char *
getUnicodeString (uint32 offset, uint32 len, char *structPtr,
		  size_t buf_start, size_t buf_len, char *output)
{
  /* prevent buffer reading overflow */
  if (offset < buf_start || offset > buf_len + buf_start
      || offset + len > buf_len + buf_start)
    len = 0;
  return unicodeToString (structPtr + offset, len / 2, output);
}

static unsigned char *
strToUnicode (const char *p, size_t l, unsigned char *buf)
{
  int i = 0;

  if (l > (NTLM_BUFSIZE / 2))
    l = (NTLM_BUFSIZE / 2);

  while (l--)
    {
      buf[i++] = *p++;
      buf[i++] = 0;
    }

  return buf;
}

static char *
toString (const char *p, size_t len, char *buf)
{
  if (len >= NTLM_BUFSIZE)
    len = NTLM_BUFSIZE - 1;

  memcpy (buf, p, len);
  buf[len] = 0;
  return buf;
}

static inline char *
getString (uint32 offset, uint32 len, char *structPtr, size_t buf_start,
	   size_t buf_len, char *output)
{
  /* prevent buffer reading overflow */
  if (offset < buf_start || offset > buf_len + buf_start
      || offset + len > buf_len + buf_start)
    len = 0;
  return toString (structPtr + offset, len, output);
}

void
dumpSmbNtlmAuthRequest (FILE * fp, tSmbNtlmAuthRequest * request)
{
  char buf1[NTLM_BUFSIZE], buf2[NTLM_BUFSIZE];
  fprintf (fp, "NTLM Request:\n"
	   "      Ident = %.8s\n"
	   "      mType = %d\n"
	   "      Flags = %08x\n"
	   "       User = %s\n"
	   "     Domain = %s\n",
	   request->ident,
	   UI32LE (request->msgType),
	   UI32LE (request->flags),
	   GetString (request, user, buf1),
	   GetString (request, domain, buf2));
}

void
dumpSmbNtlmAuthChallenge (FILE * fp, tSmbNtlmAuthChallenge * challenge)
{
  unsigned char buf[NTLM_BUFSIZE];
  fprintf (fp, "NTLM Challenge:\n"
	   "      Ident = %.8s\n"
	   "      mType = %d\n"
	   "     Domain = %s\n"
	   "      Flags = %08x\n"
	   "  Challenge = ",
	   challenge->ident,
	   UI32LE (challenge->msgType),
	   GetUnicodeString (challenge, uDomain, buf),
	   UI32LE (challenge->flags));
  dumpRaw (fp, challenge->challengeData, 8);
}

void
dumpSmbNtlmAuthResponse (FILE * fp, tSmbNtlmAuthResponse * response)
{
  unsigned char buf1[NTLM_BUFSIZE], buf2[NTLM_BUFSIZE], buf3[NTLM_BUFSIZE];
  fprintf (fp, "NTLM Response:\n"
	   "      Ident = %.8s\n"
	   "      mType = %d\n"
	   "     LmResp = ", response->ident, UI32LE (response->msgType));
  DumpBuffer (fp, response, lmResponse);
  fprintf (fp, "     NTResp = ");
  DumpBuffer (fp, response, ntResponse);
  fprintf (fp, "     Domain = %s\n"
	   "       User = %s\n"
	   "        Wks = %s\n"
	   "       sKey = ",
	   GetUnicodeString (response, uDomain, buf1),
	   GetUnicodeString (response, uUser, buf2),
	   GetUnicodeString (response, uWks, buf3));
  DumpBuffer (fp, response, sessionKey);
  fprintf (fp, "      Flags = %08x\n", UI32LE (response->flags));
}

static void
buildSmbNtlmAuthRequest_userlen (tSmbNtlmAuthRequest * request,
				 const char *user,
				 size_t user_len, const char *domain)
{
  request->bufIndex = 0;
  memcpy (request->ident, "NTLMSSP\0\0\0", 8);
  request->msgType = UI32LE (1);
  request->flags = UI32LE (0x0000b207);	/* have to figure out what these mean */
  /* FIXME this should be workstation, not username */
  AddBytes (request, user, user, user_len);
  AddString (request, domain, domain);
}

void
buildSmbNtlmAuthRequest (tSmbNtlmAuthRequest * request,
			 const char *user, const char *domain)
{
  const char *p = strchr (user, '@');
  size_t user_len = strlen (user);

  if (p)
    {
      if (!domain)
	domain = p + 1;
      user_len = p - user;
    }

  buildSmbNtlmAuthRequest_userlen (request, user, user_len, domain);
}

void
buildSmbNtlmAuthRequest_noatsplit (tSmbNtlmAuthRequest * request,
				   const char *user, const char *domain)
{
  buildSmbNtlmAuthRequest_userlen (request, user, strlen (user), domain);
}


static void
buildSmbNtlmAuthResponse_userlen (tSmbNtlmAuthChallenge * challenge,
				  tSmbNtlmAuthResponse * response,
				  const char *user, size_t user_len,
				  const char *domain, const char *password)
{
  uint8 lmRespData[24];
  uint8 ntRespData[24];

  SMBencrypt (password, challenge->challengeData, lmRespData);
  SMBNTencrypt (password, challenge->challengeData, ntRespData);

  response->bufIndex = 0;
  memcpy (response->ident, "NTLMSSP\0\0\0", 8);
  response->msgType = UI32LE (3);

  AddUnicodeString (response, uDomain, domain);
  AddUnicodeStringLen (response, uUser, user, user_len);
  /* TODO just a dummy value for workstation */
  AddUnicodeStringLen (response, uWks, user, user_len);
  AddBytes (response, lmResponse, lmRespData, 24);
  AddBytes (response, ntResponse, ntRespData, 24);
  AddString (response, sessionKey, NULL);

  response->flags = challenge->flags;
}

void
buildSmbNtlmAuthResponse (tSmbNtlmAuthChallenge * challenge,
			  tSmbNtlmAuthResponse * response,
			  const char *user, const char *password)
{
  const char *p = strchr (user, '@');
  size_t user_len = strlen (user);
  unsigned char buf[NTLM_BUFSIZE];
  const char *domain = GetUnicodeString (challenge, uDomain, buf);

  if (p)
    {
      domain = p + 1;
      user_len = p - user;
    }

  buildSmbNtlmAuthResponse_userlen (challenge, response,
				    user, user_len, domain, password);
}

void
buildSmbNtlmAuthResponse_noatsplit (tSmbNtlmAuthChallenge * challenge,
				    tSmbNtlmAuthResponse * response,
				    const char *user, const char *password)
{
  unsigned char buf[NTLM_BUFSIZE];
  const char *domain = GetUnicodeString (challenge, uDomain, buf);

  buildSmbNtlmAuthResponse_userlen (challenge, response,
				    user, strlen (user), domain, password);
}
