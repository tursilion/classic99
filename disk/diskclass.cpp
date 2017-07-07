//
// (C) 2009 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this code.
// All rights not explicity granted here are reserved by the
// author.
//
// You may redistribute this software provided the original
// archive is UNCHANGED and a link back to my web page,
// http://harmlesslion.com, is provided as the author's site.
// It is acceptable to link directly to a subpage at harmlesslion.com
// provided that page offers a URL for that purpose
//
// Source code, if available, is provided for educational purposes
// only. You are welcome to read it, learn from it, mock
// it, and hack it up - for your own use only.
//
// Please contact me before distributing derived works or
// ports so that we may work out terms. I don't mind people
// using my code but it's been outright stolen before. In all
// cases the code must maintain credit to the original author(s).
//
// -COMMERCIAL USE- Contact me first. I didn't make
// any money off it - why should you? ;) If you just learned
// something from this, then go ahead. If you just pinched
// a routine or two, let me know, I'll probably just ask
// for credit. If you want to derive a commercial tool
// or use large portions, we need to talk. ;)
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//

// Regarding nCurrentRecord and RecordNumber, so I don't forget again.
//
// RecordNumber is the value tracked in the PAB. For fixed-record files
// (and NOT relative like the E/A Manual says), it is /always/ used,
// incremented, and updated by the TI DSR. Thus it must always be
// correct when passed by the user.
// For variable-record files, the DSR uses the cached sector position
// in the file descriptors up at the top of VRAM, and the RecordNumber
// in the PAB is neither read nor written, EXCEPT during a Rewind/Restore
// operation, which zeros it. (OPEN calls Rewind/Restore, and so also
// has this effect).
//
// THUS: RecordNumber is the cached value from the user PAB
// nCurrentRecord is a simplification of the sector index & offset
// from the DSR's file tracking information.
// Both are needed and used as described above.

#include <windows.h>
#include <stdio.h>
#include <atlstr.h>
#include "tiemul.h"
#include "diskclass.h"

void WriteMemoryByte(Word address, Byte value, bool allowWrite);
Byte ReadMemoryByte(Word address, bool trueWrite);

const char *pszOptionNames[] = {
	"FIAD_WriteV9T9",		
	"FIAD_ReadTIFILES",
	"FIAD_ReadV9T9",
	"FIAD_WriteDV80AsText",
	"FIAD_WriteAllDVAsText",
	"FIAD_WriteDF80AsText",
	"FIAD_WriteAllDFAsText",
	"FIAD_ReadTextAsDV",
	"FIAD_ReadTextAsDF",
	"FIAD_ReadTextWithoutExt",
	"FIAD_ReadImgAsTIAP",
	"FIAD_AllowNoHeaderAsDF128",
	"FIAD_EnableLongFilenames",
	"FIAD_AllowMore127Files",

	"IMAGE_UseV9T9DSSD",

	"DISK_AutoMapDSK1",
	"DISK_WriteProtect",
};

const char *szDiskTypes[] = {
	"None",		

	"FIAD",						// autodetect TIFILES and V9T9
	"Image",					// autodetect PC99 and V9T9

	"TICC",
//	"CorComp Controller Card",
//  "OmniFlop drive",			// read real disks!
	
	"ClipBoard",				// it should be safe to let Clipboard float at the end
	"Clock",					// and clock too
};

//********************************************************
// s_FILEINFO (FileInfo)
//********************************************************

// constructor
s_FILEINFO::s_FILEINFO() {
	static int nCnt=0;

	nDrive=0;
	bFree=true;
	bOpen=false;
	bDirty=false;
	pData=NULL;
	nDataSize=0;
	LengthSectors=0;
	FileType=0;
	ImageType=IMAGE_UNKNOWN;
	RecordsPerSector=0;
	BytesInLastSector=0;
	RecordLength=0;
	NumberRecords=0;
	PABAddress=0;
	OpCode=0;
	Status=0;
	DataBuffer=0;
	CharCount=0;
	RecordNumber=0;
	ScreenOffset=0;
	LastError=0;
	nCurrentRecord=0;
	nLocalData=0;
	// warning: this is not atomic, but it will work here
	nIndex=nCnt++;
}

