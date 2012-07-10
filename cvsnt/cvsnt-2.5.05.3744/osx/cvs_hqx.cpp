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

#ifdef __MACH__

#include "cvs.h"
#include "system.h"
#include "error.h"

#include <Carbon/Carbon.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include "hqx.h"
#include "cvs_hqx.h"
#include "apsingle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int quiet;
extern int really_quiet;

extern int cvs_output(const char*, size_t);

OSStatus PathToFSRef(const char *filename, FSRef * res)
{
	return FSPathMakeRef((UInt8 *)filename, res, 0L);
}

OSStatus StrToUnicode(const char *str, UniChar *ustr, SInt32 *uniLen,
					  TextEncoding *outEncoder)
{
	TextEncoding encoder = kTextEncodingMacRoman;
	TextToUnicodeInfo textToUnicodeInfo;
	ByteCount oUnicodeLen;
	OSStatus status;
	Str255 pstr;

	c2pstrcpy(pstr, str);

	status = CreateTextToUnicodeInfoByEncoding(encoder, &textToUnicodeInfo);
	if(status != noErr)
		return status;

	status = ConvertFromPStringToUnicode(textToUnicodeInfo, pstr, *uniLen * sizeof(UniChar), &oUnicodeLen, ustr);
	DisposeTextToUnicodeInfo(&textToUnicodeInfo);

	if(status == noErr)
	{
		*uniLen = oUnicodeLen / sizeof(UniChar);
		if(outEncoder != 0L)
			*outEncoder = encoder;
	}

	return status;
}

struct TypeEntry {
  OSType      type;
  CFStringRef ext;
};

static void fillSemiColonTypes(const char *envVar, TypeEntry *& allTypes, int & numTypes, const char* defaults)
{
	allTypes = 0L;
	numTypes = 0;
	const char *envRes = CProtocolLibrary::GetEnvironment(envVar);
	if ( envRes == 0L )
		envRes = defaults;
	if(envRes == 0L)
		return;
	
	char *tmpStr = (char *)malloc(strlen(envRes) + 1);
	if(tmpStr == 0L)
        error (1, errno, "Running out of memory !");
	strcpy(tmpStr, envRes);
	
	char *tmp = tmpStr, *colon = 0L, *start = tmpStr;
	while((tmp = strchr(tmp, ';')) != 0L)
	{
		*tmp = '\0';
		if((tmp - start) == 4)
		{
			if(allTypes == 0L)
				allTypes = (TypeEntry *)malloc(sizeof(TypeEntry));
			else
				allTypes = (TypeEntry *)realloc(allTypes, (numTypes + 1) * sizeof(TypeEntry));
			if(allTypes == 0L)
				error (1, errno, "Running out of memory !");
			OSType atype;
			((char *)&atype)[0] = *start++;
			((char *)&atype)[1] = *start++;
			((char *)&atype)[2] = *start++;
			((char *)&atype)[3] = *start;
			allTypes[numTypes].type = atype;
			allTypes[numTypes].ext = NULL;
			numTypes++;
		}
		else if((tmp - start) > 4 && start[4] == '/')
		{
			if(allTypes == 0L)
				allTypes = (TypeEntry *)malloc(sizeof(TypeEntry));
			else
				allTypes = (TypeEntry *)realloc(allTypes, (numTypes + 1) * sizeof(TypeEntry));
			if(allTypes == 0L)
				error (1, errno, "Running out of memory !");
			OSType atype;
			((char *)&atype)[0] = *start++;
			((char *)&atype)[1] = *start++;
			((char *)&atype)[2] = *start++;
			((char *)&atype)[3] = *start++; 
			++start; // skip delimiter '/'
			allTypes[numTypes].type = atype;
			if ( *start != 0 )
			{
			  allTypes[numTypes].ext = CFStringCreateWithCString(NULL, start, kCFStringEncodingUTF8);
			  if ( allTypes[numTypes].ext == NULL )
			    error(0, coreFoundationUnknownErr, "Warning: can't create CFStringRef for extension %s (variable : %s) !", start, envRes);
			}
			else allTypes[numTypes].ext = NULL;
			numTypes++;
		}
		else
		{
			error (0, errno, "Warning unknown signature %s (variable : %s) !",
				   start, envRes);
		}
		
		start = ++tmp;
	}
}

