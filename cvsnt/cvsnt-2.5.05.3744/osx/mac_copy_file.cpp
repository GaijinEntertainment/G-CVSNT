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

//#define DEBUG 1

/*
 * Author : Jens Miltner <jum@mac.com> --- January 2004
 */
#include "cvs.h"
#include "mac_copy_file.h"
#include "error.h"
#include <CoreServices/CoreServices.h>
#include <stddef.h>
#include <unistd.h>
#include <set>

#ifndef DEBUG
# define DEBUG 0
#endif

namespace {
#if DEBUG
	inline void _myverify_noerr(OSStatus err, const char *expr, const char* function, const char *file, int line)
	{
		if ( err != noErr ) {
			fprintf(stderr, "### %s != noErr in function %s (Error: %d)\n  (file %s; line %d)\n", expr, function, err, file, line);
			fflush(stderr);
		}
	}
	inline void _mycheck(bool b, const char *expr, const char *function, const char *file, int line)
	{
		if ( !b ) {
			fprintf(stderr, "### %s failed in function %s\n  (file %s; line %d)\n", expr, function, file, line);
			fflush(stderr);
		}
	}
	#define myverify_noerr(x) _myverify_noerr((x), #x, __FUNCTION__, __FILE__, __LINE__)
	#define mycheck_noerr(x) myverify_noerr(x)
	#define mycheck(x) _mycheck((x), #x, __FUNCTION__, __FILE__, __LINE__)
	#define myverify(x) mycheck(x)
#else
	#define mycheck_noerr(x)
	#define mycheck(x)
	#define myverify(x) do { (x); } while (0)
	#define myverify_noerr(x) myverify(x)
#endif
	
#pragma mark ----- Tunable Parameters -----
	
/* The following constants control the behavior of the copy engine. */

enum {		/* BufferSizeForVolSpeed */
	/*	kDefaultCopyBufferSize	=   2L * 1024 * 1024,*/ 		/* 2MB,   Fast but not very responsive. */
	kDefaultCopyBufferSize	= 256L * 1024,					/* 256kB, Slower but can still use machine. */
	kMaximumCopyBufferSize	=   2L * 1024 * 1024,
	kMinimumCopyBufferSize	= 1024
};


enum {		
	/* for use with PBHGetDirAccess in IsDropBox */
	kPrivilegesMask			= kioACAccessUserWriteMask | kioACAccessUserReadMask | kioACAccessUserSearchMask,
	
	/* for use with FSGetCatalogInfo and FSPermissionInfo->mode			*/
	/* from sys/stat.h...  note -- sys/stat.h definitions are in octal	*/
	/*																	*/
	/* You can use these values to adjust the users/groups permissions	*/
	/* on a file/folder with FSSetCatalogInfo and extracting the		*/
	/* kFSCatInfoPermissions field.  See code below for examples		*/
	kRWXUserAccessMask		= 0x01C0,
	kReadAccessUser			= 0x0100,
	kWriteAccessUser		= 0x0080,
	kExecuteAccessUser		= 0x0040,
	
	kRWXGroupAccessMask		= 0x0038,
	kReadAccessGroup		= 0x0020,
	kWriteAccessGroup		= 0x0010,
	kExecuteAccessGroup		= 0x0008,
	
	kRWXOtherAccessMask		= 0x0007,
	kReadAccessOther		= 0x0004,
	kWriteAccessOther		= 0x0002,
	kExecuteAccessOther		= 0x0001,
	
	kDropFolderValue		= kWriteAccessOther | kExecuteAccessOther
};

struct ForkInfo
{
	HFSUniStr255 			forkName;
	SInt64					forkSize;
	
	ForkInfo() {}
	ForkInfo(const ForkInfo& inRhs)
	{
		this->operator = (inRhs);
	}
	
	ForkInfo& operator = (const ForkInfo& inRhs)
	{
		BlockMoveData(inRhs.forkName.unicode, forkName.unicode, inRhs.forkName.length * sizeof(UniChar));
		forkName.length = inRhs.forkName.length;
		forkSize = inRhs.forkSize;
		return *this;
	}
	