// copy a FileInfo object -- just the PAB block if specified
// If not, note that DATA is /moved/ rather than copied, if it was there
// Does NOT copy the bFree flag, to avoid messing that up :)
void s_FILEINFO::CopyFileInfo(FileInfo *p, bool bJustPAB) {
	if (NULL == p) return;

	// this is the PAB info
	PABAddress = p->PABAddress;
	OpCode = p->OpCode;
	Status = p->Status;
	DataBuffer = p->DataBuffer;
	CharCount = p->CharCount;
	RecordNumber = p->RecordNumber;
	ScreenOffset = p->ScreenOffset;
	csName = p->csName;
	nDrive = p->nDrive;

	if (bJustPAB) return;

	// this is everything else of value
	LengthSectors = p->LengthSectors;
	FileType = p->FileType;
	RecordsPerSector = p->RecordsPerSector;
	BytesInLastSector = p->BytesInLastSector;
	RecordLength = p->RecordLength;
	NumberRecords = p->NumberRecords;
	
	LastError = p->LastError;
	nCurrentRecord = p->nCurrentRecord;
	nLocalData = p->nLocalData;
	ImageType = p->ImageType;
	bOpen = p->bOpen;
	bDirty = p->bDirty;
//	bFree = p->bFree;		// watch out for this one!
	csOptions = p->csOptions;

	// NOTE: Changing source! We are taking it's data,
	// NOT COPYING IT!
	pData = p->pData;
	p->pData=NULL;
	nDataSize=p->nDataSize;
	p->nDataSize=0;
	// so we also nuke the open and dirty flags, since they aren't anymore
	// no need to check if they WERE, faster to just clear them
	p->bDirty = false;
	p->bOpen = false;
}

// remove any options in the filename and store them in the
// options member!
void s_FILEINFO::SplitOptionsFromName() {
	csOptions = "";
	if (csName[0] != '?') return;

	int nWhere=csName.Find('.');
	if (nWhere == -1) {
		// if no period, then it's not options!
		return;
	}

	csOptions = csName.Left(nWhere+1).MakeUpper();	// where+1 is a count
	csName = csName.Mid(nWhere+1);					// where+1 starts after the period
}

//********************************************************
// BaseDisk
//********************************************************

// initialize static member (this DOES get reset on occasion though!)
bool BaseDisk::bPowerUpRun = false;

void BaseDisk::Startup() {
	if (!bPowerUpRun) {
		// Not multi-thread safe, but should not be called by multiple threads (only CPU thread, I think)
		bPowerUpRun = true;
		//SetFiles(3);
	}
}

bool BaseDisk::SetFiles(int n) {
	CloseAllFiles();
	m_nMaxFiles=n; 

	// we just use this test to control debug, we still re-initialize everything
	int nNewTop = 0x3def - (256+256+6)*n - 5 - 1;		// should be 0x37d7 with 3 files
	int nCurTop = ReadMemoryByte(0x8370,false)*256 + ReadMemoryByte(0x8371,false);
	if (nNewTop != nCurTop) {
		if (n > 0) {
			debug_write("Setting top of VRAM to >%04X (%d files)", nNewTop, n);
		} else {
			debug_write("Setting top of VRAM to >3FFF (CS1 MODE)");
		}
	}
			
	// In order to be more compatible with real TI disk controllers, tweak up 
	// the free VRAM pointer. Note we do not USE the wasted VRAM, and for
	// today, we will allow even 0 files (for CS1 style memory)
	if (n > 0) {
		WriteMemoryByte(0x8370, (nNewTop>>8)&0xff, false);
		WriteMemoryByte(0x8371, nNewTop&0xff, false);
		updateCallFiles(nNewTop);
		// set up the header for disk buffers - P-Code Card crashes with Stack Overflow without these
		VDP[++nNewTop] = 0xaa;		// valid header
		VDP[++nNewTop] = 0x3f;		// top of VRAM, MSB
		VDP[++nNewTop] = 0xff;		// top of VRAM, LSB (TODO: CF7 will change top of VRAM value by 6 bytes)
		VDP[++nNewTop] = 0x11;		// CRU of this disk controller
		VDP[++nNewTop] = n;		// number of files
	} else {
		WriteMemoryByte(0x8370, 0x3f, false);
		WriteMemoryByte(0x8371, 0xff, false);
		updateCallFiles(0x3fff);
		// there are no disk buffers, so no disk buffer header
	}

	return true; 
}