static bool isTypeInList(OSType inFileType, 
                            CFStringRef inFileExtension, 
                            const TypeEntry* inTypeList, 
                            int inNumTypes,
                            const char* mappingsName)
{
  if ( inTypeList == NULL ) return false;
  
  // check for type _and_ extension match
	if ( inFileType != 0  &&  inFileType != '\?\?\?\?'  &&  inFileExtension != NULL )
		for(int i = 0; i < inNumTypes; i++)
		{
		  if ( inTypeList[i].ext != NULL  &&  inTypeList[i].type != '\?\?\?\?' )
			if ( inTypeList[i].type == inFileType  &&
				 CFStringCompare(inFileExtension, inTypeList[i].ext, kCFCompareCaseInsensitive|kCFCompareNonliteral) == kCFCompareEqualTo )
			  return true;
		}
  	
  // check for extension only match
	if ( inFileExtension != NULL )
		for(int i = 0; i < inNumTypes; i++)
		{
		  if ( inTypeList[i].ext != NULL  &&  inTypeList[i].type == '\?\?\?\?')
			if (CFStringCompare(inFileExtension, inTypeList[i].ext, kCFCompareCaseInsensitive|kCFCompareNonliteral) == kCFCompareEqualTo)
			  return true;
		}
  	
  // check for file type only match
	if ( inFileType != 0  &&  inFileType != '\?\?\?\?' )
		for(int i = 0; i < inNumTypes; i++)
		{
			if( inTypeList[i].type != '\?\?\?\?'  &&  inTypeList[i].ext == NULL  &&  inTypeList[i].type == inFileType )
			return true;
		}
  	
	return false;
}

static Boolean isPlainBinary(OSType ftype, CFStringRef extension)
{
	static TypeEntry *allTypes = (TypeEntry *)-1;
	static int numTypes;
	if(allTypes == (TypeEntry *)-1)
	{
		fillSemiColonTypes("MAC_BINARY_TYPES_PLAIN", allTypes, numTypes, "\?\?\?\?/gif;\?\?\?\?/tif;\?\?\?\?/tiff;\?\?\?\?/jpg;\?\?\?\?/jpeg;\?\?\?\?/png;\?\?\?\?.icns");
	}
	return isTypeInList(ftype, extension, allTypes, numTypes, "plain binary mappings");
}

static Boolean testHQXEncoding(OSType ftype, CFStringRef extension)
{
	static TypeEntry *allTypes = (TypeEntry *)-1;
	static int numTypes;
	if(allTypes == (TypeEntry *)-1)
	{
		fillSemiColonTypes("MAC_BINARY_TYPES_HQX", allTypes, numTypes, "rsrc;RSRC;\?\?\?\?/rsrc;\?\?\?\?.ppob");
	}
	return isTypeInList(ftype, extension, allTypes, numTypes, "HQX mappings");
}

static Boolean testAppleSingleEncoding(OSType ftype, CFStringRef extension)
{
	static TypeEntry *allTypes = (TypeEntry *)-1;
	static int numTypes;
	if(allTypes == (TypeEntry *)-1)
	{
		fillSemiColonTypes("MAC_BINARY_TYPES_SINGLE", allTypes, numTypes, NULL);
	}
	return isTypeInList(ftype, extension, allTypes, numTypes, "AppleSingle mappings");
}

// - Tells which encoder to use. Check the env. variables "MAC_BINARY_TYPES_HQX"
// and "MAC_BINARY_TYPES_SINGLE". If the type is not inside, return the
// default encoder "MAC_DEFAULT_RESOURCE_ENCODING".
// - Note that the plain binary test has to overide this one in every cases.

typedef enum
{
	eEncodeDefault = -1,
	eEncodeHQX,
	eEncodeAppleSingle,
	eEncodePlainBinary
} encodingPolicy;

static encodingPolicy determineEncodingPolicy(OSType ftype, CFStringRef extension)
{
	if(isPlainBinary(ftype, extension))
	{
		return eEncodePlainBinary;
	}
	else if(testHQXEncoding(ftype, extension))
	{
		return eEncodeHQX;
	}
	else if(testAppleSingleEncoding(ftype, extension))
	{
		return eEncodeAppleSingle;
	}
	else
	{
		return eEncodeDefault;
	}
}

