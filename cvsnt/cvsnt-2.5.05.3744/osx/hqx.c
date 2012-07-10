

/* file hqx.c 
	BinHex decoder/encoder routines, implementation.
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

/* Jens Miltner <jum@mac.com> 2007-01-10: made code endianness-savvy */

#ifdef __MACH__

#include "hqx.h"
#include "cvs_hqx.h"
#include <string.h>
#include <CoreServices/CoreServices.h>

#define CVS_CHANGES
	/* due to the fact we want to avoid some informations which are going
	to screw-up the server in his attempt to compare if the file *really* differs with the repository,
	we have to suppress some enclosed informations which could be only specific to a user */

	/* constants */
#define kBinHexMagic 0x1021
#define kRLEFlag 0x90
#define kCopyBufLen 4096
#define HT ((char) 9) /* horizontal tab */
#define LF ((char) 10) /* line feed, newline */
#define CR ((char) 13) /* carriage return */

	/* types */
typedef struct {
	unsigned char fInBuffer[4*1024];
	unsigned char fOutBuffer[4*1024];
	unsigned char *fOutBufp;
	long fBitBuffer, fBitBufferBits;
	long fNOutChars;
	long fRLECount;
	unsigned char fRLECharacter;
	unsigned short fCRC;
	HQXSink fSink;
	long fSinkRefCon;
} HQXEncodeVars, *HQXEncVarsPtr;

typedef struct {
	unsigned char fInBuffer[4*1024];
	unsigned char fOutBuffer[4*1024];
	unsigned char *fInBufp, *fInBufMax;
	long fBitBuffer, fBitBufferBits;
	long fRLECount;
	unsigned char fRLECharacter;
	Boolean fInHqxData;
	unsigned short fCRC;
	HQXSource fSource;
	long fSourceRefCon;
} HQXDecodeVars, *HQXDecVarsPtr;

	/* globals */
static Boolean gHQXinited = false;
static char *gHQX = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";
static short hqxtab[256];
static short ihqxtab[256];
static unsigned short crctab[256];


/* Byte swappers */
#if TARGET_RT_BIG_ENDIAN
	#define EndianFInfo_BtoN(ioFlFndrInfo) do {} while (false)
	#define EndianFInfo_NtoB(ioFlFndrInfo) do {} while (false)
#else // !TARGET_RT_BIG_ENDIAN
	static void EndianFInfo_NtoB(FInfo* ioFlFndrInfo)
	{
		if ( ioFlFndrInfo ) {
			ioFlFndrInfo->fdType = EndianU32_NtoB(ioFlFndrInfo->fdType);
			ioFlFndrInfo->fdCreator = EndianU32_NtoB(ioFlFndrInfo->fdCreator);
			ioFlFndrInfo->fdFlags = EndianU16_NtoB(ioFlFndrInfo->fdFlags);
			ioFlFndrInfo->fdLocation.v = EndianS16_NtoB(ioFlFndrInfo->fdLocation.v);
			ioFlFndrInfo->fdLocation.h = EndianS16_NtoB(ioFlFndrInfo->fdLocation.h);
			ioFlFndrInfo->fdFldr = EndianS16_NtoB(ioFlFndrInfo->fdFldr);
		}
	}
	static void EndianFInfo_BtoN(FInfo* ioFlFndrInfo)
	{
		if ( ioFlFndrInfo ) {
			ioFlFndrInfo->fdType = EndianU32_BtoN(ioFlFndrInfo->fdType);
			ioFlFndrInfo->fdCreator = EndianU32_BtoN(ioFlFndrInfo->fdCreator);
			ioFlFndrInfo->fdFlags = EndianU16_BtoN(ioFlFndrInfo->fdFlags);
			ioFlFndrInfo->fdLocation.v = EndianS16_BtoN(ioFlFndrInfo->fdLocation.v);
			ioFlFndrInfo->fdLocation.h = EndianS16_BtoN(ioFlFndrInfo->fdLocation.h);
			ioFlFndrInfo->fdFldr = EndianS16_BtoN(ioFlFndrInfo->fdFldr);
		}
	}
