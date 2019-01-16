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

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <atlstr.h>
#include <time.h>
#include "tiemul.h"
#include "diskclass.h"
#include "clipboarddisk.h"

// This guy is a simple read/write interface to the Windows clipboard
// no guarantee of non-ASCII data working, but we'll try whatever :)

//********************************************************
// ClipboardDisk
//********************************************************

// constructor
ClipboardDisk::ClipboardDisk(HWND hWnd) {
	m_hWnd=hWnd;
}

ClipboardDisk::~ClipboardDisk() {
}

// powerup routine
void ClipboardDisk::Startup() {
	BaseDisk::Startup();
}

// Use 'Close()' to release it.
FileInfo *ClipboardDisk::AllocateFileInfo() {
	// Only one open file to the clipboard at a time is allowed
	if (m_sFiles[0].bFree) {
		m_sFiles[0].bFree = false;
		debug_write("Allocating clipboard buffer (0)");
		return &m_sFiles[0];
	}

	// none free
	return NULL;
}

// There's no directory on the clipboard, so only one file can ever be open
FileInfo *ClipboardDisk::FindFileInfo(CString csFile) {
	if (!m_sFiles[0].bFree) {
		return &m_sFiles[0];
	}
	// no match
	return NULL;
}

// no filenames are needed
CString ClipboardDisk::BuildFilename(FileInfo *pFile) {
	return "";
}

// create an output file - test that we can write, set up blank data
bool ClipboardDisk::CreateOutputFile(FileInfo *pFile) {
	// first a little sanity checking -- we never overwrite an existing file
	// unless the mode is 'output', so check existance against the mode
	if ((pFile->Status & FLAG_MODEMASK) != FLAG_OUTPUT) {
		// no, we are not
		debug_write("Can't overwrite clipboard with open mode 0x%02X", (pFile->Status & FLAG_MODEMASK));
		return false;
	}

	// check if the user requested a default record length, and fill it in if so
	if (pFile->RecordLength == 0) {
		pFile->RecordLength = 254;
	}

	// now make sure the type is display (any record length, variable or fixed is both fine)
	if (pFile->Status&FLAG_INTERNAL) {
		debug_write("Clipboard supports display files only");
		return false;
	}

	// calculate necessary data and fill in header fields
	// Most of this is probably meaningless here, but that's okay, we'll keep it
	pFile->RecordsPerSector = 256  / (pFile->RecordLength + ((pFile->Status & FLAG_VARIABLE) ? 1 : 0) );
	pFile->LengthSectors = 1;
	pFile->FileType = 0;
	if (pFile->Status & FLAG_VARIABLE) pFile->FileType|=TIFILES_VARIABLE;
	if (pFile->Status & FLAG_INTERNAL) pFile->FileType|=TIFILES_INTERNAL;
	pFile->BytesInLastSector = 0;
	pFile->NumberRecords = 0;

	// we just assume the clipboard will be okay when we need it - it
	// might be a long time from now!

	// Allocate a buffer for it (size of 10 records by default)
	if (NULL != pFile->pData) {
		free(pFile->pData);
	}
	pFile->pData = (unsigned char*)malloc(10 * (pFile->RecordLength+2));
	pFile->bDirty = true;	// never written out!

	return true;
}

// Open an existing file, check the header against the parameters
bool ClipboardDisk::TryOpenFile(FileInfo *pFile) {
	char *pMode;
	int nMode = pFile->Status & FLAG_MODEMASK;	// should be UPDATE, APPEND or INPUT
	FileInfo lclInfo;

	switch (nMode) {
		case FLAG_UPDATE:
			pMode="update";
			break;

		case FLAG_APPEND:
			pMode="append";
			break;

		case FLAG_INPUT:
			pMode="input";
			break;

		default:
			debug_write("Unknown mode - can't open.");
			pMode="unknown";
			return false;
	}

	// verify the settings
	// check if the user requested a default record length, and fill it in if so
	if (pFile->RecordLength == 0) {
		pFile->RecordLength = 254;
	}

	// now make sure the type is display and record length is 80 (variable or fixed is both fine)
//	if ((pFile->RecordLength != 80) || (pFile->Status&FLAG_INTERNAL)) {
	if (pFile->Status&FLAG_INTERNAL) {
		debug_write("Clipboard supports display files only");
		return false;
	}

	// check if there is any text at all
	if (!IsClipboardFormatAvailable(CF_TEXT)) {
		debug_write("No text on clipboard!");
		pFile->LastError = ERR_BADATTRIBUTE;
		return false;
	}

	// we assume the clipboard will be good when we need it!
	return true;
}

