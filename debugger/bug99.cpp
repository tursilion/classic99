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
// bug99.cpp - main code for bug99 slave emulation
// create a window for Bug99 to communicate with
// The window must be titled "Bug99 calling"
// We will get the messages 0x9900 and 0x9901
//
// 9900 is a request to read the parallel port status:
// This comes back as a 16 bit value - the high byte represents the
// control bits, and the low byte represents the 8-bit data value.
// The only defined control bit is 'HandShakeOut', which is 0x0400
// We use 'ReplyMessage' to return the data
//
// 9901 is a request to write to the parallel port:
// wParam contains the control bits. The only defined control
// bit is HandshakeIn, which is 0x04. If it is 0xffff or -1, it is unchanged
// lParam contains the 8-bit data, -1 means unchanged
//

#include <windows.h>
#include <stdio.h>
#include "tiemul.h"
#include "..\resource.h"
#include "..\addons\rs232_pio.h"

#define BUG99_OFFSET 0x1900
#define BUGMSG_READ_PORT WM_APP+BUG99_OFFSET+0 
#define BUGMSG_WRITE_PORT WM_APP+BUG99_OFFSET+1 

HWND hBugWnd = NULL;

extern HWND myWnd;
extern int nCurrentDSR;
extern Byte rcpubyte(Word x,READACCESSTYPE rmw);
extern Word romword(Word x, READACCESSTYPE rmw);
extern void wcpubyte(Word x, Byte c);
extern void wcru(Word ad, int bt);
extern int rcru(Word ad);
extern void DoPlay();
extern void DoPause();

INT_PTR CALLBACK Bug99Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void InitBug99() {
	if (NULL == hBugWnd) {
		hBugWnd=CreateDialog(NULL, MAKEINTRESOURCE(IDD_BUG99), myWnd, Bug99Proc);
	}
}

// this many messages from Bug99 without changing state probably
// means we need to go back to the default command mode (slave does this too, but uses time)
INT_PTR CALLBACK Bug99Proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int nResult = 0;

	// nothing fancy here
    switch (uMsg) 
    { 
		// this is the new dedicated message way
		case BUGMSG_READ_PORT:
			// 9900 is a request to read the parallel port status:
			// This comes back as a 16 bit value - the high byte represents the
			// control bits, and the low byte represents the 8-bit data value.
			// The only defined control bit is 'HandShakeOut', which is 0x0400
			// We use 'ReplyMessage' to return the data
			nResult = GetExternalPIOOutputState();
			debug_write("Bug99 reading >%04X", nResult&0xffff);
			ReplyMessage(nResult);
			return TRUE;

		case BUGMSG_WRITE_PORT:
			// not so important to reply in the final version
			// 9901 is a request to write to the parallel port:
			// wParam contains the control bits. The only defined control
			// bit is HandshakeIn, which is 0x04. If it is 0xffff or -1, it is unchanged
			// lParam contains the 8-bit data, -1 means unchanged
			if (wParam == 0xffff) wParam = -1;		// handle a mistaken 16-bit set
			debug_write("Bug99 writing >%02X, >%02X", lParam&0xff, wParam&0xff);
			SetExternalPIOInputState(lParam, wParam);
			ReplyMessage(1);						// probably don't need this anymore...
			return TRUE;

		case WM_INITDIALOG:
			return TRUE;

		case WM_CLOSE:
			// don't close it, just hide it
			ShowWindow(hBugWnd, SW_HIDE);
			break;

    } 
    return FALSE; 
} 