#endif // !TARGET_RT_BIG_ENDIAN

/* CRC CALCULATION */

static void BuildCRCTable(unsigned short magic_number) {
	unsigned long i, magic, mgc, val, bit;
	for (i = 0; i < 256; i++) {
		magic = (magic_number << 8);
		mgc = (magic >> 1);
		val = (i << 16);
		for (bit = 23; bit > 15; bit--) {
			if ((val & (1 << bit)) != 0) val ^= mgc;
			mgc >>= 1;
		}
		crctab[i] = val & 0xFFFF;
	}
}

static unsigned short initial_crc(void) {
	return 0;
}

static unsigned short crc_byte(unsigned short crc, unsigned char byte) {
	return ( (crctab[(crc >> 8) & 255] ^ ((crc << 8) | byte)) & 0xFFFF );
}

static unsigned short crc_run(unsigned short crc, const void* buf, long len) {
	const unsigned char *ch = (const unsigned char *)buf;
	long i;
	unsigned short crcv = crc;
	for (i = 0; i < len; i++)
		 crcv = (crctab[(crcv >> 8) & 255] ^ ((crcv << 8) | (*ch++))) & 0xFFFF;
	return crcv;
}

static void HQXInit(void) {
	if (!gHQXinited) {
		long i, n;
		for (i = 0;i < 256;i++) {
			hqxtab[i] = -1;
			ihqxtab[i] = -1;
		}
		for (i = 0, n = strlen(gHQX); i < n; i++) {
			hqxtab[gHQX[i]] = i;
			ihqxtab[i] = gHQX[i];
		}
		hqxtab[CR] = -2;
		hqxtab[LF] = -2;
		hqxtab[HT] = -2;
		hqxtab[' '] = -2;
		hqxtab[':'] = -3;
		BuildCRCTable(kBinHexMagic);
		gHQXinited = true;
	}
}


/* BUFFERED OUTPUT */


static OSErr BUFFWrite(HQXEncVarsPtr encv, void* buffer, long count) {
	unsigned char* ch;
	long i;
	OSErr err;
	ch = (unsigned char*) buffer;
	for (i = 0;i < count;i++) {
		if (encv->fOutBufp - encv->fOutBuffer == sizeof(encv->fOutBuffer)) {
			if ((err = encv->fSink(encv->fOutBuffer, sizeof(encv->fOutBuffer), encv->fSinkRefCon)) != noErr) return err;
			encv->fOutBufp = encv->fOutBuffer;
		}
		*encv->fOutBufp++ = *ch++;
	}
	return noErr;
}

static OSErr BUFFWriteEnd(HQXEncVarsPtr encv) {
	if (encv->fOutBufp - encv->fOutBuffer > 0)
		return encv->fSink(encv->fOutBuffer, encv->fOutBufp - encv->fOutBuffer, encv->fSinkRefCon);
	else return noErr;
}


/* HQX OUTPUT */

static OSErr HQXWrite(HQXEncVarsPtr encv, void* buffer, long count) {
	unsigned char* ch, code;
	long i, v;
	OSErr err;
	ch = (unsigned char*) buffer;
	for (i = 0; i < count; i++, ch++) {
		encv->fBitBuffer = (encv->fBitBuffer << 8) | ((*ch)&255);
		encv->fBitBufferBits += 8;
		while (encv->fBitBufferBits >= 6) {
			v = (encv->fBitBuffer >> (encv->fBitBufferBits-6)) & 0x003F;
			encv->fBitBufferBits -= 6;
			code = ihqxtab[v];
			if ((err = BUFFWrite(encv, &code, 1)) != noErr) return err;
			encv->fNOutChars++;
			if ((encv->fNOutChars % 64) == 0) {
				if ((err = BUFFWrite(encv, "\n", 1)) != noErr) return err;
			}
		}
	}
	return noErr;
}

