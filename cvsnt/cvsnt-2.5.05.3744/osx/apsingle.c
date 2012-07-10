/* Implements a simple AppleSingle decoder/encoder, as described in RFC1740 */
/* http://andrew2.andrew.cmu.edu/rfc/rfc1740.html */

#ifdef __MACH__

#include <Carbon/Carbon.h>

#include "apsingle.h"
#include "cvs_hqx.h"

/* stdc */
#include <stdio.h>
#include <string.h>

#define YR_2000_SECONDS 3029572800L
#define CVS_CHANGES

/* struct definitions from RFC1740 */
#if TARGET_API_MAC_CARBON
#pragma options align=mac68k
#elif PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

typedef struct ASHeader /* header portion of AppleSingle */ 
{
    /* AppleSingle = 0x00051600; AppleDouble = 0x00051607 */
       UInt32 magicNum; /* internal file type tag */ 
       UInt32 versionNum; /* format version: 2 = 0x00020000 */ 
       UInt8 filler[16]; /* filler, currently all bits 0 */ 
       UInt16 numEntries; /* number of entries which follow */
} ASHeader ; /* ASHeader */ 

typedef struct ASEntry /* one AppleSingle entry descriptor */ 
{
	UInt32 entryID; /* entry type: see list, 0 invalid */
	UInt32 entryOffset; /* offset, in octets, from beginning */
	                          /* of file to this entry's data */
	UInt32 entryLength; /* length of data in octets */
} ASEntry; /* ASEntry */ 

typedef struct ASFinderInfo
{
	FInfo ioFlFndrInfo; /* PBGetFileInfo() or PBGetCatInfo() */
	FXInfo ioFlXFndrInfo; /* PBGetCatInfo() (HFS only) */
} ASFinderInfo; /* ASFinderInfo */

typedef struct ASMacInfo        /* entry ID 10, Macintosh file information */
{
       UInt8 filler[3]; /* filler, currently all bits 0 */ 
       UInt8 ioFlAttrib; /* PBGetFileInfo() or PBGetCatInfo() */
} ASMacInfo;

typedef struct ASFileDates      /* entry ID 8, file dates info */
{
	SInt32 create; /* file creation date/time */
	SInt32 modify; /* last modification date/time */
	SInt32 backup; /* last backup date/time */
	SInt32 access; /* last access date/time */
} ASFileDates; /* ASFileDates */


#if TARGET_API_MAC_CARBON
#pragma options align=reset
#elif PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

/* Prototypes */
OSErr decodeFileDates( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec );
OSErr decodeRealName( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec );
OSErr encodeRealName(FILE * outfp, const FSRef * inSpec );
OSErr encodeComment(FILE * outfp, Str255 comment );
OSErr encodeFileDates(FILE * outfp, const FSCatalogInfo * info);
OSErr encodeFinderInfo( FILE * outfp, const FSCatalogInfo * cinfo);
OSErr encodeMacInfo( FILE * outfp, const FSCatalogInfo * cinfo);
OSErr macFileToFileStream( FILE * outfp, SInt16 refNum, UInt32 bytesExpected); 
OSErr encodeDataFork( FILE * outfp, const FSRef * inFile, UInt32 bytesExpected);
OSErr encodeResourceFork( FILE * outfp, const FSRef * inFile, UInt32 bytesExpected);

/* asEntryToMacFile
 * Blasts the bytes specified in the entry to already opened Mac file
 */