static void getDefaultEncodingPolicy(encodingPolicy *policy)
{
	// get the default encoder
	static const char *envRes = CProtocolLibrary::GetEnvironment("MAC_DEFAULT_RESOURCE_ENCODING");
	if(envRes == 0L)
		envRes = CProtocolLibrary::GetEnvironment("CVS_DEFAULT_RESOURCE_ENCODING");
	if(envRes == 0L)
	{
		// default to plain binary, to avoid problems when working cross-platform
		*policy = eEncodePlainBinary;
		return;
	}
	
	if(strcmp(envRes, "HQX") == 0)
	{
		*policy = eEncodeHQX;
		return;
	}
	else if(strcmp(envRes, "AppleSingle") == 0)
	{
		*policy = eEncodeAppleSingle;
		return;
	}
	else if(strcmp(envRes, "PlainBinary") == 0)
	{
		*policy = eEncodePlainBinary;
		return;
	}
	
	error (1, errno, "Unknown resource encoding '%s' !\n", envRes);
}

static FILE *gDestFile;
static FILE *gSrcFile;

static OSErr MyDest(void* buffer, long count, long param) {
	fwrite(buffer, count, 1, gDestFile);
	return noErr;
}

static OSErr MySource(void* buffer, long *count, long param) {
	*count =  fread(buffer, 1, *count, gSrcFile);
	return noErr;
}

struct AutoCFStringRef
{
  CFStringRef   ref;
  AutoCFStringRef() : ref(NULL) {}
  ~AutoCFStringRef() { if ( ref ) CFRelease(ref); }
  operator CFStringRef() const { return ref; }
};

