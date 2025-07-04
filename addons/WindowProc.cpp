// TODO: add a hotkey to enable/disable the menu in full screen mode, 
// otherwise we can't change disks or quit without exitting full screen mode,
// which people want to use. Windows often uses f10, can we? It doesn't
// really do anything useful for us...

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
//*****************************************************
//* Classic99 - TI Emulator for Win32				  *
//* by M.Brent                                        *
//* Win32 WindowProc                                  *
//*****************************************************

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <windows.h>
#include <MMSystem.h>
#include <malloc.h>
#include <ddraw.h>
#include <commctrl.h>
#include <commdlg.h>
#include <atlstr.h>
#include <shellapi.h>

#include "..\resource.h"
#include "..\console\tiemul.h"
#include "..\console\cpu9900.h"
#include "..\addons\makecart.h"
#include "..\addons\screenReader.h"
#include "..\keyboard\kb.h"
#include "..\disk\diskclass.h"
#include "..\disk\FiadDisk.h"
#include "..\disk\ImageDisk.h"
#include "..\disk\TICCDisk.h"
#include "loadsave_brk.h"

extern CPU9900 * volatile pCurrentCPU;
extern CPU9900 *pCPU, *pGPU;
extern const char *szDefaultWindowText;
extern bool bIgnoreConsoleBreakpointHits;
extern HDC tmpDC;
extern int VDPDebug;
extern int CtrlAltReset;
extern int gDontInvertCapsLock;
extern int max_volume;
extern int starttimer9901;
extern int timer9901;										// 9901 interrupt timer
extern int timer9901Read;
extern int timer9901IntReq;
extern int nSystem;
extern int nCartGroup;
extern int nCart;
extern int keyboard, ps2keyboardok;
extern bool fKeyEverPressed;
extern void GenerateToneBuffer();
extern void GenerateSIDBuffer();
extern int max_cpf, cfg_cpf;
// needed for configuration
extern char AVIFileName[256];
extern int drawspeed, fJoy;
extern joyStruct joyStick[2];
extern int fJoystickActiveOnKeys;
extern int slowdown_keyboard;
extern HINSTANCE hInstance;						// global program instance
extern int TVFiltersAvailable;
extern int TVScanLines;
extern volatile signed long cycles_left;		// runs the CPU throttle
extern struct CARTS *Users;
extern int nTotalUserCarts;
extern struct _break BreakPoints[];
extern int nBreakPoints;
extern char lines[DEBUGLINES][DEBUGLEN];				// debug lines
extern bool bDebugDirty;
extern struct history Disasm[DEBUGLINES];				// last x addresses for disasm
extern bool bScrambleMemory;
extern RECT gWindowRect;
extern bool bCorruptDSKRAM;
extern unsigned char UberGROM[120*1024];
extern unsigned char UberRAM[15*1024];
extern unsigned char UberEEPROM[4*1024];
extern bool bWindowInitComplete;
extern CString csCf7Bios;
extern int bEnableAppMode;
extern int enableF10Menu;
extern int enableAltF4;
extern char AppName[];
extern Byte SidCache[29];
extern int WindowActive;
extern int enableSpeedKeys;
extern double gMouseScale;		// TIPI mouse
extern bool mouseCaptured;
extern int logAudio;
extern bool openAudioLogFiles();
extern void closeAudioLogFiles();

// window
extern int nVideoLeft, nVideoTop;
extern int bAppLockFullScreen;

// VDP tables
extern int SIT, CT, PDT, SAL, SDT, CTsize, PDTsize;
extern bool CPUSpeechHalt;
extern Byte CPUSpeechHaltByte;
extern int cpucount, cpuframes;					// CPU counters for timing
extern int timercount;							// Used to estimate runtime
extern bool bDisableBlank, bDisableSprite, bDisableBackground;
extern bool bDisableColorLayer, bDisablePatternLayer;
extern int bEnable80Columns, bEnable128k, bF18Enabled, bInterleaveGPU;
extern int bShowFPS;
extern int bShowKeyboard;
extern int statusReadLine;
extern int statusReadCount;
// sams config
extern int sams_enabled, sams_size;
extern Byte staticCPU[0x10000];					// main memory for debugger
extern Word mapperRegisters[16];
Byte ReadRawAMS(int address);
void WriteRawAMS(int address, int value);
void dumpMapperRegisters();
void WriteMapperRegisterByte(Byte reg, Byte value, bool highByte, bool force);
// sound
extern int nRegister[4];						// frequency registers
extern int nVolume[4];							// volume attenuation
extern bool enableBackgroundHum;
// back buffer for sizing
extern DDSURFACEDESC2 CurrentDDSD;
// debugger
extern HWND hBugWnd;
extern bool BreakOnIllegal, BreakOnDiskCorrupt;
extern bool bWarmBoot;
extern int installedJoysticks;
extern bool gResetTimer;

extern void sound_init(int freq);
extern void SetSoundVolumes();
extern void MuteAudio();
extern void resetDAC();
extern void ReloadDumpFiles();

extern void (*InitSid)();
extern void (*sid_update)(short *buf, double nAudioIn, int nSamples);
extern void (*write_sid)(Word ad, Byte dat);
extern void (*SetSidFrequency)(int);
extern void (*SetSidEnable)(bool);
extern void (*SetSidBanked)(bool);
extern bool (*GetSidEnable)(void);
extern SID* (*GetSidPointer)(void);

extern CString csLastDiskImage[MAX_MRU];
extern CString csLastDiskPath[MAX_MRU];
extern CString csLastUserCart[MAX_MRU];

extern void RestoreWindowPosition();

const char *pCurrentHelpMsg=NULL;
HWND hKBMap=NULL;
HWND hHeatMap=NULL;
HWND hBrkHlp=NULL;
HWND hTVDlg=NULL;
HBITMAP hHeatBmp=NULL;

// used for dynamic titles
static char speedTitle[256];

// used for debug window edit controls
void newPaint(HWND hWnd, LPPAINTSTRUCT lpPS);
LRESULT CALLBACK newEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND ctrl1,ctrl2;
HFONT mainfont;
char szCaption1[4096] = {0}, szCaption2[4096] = {0};

// used to initialize the disk config dialog 
int g_DiskCfgNum;

// whether logging disassembly to disk
FILE *fpDisasm = NULL;
int disasmLogType = 0;  // 0 = all, 1 = exclude < 2000
CRITICAL_SECTION csDisasm;  // initialized in tiemul.c

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(x) (x&0xffff)
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(y) (y>>16)
#endif

static LONG nOldVolume[11]={ -10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000};
static const char hexstr[17] = "0123456789ABCDEF";

// Stuff for the debug window
HANDLE hDebugWindowUpdateEvent = CreateEvent(NULL, false, false, NULL);
static int nMemType=0;
static bool bFrozenText=false;
static int nDebugHexOffset=0;
// top addresses for the memory banks
static char szTopMemory[6][32] = { "", "", "8300", "0000", "0000", "0000" };		// CPU, VDP, GROM, AMS - hex address or Register number (Rx)
// these match the array which matches the radio buttons 
#define MEMCPU 2
#define MEMVDP 3
#define MEMGROM 4
#define MEMAMS 5

// getting back out of full screen
bool preFullSet = false;
int preFullX, preFullY, preFullXS, preFullYS;

// references
// CartDlgProc is in makecart.cpp
INT_PTR CALLBACK DebugBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK BreakPointHelpProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ConfigureDisk(HWND hwnd, int nDiskNum);
INT_PTR CALLBACK DiskBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int EmitDebugLine(char cPrefix, struct history obj, CString &csOut, int &lines);
void UpdateUserCartMRU();

// checks for open files. Returns true to continue or false to abort
bool VerifyOpenFiles(HWND hwnd) {
	EnterCriticalSection(&csDriveType);

    bool isOpen = false;

    // check if we should warn - we only warn if the file is open for anything other than INPUT
	for (int idx=0; idx<MAX_DRIVES; idx++) {
		if (NULL != pDriveType[idx]) {
			if (pDriveType[idx]->CheckOpenFiles(true)) {
                isOpen = true;
            }
		}
	}

    if (isOpen) {
		if (IDYES != MessageBox(hwnd, "Please close all open files before changing disk configuration. If you are sure you want to override this and force files to close, click YES.", "Files open - data may be lost if you proceed.", MB_YESNO | MB_ICONASTERISK)) {
			LeaveCriticalSection(&csDriveType);
			return false;
		}
    }

	for (int idx=0; idx<MAX_DRIVES; idx++) {
		if (NULL != pDriveType[idx]) {
			if (pDriveType[idx]->CheckOpenFiles(false)) {
                debug_write("Force closing files on %s", pDriveType[idx]->GetDiskTypeAsString());
				pDriveType[idx]->CloseAllFiles();
			}
		}
	}

    LeaveCriticalSection(&csDriveType);
    return true;
}

// add a string to the MRU list
void addMRU(CString *pList, CString newStr) {
    // We move this string to the top of the list, then
    // if the string is already there, remove it, else
    // remove the last entry.

    // first, we need to make room for this string
    int remove;
    for (remove=0; remove<MAX_MRU; ++remove) {
        if (pList[remove] == newStr) break;
    }

    // if we fell off the end of the list, just remove the last
    // element
    if (remove >= MAX_MRU) {
        for (int idx=MAX_MRU-1; idx>0; --idx) {
            pList[idx] = pList[idx-1];
        }
    } else {
        // else just remove THIS element
        for (int idx=remove; idx>0; --idx) {
            pList[idx] = pList[idx-1];
        }
    }

    // and save this one in the first slot
    pList[0] = newStr;
}

// ofn - reference to OPENFILENAME string with at least hwndOwner, lpstrFileTitle, lpstrFile, nFileExtension
void OpenUserCart(OPENFILENAME &ofn) {
    int nLetterOffset=2;
    char *pPartIdx=NULL;
	int nCnt, nIdx, nPart, nOriginalCharacter;

	const int nUsr=0;		// we always use #0 now

	strncpy(Users[nUsr].szName, ofn.lpstrFileTitle, sizeof(Users[nUsr].szName));
	Users[nUsr].szName[sizeof(Users[nUsr].szName)-1]='\0';
	Users[nUsr].pDisk=NULL;
	Users[nUsr].szMessage=NULL;
	nCnt=0;

	// clear out all previous roms detected here
	for (int idx=0; idx<MAXROMSPERCART; ++idx) {
		Users[nUsr].Img[idx].nType = TYPE_NONE;
	}
                            
    // we now have to support all the different legacy naming types, /AND/ a new concept with
    // no extension whatsoever. That confuses a lot of this code, so we skip over it in that case...
    if (ofn.nFileExtension <= 1) {
        // we have no extension to play with
        nLetterOffset = -1;
        pPartIdx = NULL;
    } else {
        // we have an extension to play with...
    	if (0 != _stricmp(&ofn.lpstrFile[ofn.nFileExtension], "bin")) nLetterOffset=0;	// newer ".C" type
        nOriginalCharacter = ofn.lpstrFile[ofn.nFileExtension-nLetterOffset];

		// Someone wrote one of those ROM rename tools, and thought it would be smart to put
		// "(Part x of y)" in the filename. Since 'x' changes with every file, and the order
		// is non-deterministic (possibly), this becomes a bit of a pain. We'll assume 1-3
		// since only V9T9 carts should be indexed that way. Another good reason for the RPK carts...
		// Anyway, we'll make a semi-honest effort for them...case must match!
		pPartIdx = strstr(ofn.lpstrFile, "(Part ");
		if (pPartIdx == NULL) {
			// And... of course, someone decided they didn't like "Part" and used "File" for a set,
			// so again, %$%#@$ you very much. THE FILENAME IS THE METADATA, %$#%@#$@$#!
			pPartIdx = strstr(ofn.lpstrFile, "(File ");
		}

		if (NULL != pPartIdx) {
			pPartIdx+=6;
			if ((!isdigit(*pPartIdx)) || (0 != memcmp(pPartIdx+1, " of ", 4))) {
				// Not "(Part x of " - won't worry about the rest
				pPartIdx=NULL;
			}
		}
    }

    // label for goto.
    tryagain:
	for (nPart = 0; nPart<3; nPart++) {
        // if not doing "part" filenames, only process the first one
		if ((pPartIdx == NULL) && (nPart > 0)) continue;
                        
        // else update the part index
		if (pPartIdx != NULL) {
			*pPartIdx = nPart+'1';
		}

        // now check for all the possible letter extensions, unless we have no letter extensions.
        // in that case, just process the one single file
		for (nIdx=0; nIdx<6; nIdx++) {
            if (nLetterOffset == -1) {
                // assume non-inverted possibly-banked ROM
				Users[nUsr].Img[nCnt].nType=TYPE_378;
            } else {
				switch (nIdx) {
					case 0:	
						ofn.lpstrFile[ofn.nFileExtension-nLetterOffset]='C'; 
						Users[nUsr].Img[nCnt].nType=TYPE_ROM;
						break;
					case 1: 
						ofn.lpstrFile[ofn.nFileExtension-nLetterOffset]='D'; // legacy
						Users[nUsr].Img[nCnt].nType=TYPE_XB;
						break;
					case 2: 
						ofn.lpstrFile[ofn.nFileExtension-nLetterOffset]='G'; 
						Users[nUsr].Img[nCnt].nType=TYPE_GROM;
						break;
					case 3: 
						ofn.lpstrFile[ofn.nFileExtension-nLetterOffset]='3'; // legacy mode
						Users[nUsr].Img[nCnt].nType=TYPE_379;
						break;
					case 4: 
						ofn.lpstrFile[ofn.nFileExtension-nLetterOffset]='9';
						Users[nUsr].Img[nCnt].nType=TYPE_379;
						break;
					case 5: 
						ofn.lpstrFile[ofn.nFileExtension-nLetterOffset]='8'; 
						Users[nUsr].Img[nCnt].nType=TYPE_378;
						break;
				}
            }
			if (nCnt < 5) {		// maximum number of autodetect types
				FILE *fp=fopen(ofn.lpstrFile, "rb");
				if (NULL != fp) {
					fseek(fp, 0, SEEK_END);
                    // except for PART types, which we aren't going to rescan at load,
                    // we can set TYPE_AUTO here to make it redetect the type/size at load time
                    // This might be obsolete, being in two places... see LoadOneImg
                    if (pPartIdx == NULL) {
                        Users[nUsr].Img[nCnt].nOriginalType = TYPE_AUTO;    // setting auto will let it redetect size in the LoadOneImg
                    }
					Users[nUsr].Img[nCnt].dwImg=NULL;
					Users[nUsr].Img[nCnt].nLength=ftell(fp);
                    // And it gets better /still/, FinalGROM users are now using "C.BIN" to
                    // name /bank switched/ carts. THANK YOU VERY $%#%#$%# MUCH. I only
                    // spent 15 years establishing the standard. Now I am spending hours
                    // on "why doesn't my ROM work?????" Because it %$#@%$#@ LIES TO THE EMULATOR.
					// We should have used a header.
                    if ((Users[nUsr].Img[nCnt].nType == TYPE_ROM) && (Users[nUsr].Img[nCnt].nLength > 8192)) {
                        // at least we can assume it's non-inverted. Thank you for small favors.
                        debug_write("Cartridge image is improperly named - C.BIN images max 8k. Treating as type 8.");
                        Users[nUsr].Img[nCnt].nType = TYPE_378;
                        debug_write("Resize will not be available.");
                        Users[nUsr].Img[nCnt].nOriginalType = 0;
                    }
                    // might as well check the opposite, since the no-extension case could happen
                    if ((Users[nUsr].Img[nCnt].nType == TYPE_378) && (Users[nUsr].Img[nCnt].nLength <= 8192)) {
                        // can't bank less than 8k
                        debug_write("Cartridge image is improperly named - not banked. Treating as type C.");
                        Users[nUsr].Img[nCnt].nType = TYPE_ROM;
                        debug_write("Resize will not be available.");
                        Users[nUsr].Img[nCnt].nOriginalType = 0;
                    }
					if ((Users[nUsr].Img[nCnt].nType==TYPE_378)||(Users[nUsr].Img[nCnt].nType==TYPE_379)||(Users[nUsr].Img[nCnt].nType==TYPE_MBX)) {
						Users[nUsr].Img[nCnt].nLoadAddr=0x0000;
					} else {
						Users[nUsr].Img[nCnt].nLoadAddr=0x6000;
					}
					strncpy(Users[nUsr].Img[nCnt].szFileName, ofn.lpstrFile, 1024);
					Users[nUsr].Img[nCnt].szFileName[1023]='\0';
					nCnt++;
					fclose(fp);
					debug_write("Found %s...", ofn.lpstrFile);
				} else {
					Users[nUsr].Img[nCnt].nType=TYPE_NONE;
				}
			} else {
                debug_write("Impossible case - too many auto-detected files. Skipping %s", ofn.lpstrFile);
            }

            // if we aren't searching for extensions, then just break out
            if (nLetterOffset == -1) break;
		}
	}

    // oh, and sometimes they have a ".bin" anyway, so it's actually completely fdsafneqing impossible to
    // accurately determine if the last character is a tag or part of the filename, but if we didn't find
    // anything at all, then assume the user at least tried and go back and try as a raw name.
    if ((nCnt == 0) && (nLetterOffset != -1)) {
        ofn.lpstrFile[ofn.nFileExtension-nLetterOffset] = nOriginalCharacter;
        nLetterOffset = -1;
        // but if you mix that concept with a Part 1 of 2 filename, I give up. You're on your own.
        goto tryagain;  // %$@%^$@#. Bite me.
    }

	nCartGroup=2;
	nCart=nUsr;

	for (int idx=0; idx<100; idx++) {
		CheckMenuItem(GetMenu(myWnd), ID_USER_0+idx, MF_UNCHECKED);
	}
	for (int idx=0; idx<100; idx++) {
		CheckMenuItem(GetMenu(myWnd), ID_GAME_0+idx, MF_UNCHECKED);
	}
	for (int idx=0; idx<100; idx++) {
		CheckMenuItem(GetMenu(myWnd), ID_APP_0+idx, MF_UNCHECKED);
	}

    // update the MRU
    addMRU(csLastUserCart, Users[nUsr].Img[0].szFileName);
    UpdateUserCartMRU();

	SendMessage(ofn.hwndOwner, WM_COMMAND, ID_FILE_RESET, 0);
}

// return clipboard data, processed to ASCII.
// Returns NULL if nothing found. Otherwise, the returned string must be freed.
// IE only exports usable data in unicode, and this will probably grow. Wish I'd
// thought of just converting unicode before, but it took browsing how others do it
// to figure it out. ;) Anyway, no more enhanced clipboard!
char *GetProcessedClipboardData(bool *pError) {
	char *pRet = NULL;

	// no error by default
	if (NULL != pError) *pError = false;

	if (OpenClipboard(myWnd)) {
		HANDLE data;
		UINT fmt = 0;

#if 0
		// enumerate the clipboard formats
		for (;;) {
			char str[128];
			fmt = EnumClipboardFormats(fmt);
			if (fmt==0) break;
			GetClipboardFormatName(fmt, str, 128);
			debug_write("Found clipboard format %d, name %s", fmt, str);
			data=GetClipboardData(fmt);
		}
#endif

		// try unicode first - the world is moving this way
		data = GetClipboardData(CF_UNICODETEXT);
		if (NULL != data) {
			// got it - so we can convert it down to ASCII
			unsigned int len;
			
			// figure out length of data - see MSDN on CP_ACP - temporary use only!
			len = WideCharToMultiByte(CP_ACP, 0, (LPWSTR)data, -1, NULL, 0, NULL, NULL);
			pRet = (char*)malloc(len+1);		// supposed to include NUL byte! But we'll play safe
			memset(pRet, 0, len+1);
			WideCharToMultiByte(CP_ACP, 0, (LPWSTR)data, -1, pRet, len, NULL, NULL);
		} else {
			// try for raw ASCII
			data=GetClipboardData(CF_TEXT);

			if (NULL != data) {
				pRet=(char*)malloc(strlen((char*)data)+1);
				strcpy(pRet, (char*)data);
			}
		}
		CloseClipboard();
	} else {
		debug_write("Failed to open clipboard, err %d", GetLastError());
		if (NULL != pError) *pError = true;
	}
	
	return pRet;
}

// helper function
void EnableDlgItem(HWND hwnd, int id, BOOL bEnable) {
	HWND hCtl = GetDlgItem(hwnd, id);
	if (NULL != id) {
		EnableWindow(hCtl, bEnable);
	}
}

// checks if a pointer is inside a BASIC/XB string
bool InAString(char *pSrc, char *p) {
	bool bRet = false;
	while (pSrc < p) {
		if (*pSrc == '\"') {
			pSrc++;
			if (*pSrc != '\"') {
				bRet=!bRet;
			}
		}
		pSrc++;
	}
	return bRet;
}

// XB space stripper - modifies pStr!
void ParseForXB(char *pStr) {
	char *p;
	char *pWork;

	// remove leading spaces
	pWork = pStr;
	while (*pWork == ' ') strcpy(pWork, pWork+1);		// first line
	while (NULL != (p = strstr(pWork, "\r\n "))) {
		// first make sure we are not inside a string (unlikely in this case)
		if (!InAString(pStr, p)) {
			strcpy(p+2, p+3);
			pWork = p;
		} else {
			pWork = p+2;
		}
	}

	// remove spaces after multi-statement separators
	pWork = pStr;
	while (NULL != (p = strstr(pWork, ":: "))) {
		// first make sure we are not inside a string
		if (!InAString(pStr, p)) {
			strcpy(p+2, p+3);
			pWork = p;
		} else {
			pWork = p+2;
		}
	}

	// remove spaces before multi-statement separators,
	// EXCEPT if there is a COLON there (for PRINT)
	pWork = pStr;
	while (NULL != (p = strstr(pWork, " ::"))) {
		// also make sure we are not inside a string
		// the colon check prevents collapsing "10 PRINT : : :: GOTO 10"
		// stupid freaking syntax, btw, ANSI. Or is this one TI's fault?
		if ((!InAString(pStr, p)) && ((p > pWork) && (*(p-1) != ':'))) {
			strcpy(p, p+1);
			pWork = p-1;
			if (pWork < pStr) pWork = pStr;
		} else {
			pWork = p+1;
		}
	}

	// remove spaces before leading quotes
	pWork = pStr;
	while (NULL != (p = strstr(pWork, " \""))) {
		// first make sure we are not inside a string
		if (!InAString(pStr, p)) {
			strcpy(p, p+1);
			pWork = p-1;
			if (pWork < pStr) pWork = pStr;
		} else {
			pWork = p+1;
		}
	}

	// remove blank lines
	pWork = pStr;
	while (NULL != (p = strstr(pWork, "\r\n\r\n"))) {
		// first make sure we are not inside a string (unlikely in this case)
		if (!InAString(pStr, p)) {
			strcpy(p, p+2);
			pWork = p;
		} else {
			pWork = p+2;
		}
	}

	// remove lines that start with an exclamation mark (comments)
	pWork = pStr;
	while (*pWork == '!') {
		// first line
		// find the end
		char *p2 = strstr(pWork, "\r\n");
		if (NULL != p2) {
			strcpy(pWork, p2+2);
		} else {
			pWork++;
		}
	}
	while (NULL != (p = strstr(pWork, "\r\n!"))) {
		// first make sure we are not inside a string (unlikely in this case)
		if (!InAString(pStr, p)) {
			// find the end
			char *p2 = strstr(p+2, "\r\n");
			if (NULL != p2) {
				strcpy(p, p2);
				pWork = p;
			} else {
				pWork = p+3;
			}
		}
		pWork = p+2;
	}

	// remove spaces after line number
	pWork = pStr;
	while (isdigit(*pWork)) {					// we assume to start with a line number, everything else is filtered
		while (isdigit(*pWork)) pWork++;
		while (isspace(*pWork)) strcpy(pWork, pWork+1);
		// find next line, if any
		pWork = strstr(pWork, "\r\n");
		if (NULL == pWork) break;
		pWork+=2;
	}

}

