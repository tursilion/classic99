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
// Handlers and storage for the Multiple Personality Distorter
// I don't expect this to be generally useful to people, but
// I needed a way to develop it, and this will let people play
// with it if they don't want to hack their real TI.
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
#include "mpd.h"

// some real-time measurement stuff to simulate the Hz timer
LARGE_INTEGER tLast = { 0, 0 };
unsigned short int nHzTimer = 0;	// could be random on the real thing

// For ease of work here, everything is padded to 8k. The real device
// has less than 128k of flash space to work with and doesn't load
// a solid block this way.
unsigned char mpdGrom[19*8192];
bool bMpdActive = false;				// enables the various hacks needed to emulate this
int nCurrentBank[3] = { 0, 0 ,0 };		// which GROM is mapped into each of the 3 banks
const Byte nDefaultConfigBytes[] = { 
	(Byte)MPD_BANK_994AGROM0, 
    (Byte)~MPD_BANK_994AGROM0, 
    (Byte)MPD_BANK_994ABASIC1, 
    (Byte)~MPD_BANK_994ABASIC1, 
    (Byte)MPD_BANK_994ABASIC2, 
    (Byte)~MPD_BANK_994ABASIC2, 
    0xff, 
    0x00
};

// these functions will be different on the AVR since it needs to reference EEPROM
void MpdWriteConfigByte(int index, Byte c) {
	mpdGrom[MPD_EEPROM_OFFSET + index] = c;
}
Byte MpdGetConfigByte(int index) {
	// verify valid data, else return default. 
	if (mpdGrom[MPD_EEPROM_OFFSET + index] == ((~mpdGrom[MPD_EEPROM_OFFSET + index + 1])&0xff)) {
		return mpdGrom[MPD_EEPROM_OFFSET + index];
	} else {
		// fix the broken config bytes, and return
		if (index < sizeof(nDefaultConfigBytes)-1) {
			// only patch it when it's even
			if ((index&0x01)==0) {
				MpdWriteConfigByte(index&0xfe, nDefaultConfigBytes[index&0xfe]);
				MpdWriteConfigByte((index&0xfe)+1, nDefaultConfigBytes[(index&0xfe)+1]);
			}
			return nDefaultConfigBytes[index];
		} else {
			return 0;
		}
	}
}

Byte GetMpdOverride(Word nTestAddress, Byte z) {
	if ((nTestAddress >= 0x1800) && (nTestAddress < 0x2000)) {
		// configuration block, except for the one case of a backreference
		if (nTestAddress == MPD_SUB_ENTRY) {
			// subprogram address patching (get the GROM's real subprogram list)
			z=GROMBase[0].GROM[TI_HDR_SUBPROGRAM];
		} else if (nTestAddress == MPD_SUB_ENTRY+1) {
			// subprogram address patching (get the GROM's real subprogram list)
			z=GROMBase[0].GROM[TI_HDR_SUBPROGRAM+1];
		} else if (nTestAddress == MPD_MENU_ENTRY) {
			// subprogram address patching (get the GROM's real program list)
			z=GROMBase[0].GROM[TI_HDR_PROGRAM];
		} else if (nTestAddress == MPD_MENU_ENTRY+1) {
			// subprogram address patching (get the GROM's real program list)
			z=GROMBase[0].GROM[TI_HDR_PROGRAM+1];
		} else if (nTestAddress == MPD_SPEED_ENTRY) {
			// subprogram address patching (get the BASIC GROM's real program list)
			z=GROMBase[0].GROM[TI_HDR_BASIC_PROGRAM];
		} else if (nTestAddress == MPD_SPEED_ENTRY+1) {
			// subprogram address patching (get the BASIC GROM's real program list)
			z=GROMBase[0].GROM[TI_HDR_BASIC_PROGRAM+1];
		} else if (nTestAddress == MPD_HZ_TIMER) {
			// update the cache of the 7812.5Hz timer with the new value, and return the LSB
			LARGE_INTEGER tNow, tRate;
			QueryPerformanceCounter(&tNow);
			QueryPerformanceFrequency(&tRate);
			// get number of elapsed milliseconds and roll it into the counter (wrap-around is fine)
			tRate.QuadPart = (tRate.QuadPart * 10) / 78125;		// ticks per unit (*10 to roll in the .5 as an integer)
			nHzTimer += (unsigned short int)(((tNow.QuadPart - tLast.QuadPart)/tRate.QuadPart)&0xffff);
			z = nHzTimer & 0xff;
		} else if (nTestAddress == MPD_HZ_TIMER+1) {
			// return the cached MSB of the Hz timer
			z = (nHzTimer>>8)&0xff;
		} else {
			z=mpdGrom[(MPD_BANK_CONFIG*8192)+(nTestAddress-0x1800)];
		}
	} else switch (nTestAddress) {
		case TI_HDR_PROGRAM:
			// config menu patching
			z=(MPD_MENU_ENTRY&0xff00)>>8;
			break;

		case TI_HDR_PROGRAM+1:
			// config menu availability!
			z=MPD_MENU_ENTRY&0xff;
			break;

		case TI_HDR_SUBPROGRAM:
			// subprogram address patching (it's always available)
			z=(MPD_SUB_ENTRY&0xff00)>>8;
			break;

		case TI_HDR_SUBPROGRAM+1:
			// subprogram address patching (it's always available)
			z=MPD_SUB_ENTRY&0xff;
			break;

		case TI_HDR_BASIC_PROGRAM:
			// 'set speed' link on 99/8 only
			if (nCurrentBank[0] == MPD_BANK_998GROM0) {
				z = (MPD_SPEED_ENTRY&0xff00)>>8;
			}
			break;

		case TI_HDR_BASIC_PROGRAM+1:
			// 'set speed' link on 99/8 only
			if (nCurrentBank[0] == MPD_BANK_998GROM0) {
				z = MPD_SPEED_ENTRY&0xff;
			}
			break;
	}

	return z;
}

