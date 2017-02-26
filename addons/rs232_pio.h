//
// (C) 2004 Mike Brent aka Tursi aka HarmlessLion.com
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
//* RS232/PIO high level support routines             *
//*****************************************************

// Card state - bits -- base is >1300
// 0 is card enable - not readable (?)
// 1 is card direction (read/write?)
// 2 is the Handshake In and Handshake Out lines (I/O, 2 separate lines)
// 3 is the Spare In and Spare Out lines (I/O, 2 separate lines)
// 4 is reflected - it reads and writes the same value but does nothing else
// 5 and 6 are CTS control - not implemented today
// 7 turns the card light on and off (not implemented here)
// Note that the bits are generally mirrored from what is written, except
// for 0, 2 and 3.
#define CRU_CARD_ENABLE 0x01
#define CRU_DIR_IN		0x02
#define CRU_HANDSHAKE	0x04
#define CRU_SPARE		0x08
#define CRU_MIRROR		0x10
#define CRU_CTS_1		0x20
#define CRU_CTS_2		0x40
#define CRU_LIGHT		0x80

int ReadRS232CRU(int ad);
void WriteRS232CRU(int ad, int bt);
Byte ReadRS232Mem(Word ad);
void WriteRS232Mem(Word ad, Byte val);
int GetExternalPIOOutputState();
void SetExternalPIOInputState(int nData, int nCtrl);