// Read the file into the disk buffer
// This function's job is to read the file into individual records
// into the memory buffer so it can be worked on generically. This
// function is not used for PROGRAM image files, but everything else
// is fair game. It reads the entire file, and it stores the records
// at maximum size (even for variable length files). Note that if you 
// DO open a large file, though, Classic99 will try to allocate enough RAM
// to hold it. Buyer beware, so to speak. ;)
extern char *GetProcessedClipboardData(bool *pError);
bool ClipboardDisk::BufferFile(FileInfo *pFile) {
	unsigned char *pData;
	char *pTmp;
	char *pStr, *pBase;
	bool bErr = false;

	// this checks for PROGRAM images as well as Classic99 sequence bugs
	if (0 == pFile->RecordLength) {
		debug_write("Attempting to buffer file with 0 record length, can't do it.", (LPCSTR)pFile->csName);
		return false;
	}

	// all right, then, let's give it a shot.
	// (This code may be useful for the Clipboard access)
	// Buffer a Windows text file - records are delimited by
	// end of line characters (any two). Multibyte and Unicode
	// are not currently supported.
	pFile->NumberRecords=0;		// we don't know how many records there are, so we just read

	// get a new buffer as well sized as we can figure
	if (NULL != pFile->pData) {
		free(pFile->pData);
		pFile->pData=NULL;
	}

	// Datasize = (100) * (record size + 2) + 1
	// we assume 100 records to start and resize as needed
	// the +2 gives room for a length word (16bit) at the beginning of each
	// record, necessary because it may contain binary data with zeros
	// The final +1 is so we can read a terminating NUL safely at the end
	// NULs in the middle are overwritten by the next record
	pFile->nDataSize = (100) * (pFile->RecordLength + 2) + 1;
	pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
	pData = pFile->pData;

	// see if we can open the clipboard at all
	pStr = GetProcessedClipboardData(&bErr);
	if (bErr) {
		// No, we failed - most likely reason is someone else has it open for write
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// we only recognize clipboard text data
	// We assume all the prerequisites are met, and just read what we
	// are told. Records longer than requested are truncated.
	if (NULL != pStr) {
		pBase = pStr;
		while (*pStr) {
			// check for buffer resize
			if ((pFile->pData+pFile->nDataSize) - pData < (pFile->RecordLength+2)*10) {
				int nOffset = pData - pFile->pData;		// in case the buffer moves
				// time to grow the buffer - add another 100 lines - the +1 is already in there
				// so we don't need it here.
				pFile->nDataSize += (100) * (pFile->RecordLength + 2);
                unsigned char *pTmp = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
                if (NULL == pTmp) {
                    debug_write("Clipboard buffer failed to reallocate memory, failing.");
		            // No, we failed - most likely reason is someone else has it open for write
		            pFile->LastError = ERR_FILEERROR;
		            return false;
	            }
				pFile->pData  = pTmp;
				pData = pFile->pData + nOffset;
			}

			// clear the buffer in case the read is short
			memset(pData+2, ' ', pFile->RecordLength);

			// The final +1 is so we can read a terminating NUL safely at the end
			// NULs in the middle are overwritten by the next record
			// read until we get a CRLF, a NUL, or pFile->RecordLength+1
			// This little block emulates fgets()
			unsigned char *pOutDat = pData+2;
			for (int idx=0; idx<pFile->RecordLength+1; idx++) {
				*(pOutDat++) = *(pStr++);
				if (*(pOutDat-1) == '\0') {
					pStr--;		// make sure it's detected next loop
					break;
				}
				if (*(pOutDat-1) == '\n') {
					break;
				}
			}
			*pOutDat='\0';
			
			// remove EOL
			bool bEOLFound = false;
			while ((pTmp = strchr((char*)(pData+2), '\r')) != NULL) {
				*pTmp='\0';
				bEOLFound=true;
			}
			while ((pTmp = strchr((char*)(pData+2), '\n')) != NULL) {
				*pTmp='\0';
				bEOLFound=true;
			}
			*(unsigned short*)pData = (unsigned short)strlen((char*)(pData+2));
			// we don't want that NUL other
			pTmp=strchr((char*)(pData+2), '\0');
			if (NULL != pTmp) {
				// TI pads with spaces - especially need this for cases of fixed length
				*pTmp=' ';
			}
			pData+=pFile->RecordLength+2;
			pFile->NumberRecords++;
			if (!bEOLFound) {
				// we didn't read the whole line, so skim until we do
				// end of line should be \r\n, so just \n should be enough
				while (*pStr) {
					if (*(pStr++) == '\n') {
						break;
					}
				}
			}
		}
		pStr = pBase;
		free(pStr);
	}

	// set up the file info structure
	// calculate necessary data and fill in header fields
	// Most of this is probably meaningless here, but that's okay, we'll keep it
	pFile->RecordsPerSector = pFile->NumberRecords;
	pFile->LengthSectors = 1;		// one big ass sector!
	pFile->FileType = 0;
	if (pFile->Status & FLAG_VARIABLE) pFile->FileType|=TIFILES_VARIABLE;
	if (pFile->Status & FLAG_INTERNAL) pFile->FileType|=TIFILES_INTERNAL;
	pFile->BytesInLastSector = 0;

	debug_write("Clipboard read %d records", pFile->NumberRecords);
	return true;
}

// Write the disk buffer out to the file, with appropriate modes and headers
// A couple of extra copies are made, so very large files may be painful.
// Of course, they shouldn't be going to the clipboard then, should they?
bool ClipboardDisk::Flush(FileInfo *pFile) {
	CString csOut;
	HGLOBAL hGlob;

	// first make sure this is an open file!
	if (!pFile->bDirty) {
		// nothing to flush, return success anyway
		return true;
	}

	// figure out what we are writing and call the appropriate code
	// It doesn't matter what the file was read as, we can write it as
	// whatever is configured. Technically the open mode was already
	// checked so we can just go ahead.
	if (!OpenClipboard(m_hWnd)) {
		// No, we failed
		debug_write("Failed to open clipboard, err %d", GetLastError());
		// most likely reason is someone else has it open for write
		pFile->LastError = ERR_FILEERROR;
		return false;
	}
	// we need to empty the clipboard to take possession of it
	if (!EmptyClipboard()) {
		debug_write("Failed to empty clipboard, err, %d, can't write.", GetLastError());
		pFile->LastError = ERR_WRITEPROTECT;
		CloseClipboard();
		return false;
	}

	// The assumption is made that these are valid text strings
    // if suppressCR is set, then we check for explicit
    // CRs or LFs, and write the end of line only then
	unsigned char *pData = pFile->pData;
	char tmp[256];
	if (NULL == pData) {
		debug_write("Warning: no data to flush.");
	} else {
		// get it all into one string, first
		csOut="";
		for (int idx=0; idx<pFile->NumberRecords; idx++) {
			if (pData-pFile->pData >= pFile->nDataSize) {
				break;
			}
			int nLen = *(unsigned short*)pData;
			pData+=2;

			memset(tmp, 0, sizeof(tmp));
			memcpy(tmp, pData, nLen);
			csOut+=tmp;
			pData+=pFile->RecordLength;
            if (!suppressCR) csOut+="\r\n";
		}
        // normalize the end of line if suppressCR was set
        if (suppressCR) {
            // order matters here - tokenize possible strings, then
            // change the tokens into correct eol
            csOut.Replace("\r\n","\x1");    // using '1' as a token code
            csOut.Replace('\r', '\x1');
            csOut.Replace('\n', '\x1');
            csOut.Replace("\x1", "\r\n");
        }

		// now copy into a global buffer and give it to the clipboard
		hGlob = GlobalAlloc(GMEM_MOVEABLE, csOut.GetLength()+1);
		if (NULL == hGlob) {
			debug_write("Can't allocate global memory");
			CloseClipboard();
			pFile->LastError = ERR_FILEERROR;
			return false;
		}
		char *pStr = (char*)GlobalLock(hGlob);
		if (NULL == pStr) {
			debug_write("Can't lock global memory");
			CloseClipboard();
			GlobalFree(hGlob);
			pFile->LastError = ERR_FILEERROR;
			return false;
		}
		memcpy(pStr, csOut.GetBuffer(), csOut.GetLength());
		*(pStr+csOut.GetLength())='\0';
		GlobalUnlock(pStr);
		SetClipboardData(CF_TEXT, hGlob);
		hGlob=NULL;		// not ours anymore
	}
	CloseClipboard();

	debug_write("Flushed %d records to clipboard", pFile->NumberRecords);
	pFile->bDirty = false;

	return true;
}

// Open a file with a particular mode, creating it if necessary
// Clipboard will be DVxx or DFxx (record length of 0 is set to 254)
// Return the FileInfo object actually allocated for use, or NULL on failure.
FileInfo *ClipboardDisk::Open(FileInfo *pFile) {
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
		pFile->RecordLength=254;
	}

    // check for CR/LF options (we treat NOCR or NOLF as the option in question)
    // only ONE may be provided. There's only one clipboard so I suppose this
    // global flag for this open should be okay. ;)
    if ((pFile->csName=="NOCR") || (pFile->csName=="NOLF")) {
        debug_write("Clipboards writes will suppress end of line");
        suppressCR = true;
    } else {
        suppressCR = false;
    }

	// let's see what we are doing here...
	switch (pFile->Status & FLAG_MODEMASK) {
		case FLAG_OUTPUT:
			if (!CreateOutputFile(pFile)) {
				goto error;
			}
			break;

		// we can open these both the same way
		case FLAG_UPDATE:
		case FLAG_APPEND:
			// First, try to open the file
			if (!TryOpenFile(pFile)) {
				// No? Try to create it then?
				if (pFile->LastError == ERR_FILEERROR) {
					if (!CreateOutputFile(pFile)) {
						// Still no? Error out then (save error already set)
						goto error;
					}
                    // don't buffer a newly created file, handle like OUTPUT
                    break;
				} else {
					goto error;
				}
			}

			// So we should have a file now - read it in
			if (!BufferFile(pFile)) {
				pFile->LastError = ERR_FILEERROR;
				goto error;
			}
			break;

		default:	// should only be FLAG_INPUT
			if (!TryOpenFile(pFile)) {
				// Error out then, must exist for input (save old error)
				goto error;
			}

			// So we should have a file now - read it in
			if (!BufferFile(pFile)) {
				pFile->LastError = ERR_FILEERROR;
				goto error;
			}
			break;
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

