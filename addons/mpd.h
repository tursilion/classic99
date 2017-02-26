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

extern unsigned char mpdGrom[19*8192];	// there are 19 pages, 8k blocks used here (real core doesn't pad them)
extern bool bMpdActive;

// when Classic99 supports NVRAM, we should save these settings, as the real device will
#define MPD_EEPROM_OFFSET (MPD_BANK_EEPROM*8192)
// indexes into config
#define MPD_EEPROM_GROM0 0
#define MPD_EEPROM_GROM1 2
#define MPD_EEPROM_GROM2 4

// configuration write addresses
#define MPD_WRITE_GROM0	0x1f10
#define MPD_WRITE_GROM1	0x1f11
#define MPD_WRITE_GROM2	0x1f12

// indexes into cart header
#define TI_HDR_PROGRAM			0x0006
#define TI_HDR_SUBPROGRAM		0x000A
#define TI_HDR_BASIC_PROGRAM	0x2006

// readable hardware info
#define MPD_HZ_TIMER	0x1FFE

// Reset vector in GROM
#define TI_GROM_RESET	0x0020

// hard coded GROM addresses for tables which are dynamically used
#define MPD_MENU_ENTRY	0x1FE2
#define MPD_SUB_ENTRY	0x1FF2
#define MPD_SPEED_ENTRY	0x18E2			// 'SET SPEED' for the 99/8, only valid with non-99/4 BASIC

// predefined GROM banks
enum {
	MPD_BANK_994GROM0 = 0,
	MPD_BANK_994AGROM0,
	MPD_BANK_V22GROM0,
	MPD_BANK_998GROM0,
	MPD_BANK_TURSIGROM0,
	MPD_BANK_994BASIC1,
	MPD_BANK_994BASIC2,
	MPD_BANK_994ABASIC1,
	MPD_BANK_994ABASIC2,
	MPD_BANK_EACOMP1,
	MPD_BANK_EACOMP2,
	MPD_BANK_EACOMP3,
	MPD_BANK_EACOMP4,
	MPD_BANK_DM1000A,
	MPD_BANK_DM1000B,
	MPD_BANK_ARC303G,
	MPD_BANK_CONFIG,
	MPD_BANK_EEPROM,
	MPD_BANK_RAM,

	MPD_BANK_COUNT
};

Byte GetMpdOverride(Word nTestAddress, Byte z);
void MpdHookNewAddress(Word nTestAddress);
void MpdHookGROMWrite(Word nTestAddress, Byte nData);

