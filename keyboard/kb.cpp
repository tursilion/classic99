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

// This code is mainly from my ATMEGA PS/2 adapter for the TI-99/4A

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "kb.h"
#include "scancodes.h"
#include "ti.h"

// keyboard state
unsigned char is_up=0, isextended=0, capslock=1, lockedshiftstate=0;
unsigned char scrolllock=0,numlock=1;
unsigned char fctnrefcount,shiftrefcount,ctrlrefcount;
unsigned char ignorecount;	// used to ignore the break key
unsigned char ticols[8]={ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
unsigned char cheatcode[10]={0,0,0,0,0,0,0,0,0,0};;
// cheat code is up,up,down,down,left,right,left,right,b,a (trigger with enter)
// because we don't save the meta information, it's also 88224646ba (digits on numpad)
unsigned const char cheatmatch[10]={ 0x75,0x75,0x72,0x72,0x6B,0x74,0x6B,0x74,0x32,0x1c };
unsigned const char *pCheat=NULL;
volatile int abortCheat=0;
int cheatidx=0;			// cheat code buffer is a ring buffer to reduce overhead
// Track whether the last key required a fctn, shift, or control press
unsigned char fLastMeta=0;
unsigned char nLastRow=-1, nLastCol=-1;

// emulator variables
extern int fJoystickActiveOnKeys;
extern int CtrlAltReset;
extern int gDontInvertCapsLock;
extern int enableSpeedKeys;
extern int enableEscape;
extern int enableAltF4;
extern bool mouseCaptured;
extern HWND myWnd;
extern const char *szDefaultWindowText;
extern volatile HWND dbgWnd;

enum LASTMETA {
	METANONE = 0,

	METAFCTN,
	METACTRL,
	METASHIFT
};

#define SHIFT_COL 7
#define FCTN_ROW 4
#define SHIFT_ROW 5
#define CTRL_ROW 6

void debug_write(char *s, ...);

void init_kb(void)
{
#ifdef _DEBUG
	if (fctnrefcount) {
		debug_write("init_kb fctnrefcount at %d", fctnrefcount);
	}
	if (shiftrefcount) {
		debug_write("init_kb shiftrefcount at %d", shiftrefcount);
	}
	if (ctrlrefcount) {
		debug_write("init_kb ctrlrefcount at %d", ctrlrefcount);
	}
#endif

	fctnrefcount=0;
	shiftrefcount=0;
	ctrlrefcount=0;
	ignorecount=0;

	if (GetKeyState(VK_CAPITAL) & 0x01) {
		capslock=1;
	} else {
		capslock=0;
	}

	if (GetKeyState(VK_SCROLL) & 0x01) {
		scrolllock=1;
	} else {
		scrolllock=0;
	}

	if (GetKeyState(VK_NUMLOCK) & 0x01) {
		numlock=1;
	} else {
		numlock=0;
	}

	for (int idx=0; idx<8; idx++) {
		ticols[idx]=0xff;
	}
}

void ParseRowCol(signed char row, signed char col, unsigned char fup) {
	if ((-1 == row) || (-1 == col)) {
		return;
	}

	if (col == SHIFT_COL) {
		if (row == FCTN_ROW) {
			// FCTN
			if (fup) {
				if (fctnrefcount) fctnrefcount--;
			} else {
				fctnrefcount++;
			}
//			debug_write("FCTN refcount to %d", fctnrefcount);
			if (fctnrefcount) {
				ticols[col]&=(unsigned char)~(1<<row);
			} else {
				ticols[col]|=(unsigned char)(1<<row);
				if (fLastMeta == METAFCTN) {
					// it cleared itself up!
					fLastMeta = METANONE;
				}
			}
			return;
		}
		if (row == SHIFT_ROW) {
			// Shift
			if (fup) {
				if (shiftrefcount) shiftrefcount--;
			} else {
				shiftrefcount++;
			}
			if (shiftrefcount) {
				ticols[col]&=(unsigned char)~(1<<row);
			} else {
				ticols[col]|=(unsigned char)(1<<row);
				if (fLastMeta == METASHIFT) {
					// it cleared itself up!
					fLastMeta = METANONE;
				}
			}
			return;
		}
		if (row == CTRL_ROW) {
			// Ctrl
			if (fup) {
				if (ctrlrefcount) ctrlrefcount--;
			} else {
				ctrlrefcount++;
			}
			if (ctrlrefcount) {
				ticols[col]&=(unsigned char)~(1<<row);
			} else {
				ticols[col]|=(unsigned char)(1<<row);
				if (fLastMeta == METACTRL) {
					// it cleared itself up!
					fLastMeta = METANONE;
				}
			}
			return;
		}
	}
	if (!fup) {
		ticols[col]&=(unsigned char)~(1<<row);
	} else {
		ticols[col]|=(unsigned char)(1<<row);
	}
}

// new decode for TI
void decode(unsigned char sc)
{
	static unsigned short nLastChar=0x00ff;	// used to detect autorepeat and ignore it
	const signed char *pDat;
	static bool bLastShift=false, bLastCtrl=false, bLastFctn=false;
	bool bTmp;

	// Some of the meta keys are not 100% reliable, so
	// we are just gonna fake them here for more reliability.
	if (GetKeyState(VK_CAPITAL) & 0x01) {
		// reverse caps lock for convenience with PCs that
		// usually have caps off, where the TI usually has it on
		capslock=0;
	} else {
		capslock=1;
	}
	if (gDontInvertCapsLock) capslock=!capslock;

	if (GetKeyState(VK_SCROLL) & 0x01) {
		scrolllock=1;
	} else {
		scrolllock=0;
	}
	if ((GetKeyState(VK_NUMLOCK) & 0x01) || (NULL != pCheat)) {
		numlock=1;
	} else {
		numlock=0;
	}

	bTmp=(GetKeyState(VK_SHIFT)&0x8000)?true:false;
	if (bTmp != bLastShift) {
		// inject a change to meta key state
		bLastShift=bTmp;
		ParseRowCol(SHIFT_ROW,SHIFT_COL,!bTmp);
	}
	bTmp=(GetKeyState(VK_CONTROL)&0x8000)?true:false;
	if (bTmp != bLastCtrl) {
		// inject a change to meta key state
		bLastCtrl=bTmp;
		ParseRowCol(CTRL_ROW,SHIFT_COL,!bTmp);
	}
	bTmp=(GetKeyState(VK_MENU)&0x8000)?true:false;
	if (bTmp != bLastFctn) {
		// inject a change to meta key state
		bLastFctn=bTmp;
		ParseRowCol(FCTN_ROW,SHIFT_COL,!bTmp);
	}

	// if debug is on, we don't pass on the F keys to the emulator
	// they are re-used for debug now
	if (NULL != dbgWnd) {
		switch (sc) {
			// this sucks, but they aren't deterministic
			case VK_F1:
			case VK_F2:
			case VK_F3:
			case VK_F4:
			case VK_F5:
			case VK_F6:
			case VK_F7:
			case VK_F8:
			case VK_F9:
			case VK_F10:
			case VK_F11:
			case VK_F12:
				// ignore the rest
				is_up = 0;	// we're ignoring the whole thing, so assume the up event is complete
				return;
		}
	}
    // also skip some of the keys we use for Windows hot keys
    if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
        // control is down
        switch (sc) {
        case VK_F1:		// ctrl+F1 - edit->paste
        case VK_F2:		// ctrl+F2 - edit->copy screen
        case VK_HOME:	// ctrl+HOME - edit->debugger
			is_up = 0;	// we're ignoring the whole thing, so assume the up event is complete
            return;
        }
    }
    if (enableSpeedKeys) {
		// disable the speed keys in that case too
		switch (sc) {
			case VK_F6:		// CPU normal
			case VK_F7:		// CPU Overdrive
			case VK_F8:		// System Maximum
			case VK_F11:	// Turbo toggle (normal/maximum)
				is_up = 0;	// we're ignoring the whole thing, so assume the up event is complete
				return;
		}
	}
	if (enableAltF4) {
		if ((GetAsyncKeyState(VK_MENU) & 0x8000) && (sc == VK_F4)) {
			is_up = 0;	// ignore it
			return;
		}
	}

	// check for escape disabling mouse capture before we check if escape is otherwise disabled
	if (sc == VK_ESCAPE) {
		if (mouseCaptured) {
			ReleaseCapture();
			mouseCaptured = false;
		}
		SetWindowText(myWnd, szDefaultWindowText);
	} 

	if (!enableEscape) {
		// disable escape if told to
		if (sc == VK_ESCAPE) {
			is_up = 0;	// we're ignoring the whole thing, so assume the up event is complete
			return;
		}
	}

	// Handle translation to US PS/2 keyboard raw scancodes
	if ((sc != 0xe0) && (sc != 0xf0) && (NULL == pCheat)) {
		// not extended or release, so convert from windows VK to set two US scancode
		// with a little luck, doing it this way will let Windows take care of the
		// scancode remapping on different keyboards! (TODO: But, it didn't).
		sc=VK2ScanCode[sc];
		if (0 == sc) {
			// was not a supported code, just ignore it
			is_up=0;
			isextended=0;
			return;
		}

		// extra hack - Windows is really bad about up events, so every time
		// enter is released, clear any of the refcounted data. This lets the
		// enter key clear any screwed up states
		if ((sc == 0x5a) && (is_up)) {
//			debug_write("Enter clears refcounted state");
			fctnrefcount=0;
			ticols[SHIFT_COL]|=(unsigned char)(1<<FCTN_ROW);
			shiftrefcount=0;
			ticols[SHIFT_COL]|=(unsigned char)(1<<SHIFT_ROW);
			ctrlrefcount=0;
			ticols[SHIFT_COL]|=(unsigned char)(1<<CTRL_ROW);
		}
	}

	// to port back to the Atmel code - don't allow FCTN-=, make it have to be FCTN-CTRL-=
	if (CtrlAltReset) {
		if (sc == 0x55) {		// '='
			if ((bLastFctn)&&(!bLastCtrl)) {
				is_up = 0;	// we're ignoring the whole thing, so assume the up event is complete
				return;
			}
		}
	}
	
	// back to the atmel code here
	if (ignorecount > 0) {
		if (sc != 0xf0) {
			ignorecount--;
		}
		is_up = 0;	// we're ignoring the whole thing, so assume the up event is complete
		return;
	}

	// else parse the key
	switch (sc) {
		// special keys
		case 0xF0:	// key up
			is_up=1;
			break;

		case 0xe0:	// extended code follows
			isextended=1;
			break;

		case 0xe1:	// break sequence - ignore next two keys
			ignorecount=2;
			break;

		case 0x58:	// caps lock
			if (!isextended) {
				if (is_up) {
					is_up=0;
				}
			}
			isextended=0;
			break;

		case 0x7e:	// scroll lock
			if (isextended) {
				// this is control-break, then, parse normally
				goto dodefault;
			}
			if (is_up) {
				is_up=0;
			}
			break;

		case 0x77:	// num lock
			if (!isextended) {
				if (is_up) {
					is_up=0;
				}
			}
			isextended=0;
			break;

		case 0x83:
			sc=0x7f;		// special case for F7, remap to less than 0x80
			goto dodefault;

		default:	// any other key
dodefault:
			// certain keys are remapped for numlock (but not scroll lock in Classic99)
			if (!numlock) {
				sc=remapnumlock(sc);
			}

			// not in Classic99
//			if (scrolllock) {
//				sc=remapscrolllock(sc);
//			}
			// Classic99 only - if the joystick is active on keyboard, don't respond to the arrow keys
			if (fJoystickActiveOnKeys) {
				if (isextended) {
					// check arrow keys
					switch (sc) {
					case 0x75:			// E
					case 0x6b:			// S
					case 0x72:			// X
					case 0x74:			// D
						isextended=0;
						is_up=0;
						return;			// ignore
					}
				} else if (sc == 0x0d) {	
					// tab key, ignore that too
					isextended=0;
					is_up=0;
					return;			// ignore
				}
			}

			if (sc < 0x80) {
				unsigned short nThisChar;

				if (isextended) {
					pDat=scan2ti994aextend[sc];
				} else if ((bLastShift)||(lockedshiftstate)) {
					pDat=scan2ti994ashift[sc];
					lockedshiftstate=sc;
				} else {
					pDat=scan2ti994aflat[sc];
				}

				// check for and ignore repeated characters
				// to avoid screwing up the meta key refcounts
				if (isextended) {
					nThisChar=0xe000|sc;
				} else {
					nThisChar=sc;
				}

				// Up codes don't autorepeat, so don't check them
				if ((is_up)||(nThisChar != nLastChar)) {
					signed char row1,col1=-1;
					signed char row2,col2=-1;
					
                    if (!is_up) {
					    nLastChar=nThisChar;
                    } else if (nLastChar == nThisChar) {
                        // was the last key, release it
                        // otherwise the user is probably holding two keys
                        nLastChar = 0x00ff;
                        // if it matches the lock scan code, release lock
                        if (lockedshiftstate == sc) {
                            lockedshiftstate = 0;
                        }
                    }

					row1=*(pDat);
					if (-1 != row1) {
						col1=*(pDat+1);
					}

					pDat+=2;

					row2=*(pDat);
					if (-1 != row2) {
						col2=*(pDat+1);
					}

					// fLastMeta tracks whether the last keypress
					// added a shift-style key that the user did not
					// explicitly press it, so we can turn it off it
					// we don't need it now. It can't help a string
					// of three keypresses. ;) The extra up event won't
					// cause a problem as the refcounting code can cope
					// with that.
					if (!is_up) {
						if ((METANONE != fLastMeta) && (col1 != SHIFT_COL) && (col2 != SHIFT_COL)) {
							switch (fLastMeta) {
								case METAFCTN:	// FCTN was added last
									if ((row1 != FCTN_ROW)&&(row2 != FCTN_ROW)) {
										// turn off last key (prevents errors on up event)
//										debug_write("Turning off meta FCTN");
										ParseRowCol(FCTN_ROW,SHIFT_COL,1);
										ParseRowCol(nLastRow,nLastCol,1);
									}
									break;

								case METACTRL:	// CTRL was added last
									if ((row1 != CTRL_ROW)&&(row2 != CTRL_ROW)) {
										// turn off last key (prevents errors on up event)
										ParseRowCol(CTRL_ROW,SHIFT_COL,1);
										ParseRowCol(nLastRow,nLastCol,1);
									}
									break;

								case METASHIFT:	// SHIFT was added last
									if ((row1 != SHIFT_ROW) && (row2 != SHIFT_ROW)) {
										// turn off last key (prevents errors on up event)
										ParseRowCol(SHIFT_ROW,SHIFT_COL,1);
										ParseRowCol(nLastRow,nLastCol,1);
									}
									break; 
							}
						}
					
						fLastMeta=METANONE;
					}

					if (-1 != row1) {
						ParseRowCol(row1,col1,is_up);
					}
					if (-1 != row2) {
						ParseRowCol(row2,col2,is_up);

						if (!is_up) {
							// Update meta - it may be either one, but it only counts when both were used!
							if (col1 == SHIFT_COL) {
								switch (row1) {
									case FCTN_ROW: fLastMeta=METAFCTN; break;
									case SHIFT_ROW: fLastMeta=METASHIFT; break;
									case CTRL_ROW: fLastMeta=METACTRL; break;
								}
								if (METANONE != fLastMeta) {
									nLastRow=row2;
									nLastCol=col2;
								}
							} else if (col2 == SHIFT_COL) {
								switch (row2) {
									case FCTN_ROW: fLastMeta=METAFCTN; break;
									case SHIFT_ROW: fLastMeta=METASHIFT; break;
									case CTRL_ROW: fLastMeta=METACTRL; break;
								}
								if (METANONE != fLastMeta) {
									nLastRow=row1;
									nLastCol=col1;
								}
							}
						}
					}
				}

				// check cheat codes - this is just to catch
				// people who dump the binary and copy it to resell,
				// and also for a bit of ego boost. Please do not
				// remove it. :)
				if (NULL == pCheat) {
					if (!is_up) {
						if (0x5a != sc) {	// not Enter down
							cheatcode[cheatidx++]=sc;
							if (cheatidx>9) cheatidx=0;
						}
					} else {
						if (0x5a == sc) {	// is Enter up
							int x;
							int y;
							x=cheatidx;
							y=0;
							while (y < 10) {
								if (cheatcode[x] != cheatmatch[y]) {
									break;
								}

								x++;
								if (x>9) x=0;
								y++;
							}
							
							if (y > 9) {
								// this is it - start the playback
								memset(cheatcode, 0, 10);
								pCheat=copyright;
								abortCheat=0;
							}
						}
					}
				} else {
					if ((abortCheat)&&(is_up)&&(0x5a == sc)) {
						pCheat=NULL;
						abortCheat=0;
					}
				}

//                debug_write("up:%d this:%3d last:%3d fctn:%3d", is_up, nThisChar, nLastChar, fctnrefcount);

			}
			isextended=0;
			is_up=0;
			break;
	}
}