	bool operator < (const ForkInfo& inRhs) const
	{
		for ( UInt16 i(0); i < forkName.length; ++i ) {
			if ( i >= inRhs.forkName.length ) return false;
			if ( forkName.unicode[i] < inRhs.forkName.unicode[i] ) return true;
			if ( forkName.unicode[i] > inRhs.forkName.unicode[i] ) return false;
		}
		return forkName.length < inRhs.forkName.length;
	}
};

static OSErr FSGetVRefNum(	const FSRef		*ref,
							FSVolumeRefNum	*vRefNum)
{
	FSCatalogInfo	catalogInfo;
	OSErr			err = ( ref != NULL && vRefNum != NULL ) ? OSErr(noErr) : OSErr(paramErr);
	
	if( err == noErr )	/* get the volume refNum from the FSRef */
		err = FSGetCatalogInfo(ref, kFSCatInfoVolume, &catalogInfo, NULL, NULL, NULL);
	if( err == noErr )
		*vRefNum = catalogInfo.volume;
	
	mycheck_noerr( err );
	
	return err;
}

static OSErr FSGetVolParms(	FSVolumeRefNum			volRefNum,
							UInt32					bufferSize,
							GetVolParmsInfoBuffer	*volParmsInfo,
							UInt32					*actualInfoSize)		/*	Can Be NULL	*/
{
	HParamBlockRec	pb;
	OSErr			err = ( volParmsInfo != NULL ) ? OSErr(noErr) : OSErr(paramErr);
	
	if( err == noErr )
	{
		pb.ioParam.ioNamePtr = NULL;
		pb.ioParam.ioVRefNum = volRefNum;
		pb.ioParam.ioBuffer = (Ptr)volParmsInfo;
		pb.ioParam.ioReqCount = (SInt32)bufferSize;
		err = PBHGetVolParmsSync(&pb);
	}
	/* return number of bytes the file system returned in volParmsInfo buffer */
	if( err == noErr && actualInfoSize != NULL)
		*actualInfoSize = (UInt32)pb.ioParam.ioActCount;
	
	mycheck_noerr( err );	
	
	return ( err );
}

const char* ForkNameAsCString(const HFSUniStr255& forkName)
{
	static char		s[256];
	CFStringRef		cfstr(CFStringCreateWithCharactersNoCopy(NULL, forkName.unicode, forkName.length, kCFAllocatorNull));
	if ( cfstr ) {
		if ( !CFStringGetCString(cfstr, s, sizeof(s), kCFStringEncodingUTF8) ) sprintf(s, "<failure 1>");
		CFRelease(cfstr);
	} else sprintf(s, "<failure 2>");
	
	return s;
}

const char* FSRef2Path(const FSRef& inRef)
{
	static char		path[PATH_MAX];
	OSStatus	err;
	err = FSRefMakePath(&inRef, (UInt8*)path, sizeof(path));
	if ( err != noErr ) sprintf(path, "<error %d while converting FSRef>", err);
	return path;
}

OSStatus CollectForks(const FSRef& inFile, std::set<ForkInfo>& outForkList)
{
	CatPositionRec  iterator;
	OSStatus		err(noErr);

#if DEBUG
	printf("CollectForks for '%s'\n", FSRef2Path(inFile));
#endif
	/* need to initialize the iterator before using it */
	iterator.initialize = 0;
	
	/* Iterate over the list of forks	*/
	while ( err == noErr ) {
		ForkInfo		forkInfo;
		err = FSIterateForks( &inFile, &iterator, &forkInfo.forkName, &forkInfo.forkSize, NULL );
		if ( err == noErr ) {
#if DEBUG
			printf("found fork: '%s', fork size: %qu\n", ForkNameAsCString(forkInfo.forkName), forkInfo.forkSize);
#endif
			outForkList.insert(forkInfo);
		}
	}
	
	return ( err == errFSNoMoreItems ? noErr : err );
}

/* Writes the fork from the source, references by srcRefNum, to the destination fork	*/
/* references by destRefNum																*/
OSStatus WriteFork(	SInt16	srcRefNum,
					SInt16	destRefNum,
					SInt64	forkSize,
					UInt32	bufferSize,
					void*	buffer,
					bool	truncateFork,
					bool	copyingToLocalVolume)
{
	UInt64			bytesRemaining;
	UInt64			bytesToReadThisTime;
	UInt64			bytesToWriteThisTime;
	OSStatus		err;
	
	
	/* Here we create space for the entire fork on the destination volume.				*/	
	/* FSAllocateFork has the right semantics on both traditional Mac OS				*/
	/* and Mac OS X.  On traditional Mac OS it will allocate space for the				*/
	/* file in one hit without any other special action.  On Mac OS X,					*/
	/* FSAllocateFork is preferable to FSSetForkSize because it prevents				*/
	/* the system from zero filling the bytes that were added to the end				*/
	/* of the fork (which would be waste because we're about to write over				*/
	/* those bytes anyway.																*/
	err = FSAllocateFork(destRefNum, kFSAllocNoRoundUpMask, fsFromStart, 0, forkSize, NULL);
	
	/* Copy the file from the source to the destination in chunks of					*/
	/* no more than params->copyBufferSize bytes.  This is fairly						*/
	/* boring code except for the bytesToReadThisTime/bytesToWriteThisTime				*/
	/* distinction.  On the last chunk, we round bytesToWriteThisTime					*/
	/* up to the next 512 byte boundary and then, after we exit the loop,				*/
	/* we set the file's EOF back to the real location (if the fork size				*/
	/* is not a multiple of 512 bytes).													*/
	/* 																					*/
	/* This technique works around a 'bug' in the traditional Mac OS File Manager,		*/
	/* where the File Manager will put the last 512-byte block of a large write into	*/
	/* the cache (even if we specifically request no caching) if that block is not		*/
	/* full. If the block goes into the cache it will eventually have to be				*/
	/* flushed, which causes sub-optimal disk performance.								*/
	/*																					*/
	/* This is only done if the destination volume is local.  For a network				*/
	/* volume, it's better to just write the last bytes directly.						*/
	/*																					*/
	/* This is extreme over-optimization given the other limits of this					*/
	/* sample, but I will hopefully get to the other limits eventually.					*/
	bytesRemaining = forkSize;
	while( err == noErr && bytesRemaining != 0 )
	{
		if( bytesRemaining > bufferSize )
		{
			bytesToReadThisTime  = 	bufferSize;
			bytesToWriteThisTime = 	bytesToReadThisTime;
		}
		else 
		{
			bytesToReadThisTime  = 	bytesRemaining;
			bytesToWriteThisTime =	( copyingToLocalVolume )		  ?
				( (bytesRemaining + 0x01FF ) & ~0x01FF ) : bytesRemaining;
		}
		
		err = FSReadFork( srcRefNum, fsAtMark + noCacheMask, 0, bytesToReadThisTime, buffer, NULL );
		if( err == noErr )
			err = FSWriteFork( destRefNum, fsAtMark + noCacheMask, 0, bytesToWriteThisTime, buffer, NULL );
		if( err == noErr )
			bytesRemaining -= bytesToReadThisTime;
	}
	
	if (err == noErr && (truncateFork || (copyingToLocalVolume && ( forkSize & 0x01FF ) != 0)) )
		err = FSSetForkSize( destRefNum, fsFromStart, forkSize );
	
	return err;
}

OSStatus CopyForks(const FSRef& inSrcFile, 
				   const FSRef& inDestFile, 
				   const std::set<ForkInfo>& inSourceForks, 
				   const std::set<ForkInfo>& inDestForks, 
				   UInt32 inBufferSize, 
				   void* inBuffer)
{
	OSStatus				err(noErr);
	std::set<ForkInfo>		forksToRemove(inDestForks);
	
	// copy all of the source file's forks first
	for ( std::set<ForkInfo>::const_iterator i(inSourceForks.begin()); i != inSourceForks.end() && err == noErr; ++i ) {
		std::set<ForkInfo>::iterator	foundDestFork(forksToRemove.find(*i));
		bool							existingFork(foundDestFork != forksToRemove.end());
		
		TRACE(1, "processing fork: '%s', size = %qu bytes (%s fork)", ForkNameAsCString(i->forkName), i->forkSize, existingFork ? "existing" : "new");
		if ( !existingFork ) {
			// fork doesn't already exist in destination file -> create it
			err = FSCreateFork(&inDestFile, i->forkName.length, i->forkName.unicode);
		}
		
		if ( err == noErr  &&  ( i->forkSize > 0  ||  foundDestFork != forksToRemove.end() ) ) {
			SInt16			srcRefNum(0), destRefNum(0);
			/* Open the destination fork	*/
			err = FSOpenFork(&inDestFile, i->forkName.length, i->forkName.unicode, fsWrPerm, &destRefNum);
			if ( err == noErr ) {
				if ( i->forkSize > 0 ) {
					/* Open the source fork			*/
					err = FSOpenFork(&inSrcFile, i->forkName.length, i->forkName.unicode, fsRdPerm, &srcRefNum);
					if( err == noErr ) {		/* Write the fork to disk		*/
						TRACE(1, "%s fork %s from source file.", existingFork ? "Overwriting" : "Copying", ForkNameAsCString(i->forkName));
						err = WriteFork( srcRefNum, destRefNum, i->forkSize, inBufferSize, inBuffer, existingFork, true );
					}
				} else {
					// new fork size is 0, but fork exists in destination file -> make sure to truncate it
					TRACE(1, "Truncating existing fork %s to 0 bytes.", ForkNameAsCString(i->forkName));
					err = FSSetForkSize(destRefNum, fsFromStart, 0);
				}
			}
			if( destRefNum	!= 0 )	/* Close the destination fork	*/
				myverify_noerr( FSCloseFork( destRefNum ) );
			if( srcRefNum	!= 0 )	/* Close the source fork		*/
				myverify_noerr( FSCloseFork( srcRefNum ) );
		}
		
		if ( foundDestFork != forksToRemove.end() ) {
			// fork already existed in destination file -> remove from list of forks to remove
			forksToRemove.erase(foundDestFork);
		}
	}
	
	// then remove all untouched forks in the destination file
	for ( std::set<ForkInfo>::const_iterator i(forksToRemove.begin()); i != forksToRemove.end() &&  err == noErr; ++i ) {
		TRACE(1, "Removing existing fork %s, since it does not exist in the source file.", ForkNameAsCString(i->forkName));
		err = FSDeleteFork(&inDestFile, i->forkName.length, i->forkName.unicode);
	}
	
	return err;
}

/* Calculate an appropriate copy buffer size based on the volumes		*/
/* rated speed.  Our target is to use a buffer that takes 0.25			*/
/* seconds to fill.  This is necessary because the volume might be		*/
/* mounted over a very slow link (like ARA), and if we do a 256 KB		*/
/* read over an ARA link we'll block the File Manager queue for			*/
/* so long that other clients (who might have innocently just			*/
/* called PBGetCatInfoSync) will block for a noticeable amount of time.	*/
/*																		*/
/* Note that volumeBytesPerSecond might be 0, in which case we assume	*/
/* some default value.													*/
static UInt32 BufferSizeForVolSpeed(UInt32 volumeBytesPerSecond)
{
	ByteCount bufferSize;
	
	if (volumeBytesPerSecond == 0)
		bufferSize = kDefaultCopyBufferSize;
	else
	{	/* We want to issue a single read that takes 0.25 of a second,	*/
		/* so devide the bytes per second by 4.							*/
		bufferSize = volumeBytesPerSecond / 4;
	}

	/* Round bufferSize down to 512 byte boundary. */
	bufferSize &= ~0x01FF;

	/* Clip to sensible limits. */
	if (bufferSize < kMinimumCopyBufferSize)
	bufferSize = kMinimumCopyBufferSize;
	else if (bufferSize > kMaximumCopyBufferSize)
	bufferSize = kMaximumCopyBufferSize;

	return bufferSize;
}

/* This routine calculates the appropriate buffer size for				*/
/* the given vRefNum.  It's a simple composition of FSGetVolParms		*/
/* BufferSizeForVolSpeed.												*/
static UInt32 BufferSizeForVol(FSVolumeRefNum vRefNum)
{
	GetVolParmsInfoBuffer	volParms;
	ByteCount				volumeBytesPerSecond = 0;
	UInt32					actualSize;
	OSErr					err;
	
	err = FSGetVolParms( vRefNum, sizeof(volParms), &volParms, &actualSize );
	if( err == noErr )
	{
		/* Version 1 of the GetVolParmsInfoBuffer included the vMAttrib		*/
		/* field, so we don't really need to test actualSize.  A noErr		*/
		/* result indicates that we have the info we need.  This is			*/
		/* just a paranoia check.											*/
		
		mycheck(actualSize >= offsetof(GetVolParmsInfoBuffer, vMVolumeGrade));
		
		/* On the other hand, vMVolumeGrade was not introduced until		*/
		/* version 2 of the GetVolParmsInfoBuffer, so we have to explicitly	*/
		/* test whether we got a useful value.								*/
		
		if( ( actualSize >= offsetof(GetVolParmsInfoBuffer, vMForeignPrivID) ) &&
			( volParms.vMVolumeGrade <= 0 ) ) 
		{
			volumeBytesPerSecond = -volParms.vMVolumeGrade;
		}
	}
	
	mycheck_noerr( err );	
	
	return BufferSizeForVolSpeed(volumeBytesPerSecond);
}

#pragma mark ----- Calculate Buffer Size -----

/* Calculates the optimal buffer size for both the source	*/
/* and destination volumes									*/
static OSErr CalculateBufferSize(	const FSRef& source,
									const FSRef& destDir,
									ByteCount& outBufferSize )
{
	FSVolumeRefNum	sourceVRefNum,
	destVRefNum;
	ByteCount		tmpBufferSize = 0;
	OSErr			err(noErr);
	
	if( err == noErr )
		err = FSGetVRefNum( &source, &sourceVRefNum );
	if( err == noErr )
		err = FSGetVRefNum( &destDir, &destVRefNum);
	if( err == noErr)
	{
		tmpBufferSize = BufferSizeForVol(sourceVRefNum);
		if (destVRefNum != sourceVRefNum)
		{
			ByteCount tmp = BufferSizeForVol(destVRefNum);
			if (tmp < tmpBufferSize)
				tmpBufferSize = tmp;
		}
	}
	
	outBufferSize = tmpBufferSize;
	
	mycheck_noerr( err );	
	
	return err;
}


#pragma mark ----- CopyFile -----

/* Create a copy of the file, copying all available forks. 
	Will overwrite existing files. Behavior is similar to 'regular' unix copy:
	/verbatim
		creat(dest);
		open(src);
		while ( !eof ) {
			read(src);
			write(dest);
		}
		close(src);
		close(dest);
	\endverbatim

	There's a reason we don't use the 'standard' FSCopyObject for Mac OS: this function has a 
	different behavior when it comes to replacing existing files, especially with regards to 
	handling situations where the target file is read-only. In that case, the behavior of FSCopyObject
	is just not close enough to the default file copying mechanism in cvs to provide the same
	experience to the user as in a standard unix distribution of cvs...
*/
OSStatus CopyFile(const FSRef& source, 
				const FSRef& destDir,
				FSRef *outFileRef,
				const HFSUniStr255* newName)
{
	OSStatus				err(noErr);
	HFSUniStr255			newFileName;
	FSCatalogInfo			sourceCatInfo;
	FSPermissionInfo		originalPermissions;
	OSType					originalFileType;
	UInt16					originalNodeFlags;
	std::set<ForkInfo>		sourceForks, destForks;		
	FSRef					dest;
	FSSpec					tmpSpec;
	bool					isSymLink;
	bool					destFileExists(false);
	void*					buffer = NULL;
	UInt32					bufferSize;

	err = CalculateBufferSize( source, destDir, bufferSize);
	if ( err == noErr ) {
		buffer = malloc(bufferSize);
		if ( buffer == NULL ) err = memFullErr;
	}
	
	if( err == noErr ) {		/* get needed info about the source file */
		if ( newName ) BlockMoveData(newName, &newFileName, sizeof(newFileName));
		err = FSGetCatalogInfo( &source, kFSCatInfoSettableInfo, &sourceCatInfo, newName ? NULL : &newFileName, NULL, NULL );
	}

	// collect information about the forks to copy and replace
	err = CollectForks(source, sourceForks);
	if ( err == noErr ) {
		if ( FSMakeFSRefUnicode(&destDir, newFileName.length, newFileName.unicode, kTextEncodingUnknown, &dest) == noErr ) {
			// destination file exists -> collect forks, so we known which one already exist
			err = CollectForks(dest, destForks);
			destFileExists = true;
		}
	}
	
	/* Clear the "inited" bit so that the Finder positions the icon for us.	*/
	((FInfo *)(sourceCatInfo.finderInfo))->fdFlags &= ~kHasBeenInited;

	// now copy all data & metadata

	/* Remember to clear the file's type, so the Finder doesn't				*/
	/* look at the file until we're done.									*/
	originalFileType = ((FInfo *) &sourceCatInfo.finderInfo)->fdType;
	((FInfo *) &sourceCatInfo.finderInfo)->fdType = kFirstMagicBusyFiletype;

	/* Remember and clear the file's locked status, so that we can			*/
	/* actually write the forks we're about to create.						*/
	originalNodeFlags = sourceCatInfo.nodeFlags;
	sourceCatInfo.nodeFlags &= ~kFSNodeLockedMask;

	/* figure out if we should get the FSSpec to the new file or not			*/
	/* If we need it for symlinks												*/
	isSymLink = ( originalFileType == 'slnk' && ((FInfo *) &sourceCatInfo.finderInfo)->fdCreator == 'rhap' );

	/* we need to have user level read/write/execute access to the file we are	*/
	/* going to create otherwise FSCreateFileUnicode will return				*/
	/* -5000 (afpAccessDenied), and the FSRef returned will be invalid, yet		*/
	/* the file is created (size 0k)... bug?									*/
	originalPermissions = *((FSPermissionInfo*)sourceCatInfo.permissions);
	((FSPermissionInfo*)sourceCatInfo.permissions)->mode |= kRWXUserAccessMask;

	if( err == noErr && !destFileExists ) /* attempt to create the new file */
		err = FSCreateFileUnicode(&destDir, newFileName.length, newFileName.unicode, kFSCatInfoSettableInfo, &sourceCatInfo, &dest, isSymLink ? &tmpSpec : NULL );
	if( err == noErr )	/* Copy the forks over to the new file						*/
		err = CopyForks(source, dest, sourceForks, destForks, bufferSize, buffer);

	/* Restore the original file type, creation and modification dates,			*/
	/* locked status and permissions.											*/
	/* This is one of the places where we need to handle drop					*/
	/* folders as a special case because this FSSetCatalogInfo will fail for	*/
	/* an item in a drop folder, so we don't even attempt it.					*/
	if ( err == noErr )
	{
		((FInfo *) &sourceCatInfo.finderInfo)->fdType = originalFileType;
		sourceCatInfo.nodeFlags  = originalNodeFlags;
		*((FSPermissionInfo*)sourceCatInfo.permissions) = originalPermissions;
		
		/* 2796751, FSSetCatalogInfo returns -36 when setting the Finder Info	*/
		/* for a symlink.  To workaround this, when the file is a				*/
		/* symlink (slnk/rhap) we will finish the copy in two steps. First		*/
		/* setting everything but the Finder Info on the file, then calling		*/
		/* FSpSetFInfo to set the Finder Info for the file. I would rather use	*/
		/* an FSRef function to set the Finder Info, but FSSetCatalogInfo is	*/
		/* the only one...  catch-22...											*/
		/*																		*/
		/* The Carbon File Manager always sets the type/creator of a symlink to	*/
		/* slnk/rhap if the file is a symlink we do the two step, if it isn't	*/
		/* we use FSSetCatalogInfo to do all the work.							*/
		if ( isSymLink )
		{								/* Its a symlink							*/
			/* set all the info, except the Finder info	*/
			err = FSSetCatalogInfo(&dest, kFSCatInfoNodeFlags | kFSCatInfoPermissions, &sourceCatInfo);
			if ( err == noErr )			/* set the Finder Info to that file			*/
				err = FSpSetFInfo( &tmpSpec, ((FInfo *) &sourceCatInfo.finderInfo) );
		}
		else {							/* its a regular file 						*/
			err = FSSetCatalogInfo(&dest, kFSCatInfoNodeFlags | kFSCatInfoFinderInfo | kFSCatInfoPermissions, &sourceCatInfo);
		}
	}

	/* If we created the file and the copy failed, try to clean up by			*/
	/* deleting the file we created.  We do this because, while it's			*/
	/* possible for the copy to fail halfway through and the File Manager 		*/
	/* doesn't really clean up that well in that case, we *really* don't want	*/
	/* any half-created files being left around.								*/
	/* if the file already existed, we don't want to delete it					*/
	if( err == noErr ) {
		/* if everything was fine, then return the new file Spec/Ref		*/
		if( outFileRef != NULL ) *outFileRef = dest;
	}
	else if ( !destFileExists ) myverify_noerr(FSDeleteObject(&dest));

	if ( buffer ) free(buffer);

	return err;
}

OSStatus _split_path(const char* path, FSRef *outParentFolder, HFSUniStr255 *outName)
{
	char			tmpPath[ PATH_MAX ],
					*tmpNamePtr;
	OSErr			err;
	
					/* Get local copy of incoming path					*/
	strcpy( tmpPath, (char*)path );

					/* Get the name of the object from the given path	*/
					/* Find the last / and change it to a '\0' so		*/
					/* tmpPath is a path to the parent directory of the	*/
					/* object and tmpNamePtr is the name				*/
	tmpNamePtr = strrchr( tmpPath, '/' );
	if ( tmpNamePtr == NULL )
		getcwd(tmpPath, sizeof(tmpPath));
	else
	{
		if( *(tmpNamePtr + 1) == '\0' )
		{				/* in case the last character in the path is a /	*/
			*tmpNamePtr = '\0';
			tmpNamePtr = strrchr( tmpPath, '/' );
		}
		*tmpNamePtr = '\0';
		tmpNamePtr++;
	}
	
					/* Get the FSRef to the parent directory			*/
	err = FSPathMakeRef( (unsigned char*)tmpPath, outParentFolder, NULL );
	if( err == noErr )
	{				/* Convert the name to an HFSUniStr255 */
		CFStringRef 	tmpStringRef = CFStringCreateWithCString( kCFAllocatorDefault, tmpNamePtr ? tmpNamePtr : path, kCFStringEncodingUTF8 );
		if( tmpStringRef != NULL )
		{
			outName->length = CFStringGetLength(tmpStringRef);
			if ( outName->length > sizeof(outName->unicode)/sizeof(UniChar) )
				err = bdNamErr;
			if ( err == noErr ) CFStringGetCharacters(tmpStringRef, CFRangeMake(0, outName->length), outName->unicode);
			CFRelease( tmpStringRef );
		}
		else err = coreFoundationUnknownErr;
	}
	
	return err;
}

struct CarbonWDFixup
{
	short		savedVRefNum;
	long		savedDirID;
	