bool BaseDisk::CheckOpenFiles() {
	for (int idx=0; idx<MAX_FILES; idx++) {
		if (m_sFiles[idx].bOpen == true) {
			return true;
		}
	}

	return false;
}

void BaseDisk::CloseAllFiles() {
	for (int idx=0; idx<MAX_FILES; idx++) {
		if (m_sFiles[idx].bOpen == false) {
			Close(&m_sFiles[idx]);
		}
	}
}

// handle global options - all derived classes should call this
// if they do not handle the option themselves
void BaseDisk::SetOption(int nOption, int nValue) {
	switch (nOption) {
		case OPT_DISK_AUTOMAPDSK1:	
			bAutomapDSK1 = nValue?true:false;
			break;

		case OPT_DISK_WRITEPROTECT:	
			bWriteProtect = nValue?true:false;
			break;
	}
}

bool BaseDisk::GetOption(int nOption, int &nValue) {
	switch (nOption) {
		case OPT_DISK_AUTOMAPDSK1:	
			nValue = bAutomapDSK1;
			break;

		case OPT_DISK_WRITEPROTECT:	
			nValue = bWriteProtect;
			break;

		default:
			return false;
	}

	return true;
}

// Do not use pFile after calling this function!
bool BaseDisk::Close(FileInfo *pFile) {
	// We have to allow this even if the file was not actually open,
	// because this is how we return the object to the pool

	// derived classes can generally use this version
	Flush(pFile);				// flush decides for itself if it is needed
	pFile->bOpen = false;		// whether it was set or not, just clear it

	// this test is only here for the debug statement :) otherwise I'd just wipe it
	if (!pFile->bFree) {
		debug_write("Releasing file buffer %d", pFile->nIndex);
		pFile->bFree=true;
	}

	// release any data that may be there
	if (NULL != pFile->pData) {
		free(pFile->pData);
		pFile->pData=NULL;
		pFile->nDataSize = 0;
	}

	return true;
}

// Use 'Close()' to release it.
// TODO: should we use m_nFiles to limit the number of open files??
FileInfo *BaseDisk::AllocateFileInfo() {
	for (int idx=0; idx<MAX_FILES; idx++) {
		// check for a free one
		if (m_sFiles[idx].bFree) {
			m_sFiles[idx].bFree = false;
			debug_write("Allocating file buffer %d", idx);
			return &m_sFiles[idx];
		}
	}

	// none free
	return NULL;
}

FileInfo *BaseDisk::FindFileInfo(CString csFile) {
	for (int idx=0; idx<MAX_FILES; idx++) {
		// note the case insensitive compare for the normal Windows filesystem -
		// the original TI filesystem /was/ case sensitive. But this was rarely
		// used to any meaningful extent.
		if ((!m_sFiles[idx].bFree) && (0 == csFile.CompareNoCase(m_sFiles[idx].csName))) {
//			debug_write("Reusing file buffer %d", idx);
			return &m_sFiles[idx];
		}
	}

	// no match
	return NULL;
}

// return a formatted local path name
CString BaseDisk::BuildFilename(FileInfo *pFile) {
	CString csTmp;

	if (NULL == pFile) {
		return csTmp;
	}

	// get local path
	csTmp = pDriveType[pFile->nDrive]->GetPath();
	// check for ending backslash
	if (csTmp.Right(1) != "\\") {
		csTmp+='\\';
	}
	// append requested filename
	csTmp+=pFile->csName;

	return csTmp;
}

