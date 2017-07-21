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
//*****************************************************
//* Classic 99 - TI Emulator for Win32				  *
//* by M.Brent                                        *
//* Disk support routines                             *
//*****************************************************

// observation of behaviour from the TI controller card:
// 
//On entry to the DSR, as documented, >8354 has the length of the device/filename, 
//and >8356 points to the first period (or end of name) in VDP RAM.
//
//On return from the TI Controller DSR, >8354 points to the original PAB in VDP. 
//I have looked at it and I am fairly sure that the PAB remains intact (and one would expect it to be!)
//
//>8356 points to the file buffer copy of the file descriptor record copied from the disk 
//for the file loaded. The FDR naturally begins with the filename (but NOT the device name). 
//However, because the file is closed, the first byte was zeroed out. So the first bytes in 
//VDP for the filename "ABCD" are "<00>BCD".
//
//That explains behavior and why the (dino animation) program duplicates the first character. 
//I would have to guess the author didn't know that >8354 pointed at the original PAB (where the 
//full device+filename still lives) and didn't want to hard code it. (Seems to me there are documented 
//addresses in low RAM that also have DSRLNK results, aren't there?)
//
//Anyway, that's what's going on! I'm inclined to think that anything relying on this is relying on 
//side-effects specific to the TI controller, but I will definitely hang onto the documentation. :)
//
// Side-effects should be accomodated in Classic99, though. They should not WORK, but it should
// be clear that something is wrong that may break on other disk controllers. SO, Classic99 needs
// to damage >8354 and >8356 on return. It also needs to scramble scratchpad and VDP addresses that
// the TI disk controller would modify. My suggestion is to fill with >4A bytes -- this is a clear
// value that shows things have changed, but it's also not useful, to avoid relying on side effects.
//
// A debug option can turn the munging off, but it should be ON by default. This helps prevent
// software that ONLY works in Classic99.


// Includes
#include <stdio.h>
#include <windows.h>
#include <atlstr.h>
#include <FCNTL.H>
#include <io.h>
#include "tiemul.h"
#include "cpu9900.h"
#include "diskclass.h"
#include "fiaddisk.h"
#include "ImageDisk.h"
#include "TICCDisk.h"
#include "clipboarddisk.h"
#include "clockdisk.h"

BaseDisk *pDriveType[MAX_DRIVES];
CRITICAL_SECTION csDriveType;

extern struct DISKS *pMagicDisk;
extern CPU9900 * volatile pCurrentCPU;

// configuration option to configure blocks of RAM after DSR calls to make sure your
// program isn't using illegal memory - either as storage OR trying to reach into
// the disk controller operation, both are prohibited. This is /ON/ by default.
bool bCorruptDSKRAM = false;			// NOTE: DO NOT use this with the REAL DSR!!
int ScratchPadCorruptList[] = {
	// calling workspace offsets
	0x0000,0x0013,		// R0-R10
	// we can wipe this block EXCEPT for >8354,8356, which point to the PAB and are widely assumed untouched
	// addresses people have been observed peeking into that don't work everywhere already!
	//0x83d0,0x83d3,	set in DSRLNK and often used for boot tracking 83D0-CRU, 83D2-DSR entry address
	// from the File Management Specification from TI
	0x83da,0x83df,
	0x834a,0x836d,
	// and the TI disk DSR uses this block in some cases, too!
	//0x8310,0x831f,		// we can't corrupt it, it's only used in special cases, and the E/A assumes it is NOT modified
	-1,-1
};

// function prototypes
void LoadOneImg(struct IMG *pImg, char *szFork);
bool TryLoadMagicImage(FileInfo *pFile);
void do_files(int n);
void do_dsrlnk();
void do_sbrlnk(int nOpCode);
void setfileerror(FileInfo *pFile);
void GetFilenameFromVDP(int nName, int nMax, FileInfo *pFile);
extern Byte GetSafeCpuByte(int x, int bank);
void WriteMemoryByte(Word address, Byte value, bool allowWrite);

// do some initial housekeeping
void InitDiskDSR() {

	EnterCriticalSection(&csDriveType);

	for (int idx=0; idx<MAX_DRIVES; idx++) {
		pDriveType[idx]=NULL;
	}

	// create the reserved drives
	pDriveType[CLIP_DRIVE_INDEX] = new ClipboardDisk(myWnd);
	pDriveType[CLOCK_DRIVE_INDEX] = new ClockDisk();

	LeaveCriticalSection(&csDriveType);
}

