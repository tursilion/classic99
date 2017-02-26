//
// (C) 2011 Mike Brent aka Tursi aka HarmlessLion.com
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

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <atlstr.h>
#include <time.h>
#include "tiemul.h"
#include "diskclass.h"
#include "clockdisk.h"

// This guy is a simple read-only interface to the Windows clock
// using the Corcomp(?) clock syntax. Direct register access is
// NOT emulated because I don't want to encourage new software to
// use it. (Of course, remains to be seen if anything uses the
// DSR approach today.)

// From the Corcomp manual, you can read as:
// INPUT #1:A$,B$,C$
// and get: day of the week (1-7), date (mm/DD/yy) and time (hh:MM:ss)
// You can also sent the clock with PRINT:
// PRINT #1:"d,mm/DD/yy,hh:MM:ss"
// hh is a 24-hr clock.
// The records do not track, you always read or write all three values.
// (This suggests it's just one record, unlike Thierry's notes). Corcomp
// notes you MUST always access all three. It's legal to PRINT and then
// INPUT on the very next line without closing the file inbetween.

//********************************************************
// ClockDisk
//********************************************************

// constructor
ClockDisk::ClockDisk() {
}

ClockDisk::~ClockDisk() {
}

// powerup routine
void ClockDisk::Startup() {
	BaseDisk::Startup();
}

// Use 'Close()' to release it.
FileInfo *ClockDisk::AllocateFileInfo() {
	// only one open file to the clock at a time
	if (m_sFiles[0].bFree) {
		m_sFiles[0].bFree = false;
		debug_write("Allocating clock file buffer (0)");
		return &m_sFiles[0];
	}

	// none free
	return NULL;
}

// There's no directory on the clock, so only one file can ever be open
FileInfo *ClockDisk::FindFileInfo(CString csFile) {
	if (!m_sFiles[0].bFree) {
		return &m_sFiles[0];
	}
	// no match
	return NULL;
}

// no filenames are needed
CString ClockDisk::BuildFilename(FileInfo *pFile) {
	return "";
}

// Open an existing file, check the header against the parameters
// We'll support UPDATE or INPUT, since UPDATE is the default, but WRITES will not be permitted.
bool ClockDisk::TryOpenFile(FileInfo *pFile) {
	char *pMode;
	int nMode = pFile->Status & FLAG_MODEMASK;	// should be UPDATE or INPUT
	FileInfo lclInfo;

	switch (nMode) {
		case FLAG_UPDATE:
			pMode="update";
			break;

		case FLAG_INPUT:
			pMode="input";
			break;

		default:
			debug_write("Unsupported mode - can't open.");
			pMode="unknown";
			return false;
	}

	// verify the settings
	// check if the user requested a default record length, and fill it in if so
	// we'll otherwise accept anything over 19 characters
	if (pFile->RecordLength == 0) {
		pFile->RecordLength = 20;
	}

	// now make sure the type is display and record length >= 20 (variable or fixed is both fine)
	if (pFile->Status&FLAG_INTERNAL) {
		debug_write("Clock supports display format only");
		return false;
	}
	if (pFile->RecordLength < 20) {
		debug_write("Clock record length must be at least 20 characters");
		return false;
	}

	// ready to go!
	return true;
}

// Read the data into the disk buffer
// This function's job is to read the file into individual records
// into the memory buffer so it can be worked on generically. This
// function is not used for PROGRAM image files, but everything else
// is fair game. In this case, we don't need to do anything as the
// clock data needs to be generated each time it is read.
bool ClockDisk::BufferFile(FileInfo *pFile) {
	unsigned char *pData;

	// this checks for PROGRAM images as well as Classic99 sequence bugs
	if (0 == pFile->RecordLength) {
		debug_write("Attempting to buffer file with 0 record length, can't do it.", (LPCSTR)pFile->csName);
		return false;
	}

	// set up a single dummy record to keep the rest of the system happy
	pFile->NumberRecords=1;

	// get a new buffer large enough that it looks correct
	if (NULL != pFile->pData) {
		free(pFile->pData);
		pFile->pData=NULL;
	}

	// Datasize = (1) * (record size + 2) + 1
	// we only need 1 record (we don't really need even that)
	// the +2 gives room for a length word (16bit) at the beginning of each
	// record, necessary because it may contain binary data with zeros
	// The final +1 is so we can read a terminating NUL safely at the end
	// NULs in the middle are overwritten by the next record
	pFile->nDataSize = (1) * (pFile->RecordLength + 2) + 1;
	pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
	pData = pFile->pData;

	// and zero out that record, since nothing will go into it
	memset(pData, 0, pFile->nDataSize);

	// set up the file info structure
	// calculate necessary data and fill in header fields
	// Most of this is probably meaningless here, but that's okay, we'll keep it
	pFile->RecordsPerSector = pFile->NumberRecords;
	pFile->LengthSectors = 1;		// one sector-ish
	pFile->FileType = 0;
	if (pFile->Status & FLAG_VARIABLE) pFile->FileType|=TIFILES_VARIABLE;
	if (pFile->Status & FLAG_INTERNAL) pFile->FileType|=TIFILES_INTERNAL;
	pFile->BytesInLastSector = 0;

	debug_write("Clock read %d records", pFile->NumberRecords);
	return true;
}