// Read a record from an open file
// Need to store characters read in CharCount
bool BaseDisk::Read(FileInfo *pFile) {
	unsigned char *pDat;

	// all we need to do here is isolate the record in the data buffer
	// and copy it to the buffer.

	// handles fixed files (the record number is not used in variable files)
	if (0 == (pFile->Status & FLAG_VARIABLE)) {
		// if a file is not variable, use the record number
		pFile->nCurrentRecord = pFile->RecordNumber;
	}

	pDat = (pFile->nCurrentRecord * (pFile->RecordLength+2)) + pFile->pData;
	if ((pDat + pFile->RecordLength + 2 >= (pFile->pData + pFile->nDataSize + 2)) || (pFile->nCurrentRecord >= pFile->NumberRecords)) {
		debug_write("Seek past end of file %s", pFile->csName);
		pFile->LastError = ERR_READPASTEOF;
		return false;
	}

	// We don't need to worry much about whether it's variable or fixed here
	pFile->CharCount = *(unsigned short*)pDat;
	pDat+=2;

	// sanity test
	if (pFile->DataBuffer + pFile->CharCount > 0x4000) {
		debug_write("Attempt to read data past end of VDP, truncating.");
		pFile->CharCount = 0x4000 - pFile->DataBuffer;
	}

	// copy the data and debug
	memcpy(&VDP[pFile->DataBuffer], pDat, pFile->CharCount);
	debug_write("Read 0x%X bytes drive %d file %s (%s record %d) to >%04X", pFile->CharCount, pFile->nDrive, pFile->csName, (pFile->Status&FLAG_VARIABLE)?"Variable":"Fixed", pFile->nCurrentRecord, pFile->DataBuffer);
	
	// handle DSK1 automapping (AutomapDSK checks whether it's enabled)
	AutomapDSK(&VDP[pFile->DataBuffer], pFile->CharCount, pFile->nDrive, (pFile->Status & FLAG_VARIABLE)!=0);

	// update heatmap
	for (int idx=0; idx<pFile->CharCount; idx++) {
		UpdateHeatVDP(pFile->DataBuffer+idx);
	}

	// update current record
	pFile->nCurrentRecord++;

	// In case we need to write it back
	pFile->RecordNumber = pFile->nCurrentRecord;

	return true;
}

// Writes (or overwrites) a record. May need to append data depending on record number
bool BaseDisk::Write(FileInfo *pFile) {
	unsigned char *pDat;

	// figure out where to write to. We have a current record if we need it
	if (0 == (pFile->Status & FLAG_VARIABLE)) {
		// for fixed files with a provided record number
		pFile->nCurrentRecord = pFile->RecordNumber;
	}

	if (pFile->Status & FLAG_APPEND) {
		// always write to the end if it's append mode (override above)
		pFile->nCurrentRecord = pFile->NumberRecords;
	}

	// largest file is 32767 records, so we can test for that
	if (pFile->nCurrentRecord > 32767) {
		debug_write("Maximum record count reached: %s", pFile->csName);
		// TODO: is that the right error code?? (DSR - MEMORY_FULL >8000 - see how that maps)
		pFile->LastError = ERR_READPASTEOF;
		return false;
	}

	// otherwise, check for append. We'll allow append to arbitrary sizes
	pDat = (pFile->nCurrentRecord * (pFile->RecordLength+2)) + pFile->pData;
	if (pDat - pFile->pData >= (pFile->nDataSize-pFile->RecordLength-2)) {
		// we need to grow the data buffer
		int nOldSize = pFile->nDataSize;
		pFile->nDataSize = (pFile->nCurrentRecord+10) * (pFile->RecordLength+2);
		pFile->pData = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
		memset(pFile->pData+nOldSize, 0, pFile->nDataSize - nOldSize);
		// now set the real pointer
		pDat = (pFile->nCurrentRecord * (pFile->RecordLength+2)) + pFile->pData;
	}

	// fill in any previously unused records with valid data
	if (pFile->nCurrentRecord > pFile->NumberRecords) {
		// note test is > not >= cause if it's an append, the record will be written correctly anyway
		// this block is only to fill in gaps on random access
		for (int idx=pFile->NumberRecords; idx<pFile->nCurrentRecord; idx++) {
			unsigned char *pTmp = (idx * (pFile->RecordLength+2)) + pFile->pData;
			*(unsigned short*)pTmp = (pFile->Status&FLAG_VARIABLE) ? 0 : pFile->RecordLength;
			pTmp+=2;
			memset(pTmp, ' ', pFile->RecordLength);
		}
	}

	// We do need to worry about variable or fixed here to know how much data!
	if (pFile->Status & FLAG_VARIABLE) {
		// length is in charcount already
	} else {
		// copy length to charcount for code below - it's never written back so that's ok
		pFile->CharCount = pFile->RecordLength;
	}

	// sanity test
	if (pFile->DataBuffer + pFile->CharCount > 0x4000) {
		debug_write("Attempt to write data past end of VDP, truncating.");
		pFile->CharCount = 0x4000 - pFile->DataBuffer;
	}
	
	// set the length word
	*(unsigned short*)pDat = (unsigned short)pFile->CharCount;
	pDat+=2;

	// copy the data and debug
	memcpy(pDat, &VDP[pFile->DataBuffer], pFile->CharCount);
	debug_write("writing 0x%X bytes drive %d file %s (%s record %d) from >%04X", pFile->CharCount, pFile->nDrive, pFile->csName, (pFile->Status&FLAG_VARIABLE)?"Variable":"Fixed", pFile->nCurrentRecord, pFile->DataBuffer);

	// update the header (todo: do we need more?)
	if (pFile->nCurrentRecord+1 > pFile->NumberRecords) {
		pFile->NumberRecords = pFile->nCurrentRecord+1;
	}

	// update current record -- do only sequential files do this?? (TODO: check DSR)
	pFile->nCurrentRecord++;

	// update heatmap
	for (int idx=0; idx<pFile->CharCount; idx++) {
		UpdateHeatVDP(pFile->DataBuffer+idx);
	}

	// In case we need to write it back!
	pFile->RecordNumber = pFile->nCurrentRecord;

	return true;
}

