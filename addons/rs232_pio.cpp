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

// Some notes on the Printer emulation
// Epson FX-80 is a target printer supported in TI Artist+
// The programming manuals are available from Epson here: 
// http://www.epson.com/cgi-bin/Store/support/supDetail.jsp?infoType=Doc&oid=14285&prodoid=8490
//
// The color reference is the STAR NX-1000 Rainbow, supported
// by TI Artist+. It is compatible with the FX-80 and that's
// why that model was chosen.
//
// Epson's guide don't cover the color codes, but from looking
// at the output from the Star, and searching online,
// this looks feasible:
// ESC "r" n -- where 'n' is:
//	n = 0 Black   
//	n = 1 Red (Magenta)  
//	n = 2 Blue (Cyan)  
//	n = 3 Violet  
//	n = 4 Yellow  
//	n = 5 Orange  
//	n = 6 Green
//
// The test code I have also put a '4' after the color change reliably, may have to ignore that?

#include <windows.h>
#include "tiemul.h"
#include "rs232_pio.h"

// CRU system talks 
int nCRUOut = 0;	// all bits are zeroed
int nCRUIn = 0;		// all bits are zeroed

// Parallel port internal state
int nPortDataOut;		// 8 bits of data output
int nPortDataIn;		// 8 bits of data input (these are on the same IO pins and selected via CRU)

HANDLE hPort = INVALID_HANDLE_VALUE;
int dataIn;

// takes the shifted and translated address (ie: the bit number)
// return is 1 or 0
// address will be from 0x00 to 0x80
int ReadRS232CRU(int ad) {
	int rs232=0;

	if (ad < 8) {
		// PIO
		//	debug_write("TI reading CRU %d (value %d)", ad, nCRUIn & (1<<ad));
		if (ad > 0) debug_write("PIO not really supported");
		return nCRUIn & (1<<ad);
	} else if ((ad>=32)&&(ad<64)) {
		// RS232/1
		ad-=32;
		rs232=0;
	} else {
		// RS232/2
		ad-=64;
		rs232=1;
		debug_write("RS232/2 not supported");
		// TODO: don't allow it for now
		return 0;
	}

	// ad is offset, and rs232 indexes the uart to access
	// I'm not sure how I want to do this... maybe an async
	// process to receive the bytes so we can manage the status
	// bits correctly? Fetching like I do here, we have no way
	// to check for a pending byte...
	switch (ad) {
		case CRUR_RX_BUF_8_BITS  :
		case CRUR_RX_BUF_8_BITS+1:
		case CRUR_RX_BUF_8_BITS+2:
		case CRUR_RX_BUF_8_BITS+3:
		case CRUR_RX_BUF_8_BITS+4:
		case CRUR_RX_BUF_8_BITS+5:
		case CRUR_RX_BUF_8_BITS+6:
		case CRUR_RX_BUF_8_BITS+7:
			if (INVALID_HANDLE_VALUE == hPort) {
				return 0;
			} else {
				if (ad == CRUR_RX_BUF_8_BITS) {
					// try to fetch a new byte
					DWORD read=0;
					dataIn = 0;
					ReadFile(hPort, &dataIn, 1, &read, NULL);
					return (dataIn&1)?1:0;
				} else {
					return (dataIn&(1<<(ad-CRUR_RX_BUF_8_BITS))) ?1:0;
				}
			}
			break;

		case CRUR_RX_ERROR       :
		case CRUR_PARITY_ERROR   :
		case CRUR_OVERFLOW       :
		case CRUR_FRAME_ERROR    :
		case CRUR_FIRST_BIT      :
		case CRUR_RXING_BYTE     :
		case CRUR_RIN            :
		case CRUR_RX_INT         :
		case CRUR_EMISSION_INT   :
		case CRUR_TIMER_INT      :
		case CRUR_CTSRTS_INT     :
		case CRUR_RX_FULL        :
		case CRUR_EMISSION_EMPTY :
		case CRUR_OUTPUT_EMPTY   :
		case CRUR_TIMER_ERROR    :
		case CRUR_TIMER_ELAPSED  :
		case CRUR_RTS            :
		case CRUR_DSR            :
		case CRUR_CTS            :
		case CRUR_DSRCTS_CHANGED :
		case CRUR_REG_LOADING    :
		case CRUR_INT_OCCURRED   :
		default:
			return 0;
	}
}