/////////////////////////////////////////////////////////////////////////
// Window handler
/////////////////////////////////////////////////////////////////////////
LONG_PTR FAR PASCAL myproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// winuser.h has the VK_key key defines
	// also fill in the key[] array for on/off
	// WM_USER is used to alias ShowCursor(wParam)
    
	PAINTSTRUCT ps;
    HDC hDC;
	char szTemp[1024];
	float height;
	int i1;
	RECT myrect, myrect2;

	if (myWnd == hwnd) {	// Main TI window
		switch(msg) {
		case WM_USER:	
			// wrap ShowCursor()
			if (wParam) {
                int cnt = ShowCursor(TRUE);
                debug_write("Show cursor got %d", cnt);
                // if we didn't get to zero call a few more times
                // note that no mouse will lock it at -1, so this
                // loop will try once more to no avail
                for (int x = cnt; x < 0; ++x) {
                    ShowCursor(TRUE);
                }
			} else {
                int cnt = ShowCursor(FALSE);
                debug_write("Hide cursor got %d", cnt);
                // same thing on the hide side, but there's no infinite case
                for (int x = cnt; x >= 0; --x) {
    				ShowCursor(FALSE);
                }
			}
			break;

		case WM_MOUSEWHEEL:
			{
				if (mouseCaptured) {
					int delta = (int)((double)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA + 0.5);
					gMouseScale += (double)delta/10.0;
					if (gMouseScale > 5) gMouseScale = 5;
					if (gMouseScale < 0.1) gMouseScale = 0.1;
				}
			}
			break;

		case WM_INITMENUPOPUP:
			if (IsClipboardFormatAvailable(CF_TEXT)) {
				EnableMenuItem(GetMenu(myWnd), ID_EDITPASTE, MF_ENABLED | MF_BYCOMMAND);
				EnableMenuItem(GetMenu(myWnd), ID_EDIT_PASTEXB, MF_ENABLED | MF_BYCOMMAND);
			} else {
				EnableMenuItem(GetMenu(myWnd), ID_EDITPASTE, MF_GRAYED | MF_BYCOMMAND);
				EnableMenuItem(GetMenu(myWnd), ID_EDIT_PASTEXB, MF_GRAYED | MF_BYCOMMAND);
			}
			break;

		case WM_UNINITMENUPOPUP:
			// used to restore audio, not needed anymore, was preventing menu options
			// from causing a mute (Rasmus' breakpoint bug!)
			break;

		case WM_DISPLAYCHANGE:
			if (NULL != tmpDC) {
				DeleteDC(tmpDC);
				tmpDC=CreateCompatibleDC(NULL);
			}
			break;
		
		case WM_PAINT:
			hDC = BeginPaint(hwnd, &ps);
			GetClientRect(myWnd, &myrect);
			if (StretchMode==STRETCH_NONE) {
				FillRect(hDC, &myrect, (HBRUSH)(COLOR_MENU+1));
			}
			SetEvent(BlitEvent);
			EndPaint(hwnd, &ps);
			break;

		case WM_DESTROY:
			if (!GetWindowRect(myWnd, &gWindowRect)) {
				gWindowRect.left = -1;
				gWindowRect.top = -1;
			}
			quitflag=1;
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
			key[wParam]=1;
			if (lParam&0x1000000) {	// bit 24 is an extended key
				decode(0xe0);	// extended
			}
			decode(wParam);
    		fKeyEverPressed=true;
			break;

		case WM_KEYUP:
			key[wParam]=0;
			if (lParam&0x1000000) { // bit 24 is an extended key
				decode(0xe0);	// extended
			}
			decode(0xf0);	// key up
			decode(wParam);
			break;

		case WM_SYSKEYDOWN:				// returns from ALT and ALT+KEY (I use as FCTN)
			// some system keys we want Windows to process, but we don't take F4 anymore
			if (wParam != VK_RETURN)
			{	
				key[wParam]=1;
				if (lParam&0x1000000) {	// bit 24 is an extended key
					decode(0xe0);	// extended
				}
				decode(wParam);
			}
			else
			{
				return(DefWindowProc(hwnd, msg, wParam, lParam));
			}
			break;

		case WM_SYSKEYUP:
			key[wParam]=0;
			if (lParam&0x1000000) {	// bit 24 is an extended key
				decode(0xe0);	// extended
			}
			decode(0xf0);	// key up
			decode(wParam);
			break;

		case WM_SYSCHAR:
			// Don't remove this check, even if we need no ALT keys - otherwise all FCTN keys on the TI ding ;)
			// Fullscreen toggle - Alt-Enter
			if ((wParam==VK_RETURN)&&((lParam&0x8000)==0)) {	// TODO: bit 15 is ??? Part of repeat count?
				// don't toggle if full screen is locked
				if (!bAppLockFullScreen) {
					if (STRETCH_FULL == StretchMode) {
						StretchMode=STRETCH_DX;
						PostMessage(hwnd, WM_COMMAND, ID_VIDEO_STRETCHMODE_NONE+StretchMode, 1);
					} else {
						if (STRETCH_DX == StretchMode) {
							StretchMode=STRETCH_FULL;
							PostMessage(hwnd, WM_COMMAND, ID_VIDEO_STRETCHMODE_NONE+StretchMode, 1);
						}
					}
				} else {
					debug_write("Saw alt+enter but full screen is locked.");
				}
			}
			break;

		case WM_LBUTTONDBLCLK: 
			{
				// If in text mode, paste the character under the mouse cursor to the TI
				char ch;
				if (NULL != PasteString) {
					Beep(550,100);
					break;
				}
				GetClientRect(myWnd, &myrect);
				ch=VDPGetChar(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), myrect.right-myrect.left, myrect.bottom-myrect.top);
				if (-1 != ch) {
					PasteString=(char*)malloc(2);
					PasteString[0]=ch;
					PasteString[1]='\0';
					PasteIndex=PasteString;
				}
			}
			break;

        case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            char buf[1024];
            // we only care about the first file - even if the user dropped multiple
            if (DragQueryFile(hDrop, 0, buf, sizeof(buf))) {
			    int ret = IDNO;
                DragFinish(hDrop);
                if (VerifyOpenFiles(hwnd)) {
			        if (fKeyEverPressed) {
				        ret=MessageBox(hwnd, "This will reset the emulator - are you sure?", "Load cartridge", MB_YESNO|MB_ICONQUESTION);
			        } else {
				        ret=IDYES;
			        }
                }
			    if (IDYES == ret) {
                    // Make a fake OPENFILENAME
                    // hwndOwner, lpstrFileTitle, lpstrFile, nFileExtension
                    OPENFILENAME ofn;
                    memset(&ofn, 0, sizeof(ofn));
                    ofn.hwndOwner = myWnd;                      // my window
                    
                    ofn.lpstrFileTitle = strrchr(buf, '\\');    // file name and extension
                    if (ofn.lpstrFileTitle == NULL) {
                        ofn.lpstrFileTitle=buf;
                    } else {
                        ++ofn.lpstrFileTitle;
                    }

                    ofn.lpstrFile = buf;                        // full filename

                    char *p = strrchr(buf, '.');
                    if (NULL == p) {
                        ofn.nFileExtension = 0;
                    } else {
                        ofn.nFileExtension = p-buf+1;           // distance to extension
                    }

                    OpenUserCart(ofn);
                }
            } else {
                DragFinish(hDrop);
            }
        }
        break;

		case WM_COMMAND:
			// silence in case this takes a while
			// Check for dynamic ones first, so we don't need a huge switch
			if ((wParam >= ID_SYSTEM_0) && (wParam < ID_SYSTEM_0+100)) {
				// user requested to change system
				int ret=IDNO;
                if (VerifyOpenFiles(hwnd)) {
				    if ((!lParam)&&(fKeyEverPressed)) {
					    ret=MessageBox(hwnd, "This will reset the emulator - are you sure?", "Change System Type", MB_YESNO|MB_ICONQUESTION);
				    } else {
					    ret=IDYES;
				    }
                }
				if (IDYES == ret) {
					nSystem=wParam-ID_SYSTEM_0;
					for (int idx=0; idx<100; idx++) {
						if (idx == nSystem) {
							CheckMenuItem(GetMenu(myWnd), ID_SYSTEM_0+idx, MF_CHECKED);
						} else {
							CheckMenuItem(GetMenu(myWnd), ID_SYSTEM_0+idx, MF_UNCHECKED);
						}
					} 
					// Special case for 99/4
					if (nSystem == 0) {
						keyboard=KEY_994;	// 99/4 layout
					} else {
						// this lets the user disable ps2 keyboard support
						if (ps2keyboardok) {
							keyboard=KEY_994A_PS2;	// 99/4A with ps/2
						} else {
							keyboard=KEY_994A;		// 99/4A without ps/2
						}
					}
					SendMessage(hwnd, WM_COMMAND, ID_FILE_RESET, 0);
				}
			}
			if ((wParam >= ID_APP_0) && (wParam < ID_APP_0+100)) {
				// user requested to change cartridge (apps)
				int ret=IDNO;
                if (VerifyOpenFiles(hwnd)) {
				    if (fKeyEverPressed) {
					    ret=MessageBox(hwnd, "This will reset the emulator - are you sure?", "Change Cartridge", MB_YESNO|MB_ICONQUESTION);
				    } else {
					    ret=IDYES;
				    }
                }
				if (IDYES == ret) {
					int idx;
					nCartGroup=0;
					nCart=wParam-ID_APP_0;
					for (idx=0; idx<100; idx++) {
						if (idx == nCart) {
							CheckMenuItem(GetMenu(myWnd), ID_APP_0+idx, MF_CHECKED);
						} else {
							CheckMenuItem(GetMenu(myWnd), ID_APP_0+idx, MF_UNCHECKED);
						}
					}
					for (idx=0; idx<100; idx++) {
						CheckMenuItem(GetMenu(myWnd), ID_GAME_0+idx, MF_UNCHECKED);
					}
					for (idx=0; idx<nTotalUserCarts; idx++) {
						CheckMenuItem(GetMenu(myWnd), ID_USER_0+idx, MF_UNCHECKED);
					}
					SendMessage(hwnd, WM_COMMAND, ID_FILE_RESET, 0);
				}
			}
			if ((wParam >= ID_GAME_0) && (wParam < ID_GAME_0+100)) {
				// user requested to change cartridge (games)
				int ret=IDNO;
                if (VerifyOpenFiles(hwnd)) {
				    if (fKeyEverPressed) {
					    ret=MessageBox(hwnd, "This will reset the emulator - are you sure?", "Change Cartridge", MB_YESNO|MB_ICONQUESTION);
				    } else {
					    ret=IDYES;
				    }
                }
				if (IDYES == ret) {
					int idx;
					nCartGroup=1;
					nCart=wParam-ID_GAME_0;
					for (idx=0; idx<100; idx++) {
						if (idx == nCart) {
							CheckMenuItem(GetMenu(myWnd), ID_GAME_0+idx, MF_CHECKED);
						} else {
							CheckMenuItem(GetMenu(myWnd), ID_GAME_0+idx, MF_UNCHECKED);
						}
					}
					for (idx=0; idx<100; idx++) {
						CheckMenuItem(GetMenu(myWnd), ID_APP_0+idx, MF_UNCHECKED);
					}
					for (idx=0; idx<nTotalUserCarts; idx++) {
						CheckMenuItem(GetMenu(myWnd), ID_USER_0+idx, MF_UNCHECKED);
					}
					SendMessage(hwnd, WM_COMMAND, ID_FILE_RESET, 0);
				}
			}
			if ((wParam >= ID_USER_0) && (wParam < ID_USER_0+MAXUSERCARTS)) {
				// user requested to change cartridge (user)
				int ret=IDNO;
                if (VerifyOpenFiles(hwnd)) {
				    if (fKeyEverPressed) {
					    ret=MessageBox(hwnd, "This will reset the emulator - are you sure?", "Change Cartridge", MB_YESNO|MB_ICONQUESTION);
				    } else {
					    ret=IDYES;
				    }
                }
				if (IDYES == ret) {
					int idx;
					nCartGroup=2;
					nCart=0;	// do a search instead of assuming
					for (idx=0; idx<nTotalUserCarts; idx++) {
						if (Users[idx].nUserMenu == wParam) {
							CheckMenuItem(GetMenu(myWnd), ID_USER_0+idx, MF_CHECKED);
							nCart=idx;
						} else {
							CheckMenuItem(GetMenu(myWnd), ID_USER_0+idx, MF_UNCHECKED);
						}
					}
					for (idx=0; idx<100; idx++) {
						CheckMenuItem(GetMenu(myWnd), ID_GAME_0+idx, MF_UNCHECKED);
					}
					for (idx=0; idx<100; idx++) {
						CheckMenuItem(GetMenu(myWnd), ID_APP_0+idx, MF_UNCHECKED);
					}
					SendMessage(hwnd, WM_COMMAND, ID_FILE_RESET, 0);
				}
			}
			if ((wParam >= ID_DISK_DSK0_SETDSK0) && (wParam <= ID_DISK_DSK9_SETDSK9)) {
				// disk configuration dialog for the specified disk
				ConfigureDisk(hwnd, wParam - ID_DISK_DSK0_SETDSK0);
			}
			if ((wParam >= ID_DSK0_OPENDSK0) && (wParam <= ID_DSK9_OPENDSK9)) {
				// request to open a disk path - we just ask Explorer to do it. people can use
				// TI99Dir to open images
				EnterCriticalSection(&csDriveType);
				if (NULL != pDriveType[wParam-ID_DSK0_OPENDSK0]) {
					// we don't initialize COM, so some shell extensions may cause issues.
					// MS wants us to do this: CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)
					// But that's per-thread! And you have to DeInitialize and track how many times and everything, oi.
					int nResult = (int)ShellExecute(hwnd, NULL, pDriveType[wParam-ID_DSK0_OPENDSK0]->GetPath(), NULL, NULL, SW_SHOWNORMAL);
					LeaveCriticalSection(&csDriveType);
					if (nResult < 32) {
						HLOCAL pMsg;
						if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, nResult, 0, (LPSTR)&pMsg, 0, NULL)) {
							debug_write("Failed to execute with code %d, and FormatMessage failed with code %d", nResult, GetLastError());
							MessageBox(hwnd, "Failed to open folder, code in debug view.", "Error", MB_OK | MB_ICONERROR);
						} else {
							MessageBox(hwnd, (LPCSTR)pMsg, "Error", MB_OK | MB_ICONERROR);
							LocalFree(pMsg);
						}
					}
				} else {
					LeaveCriticalSection(&csDriveType);
					MessageBox(hwnd, "There is no disk driver attached to open.", "Sorry!", MB_OK | MB_ICONINFORMATION);
				}
			}
            if ((wParam >= ID_USERCART_MRU) && (wParam <= ID_USERCART_LASTMRU)) {
                // request to open a previously opened cart
                // we need to create an OPENFILENAME object with the filename
                OPENFILENAME ofn;
				char buf[MAX_PATH], buf2[MAX_PATH];

                // get the string from the MRU
                int idx = wParam - ID_USERCART_MRU;
                if ((idx >= 0) && (idx < MAX_MRU)) {
                    // we'll just fill it all in the same as the open below
				    memset(&ofn, 0, sizeof(OPENFILENAME));
				    ofn.lStructSize=sizeof(OPENFILENAME);
				    ofn.hwndOwner=hwnd;
				    ofn.lpstrFilter="V9T9 Carts\0*.bin;*.C;*.D;*.G;*.3\0All Files\0*\0\0";
				    ofn.lpstrFile=buf;
				    ofn.nMaxFile=MAX_PATH;
				    ofn.lpstrFileTitle=buf2;
				    ofn.nMaxFileTitle=MAX_PATH;
				    ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;

                    // buf gets lpstrFile - this is the complete path
				    strcpy(buf, csLastUserCart[idx]);

                    // buf2 gets lpstrFileTitle - name and extension - no path
                    int p = csLastUserCart[idx].GetLength() - 1;
                    while ((p > 0) && (buf[p] != '\\')) --p;
                    if (p == 0) {
                        strcpy(buf2, buf);
                    } else {
                        strcpy(buf2, &buf[p+1]);
                    }

                    // also need to find nFileExtension - count in characters in lpstrFile to the extension (after the '.'), zero if none
                    p = csLastUserCart[idx].GetLength() - 1;
                    while ((p > 0) && (buf[p] != '.')) --p;
                    if (buf[p] == '.') {
                        ofn.nFileExtension = p+1;
                    } else {
                        ofn.nFileExtension = 0;
                    }

                    // now go ahead and request the open
                    OpenUserCart(ofn);
                }
            }

			// Any others?
			switch (wParam)
			{
			case ID_CART_USER_OPEN:
			// Browse for user cartridge filename
            // We can handle:
            // name<LETTER>.bin - where <LETTER> is C, D, G, 3, 9 or 8
            // name Part x of y <LETTER>.bin - where y is fixed and x counts from 1
            // name (no restrictions, no other files searched for)
            // name.bin (where none of the above conflict, other files searched for, possibly incorrectly)
            // 
			{
				OPENFILENAME ofn;
				char buf[MAX_PATH], buf2[MAX_PATH];
				bool proceed = false;

				memset(&ofn, 0, sizeof(OPENFILENAME));
				ofn.lStructSize=sizeof(OPENFILENAME);
				ofn.hwndOwner=hwnd;
				ofn.lpstrFilter="V9T9 Carts\0*.bin;*.C;*.D;*.G;*.3\0All Files\0*\0\0";
				strcpy(buf, "");
				ofn.lpstrFile=buf;
				ofn.nMaxFile=MAX_PATH;
				strcpy(buf2, "");
				ofn.lpstrFileTitle=buf2;
				ofn.nMaxFileTitle=MAX_PATH;
				ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST;
				
				char szTmpDir[MAX_PATH];
				GetCurrentDirectory(MAX_PATH, szTmpDir);

				if (lParam != 0) {
					char *p;

					strncpy(buf, (char*)lParam, MAX_PATH);
					buf[MAX_PATH-1]='\0';

					strncpy(buf2, (char*)lParam, MAX_PATH);	// TODO: title - this one really should exclude the path
					buf2[MAX_PATH-1]='\0';

					p = strchr(buf, '.');
					if (p != NULL) {
						ofn.nFileExtension = p-buf+1;
					}
					proceed = true;
				} else {
					proceed = GetOpenFileName(&ofn) != 0;
				}

				if (proceed) {
                    OpenUserCart(ofn);
                }

                SetCurrentDirectory(szTmpDir);
			}
			break;

			case ID_CARTRIDGE_EJECT:
				nCart=-1;

				for (int idx=0; idx<nTotalUserCarts; idx++) {
					CheckMenuItem(GetMenu(myWnd), ID_USER_0+idx, MF_UNCHECKED);
				}
				for (int idx=0; idx<100; idx++) {
					CheckMenuItem(GetMenu(myWnd), ID_GAME_0+idx, MF_UNCHECKED);
				}
				for (int idx=0; idx<100; idx++) {
					CheckMenuItem(GetMenu(myWnd), ID_APP_0+idx, MF_UNCHECKED);
				}

				SendMessage(hwnd, WM_COMMAND, ID_FILE_RESET, 0);
				break;

			case ID_HELP_ABOUT:
				sprintf(szTemp, "Classic99 %s\n"\
								"�1994-2024\n\n"\
								"By Mike Brent (Tursi)\n"\
								"ROM data included under license from Texas Instruments.\n\n"\
								"So many people in the TI community make this all worthwhile!\n\n"\
								"Contains additional code by:\n"\
								"Joe Delekto - SAMS support\n"\
								"MESS Team - Speech, with thanks to\n"\
								"Ralph Nebet for speech ROM help.\n"\
								"John Butler - 9900 Disasm\n"\
								"Derek Liauw Kie Fa - 2xSaI Renderer\n"\
								"2xSaI code from the SNES9x project\n"\
								"hq4x code by Maxim Stepin\n"\
								"Shay Green for the TV Filter\n"\
								"Keyboard map by Ron Reuter - www.mainbyte.com\n"\
								"RamusM for the ECM sprite handling\n\n"\
								"tursi@harmlesslion.com\n"\
								"http://harmlesslion.com/software/classic99", 
						VERSION);
				MessageBox(myWnd, szTemp, "Classic99 About", MB_OK);
				break;

			case ID_HELP_KBMAP: 
				{
					if (NULL == hKBMap) {
						// create a modeless dialog to show the keyboard map
						hKBMap=CreateDialog(NULL, MAKEINTRESOURCE(IDD_KBMAP), hwnd, KBMapProc);
						ShowWindow(hKBMap, SW_SHOW);
					}
				}
				break;

			case ID_HELP_KNOWNISSUES:
				if (NULL == pCurrentHelpMsg) {
					pCurrentHelpMsg="There are no known issues with the currently selected cartridge.";
				}
				MessageBox(myWnd, pCurrentHelpMsg, "Compatibility Notes", MB_ICONINFORMATION|MB_OK);
				break;

			case ID_HELP_OPENHELPFILE:
				// just try to launch the PDF file
				{
					int nResult = (int)ShellExecute(hwnd, NULL, "Classic99 Manual.pdf", NULL, NULL, SW_SHOWNORMAL);
					if (nResult < 32) {
						debug_write("Failed to open help with code %d", nResult);
						MessageBox(hwnd, "Failed to open manual. Make sure it is in the Classic99 folder, and a PDF reader is installed.", "Error", MB_OK | MB_ICONERROR);
					}
				}
				break;

			case ID_FILE_RESET:
			case ID_FILE_WARMRESET:
			case ID_FILE_SCRAMBLERESET:
			case ID_FILE_ERASEUBERGROM:
                if (VerifyOpenFiles(hwnd)) {
				    // save roms before we wipe all the memory!
				    saveroms();
				
				    // now erase memories as appropriate
				    if (LOWORD(wParam) == ID_FILE_ERASEUBERGROM) {
					    memrnd(UberGROM, sizeof(UberGROM));
					    memrnd(UberRAM, sizeof(UberRAM));
					    memrnd(UberEEPROM, sizeof(UberEEPROM));
					    bScrambleMemory=false;
					    bWarmBoot = false;
				    }
				    if (LOWORD(wParam) == ID_FILE_RESET) {
					    bScrambleMemory=false;
					    bWarmBoot = false;
				    }
				    if (LOWORD(wParam) == ID_FILE_WARMRESET) {
					    bScrambleMemory = false;
					    bWarmBoot = true;
				    }
				    if (LOWORD(wParam) == ID_FILE_SCRAMBLERESET) {
					    bScrambleMemory=true;
					    bWarmBoot = false;
				    }
				
				    TriggerBreakPoint(true, false);			// halt the CPU
				    Sleep(50);								// wait for it...

				    memset(CRU, 1, 4096);					// reset 9901
	                CRU[0]=0;	// timer control
	                CRU[1]=0;	// peripheral interrupt mask
	                CRU[2]=0;	// VDP interrupt mask
	                CRU[3]=0;	// timer interrupt mask??
                //  CRU[12-14]  // keyboard column select
                //  CRU[15]     // Alpha lock 
                //  CRU[24]     // audio gate (leave high)
	                CRU[25]=0;	// mag tape out - needed for Robotron to work!
	                CRU[27]=0;	// mag tape in (maybe all these zeros means 0 should be the default??)
				    timer9901 = 0;
                    timer9901Read = 0;
				    starttimer9901 = 0;
				    timer9901IntReq=0;
				    wrword(0x83c4,0);						// Console bug work around, make sure no user int is active
				    init_kb();								// Reset keyboard emulation
				    SetupSams(sams_enabled, sams_size);		// Prepare the AMS system
				    if (NULL != InitSid) {
					    InitSid();							// reset the SID chip
					    if (NULL != SetSidBanked) {		
						    SetSidBanked(false);			// switch it out for now
					    }
				    }
				    resetDAC();
				    readroms();								// reload the real ROMs
				    if (NULL != pCurrentHelpMsg) {
					    szDefaultWindowText="Classic99 - See Help->Known Issues for this cart";
					    SetWindowText(myWnd, szDefaultWindowText);
				    } else {
                        szDefaultWindowText = AppName;
					    SetWindowText(myWnd, szDefaultWindowText);
				    }

				    pCPU->reset();
				    pGPU->reset();
				    pCurrentCPU = pCPU;
				    bF18AActive = 0;
				    for (int idx=0; idx<=PCODEGROMBASE; idx++) {
					    GROMBase[idx].grmaccess=2;			// no GROM accesses yet
				    }
				    nCurrentDSR=-1;
				    memset(nDSRBank, 0, sizeof(nDSRBank));
				    doLoadInt=false;						// no pending LOAD
				    vdpReset(true);	    					// TODO: should move these vars into the reset function
				    vdpaccess=0;							// No VDP address writes yet 
				    vdpwroteaddress=0;						// timer after a VDP address write to allow time to fetch
				    vdpscanline=0;
				    vdpprefetch=0;
				    vdpprefetchuninited = true;
				    VDPREG[0]=0;
				    VDPREG[1]=0;							// VDP registers 0/1 cleared on reset per datasheet
				    end_of_frame=0;							// No end of frame yet
				    CPUSpeechHalt=false;					// not halted for speech reasons
				    CPUSpeechHaltByte=0;					// byte pending for the speech hardware
				    cpucount=0;
				    cpuframes=0;
				    fKeyEverPressed=false;					// No key pressed yet (to disable the warning on cart change)
				    memset(CPUMemInited, 0, sizeof(CPUMemInited));	// no CPU mem written to yet
				    memset(VDPMemInited, 0, sizeof(VDPMemInited));	// or VDP
				    bWarmBoot = false;						// if it was a warm boot, it's done now
				    // set all joysticks as available
				    installedJoysticks = 0xffff;
				    // but don't reset g_bCheckUninit
				    DoPlay();
				    // these must come AFTER DoPlay()
				    max_cpf=(hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF);
				    cfg_cpf=max_cpf;
				    InterlockedExchange((LONG*)&cycles_left, max_cpf);
                }
				break;

			case ID_FILE_QUIT:
				if (IDYES == MessageBox(myWnd, "Are you sure you want to quit?", "Classic99 go ByeBye?", MB_YESNO)) {
					MuteAudio();
					if (!GetWindowRect(myWnd, &gWindowRect)) {
						gWindowRect.left = -1;
						gWindowRect.top = -1;
					}
					quitflag=1;
					PostQuitMessage(0);
				} 
				break;

			case ID_DISK_CORRUPTDSKRAM:
				if (1 != lParam) {
					bCorruptDSKRAM=bCorruptDSKRAM?0:1;
				}
				if (bCorruptDSKRAM) {
					CheckMenuItem(GetMenu(myWnd), ID_DISK_CORRUPTDSKRAM, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_DISK_CORRUPTDSKRAM, MF_UNCHECKED);
				}
				break;

            case ID_TAPE_LOADTAPE:
                // uses the load dialog, so not really much of a 'rewind',
                // but it does go back to zero
                MuteAudio();
                LoadTape();
                SetSoundVolumes();
                break;

            case ID_TAPE_STOPTAPE:
                // stop the tape if it's playing
                forceTapeMotor(false);
                break;

            case ID_TAPE_PLAYTAPE:
                // start the tape, even if the remote is off
                forceTapeMotor(true);
                break;

			case ID_VIDEO_FLICKER:
				if (lParam != 1) {
					bUse5SpriteLimit=!bUse5SpriteLimit;
				}

				if (!bUse5SpriteLimit) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FLICKER, MF_UNCHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FLICKER, MF_CHECKED);
				}
				break;

			case ID_LAYERS_DISABLEBLANKING:
				if (bDisableBlank) {
					bDisableBlank=false;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBLANKING, MF_UNCHECKED);
				} else {
					bDisableBlank=true;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBLANKING, MF_CHECKED);
				} 
				redraw_needed=REDRAW_LINES;
				break;

			case ID_LAYERS_DISABLESPRITES:
				if (bDisableSprite) {
					bDisableSprite=false;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLESPRITES, MF_UNCHECKED);
				} else {
					bDisableSprite=true;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLESPRITES, MF_CHECKED);
				} 
				redraw_needed=REDRAW_LINES;
				break;

			case ID_LAYERS_DISABLEBACKGROUND:
				if (bDisableBackground) {
					bDisableBackground=false;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBACKGROUND, MF_UNCHECKED);
				} else {
					bDisableBackground=true;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBACKGROUND, MF_CHECKED);
				} 
				redraw_needed=REDRAW_LINES;
				break;

			case ID_LAYERS_DISABLEBITMAPCOLORLAYER:
				if (bDisableColorLayer) {
					bDisableColorLayer=false;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBITMAPCOLORLAYER, MF_UNCHECKED);
				} else {
					bDisableColorLayer=true;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBITMAPCOLORLAYER, MF_CHECKED);
				} 
				redraw_needed=REDRAW_LINES;
				break;

            case ID_LAYERS_DISABLEBITMAPPATTERNLAYER:
				if (bDisablePatternLayer) {
					bDisablePatternLayer=false;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBITMAPPATTERNLAYER, MF_UNCHECKED);
				} else {
					bDisablePatternLayer=true;
					CheckMenuItem(GetMenu(myWnd), ID_LAYERS_DISABLEBITMAPPATTERNLAYER, MF_CHECKED);
				} 
				redraw_needed=REDRAW_LINES;
				break;

			case ID_VIDEO_50HZ:
				if (lParam != 1) {
					hzRate=(hzRate==HZ60)?HZ50:HZ60;
				}

				if (hzRate == HZ60) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_50HZ, MF_UNCHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_50HZ, MF_CHECKED);
				}
				break;

			case ID_VIDEO_STARTRECORDING:
				if (Recording == 0)
				{
					debug_write("Starting AVI recording");
					if (0==InitAvi(false))
					{
						szDefaultWindowText="Classic99 - Recording AVI";
						SetWindowText(myWnd, szDefaultWindowText);
						Recording=1;
						RecordFrame=0;
					}
				}
				break;

			case ID_VIDEO_STARTRECORDINGVIDEO:
				// with audio
				if (Recording == 0)
				{
					debug_write("Starting AVI recording with at audio %dHz", AudioSampleRate);
					if (0==InitAvi(true))
					{
						szDefaultWindowText="Classic99 - Recording AVI";
						SetWindowText(myWnd, szDefaultWindowText);
						Recording=1;
						RecordFrame=0;
					}
				}
				break;
			
			case ID_VIDEO_STOPRECORDING:
				if (Recording)
				{
					debug_write("Stoping AVI recording");
                    szDefaultWindowText = AppName;
					SetWindowText(myWnd, szDefaultWindowText);
					CloseAVI();
					Recording=0;
				}
				break;

			case ID_VIDEO_SCREENSHOTBAS:
				// save the current raw TI image
				SaveScreenshot(false, false);
				break;
			
			case ID_VIDEO_SCREENSHOTFILT:
				// not supported yet
				SaveScreenshot(false, true);
				break;

			case ID_VIDEO_MAINTAINASPECT:
				if (1 != lParam) {
					MaintainAspect=MaintainAspect?0:1;
				}
				if (MaintainAspect)
				{
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_MAINTAINASPECT, MF_CHECKED);
				}
				else
				{
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_MAINTAINASPECT, MF_UNCHECKED);
				}
				// we won't resize, but for sanity's sake, wipe the background
				if ((MaintainAspect)||(STRETCH_NONE == StretchMode)) {
					GetClientRect(myWnd, &myrect);
					myDC=GetDC(myWnd);
					FillRect(myDC, &myrect, (HBRUSH)(COLOR_MENUTEXT+1));	// must add 1 to system colors - this is normally black
					ReleaseDC(myWnd, myDC);
				}
				break;

			case ID_VIDEO_ENABLEF18A:
				if (1 != lParam) {
					bF18Enabled=bF18Enabled?0:1;
				}
				if (bF18Enabled) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_ENABLEF18A, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_ENABLEF18A, MF_UNCHECKED);
				}
				break;

			case ID_VIDEO_INTERLEAVEGPU:
				if (1 != lParam) {
					bInterleaveGPU=bInterleaveGPU?0:1;
				}
				if (bInterleaveGPU) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_INTERLEAVEGPU, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_INTERLEAVEGPU, MF_UNCHECKED);
				}
				pCurrentCPU = pCPU;		// just to be safe
				break;

			case ID_VIDEO_ENABLE80COLUMNHACK:
				if (1 != lParam) {
					bEnable80Columns=bEnable80Columns?0:1;
				}
				if (bEnable80Columns) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_ENABLE80COLUMNHACK, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_ENABLE80COLUMNHACK, MF_UNCHECKED);
				}
				break;

			case ID_VIDEO_SHOWFPS:
				if (1 != lParam) {
					bShowFPS = bShowFPS ? 0 : 1;
				}
				if (bShowFPS) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_SHOWFPS, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_SHOWFPS, MF_UNCHECKED);
				}
				break;

            case ID_VIDEO_SHOWKEYBOARDDEBUG:
				if (1 != lParam) {
					bShowKeyboard = bShowKeyboard ? 0 : 1;
				}
				if (bShowKeyboard) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_SHOWKEYBOARDDEBUG, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_SHOWKEYBOARDDEBUG, MF_UNCHECKED);
				}
				break;

			case ID_VIDEO_ENABLE128KHACK:
				if (1 != lParam) {
					bEnable128k=bEnable128k?0:1;
				}
				if (bEnable128k) {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_ENABLE128KHACK, MF_CHECKED);
					if (1 != lParam) {
						MessageBox(myWnd, "Note: 128k on the F18A VDP is not a normal configuration, and exists only to support certain 9938 programs. It does NOT work this way in real life.", "Classic99", MB_OK);
					}
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_VIDEO_ENABLE128KHACK, MF_UNCHECKED);
				}
				break;

			case ID_OPTIONS_PAUSEINACTIVE:
				if (1 != lParam) {
					PauseInactive=PauseInactive?0:1;
				}
				if (PauseInactive) {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_PAUSEINACTIVE, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_PAUSEINACTIVE, MF_UNCHECKED);
				}
				break;

			case ID_OPTIONS_SPEECHENABLED:
				if (1 != lParam) {
					SpeechEnabled=SpeechEnabled?0:1;
				}
				if (SpeechEnabled) {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_SPEECHENABLED, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_SPEECHENABLED, MF_UNCHECKED);
				}
				break;

			case ID_OPTIONS_CTRL_RESET:
				if (1 != lParam) {
					// toggle value
					CtrlAltReset=CtrlAltReset?0:1;
				}
				if (CtrlAltReset) {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_CTRL_RESET, MF_CHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_CTRL_RESET, MF_UNCHECKED);
				}
				break;

			case ID_OPTIONS_INVERTCAPSLOCK:
				if (1 != lParam) {
					// toggle value
					gDontInvertCapsLock=gDontInvertCapsLock?0:1;
				}
				if (gDontInvertCapsLock) {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_INVERTCAPSLOCK, MF_UNCHECKED);
				} else {
					CheckMenuItem(GetMenu(myWnd), ID_OPTIONS_INVERTCAPSLOCK, MF_CHECKED);
				}
				break;

			case ID_OPTIONS_CPUTHROTTLING:
				{
					// Make sure nothing's left over from the old speed
					InterlockedExchange((LONG*)&cycles_left, 0);

					// Now just used to set the check boxes correctly
					switch (ThrottleMode) {
					default:
						// force a known value
						debug_write("Unknown throttle mode %d - resetting to normal.", ThrottleMode);
						ThrottleMode = THROTTLE_NORMAL;
						// fall through

					case THROTTLE_NORMAL:
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_NORMAL, MF_CHECKED);
						if (Fast16BitRam) {
							snprintf(speedTitle, sizeof(speedTitle), "%s - 16-bit RAM", AppName);
							szDefaultWindowText = speedTitle;
						} else {
							szDefaultWindowText = AppName;
						}
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUSLOW, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUOVERDRIVE, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_SYSTEMMAXIMUM, MF_UNCHECKED);
						break;

					case THROTTLE_SLOW:
						snprintf(speedTitle, sizeof(speedTitle), "%s - Slow CPU", AppName);
						if (Fast16BitRam) {
							strncat(speedTitle, " - 16-bit RAM", sizeof(speedTitle)-strlen(speedTitle)-1);
						}
						szDefaultWindowText = speedTitle;
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUSLOW, MF_CHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_NORMAL, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUOVERDRIVE, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_SYSTEMMAXIMUM, MF_UNCHECKED);
						break;

					case THROTTLE_OVERDRIVE:
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUOVERDRIVE, MF_CHECKED);
						if (enableSpeedKeys) {
							snprintf(speedTitle, sizeof(speedTitle), "%s - CPU Overdrive (F6 for normal)", AppName);
						} else {
							snprintf(speedTitle, sizeof(speedTitle), "%s - CPU Overdrive", AppName);
						}
						if (Fast16BitRam) {
							strncat(speedTitle, " - 16-bit RAM", sizeof(speedTitle)-strlen(speedTitle)-1);
						}
						szDefaultWindowText = speedTitle;
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUSLOW, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_NORMAL, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_SYSTEMMAXIMUM, MF_UNCHECKED);
						break;

					case THROTTLE_SYSTEMMAXIMUM:
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_SYSTEMMAXIMUM, MF_CHECKED);
						if (enableSpeedKeys) {
							snprintf(speedTitle, sizeof(speedTitle), "%s - System Maximum (F6/F11 for normal)", AppName);
						} else {
							snprintf(speedTitle, sizeof(speedTitle), "%s - System Maximum", AppName);
						}
						if (Fast16BitRam) {
							strncat(speedTitle, " - 16-bit RAM", sizeof(speedTitle)-strlen(speedTitle)-1);
						}
						szDefaultWindowText = speedTitle;
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUOVERDRIVE, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_CPUSLOW, MF_UNCHECKED);
						CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_NORMAL, MF_UNCHECKED);
						break;
					}
					SetWindowText(myWnd, szDefaultWindowText);
				}
				break;

			case ID_CPUTHROTTLING_NORMAL:
				ThrottleMode = THROTTLE_NORMAL;
				if (lParam != 1) max_cpf=cfg_cpf;   // lParam(1) means internal message, don't change
				resetDAC();							// otherwise we will be way out of sync
				SetSoundVolumes();					// unmute in case it was in slow mode
				if ((lParam != 1) && (max_cpf <= SLOW_CPF)) {
					// we've lost the configured speed.. sorry? to remove that later anyway
					debug_write("cfg_cpf was lost, this isn't supposed to happen. Resetting.");
					max_cpf=(hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF);
					cfg_cpf=max_cpf;
				}
				PostMessage(myWnd, WM_COMMAND, ID_OPTIONS_CPUTHROTTLING, 1);
				break;

			case ID_CPUTHROTTLING_CPUSLOW:
				ThrottleMode = THROTTLE_SLOW;
				max_cpf=SLOW_CPF;
				resetDAC();		// just to empty it, it won't be filled in slow mode
				MuteAudio();
				PostMessage(myWnd, WM_COMMAND, ID_OPTIONS_CPUTHROTTLING, 1);
				break;

			case ID_CPUTHROTTLING_CPUOVERDRIVE:
				ThrottleMode = THROTTLE_OVERDRIVE;
				max_cpf=cfg_cpf;
				SetSoundVolumes();		// unmute in case it was in slow mode
				PostMessage(myWnd, WM_COMMAND, ID_OPTIONS_CPUTHROTTLING, 1);
				break;

			case ID_CPUTHROTTLING_SYSTEMMAXIMUM:
				ThrottleMode = THROTTLE_SYSTEMMAXIMUM;
				max_cpf=cfg_cpf;
				SetSoundVolumes();		// unmute in case it was in slow mode
				PostMessage(myWnd, WM_COMMAND, ID_OPTIONS_CPUTHROTTLING, 1);
				break;

			case ID_CPUTHROTTLING_16:
				// lParam of 1 means internal message, value already changed
				if (lParam != 1) {
					Fast16BitRam = !Fast16BitRam;
				}
				if (Fast16BitRam) {
					snprintf(speedTitle, sizeof(speedTitle), "%s - 16-bit RAM", AppName);
					szDefaultWindowText = speedTitle;
					CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_16, MF_CHECKED);
					if (lParam != 1) {
						MessageBox(myWnd, "Warning: 16-bit RAM setting also affects AMS. This mode is accurate only for 32k apps.", "Performance warning", MB_OK);
					}
				} else {
					szDefaultWindowText = AppName;
					CheckMenuItem(GetMenu(myWnd), ID_CPUTHROTTLING_16, MF_UNCHECKED);
				}
				break;

			case ID_SCREENREADER_CONTINUOUS:
				{
					bool val = ScreenReader::GetContinuousRead();
					if (lParam == 0) {
						val = !val;
						ScreenReader::SetContinuousRead(val);
					} else {
						// sent from config - force to true
						val = true;
						ScreenReader::SetContinuousRead(val);
					}
					if (val) {
						CheckMenuItem(GetMenu(myWnd), ID_SCREENREADER_CONTINUOUS, MF_CHECKED);
					} else {
						CheckMenuItem(GetMenu(myWnd), ID_SCREENREADER_CONTINUOUS, MF_UNCHECKED);
					}
				}
				break;

			case ID_SCREENREADER_READONCE:
				ScreenReader::ReadScreenOnce();
				break;

			case ID_SCREENREADER_STOPTALKING:
				ScreenReader::ShutUp();
				break;

			case ID_OPTIONS_AUDIO: 
				{
					if (Recording) {
						MessageBox(myWnd, "Can't change audio options while recording video", "Classic99 Error", MB_OK);
						break;
					}

					// Create a dialog to reconfigure audio voices
					if (DialogBox(NULL, MAKEINTRESOURCE(IDD_AUDIO), hwnd, AudioBoxProc) == IDOK) {
						DoPause();					// stop the system
						Sleep(100);					// wait for it
						// now reset audio
						sound_init(AudioSampleRate);
						GenerateToneBuffer();
						GenerateSIDBuffer();
						// now continue
						DoPlay();
					}
				}
				break;

			case ID_OPTIONS_OPTIONS:
				{
					// Create a dialog to reconfigure generic options
					DialogBox(NULL, MAKEINTRESOURCE(IDD_OPTIONS), hwnd, OptionsBoxProc);
					// It handles the OK/Cancel operation in OptionsBoxProc
				}
				break;

			case ID_OPTIONS_GRAM:
				{
					// Create a dialog to reconfigure GRAM options
					DialogBox(NULL, MAKEINTRESOURCE(IDD_OPTGRAM), hwnd, GramBoxProc);
					// It handles the OK/Cancel operation in GramBoxProc
				}
				break;

			case ID_OPTIONS_TV:
				// If filters aren't available, tell the user
				if (!TVFiltersAvailable) {
					MessageBox(hwnd, "TV Filter DLL is not available.", "Classic99", MB_OK);
					break;
				}
				// If we aren't in TV mode, switch to it and come back here
				if (4 != FilterMode) {
					PostMessage(hwnd, WM_COMMAND, ID_VIDEO_FILTERMODE_TVMODE, 0);
					PostMessage(hwnd, WM_COMMAND, ID_OPTIONS_TV, 0);
					break;
				}
				// If the TV dialog doesn't already exist, exist it :)
				if (NULL == hTVDlg) {
					// create a modeless dialog to show the TV controls dialog
					hTVDlg=CreateDialog(NULL, MAKEINTRESOURCE(IDD_TVOPTIONS), hwnd, TVBoxProc);
					ShowWindow(hTVDlg, SW_SHOW);
				}
				break;

			case ID_EDIT_PASTEXB:
				// this falls through into EDITPASTE, where we parse the string to remove spaces not needed to enter an XB line
				// in hopes of more listings being pastable
				if (NULL != PasteString) {
					// this MUST come before we change PasteStringHackBuffer, else we might change an existing paste!
					Beep(550,100);
					break;
				}
				PasteStringHackBuffer = true;
			case ID_EDITPASTE:
				if (NULL != PasteString) {
					Beep(550,100);
					break;
				}

				PasteString = GetProcessedClipboardData(NULL);
				if (NULL != PasteString) {
					if (wParam == ID_EDIT_PASTEXB) {
						ParseForXB(PasteString);
					}
					PasteIndex=PasteString;
				} else {
					// don't leave this on by accident if there's no paste
					PasteStringHackBuffer=false;
				}
				break;
			
            case ID_EDIT_COPYSCREEN:
                {
                    // build an output string - unknown chars will be '.'
                    CString csOut;
                    if (GetAsyncKeyState(VK_SHIFT)&0x8000) {
                        // BASIC offset
                        csOut = captureScreen(-96, '.');
                    } else {
                        csOut = captureScreen(0, '.');
                    }
                    if (csOut != "") {
          	            HGLOBAL hGlob;
	                    if (OpenClipboard(myWnd)) {
    	                    // we need to empty the clipboard to take possession of it
	                        if (EmptyClipboard()) {
		                        // now copy into a global buffer and give it to the clipboard
		                        hGlob = GlobalAlloc(GMEM_MOVEABLE, csOut.GetLength()+1);
		                        if (NULL != hGlob) {
    		                        char *pStr = (char*)GlobalLock(hGlob);
	    	                        if (NULL != pStr) {
        		                        memcpy(pStr, csOut.GetBuffer(), csOut.GetLength());
		                                *(pStr+csOut.GetLength())='\0';
		                                GlobalUnlock(pStr);
		                                SetClipboardData(CF_TEXT, hGlob);
                                        debug_write("Screen copied to clipboard.");
                                    }
                                }
                            }
                            CloseClipboard();
                        }
                    }
                }
                break;

			case ID_EDIT_DEBUGGER:
				// activate debugger screen
				if (NULL == dbgWnd) {
					LaunchDebugWindow();
				}
				break;

			case ID_EDIT_BUG99WINDOW:
				ShowWindow(hBugWnd, SW_SHOW);
				break;

			case ID_EDIT_HEATMAP: 
				// activate heatmap screen
				if (NULL == hHeatMap) {
					// create a modeless dialog to show the keyboard map
					hHeatMap=CreateDialog(NULL, MAKEINTRESOURCE(IDD_HEATMAP), hwnd, HeatMapProc);
					ShowWindow(hHeatMap, SW_SHOW);
				}
				break;

			case ID_CHANGESIZE_1X:
				{
					GetWindowRect(myWnd, &myrect);
					GetClientRect(myWnd, &myrect2);
					myrect.right = myrect.left + CurrentDDSD.dwWidth + ((myrect.right - myrect.left)-(myrect2.right - myrect2.left));
					myrect.bottom = myrect.top + CurrentDDSD.dwHeight + ((myrect.bottom - myrect.top)-(myrect2.bottom - myrect2.top));
					MoveWindow(myWnd, myrect.left, myrect.top, myrect.right-myrect.left, myrect.bottom-myrect.top, true);
				}

				if (STRETCH_NONE == StretchMode) {
					GetClientRect(myWnd, &myrect);
					myDC=GetDC(myWnd);
					FillRect(myDC, &myrect, (HBRUSH)(COLOR_MENU+1));
					ReleaseDC(myWnd, myDC);
				}

				GetClientRect(myWnd, &myrect2);
				debug_write("Client rect now %d x %d", myrect2.right-myrect2.left, myrect2.bottom-myrect2.top);

				// repeat it once only - this accounts for the menu changing from 1 to 2 lines and back
				if (lParam != 99) {
					if ((myrect2.right-myrect2.left != CurrentDDSD.dwWidth) || (myrect2.bottom-myrect2.top != CurrentDDSD.dwHeight)) {
						PostMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 99);
					}
				}

				if ((lParam != 99) && (lParam != 1)) {
					nDefaultScreenScale=1;
				}
				break;

			case ID_CHANGESIZE_2X:
				{
					GetWindowRect(myWnd, &myrect);
					GetClientRect(myWnd, &myrect2);
					myrect.right = myrect.left + CurrentDDSD.dwWidth*2 + ((myrect.right - myrect.left)-(myrect2.right - myrect2.left));
					myrect.bottom = myrect.top + CurrentDDSD.dwHeight*2 + ((myrect.bottom - myrect.top)-(myrect2.bottom - myrect2.top));
					MoveWindow(myWnd, myrect.left, myrect.top, myrect.right-myrect.left, myrect.bottom-myrect.top, true);
				}

				if (STRETCH_NONE == StretchMode) {
					GetClientRect(myWnd, &myrect);
					myDC=GetDC(myWnd);
					FillRect(myDC, &myrect, (HBRUSH)(COLOR_MENU+1));
					ReleaseDC(myWnd, myDC);
				}

				GetClientRect(myWnd, &myrect2);
				debug_write("Client rect now %d x %d", myrect2.right-myrect2.left, myrect2.bottom-myrect2.top);

				// repeat it once only - this accounts for the menu changing from 1 to 2 lines and back
				if (lParam != 99) {
					if ((myrect2.right-myrect2.left != CurrentDDSD.dwWidth*2) || (myrect2.bottom-myrect2.top != CurrentDDSD.dwHeight*2)) {
						PostMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_2X, 99);
					}
				}

				if ((lParam != 99) && (lParam != 1)) {
					nDefaultScreenScale=2;
				}
				break;

			case ID_CHANGESIZE_3X:
				{
					GetWindowRect(myWnd, &myrect);
					GetClientRect(myWnd, &myrect2);
					myrect.right = myrect.left + CurrentDDSD.dwWidth*3 + ((myrect.right - myrect.left)-(myrect2.right - myrect2.left));
					myrect.bottom = myrect.top + CurrentDDSD.dwHeight*3 + ((myrect.bottom - myrect.top)-(myrect2.bottom - myrect2.top));
					MoveWindow(myWnd, myrect.left, myrect.top, myrect.right-myrect.left, myrect.bottom-myrect.top, true);
				}

				if (STRETCH_NONE == StretchMode) {
					GetClientRect(myWnd, &myrect);
					myDC=GetDC(myWnd);
					FillRect(myDC, &myrect, (HBRUSH)(COLOR_MENU+1));
					ReleaseDC(myWnd, myDC);
				}

				GetClientRect(myWnd, &myrect2);
				debug_write("Client rect now %d x %d", myrect2.right-myrect2.left, myrect2.bottom-myrect2.top);

				// repeat it once only - this accounts for the menu changing from 1 to 2 lines and back
				if (lParam != 99) {
					if ((myrect2.right-myrect2.left != CurrentDDSD.dwWidth*3) || (myrect2.bottom-myrect2.top != CurrentDDSD.dwHeight*3)) {
						PostMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_3X, 99);
					}
				}

				if ((lParam != 99) && (lParam != 1)) {
					nDefaultScreenScale=3;
				}
				break;

			case ID_CHANGESIZE_4X:
				{
					GetWindowRect(myWnd, &myrect);
					GetClientRect(myWnd, &myrect2);
					myrect.right = myrect.left + CurrentDDSD.dwWidth*4 + ((myrect.right - myrect.left)-(myrect2.right - myrect2.left));
					myrect.bottom = myrect.top + CurrentDDSD.dwHeight*4 + ((myrect.bottom - myrect.top)-(myrect2.bottom - myrect2.top));
					MoveWindow(myWnd, myrect.left, myrect.top, myrect.right-myrect.left, myrect.bottom-myrect.top, true);
				}

				if (STRETCH_NONE == StretchMode) {
					GetClientRect(myWnd, &myrect);
					myDC=GetDC(myWnd);
					FillRect(myDC, &myrect, (HBRUSH)(COLOR_MENU+1));
					ReleaseDC(myWnd, myDC);
				}

				GetClientRect(myWnd, &myrect2);
				debug_write("Client rect now %d x %d", myrect2.right-myrect2.left, myrect2.bottom-myrect2.top);

				// repeat it once only - this accounts for the menu changing from 1 to 2 lines and back
				if (lParam != 99) {
					if ((myrect2.right-myrect2.left != CurrentDDSD.dwWidth*4) || (myrect2.bottom-myrect2.top != CurrentDDSD.dwHeight*4)) {
						PostMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_4X, 99);
					}
				}
				if ((lParam != 99) && (lParam != 1)) {
					nDefaultScreenScale=4;
				}
				break;

			case ID_VIDEO_STRETCHMODE_NONE:		// This mode is a fallback, it must not fail and must not loop back
				StretchMode=STRETCH_NONE;
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_NONE, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DIB, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DX, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DXFULLSCREEN, MF_UNCHECKED);
				InvalidateRect(myWnd, NULL, false);
				break;

			case ID_VIDEO_STRETCHMODE_DIB:
				StretchMode=STRETCH_DIB;
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DIB, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DX, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DXFULLSCREEN, MF_UNCHECKED);
				InvalidateRect(myWnd, NULL, false);
				break;

			case ID_VIDEO_STRETCHMODE_DX:
				StretchMode=STRETCH_DX;
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DIB, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DX, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DXFULLSCREEN, MF_UNCHECKED);
				// NOT checking for bAppLockFullScreen here - this is not a security setting
				// if the user bypasses it, then good for them
				takedownDirectDraw();
				SetupDirectDraw(false);
				if (preFullSet) {
					nVideoLeft = preFullX;
					nVideoTop = preFullY;
					nXSize = preFullXS;
					nYSize = preFullYS;
				}
				RestoreWindowPosition();
				if (STRETCH_DX != StretchMode) {
					myproc(hwnd, WM_COMMAND, ID_VIDEO_STRETCHMODE_NONE, 0);
				} else {
					InvalidateRect(myWnd, NULL, false);
				}
				break;

			case ID_VIDEO_STRETCHMODE_DXFULLSCREEN:
				StretchMode=STRETCH_FULL;
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DIB, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DX, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_STRETCHMODE_DXFULLSCREEN, MF_CHECKED);
				if (!GetWindowRect(myWnd, &gWindowRect)) {
					gWindowRect.left = -1;
					gWindowRect.top = -1;
				} else {
					nVideoLeft = gWindowRect.left;
					nVideoTop = gWindowRect.top;
				}
				preFullX = nVideoLeft;
				preFullY = nVideoTop;
				preFullXS = nXSize;
				preFullYS = nYSize;
				preFullSet = true;
				// no need to check bAppLockFullScreen here, we are /entering/ full screen
				takedownDirectDraw();
				SetupDirectDraw(true);
				// SetupDirectDraw will cancel StretchMode if it fails
				if (STRETCH_FULL != StretchMode) {
					myproc(hwnd, WM_COMMAND, ID_VIDEO_STRETCHMODE_NONE, 0);
				} else {
					InvalidateRect(myWnd, NULL, false);
				}
				break;

			case ID_VIDEO_FILTERMODE_NONE:
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_NONE, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_HQ4X, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPER2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPEREAGLE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_TVMODE, MF_UNCHECKED);
				if (FilterMode != 0) {
					ResizeBackBuffer(256+16, 192+16);
				}
				FilterMode=0;
				InvalidateRect(myWnd, NULL, false);
				if (lParam != 1) {
					SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 0);	// allow settings to change!
				}
				break;

			case ID_VIDEO_FILTERMODE_HQ4X:
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_HQ4X, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPER2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPEREAGLE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_TVMODE, MF_UNCHECKED);
				if (FilterMode != 5) {
					ResizeBackBuffer((256+16)*4, (192+16)*4);
				}
				FilterMode=5;
				InvalidateRect(myWnd, NULL, false);
				if (lParam != 1) {
					SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 0);	// allow settings to change!
				}
				break;

			case ID_VIDEO_FILTERMODE_2XSAI:
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_2XSAI, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_HQ4X, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPER2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPEREAGLE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_TVMODE, MF_UNCHECKED);
				if (FilterMode != 1) {
					ResizeBackBuffer(512+32,384+29);
				}
				FilterMode=1;
				InvalidateRect(myWnd, NULL, false);
				if (lParam != 1) {
					SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 0);	// allow settings to change!
				}
				break;

			case ID_VIDEO_FILTERMODE_SUPER2XSAI:
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPER2XSAI, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_HQ4X, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPEREAGLE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_TVMODE, MF_UNCHECKED);
				if (FilterMode != 2) {
					ResizeBackBuffer(512+32,384+29);
				}
				FilterMode=2;
				InvalidateRect(myWnd, NULL, false);
				if (lParam != 1) {
					SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 0);	// allow settings to change!
				}
				break;

			case ID_VIDEO_FILTERMODE_SUPEREAGLE:
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_HQ4X, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPER2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPEREAGLE, MF_CHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_TVMODE, MF_UNCHECKED);
				if (FilterMode != 3) {
					ResizeBackBuffer(512+32,384+29);
				}
				FilterMode=3;
				InvalidateRect(myWnd, NULL, false);
				if (lParam != 1) {
					SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 0);	// allow settings to change!
				}
				break;

			case ID_VIDEO_FILTERMODE_TVMODE:
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_NONE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_HQ4X, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPER2XSAI, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_SUPEREAGLE, MF_UNCHECKED);
				CheckMenuItem(GetMenu(myWnd), ID_VIDEO_FILTERMODE_TVMODE, MF_CHECKED);
				if (FilterMode != 4) {
					ResizeBackBuffer(640,384+29);
				}
				FilterMode=4;
				InvalidateRect(myWnd, NULL, false);
				if (lParam != 1) {
					SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X, 0);	// allow settings to change!
				}
				break;

			default:
				return(DefWindowProc(hwnd, msg, wParam, lParam));
			}
			break;

		case WM_SIZE: 
			{
				HDC myDC;
				
				if ((MaintainAspect)&&(bWindowInitComplete))
				{
					GetWindowRect(myWnd, &myrect);
					GetClientRect(myWnd, &myrect2);
					i1=((myrect.right - myrect.left)-(myrect2.right - myrect2.left))/2;
					myrect.left += i1;
					myrect.right = myrect.left + myrect2.right;
					myrect.bottom -= i1;
					myrect.top = myrect.bottom - myrect2.bottom;

					double ratio=(double)CurrentDDSD.dwHeight / (double)CurrentDDSD.dwWidth;
					
					height=(float)((myrect.right - myrect.left) * ratio);
					height-=(myrect.bottom - myrect.top);

					if (height)
					{
						GetWindowRect(myWnd, &myrect);
						myrect.bottom+=(long)height;
						MoveWindow(myWnd, myrect.left, myrect.top, myrect.right-myrect.left, myrect.bottom-myrect.top, true);
					}
				}

				if ((MaintainAspect)||(STRETCH_NONE == StretchMode)) {
					GetClientRect(myWnd, &myrect);
					myDC=GetDC(myWnd);
					FillRect(myDC, &myrect, (HBRUSH)(COLOR_MENUTEXT+1));	// must add 1 to system colors - this is normally black
					ReleaseDC(myWnd, myDC);
				}

				if ((!IsZoomed(myWnd)) && (StretchMode != STRETCH_FULL)) {
					// save sizes if not maximized, not full screen (and finished setting up)
                    if (bWindowInitComplete) {
					    GetWindowRect(myWnd, &myrect);
					    nXSize = myrect.right-myrect.left;
					    nYSize = myrect.bottom-myrect.top;
					    nDefaultScreenScale=-1;		// it's custom now
                    }
				}
			}
			break;

		case WM_MOUSEACTIVATE:
			SetEvent(BlitEvent);
			return(DefWindowProc(hwnd, msg, wParam, lParam));
			break;

		case WM_RBUTTONUP:	// exit full screen - if allowed
			if ((STRETCH_FULL == StretchMode) && (!bAppLockFullScreen)) {
				MuteAudio();
				StretchMode=STRETCH_DX;
				takedownDirectDraw();
				SetupDirectDraw(false);
				if (preFullSet) {
					nVideoLeft = preFullX;
					nVideoTop = preFullY;
					nXSize = preFullXS;
					nYSize = preFullYS;
				}
				RestoreWindowPosition();
				SetSoundVolumes();
			}
			break;

		case WM_SETFOCUS: 
			if (PauseInactive) {
				// Re-enable sounds
				SetSoundVolumes();
                WindowActive = 1;
			}
			break;

		case WM_KILLFOCUS:
			// Disable sounds, cache current levels
			if (PauseInactive) {
				MuteAudio();
                WindowActive = 0;
			}
			// clear all keyboard state