int
convert_hqx_file (const char *infile, int encode, const char *outfile, int *bin)
{
	OSErr err = fnfErr;
	encodingPolicy inFileType;
	OSType filetype = 0;
	OSType filecreator;
	FSRef inspec, outspec;
	FSCatalogInfo catalogInfo, catalogDestInfo;
	HFSUniStr255    fileName;
	AutoCFStringRef   extension;

	if (!encode)
	{
		// the cvswrapper has to be coherent !!!
		if((*bin) == 0)
			return 0;
		
		gSrcFile = fopen(infile, "r");
		if(gSrcFile == NULL)
			error(1, errno, "Cannot open %s (%s %d)", infile, __FILE__, __LINE__);
		
		char buf[100];
		static char hqxheader[] = "(This file must be converted with BinHex 4.0)";
		UInt32 asheader = AS_MAGIC_NUM;
		fread (buf, sizeof(char), 100, gSrcFile);

		// Determine whether the file is one of the known encodings
		if ( memcmp(hqxheader, buf, strlen(hqxheader) * sizeof(char)) == 0)
		{
			inFileType = eEncodeHQX;
			fseek(gSrcFile, 0, SEEK_SET);
		}
		else if  ( memcmp(&asheader, buf, sizeof(asheader)) == 0 )
		{
			fclose(gSrcFile);
			inFileType = eEncodeAppleSingle;
		}
		else
		{
			// it is a "plain" binary
			fclose(gSrcFile);
			return 0;
		}
	}
	else
	{	  
		// check if it is a text file
		OSStatus status = PathToFSRef(infile, &inspec);
		if(status != noErr)
			error(1, status, "Internal error: %s, %d", __FILE__, __LINE__);
		
		status = ::FSGetCatalogInfo(&inspec, kFSCatInfoFinderInfo | kFSCatInfoRsrcSizes,
									&catalogInfo, &fileName, 0L, 0L);
		if(status != noErr)
			error(1, status, "Internal error: %s, %d", __FILE__, __LINE__);
		
		filetype = ((FInfo *)catalogInfo.finderInfo)->fdType;
		if(filetype == 'TEXT')
			return 0;
		
		if ( *bin == 0 )
		{
	    	const char *envRes = CProtocolLibrary::GetEnvironment("MAC_ENCODE_ONLY_BINARY_FILES");
	    	if ( envRes == NULL  ||  strcmp(envRes, "0") != 0 ) {
				if ( !really_quiet &&  catalogInfo.rsrcLogicalSize != 0 )
				{
					cvs_output ("Resource fork of non-binary file '", 0);
					cvs_output (infile, 0);
					cvs_output ("' discarded.\n", 0);
				}
	      		return 0;
	      	}
		}
	}
		
	if(encode)
	{
		encodingPolicy 		policy;		
		AutoCFStringRef		fname;
		CFIndex      		extensionStart;

		// we won't use LSGetExtensionInfo to determine the extension, since this will only detect 'known' extensions,
		// but some of the extensions used for resource fork handling might not be detected by LSGetExtensionInfo. Thus,
		// we'll just search for the last '.' and use any following characters as the extension.
		fname.ref = CFStringCreateWithCharactersNoCopy(NULL, fileName.unicode, fileName.length, kCFAllocatorNull);
		if ( fname.ref ) {
			CFRange	foundRange;
			if ( CFStringFindWithOptions(fname, CFSTR("."), CFRangeMake(0, fileName.length), kCFCompareBackwards|kCFCompareNonliteral|kCFCompareCaseInsensitive, &foundRange)  &&  (foundRange.location + 1 < fileName.length) ) {
				extension.ref = CFStringCreateWithCharactersNoCopy(NULL, fileName.unicode + foundRange.location + 1, fileName.length - foundRange.location - 1, kCFAllocatorNull);
				if ( extension.ref == NULL )
					error (0, 0, "Warning: Can't determine extension of file '%s', thus resource fork handling might not be correct.\n", infile);
			}
		}
		else {
			error (0, 0, "Warning: Can't determine extension of file '%s', thus resource fork handling might not be correct.\n", infile);
		}
		
		policy = determineEncodingPolicy(filetype, extension);

		// special case, some files may be forced as plain binary.
		// (env. variable "MAC_BINARY_TYPES_PLAIN")
		if(policy == eEncodePlainBinary)
		{
			if ( !really_quiet &&  catalogInfo.rsrcLogicalSize != 0 )
			{
				cvs_output ("Resource fork of file '", 0);
				cvs_output (infile, 0);
				cvs_output ("' discarded because plain binary mapping was specified.\n", 0);
			}
			*bin = 1;
			return 0;
		}
		else if(policy == eEncodeDefault)
		{
			// Never encode any files that have no resource fork
			// Should we just use the test: if (
			if(catalogInfo.rsrcLogicalSize == 0)
			{
				// it is a "plain" binary
				*bin = 1;
				return 0;
			}

			// get default policy
			getDefaultEncodingPolicy(&policy);
			
			if(policy == eEncodePlainBinary)
			{
				if ( !really_quiet &&  catalogInfo.rsrcLogicalSize != 0 )
				{
					cvs_output ("Resource fork of file '", 0);
					cvs_output (infile, 0);
					cvs_output ("' discarded because default mapping 'plain binary' was used.\n", 0);
				}
				*bin = 1;
				return 0;
			}
		}
		
		if(policy == eEncodeHQX)
		{
			if ( !really_quiet )
			{
			  cvs_output ("Using BinHex encoding for file '", 0);
			  cvs_output (infile, 0);
			  cvs_output ("'.\n", 0);
			}
			gDestFile = fopen(outfile, "w");
			if(gDestFile == NULL)
				error(1, errno, "Internal error: %s, %d", __FILE__, __LINE__);
			err = HQXEncode(infile, MyDest, 0);
			fflush(gDestFile);
			fclose(gDestFile);
			if(err != noErr)
				error(1, err, "Internal error: %s, %d", __FILE__, __LINE__);
		}
		else if ( policy == eEncodeAppleSingle )
		{
			if ( !really_quiet )
			{
			  cvs_output ("Using AppleSingle encoding for file '", 0);
			  cvs_output (infile, 0);
			  cvs_output ("'.\n", 0);
			}
#define APPLESINGLE_WANTED_ENTRIES ((UInt32)AS_DATA_BIT | AS_RESOURCE_BIT | AS_COMMENT_BIT | AS_FINDERINFO_BIT | AS_MACINFO_BIT)
			UInt32 wantedEntries = APPLESINGLE_WANTED_ENTRIES;
			err = encodeAppleSingle(infile, outfile, wantedEntries);
			if (err != noErr)
				error(1, err, "Internal error: %s, %d", __FILE__, __LINE__);
		}
		else
		{
			error(1, errno, "Internal error: %s, %d", __FILE__, __LINE__);
		}
	}
	else	// decoding
	{
		if ( inFileType == eEncodeHQX )
		{
			err = HQXDecode(MySource, outfile, true, true, 0);
			
			fclose(gSrcFile);
			if(err != noErr)
				error(1, err, "Internal error: %s, %d", __FILE__, __LINE__);
		}
		else if ( inFileType == eEncodeAppleSingle)
		{
			UInt32 wantedEntries = APPLESINGLE_WANTED_ENTRIES;
			err = decodeAppleSingle(infile, outfile, wantedEntries);

			if (err != noErr)
				error(1, err, "Internal error: %s, %d", __FILE__, __LINE__);

			// Sometimes, type/creator information is not encoded inside the AS file 
			// If so, we want to set type/creator using Internet Config
			OSStatus status = PathToFSRef(outfile, &outspec);
			if(status != noErr)
				error(1, status, "Internal error: %s, %d", __FILE__, __LINE__);

			status = ::FSGetCatalogInfo(&outspec, kFSCatInfoFinderInfo,
										&catalogDestInfo, 0L, 0L, 0L);

			if(status != noErr)
				error(1, status, "Internal error: %s, %d", __FILE__, __LINE__);
		
			filetype = ((FInfo *)catalogDestInfo.finderInfo)->fdType;
			filecreator = ((FInfo *)catalogDestInfo.finderInfo)->fdCreator;
			if(filetype == AS_DEFAULT_TYPE && filecreator == AS_DEFAULT_CREATOR)
			{
				set_file_type(outfile, true);
			}
 		}
		else
		{
			error(1, errno, "Internal error: %s, %d", __FILE__, __LINE__);
		}
	}

	return 1;
}

