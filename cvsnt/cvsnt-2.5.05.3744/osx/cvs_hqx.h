/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Author : Alexandre Parenteau <aubonbeurre@hotmail.com> --- December 1997
 */

#ifndef CVS_HQX_H
#define CVS_HQX_H

#ifdef __cplusplus
extern "C" {
#endif

// will try to encode/decode file.
// Encoding :
// 1) if the signature is not 'TEXT' :
//   1a) if there is a resource fork => encode and return 1
//   1b) if there is NO resource fork => adjust bin = 1 and return 0
//       (so the "cvs import" is correct)
// 2) if the signature is 'TEXT' => return 0
// Decoding :
// 1) if !bin => return 0 (!!! THE CVSWRAPPER has to be coherent !)
// 2) if has the HQX header => decode and return 1
// 3) if has the AppleSingle header => decode and return 1
// 4) return 0

int convert_hqx_file(const char *infile, int encode, const char *outfile, int *bin);

// set file type according to Internet Config
void set_file_type(const char *outfile, Boolean warnOnFail);

// convert a character from and to an ISO8559_xxx character set
unsigned char convert_iso(int convert_from, int iso, unsigned char c);

#if TARGET_RT_MAC_MACHO
// turn a full Unix path into a FSRef
OSStatus PathToFSRef(const char *filename, FSRef * res);

// convert to Unicode, return the encoder if outEncoder is not 0L
OSStatus StrToUnicode(const char *str, UniChar *ustr, SInt32 *uniLen,
					  TextEncoding *outEncoder);
#endif
   
void mac_convert_file(const char *infile,  int encode, char *outfile, int binary);

#ifdef __cplusplus
}
#endif

#endif /* CVS_HQX_H */
