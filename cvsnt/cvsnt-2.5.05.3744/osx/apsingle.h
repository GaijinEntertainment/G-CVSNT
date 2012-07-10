/*
** This program is free software
*/

/*
 * Author : Aleks Totic <atotic@netscape.com> --- April 1998
 */

#ifndef APSINGLE_H
#define APSINGLE_H

#ifndef __GNUC__
#include <CarbonCore/MacTypes.h>
#include <CarbonCore/Files.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int 
convert_apsingle_file(const char *inFile, int inflags, const char *outFile, int outflags, int *bin);

/* Decodes 
 * Arguments:
 * inFile - name of the AppleSingle file
 * outSpec - destination. If destination is renamed (as part of decoding of realName)
 * 			the outSpec is modified to represent the new name
 * wantedEntries - a bitmask what entries do we want decoded. Bit positions correspond to entryIDs
 */
OSErr
decodeAppleSingle( const char * inFile, const char * outFile, long wantedEntries );

/*
 * Arguments:
 * inSpec - file to be encoded
 * outSpec - destination. If an error occurs during decoding, destination will be deleted!
 * wantedEntries - a bitmask what entries do we want encoded. Bit positions correspond to entryIDs
 */
OSErr
encodeAppleSingle( const char * inFile, const char * outFile, long wantedEntries );

#define AS_MAGIC_NUM	0x00051600	/* Magic start of the file */

#define AS_DEFAULT_CREATOR 'MPS '
#define AS_DEFAULT_TYPE '\?\?\?\?'

/* entryIDs */
#define AS_DATA         1 /* data fork */
#define AS_RESOURCE     2 /* resource fork */
#define AS_REALNAME     3 /* File's name on home file system */
#define AS_COMMENT      4 /* standard Mac comment */
#define AS_ICONBW       5 /* Mac black & white icon */
#define AS_ICONCOLOR    6 /* Mac color icon */
      /*              7       /* not used */
#define AS_FILEDATES    8 /* file dates; create, modify, etc */
#define AS_FINDERINFO   9 /* Mac Finder info & extended info */
#define AS_MACINFO      10 /* Mac file info, attributes, etc */
#define AS_PRODOSINFO   11 /* Pro-DOS file info, attrib., etc */
#define AS_MSDOSINFO    12 /* MS-DOS file info, attributes, etc */
#define AS_AFPNAME      13 /* Short name on AFP server */
#define AS_AFPINFO      14 /* AFP file info, attrib., etc */
#define AS_AFPDIRID     15 /* AFP directory ID */

/* Because it seems that 1 << AS_FINDER_INFO operation works unreliably, I'll just define these
   as constants */
#define AS_DATA_BIT         0x0001 
#define AS_RESOURCE_BIT     0x0002 
#define AS_REALNAME_BIT     0x0004 
#define AS_COMMENT_BIT      0x0008
#define AS_ICONBW_BIT       0x0010
#define AS_ICONCOLOR_BIT    0x0020

#define AS_FILEDATES_BIT    0x0080
#define AS_FINDERINFO_BIT   0x0100
#define AS_MACINFO_BIT      0x0200
#define AS_PRODOSINFO_BIT   0x0400
#define AS_MSDOSINFO_BIT    0x0800
#define AS_AFPNAME_BIT      0x1000
#define AS_AFPINFO_BIT      0x2000
#define AS_AFPDIRID_BIT     0x4000

#ifdef __cplusplus
}
#endif

#endif /* APSINGLE_H */