// Restore/Rewind - moves the file pointer - to the
// beginning if sequential, to a specified record if relative
bool BaseDisk::Restore(FileInfo *pFile) {
	if (pFile->Status & FLAG_RELATIVE) {
		pFile->nCurrentRecord = pFile->RecordNumber;
	} else {
		pFile->nCurrentRecord = 0;
	}
	debug_write("Restore set record number to %d", pFile->nCurrentRecord);
	return true;
}

#if 0
// Scratch - deletes a record. Not implemented on the TI disk controller
// So is there really any point to implementing it when no software will use it?
bool BaseDisk::Scratch(FileInfo *pFile) {
	unsigned char *pDat;

	if (pFile->Status & FLAG_RELATIVE) {
		// delete the record at RecordNumber
		if (pFile->RecordNumber >= pFile->NumberRecords) {
			debug_write("Can't scratch non-existant record %d", pFile->RecordNumber);
			pFile->LastError = ERR_READPASTEOF;
			return false;
		}
		
		// Do it
		pFile->nCurrentRecord = pFile->RecordNumber;
		pDat = (pFile->nCurrentRecord * (pFile->RecordLength+2)) + pFile->pData;
		memmove(pDat, pDat+pFile->RecordLength+2, pFile->pData + pFile->nDataSize - pDat - pFile->RecordLength - 2);
		pFile->NumberRecords--;
	} else {
		debug_write("Can not scratch a sequential record.");
		pFile->LastError = ERR_ILLEGALOPERATION;
		return false;
	}
	debug_write("Scratched record %d", pFile->nCurrentRecord);
	return true;
}
#endif

// get file status - we get this for the most part from the header!
bool BaseDisk::Status(FileInfo *pFile) {
	pFile->ScreenOffset = 0;
	// Currently we only return some of the bits (See STATUS_ enum)
	// Is it too hard to use ONE set of flags?
	if (pFile->FileType & TIFILES_VARIABLE)	pFile->ScreenOffset|=STATUS_VARIABLE;
	if (pFile->FileType & TIFILES_INTERNAL)	pFile->ScreenOffset|=STATUS_INTERNAL;
	if (pFile->FileType & TIFILES_PROGRAM)	pFile->ScreenOffset|=STATUS_PROGRAM;
	if (pFile->nCurrentRecord >= pFile->NumberRecords) pFile->ScreenOffset|=STATUS_EOF;

	// not supported:
	//#define STATUS_PROTECTED	0x40
	//#define STATUS_DISKFULL	0x02

	// handled elsewhere:
	//#define STATUS_NOSUCHFILE	0x80

	return true;
}

