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
// Simulation of the UberGROM. Everything is just done as 
// blocks of memory, no attempt to handle hardware is done
//

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <process.h>
#include <malloc.h>
#include <dsound.h>
#include <time.h>
#include <math.h>
#include <atlstr.h>
#include "..\resource.h"
#include "tiemul.h"
#include "ubergrom.h"

// TODO: does not support the recovery program right now
// TODO: does not support the GROM wrap bit either, because it uses the
// main system GROM address.

bool bUberGROMActive = false;			// enables the various hacks needed to emulate this
extern volatile unsigned long total_cycles;
unsigned char Override = 0xff;			// override/recovery program
int eeLocked = 0;

// the various memories we need to care about
unsigned char UberGROM[120*1024];
unsigned char UberRAM[15*1024];
unsigned char UberEEPROM[4*1024];
unsigned char UberGPIO[1];
unsigned char UberUART[4];
unsigned char UberFlash[0x0105];
unsigned char UberTimer[2];

extern unsigned char GROM6000[];
extern unsigned char GROM70A0[];

// startup code managed at >E000 on first boot only
const unsigned char UberHack[] = {
	0xaa,0x01, 		// valid id
	0x00,0x00,		// programs
	0xe0,0x0c,		// power up address (our hook!)
	0x00,0x00,		// program list
	0x00,0x00,		// DSR list
	0x00,0x00,		// subprogram list

// powerup header - 0xE00C
	0x00,0x00,		// link to next powerup -- we have to override this with the user's data!
	0xe0,0x10,		// address of this one

// powerup GPL code - 0xE010 - test keyboard for space bar - we monitor the code path to enable/disable the hack
	0xbe,0x74,0x05,	// ST >05,@>8374
	0x03,			// SCAN
	0xd6,0x75,0x20,	// CEQ >20,@>8375
	0x40,0x1A,		// BR >E01A
	
// E019
	0x0A,			// GT	-- key was pressed if this reached >E019 (GT is to have something to jump over)
					// Continue as per Thierry's site
// E01A
	0xbd,0x90,0x73,
	0x90,0x72,		// DST *>8372,*>8373	Transfer address from data stack (72) to sub stack (73)
	0x96,0x72,		// DECT @>8372			decrement data stack pointer

// E021
	0x00			// RTN					branches to next powerup routine
};

// testing the hack code
Byte ReadHack(unsigned char page, unsigned int address) {
	unsigned char x=0;		// default return is 0
	const unsigned char *adr=NULL;

	// the page contains the high order bits of the actual address in this case (not true anywhere else)
	address |= (page<<8);

	if (address >= 0xe000) {
		// anything else in the minirom is a direct dump
		// we're all in the second block
		adr = &UberHack[address-0xe000];
	} else if (address >= 0x70a0) {
		// if it's in one of the >6000 ranges, then return the big ROM
		adr = &GROM70A0[address-0x70a0];
	} else if (address >= 0x6000) {
		adr = &GROM6000[address-0x6000];
	}
	if (adr != NULL) {
		x = *adr;
	}

	// check for the magic vectors
	if (Override == 0xff) {
		// whichever one we see first, basically!
		if (address == 0xe019) {
			Override = 0x81;	// we will be active!
		}
		if (address == 0xE01A) {	// this one is always hit, so we have to check for hitting e019 first.	
			Override = 0x80;	// we will not be active
		} 
	} else if (Override & 0x80) {
		if (address == 0xE00D) {	// keep it up until the next powerup link is read!
			Override&=0x0f;
		}
	}
	
	return x;
}