static OSErr
asEntryToMacFile( ASEntry inEntry, FILE * inFile, SInt16 inRefNum)
{
#define BUFFER_SIZE 8192

	char buffer[BUFFER_SIZE];
	size_t totalRead = 0, bytesRead;
	ByteCount bytesToWrite;
	OSErr err;

	if ( fseek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	while ( totalRead < inEntry.entryLength )
	{
// Should we yield in here?
		bytesRead = fread( buffer, 1, BUFFER_SIZE, inFile );
		if ( bytesRead <= 0 )
			return ioErr;
		bytesToWrite = totalRead + bytesRead > inEntry.entryLength ? 
									inEntry.entryLength - totalRead :
									bytesRead;

		totalRead += bytesRead;
		err = FSWriteFork(inRefNum, fsAtMark, 0, bytesToWrite, buffer, &bytesToWrite);
		if (err != noErr)
			return err;
	}
	return 0;	
}

static OSErr
decodeDataFork( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	SInt16	refNum;
	OSErr err;
	HFSUniStr255 forkName;
	
	/* Setup the files */
	err = FSGetDataForkName(&forkName);
	if (err != noErr)
		return err;

	err = FSCreateFork(ioSpec, forkName.length, forkName.unicode);
	if ( err != noErr )
		return err;
	
	err = FSOpenFork(ioSpec, forkName.length, forkName.unicode, fsWrPerm,
					   &refNum);

	if ( err == noErr )
		err = asEntryToMacFile( inEntry, inFile, refNum );
	
	FSCloseFork( refNum );
	return err;
}

static OSErr
decodeResourceFork( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	SInt16	refNum;
	OSErr err;
	HFSUniStr255 forkName;
	
	err = FSGetResourceForkName(&forkName);
	if (err != noErr)
		return err;

	err = FSCreateFork(ioSpec, forkName.length, forkName.unicode);
	if ( err != noErr )
		return err;
	
	err = FSOpenFork(ioSpec, forkName.length, forkName.unicode, fsWrPerm,
					   &refNum);

	if ( err == noErr )
		err = asEntryToMacFile( inEntry, inFile, refNum );
	
	FSCloseFork( refNum );
	return err;
}

static OSErr
decodeComment( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	Str255 newComment;
	if ( inEntry.entryLength > 32 )	/* Max file name length for the Mac */
		return -1;
	
	if ( fseek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( fread( &newComment[1], 1, inEntry.entryLength, inFile ) != inEntry.entryLength )
		return -1;
	newComment[0] = inEntry.entryLength;

#if 0 // TODO
	return FSpDTSetComment(ioSpec, newComment);
#else
	return noErr;
#endif
}

static OSErr
decodeFinderInfo( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	ASFinderInfo info;
	OSErr err;
	FSCatalogInfo catalogInfo;

	if (inEntry.entryLength != sizeof( ASFinderInfo ))
		return -1;

	if ( fseek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( fread( &info, 1, sizeof(info), inFile) != inEntry.entryLength )
		return -1;
	
	memcpy(&catalogInfo.finderInfo, &info.ioFlFndrInfo, sizeof(info.ioFlFndrInfo));
	memcpy(&catalogInfo.extFinderInfo, &info.ioFlXFndrInfo, sizeof(info.ioFlXFndrInfo));

	err = FSSetCatalogInfo(ioSpec,
						   kFSCatInfoFinderInfo |
						   kFSCatInfoFinderXInfo, &catalogInfo);

	if ( err != noErr )
		return -1;

	return err;
}

#if 0 // TODO
static OSErr
decodeMacInfo( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	ASMacInfo info;
	OSErr err;
	Str31 name;
	CInfoPBRec pb;

	if (inEntry.entryLength != sizeof( ASMacInfo ))
		return -1;

	if ( fseek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( fread( &info, 1, sizeof(info), inFile) != inEntry.entryLength )
		return -1;
	
	memcpy(name, ioSpec->name, ioSpec->name[0] + 1);
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBGetCatInfoSync(&pb);
	if ( err != noErr )
		return -1;
	
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFlAttrib = info.ioFlAttrib;
	err = PBSetCatInfoSync(&pb);
	return err;
}
#endif

#if 0 // TODO
OSErr 
decodeFileDates( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	ASFileDates dates;
	OSErr err;
	FSCatalogInfo catalogInfo;

	if ( inEntry.entryLength != sizeof(dates) )	/* Max file name length for the Mac */
		return -1;

	if ( fseek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( fread( &dates, 1, inEntry.entryLength, inFile ) != inEntry.entryLength )
		return -1;

	catalogInfo.createDate = dates.create + YR_2000_SECONDS;
	catalogInfo.contentModDate = dates.modify + YR_2000_SECONDS;
	catalogInfo.backupDate = dates.backup + YR_2000_SECONDS;

	err = FSSetCatalogInfo(ioSpec,
						   kFSCatInfoCreateDate |
						   kFSCatInfoContentMod |
						   kFSCatInfoBackupDate, &catalogInfo);
	
	return err;
}
#endif

#if 0
OSErr 
decodeRealName( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	Str255 newName;
	OSErr err;

	if ( inEntry.entryLength > 32 )	/* Max file name length for the Mac */
		return -1;
	
	if ( fseek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( fread( &newName[1], 1, inEntry.entryLength, inFile ) != inEntry.entryLength )
		return -1;

	newName[0] = inEntry.entryLength;
	err =  FSpRename(ioSpec, newName);

	if (err == noErr)
		memcpy( ioSpec->name, newName, 32 );
	return err;

}
#endif

static OSErr 
processASEntry( ASEntry inEntry, FILE * inFile, const FSRef * ioSpec )
{
	switch (inEntry.entryID)
	{
		case AS_DATA:
			return decodeDataFork( inEntry, inFile, ioSpec );
			break;
		case AS_RESOURCE:
			return decodeResourceFork( inEntry, inFile, ioSpec );
			break;
		case AS_REALNAME:
//			return decodeRealName( inEntry, inFile, ioSpec );
			break;
		case AS_COMMENT:
			return decodeComment( inEntry, inFile, ioSpec );
			break;
		case AS_ICONBW:
//			return decodeIconBW( inEntry, inFile, ioSpec );
			break;
		case AS_ICONCOLOR:
//			return decodeIconColor( inEntry, inFile, ioSpec );
			break;
		case AS_FILEDATES:
//			return decodeFileDates( inEntry, inFile, ioSpec );
			break;
		case AS_FINDERINFO:
			return decodeFinderInfo( inEntry, inFile, ioSpec );
			break;
		case AS_MACINFO:
#if 0 // TODO
			return decodeMacInfo( inEntry, inFile, ioSpec );
#endif
			break;
		case AS_PRODOSINFO:
		case AS_MSDOSINFO:
		case AS_AFPNAME:
		case AS_AFPINFO:
		case AS_AFPDIRID:
		default:
			return 0;
	}
	return 0;
}

/* Decodes 
 * Arguments:
 * inFile - name of the AppleSingle file
 * outSpec - destination. If destination is renamed (as part of decoding of realName)
 * 			the outSpec is modified to represent the new name
 */
OSErr
decodeAppleSingle(const char * inFile, const char * outFile, long wantedEntries)
{
	FILE * in;
	size_t	bytesRead;
	ASHeader header;
	OSErr err;
	int i;
	FSRef outSpec, parentSpec;
	char path[1024];
	UniChar ustr[256];
	SInt32 uniLen = sizeof(ustr) / sizeof(UniChar);
	
	in = fopen( inFile, "rb");
	if ( in == NULL )
		return fnfErr;
	/* Read in the header */
	{
		bytesRead = fread(&header, 1, sizeof(ASHeader),  in );
		if ( bytesRead != sizeof(ASHeader))
			goto fail;
		if ( header.magicNum != AS_MAGIC_NUM )
			goto fail;
		if ( header.versionNum != 0x00020000 )
			goto fail;
		if ( header.numEntries == 0 )	/* nothing in this file ? */
			goto fail;
	}
	/* Create the output file */
	getcwd(path, sizeof(path));

	err = PathToFSRef(path, &parentSpec);
	if(err != noErr)
		goto fail;

	strcat(path, "/");
	strcat(path, outFile);

	err = PathToFSRef(path, &outSpec);
	if (err != noErr && err != fnfErr) goto fail;

	if(err == noErr)
	{
		err = FSDeleteObject(&outSpec); /* Preventive delete, not sure if we need it */
		if(err != noErr)
			goto fail;
	}

	/* Setup the files */
	err = StrToUnicode(outFile, ustr, &uniLen, 0L);
	if (err != noErr) goto fail;

	err = FSCreateFileUnicode(&parentSpec, uniLen, ustr, kFSCatInfoNone,
							  0L, &outSpec, 0L);
	if (err != noErr) goto fail;

	/* Loop through the entries, processing each */
	/* Set the time/date stamps last, because otherwise they'll be destroyed
	   when we write */
	{
		Boolean hasDateEntry = false;
		ASEntry dateEntry;
		for ( i=0; i < header.numEntries; i++ )
		{
			ASEntry entry;
			size_t offset = sizeof( ASHeader ) + sizeof( ASEntry ) * i;
			if ( fseek( in, offset, SEEK_SET ) != 0 )
				goto fail;
			if ( fread( &entry, 1, sizeof( entry ), in ) != sizeof( entry ))
				goto fail;
			if ( wantedEntries & ( ((UInt32)1) << (entry.entryID - 1 )))
				switch (entry.entryID)
				{
					case AS_DATA:
						err = decodeDataFork( entry, in, &outSpec );
						break;
					case AS_RESOURCE:
						err = decodeResourceFork( entry, in, &outSpec );
						break;
					case AS_REALNAME:
						//err = decodeRealName( entry, in, &outSpec );
						break;
					case AS_COMMENT:
						err = decodeComment( entry, in, &outSpec );
						break;
					case AS_FILEDATES:
	 				/* Save it for postprocessing */
		 				hasDateEntry = true;
		 				dateEntry = entry;
						break;
					case AS_FINDERINFO:
						err = decodeFinderInfo( entry, in, &outSpec );
						break;
					case AS_MACINFO:
#if 0 // TODO
						err = decodeMacInfo( entry, in, &outSpec );
#endif
						break;
					case AS_ICONBW:
					case AS_ICONCOLOR:
						fprintf(stderr, "Can't decode AS_ICONBW...");
						break;
					case AS_PRODOSINFO:
					case AS_MSDOSINFO:
					case AS_AFPNAME:
					case AS_AFPINFO:
					case AS_AFPDIRID:
					default:
						break;
				}
			if ( err != 0)
				break;
		}
		if ( hasDateEntry )
			err = processASEntry( dateEntry, in, &outSpec );
	}
	fclose(in);
	in = NULL;

	if ( err == noErr )
		return err;
	// else fall through failure
fail:
	if (in)
		fclose(in);
	FSDeleteObject(&outSpec); /* Preventive delete, not sure if we need it */
	fprintf(stderr, "AppleSingle decoding has failed: %s\n", inFile);
	return ioErr;
}

OSErr
encodeRealName(FILE * outfp, const FSRef * inSpec )
{
	FSCatalogInfo catalogInfo;
	OSErr err;
	FSSpec inFSSpec;

	err = FSGetCatalogInfo(inSpec, kFSCatInfoNone, &catalogInfo, 0L, &inFSSpec, 0L);
	if(err != noErr)
		return err;

	if ( fwrite( &(inFSSpec.name[1]), 1, inFSSpec.name[0], outfp) < 0 )
		return ioErr;
	return noErr;
}

OSErr
encodeComment(FILE * outfp, Str255 comment )
{
	if ( fwrite( &(comment[1]), 1, comment[0], outfp) < 0 )
		return ioErr;
	return noErr;
}

#if 0
OSErr
encodeFileDates(FILE * outfp, const FSCatalogInfo * info)
{
	ASFileDates dates;
	dates.create = info->createDate - YR_2000_SECONDS;
	dates.modify = info->contentModDate - YR_2000_SECONDS;
	dates.backup = info->backupDate - YR_2000_SECONDS;
	dates.access = 0;	/* Unknown on the mac */
	if ( fwrite( &dates, 1, sizeof(dates), outfp) < 0 )
		return ioErr;
	return noErr;
}
#endif

OSErr
encodeFinderInfo( FILE * outfp, const FSCatalogInfo * cinfo)
{
	ASFinderInfo info;

	memcpy(&info.ioFlFndrInfo, &cinfo->finderInfo, sizeof(info.ioFlFndrInfo));
	memcpy(&info.ioFlXFndrInfo, &cinfo->extFinderInfo, sizeof(info.ioFlXFndrInfo));

	if ( fwrite( &info, 1, sizeof(info), outfp) < 0 )
		return ioErr;
	return noErr;
}

#if 0 // TODO
OSErr
encodeMacInfo( FILE * outfp, const FSCatalogInfo * cinfo )
{
	ASMacInfo info;
	memset( &info, 0, sizeof(info));
	info.ioFlAttrib = pb->hFileInfo.ioFlAttrib;
	if ( fwrite( &info, 1, sizeof(info), outfp) < 0 )
		return ioErr;
	return noErr;
}
#endif

OSErr
macFileToFileStream( FILE * outfp, SInt16 refNum, UInt32 bytesExpected)
{
#define BUFFER_SIZE 8192

	char buffer[BUFFER_SIZE];
	UInt32 totalRead = 0;
	ByteCount currentRead;
	OSErr err;
	while ( totalRead < bytesExpected )
	{
		currentRead = BUFFER_SIZE;
		err = FSReadFork( refNum, fsAtMark, 0, currentRead, buffer, &currentRead);
		totalRead += currentRead;
		if ( err != noErr && ( totalRead < bytesExpected))
			return err;
		if ( fwrite( buffer, 1, currentRead, outfp) < 0)
			return -1;
	}
	return noErr;
}

OSErr
encodeDataFork( FILE * outfp, const FSRef * inFile, UInt32 bytesExpected)
{
	short refNum;
	OSErr err;
	HFSUniStr255 forkName;

	err = FSGetDataForkName(&forkName);
	if (err != noErr)
		return err;

	err = FSOpenFork(inFile, forkName.length, forkName.unicode, fsRdPerm,
					 &refNum);
	if (err != noErr )
		return err;

	err = macFileToFileStream( outfp, refNum, bytesExpected );
	FSCloseFork( refNum );
	return err;
}

OSErr
encodeResourceFork( FILE * outfp, const FSRef * inFile, UInt32 bytesExpected)
{
	short refNum;
	OSErr err;
	HFSUniStr255 forkName;

	err = FSGetResourceForkName(&forkName);
	if (err != noErr)
		return err;

	err = FSOpenFork(inFile, forkName.length, forkName.unicode, fsRdPerm,
					 &refNum);
	if (err != noErr )
		return err;
	err = macFileToFileStream( outfp, refNum, bytesExpected );
	FSCloseFork( refNum );
	return err;
}

/* Encodes the file as applesingle
 *
 * These are the possible parts that can be encoded as AppleSingle:
      Data Fork              1 Data fork
      Resource Fork          2 Resource fork
      Real Name              3 File's name as created on home file system
      Comment                4 Standard Macintosh comment
      File Dates Info        8 File creation date, modification date,
                                      and so on
      Finder Info            9 Standard Macintosh Finder information
      Macintosh File Info   10 Macintosh file information, attributes  and so on
  * This routine will encode all parts that are relevant (ex no data fork encoding if data fork length is 0
  */
OSErr
encodeAppleSingle(const char * inFile, const char * outFile, long wantedEntries)
{
	OSErr err;
	Boolean needDataFork, needResourceFork, needRealName, needComment, needFileDates, needFinderInfo, needMacInfo;
	ASHeader header;
	ASEntry entry;
	UInt16 numEntries;
	FILE * outfp;
	Str255 comment;
	size_t availableOffset;
	char path[1024];
	FSRef inspec;
	FSSpec inFSSpec;
	FSCatalogInfo catalogInfo;

	needDataFork = needResourceFork = needRealName = needComment
	= needFileDates = needFinderInfo = needMacInfo = false;
	
	/* Figure out which parts of will we need to encode */
	
	getcwd(path, sizeof(path));
	strcat(path, "/");
	strcat(path, inFile);
	
	err = PathToFSRef(path, &inspec);
	if(err != noErr)
		goto fail;

	err = FSGetCatalogInfo(&inspec,
						   kFSCatInfoFinderInfo |
						   kFSCatInfoFinderXInfo |
						   kFSCatInfoCreateDate |
						   kFSCatInfoContentMod |
						   kFSCatInfoBackupDate |
						   kFSCatInfoDataSizes |
						   kFSCatInfoRsrcSizes, &catalogInfo, 0L, &inFSSpec, 0L);
	if(err != noErr)
		goto fail;

#if 0	// TODO
	if ( FSpDTGetComment(&inFSSpec,comment) != noErr)
#endif
		comment[0] = 0;

	needDataFork = (catalogInfo.dataLogicalSize > 0) 
					&& ( AS_DATA_BIT & wantedEntries) ; /* Data fork? */
	needResourceFork = (catalogInfo.rsrcLogicalSize > 0) 
					&& ( AS_RESOURCE_BIT & wantedEntries ); /* Resource fork? */
	needComment = comment[0] != 0
					&& ( AS_COMMENT_BIT & wantedEntries );
	needFinderInfo = ( AS_FINDERINFO_BIT & wantedEntries) != 0;
	needRealName = (AS_REALNAME_BIT & wantedEntries) != 0;
#if 0 // TODO
	needMacInfo = ( AS_MACINFO_BIT & wantedEntries) != 0;
	needFileDates = ( AS_FILEDATES_BIT & wantedEntries) != 0;
#endif

#ifdef CVS_CHANGES
	needRealName = false;
	needFileDates = false;
#endif

	/* The header */
	memset(&header, 0, sizeof(ASHeader));	/* for the filler bits */
	header.magicNum = AS_MAGIC_NUM;
	header.versionNum = 0x00020000;
	numEntries = 0;
	if ( needDataFork ) numEntries++;
	if ( needResourceFork ) numEntries++;
	if ( needRealName ) numEntries++;
	if ( needComment ) numEntries++;
	if ( needFileDates ) numEntries++;
	if ( needFinderInfo ) numEntries++;
	if ( needMacInfo ) numEntries++;
	header.numEntries = numEntries;
	
	outfp = fopen(outFile, "wb");
	if ( outfp == NULL)
		goto fail;
	
	/* write header */
	if ( fwrite( &header, 1, sizeof(ASHeader), outfp) < 0 )
		goto fail;

	/* write out the entry headers */
	availableOffset = sizeof(ASHeader) + numEntries * sizeof(ASEntry);

	if ( needRealName )
	{
		entry.entryID = AS_REALNAME;
		entry.entryOffset = availableOffset;
		entry.entryLength = inFSSpec.name[0];
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}
	if ( needComment )
	{
		entry.entryID = AS_COMMENT;
		entry.entryOffset = availableOffset;
		entry.entryLength = comment[0];
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}
	if ( needFileDates )
	{
		entry.entryID = AS_FILEDATES;
		entry.entryOffset = availableOffset;
		entry.entryLength = sizeof (ASFileDates );
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}
	if ( needFinderInfo )
	{
		entry.entryID = AS_FINDERINFO;
		entry.entryOffset = availableOffset;
		entry.entryLength = sizeof (ASFinderInfo );
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}
	if ( needMacInfo )
	{
		entry.entryID = AS_MACINFO;
		entry.entryOffset = availableOffset;
		entry.entryLength = sizeof (ASMacInfo );
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}
	if ( needDataFork )
	{
		entry.entryID = AS_DATA;
		entry.entryOffset = availableOffset;
		entry.entryLength = catalogInfo.dataLogicalSize;
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}
	if ( needResourceFork )
	{
		entry.entryID = AS_RESOURCE;
		entry.entryOffset = availableOffset;
		entry.entryLength = catalogInfo.rsrcLogicalSize;
		if ( fwrite( &entry, 1, sizeof(ASEntry), outfp) < 0 )
			goto fail;
		availableOffset += entry.entryLength;
	}

	/* write out the entry data */
	if ( needRealName )
		if ( encodeRealName(outfp, &inspec) != noErr )
			goto fail;
	if ( needComment )
		if ( encodeComment(outfp, comment) != noErr )
			goto fail;
#if 0 // TODO
	if ( needFileDates )
		if ( encodeFileDates(outfp, &catalogInfo) != noErr )
			goto fail;
#endif
	if ( needFinderInfo )
		if ( encodeFinderInfo(outfp, &catalogInfo) != noErr )
			goto fail;
#if 0 // TODO
	if ( needMacInfo )
		if ( encodeMacInfo( outfp, &cbrec ) != noErr )
			goto fail;
#endif
	if ( needDataFork )
		if ( encodeDataFork( outfp, &inspec, catalogInfo.dataLogicalSize) != noErr )
			goto fail;
	if ( needResourceFork )
		if ( encodeResourceFork( outfp, &inspec, catalogInfo.rsrcLogicalSize) != noErr )
			goto fail;

	fclose(outfp);
	outfp = NULL;	
	/* All done! */
	return noErr;

fail:
	if ( outfp )
		fclose(outfp);
	remove( outFile);
	if (err == noErr)
		err = ioErr;
	fprintf(stderr, "Unexpected AppleSingle encoding error %s\n", outFile );
	return err;
}

#endif /* __MACH__ */