#ifdef _DEBUG
			for (int idx=0; idx<sizeof(key); idx++) {
				if (key[idx]) {
					debug_write("Kill focus clearing key %d", idx);
					key[idx]=0;
				}
			}
#else
			memset(key, 0, sizeof(key));
#endif
			init_kb();
			break;

		default:
			return(DefWindowProc(hwnd, msg, wParam, lParam));
		}
		return 0;
	}

	return(DefWindowProc(hwnd, msg, wParam, lParam));
}


INT_PTR CALLBACK AudioBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int nRate=44100;
//	static int nLocalSidEnable = false;

    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
				case IDC_11KHZ:
					nRate=11025;
					break;
				case IDC_22KHZ:
					nRate=22050;
					break;
				case IDC_44KHZ:
					nRate=44100;
					break;

//				case IDC_SIDENABLE:
//					nLocalSidEnable = (SendDlgItemMessage(hwnd, IDC_SIDENABLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
//					break;

                case IDOK: 
					max_volume=SendDlgItemMessage(hwnd, IDC_SLDVOL, TBM_GETPOS, 0, 0);
					AudioSampleRate=nRate;
					enableBackgroundHum = SendDlgItemMessage(hwnd, IDC_CHKNOISE, BM_GETCHECK, 0, 0)==BST_CHECKED;