// note: for future use -- allow mounting internal, TI, and Corcomp disk controller
// at CRU >1100. If the user allows both internal and TI, we simply use the TI/Corcomp
// DSR ROM for any drive not configured Classic99's DSR
// Return true if no error (or if startup routine)
// These addresses are hard-coded in AMI99DSK.BIN to make the emulator jump to them
// This code also handles Clipboard access now (text-only read/write device)
// For a normal DSR entry, >8356 must point to the period in the filename of the PAB,
// and >8354 (as a word) contains the length of the devicename.
bool HandleDisk() {
	// our DSR routines start at 0x4800 to reduce the odds of accidental conflict
	// with errors or functions jumping blindly into disk DSR code.
	EnterCriticalSection(&csDriveType);

	const Word PC = pCurrentCPU->GetPC();

	switch (PC) {
	case 0x4800:	// dsr entry
		do_dsrlnk();
		LeaveCriticalSection(&csDriveType);
		return true;

	case 0x4820:	// sbr 0x10
	case 0x4822:	// sbr 0x11
	case 0x4824:	// sbr 0x12
	case 0x4826:	// sbr 0x13
	case 0x4828:	// sbr 0x14
	case 0x482a:	// sbr 0x15
	case 0x482c:	// sbr 0x16
		// SBR entries for disk (calculate the opcode based on the entry point - see DSR header)
		// Technically we didn't need to do this, I guess, we could have just looked at the
		// actual name the program used. Oh well, just a little extra search on the 9900 side which
		// the real machine would do anyway.
		do_sbrlnk((PC-0x4820)/2 + 0x10);
		LeaveCriticalSection(&csDriveType);
		return true;

	// handled here (non-PAB based stuff)
	case 0x4810:
		// Powerup entry for disk - no PAB here.
		// Note that we emulate one "super-controller" so we have to make sure we
		// only do certain parts of powerup once.
		BaseDisk::ResetPowerup();
		for (int idx=0; idx<MAX_DRIVES; idx++) {
			if (NULL != pDriveType[idx]) {
				pDriveType[idx]->Startup();
			}
		}
		{
			bool bTICC = false;
			for (int idx=0; idx<MAX_DRIVES; idx++) {
				if (NULL != pDriveType[idx]) {
					if (DISK_TICC == pDriveType[idx]->GetDiskType()) {
						bTICC = true;
						break;
					}
				}
			}
			if (!bTICC) do_files(3);
		}
		LeaveCriticalSection(&csDriveType);
		return false;		// doesn't increment the return address

	case 0x4880:
		// CALL FILES entry for disk
		// This goes through the motions, though we don't really need it ;)
		// We need to do some work with the BASIC program to get past the token
		{
			int x=romword(0x832c);		// get next basic token
			x+=7;						// skip "FILES"
			int y=(VDP[x]<<8)|VDP[x+1];	// get two bytes (size of string)
			if (y == 0xc801) {
				// c8 means unquoted string, 1 is the length
				x+=2;						// increment pointer
				y=VDP[x]-0x30;				// this is the number of files in ASCII
				if ((y <= 9) && (y >= 0)) {
					// valid count - set all devices - 0 is only okay for my devices, not a real controller, and gives you the CS1 VRAM layout
					do_files(y);
					
					if (nDSRBank[1] == 0) {
						// if not running the TICC, try to skip the rest of the statement
						x+=3;
						wrword(0x832c, x);			// write new pointer
						wcpubyte(0x8342, 0);		// clear 'current' token
					}
				}
			}
		}

		LeaveCriticalSection(&csDriveType);
		return true;
	}

	// some other address?
	debug_write("Unexplained DSR access to 0x%04X, failing (crash imminent?)", PC);
	LeaveCriticalSection(&csDriveType);
	return false;
}

// Helper to enumerate the configured disks and try to match a disk name
int FindDiskName(CString csDiskName) {
	if (csDiskName == BAD_DISK_NAME) {
		// I don't know how or why you did this, but I'll warn you that you're WEIRD!
		debug_write("Can not search for bad disk name '%s'", (LPCSTR)csDiskName);
		return -1;
	}

	for (int nDrive = 0; nDrive < MAX_DRIVES-RESERVED_DRIVES; nDrive++) {
		if (NULL != pDriveType[nDrive]) {
			// we have a drive
			CString csTestName = pDriveType[nDrive]->GetDiskName();
			if (csTestName != BAD_DISK_NAME) {
				if (csDiskName.CompareNoCase(csTestName) == 0) {
					// and we have a match
					return nDrive;
				}
			}
		}
	}

	// no match
	return -1;
}