static OSStatus CachedICStart (ICInstance* outInstance)
{
	static ICInstance inst = 0;
	static OSErr theErr = noErr;
	if(inst == 0 && theErr == noErr) 
	  theErr = ICStart(&inst, 'CVS ');  // Use your creator code if you have one!
	if (theErr == noErr) 
	  *outInstance = inst;		
	
	return theErr;
}

void set_file_type(const char *outfile, Boolean warnOnFail)
{
	OSStatus err;
	ICInstance inst;
	Str255 filename;

	c2pstrcpy(filename, outfile);
	
	err = CachedICStart(&inst);			// Use your creator code if you have one!
	if (err != noErr)
	{
		static Boolean firsttime = true;
		if(firsttime)
		{
			error(0, err, "WARNING Internet config's missing !");
			firsttime = false;
		}
		return;
	}

	ICMapEntry entry;
	err = ICMapFilename(inst, filename, &entry);

	if(err == icPrefNotFoundErr)
	{
#if 0
		// TODO: to restore. create a new protocol getenv
		entry.fileCreator = FOUR_CHAR_CODE('CWIE');
		entry.fileType = FOUR_CHAR_CODE('TEXT');

		err = noErr;
#endif
	}

	if (err != noErr)
	{
		if (warnOnFail)
			error(0, err, "WARNING: Internet Config cannot map file %s", outfile);
		return;
	}

	FInfo *fndrInfo;
	FSRef outspec;
	OSStatus status;

	err = PathToFSRef(outfile, &outspec);
	if(err != noErr)
	{
		error(0, err, "Internal error: %s, %d", __FILE__, __LINE__);
		return;
	}

	FSCatalogInfo catalogInfo;
	status = ::FSGetCatalogInfo(&outspec, kFSCatInfoFinderInfo,
								&catalogInfo, 0L, 0L, 0L);

	if(status != noErr)
	{
		error(0, status, "Internal error: %s, %d", __FILE__, __LINE__);
		return;
	}

	fndrInfo = (FInfo *)catalogInfo.finderInfo;
	
	if(entry.fileCreator != 0)
		fndrInfo->fdCreator = entry.fileCreator;
	if(entry.fileType != 0)
		fndrInfo->fdType = entry.fileType;

	status = ::FSSetCatalogInfo(&outspec, kFSCatInfoFinderInfo,
								&catalogInfo);

	if(status != noErr)
	{
		error(0, status, "Internal error: %s, %d", __FILE__, __LINE__);
	}

}