// We should only be called for GROM addresses from >6000 through >FFFF
// CPU address should translate down to a base from 0-15
Byte UberGromRead(Word GromAddress, int nBase) {
	// figure out what configuration byte to read
	// first, if it's the configuration block, always return that
 	if ((GromAddress >= 0xf800) && (nBase == 15)) {
		return UberEEPROM[GromAddress-0xf800];
	}
	
	if (((UberEEPROM[0]&1)==0) || (UberEEPROM[0] != ((~UberEEPROM[1])&0xff))) {
		nBase = 0;							// not using bases, though
	}

	if (Override != 0) {
		unsigned char x = UberEEPROM[0];
		if (UberEEPROM[0] != ((~UberEEPROM[1])&0xff)) x=0;

		// handling the built-in override. Since this happens on EVERY access, we want to
		// try and be as quick as we can to rule it out
		if (x&2) {
			Override=0;	// never, so speed up later accesses
		} else {
			// see if we are still detecting powerup routine on >Exxx
			if ((Override&0x80) && ((GromAddress&0xE000) == 0xE000) && (nBase == 0)) {
				// this is the first pass test for >Exxx, map through to our code so that we can check the keyboard and set the result
				return ReadHack((GromAddress&0xE000)>>8, GromAddress&0x1fff);
			}
			// only after the mode is set do we check for override of >6000
			if ((Override == 0x01) && ((GromAddress&0xE000) == 0x6000) && (nBase == 0)) {
				// this is the first pass test for >Exxx, map through to our code so that we can check the keyboard and set the result
				return ReadHack((GromAddress&0xE000)>>8, GromAddress&0x1fff);
			}
		}
	}

	// from the address, calculate which configuration byte to read
	int nOffset=(GromAddress&0xE000)>>13;
	nOffset+=2+nBase*16;

	// is it valid?
	if (UberEEPROM[nOffset] != ((~UberEEPROM[nOffset+8])&0xff)) {
		return 0xff;
	}

	// yes, parse it
	int nPage = UberEEPROM[nOffset]&0x0f;
	int nGrom = GromAddress&0x1fff;

	switch (UberEEPROM[nOffset]&0xf0) {
	case 0x00:
		nOffset=nPage*8192+nGrom;
		if (nOffset >= 15*1024) {
			return 0xff;
		} else {
			return UberRAM[nOffset];
		}
		break;

	case 0x10:
		nOffset=nPage*8192+nGrom;
		if (nOffset >= 120*1024) {
			return 0xff;
		} else {
			return UberGROM[nOffset];
		}
		break;

	case 0x20:
		nOffset=nPage*8192+nGrom;
		if (nOffset >= 4*1024) {
			return 0xff;
		} else {
			return UberEEPROM[nOffset];
		}
		break;

	case 0x30:
		if (nGrom < 0x20) {
			return 0;
		}
		nGrom-=0x20;
		if (nGrom == 0) {
			return UberGPIO[0];
		} else {
			debug_write("UberGROM: Read GPIO pins");
			return rand()%0x10;
		}
		break;

	case 0x40:
		if (nGrom < 0x20) {
			return 0;
		}
		nGrom-=0x20;
		debug_write("UberGROM: Read ADC %d", nPage);
		return (nPage<<4)|(rand()%10);
		break;

	case 0x50:
		if (nGrom < 0x20) {
			return 0;
		}
		if (nGrom == 0x20) {
			return 0x06;		// always ready for more
		} else if ((nGrom >= 0x21)&&(nGrom < 0x24)) {
			return UberUART[nGrom];
		} else if (nGrom == 0x24) {
			return 0;			// no characters available
		} else if (nGrom == 0x25) {
			return 255;			// buffer is empty
		} else {
			if (nGrom < 0x1000) debug_write("Warning: reading UART write-data space");
			return 0xff;		// not emulating the buffer here
		}
		break;

	case 0x60:
		if (nGrom < 0x100) {
			// instead of undefined, return hard-coded 0
			return 0;
		} else if ((nGrom == 0x100)||(nGrom==0x0103)||(nGrom==0x0104)) {
			return UberFlash[nGrom];
		} else {
			return 0xff;		// most data is write-only
		}
		break;

	case 0x70:
		if (nGrom < 0x20) {
			return 0;
		}
		nGrom-=0x20;
		if (nGrom&0x01) {
			// MSB
			return UberTimer[1];
		} else {
			// LSB
			unsigned long x = total_cycles/384;		// approx 7812.5 hz assuming CPU at 3MHz
			UberTimer[0]=x&0xff;
			UberTimer[1]=(x&0xff00)>>8;
			return UberTimer[0];
		}
		break;

	default:
		return 0xff;
	}
}

