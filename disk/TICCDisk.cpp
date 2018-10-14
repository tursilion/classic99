//
// (C) 2013 Mike Brent aka Tursi aka HarmlessLion.com
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
#include "imagedisk.h"
#include "TICCDisk.h"
#include "../console/cpu9900.h"

//********************************************************
// TICCDisk - pretty hacky, this one...
//********************************************************

extern CPU9900 * volatile pCurrentCPU;
extern bool bCorruptDSKRAM;

// for now, we say there is only one controller card, so we share the CRU and registers
unsigned char TICC_CRU[8];
unsigned char TICC_REG[8];
unsigned char TICC_DIR=0;	// 0 means towards 0

// NOTE: right now there is no intention to provide hardware-level emulation
// The registers are here for convenience. The actual activity happens by intercepting
// the sector functions, for now.

// global access to registers and CRU
int ReadTICCCRU(int adr) {
	adr&=0x07;
	int ret = TICC_CRU[adr];
	if (adr == 0x04) {
		// motor strobe, turn it off if it was on 
		// (todo: supposed to turn off after 4.23 seconds)
		TICC_CRU[adr] = 1;
	}
	return TICC_CRU[adr];
}

void WriteTICCCRU(int adr, int bit) {
	adr&=0x07;

	switch (adr) {
	case 0:		// ROM select - does this even get called?
		TICC_CRU[adr]=bit;
		break;

	case 1:		// strobe motor for 4.23 seconds
		if (bit == 0) {
			TICC_CRU[0x04] = 0;		// on
		}
		break;

	case 2:		// if 0, ignore IRQ and DRQ. When 1, IRQ & DRQ trigger the 9900 READY line, halting the CPU
		// TODO: not implemented
		break;

	case 3:		// HLT - signal head loaded?
		TICC_CRU[0x00] = bit;		// HLD - load head requested? not sure this will work
		break;

	case 4:		// select drive 1
		TICC_CRU[0x01] = bit;
		break;

	case 5:		// select drive 2
		TICC_CRU[0x02] = bit;
		break;

	case 6:		// select drive 3
		TICC_CRU[0x03] = bit;
		break;

	case 7:		// select side
		TICC_CRU[0x07] = bit;
		break;
	}
}

int ReadTICCRegister(int address) {
	if ((address > 0x5ff7)||(address < 0x5ff0)) {
		return -1;
	}
	switch (address&0xfffe) {
	case 0x5ff0:
		// status register
		TICC_REG[0] = 0x20;	// head loaded, ready, not busy, not track 0, no index pulse, etc
		if (TICC_REG[1]==0) {
			TICC_REG[0]|=0x04;
		}
		TICC_REG[0]=~TICC_REG[0];
		return TICC_REG[0];

	case 0x5ff2:
		// track register
		return TICC_REG[1];

	case 0x5ff4:
		// sector register
		return TICC_REG[2];

	case 0x5ff6:
		// data register
		return TICC_REG[3];
	}

	// in case we get here
	return 0;
}

void WriteTICCRegister(int address, int val) {
	if ((address < 0x5ff8) || (address > 0x5fff)) {
		return;
	}
	switch (address&0xfffe) {
	case 0x5ff8:
		// command register
		switch (val & 0xe0) {
		case 0x00:
			// restore or seek
			if (val&0x10) {
				// seek to data reg
				TICC_REG[1]=TICC_REG[3];
			} else {
				// seek to track 0
				TICC_REG[1]=0;
			}
			break;

		case 0x20:
			// step
			if (val&0x10)	{	// if update track register
				if (TICC_DIR) {
					if (TICC_REG[1] < 255) TICC_REG[1]++;
				} else {
					if (TICC_REG[1] > 0) TICC_REG[1]--;
				}
			}
			break;

		case 0x40:
			// step in
			if (val&0x10)	{	// if update track register
				if (TICC_REG[1] < 255) TICC_REG[1]++;
			}
			break;

		case 0x60:
			// step out
			if (val&0x10)	{	// if update track register
				if (TICC_REG[1] > 0) TICC_REG[1]--;
			}
			break;

		default:
			debug_write("Disk controller got unimplemented command 0x%02X", val);
			break;
		}
		break;

	case 0x5ffA:
		// track register
		TICC_REG[1] = val;
		break;

	case 0x5ffC:
		// sector register
		TICC_REG[2] = val;
		break;

	case 0x5ffE:
		// data register
		TICC_REG[3] = val;
		break;
	}
}