#if 0
					if (NULL == SetSidEnable) {
						if (nLocalSidEnable) {
							MessageBox(myWnd, "SID DLL is not available", "Classic99 Support File Missing", MB_OK);
						}
					} else {
						SetSidEnable(nLocalSidEnable != 0);
					}
#endif
                    // Fall through. 
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
                    return TRUE; 

				 case IDC_DEFAULT:
					// set the values to default
					SendDlgItemMessage(hwnd, IDC_SLDVOL, TBM_SETPOS, TRUE, 80);
					return TRUE;
            } 
			break;

		case WM_INITDIALOG:
			SendDlgItemMessage(hwnd, IDC_SLDVOL, TBM_SETRANGE, TRUE, MAKELONG(0,100));
			SendDlgItemMessage(hwnd, IDC_SLDVOL, TBM_SETPOS, TRUE, max_volume);
			SendDlgItemMessage(hwnd, IDC_11KHZ, BM_SETCHECK, BST_UNCHECKED, 0);
			SendDlgItemMessage(hwnd, IDC_22KHZ, BM_SETCHECK, BST_UNCHECKED, 0);
			SendDlgItemMessage(hwnd, IDC_44KHZ, BM_SETCHECK, BST_UNCHECKED, 0);
			switch (AudioSampleRate) {
				case 11025:
					nRate=11025;
					SendDlgItemMessage(hwnd, IDC_11KHZ, BM_SETCHECK, BST_CHECKED, 0);
					break;
				case 44100:
					nRate=44100;
					SendDlgItemMessage(hwnd, IDC_44KHZ, BM_SETCHECK, BST_CHECKED, 0);
					break;
				default:
					nRate=22050;
					SendDlgItemMessage(hwnd, IDC_22KHZ, BM_SETCHECK, BST_CHECKED, 0);
					break;
			}
			SendDlgItemMessage(hwnd, IDC_CHKNOISE, BM_SETCHECK, enableBackgroundHum?BST_CHECKED:BST_UNCHECKED, 0);
#if 0
			if (NULL != GetSidEnable) {
				nLocalSidEnable = GetSidEnable();
			} else {
				nLocalSidEnable = false;
			}
			SendDlgItemMessage(hwnd, IDC_SIDENABLE, BM_SETCHECK, nLocalSidEnable ? BST_CHECKED:BST_UNCHECKED, 0);
#endif
			return TRUE;
    } 
    return FALSE; 
} 

INT_PTR CALLBACK OptionsBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					// Read new values from dialog
					SendDlgItemMessage(hwnd, IDC_AVIFILENAME, WM_GETTEXT, 256, (LPARAM)AVIFileName);
					max_cpf=(hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF)*SendDlgItemMessage(hwnd, IDC_SLDCPU, TBM_GETPOS, 0, 0)/10;
					cfg_cpf=max_cpf;
					drawspeed=SendDlgItemMessage(hwnd, IDC_SLDFRAMESKIP, TBM_GETPOS, 0, 0);
					slowdown_keyboard=IsDlgButtonChecked(hwnd, IDC_CHKSLOWKEY)?1:0;
					
					if (IsDlgButtonChecked(hwnd, IDC_THROTTLECPU)) {
						ThrottleMode = THROTTLE_NORMAL;
					}
					if (IsDlgButtonChecked(hwnd, IDC_UNTHROTTLECPU)) {
						ThrottleMode = THROTTLE_OVERDRIVE;
					}
					if (IsDlgButtonChecked(hwnd, IDC_UNTHROTTLEALL)) {
						ThrottleMode = THROTTLE_SYSTEMMAXIMUM;
					}
					if (IsDlgButtonChecked(hwnd, IDC_CHKALTF4)) {
						enableAltF4 = true;
					} else {
						enableAltF4 = false;
					}
					if (IsDlgButtonChecked(hwnd, IDC_CHKF10)) {
						enableF10Menu = true;
					} else {
						enableF10Menu = false;
					}
					{
						// SAMS
						int old_sams_enabled=sams_enabled;
						int old_sams_size=sams_size;
						if (IsDlgButtonChecked(hwnd, IDC_AMS_0K)) {
							sams_enabled=0;
						}
						if (IsDlgButtonChecked(hwnd, IDC_AMS_128K)) {
							sams_enabled=1;
							sams_size=0;
						}
						if (IsDlgButtonChecked(hwnd, IDC_AMS_256K)) {
							sams_enabled=1;
							sams_size=1;
						}
						if (IsDlgButtonChecked(hwnd, IDC_AMS_512K)) {
							sams_enabled=1;
							sams_size=2;
						}
						if (IsDlgButtonChecked(hwnd, IDC_AMS_1024K)) {
							sams_enabled=1;
							sams_size=3;
						}
						if ((sams_enabled!=old_sams_enabled)||(sams_size!=old_sams_size)) {
							// changing this in real time wipes memory, so don't do that!
				            int ret=IDNO;
                            if (VerifyOpenFiles(hwnd)) {
    							ret=MessageBox(hwnd, "This will reset the emulator - are you sure?", "Change AMS Memory size", MB_YESNO|MB_ICONQUESTION);
                            }
							if (IDYES == ret) {
								PostMessage(myWnd, WM_COMMAND, ID_FILE_RESET, 0);
							} else {
								sams_enabled=old_sams_enabled;
								sams_size=old_sams_size;
							}
						}
					}

					// joysticks
					fJoy=IsDlgButtonChecked(hwnd, IDC_CHKJOYST)?1:0;
					joyStick[0].changeMode(SendDlgItemMessage(hwnd, IDC_JOY1LIST, CB_GETCURSEL, 0, 0));
					joyStick[1].changeMode(SendDlgItemMessage(hwnd, IDC_JOY2LIST, CB_GETCURSEL, 0, 0));
					fJoystickActiveOnKeys=0;		// reset here too

					// Special - tell the CPU Throttle menu item the new state
					PostMessage(myWnd, WM_COMMAND, ID_OPTIONS_CPUTHROTTLING, 1);
                    // Fall through. 
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
                    return TRUE; 

				 case IDC_BROWSEAVI:
					 // Browse for new AVI filename
					 {
						OPENFILENAME ofn;
						char buf[256];

						memset(&ofn, 0, sizeof(OPENFILENAME));
						ofn.lStructSize=sizeof(OPENFILENAME);
						ofn.hwndOwner=hwnd;
						ofn.lpstrFilter="AVI Files\0*.avi\0\0";
						SendDlgItemMessage(hwnd, IDC_AVIFILENAME, WM_GETTEXT, 256, (LPARAM)buf);
						ofn.lpstrFile=buf;
						ofn.nMaxFile=256;
						ofn.Flags=OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;

						char szTmpDir[MAX_PATH];
						GetCurrentDirectory(MAX_PATH, szTmpDir);

						if (GetSaveFileName(&ofn)) {
							SendDlgItemMessage(hwnd, IDC_AVIFILENAME, WM_SETTEXT, 0, (LPARAM)ofn.lpstrFile);
						}

						SetCurrentDirectory(szTmpDir);
					 }

					 return TRUE;
            } 
			break;

		case WM_INITDIALOG:
			// load the controls with current values
			SendDlgItemMessage(hwnd, IDC_AVIFILENAME, WM_SETTEXT, 0, (LPARAM)AVIFileName);
			SendDlgItemMessage(hwnd, IDC_SLDCPU, TBM_SETRANGE, TRUE, MAKELONG(1,20));
			SendDlgItemMessage(hwnd, IDC_SLDCPU, TBM_SETPOS, TRUE, (max_cpf*10)/(hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF));
			SendDlgItemMessage(hwnd, IDC_SLDFRAMESKIP, TBM_SETRANGE, TRUE, MAKELONG(0,10));
			SendDlgItemMessage(hwnd, IDC_SLDFRAMESKIP, TBM_SETPOS, TRUE, drawspeed);
			SendDlgItemMessage(hwnd, IDC_CHKJOYST, BM_SETCHECK, fJoy?BST_CHECKED:BST_UNCHECKED, 0);
			SendDlgItemMessage(hwnd, IDC_CHKSLOWKEY, BM_SETCHECK, slowdown_keyboard?BST_CHECKED:BST_UNCHECKED, 0);
			SendDlgItemMessage(hwnd, IDC_CHKALTF4, BM_SETCHECK, enableAltF4?BST_CHECKED:BST_UNCHECKED, 0);
			SendDlgItemMessage(hwnd, IDC_CHKF10, BM_SETCHECK, enableF10Menu?BST_CHECKED:BST_UNCHECKED, 0);
			
			// default
			CheckRadioButton(hwnd, IDC_THROTTLECPU, IDC_UNTHROTTLEALL, IDC_THROTTLECPU);
			if (ThrottleMode == THROTTLE_OVERDRIVE) {
				CheckRadioButton(hwnd, IDC_THROTTLECPU, IDC_UNTHROTTLEALL, IDC_UNTHROTTLECPU);
			}
			if (ThrottleMode == THROTTLE_SYSTEMMAXIMUM) {
				CheckRadioButton(hwnd, IDC_THROTTLECPU, IDC_UNTHROTTLEALL, IDC_UNTHROTTLEALL);
			}

			// TODO: AMS configuration doesn't work right now anyway
//			CheckRadioButton(hwnd, IDC_AMS_0K, IDC_AMS_1024K, sams_enabled?sams_size+IDC_AMS_128K:IDC_AMS_0K);

			SendDlgItemMessage(hwnd, IDC_JOY1LIST, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hwnd, IDC_JOY2LIST, CB_RESETCONTENT, 0, 0);
			SendDlgItemMessage(hwnd, IDC_JOY1LIST, CB_ADDSTRING, 0, (LPARAM)"0 - Keyboard");
			SendDlgItemMessage(hwnd, IDC_JOY2LIST, CB_ADDSTRING, 0, (LPARAM)"0 - Keyboard");
			for (int idx=JOYSTICKID1; idx<JOYSTICKID1+16; ++idx) {
				JOYCAPS caps;
				if (JOYERR_NOERROR == joyGetDevCaps(idx, &caps, sizeof(caps))) {
					char buf[256];
					_snprintf(buf, sizeof(buf), "%d - %s", idx-JOYSTICKID1+1, caps.szPname);
					buf[sizeof(buf)-1]='\0';
					SendDlgItemMessage(hwnd, IDC_JOY1LIST, CB_ADDSTRING, 0, (LPARAM)buf);
					SendDlgItemMessage(hwnd, IDC_JOY2LIST, CB_ADDSTRING, 0, (LPARAM)buf);
				}
			}
			if (joyStick[0].mode < 0) joyStick[0].mode = 0;	// -1 erases the list, so don't allow that
			if (joyStick[1].mode < 0) joyStick[1].mode = 0;
			SendDlgItemMessage(hwnd, IDC_JOY1LIST, CB_SETCURSEL, joyStick[0].mode, 0);
			SendDlgItemMessage(hwnd, IDC_JOY2LIST, CB_SETCURSEL, joyStick[1].mode, 0);

			return TRUE;

    } 
    return FALSE; 
} 

INT_PTR CALLBACK GramBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					// Read new values from dialog
					for (int i=0; i<8; i++) {
						GROMBase[0].bWritable[i] = (SendDlgItemMessage(hwnd, IDC_WRITEGROM0+i, BM_GETCHECK, 0, 0) == BST_CHECKED);
					}
                    // Fall through. 
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
                    return TRUE; 
            } 
			break;

		case WM_INITDIALOG:
			// load the controls with current values
			for (int i=0; i<8; i++) {
				SendDlgItemMessage(hwnd, IDC_WRITEGROM0+i, BM_SETCHECK, GROMBase[0].bWritable[i] ? BST_CHECKED : BST_UNCHECKED, 0);
			}
			return TRUE;

    } 
    return FALSE; 
}

INT_PTR CALLBACK KBMapProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// only tricky part is loading the kb image resource
	static HBITMAP hBmp=NULL;
	HWND hWnd=NULL;

    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
                    // Fall through. 
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
					if (NULL != hBmp) {
						DeleteObject(hBmp);
						hBmp=NULL;
					}
					hKBMap=NULL;
                    return TRUE; 
            } 
			break;

		case WM_INITDIALOG:
			// load the bitmap into the frame
			hWnd=GetDlgItem(hwnd, IDC_IMAGE);
			if (NULL != hWnd) {
				hBmp=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_KBMAP));
				if (NULL != hBmp) {
					SendMessage(hWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
				}
			}
			return TRUE;
    } 
    return FALSE; 
} 