	CarbonWDFixup()
	{
		HGetVol(NULL, &savedVRefNum, &savedDirID);
		char*	wd = getcwd(NULL, 0);
		if ( wd ) {
			FSRef	wdref;
			if ( FSPathMakeRef((const UInt8*)wd, &wdref, NULL) == noErr ) {
				FSCatalogInfo	catInfo;
				if ( FSGetCatalogInfo(&wdref, kFSCatInfoNodeID|kFSCatInfoVolume, &catInfo, NULL, NULL, NULL) == noErr ) {
					HSetVol(NULL, catInfo.volume, catInfo.nodeID);
				}
			}
			free(wd);
		}
	}
	~CarbonWDFixup()
	{
		HSetVol(NULL, savedVRefNum, savedDirID);
	}
};

} // end of anonymous namespace

#if DEBUG
const char* GetCarbonWDPath()
{
	static char		path[16384];
	short			vRefNum;
	long			dirID;
	OSStatus		err;
	
	err = HGetVol(NULL, &vRefNum, &dirID);
	if ( err == noErr ) {
		FSSpec	fspec = { vRefNum, dirID, {0} };
		FSRef	fref;
		err = FSpMakeFSRef(&fspec, &fref);
		if ( err == noErr ) {
			err = FSRefMakePath(&fref, (UInt8*)path, sizeof(path));
			if ( err ) sprintf(path, "<error %d getting path>", err);
		} else sprintf(path, "<error %d creating FSRef>", err);
	} else sprintf(path, "<error %d>", err);
	
	return path;
}
#endif