void HandleTICCSector() {
	// find the right instance and invoke it to get the sector in memory, then skip over the code
	
	// 834A = sector number
	// 834C = drive (1-3)
	// 834D = 0: write, anything else = read
	// 834E = VDP buffer address

	int nDrive=rcpubyte(0x834c);		// get drive index

	// todo: would be nice to be able to map other than 1-3
	if ((nDrive > 0) && (nDrive <= 3) && (pDriveType[nDrive]!=NULL) && (DISK_TICC == pDriveType[nDrive]->GetDiskType())) {
		TICCDisk *disk = (TICCDisk*)pDriveType[nDrive];
		if (rcpubyte(0x834d)) {
			disk->readsectorwrap();
		} else {
			disk->writesectorwrap();
		}
		pCurrentCPU->SetPC(0x4676);		// return from read or write (write normally goes through read to verify)
	} else {
		debug_write("DSK%d not a valid TICC Disk! (1-3 only for now!)");
		pCurrentCPU->SetPC(0x42a0);			// error 31 (not found)
	}
}

// TODO - write sector can come later

// constructor
TICCDisk::TICCDisk() {
	memset(TICC_CRU, 0, sizeof(TICC_CRU));
	memset(TICC_REG, 0, sizeof(TICC_REG));

	TICC_CRU[0x06] = 1;		// always 1
}

TICCDisk::~TICCDisk() {
}

// powerup routine
void TICCDisk::Startup() {
	bCorruptDSKRAM=false;		// no matter what it was set to, if you are using this, you must NOT corrupt the disk ram! WE USE IT. ;)
	nDSRBank[1] = 0;			// Classic99 disk access for the default setup

	ImageDisk::Startup();
	BaseDisk::Startup();
	
	// now, page in the disk DSR, and find the startup vector
	// NOTE: we assume there is only one!
	unsigned short PC=DSR[1][0x2004]*256 + DSR[1][0x2005];	// hard coded assumptions - startup vector
	if (PC == 0) {
		nDSRBank[1] = 0;
	} else {
		if ((DSR[1][0x2000+PC-0x4000]!=0)||(DSR[1][0x2000+PC+1-0x4000]!=0)) {
			debug_write("Warning: linked power up vectors not honored!");
		}
		PC=DSR[1][0x2000+PC+2-0x4000]*256 + DSR[1][0x2000+PC+3-0x4000];
		// no matter how many are set, it will still only run once, so that SHOULD be okay..
		debug_write("Starting TICC powerup routine at 0x%04X - only happens once per boot!", PC);
		nDSRBank[1] = 1;
		nCurrentDSR = 1;	// this is usually not set here
		pCurrentCPU->SetPC(PC);
	}
}

// TODO: this means DSK.DISKNAME.FILENAME isn't supported
// TODO: means only DSK1-DSK3 is supported, we may want to enhance that someday
void TICCDisk::dsrlnk(int nDrive) {
	// just switch it in and find the correct entry point
	nDSRBank[1] = 1;
	char name[8];

	if (nDrive == -1) {
		// DSK.DISKNAME format - this should still be okay as long as we start on
		// the correct drive - I think. ;)
		strcpy(name, "DSK");
	} else {
		sprintf(name, "DSK%d", nDrive);
	}

	unsigned short p=8;
	p=DSR[1][0x2000+p]*256 + DSR[1][0x2000+p+1];	// get first entry

	while (p > 0) {
		if ((0 == memcmp(&DSR[1][0x2000+p+5-0x4000], name, strlen(name))) && (strlen(name) == DSR[1][0x2000+p+4-0x4000])) {
			break;
		}
		p=DSR[1][0x2000+p-0x4000]*256 + DSR[1][0x2000+p+1-0x4000];
	}
	if (p==0) {
		// somehow we didn't find it - this shouldn't happen - we won't get a proper error, either!
		debug_write("Failed to find %s entry point in DSR - shouldn't happen!", name);
		nDSRBank[1] = 0;	// switch back
		return;
	}

	// get launch address and jump to it
	debug_write("Handling %s via TICC DSR", name);

	p=DSR[1][0x2000+p+2-0x4000]*256 + DSR[1][0x2000+p+3-0x4000];
	pCurrentCPU->SetPC(p);
}