INT_PTR CALLBACK HeatMapProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// handles a running heatmap display - showing accessed memory
	HWND hWnd=NULL;

    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
                    // Fall through. 
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
					if (NULL != hHeatBmp) {
						DeleteObject(hHeatBmp);
						hHeatBmp=NULL;
					}
					hHeatMap=NULL;
                    return TRUE; 
            } 
			break;

		case WM_INITDIALOG:
			// create a bitmap for the frame
			hWnd=GetDlgItem(hwnd, IDC_IMAGE);
			if (NULL != hWnd) {
				hHeatBmp=CreateCompatibleBitmap(NULL, 256, 256);
				if (NULL != hHeatBmp) {
					SendMessage(hWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hHeatBmp);
				}
			}
			return TRUE;
    } 
    return FALSE; 
} 

INT_PTR CALLBACK BreakPointHelpProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// nothing fancy here
    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
                    // Fall through. 
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
					hBrkHlp=NULL;
                    return TRUE; 
            } 
			break;

		case WM_INITDIALOG:
			return TRUE;
    } 
    return FALSE; 
} 

// External references
INT_PTR CALLBACK TVBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static double hue=0, sat=0, cont=0, bright=0, sharp=0;
	double thue, tsat, tcont, tbright, tsharp, tmp;

    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					TVScanLines=IsDlgButtonChecked(hwnd, IDC_SCANLINES)?1:0;

					// Read the values one more time
					tmp=SendDlgItemMessage(hwnd, IDC_HUE, TBM_GETPOS, 0, 0);
					thue=(tmp-100)/100.0;
					tmp=SendDlgItemMessage(hwnd, IDC_SAT, TBM_GETPOS, 0, 0);
					tsat=(tmp-100)/100.0;
					tmp=SendDlgItemMessage(hwnd, IDC_CONT, TBM_GETPOS, 0, 0);
					tcont=(tmp-100)/100.0;
					tmp=SendDlgItemMessage(hwnd, IDC_BRIGHT, TBM_GETPOS, 0, 0);
					tbright=(tmp-100)/100.0;
					tmp=SendDlgItemMessage(hwnd, IDC_SHARP, TBM_GETPOS, 0, 0);
					tsharp=(tmp-100)/100.0;

					SetTVValues(thue, tsat, tcont, tbright, tsharp);
					InvalidateRect(myWnd, NULL, false);

                    EndDialog(hwnd, wParam); 
					hTVDlg=NULL;
                    return TRUE; 

				case IDCANCEL: 
					// set back to cached values (note: no cancel button on dialog today!)
					SetTVValues(hue, sat, cont, bright, sharp);
                    EndDialog(hwnd, wParam); 
					hTVDlg=NULL;
                    return TRUE; 

				case IDC_RESET:
					SetTVValues(0, 0, 0, 0, 0);
					InvalidateRect(myWnd, NULL, false);
					
					SendDlgItemMessage(hwnd, IDC_HUE, TBM_SETPOS, TRUE, 100);
					SendDlgItemMessage(hwnd, IDC_SAT, TBM_SETPOS, TRUE, 100);
					SendDlgItemMessage(hwnd, IDC_CONT, TBM_SETPOS, TRUE, 100);
					SendDlgItemMessage(hwnd, IDC_BRIGHT, TBM_SETPOS, TRUE, 100);
					SendDlgItemMessage(hwnd, IDC_SHARP, TBM_SETPOS, TRUE, 100);
					return TRUE;

				 case IDC_SCANLINES:
					TVScanLines=IsDlgButtonChecked(hwnd, IDC_SCANLINES)?1:0;
					InvalidateRect(myWnd, NULL, false);
					return TRUE;
            } 
			break;

		case WM_VSCROLL:
			if (NULL != lParam) {
				switch (LOWORD(wParam)) {
					case SB_THUMBPOSITION:
					case SB_THUMBTRACK:
						// High word contains absolute position
						tmp=(HIWORD(wParam)-100)/100.0;
						GetTVValues(&thue, &tsat, &tcont, &tbright, &tsharp);
						if (lParam == (long)GetDlgItem(hwnd, IDC_HUE)) {
							thue=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_SAT)) {
							tsat=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_CONT)) {
							tcont=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_BRIGHT)) {
							tbright=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_SHARP)) {
							tsharp=tmp;
						}
						SetTVValues(thue, tsat, tcont, tbright, tsharp);
						InvalidateRect(myWnd, NULL, false);
						break;

					case SB_BOTTOM:
					case SB_TOP:
					case SB_LINEDOWN:
					case SB_LINEUP:
					case SB_PAGEDOWN:
					case SB_PAGEUP:
					case SB_ENDSCROLL:
						// Need to query for the position (mostly keyboard interface)
						tmp=SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
						tmp=(tmp-100)/100.0;
						GetTVValues(&thue, &tsat, &tcont, &tbright, &tsharp);
						if (lParam == (long)GetDlgItem(hwnd, IDC_HUE)) {
							thue=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_SAT)) {
							tsat=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_CONT)) {
							tcont=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_BRIGHT)) {
							tbright=tmp;
						}
						if (lParam == (long)GetDlgItem(hwnd, IDC_SHARP)) {
							tsharp=tmp;
						}
						SetTVValues(thue, tsat, tcont, tbright, tsharp);
						InvalidateRect(myWnd, NULL, false);
						break;
				}
			}
			return 0;

		case WM_INITDIALOG:
			// cache the current values in case we cancel
			GetTVValues(&hue, &sat, &cont, &bright, &sharp);

			// load the controls with current values
			SendDlgItemMessage(hwnd, IDC_HUE, TBM_SETRANGE, TRUE, MAKELONG(0,200));
			SendDlgItemMessage(hwnd, IDC_HUE, TBM_SETPOS, TRUE, (int)((hue+1.0)*100));

			SendDlgItemMessage(hwnd, IDC_SAT, TBM_SETRANGE, TRUE, MAKELONG(0,200));
			SendDlgItemMessage(hwnd, IDC_SAT, TBM_SETPOS, TRUE, (int)((sat+1.0)*100));

			SendDlgItemMessage(hwnd, IDC_CONT, TBM_SETRANGE, TRUE, MAKELONG(0,200));
			SendDlgItemMessage(hwnd, IDC_CONT, TBM_SETPOS, TRUE, (int)((cont+1.0)*100));

			SendDlgItemMessage(hwnd, IDC_BRIGHT, TBM_SETRANGE, TRUE, MAKELONG(0,200));
			SendDlgItemMessage(hwnd, IDC_BRIGHT, TBM_SETPOS, TRUE, (int)((bright+1.0)*100));

			SendDlgItemMessage(hwnd, IDC_SHARP, TBM_SETRANGE, TRUE, MAKELONG(0,200));
			SendDlgItemMessage(hwnd, IDC_SHARP, TBM_SETPOS, TRUE, (int)((sharp+1.0)*100));

			SendDlgItemMessage(hwnd, IDC_SCANLINES, BM_SETCHECK, TVScanLines?BST_CHECKED:BST_UNCHECKED, 0);

			return TRUE;

    } 
    return FALSE; 
} 

// read a one-five digit hex address which might also be a range
bool ReadRange(int nType, int *A, int *B, int *Bank, char *str) {
	*A=-1;		// set the failure state now
	*Bank=-1;	// no bank specified by default

	if (*str == '(') {
		// it's a range, so read A and B
		if (2 != sscanf(str, "(%x-%x", A, B)) {
			// invalid, ignore what we got
			*A=-1;
		} else {
			*A &= 0xFFFFF;
			*B &= 0xFFFFF;
		}
	} else {
		// single value, B to 0
		*B=0;
		if ((nType == BREAK_EQUALS_REGISTER) || (nType == BREAK_EQUALS_VDPREG)) {
			// we read registers in decimal by convention
			if (1 != sscanf(str, "%d", A)) {
				// invalid, ignore what we got
				*A=-1;
			} else {
				*A &= 0xFFFFF;
			}
		} else {
			if (1 != sscanf(str, "%x", A)) {
				// invalid, ignore what we got
				*A=-1;
			} else {
				*A &= 0xFFFFF;
			}
		}
	}

	// check for a valid bank
	char *p = strchr(str, ':');
	if (p != NULL) {
		if (1 != sscanf(p+1, "%x", Bank)) {
			// invalid, ignore it and fail
			*Bank = -1;
			*A = -1;
			return false;
		}
		*Bank &= 0x0F;
	}

	return ((*A)!=-1);
}

// returns a formatted string - not thread safe
const char *FormatBreakpoint(int idx) {
	static char szTmp[32];
	int pos=0;
	
	strcpy(szTmp, "");		// empty it for now

	if (idx < MAX_BREAKPOINTS) {
		// write the prefix, if any
		switch (BreakPoints[idx].Type) {
            case BREAK_PC:
                // nothing to do here
                break;

			case BREAK_ACCESS:
				szTmp[pos++]='*';
				break;

			case BREAK_WRITE:
				szTmp[pos++]='>';
				break;

			case BREAK_WRITEAMS:
				szTmp[pos++]='>';
				szTmp[pos++]='X';
				break;

            case BREAK_WRITEVDP:
				szTmp[pos++]='>';
				szTmp[pos++]='V';
				break;

			case BREAK_WRITEGROM:
				szTmp[pos++]='>';
				szTmp[pos++]='G';
				break;

			case BREAK_READ:
				szTmp[pos++]='<';
				break;

			case BREAK_READAMS:
				szTmp[pos++]='<';
				szTmp[pos++]='X';
				break;

            case BREAK_READVDP:
				szTmp[pos++]='<';
				szTmp[pos++]='V';
				break;

			case BREAK_READGROM:
				szTmp[pos++]='<';
				szTmp[pos++]='G';
				break;

			case BREAK_EQUALS_WORD:
				szTmp[pos++]='W';
				break;

			case BREAK_EQUALS_BYTE:
				szTmp[pos++]='M';
				break;

			case BREAK_EQUALS_VDP:
				szTmp[pos++]='V';
				break;

			case BREAK_EQUALS_VDPREG:
				szTmp[pos++]='U';
				break;

			case BREAK_EQUALS_AMS:
				szTmp[pos++]='X';
				break;

            case BREAK_EQUALS_REGISTER:
				szTmp[pos++]='R';
				break;

			case BREAK_RUN_TIMER:
				szTmp[pos++]='T';
				break;

			case BREAK_DISK_LOG:
				szTmp[pos++]='L';
				break;

            case BREAK_WP:
				szTmp[pos++]='W';
				szTmp[pos++]='P';
                break;

            case BREAK_ST:
				szTmp[pos++]='S';
				szTmp[pos++]='T';
                break;
		}

		// write the address or range
		switch (BreakPoints[idx].Type) {
			case BREAK_EQUALS_VDPREG:
			case BREAK_EQUALS_REGISTER:
				pos+=sprintf(&szTmp[pos], "%d", BreakPoints[idx].A);
				break;

            case BREAK_ST:
            case BREAK_WP:
                // these have no address
                break;

			default:
				if (BreakPoints[idx].B != 0) {
					pos+=sprintf(&szTmp[pos], "(%04X-%04X)", BreakPoints[idx].A, BreakPoints[idx].B);
				} else {
					pos+=sprintf(&szTmp[pos], "%04X", BreakPoints[idx].A);
				}
		}

		// write the bank, if needed
		if (BreakPoints[idx].Bank != -1) {
			pos+=sprintf(&szTmp[pos], ":%X", BreakPoints[idx].Bank);
		}

		// if there is a mask, then apply it
		if (BreakPoints[idx].Mask == 0) {
			BreakPoints[idx].Mask = 0xffff;
		}
		if (BreakPoints[idx].Mask != 0xffff) {
			pos+=sprintf(&szTmp[pos], "{%x}", BreakPoints[idx].Mask);
		}

		// write the data, if needed
		switch (BreakPoints[idx].Type) {
			case BREAK_EQUALS_WORD:
			case BREAK_EQUALS_REGISTER:
            case BREAK_WP:
            case BREAK_ST:
				// a word
				pos+=sprintf(&szTmp[pos], "=%04X", BreakPoints[idx].Data);
				break;

			case BREAK_EQUALS_BYTE:
			case BREAK_EQUALS_AMS:
			case BREAK_EQUALS_VDP:
			case BREAK_EQUALS_VDPREG:
				// a byte
				pos+=sprintf(&szTmp[pos], "=%02X", BreakPoints[idx].Data);
				break;

			case BREAK_DISK_LOG:
				// a number
				pos+=sprintf(&szTmp[pos], "=%d", BreakPoints[idx].Data);
				break;
		}
	}

	return szTmp;
}

// true if parsed successfully
// Note that buf1 is modified in place!
bool AddBreakpoint(char *buf1) {
	bool bRet=false;
	int A,B, bank, Mask;

	// this is normally checked by the caller, but just in case...
	if (nBreakPoints >= MAX_BREAKPOINTS) {
		return false;
	}
	
	// first check for a PC - no prefix
	if (ReadRange(BREAK_EQUALS_WORD, &A, &B, &bank, buf1)) {
		BreakPoints[nBreakPoints].Type=BREAK_PC;
		BreakPoints[nBreakPoints].A=A;
		BreakPoints[nBreakPoints].B=B;
		BreakPoints[nBreakPoints].Bank=bank;
		strcpy(buf1, FormatBreakpoint(nBreakPoints));
		nBreakPoints++;
		bRet=true;
	} else {
		int nType=BREAK_NONE;
		int nTmp=0;
		int nData=0;
		char *pTmp;
		
		buf1[0]=toupper(buf1[0]);
		switch (buf1[0]) {
		case '*':	// memory address access (read or write)
			nType=BREAK_ACCESS;
			break;
		case '>':	// write to address
			if (toupper(buf1[1])=='V') {
				nType=BREAK_WRITEVDP;
				memmove(&buf1[1], &buf1[2], strlen(&buf1[1]));
			} else if (toupper(buf1[1])=='G') {
				nType=BREAK_WRITEGROM;
				memmove(&buf1[1], &buf1[2], strlen(&buf1[1]));
            } else if (toupper(buf1[1])=='X') {
				nType=BREAK_WRITEAMS;
				memmove(&buf1[1], &buf1[2], strlen(&buf1[1]));
			} else {
				nType=BREAK_WRITE;
			}
			break;
		case '<':	// read from address
			if (toupper(buf1[1])=='V') {
				nType=BREAK_READVDP;
				memmove(&buf1[1], &buf1[2], strlen(&buf1[1]));
			} else if (toupper(buf1[1])=='G') {
				nType=BREAK_READGROM;
				memmove(&buf1[1], &buf1[2], strlen(&buf1[1]));
			} else if (toupper(buf1[1])=='X') {
				nType=BREAK_READAMS;
				memmove(&buf1[1], &buf1[2], strlen(&buf1[1]));
			} else {
				nType=BREAK_READ;
			}
			break;
		case 'W':	// Word =, or WP=
            if (buf1[1] == 'P') {
                nType=BREAK_WP;
			    pTmp=strchr(buf1, '=');
			    if (NULL == pTmp) {
				    nType=BREAK_NONE;
			    } else {
				    if (1 != sscanf(pTmp+1, "%x", &nData)) {
					    nType=BREAK_NONE;
				    }
                    nData &= 0xffff;
			    }
            } else {
			    nType=BREAK_EQUALS_WORD;
			    pTmp=strchr(buf1, '=');
			    if (NULL == pTmp) {
				    nType=BREAK_NONE;
			    } else {
				    if (1 != sscanf(pTmp+1, "%x", &nData)) {
					    nType=BREAK_NONE;
				    }
                    nData &= 0xffff;
			    }
            }
			break;
		case 'S':	// ST
            if (buf1[1] == 'T') {
                nType=BREAK_ST;
			    pTmp=strchr(buf1, '=');
			    if (NULL == pTmp) {
				    nType=BREAK_NONE;
			    } else {
				    if (1 != sscanf(pTmp+1, "%x", &nData)) {
					    nType=BREAK_NONE;
				    }
                    nData &= 0xffff;
			    }
            }
			break;
		case 'M':	// Memory = 
			nType=BREAK_EQUALS_BYTE;
			pTmp=strchr(buf1, '=');
			if (NULL == pTmp) {
				nType=BREAK_NONE;
			} else {
				if (1 != sscanf(pTmp+1, "%x", &nData)) {
					nType=BREAK_NONE;
				}
                nData &= 0xff;
			}
			break;
		case 'U':	// VDP register =
			nType=BREAK_EQUALS_VDPREG;
			pTmp=strchr(buf1, '=');
			if (NULL == pTmp) {
				nType=BREAK_NONE;
			} else {
				if (1 != sscanf(pTmp+1, "%x", &nData)) {
					nType=BREAK_NONE;
				}
                nData &= 0xff;
			}
			break;
		case 'V':	// VDP memory =
			nType=BREAK_EQUALS_VDP;
			pTmp=strchr(buf1, '=');
			if (NULL == pTmp) {
				nType=BREAK_NONE;
			} else {
				if (1 != sscanf(pTmp+1, "%x", &nData)) {
					nType=BREAK_NONE;
				}
                nData &= 0xff;
			}
			break;
		case 'X':	// AMS memory =
			nType=BREAK_EQUALS_AMS;
			pTmp=strchr(buf1, '=');
			if (NULL == pTmp) {
				nType=BREAK_NONE;
			} else {
				if (1 != sscanf(pTmp+1, "%x", &nData)) {
					nType=BREAK_NONE;
				}
                nData &= 0xff;
			}
			break;
		case 'R':	// Register =
			nType=BREAK_EQUALS_REGISTER;
			pTmp=strchr(buf1, '=');
			if (NULL == pTmp) {
				nType=BREAK_NONE;
			} else {
				// register is the only one that's read as decimal! But this is the register data!
				if (1 != sscanf(pTmp+1, "%x", &nData)) {
					nType=BREAK_NONE;
				}
			}
			break;

		case 'T':	// Timing run
			nType=BREAK_RUN_TIMER;
			break;

		case 'L':	// runtime log
			nType=BREAK_DISK_LOG;
			pTmp=strchr(buf1, '=');
			if (NULL == pTmp) {
				nType=BREAK_NONE;
			} else {
				// value from 1-9 for the disk file to write to
				if (1 != sscanf(pTmp+1, "%x", &nData)) {
					nType=BREAK_NONE;
				} else if ((nData < 1) || (nData > 9)) {
					nType = BREAK_NONE;
				}
			}
			break;
		}

		if (nType != BREAK_NONE) {
            // WP and ST have no ranges
            if ((nType != BREAK_WP)&&(nType != BREAK_ST)) {
    			if (!ReadRange(nType, &A, &B, &bank, &buf1[1])) {
                    // parse error
                    nType = BREAK_NONE;
                }
            }
        }

        // if it's STILL good...
        if (nType != BREAK_NONE) {
			// eliminate a few special cases
            // only AMS supports more than 64k
            if ((nType != BREAK_READAMS) && (nType != BREAK_WRITEAMS) && (nType != BREAK_EQUALS_AMS)) {
                A&=0xffff;
                B&=0xffff;
            }
			switch (nType) {
				case BREAK_RUN_TIMER:
					if (B==0) {
						nType = BREAK_NONE;		// invalid without range
					}
					break;

				case BREAK_EQUALS_REGISTER:
				case BREAK_EQUALS_VDPREG:
					if (B!=0) {
						nType = BREAK_NONE;		// range not legal on registers
					}
					if (bank != -1) {
						nType = BREAK_NONE;		// bank not valid on registers
					}
					break;

				case BREAK_EQUALS_AMS:
				case BREAK_EQUALS_VDP:
					if (bank != -1) {
						nType = BREAK_NONE;		// bank not valid on VDP or AMS
					}
					break;

				case BREAK_WRITEVDP:
				case BREAK_READVDP:
					if ((A>0x47ff) || (B>0x47ff)) {
						nType = BREAK_NONE;		// out of range for VDP RAM
					}
					break;
			}
		}

        // check for a mask
		Mask = 0xffff;
		pTmp = strchr(buf1, '{');
		if (NULL != pTmp) {
			if (1 != sscanf(pTmp+1, "%x", &Mask)) {
				nType = BREAK_NONE;
			}
			if (Mask == 0) Mask = 0xffff;
			if ((nData&Mask) != nData) {
				// invalid data
				nType = BREAK_NONE;
			}
		}

		if (nType != BREAK_NONE) {
			// still valid, now we need to save and format it
			BreakPoints[nBreakPoints].Type=nType;
			BreakPoints[nBreakPoints].A=A;
			BreakPoints[nBreakPoints].B=B;
			BreakPoints[nBreakPoints].Bank=bank;
			BreakPoints[nBreakPoints].Data=nData;
			BreakPoints[nBreakPoints].Mask=Mask;
			strcpy(buf1, FormatBreakpoint(nBreakPoints));
			nBreakPoints++;
			bRet=true;
		}
	}

	return bRet;
}

void LaunchDebugWindow() {
	// don't UNPAUSE if we're entering already paused
	bool paused = false;
	if (max_cpf == 0) paused = true;

	DoPause();
	Sleep(100);
	if (NULL == dbgWnd) {
		dbgWnd=CreateDialog(NULL, MAKEINTRESOURCE(IDD_DEBUG), myWnd, DebugBoxProc);

		// the new bigger debug views flicker pretty badly,
		// so we need to subclass them
		ctrl1 = GetDlgItem(dbgWnd, IDC_MAINEDIT);
		SetWindowLongPtr(ctrl1, GWLP_WNDPROC, (LONG_PTR)newEditProc);
		ctrl2 = GetDlgItem(dbgWnd, IDC_SECONDEDIT);
		SetWindowLongPtr(ctrl2, GWLP_WNDPROC, (LONG_PTR)newEditProc);
	}
	ShowWindow(dbgWnd, SW_SHOW);
	Sleep(100);
	if (!paused) {
		DoPlay();
	}
}

void UpdateMakeMenu(HWND hwnd, int enable) {
	HMENU menu=GetMenu(hwnd);
	if (menu != NULL) {
		EnableMenuItem(menu, ID_MAKE_SAVEPROGRAM, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
		DrawMenuBar(hwnd);
	}
}

// TODO: this creates a new bitmap every frame... optimize and also check for handle leaks
// We can improve it by reusing the same bitmap every frame
static void newPaint(HWND hWnd, LPPAINTSTRUCT lpPS)
{
    RECT rc;
    HFONT hfntOld = NULL;
	char *cap;
    HDC hdcMem;
    HBITMAP hbmMem, hbmOld;
    HBRUSH hbrBkGnd;

	// check which control
	if (hWnd == ctrl1) {
		cap = szCaption1;
	} else if (hWnd == ctrl2) {
		cap = szCaption2;
	} else {
		return;
	}

	// create the bitmap to draw into
    GetClientRect(hWnd, &rc);
	hdcMem = CreateCompatibleDC(lpPS->hdc);
    hbmMem = CreateCompatibleBitmap(lpPS->hdc,
                                    rc.right-rc.left,
                                    rc.bottom-rc.top);
    hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

	// erase the bitmap
    hbrBkGnd = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(hdcMem, &rc, hbrBkGnd);
    DeleteObject(hbrBkGnd);

	// add the font
	if (mainfont != NULL) {
		hfntOld = (HFONT)SelectObject(hdcMem, mainfont);
	}

	// draw it offscreen
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, GetSysColor(COLOR_WINDOWTEXT));
    DrawText(hdcMem,
             cap,
             -1,
             &rc,
             DT_LEFT);

    if (hfntOld) {
        SelectObject(lpPS->hdc, hfntOld);
    }

	// blit the changed image
    BitBlt(lpPS->hdc,
           rc.left, rc.top,
           rc.right-rc.left, rc.bottom-rc.top,
           hdcMem,
           0, 0,
           SRCCOPY);

	// clean up
    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

//https://docs.microsoft.com/en-us/previous-versions/ms969905(v=msdn.10)?redirectedfrom=MSDN
//https://docs.microsoft.com/en-us/windows/win32/controls/subclassing-overview
LRESULT CALLBACK newEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;
	char *cap;

	if (hWnd == ctrl1) {
		cap = szCaption1;
	} else if (hWnd == ctrl2) {
		cap = szCaption2;
	} else {
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

    switch(msg) {
    case WM_SETTEXT:
        strncpy(cap, (LPSTR)lParam, sizeof(szCaption1));	// assumes both strings are same size
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_SETFONT:
		// we use the parent window default font
        break;

	case WM_ERASEBKGND:
		// doesn't seem to be sent anyway...
		// lie and say we did it.
		return (LRESULT)1;
		break;

    case WM_PAINT:
        BeginPaint(hWnd, &ps);
        newPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_USER:
        {
            // control-c
            char *pText;
            int len;
            if (hWnd == ctrl1) {
                pText = szCaption1;
                len = sizeof(szCaption1);
            } else {
                pText = szCaption2;
                len = sizeof(szCaption2);
            }
            if (OpenClipboard(hWnd)) {
                EmptyClipboard();
                HANDLE hCopy = GlobalAlloc(GMEM_MOVEABLE, (len+1)*sizeof(TCHAR));
                if (NULL != hCopy) {
                    unsigned char *pLock = (unsigned char*)GlobalLock(hCopy);
                    memcpy(pLock, pText, len);
                    pLock[len] = (TCHAR)'\0';
                    GlobalUnlock(pLock);
                    SetClipboardData(CF_TEXT, hCopy);
                }
                CloseClipboard();
            }
        }
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
        break;
    }

    return NULL;
}

// TODO: we don't handle banking yet, so if you have address conflicts across
// banks only the last one read will be recorded.
extern void ImportMapFile(const char *fn);
int LoadMap(HWND *myhwnd) {
	OPENFILENAME ofn;                          // Structure for filename dialog
	char buf[256], buf2[256];

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize    = sizeof(OPENFILENAME);
	ofn.hwndOwner      = NULL;
	ofn.lpstrFilter    = "Map file\0*.lst;*.map;*.txt\0\0"; 
	strcpy(buf, "");
	ofn.lpstrFile      = buf;
	ofn.nMaxFile       = 256;
	strcpy(buf2, "");
	ofn.lpstrFileTitle = buf2;
	ofn.nMaxFileTitle  = 256;
	ofn.Flags          = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	char szTmpDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, szTmpDir);

    bool ret = GetOpenFileName(&ofn);

    SetCurrentDirectory(szTmpDir);

	if (ret) {
		debug_write("Reading map file ....");
		ImportMapFile(ofn.lpstrFile);
		return true;
	} else {
		return false;
	}
}