// Open a file with a particular mode
// Clock will be DVxx or DFxx (record length of 0 is set to 10)
// Return the FileInfo object actually allocated for use, or NULL on failure.
FileInfo *ClockDisk::Open(FileInfo *pFile) {
	FileInfo *pNewFile=NULL;

	if (pFile->bOpen) {
		// trying to open a file that is already open! Can't allow that!
		pFile->LastError = ERR_FILEERROR;
		return NULL;
	}

	// See if we can get a new file handle from the driver
	pNewFile = pDriveType[pFile->nDrive]->AllocateFileInfo();
	if (NULL == pNewFile) {
		// no files free
		pFile->LastError = ERR_BUFFERFULL;
		return NULL;
	}

	// set default record length
	if (pFile->RecordLength == 0) {
		pFile->RecordLength=20;
	}

	// let's see what we are doing here...
	switch (pFile->Status & FLAG_MODEMASK) {
		// we can open these both the same way
		case FLAG_INPUT:
		case FLAG_UPDATE:
		case FLAG_APPEND:
			// First, try to open the file
			if (!TryOpenFile(pFile)) {
				// no? Error out then (save error already set). There's no create step.
				goto error;
			}

			// So we should have a file now - read it in (faked anyway)
			if (!BufferFile(pFile)) {
				pFile->LastError = ERR_FILEERROR;
				goto error;
			}
			break;

		default:	// nothing else supported - we shouldn't get here
			debug_write("Clock got into unexpected open mode.");
			pFile->LastError = ERR_FILEERROR;
			goto error;
	}

	// Finally, transfer the object over to the DriveType object
	pNewFile->CopyFileInfo(pFile, false);
	return pNewFile;

error:
	// release the allocated fileinfo, we didn't succeed to open
	// Use the base class as we have nothing to flush
	Close(pNewFile);
	return NULL;
}

// Read a record from an open file
// Need to store characters read in CharCount
// This override doesn't increment the record number, so record 0 is
// always accessed without risk of EOF. It also generates the output
// string on the fly from the system clock, rather than using the buffer.
bool ClockDisk::Read(FileInfo *pFile) {
	// get the current time into a formatted string
	char buf[32];
	time_t tm;
	struct tm *timeinfo;

	time(&tm);
	timeinfo = localtime(&tm);
	strftime(buf, 32, "%w,%m/%d/%y,%H:%M:%S", timeinfo);
	//buf[0]++;					// clock uses 1-7 for days, not 0-6 like strftime
	//							   NOT TRUE! Apparently the CorComp manual was wrong ;)

	pFile->CharCount = strlen(buf);	

	// sanity test
	if (pFile->DataBuffer + pFile->CharCount > 0x4000) {
		debug_write("Attempt to read data past end of VDP, truncating.");
		pFile->CharCount = 0x4000 - pFile->DataBuffer;
	}

	// copy the data and debug
	memcpy(&VDP[pFile->DataBuffer], buf, pFile->CharCount);
	debug_write("Read 0x%X bytes drive %d file %s (%s record %d) to >%04X", pFile->CharCount, pFile->nDrive, pFile->csName, (pFile->Status&FLAG_VARIABLE)?"Variable":"Fixed", pFile->nCurrentRecord, pFile->DataBuffer);

	// we always want to use record 0
	pFile->RecordNumber = pFile->nCurrentRecord = 0;

	return true;
}

// Writes (or overwrites) a record. Writing is not permitted in the Classic99
// implementation - this function is needed because we allow to open in UPDATE mode.
bool ClockDisk::Write(FileInfo *pFile) {
	pFile->LastError = ERR_FILEERROR;
	return false;
}