unsigned char const mac_to_iso_maccvspro[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,	/*   0 -   7  */
      8,   9,  10,  11,  12,  13,  14,  15,	/*   8 -  15  */
     16,  17,  18,  19,  20,  21,  22,  23,	/*  16 -  23  */
     24,  25,  26,  27,  28,  29,  30,  31,	/*  24 -  31  */
     32,  33,  34,  35,  36,  37,  38,  39,	/*  32 -  39  */
     40,  41,  42,  43,  44,  45,  46,  47,	/*  40 -  47  */
     48,  49,  50,  51,  52,  53,  54,  55,	/*  48 -  55  */
     56,  57,  58,  59,  60,  61,  62,  63,	/*  56 -  63  */
     64,  65,  66,  67,  68,  69,  70,  71,	/*  64 -  71  */
     72,  73,  74,  75,  76,  77,  78,  79,	/*  72 -  79  */
     80,  81,  82,  83,  84,  85,  86,  87,	/*  80 -  87  */
     88,  89,  90,  91,  92,  93,  94,  95,	/*  88 -  95  */
     96,  97,  98,  99, 100, 101, 102, 103,	/*  96 - 103  */
    104, 105, 106, 107, 108, 109, 110, 111,	/* 104 - 111  */
    112, 113, 114, 115, 116, 117, 118, 119,	/* 112 - 119  */
    120, 121, 122, 123, 124, 125, 126, 127,	/* 120 - 127  */
    196, 197, 199, 201, 209, 214, 220, 225,	/* 128 - 135  */
    224, 226, 228, 227, 229, 231, 233, 232,	/* 136 - 143  */
    234, 235, 237, 236, 238, 239, 241, 243,	/* 144 - 151  */
    242, 244, 246, 245, 250, 249, 251, 252,	/* 152 - 159  */
    190, 176, 162, 163, 167, 130, 182, 223,	/* 160 - 167  */
    174, 169, 142, 180, 168, 173, 198, 216,	/* 168 - 175  */
    141, 177, 178, 179, 165, 181, 166, 135,	/* 176 - 183  */
    159, 185, 188, 170, 186, 189, 230, 248,	/* 184 - 191  */
    191, 161, 172, 146, 128, 129, 140, 171,	/* 192 - 199  */
    187, 131, 160, 192, 195, 144, 145, 147,	/* 200 - 207  */
    208, 132, 150, 148, 149, 213, 247, 215,	/* 208 - 215  */
    255, 153, 152, 164, 134, 221, 222, 151,	/* 216 - 223  */
    136, 183, 137, 139, 138, 194, 202, 193,	/* 224 - 231  */
    203, 200, 205, 206, 207, 204, 211, 212,	/* 232 - 239  */
    240, 210, 218, 219, 217, 155, 154, 133,	/* 240 - 247  */
    175, 157, 156, 158, 184, 253, 254, 143,	/* 248 - 255  */
  };



/* Conversion table generated mechanically by Free `recode' 3.5
   for sequence ISO-8859-1..macintosh (reversible).  */

unsigned char const iso_to_mac_maccvspro[256] =
  {
      0,   1,   2,   3,   4,   5,   6,   7,	/*   0 -   7  */
      8,   9,  10,  11,  12,  13,  14,  15,	/*   8 -  15  */
     16,  17,  18,  19,  20,  21,  22,  23,	/*  16 -  23  */
     24,  25,  26,  27,  28,  29,  30,  31,	/*  24 -  31  */
     32,  33,  34,  35,  36,  37,  38,  39,	/*  32 -  39  */
     40,  41,  42,  43,  44,  45,  46,  47,	/*  40 -  47  */
     48,  49,  50,  51,  52,  53,  54,  55,	/*  48 -  55  */
     56,  57,  58,  59,  60,  61,  62,  63,	/*  56 -  63  */
     64,  65,  66,  67,  68,  69,  70,  71,	/*  64 -  71  */
     72,  73,  74,  75,  76,  77,  78,  79,	/*  72 -  79  */
     80,  81,  82,  83,  84,  85,  86,  87,	/*  80 -  87  */
     88,  89,  90,  91,  92,  93,  94,  95,	/*  88 -  95  */
     96,  97,  98,  99, 100, 101, 102, 103,	/*  96 - 103  */
    104, 105, 106, 107, 108, 109, 110, 111,	/* 104 - 111  */
    112, 113, 114, 115, 116, 117, 118, 119,	/* 112 - 119  */
    120, 121, 122, 123, 124, 125, 126, 127,	/* 120 - 127  */
    196, 197, 165, 201, 209, 247, 220, 183,	/* 128 - 135  */
    224, 226, 228, 227, 198, 176, 170, 255,	/* 136 - 143  */
    205, 206, 195, 207, 211, 212, 210, 223,	/* 144 - 151  */
    218, 217, 246, 245, 250, 249, 251, 184,	/* 152 - 159  */
    202, 193, 162, 163, 219, 180, 182, 164,	/* 160 - 167  */
    172, 169, 187, 199, 194, 173, 168, 248,	/* 168 - 175  */
    161, 177, 178, 179, 171, 181, 166, 225,	/* 176 - 183  */
    252, 185, 188, 200, 186, 189, 160, 192,	/* 184 - 191  */
    203, 231, 229, 204, 128, 129, 174, 130,	/* 192 - 199  */
    233, 131, 230, 232, 237, 234, 235, 236,	/* 200 - 207  */
    208, 132, 241, 238, 239, 213, 133, 215,	/* 208 - 215  */
    175, 244, 242, 243, 134, 221, 222, 167,	/* 216 - 223  */
    136, 135, 137, 139, 138, 140, 190, 141,	/* 224 - 231  */
    143, 142, 144, 145, 147, 146, 148, 149,	/* 232 - 239  */
    240, 150, 152, 151, 153, 155, 154, 214,	/* 240 - 247  */
    191, 157, 156, 158, 159, 253, 254, 216,	/* 248 - 255  */
  };