static OSErr HQXWriteEnd(HQXEncVarsPtr encv) {
	unsigned char code;
	OSErr err;
	if (encv->fBitBufferBits > 0) {
		code = ihqxtab[encv->fBitBuffer<<(6-encv->fBitBufferBits) & 0x03F];
		if ((err = BUFFWrite(encv, &code, 1)) != noErr) return err;
		encv->fNOutChars++;
	}
	return BUFFWrite(encv, ":\n", 2);
}


/* RLE OUTPUT */


static OSErr FlushRLE(HQXEncVarsPtr encv) {
	long n;
	OSErr err;
	unsigned char buffer[10], *putp;
	while (encv->fRLECount > 0) {
		n = encv->fRLECount > 255 ? 255 : encv->fRLECount;
		putp = buffer;
		*putp++ = encv->fRLECharacter;
		if (encv->fRLECharacter == kRLEFlag) {
			*putp++ = 0;	/* literal flag value */
			if (n == 2) {
				*putp++ = kRLEFlag;
				*putp++ = 0;	/* literal flag value */
			} else if (n > 2) {
				*putp++ = kRLEFlag;
				*putp++ = n;	/* repeat count */
			}
		} else {
			if (n == 2)
				*putp++ = encv->fRLECharacter;
			else if (n == 3) {
				*putp++ = encv->fRLECharacter;
				*putp++ = encv->fRLECharacter;
			} else if (n > 3) {
				*putp++ = kRLEFlag;
				*putp++ = n;
			}
		}
		if ((err = HQXWrite(encv, buffer, putp - buffer)) != noErr) return err;
		encv->fRLECount -= n;
	}
	return err;
}

static OSErr RLEWrite(HQXEncVarsPtr encv, const void* buffer, long count) {
	const unsigned char *ch;
	long i;
	OSErr err;
	ch = (const unsigned char*) buffer;
	err = noErr;
	for (ch = (const unsigned char*) buffer, i = 0; i < count; i++, ch++) {
		if (encv->fRLECount == 0) {
			encv->fRLECharacter = *ch;
			encv->fRLECount = 1;
		} else if (*ch == encv->fRLECharacter)
			encv->fRLECount += 1;
		else {
			if ((err = FlushRLE(encv)) != noErr) return err;
			encv->fRLECharacter = *ch;
			encv->fRLECount = 1;
		}
	}
	return err;
}

static OSErr RLEWriteEnd(HQXEncVarsPtr encv) {
	OSErr err;
	if ((err = FlushRLE(encv)) != noErr) return err;
	if ((err = HQXWriteEnd(encv)) != noErr) return err;
	return BUFFWriteEnd(encv);
}


static OSErr WriterInit(HQXEncVarsPtr encv, HQXSink dst, long refcon) {
	OSErr err;
	char *p;
	p = "(This file must be converted with BinHex 4.0)\n:";
	encv->fRLECount = 0;
	encv->fOutBufp = encv->fOutBuffer;
	encv->fSink = dst;
	encv->fSinkRefCon = refcon;
	if ((err = BUFFWrite(encv, p, strlen(p))) != noErr) return err;
	encv->fNOutChars = 1;
	encv->fBitBuffer = encv->fBitBufferBits = 0;
	return noErr;
}

/* CRC OUTPUT */


static OSErr CRCWriteInit(HQXEncVarsPtr encv) {
	encv->fCRC = initial_crc();
	return noErr;
}

static OSErr CRCWrite(HQXEncVarsPtr encv, const void* buffer, long count) {
	encv->fCRC = crc_run(encv->fCRC, buffer, count);
	return RLEWrite(encv, buffer, count);
}

static OSErr CRCWriteEnd(HQXEncVarsPtr encv) {
	unsigned short  crc;
	encv->fCRC = crc_byte(encv->fCRC, 0);
	encv->fCRC = crc_byte(encv->fCRC, 0);
	crc = EndianU16_NtoB(encv->fCRC);
	return RLEWrite(encv, &crc, 2);
}