///////////////////////////////////////////////////////////////////////
// Base entry for standard DSR call
///////////////////////////////////////////////////////////////////////
void do_dsrlnk() {
	// performs file i/o using the PAB passed as per a normal dsrlnk call.
	// address of length byte passed in CPU >8356
	int PAB;
	FileInfo tmpFile, *pWorkFile;
	int nDrive, nLen;
	bool bDiskByName = false;		// true for DSK.DISKNAME.FILE - only needed for TICC

	if (pCurrentCPU->GetWP() != 0x83E0) {
		debug_write("Warning: Calling DSR without setting GPLWS may not work on hw (WP=0x04X)!", pCurrentCPU->GetWP());
	}
	if (romword(0x83d0) != 0x1100) {	// warning: hard-coded CRU base
		debug_write("Warning: DSRLNK functions should store the CRU base of the device at >83D0! (Got >%04X)", romword(0x83d0));
	}
	// steal nLen for a quick test...
	nLen = romword(0x83d2);
	if (romword(nLen+2) != pCurrentCPU->GetPC()) {	// a hacky check that we are probably at the right place in the header
		debug_write("Warning: DSRLNK functions should store the DSR address of the device name entry at >83D2! (Got >%04X)", romword(0x83d2));
	}

	// get the base address of PAB in VDP RAM
	PAB = romword(0x8356);		// this points to the character AFTER the device name (the '.' or end of string)
	// since there must be 10 bytes before the name, we can do a quick early out here...
	if (PAB < 13) {		// 10 bytes, plus three for shortest device name "DSK"
		debug_write("Bad PAB address >%04X", PAB);		// not really what the TI would do, but it's wrong anyway.
		// we can't set the error, because we'd underrun! (real TI would wrap around the address)
		return;
	}

	// verify mandatory setup makes some form of sense - for debugging DSR issues
	{
		static int nLastOp = -1;
		static int nLastPAB = -1;	// reduce debug by reducing repetition

		int nLen = romword(0x8354);
		if (nLen > 7) {
			debug_write("Warning: bad DSR name length %d in >8354", nLen);
			// this is supposed to tell us where to write the error!?
			return;
		}

		int nOpcode=VDP[PAB-nLen-10];
		if ((nOpcode < 0) || (nOpcode > 9)) {
			debug_write("Warning: bad Opcode in PAB at VDP >%04X (>%02X)", PAB-nLen-10,nOpcode);
			TriggerBreakPoint();
			// since we don't have a structure yet, set it manually
			VDP[PAB-nLen-10+1] &= 0x1f;					// no errors
			VDP[PAB-nLen-10+1] |= ERR_BADATTRIBUTE<<5;	// file error
			return;
		}

		int nNameLen = VDP[PAB-nLen-1];
		if (PAB-nLen+nNameLen > 0x3fff) {
			debug_write("PAB name too long (%d) for VDP >%04X", nNameLen, PAB);		// not really what the TI would do, but it's wrong anyway.
			// since we don't have a structure yet, set it manually
			VDP[PAB-nLen-10+1] &= 0x1f;					// no errors
			VDP[PAB-nLen-10+1] |= ERR_BADATTRIBUTE<<5;	// file error
			return;
		}

		if ((nLastOp!=nOpcode)||(nLastPAB != PAB)||(nOpcode == 0)) {
			char buf[32];
			if (nNameLen > 32) {
				debug_write("Excessively long filename (%d bytes) probably won't work on all devices", nNameLen);
				nNameLen=31;
			}
			memset(buf, 0, sizeof(buf));
			memcpy(buf, &VDP[PAB-nLen], nNameLen);
			debug_write("DSR opcode >%d on PAB >%04X, filename %s", nOpcode, PAB, buf);
			nLastOp = nOpcode;
			nLastPAB = PAB;
		}
	}

	// finally, check the VDP buffer header is intact
	verifyCallFiles();

	// You can make assumptions when the device name is always the same length, but
	// we have a few, so we need to work out which one we had, then subtract an additional 10 bytes.
	PAB -= 4;					// enough to differentiate which we have (DSKx and CLIP are both 4 chars)
	if (0 == _strnicmp("clock", (const char*)&VDP[PAB]-1, 5)) PAB--;		// must be CLOCK, subtract one more
	if (0 == _strnicmp("dsk.", (const char*)&VDP[PAB]+1, 4)) PAB++;			// if DSK., then add one (this is kind of hacky)
	PAB-=10;					// back up to the beginning of the PAB
	PAB&=0x3FFF;				// mask to VDP memory range

	// The DSR limited the names that can be passed in here, so it's okay to
	// make some assumptions.!
	// TODO: It's TI code that decides if we get called, so it's actually NOT okay,
	// we need to actually verify the name. I had a DSRLNK that passed a block of
	// zero bytes and ended up accessing the CLIP device instead of failing.
	// Besides DSK0-9, we have:
	// 10 - Clipboard "CLIP"
	// 11 - Clock "CLOCK"
	// ?? - Diskname "DSK"
	if (isdigit(VDP[(PAB+13)&0x3FFF])) {
		nDrive = VDP[(PAB+13)&0x3FFF] - '0';
	} else {
		// differentiate non-numbered names
		if ((VDP[(PAB+13)&0x3fff] == '.') || (VDP[(PAB+13)&0x3fff]&0x80)) {		// TI DSR allows high ascii as a separator
			// it must be DSK.DISKNAME.FILENAME, so work out which disk the user wants
			CString csDiskName;
			int pos = PAB+14;
			int nLen = VDP[(PAB+9)&0x3fff] - 4;		// 4 for 'DSK.'
			while ((VDP[pos&0x3fff] != '.') && ((VDP[pos&0x3fff]&0x80) == 0)) {
				csDiskName+=VDP[pos&0x3fff];
				pos++;
				nLen--;
				if (nLen == 0) break;
			}
			nDrive = FindDiskName(csDiskName);
			if (nDrive == -1) {
				debug_write("Diskname '%s' was not found.", csDiskName);
				// since we don't have a structure yet, set it manually
				VDP[PAB+1] &= 0x1f;					// no errors
				VDP[PAB+1] |= ERR_BADATTRIBUTE<<5;	// file error
				return;
			}
			bDiskByName = true;
		} else if (toupper(VDP[(PAB+12)&0x3fff]) == 'O') {
			nDrive = CLOCK_DRIVE_INDEX;		// clOck
		} else {
			nDrive = CLIP_DRIVE_INDEX;		// clIp
		}
	}

	// technically, it should not be possible for this to be illegal, but just in case
	if ((nDrive < 0) || (nDrive >= MAX_DRIVES)) {
		debug_write("Access to illegal drive DSK%d", nDrive);
		// since we don't have a structure yet, set it manually
		VDP[PAB+1] &= 0x1f;					// no errors
		VDP[PAB+1] |= ERR_BADATTRIBUTE<<5;	// file error
		return;
	}

	// save the drive index
	tmpFile.nDrive = nDrive;

	// copy the PAB data into the tmpFile struct
	tmpFile.PABAddress = PAB;
	tmpFile.OpCode = VDP[PAB++];							// 0
	PAB&=0x3FFF;
	tmpFile.Status = VDP[PAB++];							// 1
	PAB&=0x3FFF;
	tmpFile.DataBuffer = (VDP[PAB]<<8) | VDP[PAB+1];		// 2,3
	PAB+=2;
	PAB&=0x3FFF;
	tmpFile.RecordLength = VDP[PAB++];						// 4
	PAB&=0x3FFF;
	tmpFile.CharCount = VDP[PAB++];							// 5
	PAB&=0x3FFF;
	tmpFile.RecordNumber = (VDP[PAB]<<8) | VDP[PAB+1];		// 6,7	It's not relative vs sequential, it's variable (not used) vs fixed (used)
	PAB+=2;
	PAB&=0x3FFF;
	tmpFile.ScreenOffset = VDP[PAB++];						// 8
	PAB&=0x3FFF;
	nLen = VDP[PAB++];										// 9
	if ((VDP[(PAB+3)&0x3fff] == '.') || (VDP[(PAB+3)&0x3fff]&0x80)) {
		// skip the DSK.DISKNAME.
		PAB+=4;
		PAB&=0x3fff;
		nLen-=4;
		while ((VDP[PAB] != '.') && ((VDP[PAB] & 0x80) == 0)) {
			PAB = (++PAB)&0x3fff;
			if (--nLen == 0) break;
		}
		PAB = (++PAB)&0x3fff;		// skip the '.' too
		--nLen;
	} else {
		PAB+=5;			// skip the "DSKx."						// 10-14
		PAB&=0x3FFF;
		nLen -= 5;		// "DSKx."
	}
	GetFilenameFromVDP(PAB, nLen, &tmpFile);

	// somewhat annoying, but we need to try and keep the FileType (TIFILES) and Status (PAB)
	// in sync, so map Status over
	tmpFile.FileType=0;
	if (tmpFile.Status & FLAG_VARIABLE) tmpFile.FileType |= TIFILES_VARIABLE;
	if (tmpFile.Status & FLAG_INTERNAL) tmpFile.FileType |= TIFILES_INTERNAL;

	// now see if we can find a disk driver attached to this drive
	if (NULL == pDriveType[nDrive]) {
		// nope, none!
		debug_write("No disk driver attached for DSK%d", nDrive);
		// Bad drive = file error
		tmpFile.LastError = ERR_FILEERROR;	// we can not use error code 0 because it's too late at this point, it won't detect as an error for some ops!
		setfileerror(&tmpFile);
		// set COND bit for non-existant DSR - who knows? :) Someone might use it!
		wcpubyte(0x837c, rcpubyte(0x837c) | 0x20); 
		return;
	}
	if (DISK_TICC == pDriveType[nDrive]->GetDiskType()) {
		// it's a TI disk controller! We have to handle it special
		TICCDisk *pDisk = (TICCDisk*)pDriveType[nDrive];
		if (bDiskByName) {
			pDisk->dsrlnk(-1);
		} else {
			pDisk->dsrlnk(nDrive);
		}
		return;
	}

	// split name into options and name
	tmpFile.SplitOptionsFromName();

	// See if we can find an existing FileInfo for this request, and use it instead
	pWorkFile=pDriveType[nDrive]->FindFileInfo(tmpFile.csName);
	if (NULL == pWorkFile) {
		// none was found, just use what we have, hopefully it's an existing file
		pWorkFile=&tmpFile;
	} else {
		// XB is bad about not closing its files, so if this is an open request,
		// then just assume we should close the old file and do it again. Usually this
		// happens only if an error occurs while loading a program. TODO: how did TI deal with this?
		if (tmpFile.OpCode == OP_OPEN) {
			// discard any changes to the old file, it was never closed
			pWorkFile->bDirty = false;
			pDriveType[nDrive]->Close(pWorkFile);
			debug_write("Recycling unclosed file buffer %d", pWorkFile->nIndex);
			// and then use tmpFile as if it were new
			pWorkFile = &tmpFile;
		} else {
			// it was found, so it's open. Verify the header/mode parameters match what we have
			// then go ahead and swap it over
			// We don't care on a CLOSE request
			if (tmpFile.OpCode != OP_CLOSE) {
				if ((pWorkFile->Status&FLAG_TYPEMASK) != (tmpFile.Status&FLAG_TYPEMASK)) {
					debug_write("File mode/status does not match open file for DSK%d.%s (0x%02X vs 0x%02X)", nDrive, (LPCSTR)tmpFile.csName, pWorkFile->Status&0x1f, tmpFile.Status&0x1f);
					pWorkFile->LastError = ERR_BADATTRIBUTE;
					setfileerror(pWorkFile);
					return;
				}
				if (pWorkFile->RecordLength != tmpFile.RecordLength) {
					debug_write("File record length does not match open file for DSK%d.%s (%d vs %d)", nDrive, (LPCSTR)tmpFile.csName, pWorkFile->RecordLength, tmpFile.RecordLength);
					pWorkFile->LastError = ERR_BADATTRIBUTE;
					setfileerror(pWorkFile);
					return;
				}
			}
			
			// okay, that's the important stuff, copy the request data we got into the new one
			pWorkFile->CopyFileInfo(&tmpFile, true);
		}
	}

	// make sure the last error is cleared
	pWorkFile->LastError = ERR_NOERROR;

#define HELPFULDEBUG(x) debug_write(x " DSK%d.%s on drive type %s", nDrive, (LPCSTR)pWorkFile->csName, pDriveType[nDrive]->GetDiskTypeAsString());
#define HELPFULDEBUG1(x,y) debug_write(x " DSK%d.%s on drive type %s", y, nDrive, (LPCSTR)pWorkFile->csName, pDriveType[nDrive]->GetDiskTypeAsString());
	switch (pWorkFile->OpCode) {
		case OP_OPEN:
			{
				FileInfo *pNewFile=NULL;

				HELPFULDEBUG("Opening");

				// check for disk write protection on output or append (update will come if a write is attempted)
				if (pDriveType[nDrive]->GetWriteProtect()) {
					if (((pWorkFile->Status & FLAG_MODEMASK) == FLAG_OUTPUT) ||
						((pWorkFile->Status & FLAG_MODEMASK) == FLAG_APPEND)) {
							debug_write("Attempt to write to write-protected disk.");
							pWorkFile->LastError = ERR_WRITEPROTECT;
							setfileerror(pWorkFile);
							break;
					}
				}

				// a little more info just for opens
				debug_write("PAB requested file type is %c%c%d", (pWorkFile->Status&FLAG_INTERNAL)?'I':'D', (pWorkFile->Status&FLAG_VARIABLE)?'V':'F', pWorkFile->RecordLength);
				pNewFile = pDriveType[nDrive]->Open(pWorkFile);
				if ((NULL == pNewFile) /*|| (!pNewFile->bOpen)*/) {
					// the bOpen check is to make STATUS able to use Open to check PROGRAM files
					pNewFile = NULL;
					setfileerror(pWorkFile);
				} else {
					// if the user didn't specify a RecordLength in the PAB, we should do that now
					if ((VDP[pWorkFile->PABAddress+4] == 0) && (pNewFile->RecordLength != 0)) {
						VDP[pWorkFile->PABAddress+4]=pNewFile->RecordLength;
					}

					// ALWAYS performs a rewind operation , which has this effect (should be zeroes)
					pDriveType[nDrive]->Restore(pWorkFile);		// todo: should we check for error?
					VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
					VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;

					// The pointer to the new object is needed since that's what the derived
					// class updated/created. (This fixes Owen's Wycove Forth issue)
					pNewFile->nCurrentRecord = 0;
					pNewFile->bOpen = true;
					pNewFile->bDirty = false;	// can't be dirty yet!
				}
			}
			break;

		case OP_CLOSE:
			// we should check for open here, but safer to just always try to close
			HELPFULDEBUG("Closing");
			if (!pDriveType[nDrive]->Close(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_READ:
			if ((!pWorkFile->bOpen) ||
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_OUTPUT) || 
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_APPEND)) {
				debug_write("Can't read from %s as file is not opened for read.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			if (!pDriveType[nDrive]->Read(pWorkFile)) {
				HELPFULDEBUG1("Failed reading max %d bytes", pWorkFile->RecordLength);
				setfileerror(pWorkFile);
			} else {
				// need to copy the char count back to the PAB if fixed
				VDP[tmpFile.PABAddress+5] = pWorkFile->CharCount;
				if (0 == (tmpFile.Status & FLAG_VARIABLE)) {
					VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
					VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;
				}
			}
			break;

		case OP_WRITE:
			if ((!pWorkFile->bOpen) || ((pWorkFile->Status & FLAG_MODEMASK) == FLAG_INPUT)) {
				debug_write("Can't write to %s as file is not opened for write.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			// check for disk write protection
			if (pDriveType[nDrive]->GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}

			if (!pDriveType[nDrive]->Write(pWorkFile)) {
				HELPFULDEBUG1("Failed writing %d bytes", pWorkFile->CharCount);
				setfileerror(pWorkFile);
			} else {
				// mark it dirty
				pWorkFile->bDirty = true;
				if (0 == (tmpFile.Status & FLAG_VARIABLE)) {
					VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
					VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;
				}
			}
			break;

		case OP_RESTORE:
			if ((!pWorkFile->bOpen) ||
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_OUTPUT) || 
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_APPEND)) {
				debug_write("Can't restore %s as file is not opened for read.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			// the basics of this operation is copied in OPEN, so if you fix something,
			// also look there.
			HELPFULDEBUG1("Restoring record %d", pWorkFile->RecordNumber);
			if (!pDriveType[nDrive]->Restore(pWorkFile)) {
				setfileerror(pWorkFile);
			} else {
				// update the PAB, all file types (verified via TICC emulation)
				VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
				VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;
			}
			break;

		case OP_LOAD:
			HELPFULDEBUG1("Loading to VDP >%04X", pWorkFile->DataBuffer);
			if (!pDriveType[nDrive]->Load(pWorkFile)) {
				if (!TryLoadMagicImage(pWorkFile)) {
					setfileerror(pWorkFile);
				}
			}
			break;

		case OP_SAVE:
			HELPFULDEBUG1("Saving from VDP >%04X", pWorkFile->DataBuffer);
			
			// check for disk write protection
			if (pDriveType[nDrive]->GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}

			if (!pDriveType[nDrive]->Save(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_DELETE:
			HELPFULDEBUG("Deleting");
			// check for disk write protection
			if (pDriveType[nDrive]->GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}
			if (!pDriveType[nDrive]->Delete(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_SCRATCH:
			if (!pWorkFile->bOpen) {
				debug_write("Can't scratch in %s as file is not opened.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			HELPFULDEBUG1("Scratching record %d", pWorkFile->RecordNumber);

			// check for disk write protection
			if (pDriveType[nDrive]->GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}

			if (!pDriveType[nDrive]->Scratch(pWorkFile)) {
				setfileerror(pWorkFile);
			} else {
				// mark it dirty
				pWorkFile->bDirty = true;
			}
			break;

		case OP_STATUS:
			pWorkFile->ScreenOffset = 0;
			if (!pDriveType[nDrive]->GetStatus(pWorkFile)) {
				setfileerror(pWorkFile);
			} else {
				// write the result back to the PAB
				VDP[tmpFile.PABAddress+8] = pWorkFile->ScreenOffset;
			}
			HELPFULDEBUG1("Status returns >%02X on", VDP[tmpFile.PABAddress+8]);
			break;

		default:
			HELPFULDEBUG1("Unknown DSRLNK opcode %d", pWorkFile->OpCode);
			pWorkFile->LastError = ERR_BADATTRIBUTE;
			setfileerror(pWorkFile);		// Bad open attribute
			break;
	}
#undef HELPFULDEBUG
#undef HELPFULDEBUG1

	if (bCorruptDSKRAM) {
		// before we return, deliberately corrupt memory known used by the TI disk controller
		// with a fixed pattern to help developers recognize when they are relying on data
		// that may not be there in all cases. This is not perfect, but should help compatibility
		// with various disk devices by making it harder to rely on side effects and should
		// help developers write code that works with real hardware (as Classic99's DSR
		// is way too permissive - it just works with whatever. Even /I/ get bit. ;) )

		// I'll use 0x4A for my wipe byte (so it says HA HA HA HA HA ;) )
		// corrupt VDP above the top of VRAM pointer
		int vp=(GetSafeCpuByte(0x8370, 0)<<8)|GetSafeCpuByte(0x8371, 0)+1;
		bool warned=false;
		// check the header
		if (VDP[vp++] != 0xaa) {	// header byte
			debug_write("Disk buffer header corrupted at >%04X", vp);
		}
		if (VDP[vp++] != 0x3f) {	// TODO: top of VRAM MSB
			debug_write("Disk buffer header corrupted at >%04X", vp);
		}
		if (VDP[vp++] != 0xff) {	// TODO: top of VRAM LSB
			debug_write("Disk buffer header corrupted at >%04X", vp);
		}
		if (VDP[vp++] != 0x11) {	// Disk CRU
			debug_write("Disk buffer header corrupted at >%04X", vp);
		}
		if (VDP[vp++] != 0x03) {	// TODO: number of open files - we don't track this so we can't check it. (still need the increment!)
//			debug_write("Disk buffer header corrupted at >%04X", vp);
		}

		// check and wipe the rest
		for (int idx=vp; idx<0x3fff; idx++) {
			if (!warned) {
				if (VDP[idx] != 0x4a) {
					debug_write("Found and overwrote data in VDP reserved space at >%04X", idx);
					warned=true;
				}
			}
			VDP[idx]=0x4a;
		}
		// TODO: also mangle scratchpad addresses that we know are used - by TI at least!
		// experimental, these ones!
		int p = 0;
		while (ScratchPadCorruptList[p]!=-1) {
			int start=ScratchPadCorruptList[p];
			int end=ScratchPadCorruptList[p+1];
			if (start<0x8300) {
				start+=pCurrentCPU->GetWP();
				end+=pCurrentCPU->GetWP();
			}
			for (int idx=start; idx<=end; idx++) {
				// We can't /test/ them, though, because the rest of the system uses them a lot too.
				WriteMemoryByte(idx, 0x4a, false);
			}
			p+=2;
		}
	}
}

// helper function to get csName from VDP
void GetFilenameFromVDP(int nName, int nMax, FileInfo *pFile) {
	// get the filename from VDP
	pFile->csName = "";
	for (int idx=0; idx<nMax; idx++) {
		// wraparound address
		while (nName >= 0x4000) {
			nName-=0x4000;
		}
		if (VDP[nName] == ' ') {
			break;
		}
		if (VDP[nName] & 0x80) {
			// TI disk controller allows high ASCII as a separator
			pFile->csName += '.';
		} else {
			pFile->csName += VDP[nName++];
		}
	}
	// disk directories should not work unless they end with a period, so,
	// if the filename is blank, copy the period over so the controller can check
	if (pFile->csName.GetLength() == 0) {
		// not even bothering if you try to do a wraparound name...
		if ((nName>0) && (VDP[nName-1] == '.')) {
			pFile->csName += '.';
		}
	}
	// this should already be true for subdirectories, which I still need to work out...
}

/////////////////////////////////////////////////////////////////////////
// Perform low level disk call
/////////////////////////////////////////////////////////////////////////
void do_sbrlnk(int nOpCode) {
	// data is passed in without a PAB, using preset addresses 
	// in scratchpad. The TI disk DSR introduced these functions,
	// so many other disk devices copied them.
	// Each subprogram has its own data.
	// NOTE: Do NOT corrupt DSK RAM after these calls, tools like
	// the P-Code card use their own disk access through sector access,
	// and expect VDP RAM to be untouched.
	FileInfo tmpFile;

	if (nOpCode == SBR_FILES) {
		// call files doesn't use a specific drive, so check it here
		do_files(-(rcpubyte(0x834c)));
		// we don't have an error code for this one, even if it was bad!
		wcpubyte(0x8350, ERR_NOERROR);
		return;
	}

	// the only common value, used by all but SBR_FILES
	tmpFile.nDrive = rcpubyte(0x834c);		// drive index
	if ((tmpFile.nDrive < 0) || (tmpFile.nDrive >= MAX_DRIVES) || (NULL == pDriveType[tmpFile.nDrive])) {
		debug_write("SBRLNK call to invalid drive %d", tmpFile.nDrive);
		tmpFile.LastError=ERR_DEVICEERROR;
		wcpubyte(0x8350, tmpFile.LastError);
		return;
	}
	if (DISK_TICC == pDriveType[tmpFile.nDrive]->GetDiskType()) {
		// it's a TI disk controller! We have to handle it special
		TICCDisk *pDisk = (TICCDisk*)pDriveType[tmpFile.nDrive];
		pDisk->sbrlnk(nOpCode);
		return;
	}

	switch (nOpCode) {
		case SBR_SECTOR:
			{
				// raw sector I/O to the disk
				tmpFile.DataBuffer = romword(0x834e);	// address in VDP of data buffer
				tmpFile.RecordNumber = romword(0x8350);	// sector index

				if (rcpubyte(0x834d)) {		// 0 = write, else read
					debug_write("Sector read: drive %d, sector %d, VDP >%04X", tmpFile.nDrive, tmpFile.RecordNumber, tmpFile.DataBuffer);
					if (!pDriveType[tmpFile.nDrive]->ReadSector(&tmpFile)) {
						// write the error code into >8350 (done below)
						// wcpubyte(0x8350, tmpFile.LastError);
					}
				} else {
					debug_write("Sector write: drive %d, sector %d, VDP >%04X", tmpFile.nDrive, tmpFile.RecordNumber, tmpFile.DataBuffer);

					// check for disk write protection
					if (pDriveType[tmpFile.nDrive]->GetWriteProtect()) {
						debug_write("Attempt to write to write-protected disk.");
						// write the error code into >8350 (done below)
						tmpFile.LastError = ERR_WRITEPROTECT;
						break;
					}

					if (!pDriveType[tmpFile.nDrive]->WriteSector(&tmpFile)) {
						// write the error code into >8350 (done below)
						// wcpubyte(0x8350, tmpFile.LastError);
					}
				}

				// fill in the return data
				wrword(0x834a, tmpFile.RecordNumber);
			}
			break;

		case SBR_FORMAT:
			{
				// format the disk - not sure what some of these would be set to!
				tmpFile.NumberRecords = rcpubyte(0x834d);	// number of tracks (usually 35 or 40)
				tmpFile.DataBuffer = romword(0x834e);		// address of data buffer
				tmpFile.RecordsPerSector = rcpubyte(0x8350);// density
				tmpFile.LengthSectors = rcpubyte(0x8351);	// number of sides
			
				debug_write("Format disk: drive %d, %d tracks, density %d, %d sides, VDP >%04X",
					tmpFile.nDrive, tmpFile.NumberRecords, tmpFile.RecordsPerSector, tmpFile.LengthSectors, tmpFile.DataBuffer);

				// check for disk write protection
				if (pDriveType[tmpFile.nDrive]->GetWriteProtect()) {
					debug_write("Attempt to write to write-protected disk.");
					tmpFile.LastError = ERR_WRITEPROTECT;
					// copy the error code into >8350 and >8351, zero LengthSectors so it will also record 0
					// 8350 is done below
					wcpubyte(0x8351, tmpFile.LastError);
					// make up a fake size for apps that don't check
					tmpFile.LengthSectors=9*tmpFile.NumberRecords*tmpFile.RecordsPerSector*tmpFile.LengthSectors;
					if (tmpFile.LengthSectors == 0) tmpFile.LengthSectors=360;
					break;
				}

				// FormatDisk needs to update LengthSectors to the number of sectors on the disk
				if (!pDriveType[tmpFile.nDrive]->FormatDisk(&tmpFile)) {
					// copy the error code into >8350 and >8351, zero LengthSectors so it will also record 0
					// 8350 is done below
					//wcpubyte(0x8350, tmpFile.LastError);
					wcpubyte(0x8351, tmpFile.LastError);
					// make up a fake size for apps that don't check
					tmpFile.LengthSectors=9*tmpFile.NumberRecords*tmpFile.RecordsPerSector*tmpFile.LengthSectors;
					if (tmpFile.LengthSectors == 0) tmpFile.LengthSectors=360;
				}

				// fill in the return data
				wrword(0x834a, tmpFile.LengthSectors);
			}
			break;

		case SBR_PROTECT:
			{
				// change file protection
				// This is baloney, we need to build the filename here. But this isn't implemented yet anyway
				// Passing the address isn't right. :) Build it in csName. That's what the debug uses!
				// (must be max 10 since 10 char filenames won't be padded)
				GetFilenameFromVDP(romword(0x834e), 10, &tmpFile);	// address of filename in VDP - padded with spaces 
				tmpFile.OpCode = romword(0x834d);		// 0 - unprotect, 0xff - protect

				// check for disk write protection
				if (pDriveType[tmpFile.nDrive]->GetWriteProtect()) {
					debug_write("Attempt to write to write-protected disk.");
					// write the error code into >8350 (done below)
					tmpFile.LastError = ERR_WRITEPROTECT;
					break;
				}

				if ((tmpFile.OpCode != 0) && (tmpFile.OpCode != 0xff)) {
					debug_write("Invalid protection flag on drive %d", tmpFile.nDrive);
				} else {
					if (tmpFile.OpCode) {
						debug_write("Protect file %s", tmpFile.csName);
						if (!pDriveType[tmpFile.nDrive]->ProtectFile(&tmpFile)) {
							//wcpubyte(0x8350, tmpFile.LastError);
						}
					} else {
						debug_write("Unprotect file %s", tmpFile.csName);
						if (!pDriveType[tmpFile.nDrive]->UnProtectFile(&tmpFile)) {
							//wcpubyte(0x8350, tmpFile.LastError);
						}
					}
				}
			}
			break;

		case SBR_RENAME:
			{
				CString csNewFile;

				// rename a file - we are limited to 10 characters in this call
				GetFilenameFromVDP(romword(0x8350), 10, &tmpFile);		// new filename
				csNewFile = tmpFile.csName;
				GetFilenameFromVDP(romword(0x834e), 10, &tmpFile);		// old filename

				debug_write("Rename file, drive %d, from %s to %s", tmpFile.nDrive, (LPCSTR)tmpFile.csName, (LPCSTR)csNewFile);

				// check for disk write protection
				if (pDriveType[tmpFile.nDrive]->GetWriteProtect()) {
					debug_write("Attempt to write to write-protected disk.");
					// write the error code into >8350 (done below)
					tmpFile.LastError = ERR_WRITEPROTECT;
					break;
				}

				if (!pDriveType[tmpFile.nDrive]->RenameFile(&tmpFile, csNewFile)) {
					// write the error code into >8350 (done below)
					// wcpubyte(0x8350, tmpFile.LastError);
				}
			}
			break;

		case SBR_FILEIN:
		case SBR_FILEOUT:
			{
				// read or write 'n' sectors from a file, or file info block
				// the Fiad versions already emit a lot of debug, so no extra here
				int nInfo;
				bool isInfoRequest = false;
				nInfo = rcpubyte(0x8350)+0x8300;			// address of info block

				tmpFile.LengthSectors = rcpubyte(0x834d);	// number of sectors to read

				tmpFile.DataBuffer = romword(nInfo);		// address of data buffer
				tmpFile.RecordNumber = romword(nInfo+2);	// first sector to read/write
				tmpFile.FileType = rcpubyte(nInfo+4);		// file type
				tmpFile.RecordsPerSector = rcpubyte(nInfo+5);//records per sector
				tmpFile.BytesInLastSector = rcpubyte(nInfo+6);//EOF offset
				tmpFile.RecordLength = rcpubyte(nInfo+7);	// record length
				tmpFile.NumberRecords = romword(nInfo+8);	// number of records (WORD, Thierry's notes are a little confusing)

				// get the filename from VDP (must be 10 since 10 char filenames won't be padded)
				GetFilenameFromVDP(romword(0x834e), 10, &tmpFile);

				if (SBR_FILEIN == nOpCode) {
					// check whether it is just an info request
					isInfoRequest = (tmpFile.LengthSectors == 0);
					if (!pDriveType[tmpFile.nDrive]->ReadFileSectors(&tmpFile)) {
						// write the error code into >8350 (done below)
						// wcpubyte(0x8350, tmpFile.LastError);
					} else if (isInfoRequest) {
						// fill in the return data
						wrword  (nInfo+2, tmpFile.LengthSectors);	// info block
						wcpubyte(nInfo+4, tmpFile.FileType);
						wcpubyte(nInfo+5, tmpFile.RecordsPerSector);
						wcpubyte(nInfo+6, tmpFile.BytesInLastSector);
						wcpubyte(nInfo+7, tmpFile.RecordLength);
						wrword  (nInfo+8, tmpFile.NumberRecords);
					}
				} else {
					// check for disk write protection
					if (pDriveType[tmpFile.nDrive]->GetWriteProtect()) {
						debug_write("Attempt to write to write-protected disk.");
						// write the error code into >8350 (done below)
						tmpFile.LastError = ERR_WRITEPROTECT;
						break;
					}

					if (!pDriveType[tmpFile.nDrive]->WriteFileSectors(&tmpFile)) {
						// write the error code into >8350 (done below)
						// wcpubyte(0x8350, tmpFile.LastError);
					}
				}

				wcpubyte(0x834c, 0);						// always zero the drive index
				wcpubyte(0x834d, tmpFile.LengthSectors);	// sectors read/written
			}
			break;

		default:
			debug_write("Unsupported SBRLNK opcode 0x%x", nOpCode);
			tmpFile.LastError = ERR_DEVICEERROR;
			break;
	}

	// store the error code before exit
	wcpubyte(0x8350, tmpFile.LastError);	// should still be 0 if no error occurred
}

///////////////////////////////////////////////////////////////////////
// Load a PROGRAM image file
///////////////////////////////////////////////////////////////////////
bool TryLoadMagicImage(FileInfo *pFile) {
	// This function is called if the file's not on the disk, we
	// instead will try to read it from the resources
	// Resources are assumed to have no header
	struct DISKS *pDsk;
	struct IMG tImg;

	if ((NULL == pMagicDisk) || (NULL == pFile)) {
		return false;
	}

	if (pFile->DataBuffer > 0xffff) return false;

	pDsk=pMagicDisk;

	while (pDsk->dwImg) {
		if (0 == pFile->csName.Compare(pDsk->szName)) {
			// Load this file
			tImg.dwImg=pDsk->dwImg;
			tImg.nType=TYPE_VDP;
			tImg.nLoadAddr=pFile->DataBuffer;
			tImg.nLength=pFile->RecordNumber;
			tImg.nBank=0;
			LoadOneImg(&tImg, "DISKFILES");
			return true;
		}
		pDsk++;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////
// Set the error bit in the PAB
// Note that the user program needs to clear this, not the DSR
/////////////////////////////////////////////////////////////////////////
void setfileerror(FileInfo *pFile) {
	debug_write("Setting file error %d on file buffer %d", pFile->LastError, pFile->nIndex);
//	VDP[pFile->PABAddress+1] &= 0x1f;						// no errors (program must clear this - confirmed by TI spec)
	VDP[pFile->PABAddress+1] |= pFile->LastError<<5;		// file error
	// Only set COND on non-existant DSR - confirmed by TI spec
	// In an error case we need to release the object - programs generally won't close it
	if (!pFile->bFree) {
		if (NULL != pDriveType[pFile->nDrive]) {
			pDriveType[pFile->nDrive]->Close(pFile);
		} else {
			// manual release then
			debug_write("Releasing file buffer %d (no drive!)", pFile->nIndex);
			// release any data that may be there
			if (NULL != pFile->pData) {
				free(pFile->pData);
				pFile->pData=NULL;
				pFile->nDataSize = 0;
			}
			pFile->bFree = true;
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Equivalent to CALL FILES(n)
// positive count = CALL FILES(x), negative count = SBRLNK access
///////////////////////////////////////////////////////////////////////
void do_files(int n)				/* call files(n) */ 
{
	// check to see if any of the configured drives are a TICC, if so,
	// we let it manage this instead of faking it.
	bool bTICC = false;
	for (int idx=0; idx<MAX_DRIVES; idx++) {
		if (NULL != pDriveType[idx]) {
			if (DISK_TICC == pDriveType[idx]->GetDiskType()) {
				bTICC = true;
				break;
			}
		}
	}

	if (bTICC) {
		char name[8];

		debug_write("Found TICC, letting it handle FILES");
		nDSRBank[1] = 1;		// switch in the TICC

		if (n >= 0) {
			// positive value, 'CALL FILES' mode, so find that
			strcpy(name, "FILES");
		} else {
			// negative value, SBRLNK, find the control code name
			name[0] = SBR_FILES;
			name[1] = '\0';
		}

		unsigned short p = 0x000A;
		p=DSR[1][0x2000+p]*256 + DSR[1][0x2000+p+1];

		while (p > 0) {
			if ((0 == memcmp(&DSR[1][0x2000+p+5-0x4000], name, strlen(name))) && (strlen(name) == DSR[1][0x2000+p+4-0x4000])) {
				break;
			}
			p=DSR[1][0x2000+p-0x4000]*256 + DSR[1][0x2000+p+1-0x4000];
		}
		if (p==0) {
			// somehow we didn't find it - this shouldn't happen - we won't get a proper error, either!
			debug_write("Failed to find FILES subprogram (%d) in DSR - shouldn't happen!", n);
			nDSRBank[1] = 0;	// switch back
			return;
		}

		// get launch address and jump to it
		debug_write("Handling FILES(%d) via TICC DSR", n);

		p=DSR[1][0x2000+p+2-0x4000]*256 + DSR[1][0x2000+p+3-0x4000];
		pCurrentCPU->SetPC(p);
	} else {
		if (n < 0) n=-n;		// this code doesn't care how it was called
		// tell all the DSRs that files was changed
		for (int idx=0; idx<MAX_DRIVES; idx++) {
			if (NULL != pDriveType[idx]) {
				// no error return? Bad call files should set 0x8350 to >FFFF
				pDriveType[idx]->SetFiles(n);
			}
		}
	}
	return;
}

/////////////////////////////////////////////////////////////////////////
// Read (part of) a file from a disk image
/////////////////////////////////////////////////////////////////////////
#if 0
void read_image_file(int PAB, char *buffer, int offset, int len) 
{
	Byte bufs1[256], buffds[256], fn[10];
	Byte drive;
	Byte *p1, *p2, *p3;
	Word Start, Length;
	int found, fds, i;
	char temp[1024];

	drive=VDP[PAB+13] - '0';	// drive number
	read_sect(drive, 1, bufs1);
	p1 = stpncpy(fn, VDP+PAB+15, VDP[PAB+9]-5);
	while (p1<fn+10)				/* fill with spaces for comparing */
	{
		*p1++ = ' ';
	}
	p1=bufs1, p2=bufs1+256;
	found=0;
	
	do 
	{
		p = p1 + (((p2-p1)/2) & ~1);
		fds = (*p)<<8 | *(p+1);
		if (!fds) 
		{
			p2 = p;
			continue;
		}
		read_sect(drive, fds, buffds);
		if (0 == (cmp=strncmp(fn, buffds, 10))) 
		{
			found=1;
			break;
		} else 
		{
			if (cmp>0)
			{
				p1=p+2;
			}
			else
			{
				p2=p;
			}
		}
	} while (p1<p2);

	if (found) 
	{
		size = 256;
		Length = 0;
		Prog = buffds[12] & 1;
		Dis = (buffds[12] & 2) ? 0 : 1;
		Var = buffds[12] >> 7;
		Len = buffds[0x11];
		debug_write("Copying %d %s %s %s%s%s%d", o, argv[o], filename, Prog ? "P": " ", !Prog && Dis ? "D": " ", !Prog && Var ? "V": " ", Len);
		for (p3 = buffds+0x1c; *p3 | *(p3+1); p3 += 3) 
		{		/* all chunks */
			Start = *p3 | ((*(p3+1) & 0x0f) << 8);
			Length = (((*(p3+1) & 0xf0) >> 4) | (*(p3+2) << 4)) - Length;
			for (i=0; i<=Length; i++) 
			{ /* read every sector */
				read_sect(Start++, buffer);
				if (Prog && i==Length && (*(p3+3) | *(p3+4)) == 0)
				{
					size =  buffds[0x10] ?  buffds[0x10] : 256;
				}
				/* last sector of P-File: shorter */
				fwrite(buffer, size, 1, outfile);
			} /* for i */
		} /* for p3 */
	} /* found / for p */
}
#endif