// Write an integer into a buffer as TI floating point
// From Thierry's page (nouspikel.com/ti99)
//*--------------------------------------
//* Floating point format
//* ---------------------
//* Float numbers are 8 bytes long: EE 12 34 56 78 9A BC
//* EE is the exponent in radix 100 (not in radix 10 as usual!). It is biased
//* by 64: >40=0, 41=1 (i.e *100), >42=2 (i.e * 10,000) >3F= -1 (i.e /100), etc
//*
//* 12 ... BC are the mantissa in binary coded decimal: each byte encodes two
//* decimal digits from 00 to 99
//*
//* For negative numbers, the first word is negated
//* For zero, the first word is >0000 the others are irrelevant
//*
//* Examples: 40 08 00 00 00 00 00 00 is 8.0
//*           41 02 37 00 00 00 00 00 is 255.0 (>37 hex = 55 decimal)
//*           BF F8 00 00 00 00 00 00 is -8.0
//*           43 01 02 03 04 05 06 07 is 1020304.050607
//*--------------------------------------
// reference the TI disk controller at address >5AE6
// So this function only handles integers, no fractions!
// Int must be from -9999 to 9999
char *BaseDisk::WriteAsFloat(char *pData, int nVal) {
	int nTmp;
	int word[2];

	// First write a size byte of 8
	*(pData++)=8;

	// translation of the TICC code, we can do better later ;)
	// Basically, we get the exponent and two bytes, and the rest are zeros
	nTmp = nVal;
	if (nVal < 0) nVal*=-1;
	if (nVal >= 100) {
		word[0]=(nVal/100) | 0x4100;		// 0x41 is the 100s counter, not sure how this works with 10,000, maybe it doesn't?
		word[1]=nVal%100;
	} else {
		if (nVal == 0) {
			word[0]=0;
		} else {
			word[0]= nVal | 0x4000;
		}
		word[1] = 0;
	}
	if (nTmp < 0) {
		word[0]=(~word[0])+1;
	}
	*(pData++)=(word[0]>>8)&0xff;
	*(pData++)=word[0]&0xff;
	*(pData++)=word[1]&0xff;

	// and five zeros
	for (int idx=0; idx<5; idx++) {
		*(pData++)=0;
	}

	return pData;
}