int mac_copy_file(const char* from, const char* to, int force_overwrite, int must_exist)
{
	OSStatus		err;
	FSRef			fromRef, toParentRef;
	HFSUniStr255	toName;
	//CarbonWDFixup	fixCarbonWD;
	
	TRACE(1, "mac_copy_file(%s, %s, %d, %d)", from, to, force_overwrite, must_exist);
#if DEBUG
	TRACE(1, "Carbon Working Directory = %s", GetCarbonWDPath());
	TRACE(1, "getcwd() = %s", getcwd(NULL, 0));
#endif
	
	// get source FSRef
	err = FSPathMakeRef( (unsigned char*) from, &fromRef, NULL );
	if ( err ) {
		if ( must_exist )
			error (1, 0, "cannot access %s for copying: error %d", from, (int)err);
		else 
			return -1;
	}
	// and destination FSRef (i.e. target parent folder) & name
	err = _split_path(to, &toParentRef, &toName);
	if ( err ) error (1, 0, "bad target %s for copying: error %d", to, (int)err);
	// remove file before attempting to copy if force-overwriting, since FSCopyObject may not do this properly
	if (force_overwrite && CVS_UNLINK(to) && !existence_error (errno))
		error (1, errno, "unable to remove %s", to);
	// now copy all forks
	err = CopyFile(fromRef, toParentRef, NULL, &toName);
	if ( err ) error (1, 0, "error while trying to copy file %s to %s: error %d", from, to, (int)err);
}
