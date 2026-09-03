/*
	CVSNT Generic API - file content classification
    Copyright (C) 2026 Gaijin Entertainment

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
#include <config.h>
#include "lib/api_system.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zstd.h>

#include "cvs_string.h"
#include "FileAccess.h"

/* The classification window.  The first 8 KB settles the common cases: a NUL
   in it means binary, and an all-normal 8 KB means text.  Anything else - a
   high byte (an em dash, an accented letter), an unusual control - means 8 KB
   is not enough to trust, so we read up to 64 KB more looking for a NUL.  A
   text encoding is much stricter than "any byte but NUL", so a single odd byte
   is enough to keep reading; only the absence of a NUL across the whole window
   concludes text. */
static const size_t FC_FIRST = 8 * 1024;
static const size_t FC_MAX   = FC_FIRST + 64 * 1024;

static inline bool fc_normal_text (unsigned char c)
{
	return (c >= 0x20 && c <= 0x7e) || c == 0x09 || c == 0x0a || c == 0x0c || c == 0x0d;
}

/* Read FILE's classification window into BUF (capacity FC_MAX), report the
   bytes read in *outn, and return whether the content is binary.  UTF-16/32
   text opens with a BOM and is exempt (cvsnt carries it with an encoding
   kflag).  An unreadable file reports as text so the real open error surfaces
   at the caller. */
static bool fc_scan (const char *file, unsigned char *buf, size_t *outn)
{
	*outn = 0;
	FILE *fp = fopen (file, "rb");
	if (!fp)
		return false;

	size_t n = fread (buf, 1, FC_FIRST, fp);
	if (n >= 2 && ((buf[0] == 0xff && buf[1] == 0xfe) || (buf[0] == 0xfe && buf[1] == 0xff)))
	{
		fclose (fp);
		*outn = n;
		return false;
	}
	if (n >= 4 && buf[0] == 0 && buf[1] == 0 && buf[2] == 0xfe && buf[3] == 0xff)
	{
		fclose (fp);
		*outn = n;
		return false;
	}

	bool binary = false, unusual = false;
	for (size_t i = 0; i < n; i++)
	{
		if (buf[i] == 0) { binary = true; break; }
		if (!fc_normal_text (buf[i])) unusual = true;
	}

	/* Only extend when the first read filled the window (there may be more)
	   and something unusual but no NUL turned up. */
	if (!binary && unusual && n == FC_FIRST)
	{
		size_t more = fread (buf + n, 1, FC_MAX - n, fp);
		for (size_t i = n; i < n + more; i++)
			if (buf[i] == 0) { binary = true; break; }
		n += more;
	}

	fclose (fp);
	*outn = n;
	return binary;
}

bool CFileAccess::looks_binary (const char *file)
{
	unsigned char *buf = (unsigned char *) malloc (FC_MAX);
	if (!buf)
		return false;
	size_t n;
	bool binary = fc_scan (file, buf, &n);
	free (buf);
	return binary;
}

const char *CFileAccess::content_binary_kopt (const char *file)
{
	unsigned char *buf = (unsigned char *) malloc (FC_MAX);
	if (!buf)
		return "";
	size_t n;
	bool binary = fc_scan (file, buf, &n);
	if (!binary)
	{
		free (buf);
		return "";
	}

	/* Binary content defaults to Bz (blob, zstd-compressed); it is only B when
	   the sampled bytes will not compress - already-compressed data such as
	   jpeg, png or zip.  "Saves at least 5%" is the bar for calling it worth
	   compressing. */
	/* fc_scan only reports binary after finding a NUL, so n > 0 here. */
	const char *kopt = "B";
	size_t bound = ZSTD_compressBound (n);
	void *out = malloc (bound);
	if (out)
	{
		size_t csz = ZSTD_compress (out, bound, buf, n, 3);
		if (!ZSTD_isError (csz) && csz * 20 < n * 19)
			kopt = "Bz";
		free (out);
	}

	free (buf);
	return kopt;
}