INT_PTR CALLBACK DebugBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char buf1[80];
	int idx;

    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
				case IDOK: 
				case IDCANCEL: 
				case ID_FILE_CLOSE40101:
                    EndDialog(hwnd, wParam); 
					g_bCheckUninit = false;
					bFrozenText = false;
					VDPDebug = false;
					dbgWnd=NULL;
                    return TRUE; 

				case ID_FILE_READMAPFILE:
					LoadMap(&hwnd);
					break;

				case ID_FILE_LOADBRK:
					LoadBreakpoints(&hwnd);
					break;

				case ID_FILE_SAVEBRK:					
					SaveBreakpoints(&hwnd);
					break;

				case IDC_RADIO1:
					bDebugDirty=true;
					// fall through
				case IDC_RADIO2:
				case IDC_RADIO3:
				case IDC_RADIO4:
				case IDC_RADIO5:
                case IDC_RADIO6:
					nMemType=LOWORD(wParam)-IDC_RADIO1;
					SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
					SetEvent(hDebugWindowUpdateEvent);
					break;

				case IDC_BREAKCPU:
					pCPU->enableDebug = (SendDlgItemMessage(hwnd, IDC_BREAKCPU, BM_GETCHECK, 0, 0)==BST_CHECKED) ? 1 : 0;
					break;

				case IDC_BREAKGPU:
					pGPU->enableDebug = (SendDlgItemMessage(hwnd, IDC_BREAKGPU, BM_GETCHECK, 0, 0)==BST_CHECKED) ? 1 : 0;
					break;

                case IDC_IGNORECONSOLE:
                    bIgnoreConsoleBreakpointHits = !bIgnoreConsoleBreakpointHits;
                    break;

				case IDC_ADDBREAK:
					{
						// add a new breakpoint
						if (nBreakPoints >= MAX_BREAKPOINTS) {
							Beep(550,100);
							break;
						}
						if (SendDlgItemMessage(hwnd, IDC_COMBO1, WM_GETTEXT, 80, (LPARAM)buf1) > 0) {
							if (AddBreakpoint(buf1)) {
								SendDlgItemMessage(hwnd, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)buf1);
								ReloadDumpFiles();
							}
						}
					}
					break;

				case IDC_REMOVEBREAK:
					{
						int n;
						while (CB_ERR != (n=SendDlgItemMessage(hwnd, IDC_COMBO1, CB_GETCURSEL, 0, 0))) {
							SendDlgItemMessage(hwnd, IDC_COMBO1, CB_DELETESTRING, n, 0);
						}
						// rebuild the breakpoint list
						nBreakPoints=0;
						n=SendDlgItemMessage(hwnd, IDC_COMBO1, CB_GETCOUNT, 0, 0);
						while (n > 0) {
							char buf[32];
							strcpy(buf, "");
							SendDlgItemMessage(hwnd, IDC_COMBO1, CB_GETLBTEXT, n-1, (LPARAM)buf);
							if (strlen(buf)) {
								AddBreakpoint(buf);
							}
							n--;
						}
					}
					break;

				case IDC_BREAKHELP:
					{
						if (NULL == hBrkHlp) {
							// create a modeless dialog to show the breakpoint help
							hBrkHlp=CreateDialog(NULL, MAKEINTRESOURCE(IDD_BRKHLP), hwnd, BreakPointHelpProc);
							ShowWindow(hBrkHlp, SW_SHOW);
						}
					}
					break;

// max lines is DEBUGLINES, each line is 8 bytes
#define LINES_TO_STEP 32
				case IDC_NEXT:
					switch (nMemType) {
						case MEMCPU:		// CPU (can also support a page number to be saved)
							// step forward in memory 
							{
								int x, bank;
								// validate it
								if (2 == sscanf(szTopMemory[nMemType], "%X:%X", &x, &bank)) {
									// it is a number with a bank
									x+=(LINES_TO_STEP*8);
									sprintf(szTopMemory[nMemType], "%04X:%X", x, bank);
									SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
								} else if (1 == sscanf(szTopMemory[nMemType], "%X", &x)) {
									// it is a number, not a register
									x+=(LINES_TO_STEP*8);
									sprintf(szTopMemory[nMemType], "%04X", x);
									SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
								}
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
							}
							break;

						case MEMVDP:		// VDP
						case MEMGROM:		// GROM
                        case MEMAMS:        // AMS
							// step forward in memory 
							{
								int x;
								// validate it
								if (1 == sscanf(szTopMemory[nMemType], "%X", &x)) {
									// it is a number, not a register
									x+=(LINES_TO_STEP*8);

                                    if (nMemType == MEMGROM) {
                                        // TODO: GROM assumes 16 bases
                                        x &= 0xfffff;
                                    } else if (nMemType == MEMAMS) {
                                        // TODO: AMS assumes 32MB
                                        x &= 0x1ffffff;
                                    } else {
									    if (nMemType == MEMVDP) {
										    if (bF18Enabled) {
											    if (x >= 0x4800) {
												    x=0;
											    }
										    } else {
											    if (x >= 0x4000) {
												    x=0;
											    }
										    }
									    }
									    x &= 0xffff;
                                    }

									sprintf(szTopMemory[nMemType], "%04X", x);
									SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
								}
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
							}
							break;
					}
					break;

				case IDC_PREV:
					switch (nMemType) {
						case MEMCPU:		// CPU (can also support a page number to be saved)
							// step back in memory 
							{
								int x, bank;
								// validate it
								if (2 == sscanf(szTopMemory[nMemType], "%X:%x", &x, &bank)) {
									// it is a number with a bank
									x-=(LINES_TO_STEP*8);
									sprintf(szTopMemory[nMemType], "%04X:%X", x, bank);
									SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
								} else if (1 == sscanf(szTopMemory[nMemType], "%X", &x)) {
									// it is a number, not a register
									x-=(LINES_TO_STEP*8);
									sprintf(szTopMemory[nMemType], "%04X", x);
									SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
								}
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
							}
							break;

						case MEMVDP:		// VDP
						case MEMGROM:		// GROM
                        case MEMAMS:        // AMS
							// step back in memory 
							{
								int x;
								// validate it
								if (1 == sscanf(szTopMemory[nMemType], "%X", &x)) {
									// it is a number, not a register
									x-=(LINES_TO_STEP*8);

                                    if (nMemType == MEMGROM) {
                                        // TODO: GROM assumes 16 bases
                                        x &= 0xfffff;
                                    } else if (nMemType == MEMAMS) {
                                        // TODO: AMS assumes 32MB
                                        x &= 0x1ffffff;
                                    } else {
									    if (x < 0) {
										    if (nMemType == MEMVDP) {
											    if (bF18Enabled) {
												    x=0x4800-(LINES_TO_STEP*8);
											    } else {
												    x=0x4000-(LINES_TO_STEP*8);
											    }
										    }
									    }
									    x &= 0xffff;
                                    }

									sprintf(szTopMemory[nMemType], "%04X", x);
									SetDlgItemText(hwnd, IDC_ADDRESS, szTopMemory[nMemType]);
								}
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
							}
							break;
					}
					break;

				case IDC_APPLY:
					{
						// all types available in all windows now.
						char buf[128];
						int x,y;
						int ok,byte,romok;
						romok=0;
						memset(buf, 0, sizeof(buf));
						GetDlgItemText(hwnd, IDC_ADDRESS, buf, 128);
						_strupr(buf);								

						// Special check for PC or WP or AMS
						{
							if (1 == sscanf(buf, "PC=%X", &x)) {
								// make a change to the Program Counter
								pCurrentCPU->SetPC(x);
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
								break;
							}
							if (1 == sscanf(buf, "WP=%X", &x)) {
								// make a change to the Workspace Pointer
								pCurrentCPU->SetWP(x);
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
								break;
							}
							if (1 == sscanf(buf, "ST=%X", &x)) {
								// make a change to the Status register
								pCurrentCPU->SetST(x);
								// refresh the dialog
								SetEvent(hDebugWindowUpdateEvent);
								break;
							}
                            if (0 == _stricmp(buf, "AMS")) {
                                dumpMapperRegisters();
                                break;
                            }
                            if (0 == _stricmp(buf, "help")) {
                                debug_write("==== debugger control ====");
                                debug_write("xxxx                - set base address to view in CPU, AMS, VDP and GROM");
                                debug_write("PC=xxxx             - set Program Counter to xxxx");
                                debug_write("WP=xxxx             - set Workspace Pointer to xxxx");
                                debug_write("ST=xxxx             - set Status register to xxxx");
                                debug_write("Cxxxx=yy[yyyyyy...] - write byte or bytes to CPU memory");
                                debug_write("Vxxxx=yy[yyyyyy...] - write byte or bytes to VDP memory");
                                debug_write("Gxxxx=yy[yyyyyy...] - write byte or bytes to GROM");
                                debug_write("Xxxxxx=yy[yyyyyy...]- write byte or bytes to raw AMS (X=eXtended)");
                                debug_write("CRxx=yy             - set CPU register xx to value yy");
                                debug_write("VRxx=yy             - set VDP register xx to value yy");
                                debug_write("ARxx=yy             - set AMS register xx to value yy");
                                debug_write("DISASM=xxxx,yyyy    - write disasm.txt from addresses xxxx-yyyy");
                                debug_write("AMS                 - dump AMS register summary to debug");
                                debug_write(" ");
                                debug_write("Note register indexes are always in DECIMAL, all other");
                                debug_write("values are assumed to be hex");
                                break;
                            } 
						}

						// special check for disassembly req
						{
							if (2 == sscanf(buf,"DISASM=%X,%X", &x, &y)) {
								if (y<=x) {
									MessageBox(hwnd, "Bad disassembly range", "Classic99 Debugger", MB_OK | MB_ICONSTOP);
								} else {
                                    x&=0xffff;
                                    y&=0xffff;
									FILE *fp = fopen("disasm.txt", "w");
									if (NULL == fp) {
										MessageBox(hwnd, "Failed to open file", "Classic99 Debugger", MB_OK | MB_ICONSTOP);
									} else {
										CString csOut;
										for (int idx = x; idx <= y; ) {
											struct history obj;
                                            int lines = 0;
											obj.bank=0;
											obj.cycles=0;
											obj.pc=idx;
											csOut = "";
											idx+=EmitDebugLine(' ', obj, csOut, lines);
											csOut.Remove('\r');
											fprintf(fp, "%s", (const char*)csOut);
										}
										fclose(fp);
										MessageBox(hwnd, "Disassembly dumped", "Classic99 Debugger", MB_OK);
									}
								}
								break;
							}
						}

						// validate it
						if (strchr(buf, '=')) {
							// this is a write command!
							// remove leading whitespace that might screw up the register check
							while (isspace(buf[0])) {
								memmove(&buf[0], &buf[1], sizeof(buf)-1);
							}
							int nCurMemType = 0;		// nothing until we are told what!
							bool bIsReg = false;		// default to not register
							// first check for C, V or G prefixes
							if (buf[0] == 'C') {
								nCurMemType = MEMCPU;
								memmove(&buf[0], &buf[1], sizeof(buf)-1);	// delete the char
							}
							if (buf[0] == 'V') {
								nCurMemType = MEMVDP;
								memmove(&buf[0], &buf[1], sizeof(buf)-1);	// delete the char
							}
							if (buf[0] == 'G') {
								nCurMemType = MEMGROM;
								memmove(&buf[0], &buf[1], sizeof(buf)-1);	// delete the char
							}
							if (buf[0] == 'X') {
								nCurMemType = MEMAMS;
								memmove(&buf[0], &buf[1], sizeof(buf)-1);	// delete the char
							}
							// now check for a register
							if (buf[0] == 'R') {
								if (nCurMemType == MEMGROM) {
									MessageBox(hwnd, "No registers on this memory type", "Classic99 Debugger", MB_OK | MB_ICONSTOP);
									break;
								}
								if (nCurMemType == 0) {
									MessageBox(hwnd, "You must specify which register type, try 'help'", "Classic99 Debugger", MB_OK | MB_ICONSTOP);
                                    break;
								}
								bIsReg=true;
								memmove(&buf[0], &buf[1], sizeof(buf)-1);	// delete the char
							}

							// if we got nothing, warn the user that the syntax has changed
							if (0 == nCurMemType) {
								MessageBox(hwnd, "All memory writes must be prefixed with a type.\r\nTry 'help' and view debug output.", "Classic99 Debugger", MB_OK | MB_ICONINFORMATION);
								break;
							}

							// now check the rest for valid syntax
							if (bIsReg) {
								// register indexes are decimal, not hex
								if (2 != sscanf(buf, "%d=%X", &x, &y)) {
									// invalid read
									MessageBox(hwnd, "Invalid syntax. Try 'help' and view debug output.", "Unrecognized string", MB_ICONASTERISK);
									break;
								}
							} else {
								if (2 != sscanf(buf, "%X=%X", &x, &y)) {
									// invalid read
									MessageBox(hwnd, "Invalid syntax. Try 'help' and view debug output.", "Unrecognized string", MB_ICONASTERISK);
									break;
								}
							}
							// check if more than a byte - VDP, AMS, GROM and CPU will allow long strings
                            // (which in turn won't use 'y')
							char *p=strchr(buf,'=');
							p++;
							byte=0;
							while (isspace(*p)) p++;
							while (isalnum(*p)) {
								byte++;
								p++;
							}
							if ((byte > 2) && ((nCurMemType == MEMCPU)||(nCurMemType==MEMVDP)||(nCurMemType==MEMGROM)||(nCurMemType==MEMAMS)) ) {
								byte = 0;		// no, multiple bytes (we won't use 'y') (TODO: so why are we masking it?)
								y&=0xffff;
							} else {
								byte = 1;		// yes, byte
								y&=0xff;
							}
							// do various warnings/errors
							ok=IDYES;
							switch (nCurMemType) {
								case MEMCPU:		// CPU
                                    x&=0xffff;
									if ((bIsReg) && (x > 15)) {
										MessageBox(hwnd, "Out of range for CPU registers", "Classic99 Debugger", MB_ICONSTOP);
										ok=IDNO;
									} else if ((!bIsReg) && (ROMMAP[x])) {
										ok=MessageBox(hwnd, "Modify ROM? (Bank 0 only, non-permanent)\nif you answer no, the write will be performed normally.", "Classic99 Debugger", MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
										if (ok == IDYES) {
											romok=1;
										} else {
											romok=0;
										}
										// allow it either way
										ok=IDYES;
									} else {
										ok=IDYES;
									}
									break;
                                case MEMAMS:        // AMS
                                    x&=0x1ffffff;
									if ((bIsReg) && (x > 15)) {
										MessageBox(hwnd, "Out of range for AMS registers", "Classic99 Debugger", MB_ICONSTOP);
										ok=IDNO;
                                    } else {
                                        ok=IDYES;
                                    }
                                    break;
								case MEMVDP:		// VDP
                                    x&=0xffff;
									if ((bIsReg) && (x > 7)) {
										MessageBox(hwnd, "Out of range for VDP registers", "Classic99 Debugger", MB_ICONSTOP);
										ok=IDNO;
									} else if (x > 0x47FF) {
										MessageBox(hwnd, "Out of range for VDP address", "Classic99 Debugger", MB_ICONSTOP);
										ok=IDNO;
									}
									break;
								case MEMGROM:		// GROM
                                    x&=0xfffff;
									ok=MessageBox(hwnd, "Modify GROM? (non-permanent)", "Classic99 Debugger", MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
									break;
								default:
									ok=IDNO;
									break;
							}
							if (ok == IDYES) {
								// CPU writes /are/ allowed to cause side effects!
								switch (nCurMemType) {
									case MEMCPU:		// CPU again
										if (bIsReg) {
											// convert register index to address
											x=pCurrentCPU->GetWP() + (x*2);
											bIsReg = false;		// not anymore
										}
										if (byte) {
											if ((ROMMAP[x]) && (romok)) {
												// need to be direct
												staticCPU[x]=y;
											} else {
												// do this so side effects work
												wcpubyte(x, y);
											}
										} else {
                                            // parse the string and write each byte...
							                char *p=strchr(buf,'=');
							                p++;
							                while (isspace(*p)) p++;
							                while ((*p)&&(*(p+1))&&(isalnum(*p))) {
                                                char tmp[3];
                                                tmp[0]=*p;
                                                tmp[1]=*(p+1);
                                                tmp[2]='\0';
                                                if (0 == sscanf(tmp, "%X", &y)) {
                                                    break;
                                                }
                                                p+=2;

											    if ((ROMMAP[x]) && (romok)) {
												    // need to be direct
												    staticCPU[x]=y;
											    } else {
												    // do this so side effects work
												    wcpubyte(x, y);
											    }

                                                ++x;
                                                if (x > 0xffff) {
                                                    // no wraparound
                                                    break;
                                                }
                                            }
										}
										break;

                                    case MEMAMS:        // AMS again
										if (bIsReg) {
                                            // it's a register - we assume it might be 8-bit or 16 bit, but the function
                                            // only takes in bytes. The third argument is whether this is the MSB of a 16-bit value...
                                            // we always write all 16 bits to avoid confusion
                                            WriteMapperRegisterByte(x, y&0xff, false, true);
                                            WriteMapperRegisterByte(x, (y>>8)&0xff, true, true);
                                            dumpMapperRegisters();
                                            break;
										}
										if (byte) {
        									WriteRawAMS(x, y);
										} else {
                                            // parse the string and write each byte...
							                char *p=strchr(buf,'=');
							                p++;
							                while (isspace(*p)) p++;
							                while ((*p)&&(*(p+1))&&(isalnum(*p))) {
                                                char tmp[3];
                                                tmp[0]=*p;
                                                tmp[1]=*(p+1);
                                                tmp[2]='\0';
                                                if (0 == sscanf(tmp, "%X", &y)) {
                                                    break;
                                                }
                                                p+=2;

    										    WriteRawAMS(x, y);

                                                ++x;
                                                if (x > 0x1ffffff) {
                                                    // no wraparound
                                                    break;
                                                }
                                            }
										}
										break;

									case MEMVDP:		// VDP again
										if (bIsReg) {
											// it's a register
											VDPREG[x] = y;

											if (x==7)
											{	/* color of screen, set color 0 (trans) to match */
												/* todo: does this save time in drawing the screen? it's dumb */
												// TODO: F18A has some modes where color 0 is drawn, not transparent, so recheck how to do this
												int t=y&0xf;
												if (t) {
													F18APalette[0]=F18APalette[t];
												} else {
													F18APalette[0]=0x000000;	// black
												}
											}
										} else {
                                            if (byte) {
    											VDP[x] = y;
                                            } else {
                                                // parse the string and write each byte...
							                    char *p=strchr(buf,'=');
							                    p++;
							                    while (isspace(*p)) p++;
							                    while ((*p)&&(*(p+1))&&(isalnum(*p))) {
                                                    char tmp[3];
                                                    tmp[0]=*p;
                                                    tmp[1]=*(p+1);
                                                    tmp[2]='\0';
                                                    if (0 == sscanf(tmp, "%X", &y)) {
                                                        break;
                                                    }
                                                    p+=2;

                                                    VDP[x] = y;

                                                    ++x;
                                                    if (x>=0x47ff) {
                                                        // no wraparound
                                                        break;
                                                    }
                                                }
                                            }
										}
										// force VDP to update
										redraw_needed=REDRAW_LINES;
										break;

									case MEMGROM:		// GROM 
										// base set by first byte of address
                                        if (byte) {
    										GROMBase[0].GROM[x]=y;
                                        } else {
                                            // parse the string and write each byte...
                                            int base = (x&0xf0000)>>16;
                                            x&=0xffff;
							                char *p=strchr(buf,'=');
							                p++;
							                while (isspace(*p)) p++;
							                while ((*p)&&(*(p+1))&&(isalnum(*p))) {
                                                char tmp[3];
                                                tmp[0]=*p;
                                                tmp[1]=*(p+1);
                                                tmp[2]='\0';
                                                if (0 == sscanf(tmp, "%X", &y)) {
                                                    break;
                                                }
                                                p+=2;

                                                // TODO: bug: this can't write to MPD or UberGROM
                                                GROMBase[0].GROM[x]=y;

                                                ++x;
                                                if (x>=0xffff) {
                                                    // no wraparound
                                                    break;
                                                }
                                            }
                                        }
										break;
								}
							}
						} else {
							if ((1 == sscanf(buf, "%X", &x)) || (tolower(buf[0])=='r')) {
								strcpy(szTopMemory[nMemType], buf);
							}
						}
						// refresh the dialog
						SetEvent(hDebugWindowUpdateEvent);
					}
					break;

				case IDC_EDITMEM:
				;

                case ID_DEBUG_RESETUNINITMEM:
					// should breakpoint before this...
					memset(CPUMemInited, 0, sizeof(CPUMemInited));
					memset(VDPMemInited, 0, sizeof(VDPMemInited));
					break;

                case ID_DEBUG_RESETTIMERSTATISTICS:
                    gResetTimer= true;
                    break;

				case ID_DEBUG_DETECTUNINITMEM:
					if (g_bCheckUninit) {
						g_bCheckUninit = false;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_DETECTUNINITMEM, MF_UNCHECKED);
					} else {
						g_bCheckUninit = true;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_DETECTUNINITMEM, MF_CHECKED);
					}
					break;

				case ID_DEBUG_PAUSE:
					if (0 == max_cpf) {
						// already paused, restore
						DoPlay();
					} else {
						// running normal, so pause
						DoPause();
					}
					break;

				case ID_DEBUG_STEP:
					DoStep();
					break;

				case ID_DEBUG_STEPOVER:
					DoStepOver();
					break;

				case ID_DEBUG_NORMALSPEED:
					DoPlay();
					break;

				case ID_DEBUG_HIGHSPEED:
					DoFastForward();
					break;

				case ID_DEBUG_VDPCHARS:
					if (VDPDebug) {
						VDPDebug = false;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_VDPCHARS, MF_UNCHECKED);
					} else {
						VDPDebug = true;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_VDPCHARS, MF_CHECKED);
					}
					redraw_needed=REDRAW_LINES;
					break;

				case ID_DEBUG_LOADINT:
					DoLoadInterrupt();
					break;

				case ID_DEBUG_DUMPRAM:
					DoMemoryDump();
					break;

				case ID_DEBUG_BREAKONILLEGALOPCODE:
					if (BreakOnIllegal) {
						BreakOnIllegal = false;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_BREAKONILLEGALOPCODE, MF_UNCHECKED);
					} else {
						BreakOnIllegal = true;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_BREAKONILLEGALOPCODE, MF_CHECKED);
					}
					break;

				case ID_DEBUG_BREAKONDISKCORRUPT:
					if (BreakOnDiskCorrupt) {
						BreakOnDiskCorrupt = false;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_BREAKONDISKCORRUPT, MF_UNCHECKED);
					} else {
						BreakOnDiskCorrupt = true;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_BREAKONDISKCORRUPT, MF_CHECKED);
					}
					break;

				case ID_DEBUG_LOGAUDIO:
					if (logAudio) {
						closeAudioLogFiles();
						logAudio = 0;
						CheckMenuItem(GetMenu(hwnd), ID_DEBUG_LOGAUDIO, MF_UNCHECKED);
					} else {
						MessageBox(myWnd, "Audio logging will survive reset, make sure to stop it!", "Warning", MB_OK);
						if (openAudioLogFiles()) {
							logAudio = true;
							CheckMenuItem(GetMenu(hwnd), ID_DEBUG_LOGAUDIO, MF_CHECKED);
						}
					}
					break;

				case ID_VIEW_FREEZE:
					if (bFrozenText) {
						bFrozenText=false;
						CheckMenuItem(GetMenu(hwnd), ID_VIEW_FREEZE, MF_UNCHECKED);
					} else {
						bFrozenText=true;
						CheckMenuItem(GetMenu(hwnd), ID_VIEW_FREEZE, MF_CHECKED);
					}
					break;

				case ID_VIEW_REDRAW:
					bDebugDirty=true;
					SetEvent(hDebugWindowUpdateEvent);
					break;

				case ID_VIEW_CLEAR:
					if (0 == nMemType) {
						// clear debug buffer
						memset(lines, 0, sizeof(lines));
						bDebugDirty=true;
						SetEvent(hDebugWindowUpdateEvent);
					}
					break;

                case ID_VIEW_HEXVIEWADDSCREENOFFSETFORBASIC:
                    if (0 == nDebugHexOffset) {
                        // BASIC and XB use a screen offset of >60 for ASCII
                        nDebugHexOffset = 0x60;
						CheckMenuItem(GetMenu(hwnd), ID_VIEW_HEXVIEWADDSCREENOFFSETFORBASIC, MF_CHECKED);
					} else {
                        nDebugHexOffset = 0;
						CheckMenuItem(GetMenu(hwnd), ID_VIEW_HEXVIEWADDSCREENOFFSETFORBASIC, MF_UNCHECKED);
					}
					break;

                case ID_VIEW_LOGDISASMTODISK:
                    EnterCriticalSection(&csDisasm);
                    if (fpDisasm) {
                        fclose(fpDisasm);
                        fpDisasm = NULL;
                        CheckMenuItem(GetMenu(hwnd), ID_VIEW_LOGDISASMTODISK, MF_UNCHECKED);
                    } else {
                        int ret = MessageBox(hwnd, "This will write all disassembly to 'disasm.txt' until you turn it off,\r\n"
                            "quickly creating a very large file. Do you want to include console ROM (interrupt, etc)?\r\n"
                            "Click Yes to include, No to exclude, or Cancel to abort logging.", "Classic99", MB_YESNOCANCEL);
                        if (ret != IDCANCEL) {
                            if (ret == IDYES) {
                                disasmLogType = 0;  // include ROM
                            } else {
                                disasmLogType = 1;  // exclude > 0x2000
                            }

                            fpDisasm = fopen("disasm.txt", "w");
                            if (NULL == fpDisasm) {
                                MessageBox(hwnd, "Failed to open file!", "Classic99 Error", MB_OK);
                            } else {
                                CheckMenuItem(GetMenu(hwnd), ID_VIEW_LOGDISASMTODISK, MF_CHECKED);
                            }
                        }
                    }
                    LeaveCriticalSection(&csDisasm);
                    break;

                case ID_VIEW_COPYLEFTPANETOCLIPBOARD:
                    SendMessage(ctrl1, WM_USER, 0, 0);
                    break;

                case ID_VIEW_COPYRIGHTPANETOCLIPBOARD:
                    SendMessage(ctrl2, WM_USER, 0, 0);
                    break;
                
                case ID_MAKE_SAVEPROGRAM:
					DoMakeDlg(hwnd);
					PostMessage(hwnd, WM_COMMAND, ID_VIEW_REDRAW, 0);
					break;
            } 
			return TRUE;

		case WM_INITDIALOG:
			mainfont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);

			CheckRadioButton(hwnd, IDC_RADIO1, IDC_RADIO5, IDC_RADIO1);
			nMemType=0;
			SendDlgItemMessage(hwnd, IDC_MAINEDIT, WM_SETTEXT, NULL, (LPARAM)"");
			UpdateMakeMenu(hwnd, 0);
			// must be turned on in this dialog
			g_bCheckUninit = false;
			bFrozenText = false;
			VDPDebug = false;

			SendDlgItemMessage(hwnd, IDC_BREAKCPU, BM_SETCHECK, pCPU->enableDebug?BST_CHECKED:BST_UNCHECKED, 0);
			SendDlgItemMessage(hwnd, IDC_BREAKGPU, BM_SETCHECK, pGPU->enableDebug?BST_CHECKED:BST_UNCHECKED, 0);

			// breakpoints
			SendDlgItemMessage(hwnd, IDC_COMBO1, CB_RESETCONTENT, NULL, NULL);
			for (idx=0; idx<nBreakPoints; idx++) {
				char buf[128];
				strcpy(buf, FormatBreakpoint(idx));
				SendDlgItemMessage(hwnd, IDC_COMBO1, CB_ADDSTRING, NULL, (LPARAM)buf);
			}
			SendDlgItemMessage(hwnd, IDC_COMBO1, CB_LIMITTEXT, 32, NULL);
			// fall through
		case WM_APP:
			// refresh the dialog
			SetEvent(hDebugWindowUpdateEvent);
			return TRUE;

		case WM_CLOSE:
			// must be turned on only during this dialog
			g_bCheckUninit = false;
			bFrozenText = false;
			VDPDebug = false;
			break;
    } 
    return FALSE; 
} 