// Emulator writes to RS232 card's CRU
void WriteRS232CRU(int ad, int bt) {
	int rs232 = 0;

	if (ad < 8) {
		if (ad > 0) debug_write("PIO not really supported");
		if (bt) {
			//debug_write("TI writing CRU %d (value %d)", ad, 1);
			// writing 1
			int nVal = 1<<ad;
			nCRUOut |= nVal;
			// handle mirroring
			nVal &= ~(CRU_HANDSHAKE|CRU_SPARE);
			nCRUIn |= nVal;
		} else {
			//debug_write("TI writing CRU %d (value %d)", ad, 0);
			// writing 0
			int nVal = ~(1<<ad);
			nCRUOut &= nVal;
			// handle mirroring
			nVal |= (CRU_HANDSHAKE|CRU_SPARE);
			nCRUIn &= nVal;
		}
	} else if ((ad>=32)&&(ad<64)) {
		// RS232/1
		ad-=32;
		rs232=0;
	} else {
		// RS232/2
		ad-=64;
		rs232=1;
		debug_write("RS232/2 not supported");
		return;
	}

	switch (ad) {
		case CRUW_VALUE_11_BITS  :
		case CRUW_LOAD_EMISSION  :
		case CRUW_LOAD_RECEIVE   :
		case CRUW_LOAD_INTERVAL  :
		case CRUW_LOAD_CONTROL   :
		case CRUW_TEST_MODE      :
		case CRUW_RTS            :
		case CRUW_ABORT          :
		case CRUW_EN_RX_INT      :
		case CRUW_EN_TX_INT      :
		case CRUW_EN_TIMER_INT   :
		case CRUW_EN_CTSDSR_INT  :
		case CRUW_RESET          :
			break;
	}
}

// read from RS232 memory space
Byte ReadRS232Mem(Word ad) {
	// only PIO direct access right now
	if (ad == 0x1000) {
		if (nCRUOut & CRU_DIR_IN) {
			debug_write("TI reading in port >%02X", (nPortDataIn & 0xff));
			return (nPortDataIn & 0xff);
		} else {
			debug_write("TI reading out port >%02X", (nPortDataOut & 0xff));
			return (nPortDataOut & 0xff);
		}
	}

	// something else
	return 0;
}
// write to RS232 memory space
void WriteRS232Mem(Word ad, Byte val) {
	// only PIO direct access right now
	if ((ad == 0x1000) && (0 == (nCRUOut & CRU_DIR_IN))) {
		debug_write("TI writing port >%02X", val);
		nPortDataOut = val;
	}
}

// local helper functions (mostly for Bug99)
// This function reads the parallel port as if it were a device attached to it
int GetExternalPIOOutputState() {
	// low byte is the parallel port data
	// next byte is the control output bits - 0x0400 is handshake out, 0x0800 is spare out
	return nPortDataOut | ((nCRUOut&(CRU_HANDSHAKE|CRU_SPARE)) << 8);
}

// this function sets the parallel port as if it were a device attached to it
// nData or nCtrl can be -1 to ignore and not change it
void SetExternalPIOInputState(int nData, int nCtrl) {
	if (nData != -1) {
		// update data
		nPortDataIn = nData&0xff;
	}
	if (nCtrl != -1) {
		// change the two dedicated input bits
		nCRUIn &= ~(CRU_HANDSHAKE|CRU_SPARE);
		nCRUIn |= nCtrl&(CRU_HANDSHAKE|CRU_SPARE);
	}
}