/* From Fetch.  */

unsigned char const mac_to_iso[256] =
  {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0xC4, 0xC5, 0xC7, 0xC9, 0xD1, 0xD6, 0xDC, 0xE1,
	0xE0, 0xE2, 0xE4, 0xE3, 0xE5, 0xE7, 0xE9, 0xE8,
	0xEA, 0xEB, 0xED, 0xEC, 0xEE, 0xEF, 0xF1, 0xF3,
	0xF2, 0xF4, 0xF6, 0xF5, 0xFA, 0xF9, 0xFB, 0xFC,
	0xDD, 0xB0, 0xA2, 0xA3, 0xA7, 0x80, 0xB6, 0xDF,
	0xAE, 0xA9, 0x81, 0xB4, 0xA8, 0x82, 0xC6, 0xD8,
	0x83, 0xB1, 0xBE, 0x84, 0xA5, 0xB5, 0x8F, 0x85,
	0xBD, 0xBC, 0x86, 0xAA, 0xBA, 0x87, 0xE6, 0xF8,
	0xBF, 0xA1, 0xAC, 0x88, 0x9F, 0x89, 0x90, 0xAB,
	0xBB, 0x8A, 0xA0, 0xC0, 0xC3, 0xD5, 0x91, 0xA6,
	0xAD, 0x8B, 0xB3, 0xB2, 0x8C, 0xB9, 0xF7, 0xD7,
	0xFF, 0x8D, 0x8E, 0xA4, 0xD0, 0xF0, 0xDE, 0xFE,
	0xFD, 0xB7, 0x92, 0x93, 0x94, 0xC2, 0xCA, 0xC1,
	0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0xD3, 0xD4,
	0x95, 0xD2, 0xDA, 0xDB, 0xD9, 0x9E, 0x96, 0x97,
	0xAF, 0x98, 0x99, 0x9A, 0xB8, 0x9B, 0x9C, 0x9D,
  };



unsigned char const iso_to_mac[256] =
  {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0xA5, 0xAA, 0xAD, 0xB0, 0xB3, 0xB7, 0xBA, 0xBD,
	0xC3, 0xC5, 0xC9, 0xD1, 0xD4, 0xD9, 0xDA, 0xB6,
	0xC6, 0xCE, 0xE2, 0xE3, 0xE4, 0xF0, 0xF6, 0xF7,
	0xF9, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF, 0xF5, 0xC4,
	0xCA, 0xC1, 0xA2, 0xA3, 0xDB, 0xB4, 0xCF, 0xA4,
	0xAC, 0xA9, 0xBB, 0xC7, 0xC2, 0xD0, 0xA8, 0xF8,
	0xA1, 0xB1, 0xD3, 0xD2, 0xAB, 0xB5, 0xA6, 0xE1,
	0xFC, 0xD5, 0xBC, 0xC8, 0xB9, 0xB8, 0xB2, 0xC0,
	0xCB, 0xE7, 0xE5, 0xCC, 0x80, 0x81, 0xAE, 0x82,
	0xE9, 0x83, 0xE6, 0xE8, 0xED, 0xEA, 0xEB, 0xEC,
	0xDC, 0x84, 0xF1, 0xEE, 0xEF, 0xCD, 0x85, 0xD7,
	0xAF, 0xF4, 0xF2, 0xF3, 0x86, 0xA0, 0xDE, 0xA7,
	0x88, 0x87, 0x89, 0x8B, 0x8A, 0x8C, 0xBE, 0x8D,
	0x8F, 0x8E, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,
	0xDD, 0x96, 0x98, 0x97, 0x99, 0x9B, 0x9A, 0xD6,
	0xBF, 0x9D, 0x9C, 0x9E, 0x9F, 0xE0, 0xDF, 0xD8
  };