OSErr HQXEncode(const char *filename, HQXSink dst, long refcon) {
	OSErr err;
	long zero, ldata;
	ByteCount bytecount, actcount;
	HQXEncVarsPtr encv;
	FSRef inspec;
	FSCatalogInfo catalogInfo;
	FSSpec fsSpec;
	FInfo ioFlFndrInfo;
	UInt32 ioFlLgLen, ioFlRLgLen;
	HFSUniStr255 forkName;
	SInt16 forkRefNum;
	char path[1024];

#ifdef CVS_CHANGES
	Point zeroPoint = {0, 0};
#endif

		/* set up */
	zero = 0;
	forkRefNum = 0;
	encv = NULL;
	HQXInit();
	encv = (HQXEncVarsPtr) NewPtrClear(sizeof(HQXEncodeVars));
	if ((err = MemError()) != noErr) goto bail;
	encv->fRLECount = 0;
	
	err = WriterInit(encv, dst, refcon);
	if (err != noErr) goto bail;

		/* hqx file header */
	err = CRCWriteInit(encv);
	if (err != noErr) goto bail;

	getcwd(path, sizeof(path));
	strcat(path, "/");
	strcat(path, filename);

	err = PathToFSRef(path, &inspec);
	if (err != noErr) goto bail;

	err = FSGetCatalogInfo(&inspec,
							 kFSCatInfoFinderInfo |
							 kFSCatInfoDataSizes |
							 kFSCatInfoRsrcSizes, &catalogInfo, 0L, &fsSpec, 0L);
	if (err != noErr) goto bail;

	ioFlFndrInfo = *(FInfo *)catalogInfo.finderInfo;
	ioFlLgLen = catalogInfo.dataLogicalSize;
	ioFlRLgLen = catalogInfo.rsrcLogicalSize;

#ifdef CVS_CHANGES
	/* the name is no use for cvs */
	err = CRCWrite(encv, "", 1);
#else
	err = CRCWrite(encv, fsSpec.name, *fsSpec.name + 1);
#endif
	if (err != noErr) goto bail;

	ldata = EndianS32_NtoB(zero);
	err = CRCWrite(encv, &ldata, 1);
	if (err != noErr) goto bail;
#ifdef CVS_CHANGES
	/* let remove some user specific flags which may differ with the repository file */
	ioFlFndrInfo.fdFldr = 0;
	ioFlFndrInfo.fdLocation = zeroPoint;
	ioFlFndrInfo.fdFlags &= ~(kNameLocked | kHasBeenInited);
#endif
	EndianFInfo_NtoB(&ioFlFndrInfo);
	err = CRCWrite(encv, &ioFlFndrInfo, 10);
	if (err != noErr) goto bail;
	ldata = EndianS32_NtoB(ioFlLgLen);
	err = CRCWrite(encv, &ldata, 4);
	if (err != noErr) goto bail;
	ldata = EndianS32_NtoB(ioFlRLgLen);
	err = CRCWrite(encv, &ldata, 4);
	if (err != noErr) goto bail;
	err = CRCWriteEnd(encv);
	if (err != noErr) goto bail;

		/*  file data fork */
	err = CRCWriteInit(encv);
	if (err != noErr) goto bail;

	err = FSGetDataForkName(&forkName);
	if (err != noErr) goto bail;

	err = FSOpenFork(&inspec, forkName.length, forkName.unicode, fsRdPerm,
					   &forkRefNum);
	if (err != noErr) goto bail;

	for (bytecount = 0; bytecount < ioFlLgLen; bytecount += actcount)
	{
		actcount = ioFlLgLen - bytecount;
		if (actcount > sizeof(encv->fInBuffer)) actcount = sizeof(encv->fInBuffer);
		err = FSReadFork(forkRefNum, fsAtMark, 0, actcount, encv->fInBuffer, &actcount);
		if (err != noErr) goto bail;
		err = CRCWrite(encv, encv->fInBuffer, actcount);
		if (err != noErr) goto bail;
	}
	FSCloseFork(forkRefNum); forkRefNum = 0;

	err = CRCWriteEnd(encv);
	if (err != noErr) goto bail;

		/* file resource fork */
	err = CRCWriteInit(encv);
	if (err != noErr) goto bail;

	err = FSGetResourceForkName(&forkName);
	if (err != noErr) goto bail;

	err = FSOpenFork(&inspec, forkName.length, forkName.unicode, fsRdPerm,
					   &forkRefNum);
	if (err != noErr) goto bail;

	for (bytecount = 0; bytecount < ioFlRLgLen; bytecount += actcount) {
		actcount = ioFlRLgLen - bytecount;
		if (actcount > sizeof(encv->fInBuffer)) actcount = sizeof(encv->fInBuffer);
		err = FSReadFork(forkRefNum, fsAtMark, 0, actcount, encv->fInBuffer, &actcount);
		if (err != noErr) goto bail;
		err = CRCWrite(encv, encv->fInBuffer, actcount);
		if (err != noErr) goto bail;
	}
	FSCloseFork(forkRefNum); forkRefNum = 0;

	err = CRCWriteEnd(encv);
	if (err != noErr) goto bail;
	
	err = RLEWriteEnd(encv);
	if (err != noErr) goto bail;
	DisposePtr((Ptr) encv);
	
	return noErr;
	
bail:
	if (forkRefNum != 0) FSCloseFork(forkRefNum);
	if (encv != NULL) DisposePtr((Ptr) encv);
	return err;
}
	