// helper for DebugUpdateThread - updates csOut
int EmitDebugLine(char cPrefix, struct history obj, CString &csOut, int &lines) {
	char buf1[80], buf2[80];
	CPU9900 *pLclCPU = pCPU;

	// copy to local work variable
	Word PC = obj.pc;

	// tweak the prefix with the bank if not specified
	if (cPrefix == 'b') {
		cPrefix = hexstr[obj.bank&0x0f];
	} else if ((cPrefix == ' ') && (obj.bank == -1)) {
		cPrefix = 'G';
		pLclCPU = pGPU;
	}

	int nSize = Dasm9900(buf2, PC, obj.bank);
	int nRet = nSize;
	if (obj.cycles > 0) {
		sprintf(buf1, "%c  %04X  %04X  %-27s (%d)\r\n", cPrefix, PC, pLclCPU->GetSafeWord(PC, obj.bank), buf2, obj.cycles);
	} else {
		sprintf(buf1, "%c  %04X  %04X  %-27s\r\n", cPrefix, PC, pLclCPU->GetSafeWord(PC, obj.bank), buf2);
	}
	csOut+=buf1;
    ++lines;


	while ((nSize-=2) > 0) {
		PC+=2;
		sprintf(buf1, "         %04X\r\n", pLclCPU->GetSafeWord(PC, obj.bank));
		csOut+=buf1;
        ++lines;
	}

	return nRet;
}

// DEBUG TODOs:
// - ability to spawn multiple debug windows
// - register views for each device (rather than combined as today)
//		-CPU and CRU state (registers and flags)
//		-VDP state (registers, flags, tables, scanline, F18A extended registers)
//		-Audio state (9919, Speech (with FIFO size), Audio Gate bit, SID)
//		-Cartridge: GROM and UberGROM Configuration/status, ROM bank, DSR bank, pCode
//		-SAMS registers
//		-RS232/PIO card (someday)
extern Byte ReadSafeGrom(int nBase, int adr);
void DebugUpdateThread(void*) {
	static int nOldCPC=-1;
	static int nOldGPC=-1;
	static int nOldMemType=-1;
	static char szOldMemory[5][32] = { "", "", "", "", "" };
	static CString csOut;

	char buf1[80], buf2[80];
	int idx, idx2;
	unsigned int tmpPC;
	HWND hWnd;

	while (!quitflag) {
		if (WAIT_TIMEOUT == WaitForSingleObject(hDebugWindowUpdateEvent, 500)) {
			// timeout every so often to recheck quitflag
			continue;
		}

		if (NULL == dbgWnd) {
			continue;
		}

		// Helps a bit -- not much can change unless the PC does ;)
		if ((nOldCPC == pCPU->GetPC()) && (nOldGPC == pGPU->GetPC()) && (nOldMemType == nMemType) && (0 == memcmp(szOldMemory, szTopMemory, sizeof(szOldMemory)))) {
			continue;
		}

		nOldCPC=pCPU->GetPC();
		nOldGPC=pGPU->GetPC();
		nOldMemType=nMemType;

		// Debug window
		hWnd=GetDlgItem(dbgWnd, IDC_MAINEDIT);
		if (NULL != hWnd) {
			csOut="";

			if (!bFrozenText) {
				switch (nMemType) {
					case 0:		// debug
						if (bDebugDirty) {
							for (idx=0; idx<DEBUGLINES; idx++) {
								csOut+=lines[idx];
                                csOut.TrimRight();
								csOut+="\r\n";
							}
							bDebugDirty=false;
						}
						break;

					case 1:		// disassembly
						{
							// work out how many lines we can display
							int nLineCnt=0;
							struct history myHist;

							// show line with bank only for multi-bank cartridges
							// we want to generate a few extra lines to guarantee the cursor is stable
							int precount = DEBUGLINES*2/3;
							for (idx=precount-10; idx<DEBUGLINES; idx++) {
								if ((xb)&&((Disasm[idx].pc & 0xE000) == 0x6000)) {
									EmitDebugLine('b', Disasm[idx], csOut, nLineCnt);
								} else {
									EmitDebugLine(' ', Disasm[idx], csOut, nLineCnt);
								}
							}
							while (nLineCnt > precount) {
								// most likely at least a few
								int nPos = csOut.Find('\n');
								if (nPos == -1) break;
								csOut = csOut.Mid(nPos+1);
								nLineCnt--;
							}

							myHist.pc = pCurrentCPU->GetPC();
							if (pCurrentCPU == pCPU) {
								myHist.bank = xbBank;
							} else {
								// GPU
								myHist.bank = -1;
							}
							myHist.cycles = 0;

							myHist.pc += EmitDebugLine('>', myHist, csOut, nLineCnt);

							for (idx=nLineCnt; idx<DEBUGLINES; idx++) {
								int nTmp = EmitDebugLine(' ', myHist, csOut, nLineCnt);
								myHist.pc += nTmp;
								while (nTmp > 2) {
									idx++;
									nTmp-=2;
								}
							}
						}
						break;

					case MEMCPU:		// CPU Memory
						{
							// CPU must not call the read function, as it may call the memory
							// mapped devices and affect them.
							char buf3[32];
							int c, tmpBank;
							strncpy(buf3, szTopMemory[nMemType], 32);
							buf3[31]='\0';
							if (2 != sscanf(buf3, "%X:%X", &tmpPC, &tmpBank)) {
								tmpBank=xbBank;
							} else {
								tmpBank &= 0x0f;
							}
							if (1 != sscanf(buf3, "%X", &tmpPC)) {
								tmpPC=0;
								if (tolower(buf3[0])=='r') {
									Word WP = pCurrentCPU->GetWP();
									c=atoi(&buf3[1])*2;
									tmpPC=GetSafeCpuByte(WP+c, xbBank)*256 + GetSafeCpuByte(WP+c+1, xbBank);
								}
							}
							tmpPC&=0xffff;
							for (idx2=0; idx2<34; idx2++) {
								sprintf(buf1, "%04X: ", tmpPC);
								strcpy(buf3, "");
								for (idx=0; idx<8; idx++) {
									c=GetSafeCpuByte(tmpPC++, tmpBank);
									tmpPC&=0xffff;
									sprintf(buf2, "%02X ", c);
									strcat(buf1, buf2);
									if ((c>=32)&&(c<=126)) {
										buf2[0]=c;
									} else {
										buf2[0]='.';
									}
									buf2[1]='\0';
									strcat(buf3, buf2);
								}
								strcat(buf1, buf3);
								csOut+=buf1;
								csOut+="\r\n";
							}
						}
						break;

					case MEMVDP:		// VDP Memory
						{
							// VDP and GROM *must not* call the read functions, as it will
							// change the address!! (But, since we don't do bank switching in them...)
                            // VDP memory only has the screen offset added to the ASCII display
							char buf3[32];
							int c;
							strncpy(buf3, szTopMemory[nMemType], 32);
							buf3[31]='\0';
							if (1 != sscanf(buf3, "%X", &tmpPC)) {
								tmpPC=0;
								if (tolower(buf3[0])=='r') {
									Word WP = pCurrentCPU->GetWP();
									c=atoi(&buf3[1])*2;
									tmpPC=GetSafeCpuByte(WP+c, xbBank)*256 + GetSafeCpuByte(WP+c+1, xbBank);
								}
							}
							if (bF18Enabled) {
								// extra 2k of GPU memory
								while (tmpPC > 0x47ff) tmpPC-=0x4800;
							} else {
								while (tmpPC > 0x3fff) tmpPC-=0x4000;
							}
							for (idx2=0; idx2<34; idx2++) {
								sprintf(buf1, "%04X: ", tmpPC);
								strcpy(buf3, "");
								for (idx=0; idx<8; idx++) {
									c=VDP[tmpPC++];
									if (bF18Enabled) {
										// extra 2k of GPU memory
										while (tmpPC > 0x47ff) tmpPC-=0x4800;
									} else {
										while (tmpPC > 0x3fff) tmpPC-=0x4000;
									}
									sprintf(buf2, "%02X ", c);
									strcat(buf1, buf2);
                                    c -= nDebugHexOffset;
									if ((c>=32)&&(c<=126)) {
										buf2[0]=c;
									} else {
										buf2[0]='.';
									}
									buf2[1]='\0';
									strcat(buf3, buf2);
								}
								strcat(buf1, buf3);
								csOut+=buf1;
								csOut+="\r\n";
							}
						}
						break;

					case MEMGROM:		// GROM
							char buf3[32];
							int c;
							strncpy(buf3, szTopMemory[nMemType], 32);
							buf3[31]='\0';
							if (1 != sscanf(buf3, "%X", &tmpPC)) {
								tmpPC=0;
								if (tolower(buf3[0])=='r') {
									Word WP = pCurrentCPU->GetWP();
									c=atoi(&buf3[1])*2;
									tmpPC=GetSafeCpuByte(WP+c, xbBank)*256 + GetSafeCpuByte(WP+c+1, xbBank);
								}
							}
                            {
                                int base = (tmpPC&0xf0000)>>16;
                                tmpPC&=0xffff;
							    for (idx2=0; idx2<34; idx2++) {
								    sprintf(buf1, "%X%04X: ", base, tmpPC);
								    strcpy(buf3, "");
								    for (idx=0; idx<8; idx++) {
                                        c=ReadSafeGrom(base, tmpPC++);
									    tmpPC&=0xffff;
									    sprintf(buf2, "%02X ", c);
									    strcat(buf1, buf2);
									    if ((c>=32)&&(c<=126)) {
										    buf2[0]=c;
									    } else {
										    buf2[0]='.';
									    }
									    buf2[1]='\0';
									    strcat(buf3, buf2);
								    }
								    strcat(buf1, buf3);
								    csOut+=buf1;
								    csOut+="\r\n";
							    }
                            }
							break;

                    case MEMAMS:		// AMS Memory
						{
							// CPU must not call the read function, as it may call the memory
							// mapped devices and affect them.
							char buf3[32];
							int c;
							strncpy(buf3, szTopMemory[nMemType], 32);
							buf3[31]='\0';
							if (1 != sscanf(buf3, "%X", &tmpPC)) {
								tmpPC=0;
							}
							tmpPC&=0x1ffffff; // todo: 32MB assumption
							for (idx2=0; idx2<34; idx2++) {
								sprintf(buf1, "%04X: ", tmpPC);
								strcpy(buf3, "");
								for (idx=0; idx<8; idx++) {
									c=ReadRawAMS(tmpPC++);
									tmpPC&=0x1ffffff;
									sprintf(buf2, "%02X ", c);
									strcat(buf1, buf2);
									if ((c>=32)&&(c<=126)) {
										buf2[0]=c;
									} else {
										buf2[0]='.';
									}
									buf2[1]='\0';
									strcat(buf3, buf2);
								}
								strcat(buf1, buf3);
								csOut+=buf1;
								csOut+="\r\n";
							}
						}
						break;
				}
				if (!csOut.IsEmpty()) {
					SendMessage(hWnd, WM_SETTEXT, NULL, (LPARAM)(LPCSTR)csOut);
				}
			}
		}

		hWnd=GetDlgItem(dbgWnd, IDC_SECONDEDIT);
		if (NULL != hWnd) {
			int val;

			csOut="";

			if (!bFrozenText) {
				// prints the register information in a single edit control
				// spacing: <5 label><1 space><4 value><4 spaces><5 label><1 space><4 value>
				Word WP = pCurrentCPU->GetWP();
				for (idx=0; idx<8; idx++) {
					int val=pCurrentCPU->GetSafeWord(WP+idx*2, xbBank);
					int val2=pCurrentCPU->GetSafeWord(WP+(idx+8)*2, xbBank);
                    if (idx == 0) {
    					sprintf(buf1, " R%2d  %04X   R%2d  %04X    %s\r\n", idx, val, idx+8, val2, (pCurrentCPU==pGPU)?"GPU":"CPU");
                    } else {
    					sprintf(buf1, " R%2d  %04X   R%2d  %04X\r\n", idx, val, idx+8, val2);
                    }
					csOut+=buf1;
				}

				csOut+="\r\n";

				// VDP registers and associated registers beside them
				for (idx=0; idx<8; idx++) {
					val=VDPREG[idx];
					sprintf(buf1, "VDP%d  %02X    ", idx, val);
					csOut+=buf1;
					switch (idx) {
						case 0:	sprintf(buf1, " VDP  %04X\r\n", VDPADD); break;
						case 1: sprintf(buf1, " GROM %04X (%04X.%1.1X)\r\n", GROMBase[0].GRMADD, GROMBase[0].LastRead, GROMBase[0].LastBase); break;
						case 2: sprintf(buf1, "VDPST %02X\r\n", VDPS); break;
						case 3: sprintf(buf1, "  PC  %04X\r\n", pCurrentCPU->GetPC()); break;
						case 4: sprintf(buf1, "  WP  %04X\r\n", pCurrentCPU->GetWP()); break;
						case 5: sprintf(buf1, "  ST  %04X\r\n", pCurrentCPU->GetST()); break;
						case 6: sprintf(buf1, " Bank %08X\r\n", ((xb&0xffff)<<16)|(xbBank&0xffff)); break;
						case 7: sprintf(buf1, " DSR  %04X\r\n", nCurrentDSR&0xffff); break;
						default: strcpy(buf1, "\r\n");
					}
					csOut+=buf1;
				}

				csOut+="\r\n";

				// VDP tables
				sprintf(buf1, " SIT  %04X\r\n", SIT);
				csOut+=buf1;
				sprintf(buf1, " SDT  %04X   SAL  %04X\r\n", SDT, SAL);
				csOut+=buf1;
				if (VDPREG[0]&0x02) {
					// bitmap
					sprintf(buf1, " PDT  %04X   Mask %04X\r\n", PDT, PDTsize);
					csOut+=buf1;
					sprintf(buf1, "  CT  %04X   Mask %04X\r\n", CT, CTsize);
				} else {
					sprintf(buf1, " PDT  %04X   Size %04X\r\n", PDT, PDTsize);
					csOut+=buf1;
					sprintf(buf1, "  CT  %04X   Size %04X\r\n", CT, CTsize);
				}
				csOut+=buf1;

				csOut+="\r\n";

				// break down the status registers
				val=VDPS;
				sprintf(buf1, "VDPST: %s %s %s", (val&VDPS_INT)?"INT":"   ", (val&VDPS_5SPR)?"5SP":"   ", (val&VDPS_SCOL)?"COL":"   ");
				csOut+=buf1;
				if (val&VDPS_5SPR) {
					sprintf(buf1, "5thSP: %X\r\n", VDPS&(~(VDPS_INT|VDPS_5SPR|VDPS_SCOL)));
				} else {
					strcpy(buf1, "\r\n");
				}
				csOut+=buf1;

				val=pCurrentCPU->GetST();
				sprintf(buf1, "  ST : %s %s %s %s %s %s %s\r\n", (val&BIT_LGT)?"LGT":"   ", (val&BIT_AGT)?"AGT":"   ", (val&BIT_EQ)?"EQ":"  ",
					(val&BIT_C)?"C":" ", (val&BIT_OV)?"OV":"  ", (val&BIT_OP)?"OP":"  ", (val&BIT_XOP)?"XOP":" ");
				csOut+=buf1;
				sprintf(buf1, " MASK: %X  STRead: %d (%d%%)  \r\n", val&INTMASK, statusReadLine, statusReadCount*100/262);
				csOut+=buf1;

				csOut+="\r\n";

                // CRU
                sprintf(buf1, " 9901 %04X %04X %04X %c %c %c %c\r\n",
                        timer9901, timer9901Read, starttimer9901,
                        CRU[0]?'1':'0', CRU[1]?'1':'0', 
                        CRU[2]?'1':'0', CRU[3]?'1':'0' );
                csOut += buf1;

				// Sound chip
				sprintf(buf1, " 9919 %03X %03X %03X %X\r\n", nRegister[0], nRegister[1], nRegister[2], nRegister[3]);
				csOut+=buf1;

				sprintf(buf1, " VOL   %X   %X   %X  %X\r\n", nVolume[0], nVolume[1], nVolume[2], nVolume[3]);
				csOut+=buf1;

				csOut+="\r\n";

				// AMS - 16 registers up to 16-bits each (normally 8 though)
				sprintf(buf1, "AMS\r\n %04X %04X %04X %04X %04X %04X %04X %04X\r\n",
					mapperRegisters[0],mapperRegisters[1],mapperRegisters[2],mapperRegisters[3],
					mapperRegisters[4],mapperRegisters[5],mapperRegisters[6],mapperRegisters[7]);
				csOut +=buf1;
				sprintf(buf1, " %04X %04X %04X %04X %04X %04X %04X %04X\r\n",
					mapperRegisters[8],mapperRegisters[9],mapperRegisters[10],mapperRegisters[11],
					mapperRegisters[12],mapperRegisters[13],mapperRegisters[14],mapperRegisters[15]);
				csOut +=buf1;

				// SID - 29 8-bit registers
				sprintf(buf1, "\r\nSID:%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
					SidCache[0], SidCache[1], SidCache[2], SidCache[3], SidCache[4], 
					SidCache[5], SidCache[6], SidCache[7], SidCache[8], SidCache[9],
					SidCache[10], SidCache[11], SidCache[12]);
				csOut +=buf1;
				sprintf(buf1, " %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
					SidCache[13], SidCache[14], SidCache[15], SidCache[16], 
					SidCache[17], SidCache[18], SidCache[19], SidCache[20],
					SidCache[21], SidCache[22], SidCache[23], SidCache[24], 
					SidCache[25], SidCache[26], SidCache[27], SidCache[28]);
				csOut +=buf1;

				SendMessage(hWnd, WM_SETTEXT, NULL, (LPARAM)(LPCSTR)csOut);
			}
		}
	}
}

// Some stuff for the disk configuration dialog
void ConfigureDisk(HWND hwnd, int nDiskNum) {
    if ((csCf7Bios.GetLength() > 0) && (nDiskNum > 0) && (nDiskNum < 4)) {
        MessageBox(hwnd, "You can not configure DSK1-3 while CF7 emulation is active (CF7BIOS in Classic99.ini)", "Classic99", MB_OK);
        return;
    }

    if (!VerifyOpenFiles(hwnd)) {
        return;
    }

    // this is a bit harsh, but we keep the disk system locked and make this dialog modal
	EnterCriticalSection(&csDriveType);

    // Create a dialog to reconfigure disk settings - note we hold the lock through this whole thing!
	g_DiskCfgNum = nDiskNum;						// a bit hard to pass data to a modal dialog!
	DialogBox(NULL, MAKEINTRESOURCE(IDD_DISKCFG), hwnd, DiskBoxProc);

	LeaveCriticalSection(&csDriveType);
}

