
#ifndef __HQX__
#define __HQX__

/* file hqx.h 
	BinHex decoder/encoder routines, definitions.
	Copyright (c) 1995, 1996, 1997 by John Montbriand.  All Rights Reserved.
	Permission hereby granted for public use.
    	Distribute freely in areas where the laws of copyright apply.
	USE AT YOUR OWN RISK.
	DO NOT DISTRIBUTE MODIFIED COPIES.
	Comments/questions/postcards* to the author at the address:
	  John Montbriand
	  P.O. Box. 1133
	  Saskatoon Saskatchewan Canada
	  S7K 3N2
	or by email at:
	  tinyjohn@sk.sympatico.ca
	*if you mail a postcard, then I will provide you with technical support
	regarding questions you may have about this file.
*/

/* routines for encoding or decoding binhex files.  These routines are designed
for use with any data source/sink.  They are re-entrant so they can be used
in several threads simultaneously. System 6 on up...  Can handle any file
size.  Uses a chained read/write algorithm so memory requirements are low.  */

#include <Carbon/Carbon.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BINHEX ENCODER -- encode a macintosh file in binhex format */
/*    output is sent to a user-defined output sink. */

/* HQXSink defines a function that will receive data produced by
	the HQXEncode routine.  refcon is available for your application's
	use and is the same refcon value passed to HQXEncode.  returning
	any error code other than noErr will abort HQXEncode.  HQXEncode
	implements a buffering scheme of its own, so there is no need for
	your sink function to be concerned about buffering data. */
typedef OSErr (*HQXSink)(void* buffer, long count, long refcon);

/* HQXEncode reads the indicated macintosh file and outputs the binhex
	equivalent to the sink.  If the HQXSink returns a non-zero result code,
	then execution stops and HQXEncode will return the result.  refcon
	is a value passed to the sink function.  */
OSErr HQXEncode(const char *filename, HQXSink dst, long refcon);



/* BINHEX DECODER -- decode binhex input extracted from a user */
/* defined input source.   Only Binhex characters delimited by colons */
/* are converted and multiple binhex data sections are allowed. */
/* For example,  the data: */
/*       :"@KaH#jS!:     ...some garbage characters...  :&4&@&40: */
/* is processed the same as the binhex sequence:*/
/*       :"@KaH#jS!&4&@&40:	*/


/* HQXSource defines a function providing a data source for the HQXDecode
	routine.  Your function should attempt to read *count bytes to the
	buffer.  Before returning, your function should set *count to the
	actual number of bytes read into the buffer.  If count is zero, or the
	source function returns some result other than eofErr, HQXDecode
	aborts processing.  HQXDecode implements its own buffering scheme
	so your source function need not be concerned about buffering.  */
typedef OSErr (*HQXSource)(void* buffer, long *count, long refcon);

/* HQXDecode decodes a binhex file using data from the source function.
	fname, if not NULL, is called once the file name has been extracted from
	the binhex data so you can change where the file is saved.  if can_replace
	is true then decoded files will replace files with the same name.  refcon
	is an application-specific parameter passed to both the name filter function
	and the source function.  if header_search is true, the routine does a
	search for the string "(This file must be converted with BinHex 4.0)"
	before beginning decoding binhex data.  */
OSErr HQXDecode(HQXSource src, const char *name, Boolean can_replace, 
	Boolean header_search, long refcon);

#ifdef __cplusplus
}
#endif

#endif