// search a buffer for "DSK1" and replace the '1' with the passed in value
// This is not quite perfect of course, it only works on the passed buffer,
// so if the DSK1 string is split across buffers, it will fail. 
// It will fail most often on EA#3 uncompressed files, since the data:header
// ratio is so low (only about 16 bytes per line). Still a 75% chance of working!
// About 84% chance on compressed EA#3. And about 95% chance on PROGRAM files.
void BaseDisk::AutomapDSK(unsigned char *pBuf, int nSize, int nDsk, bool bVariable) {
	if (!bAutomapDSK1) return;

	if ((nDsk > 9)||(nDsk < 0)) {
		debug_write("Automap error - can't map to dsk %d", nDsk);
		return;
	}

	unsigned char *pSave = pBuf;
	unsigned char *pEnd = pBuf + nSize - 4;
	while (pBuf < pEnd) {
		// check for a binary string (could be any record type, since it may load from data files)
		if (0 == memcmp(pBuf, "DSK1", 4)) {
			debug_write("Replacing binary string DSK1 with DSK%d", nDsk);
			pBuf+=3;
			*pBuf=nDsk+'0';
		}
		pBuf++;
	}
	if (nSize == 80) {
		// Check for an Editor/Assembler #3 string
		// these are 80 character records, so the size should reflect that
		// tags from 0-I are placed either every 4 or every 3 characters,
		// with the last one being F (followed by a space), except tag 0
		// is special and also has a 6 character name. We'll try being very
		// picky, only take lines that start with 9 or A (absolute or
		// relocatable data), and only read tags B or C, stopping at tag F.

		// first, scan backwards and see if the last tag is F. Note there
		// may be a line number after the F, but it should be decimal.
		// Ignore till the first whitespace, then it better be 'F'
		bool bValid = false;
		pBuf = pEnd-1;
		while ((isdigit(*pBuf)) && (pBuf>pSave)) pBuf--;
		while ((isspace(*pBuf)) && (pBuf>pSave)) pBuf--;
		if ((pBuf > pSave) && (*pBuf == 'F')) bValid=true;

		if (bValid) {
			// looks like it might be a record, now check the tags are valid
			if (!bVariable) {
				// try compressed (D/F80)
				pBuf = pSave;
				if (*pBuf == '0') pBuf+=9;		// skip ident tag (tag + 2 bytes + 6 byte string)
				while (pBuf < pEnd) {
					if (islower(*pBuf)) {		// no lower case letters
						break;
					}
					if (!isxdigit(*pBuf)) {		// fail on tags G,H,I - not valid on TI though I is supposed to be ignored
						break;
					}
					if ((*pBuf == 'E')||(*pBuf == 'D')) {	// illegal tags on the TI
						break;
					}
					if ((*pBuf>'0')&&(*pBuf<'7')) {			// tags 1-6 are legal, but these lines won't contain data
						break;
					}
					if (*pBuf == 'F') break;				// end of line tag
					pBuf+=3;
				}
				if (*pBuf == 'F') {
					char buf[4] = { 0, 0, 0, 0 };
					// looks as good as we can tell - check for the DSK1 string
					// data is contained only in tags B and C
					pBuf = pSave;
					if (*pBuf == '0') pBuf+=9;		// skip ident tag (tag + 2 bytes + 6 byte string)
					while (pBuf < pEnd) {
						if (*pBuf == 'F') break;
						if ((*pBuf == 'B') || (*pBuf == 'C')) {
							memmove(buf, &buf[1], 3);
							buf[3]=*(pBuf+1);
							if (0 == memcmp(buf, "DSK1", 4)) {
								debug_write("Replacing compressed EA3 string DSK1 with DSK%d", nDsk);
								*(pBuf+1) = nDsk+'0';
							}
							memmove(buf, &buf[1], 3);
							buf[3]=*(pBuf+2);
							if (0 == memcmp(buf, "DSK1", 4)) {
								debug_write("Replacing compressed EA3 string DSK1 with DSK%d", nDsk);
								*(pBuf+2) = nDsk+'0';
							}
						}
						pBuf+=3;
					}
				}
			} else {
				// try uncompressed (D/V80)
				pBuf = pSave;
				if (*pBuf == '0') pBuf+=11;		// skip ident tag (tag + 4 bytes + 6 byte string)
				while (pBuf < pEnd) {
					if (islower(*pBuf)) {		// no lower case letters
						break;
					}
					if (!isxdigit(*pBuf)) {		// fail on tags G,H,I - not valid on TI though I is supposed to be ignored
						break;
					}
					if ((*pBuf == 'E')||(*pBuf == 'D')) {	// illegal tags on the TI
						break;
					}
					if ((*pBuf>'0')&&(*pBuf<'7')) {			// tags 1-6 are legal, but these lines won't contain data
						break;
					}
					if (*pBuf == 'F') break;				// end of line tag
					pBuf+=5;
				}
				if (*pBuf == 'F') {
					char buf[4] = { 0, 0, 0, 0 };
					char buftmp[6];
					// looks as good as we can tell - check for the DSK1 string
					// data is contained only in tags B and C
					pBuf = pSave;
					if (*pBuf == '0') pBuf+=9;		// skip ident tag (tag + 2 bytes + 6 byte string)
					while (pBuf < pEnd) {
						int nTmp;

						if (*pBuf == 'F') break;
						if ((*pBuf == 'B') || (*pBuf == 'C')) {
							memmove(buf, &buf[1], 3);
							buftmp[0]=*(pBuf+1);
							buftmp[1]=*(pBuf+2);
							buftmp[2]=0;
							if (1 != sscanf(buftmp, "%X", &nTmp)) break;	// bad string
							buf[3]=nTmp;		// actual value
							if (0 == memcmp(buf, "DSK1", 4)) {
								debug_write("Replacing uncompressed EA3 string DSK1 with DSK%d", nDsk);
								sprintf(buftmp, "%02X", (unsigned)(nDsk+'0')&0xff);
								memcpy(pBuf+1, buftmp, 2);
							}
							memmove(buf, &buf[1], 3);
							buftmp[0]=*(pBuf+3);
							buftmp[1]=*(pBuf+4);
							buftmp[2]=0;
							if (1 != sscanf(buftmp, "%X", &nTmp)) break;	// bad string
							buf[3]=nTmp;		// actual value
							if (0 == memcmp(buf, "DSK1", 4)) {
								debug_write("Replacing uncompressed EA3 string DSK1 with DSK%d", nDsk);
								sprintf(buftmp, "%02X", (unsigned)(nDsk+'0')&0xff);
								memcpy(pBuf+3, buftmp, 2);
							}
						}
						pBuf+=5;
					}
				}
			}
		}
	}
}