static const unsigned char *convert_iso(int convert_from, int iso)
{
	if(iso != 1 && iso != 2) // only Latin 1 available right now
		return NULL;
	
	if(iso == 1)
	{
		return convert_from ? iso_to_mac : mac_to_iso;
	}
	else
		return convert_from ? iso_to_mac_maccvspro : mac_to_iso_maccvspro;
}

void
mac_convert_file (const char *infile, int encode, char *outfile, int binary)
{
    FILE *infd, *outfd;
    char buf[8192];
    int len;
    int decode; /* 1 : server to client format, 0: client to server format */
#	define ISO8559_VALID(iso) ((iso) == 1 || (iso) == 2)
	static int set_text_signature = -1;
	static int convert_iso8559 = -1;
	static int convert_maclf = -1;
	
	decode = encode ? 0 : 1;
	
	if(convert_iso8559 == -1)
	{
		const char *iso = CProtocolLibrary::GetEnvironment("ISO8859");
		if(iso == NULL || sscanf(iso, "%d", &convert_iso8559) != 1 ||
			!ISO8559_VALID(convert_iso8559))
				convert_iso8559 = 0;
	}
	if(set_text_signature == -1)
	{
		set_text_signature = CProtocolLibrary::GetEnvironment("IC_ON_TXT") != NULL ? 1 : 0;
	}

	// create text files with the Mac CR (0x0D)
	// the default is LF on OSX
	if(convert_maclf == -1)
	{
		convert_maclf = CProtocolLibrary::GetEnvironment("CVS_MACLF") != NULL ? 1 : 0;
	}

	/* "update" doesn't check if we can erase the dest file */
	chmod(outfile, S_IRWXU);
	
	if(convert_hqx_file(infile, encode, outfile, &binary))
		return;

	const unsigned char *isotab = NULL;
	if(convert_iso8559)
		isotab = convert_iso(decode, convert_iso8559);
	
    if ((infd = fopen (infile, (binary || convert_maclf) ? "rb" : "r")) == NULL)
        error (1, errno, "cannot read %s", infile);
    if ((outfd = fopen (outfile, (binary || convert_maclf) ? "wb" : "w")) == NULL)
        error (1, errno, "cannot write %s", outfile);

    while ((len = fread (buf, sizeof (char), sizeof (buf) / sizeof(char), infd)) > 0)
	{
		// for a text file, we have some post process depending
		// if we encode for the server or decode from the server
		if(binary == 0)
		{
			char * conv 	= buf;
			int    convlen  = len;
			while (convlen--)
			{
				if(isotab != NULL)
				{
					*conv = (char)isotab[(unsigned char)*conv];
				}
				// on OSX, we don't have MSL-stdio switching the CR <-> LF,
				// we can go straigth to the point
				if(convert_maclf)
				{
					if(decode)
					{
						// for a text file, this will turn the line feed
						// to the mac one
						if(*conv == 0x0a)
							*conv = 0x0d;
					}
					else
					{
						if(*conv == 0x0d)
							*conv = 0x0a;
					}
				}
				conv++;
			}
		}

		if (fwrite (buf, sizeof(char), len, outfd) < 0)
			error (1, errno, "error writing %s", outfile);
    }
    if (len < 0)
        error (1, errno, "error reading %s", infile);

    if (fclose (outfd) != 0)
        error (0, errno, "warning: cannot close %s", outfile);
    if (fclose (infd) != 0)
        error (0, errno, "warning: cannot close %s", infile);
        
	if((binary || set_text_signature) && decode)
		set_file_type(outfile, !quiet);
}

#endif /* __MACH__ */