void UberGromWrite(Word GromAddress, int nBase, Byte x) {
	// figure out what configuration byte to read
	// first, if it's the configuration block, always return that
	if ((GromAddress >= 0xf800) && (nBase == 15)) {
		// check for unlock sequence
		if (GromAddress == 0xffff) {
			if (x == 0x55) {
				eeLocked = 0x55;	// first byte of sequence
			} else if ((x == 0xaa) && (eeLocked == 0x55)) {
				eeLocked = 0xaa;	// second byte of sequence
			} else if ((x == 0x5a) && (eeLocked == 0xaa)) {
				eeLocked = 0x5a;	// final byte of sequence
				debug_write("UberGROM EEPROM Unlocked");
			} else {
				if (eeLocked) {
					debug_write("UberGROM EEPROM locked");
				}
				eeLocked = 0;		// re-lock
			}
		} else {
			if (eeLocked == 0x5a) {
				UberEEPROM[GromAddress-0xf800]=x;
			}
		}
		return;
	}
	
	if (((UberEEPROM[0]&1)==0) || (UberEEPROM[0] != ((~UberEEPROM[1])&0xff))) {
		nBase = 0;							// not using bases, though
	}

	// from the address, calculate which configuration byte to read
	int nOffset=(GromAddress&0xE000)>>13;
	nOffset+=2+nBase*16;

	// is it valid?
	if (UberEEPROM[nOffset] != ((~UberEEPROM[nOffset+8])&0xff)) {
		return;
	}

	// yes, parse it
	int nPage = UberEEPROM[nOffset]&0x0f;
	int nGrom = GromAddress&0x1fff;

	switch (UberEEPROM[nOffset]&0xf0) {
	case 0x00:
		nOffset=nPage*8192+nGrom;
		if (nOffset >= 15*1024) {
		} else {
			UberRAM[nOffset]=x;
		}
		break;

	case 0x10:
		// GROM not writable
		break;

	case 0x20:
		nOffset=nPage*8192+nGrom;
		if (nOffset >= 4*1024) {
		} else if (eeLocked == 0x5a) {
			UberEEPROM[nOffset]=x;
		}
		break;

	case 0x30:
		if (nGrom < 0x20) {
			return;
		}
		nGrom-=0x20;
		if (nGrom == 0) {
			UberGPIO[0]=x;
		} else {
			debug_write("UberGROM: Write GPIO pins %X", x);
		}
		break;

	case 0x40:
		// nothing to write for ADC
		break;

	case 0x50:
		if (nGrom < 0x20) {
			return;
		}
		if (nGrom == 0x20) {
			// read only register
			debug_write("Warning: write read-only UART register 0");
		} else if ((nGrom >= 0x21)&&(nGrom < 0x24)) {
			UberUART[nGrom]=x;
		} else {
			if ((nGrom>=0x0100)&&(nGrom<0x1000)) {
				debug_write("UberGROM: writing UART data 0x%02X at rate %d bps", x, 8000000/(UberUART[2]+UberUART[3]*256+1)/16);
			} else {
				debug_write("WARNING: writing UART read area");
			}
		}
		break;

	case 0x60:
		if (nGrom == 0x103) {
			debug_write("Warning: writing Flash device status register");
		} else {
			UberFlash[nGrom]=x;
			if (nGrom == 0x0101) {
				UberFlash[0x0103] = 0;	// clear error
			}
			if (nGrom == 0x0102) {
				if (UberFlash[0x0101]!=((~x)&0xff)) {
					UberFlash[0x0103]=3;
					debug_write("Warning: flash command bytes do not match (%02X/%02X)", UberFlash[0x0101], UberFlash[0x0102]);
				} else if (x == 0xce) {
					// erase
					// calculate the offset in the GROM space
					nOffset=UberFlash[0x104]*8192+UberFlash[0x100]*256;
					if (nOffset >= 120*1024) {
						debug_write("Warning: invalid flash address 0x%X", nOffset);
						UberFlash[0x0103]=3;
					} else {
						memset(&UberGROM[nOffset], 0xff, 256);
					}
				} else if (x == 0x2d) {
					// write
					// calculate the offset in the GROM space
					nOffset=UberFlash[0x104]*8192+UberFlash[0x100]*256;
					if (nOffset >= 120*1024) {
						debug_write("Warning: invalid flash address 0x%X", nOffset);
						UberFlash[0x0103]=3;
					} else {
						memcpy(&UberGROM[nOffset], &UberFlash[0], 256);
					}
				} else {
					debug_write("Warning: unknown flash command (%02X/%02X)", UberFlash[0x0101], UberFlash[0x0102]);
					UberFlash[0x0103]=3;
				}
			}
		}
		break;

	case 0x70:
		// can't write timer
		break;

	default:
		return;
	}
}