/* BINHEX DECODER */


/* HQX INPUT, HQXSource calls */

static OSErr GetNextCharacter(HQXDecVarsPtr decv, unsigned char* the_char) {
	OSErr err;
	long bytes;
	if (decv->fInBufp >= decv->fInBufMax) {
		err = decv->fSource(decv->fInBuffer, (bytes = sizeof(decv->fInBuffer), &bytes), decv->fSourceRefCon);
		if (err == eofErr) err = noErr;
		if (err != noErr) return err;
		decv->fInBufp = decv->fInBuffer;
		decv->fInBufMax = decv->fInBuffer + bytes;
		if (decv->fInBufp >= decv->fInBufMax) return eofErr;
	}
	*the_char = *decv->fInBufp++;
	return noErr;
}

static OSErr HQXRead(HQXDecVarsPtr decv, void* buffer, long count) {
	unsigned char *dst, code;
	long i;
	long readcode;
	OSErr err;

	dst = (unsigned char*) buffer;
	for ( i = 0; i < count; i++) {
		while (decv->fBitBufferBits < 8)  {
			readcode = -1;
			while (readcode < 0) {
					/* get the next character */
				err = GetNextCharacter(decv, &code);
				if (err != noErr) return err;
					/* look up the code in the table and dispatch on the value */
				readcode = hqxtab[code];
				if (readcode == -3) {
					decv->fInHqxData = !decv->fInHqxData;
				} else if (!decv->fInHqxData)
					readcode = -1;
			}			
			decv->fBitBuffer = (decv->fBitBuffer<<6) | readcode;
			decv->fBitBufferBits += 6;
		}
		*dst++ = (decv->fBitBuffer>>(decv->fBitBufferBits-8));
		decv->fBitBufferBits -= 8;
	}
	
	return noErr;
}


/* RLE INPUT,  HQXRead calls */

static OSErr RLERead(HQXDecVarsPtr decv, void* buffer, long count) {
	unsigned char *ch, nextchar, countvalue;
	long i;
	OSErr err;

	ch = (unsigned char*) buffer;
	err = noErr;
	for ( i=0; i < count; )
		if (decv->fRLECount > 0) {
			*ch++ = decv->fRLECharacter;
			decv->fRLECount--;
			i++;
		} else {
			if ((err = HQXRead(decv, &nextchar, 1)) != noErr) return err;
			if (nextchar == kRLEFlag) {
				if ((err = HQXRead(decv, &countvalue, 1)) != noErr) return err;
				if (countvalue == 0) {
					decv->fRLECharacter = kRLEFlag;
					decv->fRLECount = 1;
				} else decv->fRLECount = countvalue - 1;
			} else {
				decv->fRLECharacter = nextchar;
				decv->fRLECount = 1;
			}
		}
		
	return noErr;
}