// when numlock is off, the numeric keypad has alternate functions
unsigned char remapnumlock(unsigned char in) {
	// this is nice - the scan codes map the same as the real keys,
	// only they become extended
	if (isextended) {
		return in;
	}

	switch (in) {//Numpad digit
	case 0x69:	// 1
	case 0x6b:	// 4
	case 0x6c:	// 7
	case 0x70:	// 0
	case 0x71:	// Period
	case 0x72:	// 2
	case 0x74:	// 6
	case 0x75:	// 8
	case 0x7a:	// 3
	case 0x7d:	// 9
		isextended=1;	// lie and make it extended!
	}

	return in;
}

#if 0
// not used in Classic99
// when scroll lock is on, we remap the arrow keys to ESDX
unsigned char remapscrolllock(unsigned char in) {
	if (isextended) {
		switch (in) {
		case 0x75:
			isextended=0;
			return 0x24;	// E
		case 0x6b:
			isextended=0;
			return 0x1b;	// S
		case 0x72:
			isextended=0;
			return 0x22;	// X
		case 0x74:
			isextended=0;
			return 0x23;	// D
		}
	}
	return in;
}
#endif

// Please do not remove
void InjectCheatKey() {
	// assuming pCheat is running, read the next scancode and feed it into the system
	unsigned char dat;

	if (NULL != pCheat) {
		dat=*(pCheat++);
		if (0 == dat) {
			pCheat=NULL;
		} else {
			decode(dat);
		}
		if (dat == 0xf0) {		// release code
			InjectCheatKey();	// inject the next one too
		}
	}
}