void DisableAllDiskOptions(HWND hwnd) {
	EnableDlgItem(hwnd, IDC_FIAD_WRITETIFILES, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEV9T9, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEDV80ASTEXT, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEALLDVASTEXT, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEDF80ASTEXT, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEALLDFASTEXT, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_READTIFILES, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_READV9T9, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_READTEXTASDF, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_READTEXTASDV, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_ALLOWDELETE, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_READTEXTWITHOUTEXT, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_ALLOWNOHEADERASDF128, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_ENABLELONGFILENAMES, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_ALLOWMORE127FILES, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_RETURNSUBDIRS, FALSE);
	EnableDlgItem(hwnd, IDC_FIAD_CASESENSITIVE, FALSE);

	EnableDlgItem(hwnd, IDC_IMAGE_USEV9T9DSSD, FALSE);

	EnableDlgItem(hwnd, IDC_DISK_AUTOMAPDSK1, FALSE);
	EnableDlgItem(hwnd, IDC_DISK_WRITEPROTECT, FALSE);
}

void EnableDiskGlobalOptions(HWND hwnd, BaseDisk *pDisk) {
	EnableDlgItem(hwnd, IDC_DISK_AUTOMAPDSK1, TRUE);
	EnableDlgItem(hwnd, IDC_DISK_WRITEPROTECT, TRUE);
	
	if (NULL != pDisk) {
		// then set options based on it
		int nValue;

		nValue = 0;
		pDisk->GetOption(OPT_DISK_AUTOMAPDSK1, nValue);
		SendDlgItemMessage(hwnd, IDC_DISK_AUTOMAPDSK1, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0;
		pDisk->GetOption(OPT_DISK_WRITEPROTECT, nValue);
		SendDlgItemMessage(hwnd, IDC_DISK_WRITEPROTECT, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);
	}
}

void EnableDiskFiadOptions(HWND hwnd, BaseDisk *pDisk) {
	EnableDlgItem(hwnd, IDC_FIAD_WRITETIFILES, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEV9T9, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEDV80ASTEXT, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEALLDVASTEXT, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEDF80ASTEXT, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_WRITEALLDFASTEXT, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_READTIFILES, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_READV9T9, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_READTEXTASDF, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_READTEXTASDV, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_ALLOWDELETE, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_READTEXTWITHOUTEXT, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_ALLOWNOHEADERASDF128, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_ENABLELONGFILENAMES, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_ALLOWMORE127FILES, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_RETURNSUBDIRS, TRUE);
	EnableDlgItem(hwnd, IDC_FIAD_CASESENSITIVE, TRUE);

	if (NULL != pDisk) {
		// then set options based on it
		int nValue;
		
		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_WRITEV9T9, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_WRITETIFILES, BM_SETCHECK, nValue?BST_UNCHECKED:BST_CHECKED, 0);
		SendDlgItemMessage(hwnd, IDC_FIAD_WRITEV9T9, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_WRITEDV80ASTEXT, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_WRITEDV80ASTEXT, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_WRITEALLDVASTEXT, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_WRITEALLDVASTEXT, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_WRITEDF80ASTEXT, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_WRITEDF80ASTEXT, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_WRITEALLDFASTEXT, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_WRITEALLDFASTEXT, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_READTIFILES, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_READTIFILES, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_READV9T9, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_READV9T9, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_READTXTASDV, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_READTEXTASDV, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);
	
		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_ALLOWDELETE, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_ALLOWDELETE, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

        nValue = 0; 
		pDisk->GetOption(OPT_FIAD_READTXTWITHOUTEXT, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_READTEXTWITHOUTEXT, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);
	
		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_ALLOWNOHEADERASDF128, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_ALLOWNOHEADERASDF128, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_ENABLELONGFILENAMES, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_ENABLELONGFILENAMES, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

		nValue = 0; 
		pDisk->GetOption(OPT_FIAD_ALLOWMORE127FILES, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_ALLOWMORE127FILES, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

    	nValue = 0; 
		pDisk->GetOption(OPT_FIAD_SWAPSLASHES, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_SWAPSLASHES, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

    	nValue = 0; 
		pDisk->GetOption(OPT_FIAD_RETURNSUBDIRS, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_RETURNSUBDIRS, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);

        nValue = 0; 
		pDisk->GetOption(OPT_FIAD_CASESENSITIVE, nValue);
		SendDlgItemMessage(hwnd, IDC_FIAD_CASESENSITIVE, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);
    }

	EnableDiskGlobalOptions(hwnd, pDisk);
}

void EnableDiskImageOptions(HWND hwnd, BaseDisk *pDisk) {
	EnableDlgItem(hwnd, IDC_IMAGE_USEV9T9DSSD, TRUE);

	if (NULL != pDisk) {
		// then set options based on it
		int nValue;
		
		nValue = 0; 
		pDisk->GetOption(OPT_IMAGE_USEV9T9DSSD, nValue);
		SendDlgItemMessage(hwnd, IDC_IMAGE_USEV9T9DSSD, BM_SETCHECK, nValue?BST_CHECKED:BST_UNCHECKED, 0);
	}

	EnableDiskGlobalOptions(hwnd, pDisk);
}

void GetDiskGlobalOptions(HWND hwnd, BaseDisk *pDisk) {
	if (NULL != pDisk) {
		pDisk->SetOption(OPT_DISK_AUTOMAPDSK1, SendDlgItemMessage(hwnd, IDC_DISK_AUTOMAPDSK1, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_DISK_WRITEPROTECT, SendDlgItemMessage(hwnd, IDC_DISK_WRITEPROTECT, BM_GETCHECK, 0, 0)==BST_CHECKED);
	}
}

void GetDiskFiadOptions(HWND hwnd, BaseDisk *pDisk) {
	if (NULL != pDisk) {
		// Don't need to also check the TIFILES checkbox, it's only there for the user experience
		pDisk->SetOption(OPT_FIAD_WRITEV9T9,		SendDlgItemMessage(hwnd, IDC_FIAD_WRITEV9T9, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_WRITEDV80ASTEXT,	SendDlgItemMessage(hwnd, IDC_FIAD_WRITEDV80ASTEXT, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_WRITEALLDVASTEXT, SendDlgItemMessage(hwnd, IDC_FIAD_WRITEALLDVASTEXT, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_WRITEDF80ASTEXT,	SendDlgItemMessage(hwnd, IDC_FIAD_WRITEDF80ASTEXT, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_WRITEALLDFASTEXT, SendDlgItemMessage(hwnd, IDC_FIAD_WRITEALLDFASTEXT, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_READTIFILES,		SendDlgItemMessage(hwnd, IDC_FIAD_READTIFILES, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_READV9T9,			SendDlgItemMessage(hwnd, IDC_FIAD_READV9T9, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_READTXTASDV,		SendDlgItemMessage(hwnd, IDC_FIAD_READTEXTASDV, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_ALLOWDELETE,		SendDlgItemMessage(hwnd, IDC_FIAD_ALLOWDELETE, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_READTXTWITHOUTEXT, SendDlgItemMessage(hwnd, IDC_FIAD_READTEXTWITHOUTEXT, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_ALLOWNOHEADERASDF128, SendDlgItemMessage(hwnd, IDC_FIAD_ALLOWNOHEADERASDF128, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_ENABLELONGFILENAMES, SendDlgItemMessage(hwnd, IDC_FIAD_ENABLELONGFILENAMES, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_ALLOWMORE127FILES, SendDlgItemMessage(hwnd, IDC_FIAD_ALLOWMORE127FILES, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_SWAPSLASHES,      SendDlgItemMessage(hwnd, IDC_FIAD_SWAPSLASHES, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_RETURNSUBDIRS,    SendDlgItemMessage(hwnd, IDC_FIAD_RETURNSUBDIRS, BM_GETCHECK, 0, 0)==BST_CHECKED);
		pDisk->SetOption(OPT_FIAD_CASESENSITIVE,    SendDlgItemMessage(hwnd, IDC_FIAD_CASESENSITIVE, BM_GETCHECK, 0, 0)==BST_CHECKED);
    }
	GetDiskGlobalOptions(hwnd, pDisk);
}

void GetDiskImageOptions(HWND hwnd, BaseDisk *pDisk) {
	if (NULL != pDisk) {
		// then get options into it
		pDisk->SetOption(OPT_IMAGE_USEV9T9DSSD, SendDlgItemMessage(hwnd, IDC_IMAGE_USEV9T9DSSD, BM_GETCHECK, 0, 0)==BST_CHECKED);
	}
	GetDiskGlobalOptions(hwnd, pDisk);
}

// alternate on two checkboxes
void FakeRadioButton(HWND hwnd, int nCtrlClicked, int nCtrlAffected) {
	if (BST_CHECKED == SendDlgItemMessage(hwnd, nCtrlClicked, BM_GETCHECK, 0, 0)) {
		SendDlgItemMessage(hwnd, nCtrlAffected, BM_SETCHECK, BST_UNCHECKED, 0);
	} else {
		SendDlgItemMessage(hwnd, nCtrlAffected, BM_SETCHECK, BST_CHECKED, 0);
	}
}

// everything in this dialog locks the disk system, so protected by the disk critical section
INT_PTR CALLBACK DiskBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int nIndex = 0;
	static bool bFiadSet = false;
	static bool bImageSet = false;
	char buf[MAX_PATH];

	// nothing fancy here
    switch (uMsg) 
    { 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
					{
						HMENU hMenu = GetMenu(myWnd);
						if (NULL != hMenu) {
							hMenu = GetSubMenu(hMenu, 4);	// disk menu
							if (hMenu != NULL) {
								hMenu = GetSubMenu(hMenu, nIndex);	// disk item
							}
						}

						int nType = SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_GETCURSEL, 0, 0);
						if (nType > 0) {
							buf[0]='\0';
							SendDlgItemMessage(hwnd, IDC_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)buf);
							buf[MAX_PATH-1]='\0';
							if (buf[0] == '\"') {
								// remove quotes (assume ending quote)
								if (strlen(buf+1)>1) {
									char *p=buf+strlen(buf+1)-1;
									memmove(buf, buf+1, p-buf+1);
									*p='\0';
								}
							}
							if (strlen(buf)<1) {
								MessageBox(hwnd, "You must enter a path for the disk to use.", "Need more Input", MB_OK | MB_ICONSTOP);
								break;
							} else if (strlen(buf) > 245) {
								MessageBox(hwnd, "Path to disk is too long - please move to a shorter path.", "Path too long", MB_OK | MB_ICONERROR);
								break;
							}
							if (hMenu != NULL) {
								ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_DISK_DSK0_SETDSK0+nIndex, buf);
							}
						} else {
							if (hMenu != NULL) {
								sprintf(buf, "Set DSK%d", nIndex);
								ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_DISK_DSK0_SETDSK0+nIndex, buf);
							}
						}
						switch (nType) {
							default:
								if (NULL != pDriveType[nIndex]) {
									// all files should already be closed - this is safe?
									delete pDriveType[nIndex];
									pDriveType[nIndex] = NULL;
								}
								break;

							case DISK_FIAD:
                                if (buf[strlen(buf)-1] != '\\') {
                                    strcat(buf, "\\");
                                }
								if ((NULL == pDriveType[nIndex]) || (DISK_FIAD != pDriveType[nIndex]->GetDiskType())) {
									if (NULL != pDriveType[nIndex]) {
										delete pDriveType[nIndex];
									}
									pDriveType[nIndex] = new FiadDisk();
								}
								GetDiskFiadOptions(hwnd, pDriveType[nIndex]);
								pDriveType[nIndex]->SetPath(buf);
                                addMRU(csLastDiskPath, buf);
								break;

							case DISK_SECTOR:
								if ((NULL == pDriveType[nIndex]) || (DISK_SECTOR != pDriveType[nIndex]->GetDiskType())) {
									if (NULL != pDriveType[nIndex]) {
										delete pDriveType[nIndex];
									}
									pDriveType[nIndex] = new ImageDisk();
								}
								GetDiskImageOptions(hwnd, pDriveType[nIndex]);
								pDriveType[nIndex]->SetPath(buf);
                                addMRU(csLastDiskImage, buf);
								break;

							case DISK_TICC:
								if ((NULL == pDriveType[nIndex]) || (DISK_TICC != pDriveType[nIndex]->GetDiskType())) {
                                    // if this is the first one, we need to warn the user
                                    int i;
                                    for (i=1; i<4; ++i) {
                                        if ((pDriveType[i] != NULL) && (pDriveType[i]->GetDiskType() == DISK_TICC)) break;
                                    }
                                    if (i < 4) {
                                        MessageBox(hwnd, "You may need to reset the TI for the controller card to work! You may lose data if you do not. (Alt-Equals or File->Reset)", "Classic99", MB_OK | MB_ICONWARNING);
                                    }
                                    // now continue to set it up                                    
                                    if (NULL != pDriveType[nIndex]) {
										delete pDriveType[nIndex];
									}
									pDriveType[nIndex] = new TICCDisk();
								}
								GetDiskImageOptions(hwnd, pDriveType[nIndex]);	// still an image type
								pDriveType[nIndex]->SetPath(buf);
                                addMRU(csLastDiskImage, buf);
								break;
						}
					}
					// fall through and close the dialog
                 case IDCANCEL: 
                    EndDialog(hwnd, wParam); 
                    return TRUE; 

				 case IDC_LSTTYPE:
					 if (HIWORD(wParam) == CBN_SELCHANGE) {
						 DisableAllDiskOptions(hwnd);
                         SendDlgItemMessage(hwnd, IDC_PATH, CB_RESETCONTENT, 0, 0);

						 switch (SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_GETCURSEL, 0, 0)) {
							case 0:
							default:
								EnableDlgItem(hwnd, IDC_PATH, FALSE);
								break;

							case DISK_FIAD:
								EnableDlgItem(hwnd, IDC_PATH, TRUE);
								if ((NULL != pDriveType[nIndex]) && (pDriveType[nIndex]->GetDiskType() == DISK_FIAD)) {
									SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)pDriveType[nIndex]->GetPath());
								} else {
									SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)"");
								}
								if (bFiadSet) {
									EnableDiskFiadOptions(hwnd, NULL);
								} else {
									FiadDisk tmpFiad;
									EnableDiskFiadOptions(hwnd, &tmpFiad);
									bFiadSet = true;
								}
                                for (int idx=0; idx<MAX_MRU; ++idx) {
                                    if (csLastDiskPath[idx].GetLength() == 0) break;
                                    SendDlgItemMessage(hwnd, IDC_PATH, CB_ADDSTRING, 0, (LPARAM)csLastDiskPath[idx].GetString());
                                }
								break;

							case DISK_SECTOR:
								EnableDlgItem(hwnd, IDC_PATH, TRUE);
								if ((NULL != pDriveType[nIndex]) && (pDriveType[nIndex]->GetDiskType() == DISK_SECTOR)) {
									SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)pDriveType[nIndex]->GetPath());
								} else {
									SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)"");
								}
								if (bImageSet) {
									EnableDiskImageOptions(hwnd, NULL);
								} else {
									ImageDisk tmpImage;
									EnableDiskImageOptions(hwnd, &tmpImage);
									bImageSet = true;
								}
                                for (int idx=0; idx<MAX_MRU; ++idx) {
                                    if (csLastDiskImage[idx].GetLength() == 0) break;
                                    SendDlgItemMessage(hwnd, IDC_PATH, CB_ADDSTRING, 0, (LPARAM)csLastDiskImage[idx].GetString());
                                }
								break;

							case DISK_TICC:
                                if ((nIndex < 1) || (nIndex > 3)) {
                                    MessageBox(hwnd, "TI Disk controller only works on DSK1 through DSK3", "Classic99 Error", MB_OK);
                                    SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_SETCURSEL, (WPARAM)DISK_SECTOR, 0);
                                } else {
                                    debug_write("WARNING: TI Controller is limited to 180k images and DSK1 through DSK3)");
								    EnableDlgItem(hwnd, IDC_PATH, TRUE);
								    if ((NULL != pDriveType[nIndex]) && (pDriveType[nIndex]->GetDiskType() == DISK_TICC)) {
									    SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)pDriveType[nIndex]->GetPath());
								    } else {
									    SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)"");
								    }
								    if (bImageSet) {
									    EnableDiskImageOptions(hwnd, NULL);
								    } else {
									    TICCDisk tmpImage;
									    EnableDiskImageOptions(hwnd, &tmpImage);
									    bImageSet = true;
								    }
                                }
                                for (int idx=0; idx<MAX_MRU; ++idx) {
                                    if (csLastDiskImage[idx].GetLength() == 0) break;
                                    SendDlgItemMessage(hwnd, IDC_PATH, CB_ADDSTRING, 0, (LPARAM)csLastDiskImage[idx].GetString());
                                }
								break;
						 }
					 }
					 break;

					// certain checkboxes really should be radio buttons, they are exclusive
					case IDC_FIAD_WRITETIFILES:
						if (HIWORD(wParam) == BN_CLICKED) {
							FakeRadioButton(hwnd, IDC_FIAD_WRITETIFILES, IDC_FIAD_WRITEV9T9);
						}
						break;

					case IDC_FIAD_WRITEV9T9:
						if (HIWORD(wParam) == BN_CLICKED) {
							FakeRadioButton(hwnd, IDC_FIAD_WRITEV9T9, IDC_FIAD_WRITETIFILES);
						}
						break;

					case IDC_FIAD_READTEXTWITHOUTEXT:
						if (HIWORD(wParam) == BN_CLICKED) {
							FakeRadioButton(hwnd, IDC_FIAD_READTEXTWITHOUTEXT, IDC_FIAD_ALLOWNOHEADERASDF128);
						}
						break;

					case IDC_FIAD_ALLOWNOHEADERASDF128:
						if (HIWORD(wParam) == BN_CLICKED) {
							FakeRadioButton(hwnd, IDC_FIAD_ALLOWNOHEADERASDF128, IDC_FIAD_READTEXTWITHOUTEXT);
						}
						break;

					case IDC_BROWSE:
						// bring up a file dialog
						 {
							OPENFILENAME ofn;
							char buf[256];
							int nType = SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_GETCURSEL, 0, 0);
							if (nType < 1) {
								break;
							}

							memset(&ofn, 0, sizeof(OPENFILENAME));
							SendDlgItemMessage(hwnd, IDC_PATH, WM_GETTEXT, 256, (LPARAM)buf);

							char szTmpDir[MAX_PATH];
							GetCurrentDirectory(MAX_PATH, szTmpDir);

							ofn.lStructSize=sizeof(OPENFILENAME);
							ofn.hwndOwner=hwnd;
							ofn.lpstrFile=buf;
							ofn.nMaxFile=256;

							if (nType==DISK_FIAD) {
								ofn.lpstrFilter="Pick any file in the desired folder\0*.*\0\0";
								strcpy(buf, "");
							} else {
								ofn.lpstrFilter="Sector-Based Disk Image\0*.DSK;*.TIDISK\0\0";
							}
							ofn.Flags=OFN_HIDEREADONLY;

							if (GetOpenFileName(&ofn)) {
								SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)ofn.lpstrFile);
								if ((nType == DISK_SECTOR)||(nType == DISK_TICC)) {
									SetCurrentDirectory(szTmpDir);
									FILE *fp = fopen(ofn.lpstrFile, "rb");
									if (NULL != fp) {
										fclose(fp);
									} else if (errno == ENOENT) {
										// we'll pick up other errors later
										if (IDYES == MessageBox(hwnd, "Disk image does not exist - would you like to create a blank disk?", "Create Blank Disk?", MB_YESNO | MB_ICONQUESTION)) {
											// we'll just always create 180k DSSD disks for now, make this more flexible later
											FILE *fp = fopen(ofn.lpstrFile, "wb");
											if (NULL == fp) {
												MessageBox(hwnd, "Unable to write to disk file, aborting.", "Error", MB_OK | MB_ICONERROR);
											} else {
												unsigned char sector[256];
												unsigned char init0[] = { 0x02, 0xd0, 0x09, 'D', 'S', 'K', ' ', 0x28, 0x02, 0x01 };
												// sector 0
												memset(sector, 0xff, 256);
												// disk name
												memset(sector, 0x20, 10);
												strcpy((char*)sector, &ofn.lpstrFile[ofn.nFileOffset]);	// if it's longer we'll overwrite it anyway
												// parameters
												memcpy(&sector[0x0a], init0, sizeof(init0));
												// bitmap
												sector[0x14]=0x03;
												memset(&sector[0x15], 0, 0xec-0x15);
												fwrite(sector, 256, 1, fp);
												// sector 1 (and then the rest of the disk)
												memset(sector, 0, 256);
												for (int i=1; i<720; i++) {
													fwrite(sector, 256, 1, fp);
												}
												fclose(fp);
											}
										} else {
											// force the user to select again
											PostMessage(hwnd, WM_COMMAND, wParam, lParam);
										}
									}
								} else {
									// FIAD then - strip the filename (it's in my buffer, so I can change it)
									ofn.lpstrFile[ofn.nFileOffset] = '\0';
									SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)ofn.lpstrFile);
								}
							}

							SetCurrentDirectory(szTmpDir);
						 }
						 break;
            } 
			break;

		case WM_INITDIALOG:
			SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_ADDSTRING, 0, (LPARAM)"None");
			SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_ADDSTRING, 0, (LPARAM)"Files (FIAD)");
			SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_ADDSTRING, 0, (LPARAM)"Image (DSK)");
			SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_ADDSTRING, 0, (LPARAM)"TI Controller (DSK)");
            bFiadSet = false;
			bImageSet = false;

			nIndex = g_DiskCfgNum;
			sprintf(buf, "DSK%d", nIndex);
			SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)buf);

			DisableAllDiskOptions(hwnd);

			if (NULL == pDriveType[nIndex]) {
				SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_SETCURSEL, 0, 0);
				EnableDlgItem(hwnd, IDC_PATH, FALSE);
                SendDlgItemMessage(hwnd, IDC_PATH, CB_RESETCONTENT, 0, 0);
			} else {
				EnableDlgItem(hwnd, IDC_PATH, TRUE);
                SendDlgItemMessage(hwnd, IDC_PATH, CB_RESETCONTENT, 0, 0);
				SendDlgItemMessage(hwnd, IDC_PATH, WM_SETTEXT, 0, (LPARAM)pDriveType[nIndex]->GetPath());

				switch (pDriveType[nIndex]->GetDiskType()) {
					default:
						SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_SETCURSEL, DISK_NONE, 0);
						EnableDlgItem(hwnd, IDC_PATH, FALSE);
						break;

					case DISK_FIAD:
						SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_SETCURSEL, DISK_FIAD, 0);
                        for (int idx=0; idx<MAX_MRU; ++idx) {
                            if (csLastDiskPath[idx].GetLength() == 0) break;
                            SendDlgItemMessage(hwnd, IDC_PATH, CB_ADDSTRING, 0, (LPARAM)csLastDiskPath[idx].GetString());
                        }
						EnableDiskFiadOptions(hwnd, pDriveType[nIndex]);
						bFiadSet = true;
						break;

					case DISK_SECTOR:
						SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_SETCURSEL, DISK_SECTOR, 0);
                        for (int idx=0; idx<MAX_MRU; ++idx) {
                            if (csLastDiskImage[idx].GetLength() == 0) break;
                            SendDlgItemMessage(hwnd, IDC_PATH, CB_ADDSTRING, 0, (LPARAM)csLastDiskImage[idx].GetString());
                        }
						EnableDiskImageOptions(hwnd, pDriveType[nIndex]);
						bImageSet = true;
						break;

					case DISK_TICC:
						SendDlgItemMessage(hwnd, IDC_LSTTYPE, CB_SETCURSEL, DISK_TICC, 0);
                        for (int idx=0; idx<MAX_MRU; ++idx) {
                            if (csLastDiskImage[idx].GetLength() == 0) break;
                            SendDlgItemMessage(hwnd, IDC_PATH, CB_ADDSTRING, 0, (LPARAM)csLastDiskImage[idx].GetString());
                        }
						EnableDiskImageOptions(hwnd, pDriveType[nIndex]);
						bImageSet = true;
						break;
				}
			}

			return TRUE;
    } 
    return FALSE; 
} 