/* CRC INPUT, RLERead calls */

static OSErr CRCReadInit(HQXDecVarsPtr decv) {
	decv->fCRC = initial_crc();
	return noErr;
}

static OSErr CRCRead(HQXDecVarsPtr decv, void* buffer, long count) {
	OSErr err;
	if ((err = RLERead(decv, buffer, count)) != noErr) return err;
	decv->fCRC = crc_run(decv->fCRC, buffer, count);
	return noErr;
}

static OSErr CRCReadEnd(HQXDecVarsPtr decv) {
	OSErr err;
	unsigned short saved_crc;
	if ((err = RLERead(decv, &saved_crc, 2)) != noErr) return err;
	saved_crc = EndianU16_BtoN(saved_crc);
	decv->fCRC = crc_byte(decv->fCRC, 0);
	decv->fCRC = crc_byte(decv->fCRC, 0);
	if (saved_crc != decv->fCRC) return paramErr; else return noErr;
}



/* FILE INPUT, CRCRead calls */

OSErr HQXDecode(HQXSource src, const char *fname, Boolean can_replace, Boolean header_search, long refcon) {
	OSErr err;
	FInfo *finfo;
	FInfo info;
	long zero, data_length, rsrc_length;
	ByteCount bytecount, actcount;
	short refnum;
	Str255 name;
	Boolean file_exists;
	HQXDecVarsPtr decv;
	FSRef outSpec, parentSpec;
	HFSUniStr255 forkName;
	FSCatalogInfo catalogInfo;
	char path[1024];
	UniChar ustr[256];
	SInt32 uniLen = sizeof(ustr) / sizeof(UniChar);

		/* set up */
	HQXInit();
	decv = NULL;
	zero = 0;
	refnum = 0;
	file_exists = false;
		
		/* allocate shared variables */
	decv = (HQXDecVarsPtr) NewPtrClear(sizeof(HQXDecodeVars));
	if ((err = MemError()) != noErr) goto bail;

		/* reader globals */
	decv->fRLECount = 0;
	decv->fInBufMax = decv->fInBufp = decv->fInBuffer;
	decv->fSource = src;
	decv->fSourceRefCon = refcon;
	decv->fBitBuffer = decv->fBitBufferBits = 0;
	decv->fInHqxData = false;
	
		/* search for the header string */
	if (header_search) {
		char *header, *headerp, inputchar;
		headerp = header = "(This file must be converted with BinHex 4.0)";
		while (*headerp != '\0') {
			err = GetNextCharacter(decv, (unsigned char*) &inputchar);
			if (err != noErr) goto bail;
			if (inputchar == *headerp) headerp++; else headerp = header;
		}
	}
	
		/* hqx file header */
	err = CRCReadInit(decv);
	if (err != noErr) goto bail;
	err = CRCRead(decv, name, 1);
	if (err != noErr) goto bail;
	err = CRCRead(decv, name+1, *name);
	if (err != noErr) goto bail;
	err = CRCRead(decv, &zero, 1);
	if (err != noErr) goto bail;
	zero = EndianS32_BtoN(zero);
	err = CRCRead(decv, &info, 10);
	if (err != noErr) goto bail;
	EndianFInfo_BtoN(&info);
	err = CRCRead(decv, &data_length, 4);
	if (err != noErr) goto bail;
	data_length = EndianS32_BtoN(data_length);
	err = CRCRead(decv, &rsrc_length, 4);
	if (err != noErr) goto bail;
	rsrc_length = EndianS32_BtoN(rsrc_length);
	err = CRCReadEnd(decv);
	if (err != noErr) goto bail;
	
  		/* create destination file */
	getcwd(path, sizeof(path));
	
	err = PathToFSRef(path, &parentSpec);
	if (err != noErr) goto bail;

	strcat(path, "/");
	strcat(path, fname);
	err = PathToFSRef(path, &outSpec);
	if (err != noErr && err != fnfErr) goto bail;

	if (can_replace && err == noErr) {
		err = FSDeleteObject(&outSpec);
		if (err == fnfErr) err = noErr;
		if (err != noErr) goto bail;
	}

		/* file data fork */
	err = CRCReadInit(decv);
	if (err != noErr) goto bail;

	err = StrToUnicode(fname, ustr, &uniLen, 0L);
	if (err != noErr) goto bail;

	err = FSCreateFileUnicode(&parentSpec, uniLen, ustr, kFSCatInfoNone,
							  0L, &outSpec, 0L);
	if (err != noErr) goto bail;

	file_exists = true;

	err = FSGetDataForkName(&forkName);
	if (err != noErr) goto bail;

	err = FSCreateFork(&outSpec, forkName.length, forkName.unicode);
	if (err != noErr) goto bail;

	err = FSOpenFork(&outSpec, forkName.length, forkName.unicode, fsRdWrPerm,
					   &refnum);
	if (err != noErr) goto bail;

	for (bytecount = 0; bytecount < data_length; bytecount += actcount) {
		actcount = data_length - bytecount;
		if (actcount > sizeof(decv->fOutBuffer)) actcount = sizeof(decv->fOutBuffer);
		err = CRCRead(decv, decv->fOutBuffer, actcount);
		if (err != noErr) goto bail;
		err = FSWriteFork(refnum, fsAtMark, 0, actcount, decv->fOutBuffer, &actcount);
		if (err != noErr) goto bail;
	}
	FSCloseFork(refnum); refnum = 0;

	err = CRCReadEnd(decv);
	if (err != noErr) goto bail;

		/* file resource fork */
	err = CRCReadInit(decv);
	if (err != noErr) goto bail;

	err = FSGetResourceForkName(&forkName);
	if (err != noErr) goto bail;

	err = FSCreateFork(&outSpec, forkName.length, forkName.unicode);
	if (err != noErr) goto bail;

	err = FSOpenFork(&outSpec, forkName.length, forkName.unicode, fsRdWrPerm,
					   &refnum);
	if (err != noErr) goto bail;

	for (bytecount = 0; bytecount < rsrc_length; bytecount += actcount) {
		actcount = rsrc_length - bytecount;
		if (actcount > sizeof(decv->fOutBuffer)) actcount = sizeof(decv->fOutBuffer);
		err = CRCRead(decv, decv->fOutBuffer, actcount);
		if (err != noErr) goto bail;
		err = FSWriteFork(refnum, fsAtMark, 0, actcount, decv->fOutBuffer, &actcount);
		if (err != noErr) goto bail;
	}
	FSCloseFork(refnum); refnum = 0;

	err = CRCReadEnd(decv);
	if (err != noErr) goto bail;

	err = FSGetCatalogInfo(&outSpec, kFSCatInfoFinderInfo, &catalogInfo, 0L, 0L, 0L);
	if (err != noErr) goto bail;

	finfo = (FInfo *)catalogInfo.finderInfo;
#ifdef CVS_CHANGES
	finfo->fdFlags = info.fdFlags & (~(kNameLocked | kHasBeenInited));
#else
	finfo->fdFlags = info.fdFlags & (~kHasBeenInited);
#endif
	finfo->fdType = info.fdType;
	finfo->fdCreator = info.fdCreator;

	err = FSSetCatalogInfo(&outSpec, kFSCatInfoFinderInfo, &catalogInfo);
	if (err != noErr) goto bail;

	DisposePtr((Ptr) decv);
	return noErr;
	
bail:
	if (refnum != 0) FSCloseFork(refnum);
	if (file_exists) FSDeleteObject(&outSpec);
	if (decv != NULL) DisposePtr((Ptr) decv);
	return err;
}


/* end of file hqx.c */

#endif /* __MACH__ */
