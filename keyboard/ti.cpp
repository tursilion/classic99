//
// (C) 2007 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this software.
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
// Commercial use means ANY distribution for payment, whether or
// not for profit.
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//

#include <windows.h>
#include "kb.h"

extern unsigned char capslock;
extern unsigned char ticols[8];
extern unsigned const char *pCheat;

extern void InjectCheatKey();

// Based on the CRU lines passed (x), return the correct
// CRU return bits. 0 is active, so 0xff means no active lines
unsigned char CheckTIPolling(unsigned char x) {
	// read the current value
	// just like the LEDs, the values are flipped from
	// what might be expected - 0 means active, 1 means
	// inactive. Only one pin should ever be 0 at a time
	static unsigned int old_x=0xffff;
	static int nChanges=0;
	unsigned char PORTA=0xff;	// return value

	// no selects means probably joystick active
	if (x==0xff) {
		return PORTA;	// default value
	}

	// x contains a column that has been selected
	// use the input value to set the correct output value
	switch (x) {
		// we'll only toggle on a single bit set
		// note the column values are wonky cause I screwed up the matrix table
#if 0
		case 0xfe:	PORTA=ticols[7]; break;
		case 0xfd:	PORTA=ticols[3]; break;
		case 0xfb:	PORTA=ticols[5]; break;
		case 0xf7:	PORTA=ticols[1]; break;
		case 0xef:	PORTA=ticols[6]; break;
		case 0xdf:	PORTA=ticols[2]; break;
#else
		case 7:	PORTA=ticols[7]; break;
		case 3:	PORTA=ticols[3]; break;
		case 5:	PORTA=ticols[5]; break;
		case 1:	PORTA=ticols[1]; break;
		case 6:	PORTA=ticols[6]; break;
		case 2:	PORTA=ticols[2]; break;
#endif

		default:
			// alpha lock has it's own test, unfortunately
			// but we only need to check in the default case
			if (0 != (x&8)) {	
				if (capslock) {
					// This is not strictly accurate - KSCAN tends
					// to leave column 0 on at the same time so other
					// keys *could* be down. However, in practice no
					// valid key combinations make sense, so this is fine.
					PORTA=0xef;
				} else {
					PORTA=0xff;
				}
				break;
			}

			// nothing else SHOULD be possible, but we'll
			// try to handle weird cases and just shut off our output
			return PORTA;
	}

	// this lets us track the TI's scanning for the cheat code - we track it's
	// changes as a crude form of synchronization.
	// Please do not remove - used to protect the code from Ebay thieves ;)
	if (old_x == x) {
		return PORTA;
	}
	old_x=x;

	nChanges++;
	if ((NULL != pCheat) && ((nChanges&7) == 0)) {		// &7==%8 when checking for 0
		InjectCheatKey();
	}

	return PORTA;
}