void TICCDisk::sbrlnk(int nOpCode) {
	// note that files is handled elsewhere and won't end up in here...

	// just switch it in and find the correct entry point
	nDSRBank[1] = 1;
	char name[8];
	name[0] = nOpCode;
	name[1] = '\0';

	unsigned short p=0x000A;
	p=DSR[1][0x2000+p]*256 + DSR[1][0x2000+p+1];	// get first entry

	while (p > 0) {
		if ((0 == memcmp(&DSR[1][0x2000+p+5-0x4000], name, strlen(name))) && (strlen(name) == DSR[1][0x2000+p+4-0x4000])) {
			break;
		}
		p=DSR[1][0x2000+p-0x4000]*256 + DSR[1][0x2000+p+1-0x4000];
	}
	if (p==0) {
		// somehow we didn't find it - this shouldn't happen - we won't get a proper error, either!
		debug_write("Failed to find SBRLNK(%d) entry point in DSR - shouldn't happen!", nOpCode);
		nDSRBank[1] = 0;	// switch back
		return;
	}

	// get launch address and jump to it
	debug_write("Handling SBRLNK(%d) via TICC DSR", nOpCode);

	p=DSR[1][0x2000+p+2-0x4000]*256 + DSR[1][0x2000+p+3-0x4000];
	pCurrentCPU->SetPC(p);
}

void TICCDisk::readsectorwrap() {

	// simple wrapper to read a sector from an image disk
	// we are already on the right drive
	
	// 834A = sector number
	// 834C = drive (1-3)
	// 834D = 0: write, anything else = read
	// 834E = VDP buffer address

	FileInfo lclFile;

	lclFile.nDrive=rcpubyte(0x834c);
	lclFile.DataBuffer = romword(0x834e);	// address in VDP of data buffer
	lclFile.RecordNumber = romword(0x834a);	// sector index

	// This may get spammy...
	debug_write("Sector read: TICC drive %d, sector %d, VDP >%04X", lclFile.nDrive, lclFile.RecordNumber, lclFile.DataBuffer);
	if (!ReadSector(&lclFile)) {
		// write the error code into >8350 (done below)
		// wcpubyte(0x8350, tmpFile.LastError);
	}

	// fill in the return data
	wrword(0x834a, lclFile.RecordNumber);

	// store the error code before exit
	wcpubyte(0x8350, lclFile.LastError);	// should still be 0 if no error occurred
}

void TICCDisk::writesectorwrap() {

	// simple wrapper to write a sector to an image disk
	// we are already on the right drive
	
	// 834A = sector number
	// 834C = drive (1-3)
	// 834D = 0: write, anything else = read
	// 834E = VDP buffer address

	FileInfo lclFile;

	lclFile.nDrive=rcpubyte(0x834c);
	lclFile.DataBuffer = romword(0x834e);	// address in VDP of data buffer
	lclFile.RecordNumber = romword(0x834a);	// sector index

	// This may get spammy...
	debug_write("Sector write: TICC drive %d, sector %d, VDP >%04X", lclFile.nDrive, lclFile.RecordNumber, lclFile.DataBuffer);
	if (!WriteSector(&lclFile)) {
		// write the error code into >8350 (done below)
		// wcpubyte(0x8350, tmpFile.LastError);
	}

	// fill in the return data
	wrword(0x834a, lclFile.RecordNumber);

	// store the error code before exit
	wcpubyte(0x8350, lclFile.LastError);	// should still be 0 if no error occurred
}