void MpdHookNewAddress(Word nTestAddress) {
	// we only care about >0020 - that's the start of GPL execution and means we did a reset.
	// the console ROM doesn't check anything else in the GROM first, so this is handy.

	if (nTestAddress == TI_GROM_RESET) {
		// update settings
		nCurrentBank[0] = MpdGetConfigByte(MPD_EEPROM_GROM0);
		nCurrentBank[1] = MpdGetConfigByte(MPD_EEPROM_GROM1);
		nCurrentBank[2] = MpdGetConfigByte(MPD_EEPROM_GROM2);

		// using the configuration, load the appropriate GROMs
		debug_write("Resetting MPD, loading GROMs %d, %d and %d", nCurrentBank[0], nCurrentBank[1], nCurrentBank[2]);
		memcpy(&GROMBase[0].GROM[0x0000], &mpdGrom[nCurrentBank[0]*0x2000], 0x1800);		// 6k only!
		memcpy(&GROMBase[0].GROM[0x2000], &mpdGrom[nCurrentBank[1]*0x2000], 0x2000);		// 8k
		memcpy(&GROMBase[0].GROM[0x4000], &mpdGrom[nCurrentBank[2]*0x2000], 0x2000);		// 8k
	}
}

void MpdHookGROMWrite(Word nTestAddress, Byte c) {
	debug_write("MPD Write %d to >%04X", c, nTestAddress);

	if ((nTestAddress >= 0x1f10) && (nTestAddress <= 0x1F12)) {
		switch (nTestAddress) {
			// these ones change bank immediately
			case MPD_WRITE_GROM0:
				if (c >= MPD_BANK_COUNT) {
					debug_write("Warning: MPD writing illegal GROM bank %d to config at >%04X", c, nTestAddress);
					c=nDefaultConfigBytes[0];
				}
				nCurrentBank[0] = c;
				// These should be switch..case in the final so that bad values can load the default
				memcpy(&GROMBase[0].GROM[0x0000], &mpdGrom[c*0x2000], 0x1800);		// 6k only!
				break;

			case MPD_WRITE_GROM1:
				if (c >= MPD_BANK_COUNT) {
					debug_write("Warning: MPD writing illegal GROM bank %d to config at >%04X", c, nTestAddress);
					c=nDefaultConfigBytes[2];
				}
				nCurrentBank[1] = c;
				// These should be switch..case in the final so that bad values can load the default
				memcpy(&GROMBase[0].GROM[0x2000], &mpdGrom[c*0x2000], 0x2000);		// 8k only!
				break;

			case MPD_WRITE_GROM2:
				if (c >= MPD_BANK_COUNT) {
					debug_write("Warning: MPD writing illegal GROM bank %d to config at >%04X", c, nTestAddress);
					c=nDefaultConfigBytes[4];
				}
				nCurrentBank[2] = c;
				// These should be switch..case in the final so that bad values can load the default
				memcpy(&GROMBase[0].GROM[0x4000], &mpdGrom[c*0x2000], 0x2000);		// 8k only!
				break;
		}
	}

	// test for special bank mappings
	int nBank = 0;
	if ((nTestAddress >= 0x4000) && (nTestAddress < 0x6000)) nBank = 2;
	else if ((nTestAddress >= 0x2000) && (nTestAddress < 0x4000)) nBank = 1;
	
	if (nBank) {
		if (((nTestAddress&0x1fff) < 0x0800) && (nCurrentBank[nBank] == MPD_BANK_EEPROM)) {
			// configuration EEPROM (if mapped) (only 2k!)
			MpdWriteConfigByte(nTestAddress-(nBank*0x2000), c);
			GROMBase[0].GROM[nTestAddress] = c;
		}
		if (nCurrentBank[nBank] == MPD_BANK_RAM) {
			// SRAM (if mapped)
			mpdGrom[(MPD_BANK_RAM*8192)+(nTestAddress-(nBank*0x2000))] = c;
			GROMBase[0].GROM[nTestAddress] = c;
		}
	}
}
