// TODO: Add RPK support. Wavemotion has offered the use of his DS code:
// @Tursi - I know you've got 1001 projects cooking but if you do get to RPK support, 
// you're free to use any part of my codebase. 
// https://github.com/wavemotion-dave/DS994a/tree/main/arm9/source/rpk 
// The two libraries I use are lowzip (for depacking) and yxml  (for XML parsing) - 
// both are really simple and designed to work on virtually any architecture (you 
// provide the file read callback). Both are released under the permissive MIT 
// license for any use imaginable.  My code is, of course, equally free to 
// borrow in part or whole.

// TODO: add to the auto-cart loader a check for a like-named INI file
// If present, treat it like it was part of Classic99.ini and read the cart that way

// TODO: add an I/O error reference to the help menu


//
// (C) 2007-2024 Mike Brent aka Tursi aka HarmlessLion.com
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
//*                                                   *
//* Thanks to many people - see documentation         *
//*****************************************************

// Scratchpad RAM is now at 0x8300
// any patches that want to access it directly (not through ROMWORD or RCPUBYTE)
// must take note of this or they will fail

// CRU device map
// >0000	Console / SID Blaster
// >1000	CF7 / RAMdisk
// >1100	Classic99 DSR / TI DSR
// >1200	TIPI Sim
// >1300	RS232/PIO
// >1400
// >1500    Reserved for second RS232/PIO
// >1600
// >1700
// >1800    Reserved for Thermal Printer
// >1900
// >1A00
// >1B00
// >1C00
// >1D00
// >1E00	AMS
// >1F00	P-Code

#pragma warning (disable: 4113 4761 4101)

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

////////////////////////////////////////////
// Includes
////////////////////////////////////////////
#include <InitGuid.h>
#include <CGuid.h>
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <process.h>
#include <malloc.h>
#include <dsound.h>
#include <time.h>
#include <math.h>
#include <shellapi.h>
#include <atlstr.h>

#include "..\resource.h"
#include "tiemul.h"
#include "cpu9900.h"
#include "..\addons\rs232_pio.h"
#include "..\keyboard\kb.h"
#include "..\keyboard\ti.h"
#include "..\addons\ams.h"
#include "..\addons\screenReader.h"
#include "..\disk\diskclass.h"
#include "..\disk\fiaddisk.h"
#include "..\disk\imagedisk.h"
#include "..\disk\TICCDisk.h"
#include "..\disk\cf7Disk.h"
#include "..\disk\tipiDisk.h"
#include "..\disk\ramdisk.h"
#include "sound.h"
#include "..\debugger\bug99.h"
#include "..\addons\mpd.h"
#include "..\addons\ubergrom.h"
#include "..\debugger\dbghook.h"

extern void rampVolume(LPDIRECTSOUNDBUFFER ds, long newVol);       // to reduce up/down clicks

////////////////////////////////////////////
// Globals
// These don't all NEED to be globals, but I'm only cleaning up the code, 
// not re-writing it all from scratch.
////////////////////////////////////////////

// TODO HACK
#define USE_BIG_ARRAY
#ifdef USE_BIG_ARRAY
unsigned char *BIGARRAY;
unsigned int BIGARRAYADD;
unsigned int BIGARRAYSIZE;
#endif

// Win32 Stuff
HINSTANCE hInstance;						// global program instance
HINSTANCE hPrevInstance;					// prev instance (always null so far)
bool bWindowInitComplete = false;           // just a little sync so we ignore size changes till we're done
extern HANDLE hDebugWindowUpdateEvent;		// from WindowProc.cpp
extern BOOL CALLBACK DebugBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern void DebugUpdateThread(void*);
extern void UpdateMakeMenu(HWND hwnd, int enable);
void SetMenuMode(bool showTitle, bool showMenu);

// User interface
CString csLastDiskImage[MAX_MRU];
CString csLastDiskPath[MAX_MRU];
CString csLastUserCart[MAX_MRU];

// audio
extern int AudioSampleRate;				// in hz
extern unsigned int CalculatedAudioBufferSize;		// round audiosample rate up to a multiple of frame rate
extern CRITICAL_SECTION csAudioBuf;

// speech
#define SPEECHUPDATETIMESPERFRAME 5
INT16 SpeechTmp[SPEECHRATE*2];				// two seconds worth of buffer
int nSpeechTmpPos=0;
CRITICAL_SECTION csSpeechBuf;
double nDACLevel=0.0;						// DAC level percentage (from cassette port) - added into the audio buffer on update
double CRU_TOGGLES = 0.0;
bool enableBackgroundHum = false;
HANDLE hSpeechBufferClearEvent=INVALID_HANDLE_VALUE;		// notification of speech buffer looping

HMODULE hSpeechDll;											// Handle to speech DLL
void (*SpeechInit)(Byte *pROM, int nRomLen, int BufLen,int SampleRate);	// Pointer to SpeechInit function
void (*SpeechStop)(void);									// Pointer to SpeechStop function
Byte (*SpeechRead)(void);									// Pointer to SpeechRead function
bool (*SpeechWrite)(Byte b, bool f);						// Pointer to SpeechWrite function
void (*SpeechProcess)(Byte *pBuf, int nLen);				// Pointer to SpeechProcess function
HANDLE hWakeupEvent=NULL;									// used to sleep the CPU when not busy
volatile signed long cycles_left=0;							// runs the CPU throttle
volatile unsigned long total_cycles=0;						// used for interrupts
unsigned long speech_cycles=0;								// used to sync speech
bool total_cycles_looped=false;
bool bDebugAfterStep=false;									// force debug after step
bool bStepOver=false;										// whether step over is on
int nStepCount=0;											// how many instructions to step before breakpoints work again (usually 1)
int enableDebugOpcodes = 0;									// enable debug opcodes for CPU
bool bScrambleMemory = false;								// whether to set RAM to random values on reset
bool bWarmBoot = false;										// whether to leave memory alone on reset
int HeatMapFadeSpeed = 25;									// how many pixels per access to fade - bigger = more CPU but faster fade
int installedJoysticks = 0xffff;							// bitmask - up to 16 possible joystick devices (TI only uses 2 though)

// Cartridge Pack
HMODULE hCartPackDll;										// Handle to speech DLL
struct CARTS* (*get_app_array)(void);                       // get the applications list
struct CARTS* (*get_game_array)(void);                      // get the games list
int (*get_app_count)(void);                                 // get the applications count
int (*get_game_count)(void);                               // get the games count

// AppMode
int bEnableAppMode = 0;                                     // whether to enable App Mode
int bSkipTitle = 0;                                         // whether to skip the master title page
int nAutoStartCart = 0;                                     // Which cartridge to autostart on the selection screen (or 0 for none)
int bAppLockFullScreen = 0;									// if in App mode, lock to full screen only
int bEnableINIWrite = 1;									// not /just/ app mode, but app mode enables it
char AppName[128];                                          // the title bar name to display instead of Classic99

// debug
struct _break BreakPoints[MAX_BREAKPOINTS];
int cycleCounter[65536];                                    // cycle counting during a trace
bool cycleCountOn=false;
int nBreakPoints=0;
bool gResetTimer=false;                                     // used to reset the debug timer breakpoint
bool BreakOnIllegal = false;
bool BreakOnDiskCorrupt = false;
bool gDisableDebugKeys = false;
bool bIgnoreConsoleBreakpointHits = false;
CRITICAL_SECTION debugCS;
char g_cmdLine[512];
extern bool bWarmBoot;
extern FILE *fpDisasm;          // file pointer for logging disassembly, if active
extern int disasmLogType;       // 0 = all, 1 = exclude < 2000, valid only when fpDisasm is not NULL
extern CRITICAL_SECTION csDisasm; 
int logAudio = 0;				// whether to log audio

// disk
extern bool bCorruptDSKRAM;
int filesTopOfVram = 0x3fff;
CString csCf7Bios = "";                     // not sure if I can include the CF7 BIOS, so not a top level feature yet
CString csCf7Disk = ".\\cf7Disk.img";
int nCf7DiskSize = 128*1024*1024;
CString csRamDisk = "";
int nRamDiskSize = 0;

// Must remain compatible with LARGE_INTEGER - just here
// to make QuadPart unsigned ;)
typedef union {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    unsigned __int64 QuadPart;
} MY_LARGE_INTEGER;

// Memory
Byte CPUMemInited[65536];					// not going to support AMS yet -- might switch to bits, but need to sort out AMS memory usage (16MB vs 1MB?)
Byte VDPMemInited[128*1024];				// track VDP mem
bool g_bCheckUninit = false;				// track reads from uninitialized RAM
bool nvRamUpdated = false;					// if cartridge RAM is written to (also needs an NVRAM type to be saved)

extern Byte staticCPU[];     				// main memory
Byte *CPU2=NULL;				            // Cartridge space bank-switched (ROM >6000 space, 8k blocks, XB, 379, SuperSpace and MBX ROM), sized by xbmask
Byte mbx_ram[1024];							// MBX cartridge RAM (1k)
Byte ROMMAP[65536];							// Write-protect map of CPU space
Byte DumpMap[65536];						// map for data to dump to files
FILE *DumpFile[10];							// byte dump file 0-9
Byte CRU[4096];								// CRU space (todo: could be bits)
Byte SPEECH[65536];							// Speech Synth ROM
Byte DSR[16][16384];						// 16 CRU bases, up to 16k each (ROM >4000 space)
int  nDSRBank[16];							// Is the DSR bank switched?
struct GROMType GROMBase[17];				// support 16 GROM bases (there is room for 256 of them!), plus 1 for PCODE
int  bankedConsoleGROMs = 0;                // if non-zero, console groms bank just like cartridge GROMs do. No real hardware for this today.
int  nSystem=1;								// Which system do we default to?
int  nCartGroup=0;							// Which cart group?
int	 nCart=-1;								// Which cart is loaded (-1=none)
struct DISKS *pMagicDisk=NULL;				// which magic disk is loaded?
bool fKeyEverPressed=false;					// used to suppress warning when changing cartridges
int  nLoadedUserCarts[100]= { 0 };			// for each group
int  nLoadedUserGroups=0;					// how many groups
char UserGroupNames[100][32];				// name of each group
int nTotalUserCarts=0;						// total user carts loaded
int CRUTimerTicks=0;						// used for 9901 timer

unsigned char DummyROM[6]={
	0x83, 0x00,								// >0000	reset vector: workspace
	0x00, 0x04,								// >0002	reset vector: address
	0x10, 0xff								// >0004	JMP @>0004
};

int KEYS[2][8][8]= {  
{
// Keyboard - 99/4 - no PS/2 emulation :)
/* unused */	VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE,

/* 1 */			'N', 'H', 'U', '7', 'C', 'D', 'R', '4',
/* Joy 1 */		VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE,
/* 3 */			VK_OEM_PERIOD, 'K', 'O', '9', 'Z', 'A', 'W', '2',

/* Joy 2 */		VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE,
	
/* 5 */			'M', 'J', 'I', '8', 'X', 'S', 'E', '3',
/* 6 */			'B', 'G', 'Y', '6', 'V', 'F', 'T', '5',
/* 7 */			VK_RETURN, 'L', 'P', '0', VK_SHIFT, VK_SPACE, 'Q', '1'
},
{
// Keyboard - 99/4A
/* Joy 2 */		VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE,

/* 1 */			'M', 'J', 'U', '7', '4', 'F', 'R', 'V',					// MJU7 4FRV
/* 2 */			VK_OEM_2, VK_OEM_1, 'P', '0', '1', 'A', 'Q', 'Z',		// /;P0 1AQZ
/* 3 */			VK_OEM_PERIOD, 'L', 'O', '9', '2', 'S', 'W', 'X',		// .LO9 2SWX

/* Joy 1 */		VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE, VK_ESCAPE,
	
/* 5 */			VK_OEM_COMMA, 'K', 'I', '8', '3', 'D', 'E', 'C',		// ,KI8 3DEC
/* 6 */			'N', 'H', 'Y', '6', '5', 'G', 'T', 'B',					// NHY6 5GTB
/* 7 */			VK_OEM_PLUS, VK_SPACE, VK_RETURN, VK_ESCAPE, VK_MENU, VK_SHIFT, VK_CONTROL, VK_ESCAPE 
}																		// = rx fscx
};

char key[256];										// keyboard state buffer

// Win32 Joysticks
JOYINFOEX myJoy;
int fJoy;
int fJoystickActiveOnKeys;
joyStruct joyStick[2];

// Audio
int latch_byte;										// latched byte
extern int max_volume;								// maximum volume as a percentage
LPDIRECTSOUND lpds;									// DirectSound handle
LPDIRECTSOUNDBUFFER soundbuf;						// sound chip audio buffer
LPDIRECTSOUNDBUFFER sidbuf;							// sid blaster audio buffer
LPDIRECTSOUNDBUFFER speechbuf;						// speech audio buffer
// Used to halt the CPU when writing to the synth too quicky
// This probably belongs in the speech emulation
bool CPUSpeechHalt=false;
Byte CPUSpeechHaltByte=0;
Byte SidCache[29];                                  // cache of written sid registers

// disassembly view
struct history Disasm[DEBUGLINES];					// history object - protect with csDisasm

// video
extern int bEnable80Columns;						// 80 column hack
extern int bEnable128k;								// 128k hack
extern int bF18Enabled;								// F18A support
extern int bInterleaveGPU;							// simultaneous GPU (not really)
extern int vdpscanline;								// used for load stats

// these two variables are used to estimate a screen clear for the speech code -
// if 768 bytes are written within 960 bytes of the start of screen memory, and they
// are all the same byte, the screen is determined to be cleared. This is even okay
// for text mode with 2 lines of fixed header (880 bytes would be written).
// (768 is the size of regular 32x24, and 960 is the size of 40x24)
unsigned char lastVideoByte = 255;
int lastVideoCount = 0;

int statusReadLine=0;								// the line we last read status at
int statusReadCount=0;								// how many lines since we last read status
int statusFrameCount=0;								// entire frames missed since update
bool statusUpdateRead = false;						// frame has finished, watch for status test again

// Assorted
char qw[80];										// temp string
volatile int quitflag;								// quit flag
char lines[DEBUGLINES][DEBUGLEN];					// debug lines
bool bDebugDirty;									// whether debug has changed
volatile int xbBank=0;								// Cartridge bank switch
volatile int bInvertedBanks=false;					// whether switching uses Jon's inverted 379
volatile int bUsesMBX=false;						// whether switching uses MBX style ROM
int xb = 0;											// Is bank-switched cartridge ROM loaded?
int grombanking = 0;								// Did we load multiple GROM bases?
int nCurrentDSR=-1;									// Which DSR Bank are we on?
unsigned int index1;								// General counter variable
int drawspeed=0;									// flag used in display updating
int max_cpf=DEFAULT_60HZ_CPF;						// Maximum cycles per frame (default)
int cfg_cpf=max_cpf;								// copy of same
int cfg_overdrive = 50;								// how much faster CPU overdrive tries for
int slowdown_keyboard = 1;							// slowdown keyboard autorepeat in the GROM code
int cpucount, cpuframes;							// CPU counters for timing
int timercount;										// Used to estimate runtime
int CtrlAltReset = 0;								// if true, require control+alt+equals
int gDontInvertCapsLock = 0;						// if true, caps lock is not inverted
const char *szDefaultWindowText="Classic99";		// used to set Window back to normal after a change

int timer9901;										// 9901 interrupt timer
int timer9901Read;                                  // the read-back register
int starttimer9901;									// and it's set time
int timer9901IntReq;								// And whether it is requesting an interrupt
int keyboard=KEY_994A_PS2;							// keyboard map (0=99/4, 1=99/4A, 2=99/4A PS/2 (see enum in .h))
int ps2keyboardok=1;								// whether to allow PS2 keyboards

int sams_enabled = 1;								// memory type (0 = disabled, 1 SAMS enabled)
int sams_size = 3;									// SAMS emulation memory size (0 = 128k, 1 = 256k, 2 = 512k, 3 = 1024k)

int retrace_count=0;								// count on the 60hz timer

int PauseInactive;									// what to do when the window is inactive
int WindowActive;                                   // true if the Classic99 window is active
int SpeechEnabled;									// whether speech is enabled
volatile int ThrottleMode = THROTTLE_NORMAL;		// overall throttling mode
int Fast16BitRam = 0;								// whether to disable wait states on the 32K memory space
int enableSpeedKeys = 0;							// allow the INI to make F6,F7,F8,F11 available all the time
int enableAltF4 = 0;								// allow alt+F4 to close the emulator
int enableF10Menu = 0;								// allow F10 to activate the menu bar
int enableEscape = 1;								// allow Escape to act as Fctn-9 (back)
bool mouseCaptured = false;							// used for mouse support - just TIPI today

time_t STARTTIME, ENDTIME;
volatile long ticks;

CPU9900 * volatile pCurrentCPU;	// todo: I'm not sure. the contents are volatile too...
CPU9900 *pCPU, *pGPU;

ATOM myClass;										// Window Class
HWND myWnd;											// Handle to windows
HMENU myMenu;                                       // Handle to menu (for full screen)
volatile HWND dbgWnd;								// Handle to windows
HDC myDC;											// Handle to Device Context
int fontX, fontY;									// Non-proportional font x and y size
DWORD g_dwMyStyle = WS_OVERLAPPEDWINDOW | WS_SIZEBOX | WS_VISIBLE;
int nVideoLeft = -1, nVideoTop = -1;
RECT gWindowRect;       // not always up to date!

char AVIFileName[256]="C:\\TI99AVI.AVI";			// AVI Filename

char *PasteString;									// Used for Edit->Paste
char *PasteIndex;
bool PasteStringHackBuffer=false;					// forces long inputs under BASIC/XB (may cause crashes)
int PasteCount;

unsigned long myThread;								// timer thread
CRITICAL_SECTION VideoCS;							// Video CS
CRITICAL_SECTION DebugCS;							// Debug CS
CRITICAL_SECTION TapeCS;							// Tape CS

extern const char *pCurrentHelpMsg;
extern int VDPDebug;
extern int SIT;
extern int TVScanLines;
extern CString TipiURI[3];
extern CString TipiDirSort;
extern CString TipiAuto;
extern CString TipiTz;
extern CString TipiSSID;
extern CString TipiPSK;
extern CString TipiName;

#define INIFILE ".\\classic99.ini"

///////////////////////////////////
// JoyStruct
///////////////////////////////////

joyStruct::joyStruct() { reset(); }
void joyStruct::reset() {
	mode = 0;
	Xaxis = 0;
	Yaxis = 1;
	btnMask = 0xffffffff;
	minXDead = 0x4000;
	maxXDead = 0xC000;
	minYDead = 0x4000;
	maxYDead = 0xC000;
}
void joyStruct::changeMode(int n) {
	if (n != mode) {
		reset();
		mode = n;
	}
}

///////////////////////////////////
// Built-in Cart library
///////////////////////////////////

// ROMs to always load
struct IMG AlwaysLoad[] = {
    {	IDR_AMI99DSK,	0x1100, 0x0200,	TYPE_DSR	, 0, 0},
    {	IDR_TIDISK,		0x1100, 0x2000,	TYPE_DSR2	, 0, 0},	// not paged on the real hardware, but this is how we fake it with all our features :)
    {   IDR_TIPISIM,    0x1200, 0x0110, TYPE_DSR    , 0, 0},
//	{	IDR_RS232,		0x1300, 0x0900, TYPE_DSR	, 0, 0},
    {	IDR_SPCHROM,	0x0000,	0x8000,	TYPE_SPEECH	, 0, 0},
    {	IDR_PGROM,		0x0000, 0xF800, TYPE_PCODEG , 0, 0},
};

// Actual cartridge definitions (broken into categories)
struct CARTS *Users=NULL;		// these are loaded dynamically
struct CARTS *Apps=NULL;
struct CARTS *Games=NULL;

struct CARTS Systems[] = {
	{	
		"TI-99/4",	
		{	
			{	IDR_CON4R0,		0x0000, 0x2000,	TYPE_ROM	, 0 , -1},
			{	IDR_CON4G0,		0x0000, 0x2000,	TYPE_GROM	, 0 , -1},
			{	IDR_CON4G1,		0x2000,	0x2000,	TYPE_GROM	, 0 , -1},
			{	IDR_CON4G2,		0x4000,	0x2000,	TYPE_GROM	, 0 , -1},
		},
		NULL,
		NULL,
		0
	},

	{	
		"TI-99/4A",	
		{	
			{	IDR_994AGROM,	0x0000, 0x6000,	TYPE_GROM	, 0, -1},
			{	IDR_994AROM,	0x0000,	0x2000,	TYPE_ROM	, 0, -1},
		},
		NULL,
		NULL,
		0
	},

	{	
		"TI-99/4A V2.2",
		{
			{	IDR_CON22R0,	0x0000, 0x2000,	TYPE_ROM	, 0, -1},
			{	IDR_CON22G0,	0x0000,	0x2000,	TYPE_GROM	, 0, -1},
			{	IDR_CON22G1,	0x2000,	0x2000,	TYPE_GROM	, 0, -1},
			{	IDR_CON22G2,	0x4000,	0x2000,	TYPE_GROM	, 0, -1},
		},
		NULL,
		NULL,
		0
	},
};

// another hack - hacks in the programming environment for Seahorse boards
//#define USE_GIGAFLASH
#ifdef USE_GIGAFLASH
#include "../addons/gigaflash.cpp"
#endif

// breakpoint helper 
bool CheckRange(int nBreak, int x) {
	// check bank first (assumes ranges are only for addresses, not data)
	if (BreakPoints[nBreak].Bank != -1) {
		if ((x>=0x6000) && (x<=0x7FFF)) {
			if (xbBank != BreakPoints[nBreak].Bank) {
				// bank required and not the right bank
				return false;
			}
		}
	}

	if (BreakPoints[nBreak].B) {
		// this is a range
		if ((x >= BreakPoints[nBreak].A) && (x <= BreakPoints[nBreak].B)) {
			return true;
		}
	} else {
		// not a range
		if (x == BreakPoints[nBreak].A) {
			return true;
		}
	}
	return false;
}

// Configuration access
void ReadConfig() {
	int idx,idx2,idx3;
	bool bFilePresent=true;

	// Check if the file is even present - if it's not we need to fake the disk config
	FILE *fp=fopen(INIFILE, "r");
	if (NULL == fp) {
		// no such file
		debug_write("No configuration file - setting defaults");
		bFilePresent=false;
	} else {
		fclose(fp);
	}

	// Volume percentage
	max_volume =			GetPrivateProfileInt("audio",	"max_volume",	max_volume,					INIFILE);

	// SID blaster
	if (NULL != SetSidEnable) {
		SetSidEnable(		GetPrivateProfileInt("audio",	"sid_blaster",	0,							INIFILE) != 0	);
	}

	// audio rate
	AudioSampleRate =		GetPrivateProfileInt("audio",	"samplerate",	AudioSampleRate,			INIFILE);

	// backgroud hum
	enableBackgroundHum = 	GetPrivateProfileInt("audio",	"backgroundNoise",	enableBackgroundHum?1:0,INIFILE) != 0;

	// continuous screen reader
	ScreenReader::SetContinuousRead(		GetPrivateProfileInt("audio",	"continuousReader",	ScreenReader::GetContinuousRead()?1:0,INIFILE) != 0);

	// load the new style config
	EnterCriticalSection(&csDriveType);

	if (bFilePresent) {
		HMENU hMenu = GetMenu(myWnd);
		if (NULL != hMenu) {
			hMenu = GetSubMenu(hMenu, 4);	// disk menu
		}

		for (int idx=0; idx < MAX_DRIVES-RESERVED_DRIVES; idx++) {
			CString cs;
			
			if (NULL != pDriveType[idx]) {
				delete pDriveType[idx];
				pDriveType[idx]=NULL;
			}

			cs.Format("Disk%d", idx);

			int nType = GetPrivateProfileInt(cs, "Type", DISK_NONE, INIFILE);
			if (nType != DISK_NONE) {
				switch (nType) {
					case DISK_FIAD:
						pDriveType[idx] = new FiadDisk;
						break;

					case DISK_SECTOR:
						pDriveType[idx] = new ImageDisk;
						break;

					case DISK_TICC:
						pDriveType[idx] = new TICCDisk;
						break;
				}

				if (NULL != pDriveType[idx]) {
					// get the path
					char buf[MAX_PATH];
					GetPrivateProfileString(cs, "Path", ".", buf, MAX_PATH, INIFILE);
					pDriveType[idx]->SetPath(buf);

					if (hMenu != NULL) {
						HMENU hSub = GetSubMenu(hMenu, idx);
						if (hSub != NULL) {
							ModifyMenu(hSub, 0, MF_BYPOSITION | MF_STRING, ID_DISK_DSK0_SETDSK0+idx, buf);
						}
					}
					
					// note that all values should use 0 for default, since a 
					// 0 from the config will not be relayed to the class
					for (idx2=0; idx2 < DISK_OPT_MAXIMUM; idx2++) {
						int nTmp = GetPrivateProfileInt(cs, pszOptionNames[idx2], -1, INIFILE);
						if (nTmp != -1) {
							pDriveType[idx]->SetOption(idx2, nTmp);
						}
					}
				}
			}
		}
	} else {
		// There's no configuration file - this sets the default drive layout
		for (int idx=0; idx < MAX_DRIVES-RESERVED_DRIVES; idx++) {
			CString cs;
			HMENU hMenu = GetMenu(myWnd);
			if (NULL != hMenu) {
				hMenu = GetSubMenu(hMenu, 4);	// disk menu
			}
			
			if (NULL != pDriveType[idx]) {
				delete pDriveType[idx];
				pDriveType[idx]=NULL;
			}

			if ((idx>0)&&(idx<4)) {		// 1-3
				CString csTmp;
				// the main defaults for FIADDisk are fine, just set type and path
				pDriveType[idx] = new FiadDisk;
				csTmp.Format(".\\DSK%d\\", idx);
				pDriveType[idx]->SetPath(csTmp);
				if (hMenu != NULL) {
					HMENU hSub = GetSubMenu(hMenu, idx);
					if (hSub != NULL) {
						ModifyMenu(hSub, 0, MF_BYPOSITION | MF_STRING, ID_DISK_DSK0_SETDSK0+idx, csTmp.GetBuffer());
					}
				}
			}
		}
	}
	LeaveCriticalSection(&csDriveType);
	// the menu may have changed!
	DrawMenuBar(myWnd);

    // see if we're going to do anything with the CF7 - if it's set, we'll not use the disk DSR
    {
        char buf[1024];
    	GetPrivateProfileString("CF7", "BIOS", "", buf, sizeof(buf), INIFILE);
        if (buf[0] != '\0') {
            csCf7Bios = buf;
        }
    	GetPrivateProfileString("CF7", "Disk", "", buf, sizeof(buf), INIFILE);
        if (buf[0] != '\0') {
            csCf7Disk = buf;
        }
        nCf7DiskSize = GetPrivateProfileInt("CF7", "Size", nCf7DiskSize, INIFILE);
    }
    // do similar for RAMdisk
    {
        char buf[1024];
        GetPrivateProfileString("Ramdisk", "Disk", "", buf, sizeof(buf), INIFILE);
        if (buf[0] != '\0') {
            csRamDisk = buf;
        }
        nRamDiskSize = GetPrivateProfileInt("Ramdisk", "Size", nRamDiskSize, INIFILE);
    }

	// NOTE: emulation\enableAltF4 is down under the video block, due to needing to set different defaults
	// Filename used to write recorded video
	GetPrivateProfileString("emulation", "AVIFilename", AVIFileName, AVIFileName, 256, INIFILE);
	// Throttle mode is all in one now, from -1: THROTTLE_SLOW, THROTTLE_NORMAL, THROTTLE_OVERDRIVE, THROTTLE_SYSTEMMAXIMUM
	ThrottleMode =  GetPrivateProfileInt("emulation",   "throttlemode",         ThrottleMode,   INIFILE);
	// 16-bit RAM is now supported
	Fast16BitRam =  GetPrivateProfileInt("emulation",   "fast16bitram",         Fast16BitRam,   INIFILE);
	// Proper CPU throttle (cycles per frame) - ipf is deprecated - this defines "normal" and probably should go away too
	max_cpf=		GetPrivateProfileInt("emulation",	"maxcpf",				max_cpf,		INIFILE);
	cfg_cpf = max_cpf;
	// Overdrive CPU multiplier
	cfg_overdrive=	GetPrivateProfileInt("emulation",	"overdrive",			cfg_overdrive,	INIFILE);
	// map through certain function keys as emulator speed control
	enableSpeedKeys = GetPrivateProfileInt("emulation", "enableSpeedKeys",		enableSpeedKeys, INIFILE);
	// map through certain function keys as emulator speed control
	enableEscape = GetPrivateProfileInt("emulation",    "enableEscape",		    enableEscape, INIFILE);
	// F10 can be set to enable the menu
	enableF10Menu = GetPrivateProfileInt("emulation", "enableF10Menu",			enableF10Menu,   INIFILE);
	// Pause emulator when window inactive: 0-no, 1-yes
	PauseInactive=	GetPrivateProfileInt("emulation",	"pauseinactive",		PauseInactive,	INIFILE);
	// Disable speech if desired
	SpeechEnabled=  GetPrivateProfileInt("emulation",   "speechenabled",         SpeechEnabled,  INIFILE);
	// require additional control key to reset (QUIT)
	CtrlAltReset=	GetPrivateProfileInt("emulation",	"ctrlaltreset",			CtrlAltReset,	INIFILE);
	// override the inverted caps lock
	gDontInvertCapsLock = !GetPrivateProfileInt("emulation","invertcaps",		!gDontInvertCapsLock, INIFILE);
	// Get system type: 0-99/4, 1-99/4A, 2-99/4Av2.2
	nSystem=		GetPrivateProfileInt("emulation",	"system",				nSystem,		INIFILE);
	// Read flag for slowing keyboard repeat: 0-no, 1-yes
	slowdown_keyboard=GetPrivateProfileInt("emulation",	"slowdown_keyboard",	slowdown_keyboard, INIFILE);
	// Check whether to use the ps/2 keyboard (normally yes for 99/4A)
	if (nSystem == 0) {
		keyboard=KEY_994;		// 99/4
	} else {
		ps2keyboardok=GetPrivateProfileInt("emulation", "ps2keyboard", 1, INIFILE);
		if (ps2keyboardok) {
			keyboard=KEY_994A_PS2;	// 99/4A with ps/2
		} else {
			keyboard=KEY_994A;		// 99/4A without ps/2
		}
	}
	// SAMS emulation
	sams_enabled = GetPrivateProfileInt("emulation", "sams_enabled", sams_enabled, INIFILE);
	// Read flag for SAMS memory size if selected
	sams_size = GetPrivateProfileInt("emulation", "sams_size", sams_size, INIFILE);

    // whether to use banked console GROMs - special case and no hardware exists today
	bankedConsoleGROMs = GetPrivateProfileInt("emulation", "bankedConsoleGROMs",   bankedConsoleGROMs, INIFILE);

	// Joystick active: 0 - off, 1 on
	fJoy=		GetPrivateProfileInt("joysticks", "active",		fJoy,		INIFILE);
	joyStick[0].mode=	 (unsigned)GetPrivateProfileInt("joysticks", "joy1mode",	joyStick[0].mode,	 INIFILE);
	joyStick[0].Xaxis=   (unsigned)GetPrivateProfileInt("joysticks", "joy1xaxis",	joyStick[0].Xaxis,   INIFILE);
	joyStick[0].Yaxis=   (unsigned)GetPrivateProfileInt("joysticks", "joy1yaxis",	joyStick[0].Yaxis,   INIFILE);
	joyStick[0].btnMask= (unsigned)GetPrivateProfileInt("joysticks", "joy1btns",	joyStick[0].btnMask, INIFILE);
	joyStick[0].minXDead=(unsigned)GetPrivateProfileInt("joysticks", "joy1minX",	joyStick[0].minXDead,INIFILE);
	joyStick[0].minYDead=(unsigned)GetPrivateProfileInt("joysticks", "joy1minY",	joyStick[0].minYDead,INIFILE);
	joyStick[0].maxXDead=(unsigned)GetPrivateProfileInt("joysticks", "joy1maxX",	joyStick[0].maxXDead,INIFILE);
	joyStick[0].maxYDead=(unsigned)GetPrivateProfileInt("joysticks", "joy1maxY",	joyStick[0].maxYDead,INIFILE);

	joyStick[1].mode=	 (unsigned)GetPrivateProfileInt("joysticks", "joy2mode",	joyStick[1].mode,	 INIFILE);
	joyStick[1].Xaxis=   (unsigned)GetPrivateProfileInt("joysticks", "joy2xaxis",	joyStick[1].Xaxis,   INIFILE);
	joyStick[1].Yaxis=   (unsigned)GetPrivateProfileInt("joysticks", "joy2yaxis",	joyStick[1].Yaxis,   INIFILE);
	joyStick[1].btnMask= (unsigned)GetPrivateProfileInt("joysticks", "joy2btns",	joyStick[1].btnMask, INIFILE);
	joyStick[1].minXDead=(unsigned)GetPrivateProfileInt("joysticks", "joy2minX",	joyStick[1].minXDead,INIFILE);
	joyStick[1].minYDead=(unsigned)GetPrivateProfileInt("joysticks", "joy2minY",	joyStick[1].minYDead,INIFILE);
	joyStick[1].maxXDead=(unsigned)GetPrivateProfileInt("joysticks", "joy2maxX",	joyStick[1].maxXDead,INIFILE);
	joyStick[1].maxYDead=(unsigned)GetPrivateProfileInt("joysticks", "joy2maxY",	joyStick[1].maxYDead,INIFILE);

	fJoystickActiveOnKeys = 0;		// just reset this

	// Cartridge group loaded (0-apps, 1-games, 2-user)
	nCartGroup=	GetPrivateProfileInt("roms",	"cartgroup",	nCartGroup,	INIFILE);
	// Cartridge index (depends on group)
	nCart=		GetPrivateProfileInt("roms",	"cartidx",		nCart,		INIFILE);
	// User cartridges
	memset(nLoadedUserCarts, 0, sizeof(nLoadedUserCarts));
	nLoadedUserGroups=0;
	// up to 100 groups, each with up to 100 carts. The last group is always "usercart%d" (and we remove it if it's empty)
	idx2=0;
	for (idx=0; idx<100; idx++) {
		char buf[256], buf2[256];
		sprintf(buf, "Group%d", idx);
		GetPrivateProfileString("CartGroups", buf, "", UserGroupNames[idx2], sizeof(UserGroupNames[idx2]), INIFILE);
		if (strlen(UserGroupNames[idx2]) > 0) idx2++;
	}
	// now sneak in usercart, in case the user had it configured... but only if there is room!
	if (idx2 < 100) {
		strcpy(UserGroupNames[idx2], "UserCart");
		idx2++;
	}
	// save the count
	nLoadedUserGroups=idx2;
	nTotalUserCarts = 1;	// there's always one to start with, and we leave it blank
	// now run through all the groups and scan for carts to load to the menu
	for (int cart=0; cart<nLoadedUserGroups; cart++) {
		idx2=0;

		for (idx=0; idx<100; idx++) {
			char buf[256], buf2[256];

            // TODO: check for memory leaks on failed realloc
			// it's not the most efficient to keep reallocing, but it will be fine in this limited use case
			Users=(CARTS*)realloc(Users, (nTotalUserCarts+1) * sizeof(Users[0]));
            if (NULL == Users) {
                fail("Unable to allocate user cart memory!");
            }
			memset(&Users[nTotalUserCarts], 0, sizeof(CARTS));

			sprintf(buf, "%s%d", UserGroupNames[cart], idx);
			GetPrivateProfileString(buf, "name", "", Users[nTotalUserCarts].szName, sizeof(Users[nTotalUserCarts].szName), INIFILE);
			if (strlen(Users[nTotalUserCarts].szName) > 0) {
				Users[nTotalUserCarts].pDisk=NULL;
				GetPrivateProfileString(buf, "message", "", buf2, 256, INIFILE);
				if (strlen(buf2) > 0) {
					Users[nTotalUserCarts].szMessage=_strdup(buf2);		// this memory will leak!
				} else {
					Users[nTotalUserCarts].szMessage=NULL;
				}
				for (idx3=0; idx3<MAXROMSPERCART; idx3++) {
					char buf3[1024];

					sprintf(buf2, "ROM%d", idx3);
					// line is formatted, except filename which finishes the line
					// T[x]|AAAA|LLLL|filename
					// [x] is the optional bank number from 0-F
					Users[nTotalUserCarts].Img[idx3].dwImg=NULL;
					Users[nTotalUserCarts].Img[idx3].nBank=0;
					GetPrivateProfileString(buf, buf2, "", buf3, 1024, INIFILE);
					if (strlen(buf3) > 0) {
						int strpos=0;
						if (3 != sscanf(buf3, "%c|%x|%x|%n", 
							&Users[nTotalUserCarts].Img[idx3].nType,
							&Users[nTotalUserCarts].Img[idx3].nLoadAddr,
							&Users[nTotalUserCarts].Img[idx3].nLength,
							&strpos)) {
								if (4 != sscanf(buf3, "%c%x|%x|%x|%n", 
									&Users[nTotalUserCarts].Img[idx3].nType,
									&Users[nTotalUserCarts].Img[idx3].nBank,
									&Users[nTotalUserCarts].Img[idx3].nLoadAddr,
									&Users[nTotalUserCarts].Img[idx3].nLength,
									&strpos)) {
										sprintf(buf3, "INI File error reading %s in %s", buf2, buf);
										MessageBox(myWnd, buf3, "Classic99 Error", MB_OK);
										goto skiprestofuser;
								}
						}
						// copy the full string (have to do it this way to include spaces)
						strcpy(Users[nTotalUserCarts].Img[idx3].szFileName, &buf3[strpos]);
						// this doesn't read correctly? Sometimes it does??
						Users[nTotalUserCarts].Img[idx3].nType=buf3[0];
					}
				}
				Users[nTotalUserCarts].nUserMenu = nTotalUserCarts+ID_USER_0;
				++idx2;
				++nTotalUserCarts;
				if (nTotalUserCarts+ID_USER_0 >= ID_SYSTEM_0) break;	// inner loop break
                if (nTotalUserCarts >= MAXUSERCARTS) break;                 // system max
			}
			if (nTotalUserCarts+ID_USER_0 >= ID_SYSTEM_0) break;	// mid loop break
            if (nTotalUserCarts >= MAXUSERCARTS) break;                 // system max
		}
        debug_write("Loaded %d user carts total.", nTotalUserCarts);
		nLoadedUserCarts[cart]=idx2;
		if (idx2 == 0) {
			// there were no carts in this one, so just remove it from the list
			debug_write("Cartridge Group '%s' empty, dropping from list.", UserGroupNames[cart]);
			if (cart < 99) {
				memcpy(UserGroupNames[cart], UserGroupNames[cart+1], sizeof(UserGroupNames[cart]));
			} 
			cart--;		// it will be incremented and we'll be right back where we are
			nLoadedUserGroups--;
		}
		if (nTotalUserCarts+ID_USER_0 >= ID_SYSTEM_0) break;	// outer loop break
        if (nTotalUserCarts >= MAXUSERCARTS) break;                 // system max (do we need this?)
	}
	if (nTotalUserCarts+ID_USER_0 >= ID_SYSTEM_0) {
		debug_write("User cartridge count exceeded available units of %d (tell Tursi!)", ID_SYSTEM_0 - ID_USER_0);
	}
	if (nTotalUserCarts >= 1000) {
		debug_write("Exceeded maximum user cartridge count of %d (impressive!)", MAXUSERCARTS);
	}

skiprestofuser:
	// video filter mode
	FilterMode=		GetPrivateProfileInt("video",	"FilterMode",		FilterMode,		INIFILE);
	// essentially frameskip
	drawspeed=		GetPrivateProfileInt("video",	"frameskip",		drawspeed,		INIFILE);
	// heat map fade speed
	HeatMapFadeSpeed=GetPrivateProfileInt("video",	"heatmapfadespeed",	HeatMapFadeSpeed, INIFILE);
	// set interrupt rate - 50/60
	hzRate=			GetPrivateProfileInt("video",	"hzRate",			hzRate,			INIFILE);
	if ((hzRate != HZ50) && (hzRate != HZ60)) {
		// upgrade code
		if (hzRate == 50) hzRate = HZ50;
		else if (hzRate == 60) hzRate = HZ60;
		else hzRate=HZ60;
	}
	// Whether to enable the F18A support
	bF18Enabled=GetPrivateProfileInt("video",	"EnableF18A",		bF18Enabled, INIFILE);
	// Whether to allow a hacky 80 column mode
	bEnable80Columns=GetPrivateProfileInt("video",	"Enable80Col",		bEnable80Columns, INIFILE);
	// whether to allow an even hackier 128k mode (and will only be valid when 80 columsn is up for now)
	bEnable128k=GetPrivateProfileInt("video",	"Enable128k",		bEnable128k, INIFILE);
	// whether to interleave the GPU execution
	bInterleaveGPU = GetPrivateProfileInt("video",	"InterleaveGPU",	bInterleaveGPU, INIFILE);
	// whether to force correct aspect ratio
	MaintainAspect=	GetPrivateProfileInt("video",	"MaintainAspect",	MaintainAspect, INIFILE);
	// 0-none, 1-DIB, 2-DX, 3-DX Full
	StretchMode=	GetPrivateProfileInt("video",	"StretchMode",		StretchMode,	INIFILE);
	// 5 sprite per line flicker
	bUse5SpriteLimit = GetPrivateProfileInt("video","Flicker",			bUse5SpriteLimit,INIFILE);
	// default screen scale size
	nDefaultScreenScale = GetPrivateProfileInt("video","ScreenScale",	nDefaultScreenScale,INIFILE);
	// -1 means custom
	if ((nDefaultScreenScale!=-1) && ((nDefaultScreenScale < 1) || (nDefaultScreenScale > 4))) nDefaultScreenScale=1;
	nXSize = GetPrivateProfileInt("video", "ScreenX", nXSize, INIFILE);
	if (nXSize < 64) nXSize=64;
	nYSize = GetPrivateProfileInt("video", "ScreenY", nYSize, INIFILE);
	if (nYSize < 64) nYSize=64;
	// full screen lock (overrides StretchMode)
	bAppLockFullScreen = GetPrivateProfileInt("video","LockFullScreen", bAppLockFullScreen, INIFILE);
	if (bAppLockFullScreen) {
		StretchMode = STRETCH_FULL;
		enableAltF4 = 1;	// by default, allow Alt+F4
	}

    // the new application mode - this can only be set manually, it's not saved
    bEnableAppMode = GetPrivateProfileInt("AppMode", "EnableAppMode", bEnableAppMode, INIFILE);
	if (bEnableAppMode) bEnableINIWrite = 0;	// turn off the INI write unless specifically overridden
    bSkipTitle = GetPrivateProfileInt("AppMode", "SkipTitle", bSkipTitle, INIFILE);
    nAutoStartCart = GetPrivateProfileInt("AppMode", "AutoStartCart", nAutoStartCart, INIFILE);
    GetPrivateProfileString("AppMode", "AppName", "Powered by Classic99", AppName, sizeof(AppName), INIFILE);

	// some late "emulation" checks
	// so, we need to read the alt+f4 config here, AFTER we changed the default
	enableAltF4 = GetPrivateProfileInt("emulation", "enableAltF4",	enableAltF4,   INIFILE);
	// and also read the enableINIWrite
	bEnableINIWrite = GetPrivateProfileInt("emulation", "enableINIWrite", bEnableINIWrite,   INIFILE);

	// get screen position
	nVideoLeft = GetPrivateProfileInt("video",		"topX",				-1,					INIFILE);
	nVideoTop = GetPrivateProfileInt("video",		"topY",				-1,					INIFILE);

	// debug
	bScrambleMemory = GetPrivateProfileInt("debug","ScrambleRam",	bScrambleMemory, INIFILE) ? true : false;
	bCorruptDSKRAM =  GetPrivateProfileInt("debug","CorruptDSKRAM",	bCorruptDSKRAM, INIFILE) ? true : false;
	enableDebugOpcodes = GetPrivateProfileInt("debug", "enableDebugOpcodes", enableDebugOpcodes, INIFILE);

	// TV stuff
	TVScanLines=	GetPrivateProfileInt("tvfilter","scanlines",		TVScanLines,	INIFILE);
	double thue, tsat, tcont, tbright, tsharp, tmp;
	tmp=			GetPrivateProfileInt("tvfilter","hue",				100,			INIFILE);
	thue=(tmp-100)/100.0;
	tmp=			GetPrivateProfileInt("tvfilter","saturation",		100,			INIFILE);
	tsat=(tmp-100)/100.0;
	tmp=			GetPrivateProfileInt("tvfilter","contrast",			100,			INIFILE);
	tcont=(tmp-100)/100.0;
	tmp=			GetPrivateProfileInt("tvfilter","brightness",		100,			INIFILE);
	tbright=(tmp-100)/100.0;
	tmp=			GetPrivateProfileInt("tvfilter","sharpness",		100,			INIFILE);
	tsharp=(tmp-100)/100.0;
	SetTVValues(thue, tsat, tcont, tbright, tsharp);

    // TIPISim
    {
        char buf[256];
        GetPrivateProfileString("TIPISim", "URI1", "", buf, sizeof(buf), INIFILE);
        TipiURI[0] = buf;
        GetPrivateProfileString("TIPISim", "URI2", "", buf, sizeof(buf), INIFILE);
        TipiURI[1] = buf;
        GetPrivateProfileString("TIPISim", "URI3", "", buf, sizeof(buf), INIFILE);
        TipiURI[2] = buf;
        GetPrivateProfileString("TIPISim", "TipiDirSort", "FIRST", buf, sizeof(buf), INIFILE);
        TipiDirSort = buf;
        GetPrivateProfileString("TIPISim", "TipiAuto", "off", buf, sizeof(buf), INIFILE);
        TipiAuto = buf;
        GetPrivateProfileString("TIPISim", "TipiTz", "Emu/Classic99", buf, sizeof(buf), INIFILE);
        TipiTz = buf;
        GetPrivateProfileString("TIPISim", "TipiSSID", "", buf, sizeof(buf), INIFILE);
        TipiSSID = buf;
        GetPrivateProfileString("TIPISim", "TipiPSK", "", buf, sizeof(buf), INIFILE);
        TipiPSK = buf;
        GetPrivateProfileString("TIPISim", "TipiName", "TIPISim", buf, sizeof(buf), INIFILE);
        TipiName = buf;
    }

    // MRUs
    for (int idx=1; idx<=MAX_MRU; ++idx) {
        char buf[1024];
        char str[80];
        sprintf(str, "MRU%d", idx);
        GetPrivateProfileString("LastDiskMRU", str, "", buf, sizeof(buf), INIFILE);
        csLastDiskImage[idx-1] = buf;
    }

    for (int idx=1; idx<=MAX_MRU; ++idx) {
        char buf[1024];
        char str[80];
        sprintf(str, "MRU%d", idx);
        GetPrivateProfileString("LastPathMRU", str, "", buf, sizeof(buf), INIFILE);
        csLastDiskPath[idx-1] = buf;
    }
     
    for (int idx=1; idx<=MAX_MRU; ++idx) {
        char buf[1024];
        char str[80];
        sprintf(str, "MRU%d", idx);
        GetPrivateProfileString("LastCartMRU", str, "", buf, sizeof(buf), INIFILE);
        csLastUserCart[idx-1] = buf;
    }
}

// Wrapper function - not available in Win32?
void WritePrivateProfileInt(LPCTSTR lpApp, LPCTSTR lpKey, int nVal, LPCTSTR lpFile) {
	char buf[256];

	sprintf(buf, "%d", nVal);
	WritePrivateProfileString(lpApp, lpKey, buf, lpFile);
}

void SaveConfig() {
	int idx;

	if (!bEnableINIWrite) {
		debug_write("Skipping INI write per configuration");
		return;
	}

	WritePrivateProfileInt(		"audio",		"max_volume",			max_volume,					INIFILE);
	WritePrivateProfileInt(		"audio",		"samplerate",			AudioSampleRate,			INIFILE);
	if (NULL != GetSidEnable) {
		WritePrivateProfileInt(	"audio",		"sid_blaster",			GetSidEnable(),				INIFILE);
	}
	WritePrivateProfileInt(		"audio",		"backgroundNoise",		enableBackgroundHum,		INIFILE);
	WritePrivateProfileInt(		"audio",		"continuousReader",		ScreenReader::GetContinuousRead()?1:0,	INIFILE);

	// write the new data
	EnterCriticalSection(&csDriveType);

	for (int idx=0; idx < MAX_DRIVES-RESERVED_DRIVES; idx++) {
		CString cs;

		cs.Format("Disk%d", idx);

		if (NULL == pDriveType[idx]) {
			WritePrivateProfileInt(cs, "Type", DISK_NONE, INIFILE);
			continue;
		}

		WritePrivateProfileInt(cs, "Type", pDriveType[idx]->GetDiskType(), INIFILE);
		WritePrivateProfileString(cs, "Path", pDriveType[idx]->GetPath(), INIFILE);

		for (int idx2=0; idx2 < DISK_OPT_MAXIMUM; idx2++) {
			int nVal;
			if (pDriveType[idx]->GetOption(idx2, nVal)) {
				WritePrivateProfileInt(cs, pszOptionNames[idx2], nVal, INIFILE);
			}
		}
	}
	LeaveCriticalSection(&csDriveType);

    WritePrivateProfileString("CF7", "BIOS", csCf7Bios, INIFILE);
    WritePrivateProfileString("CF7", "Disk", csCf7Disk, INIFILE);
    WritePrivateProfileInt("CF7", "Size", nCf7DiskSize, INIFILE);

    WritePrivateProfileString("Ramdisk", "Disk", csRamDisk, INIFILE);
    WritePrivateProfileInt("Ramdisk", "Size", nRamDiskSize, INIFILE);

	WritePrivateProfileString(	"emulation",	"AVIFilename",			AVIFileName,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"throttlemode",			ThrottleMode,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"fast16bitram",			Fast16BitRam,				INIFILE);

	if (0 != max_cpf) {
		WritePrivateProfileInt(	"emulation",	"maxcpf",				max_cpf,					INIFILE);
	}
	WritePrivateProfileInt(		"emulation",	"overdrive",			cfg_overdrive,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"enableSpeedKeys",		enableSpeedKeys,			INIFILE);
	WritePrivateProfileInt(		"emulation",	"enableEscape",			enableEscape,	  		    INIFILE);
	WritePrivateProfileInt(		"emulation",	"pauseinactive",		PauseInactive,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"ctrlaltreset",			CtrlAltReset,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"invertcaps",			!gDontInvertCapsLock,		INIFILE);
	WritePrivateProfileInt(     "emulation",    "speechenabled",        SpeechEnabled,              INIFILE);
	WritePrivateProfileInt(		"emulation",	"system",				nSystem,					INIFILE);
	WritePrivateProfileInt(		"emulation",	"slowdown_keyboard",	slowdown_keyboard,			INIFILE);
	WritePrivateProfileInt(		"emulation",	"ps2keyboard",			ps2keyboardok,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"sams_enabled",			sams_enabled,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"sams_size",			sams_size,					INIFILE);
	WritePrivateProfileInt(		"emulation",	"enableAltF4",			enableAltF4,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"enableF10Menu",		enableF10Menu,				INIFILE);
	WritePrivateProfileInt(		"emulation",	"enableINIWrite",		bEnableINIWrite,			INIFILE);
    WritePrivateProfileInt(     "emulation",    "bankedConsoleGROMs",   bankedConsoleGROMs,         INIFILE);

	WritePrivateProfileInt(		"joysticks",	"active",				fJoy,						INIFILE);

	WritePrivateProfileInt(		"joysticks",	"joy1mode",				joyStick[0].mode,			INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1xaxis",			joyStick[0].Xaxis,			INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1yaxis",			joyStick[0].Yaxis,			INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1btns",				joyStick[0].btnMask,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1minX",				joyStick[0].minXDead,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1minY",				joyStick[0].minYDead,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1maxX",				joyStick[0].maxXDead,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy1maxY",				joyStick[0].maxYDead,		INIFILE);

	WritePrivateProfileInt(		"joysticks",	"joy2mode",				joyStick[1].mode,			INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2xaxis",			joyStick[1].Xaxis,			INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2yaxis",			joyStick[1].Yaxis,			INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2btns",				joyStick[1].btnMask,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2minX",				joyStick[1].minXDead,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2minY",				joyStick[1].minYDead,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2maxX",				joyStick[1].maxXDead,		INIFILE);
	WritePrivateProfileInt(		"joysticks",	"joy2maxY",				joyStick[1].maxYDead,		INIFILE);

	WritePrivateProfileInt(		"roms",			"cartgroup",			nCartGroup,					INIFILE);
	WritePrivateProfileInt(		"roms",			"cartidx",				nCart,						INIFILE);
	
	WritePrivateProfileInt(		"video",		"FilterMode",			FilterMode,					INIFILE);
	WritePrivateProfileInt(		"video",		"frameskip",			drawspeed,					INIFILE);
	WritePrivateProfileInt(		"video",		"heatmapfadespeed",		HeatMapFadeSpeed,			INIFILE);

	WritePrivateProfileInt(		"video",		"hzRate",				hzRate,						INIFILE);
	WritePrivateProfileInt(		"video",		"MaintainAspect",		MaintainAspect,				INIFILE);
	WritePrivateProfileInt(		"video",		"EnableF18A",			bF18Enabled,				INIFILE);
	WritePrivateProfileInt(		"video",		"Enable80Col",			bEnable80Columns,			INIFILE);
	WritePrivateProfileInt(		"video",		"Enable128k",			bEnable128k,			    INIFILE);
	WritePrivateProfileInt(		"video",		"InterleaveGPU",		bInterleaveGPU,				INIFILE);

	WritePrivateProfileInt(		"video",		"StretchMode",			StretchMode,				INIFILE);
	WritePrivateProfileInt(		"video",		"Flicker",				bUse5SpriteLimit,			INIFILE);
	WritePrivateProfileInt(		"video",		"ScreenScale",			nDefaultScreenScale,		INIFILE);
	WritePrivateProfileInt(		"video",		"ScreenX",				nXSize,						INIFILE);
	WritePrivateProfileInt(		"video",		"ScreenY",				nYSize,						INIFILE);
	WritePrivateProfileInt(		"video",		"LockFullScreen",		bAppLockFullScreen,			INIFILE);

	WritePrivateProfileInt(		"video",		"topX",					gWindowRect.left,			INIFILE);
	WritePrivateProfileInt(		"video",		"topY",					gWindowRect.top,			INIFILE);

	// debug
	WritePrivateProfileInt(		"debug",		"ScrambleRam",			bScrambleMemory,			INIFILE);
	WritePrivateProfileInt(		"debug",		"CorruptDSKRAM",		bCorruptDSKRAM,				INIFILE);
	WritePrivateProfileInt(		"debug",		"enableDebugOpcodes",	enableDebugOpcodes,			INIFILE);

	// TV stuff
	double thue, tsat, tcont, tbright, tsharp;
	int tmp;
	GetTVValues(&thue, &tsat, &tcont, &tbright, &tsharp);

	tmp=(int)((thue+1.0)*100.0);
	WritePrivateProfileInt(		"tvfilter",		"hue",					tmp,						INIFILE);
	tmp=(int)((tsat+1.0)*100.0);
	WritePrivateProfileInt(		"tvfilter",		"saturation",			tmp,						INIFILE);
	tmp=(int)((tcont+1.0)*100.0);
	WritePrivateProfileInt(		"tvfilter",		"contrast",				tmp,						INIFILE);
	tmp=(int)((tbright+1.0)*100.0);
	WritePrivateProfileInt(		"tvfilter",		"brightness",			tmp,						INIFILE);
	tmp=(int)((tsharp+1.0)*100.0);
	WritePrivateProfileInt(		"tvfilter",		"sharpness",			tmp,						INIFILE);
	WritePrivateProfileInt(		"tvfilter",		"scanlines",			TVScanLines,				INIFILE);

    // TIPISim
    WritePrivateProfileString("TIPISim", "URI1", TipiURI[0], INIFILE);
    WritePrivateProfileString("TIPISim", "URI2", TipiURI[1], INIFILE);
    WritePrivateProfileString("TIPISim", "URI3", TipiURI[2], INIFILE);
    WritePrivateProfileString("TIPISim", "TipiDirSort", TipiDirSort, INIFILE);
    WritePrivateProfileString("TIPISim", "TipiAuto", TipiAuto, INIFILE);
    WritePrivateProfileString("TIPISim", "TipiTz", TipiTz, INIFILE);
    WritePrivateProfileString("TIPISim", "TipiSSID", TipiSSID, INIFILE);
    WritePrivateProfileString("TIPISim", "TipiPSK", TipiPSK, INIFILE);
    WritePrivateProfileString("TIPISim", "TipiName", TipiName, INIFILE);

    // MRUs
    for (int idx=1; idx<=MAX_MRU; ++idx) {
        char str[80];
        sprintf(str, "MRU%d", idx);
        WritePrivateProfileString("LastDiskMRU", str, csLastDiskImage[idx-1], INIFILE);
    }

    for (int idx=1; idx<=MAX_MRU; ++idx) {
        char str[80];
        sprintf(str, "MRU%d", idx);
        WritePrivateProfileString("LastPathMRU", str, csLastDiskPath[idx-1], INIFILE);
    }
     
    for (int idx=1; idx<=MAX_MRU; ++idx) {
        char str[80];
        sprintf(str, "MRU%d", idx);
        WritePrivateProfileString("LastCartMRU", str, csLastUserCart[idx-1], INIFILE);
    }

}

// convert config into meaningful values for AMS system
void SetupSams(int sams_mode, int sams_size) {
	EmulationMode emuMode = None;
	AmsMemorySize amsSize = Mem128k;
	
	// TODO: We don't really NEED this translation layer, but we can remove it later.
	if (sams_mode) {
		// currently only SuperAMS, so if anything set use that
		emuMode = Sams;

		switch (sams_size) {
		case 1:
			amsSize = Mem256k;
			break;
		case 2:
			amsSize = Mem512k;
			break;
		case 3:
			amsSize = Mem1024k;
			break;
		default:
			break;
		}
	}

	SetAmsMemorySize(amsSize);
	InitializeMemorySystem(emuMode);
	SetMemoryMapperMode(Map);
}

void CloseDumpFiles() {
	for (int idx=0; idx<65536; idx++) {
		if (DumpMap[idx]) {
			if ((DumpMap[idx]>0)&&(DumpMap[idx]<10)) {
				if (DumpFile[DumpMap[idx]]) {
					debug_write("Closing dump file %d\n", DumpMap[idx]);
					fclose(DumpFile[DumpMap[idx]]);
					DumpFile[DumpMap[idx]] = NULL;
				}
			}
			DumpMap[idx] = 0;
		}
	}
}
void ReloadDumpFiles() {
	// scan all breakpoints and open any un-open files
	for (int idx=0; idx<MAX_BREAKPOINTS; idx++) {
		if (BreakPoints[idx].Type == BREAK_DISK_LOG) {
			if (NULL == DumpFile[BreakPoints[idx].Data]) {
				char buf[128];
				sprintf(buf, "dump%d.bin", BreakPoints[idx].Data);
				debug_write("Opening dump file %d as %s\n", BreakPoints[idx].Data, buf);
				DumpFile[BreakPoints[idx].Data] = fopen(buf, "wb");
			}
		}
	}
}

// rewrite the user last used list of the user carts
void UpdateUserCartMRU() {
	HMENU hMenu=GetMenu(myWnd);   // root menu
	if (hMenu) {
		hMenu=GetSubMenu(hMenu, 3);     // cartridge menu
		if (hMenu) {
			hMenu=GetSubMenu(hMenu, 2);     // user menu
			if (hMenu) {
                hMenu=GetSubMenu(hMenu, 1);     // recent menu
                if (hMenu) {
                    // we're all set now...
                    // just a dumb blind wipe of MAX_MRU items...
                    for (int idx=0; idx<MAX_MRU; ++idx) {
                        if (!DeleteMenu(hMenu, 0, MF_BYPOSITION)) break;
                    }
                    // now add the MRU list in
                    for (int idx=0; idx<MAX_MRU; ++idx) {
                        if (csLastUserCart[idx].GetLength() == 0) break;
    					AppendMenu(hMenu, MF_STRING, ID_USERCART_MRU+idx, csLastUserCart[idx]);
                    }
				}
			}
		}
	}
}

// used at startup and when switching out of fullscreen mode
void RestoreWindowPosition() {
	// position the window if needed
	if ((nVideoLeft != -1) || (nVideoTop != -1)) {
		RECT check;
		check.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
		check.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
		check.right = check.left + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 256;
		check.bottom = check.top + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 192;
		// if it looks onscreen more or less, then allow it
		if ((nVideoLeft >= check.left) && (nVideoLeft <= check.right) && (nVideoTop >= check.top) && (nVideoTop <= check.bottom)) {
			SetWindowPos(myWnd, HWND_TOP, nVideoLeft, nVideoTop, 0, 0, SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
		}
	}

	SetWindowPos(myWnd, HWND_TOP, nVideoLeft, nVideoTop, nXSize, nYSize, SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);

	ShowWindow(myWnd, SW_SHOWNORMAL);
}

///////////////////////////////////
// Main
// Startup and shutdown system
///////////////////////////////////
int WINAPI WinMain( HINSTANCE hInst, HINSTANCE hInPrevInstance, LPSTR lpCmdLine, int /*nCmdShow*/)
{
	int idx;
	int err;
	char temp[255];
	WNDCLASS aclass;
	TEXTMETRIC myMetric;
	RECT myrect, myrect2;

	// Get the CS initialization done early
	InitializeCriticalSection(&VideoCS);
	InitializeCriticalSection(&DebugCS);
	InitializeCriticalSection(&csDriveType);
	InitializeCriticalSection(&csAudioBuf);
	InitializeCriticalSection(&csSpeechBuf);
    InitializeCriticalSection(&TapeCS);
    InitializeCriticalSection(&csDisasm);

    // setup VDP and staticCPU as shared memory
    const int VDPSIZE = 128*1024;
    const int CPUSIZE = 64*1024;
    HANDLE hMapVDP = NULL;
    HANDLE hMapCPU = NULL;

    // now set up the shared memory - for now just VDP and CPU
    // note that VDP beyond 18k is F18A register centric and probably won't work like you want anyway
    hMapVDP = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // Use system paging file
        NULL,                    // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // Maximum object size (high-order DWORD)
        VDPSIZE,             // Maximum object size (low-order DWORD)
        "Classic99VDPSharedMemory");      // Name of mapping object
    if (hMapVDP != NULL) {
        debug_write("Shared VDP as Classic99VDPSharedMemory");
    }
    VDP = (Byte*)MapViewOfFile(
        hMapVDP ,                // Handle to mapping object
        FILE_MAP_ALL_ACCESS,     // Read/write permission
        0,
        0,
        VDPSIZE);
    if (VDP == NULL) {
        debug_write("VDP FAILED TO SHARE - mallocing.");
        VDP = (Byte*)malloc(VDPSIZE);
    }
    // CPU memory is a bit too much of a mess for a simple share right now, maybe in V4

	hInstance = hInst;
	hPrevInstance=hInPrevInstance;

	// Null the pointers
	myClass=0;
	myWnd=NULL;		// Classic99 Window
	dbgWnd=NULL;	// Debug Window
	lpds=NULL;
	soundbuf=NULL;
	sidbuf=NULL;
	PasteString=NULL;
	PasteIndex=NULL;
	PasteStringHackBuffer=false;
	PasteCount=-1;
	ZeroMemory(nLoadedUserCarts, sizeof(nLoadedUserCarts));
	nLoadedUserGroups=0;
	memset(BreakPoints, 0, sizeof(BreakPoints));
	nBreakPoints=0;
	BreakOnIllegal=false;
	BreakOnDiskCorrupt=false;
	for (idx=0; idx<10; idx++) {
		DumpFile[idx]=NULL;
	}

	// set the working folder to the classic99.exe path
	{
		char path1[1024];
		int bytes = GetModuleFileName(NULL, path1, 1024);
		if (bytes) {
			SetCurrentDirectory(path1);
			debug_write("Set working folder to %s", path1);
		} else {
			debug_write("Failed to get working folder.");
		}
	}

	// Also do the Winsock init (non-fatal if fails)
	{
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;
  
		/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
		wVersionRequested = MAKEWORD(2, 2);
  
		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0) {
			/* Tell the user that we could not find a usable */
			/* Winsock DLL.                                  */
			debug_write("Net init failed with error: %d", err);
		}
	}

	// build the CPU super early - lots of init functions write to its memory
	debug_write("Building CPU");
	pCPU = new CPU9900();							// does NOT reset
	pGPU = new GPUF18A();
	pCurrentCPU = pCPU;
	hWakeupEvent=CreateEvent(NULL, FALSE, FALSE, NULL);
	InterlockedExchange((LONG*)&cycles_left, max_cpf);

	// Get the default np font dimensions with a dummy dc
	myDC=CreateCompatibleDC(NULL);
	SelectObject(myDC, GetStockObject(ANSI_FIXED_FONT));
	if (GetTextMetrics(myDC, &myMetric)) {
		fontX=myMetric.tmMaxCharWidth;
		fontY=myMetric.tmHeight;
	} else {
		fontX=20;
		fontY=20;
	}
	DeleteDC(myDC);

	framedata=(unsigned int*)malloc((512+16)*(192+16)*4);	// This is where we draw everything - 8 pixel border - extra room left for 80 column mode
	framedata2=(unsigned int*)malloc((256+16)*4*(192+16)*4*4);// used for the filters - 16 pixel border on SAI and 8 horizontal on TV (x2), HQ4x is the largest

    if ((framedata==NULL)||(framedata2==NULL)) {
        fail("Unable to allocate framebuffers");
    }

	// create and register a class and open a window
	if (NULL == hPrevInstance)
	{
		aclass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		aclass.lpfnWndProc = myproc;
		aclass.cbClsExtra = 0;
		aclass.cbWndExtra = 0;
		aclass.hInstance = hInstance;
		aclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
		aclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		aclass.hbrBackground = NULL;
		aclass.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
		aclass.lpszClassName = "TIWndClass";
		myClass = RegisterClass(&aclass);
		if (0 == myClass)
		{	
			err=GetLastError();
			sprintf(temp, "Can't create class: 0x%x", err);
			fail(temp);
		}

		// Info windows
		aclass.lpszClassName="Classic99Info";
		aclass.hbrBackground=NULL;
		aclass.lpszMenuName=NULL;
		myClass=RegisterClass(&aclass);
		if (0 == myClass) {
			debug_write("Couldn't register debug window class: 0x%x", GetLastError());
		}
	}

	myWnd = CreateWindow("TIWndClass", "Classic99", g_dwMyStyle, CW_USEDEFAULT, CW_USEDEFAULT, 536, 446, NULL, NULL, hInstance, NULL);
	if (NULL == myWnd)
	{	
		err=GetLastError();
		sprintf(temp, "Can't open window: %x", err);
		fail(temp);
	}
    DragAcceptFiles( myWnd, TRUE );
	ShowWindow(myWnd, SW_HIDE);
	UpdateWindow(myWnd);
	SetActiveWindow(myWnd);
    myMenu = ::GetMenu(myWnd);

	// start the debug updater thread
	if (-1 == _beginthread(DebugUpdateThread, 0, NULL)) {
		debug_write("Failed to start debug update thread.");
	} else {
		debug_write("Debug update thread started.");
	}

    // Load the cartpack, if possible
	hCartPackDll=LoadLibrary("cartpack.dll");
	if (NULL == hCartPackDll) {
		debug_write("Failed to load cartpack library.");
	} else {
        get_app_array=(struct CARTS* (*)(void))GetProcAddress(hCartPackDll, "get_app_array");
        get_game_array=(struct CARTS* (*)(void))GetProcAddress(hCartPackDll, "get_game_array");
        get_app_count=(int (*)(void))GetProcAddress(hCartPackDll, "get_app_count");
        get_game_count=(int (*)(void))GetProcAddress(hCartPackDll, "get_game_count");
	}

	// Fill in the menus
	HMENU hMenu;
	// Systems
	hMenu=GetMenu(myWnd);
	if (hMenu) {
		hMenu=GetSubMenu(hMenu, 2);
		if (hMenu) {
			for (idx=0; idx<sizeof(Systems)/sizeof(struct CARTS); idx++) {
				AppendMenu(hMenu, MF_STRING, ID_SYSTEM_0+idx, Systems[idx].szName);
			}
			DeleteMenu(hMenu, 0, MF_BYPOSITION);	// remove temp separator
		}
	}
	// Apps - loaded above (potentially)
    if ((NULL != hCartPackDll) && (NULL != get_app_array())) {
        Apps = get_app_array();
        int cnt = get_app_count();
        debug_write("Loaded %d applications", cnt);

	    hMenu=GetMenu(myWnd);
	    if (hMenu) {
		    hMenu=GetSubMenu(hMenu, 3);
		    if (hMenu) {
			    hMenu=GetSubMenu(hMenu, 0);
			    if (hMenu) {
				    for (idx=0; idx<cnt; idx++) {
					    AppendMenu(hMenu, MF_STRING, ID_APP_0+idx, Apps[idx].szName);
				    }
				    DeleteMenu(hMenu, 0, MF_BYPOSITION);	// remove temp separator
			    }
		    }
	    }
    } else {
        debug_write("No applications loaded.");
    }

	// Games
    if ((NULL != hCartPackDll) && (NULL != get_game_array())) {
        Games = get_game_array();
        int cnt = get_game_count();
        debug_write("Loaded %d games", cnt);

	    hMenu=GetMenu(myWnd);
	    if (hMenu) {
		    hMenu=GetSubMenu(hMenu, 3);
		    if (hMenu) {
			    hMenu=GetSubMenu(hMenu, 1);
			    if (hMenu) {
				    for (idx=0; idx<cnt; idx++) {
					    AppendMenu(hMenu, MF_STRING, ID_GAME_0+idx, Games[idx].szName);
				    }
				    DeleteMenu(hMenu, 0, MF_BYPOSITION);	// remove temp separator
			    }
		    }
	    }
    } else {
        debug_write("No games loaded.");
    }

	// create the default user0 cart (used for 'open')
	Users=(CARTS*)malloc(sizeof(CARTS));
    if (NULL == Users) {
        fail("Failed to allocate user cart memory default");
    }
	ZeroMemory(Users, sizeof(CARTS));

	for (idx=0; idx<=PCODEGROMBASE; idx++) {
		GROMBase[idx].GRMADD=0;
		for (int i2=0; i2<8; i2++) {
			GROMBase[idx].bWritable[i2]=false;
		}
		GROMBase[idx].grmaccess=2;
		GROMBase[idx].grmdata=0;
        GROMBase[idx].LastRead=0;   // only idx==0 is used though
        GROMBase[idx].LastBase=0;
	}
		
	vdpReset(true);		// TODO: should move these vars into the reset function
	vdpaccess=0;		// No VDP address writes yet 
	vdpwroteaddress=0;
	vdpscanline=0;
	vdpprefetch=0;		// Not really accurate, but eh
	vdpprefetchuninited=true;
	end_of_frame=0;		// Not end of frame yet
	quitflag=0;			// no quit yet
	nCurrentDSR=-1;		// no DSR selected
	memset(nDSRBank, 0, sizeof(nDSRBank));
	timer9901 = 0;
    timer9901Read = 0;
	starttimer9901 = 0;
	timer9901IntReq = 0;
	doLoadInt=false;			// no pending LOAD

	// clear debugging strings
	memset(lines, 0, sizeof(lines));
    EnterCriticalSection(&csDisasm);
        memset(Disasm, 0, sizeof(Disasm));
    LeaveCriticalSection(&csDisasm);
	bDebugDirty=true;

	// Print some initial debug
	debug_write("---");
	debug_write("Classic99 version %s (C)2002-2024 M.Brent", VERSION);
	debug_write("ROM files included under license from Texas Instruments");

	// copy out the command line
	memset(g_cmdLine, 0, sizeof(g_cmdLine));
	if (NULL != lpCmdLine) {
		strncpy(g_cmdLine, lpCmdLine, sizeof(g_cmdLine));
		g_cmdLine[sizeof(g_cmdLine)-1]='\0';
		debug_write("Got command line: %s", g_cmdLine);
	}
	 
	// Set default values for config (alphabetized here)
	strcpy(AVIFileName, "C:\\Classic99.AVI");	// default movie filename
	nCartGroup=0;				// Cartridge group (0-apps, 1-games, 2-user)
	nCart=-1;					// loaded cartridge (-1 is none)
	ThrottleMode = THROTTLE_NORMAL;	// normal throttle
	Fast16BitRam = 0;			// 8-bit RAM
	drawspeed=0;				// no frameskip
	FilterMode=2;				// super 2xSAI
	nDefaultScreenScale=1;		// 1x by default
	nXSize = 256+16;			// default size, but not used while screenscale is set
	nYSize = 192+16;
	fJoy=1;						// enable joysticks
	joyStick[0].mode=0;			// keyboard (constructor actually covers this, but will be explicit since we need to for joy2)
	joyStick[1].mode=1;			// joystick 2
	fJoystickActiveOnKeys=0;	// not reading joystick in the last 3 seconds or so (180 frames)
	hzRate=HZ60;				// 60 hz
	MaintainAspect=1;			// Keep aspect ratio
	bEnable80Columns=1;			// allow the 80 column hack
	bEnable128k=0;				// disable the 128k mode
	max_cpf=DEFAULT_60HZ_CPF;	// max cycles per frame
	cfg_cpf=max_cpf;
	PauseInactive=0;			// don't pause when window inactive
    WindowActive=(myWnd == GetForegroundWindow());  // true if we're active
	SpeechEnabled=1;			// speech is decent now
	Recording=0;				// not recording AVI
	slowdown_keyboard=1;		// slow down keyboard repeat when read via BASIC
	StretchMode=STRETCH_DX;		// dx
	bUse5SpriteLimit=1;			// enable flicker by default
	TVScanLines=1;				// on by default
	sams_enabled=1;				// off by default
	sams_size=3;				// 1MB by default when on (no reason not to use a large card)
	nSystem=1;					// TI-99/4A
	max_volume=80;				// percentage of maximum volume to use
	CPUSpeechHalt=false;		// not halted for speech reasons
	CPUSpeechHaltByte=0;		// doesn't matter
	doLoadInt=false;			// no pending LOAD
	pCPU->enableDebug=1;		// whether breakpoints affect CPU
	pGPU->enableDebug=1;		// whether breakpoints affect GPU

	// initialize debugger links
	InitBug99();
	initDbgHook();

	// init disk DSR system
	InitDiskDSR();

	// Load the Audio code (must be before config)
	PrepareSID();
	if (NULL != SetSidEnable) {
		SetSidEnable(false);	// by default, off
	}
	ScreenReader::initScreenReader();

	// Read configuration - uses above settings as default!
	ReadConfig();
	// A little hacky, but rebuild the CPU using the new settings
	pCPU->buildcpu();
	pGPU->buildcpu();

    // right off the bat, if we are in App Mode, then we need to do some work
    if (bEnableAppMode) {
        // notify
        debug_write("** Application Mode Enabled **");
        debug_write("App: %s", AppName);
        debug_write("Skip Title: %d  AutoStart Index: %d", bSkipTitle, nAutoStartCart);

        // turn off the menu
        SetMenuMode(true, false);
    } else {
        strcpy(AppName, "Classic99 " VERSION);
    }
    // set the title
    szDefaultWindowText = AppName;

	// assume whatever joysticks are there
	installedJoysticks = 0xffff;

	// Update user menu - this will be a function later
	hMenu=GetMenu(myWnd);   // root menu
	if (hMenu) {
		hMenu=GetSubMenu(hMenu, 3);     // cartridge menu
		if (hMenu) {
			hMenu=GetSubMenu(hMenu, 2);     // user menu
			if (hMenu) {
				// User ROMs are a bit different, since we have to read them
				// from the configuration - now nested two deep!
				int nCartIdx = 1;		// skip cart 0, it's used by the open menu
				for (int cart = 0; cart < nLoadedUserGroups; cart++) {
					HMENU hRef = hMenu;
					// as a bit of backwards compatibility, if it's called 'usercart' then store it in the original location
					if (strcmp(UserGroupNames[cart], "UserCart") != 0) {
						// it's different, so create a submenu for it
						hRef = CreateMenu();
						AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hRef, UserGroupNames[cart]);
					}
					for (idx=0; idx<nLoadedUserCarts[cart]; idx++) {
						AppendMenu(hRef, MF_STRING, Users[nCartIdx].nUserMenu, Users[nCartIdx].szName);
						nCartIdx++;
					}
				}
			}
		}
	}
    UpdateUserCartMRU();
	DrawMenuBar(myWnd);
	
	// set temp stuff
	cfg_cpf=max_cpf;

	// set up SAMS emulation
	SetupSams(sams_enabled, sams_size);

	// Load a dummy CPU ROM for the emu to spin on till we load something real
	WriteMemoryBlock(0x0000, DummyROM, 6);

	// start the video processor
	debug_write("Starting Video");
	startvdp();

	// wait for the video thread to initialize so we can resize the window :)
	Sleep(500);

	RestoreWindowPosition();
	if (nDefaultScreenScale != -1) {
		SendMessage(myWnd, WM_COMMAND, ID_CHANGESIZE_1X+nDefaultScreenScale-1, 1);
	}

	// Set menu-based settings (lParam 1 means it's coming from here, not the user)
	// Only some messages care about that param, though
	SendMessage(myWnd, WM_COMMAND, ID_SYSTEM_0+nSystem, 1);
	SendMessage(myWnd, WM_COMMAND, ID_OPTIONS_CPUTHROTTLING, 1);
	SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_16, 1);
	SendMessage(myWnd, WM_COMMAND, ID_DISK_CORRUPTDSKRAM, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_MAINTAINASPECT, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_FILTERMODE_NONE+FilterMode, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_ENABLEF18A, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_INTERLEAVEGPU, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_ENABLE80COLUMNHACK, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_ENABLE128KHACK, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_STRETCHMODE_NONE+StretchMode, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_50HZ, 1);
	SendMessage(myWnd, WM_COMMAND, ID_OPTIONS_PAUSEINACTIVE, 1);
	if (SpeechEnabled) SendMessage(myWnd, WM_COMMAND, ID_OPTIONS_SPEECHENABLED, 1);
	if (CtrlAltReset) SendMessage(myWnd, WM_COMMAND, ID_OPTIONS_CTRL_RESET, 1);
	if (!gDontInvertCapsLock) SendMessage(myWnd, WM_COMMAND, ID_OPTIONS_INVERTCAPSLOCK, 1);
	SendMessage(myWnd, WM_COMMAND, ID_VIDEO_FLICKER, 1);
	if (ScreenReader::GetContinuousRead()) SendMessage(myWnd, WM_COMMAND, ID_SCREENREADER_CONTINUOUS, 1);
	
	if (nCart != -1) {
		switch (nCartGroup) {
		case 0:
			SendMessage(myWnd, WM_COMMAND, ID_APP_0+nCart, 1);
			break;

		case 1:
			SendMessage(myWnd, WM_COMMAND, ID_GAME_0+nCart, 1);
			break;

		case 2:
			SendMessage(myWnd, WM_COMMAND, ID_USER_0+nCart, 1);
			break;
		}
	} else {
		// no cart, we still need to send a reset to load up the system
		SendMessage(myWnd, WM_COMMAND, ID_FILE_RESET, 1);
	}

	// reset the CPU
	pCPU->reset();
	pGPU->reset();

	// Initialize emulated keyboard
	init_kb();

	// start sound
	debug_write("Starting Sound");
	startsound();
	
	// Init disk
	debug_write("Starting Disk");

	// prepare the emulation...
	cpucount=0;
	cpuframes=0;
	timercount=0;

	// set up 60hz timer
	myThread=_beginthread(TimerThread, 0, NULL);
	if (myThread != -1) {
		debug_write("Timer thread began...");
		// try to raise the priority of the thread
		if (!SetThreadPriority((HANDLE)myThread, THREAD_PRIORITY_ABOVE_NORMAL)) {
			debug_write("Failed to update thread priority.");
		}
	} else {
		debug_write("Timer thread failed.");
	}
	// set up speech buffer restart
	myThread=_beginthread(SpeechBufThread, 0, NULL);
	if (myThread != -1) {
		debug_write("Speech Buffer thread began...");
		if (!SetThreadPriority((HANDLE)myThread, THREAD_PRIORITY_ABOVE_NORMAL)) {
			debug_write("Failed to update thread priority.");
		}
	} else {
		debug_write("Speech Buffer thread failed.");
	}

	Sleep(100);			// time for threads to start

	// start up CPU handler
	myThread=_beginthread(emulti, 0, NULL);
	if (myThread != -1) {
		debug_write("CPU thread began...");
	} else {
		debug_write("CPU thread failed.");
		// that's fatal, otherwise nothing will work
		quitflag=1;
	}

	// window management start - returns when it's time to exit
	debug_write("Starting Window management");
	SetFocus(myWnd);
	WindowThread();

    // quiet down the audio
    MuteAudio();
	// save out our config
	SaveConfig();
	// save any previous NVRAM
	saveroms();

	// Fail is the full exit
	debug_write("Shutting down");

    // close anything we still have open
    for (int idx = 0; idx < MAX_DRIVES; idx++) {
        if (NULL != pDriveType[idx]) {
            if (pDriveType[idx]->CheckOpenFiles(false)) {
                debug_write("Force closing files on %s", pDriveType[idx]->GetDiskTypeAsString());
                pDriveType[idx]->CloseAllFiles();
            }
        }
    }

	fail("Normal Termination");
	CloseHandle(hWakeupEvent);

	ShutdownMemorySystem();
	CloseDumpFiles();

    // unmap files
    UnmapViewOfFile(VDP);
    UnmapViewOfFile(staticCPU);
    CloseHandle(hMapVDP);
    CloseHandle(hMapCPU);

	// shutdown Winsock
	WSACleanup();

	// good bye
	return 0;
}


//////////////////////////////////////////////////////
// start up the sound system
//////////////////////////////////////////////////////
void GenerateToneBuffer() {
	unsigned int idx2;
	UCHAR c;
	UCHAR *ptr1, *ptr2;
	unsigned long len1, len2, len;
	DSBUFFERDESC dsbd;
	WAVEFORMATEX pcmwf;

	EnterCriticalSection(&csAudioBuf);

	// if we already have one, get rid of it
	if (NULL != soundbuf) {
		rampVolume(soundbuf, DSBVOLUME_MIN);
		Sleep(1);
		soundbuf->Stop();
		soundbuf->Release();
		soundbuf=NULL;
	}

	// calculate new buffer size - 1 second of sample rate, rounded up to a multiple of hzRate (fps)
	CalculatedAudioBufferSize=AudioSampleRate;
	if (CalculatedAudioBufferSize%(hzRate) > 0) {
		CalculatedAudioBufferSize=((CalculatedAudioBufferSize/(hzRate))+1)*(hzRate);
	}
	CalculatedAudioBufferSize*=2;		// now upscale from samples to bytes
	debug_write("Sample rate: %dhz, Buffer size: %d bytes", AudioSampleRate, CalculatedAudioBufferSize);

	// Here's the format of the audio buffer, 16 bit signed today
	ZeroMemory(&pcmwf, sizeof(pcmwf));
	pcmwf.wFormatTag = WAVE_FORMAT_PCM;		// wave file
	pcmwf.nChannels=1;						// 1 channel (mono)
	pcmwf.nSamplesPerSec=AudioSampleRate;	// 22khz
	pcmwf.nBlockAlign=2;					// 2 bytes per sample * 1 channel
	pcmwf.nAvgBytesPerSec=pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	pcmwf.wBitsPerSample=16;				// 16 bit samples
	pcmwf.cbSize=0;							// always zero (extra data size, not struct size)

	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize=sizeof(dsbd);
	dsbd.dwFlags=DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
//	dsbd.dwBufferBytes=AUDIO_BUFFER_SIZE;	// the sample is AUDIO_BUFFER_SIZE bytes long
	dsbd.dwBufferBytes=CalculatedAudioBufferSize;	// the sample is CalculatedAudioBufferSize bytes long
	dsbd.lpwfxFormat=&pcmwf;
	dsbd.guid3DAlgorithm=GUID_NULL;

	if (FAILED(lpds->CreateSoundBuffer(&dsbd, &soundbuf, NULL)))
	{
		debug_write("Failed to create sound buffer");
		LeaveCriticalSection(&csAudioBuf);
		return;
	}
	
	if (SUCCEEDED(soundbuf->Lock(0, CalculatedAudioBufferSize, (void**)&ptr1, &len1, (void**)&ptr2, &len2, DSBLOCK_ENTIREBUFFER)))
	{
		// since we haven't started the sound, hopefully the second pointer is null
		if (len2 != 0) {
			MessageBox(myWnd, "Failed to lock tone buffer", "Classic99 Error", MB_OK);
		}

		// just make sure it's all zeroed out
		memset(ptr1, 0, len1);
		
		// and unlock
		soundbuf->Unlock(ptr1, len1, ptr2, len2);
	}

	// mute for now - caller will set the volume
	soundbuf->SetVolume(DSBVOLUME_MIN);

	if (FAILED(soundbuf->Play(0, 0, DSBPLAY_LOOPING))) {
		debug_write("Voice DID NOT START");
	}

	LeaveCriticalSection(&csAudioBuf);
}

void GenerateSIDBuffer() {
	unsigned int idx2;
	UCHAR c;
	UCHAR *ptr1, *ptr2;
	unsigned long len1, len2, len;
	DSBUFFERDESC dsbd;
	WAVEFORMATEX pcmwf;

	EnterCriticalSection(&csAudioBuf);

	// if we already have one, get rid of it
	if (NULL != sidbuf) {
		rampVolume(sidbuf, DSBVOLUME_MIN);
		Sleep(1);
		sidbuf->Stop();
		sidbuf->Release();
		sidbuf=NULL;
	}

	// calculate new buffer size - 1 second of sample rate, rounded up to a multiple of hzRate (fps)
	CalculatedAudioBufferSize=AudioSampleRate;
	if (CalculatedAudioBufferSize%(hzRate) > 0) {
		CalculatedAudioBufferSize=((CalculatedAudioBufferSize/(hzRate)+1)*(hzRate));
	}
	CalculatedAudioBufferSize*=2;		// now upscale from samples to bytes

	// Here's the format of the audio buffer, 16 bit signed today
	ZeroMemory(&pcmwf, sizeof(pcmwf));
	pcmwf.wFormatTag = WAVE_FORMAT_PCM;		// wave file
	pcmwf.nChannels=1;						// 1 channel (mono)
	pcmwf.nSamplesPerSec=AudioSampleRate;	// 22khz
	pcmwf.nBlockAlign=2;					// 2 bytes per sample * 1 channel
	pcmwf.nAvgBytesPerSec=pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	pcmwf.wBitsPerSample=16;				// 16 bit samples
	pcmwf.cbSize=0;							// always zero (extra data size, not struct size)

	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize=sizeof(dsbd);
	dsbd.dwFlags=DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
//	dsbd.dwBufferBytes=AUDIO_BUFFER_SIZE;	// the sample is AUDIO_BUFFER_SIZE bytes long
	dsbd.dwBufferBytes=CalculatedAudioBufferSize;	// the sample is CalculatedAudioBufferSize bytes long
	dsbd.lpwfxFormat=&pcmwf;
	dsbd.guid3DAlgorithm=GUID_NULL;

	if (FAILED(lpds->CreateSoundBuffer(&dsbd, &sidbuf, NULL)))
	{
		debug_write("Failed to create SID buffer");
		LeaveCriticalSection(&csAudioBuf);
		return;
	}
	
	if (SUCCEEDED(sidbuf->Lock(0, CalculatedAudioBufferSize, (void**)&ptr1, &len1, (void**)&ptr2, &len2, DSBLOCK_ENTIREBUFFER)))
	{
		// since we haven't started the sound, hopefully the second pointer is null
		if (len2 != 0) {
			MessageBox(myWnd, "Failed to lock SID buffer", "Classic99 Error", MB_OK);
		}

		// just make sure it's all zeroed out
		memset(ptr1, 0, len1);
		
		// and unlock
		sidbuf->Unlock(ptr1, len1, ptr2, len2);
	}

	// mute for now - caller will set the volume
	sidbuf->SetVolume(DSBVOLUME_MIN);

	if (FAILED(sidbuf->Play(0, 0, DSBPLAY_LOOPING))) {
		debug_write("SID DID NOT START");
	}

	LeaveCriticalSection(&csAudioBuf);
}

void startsound()
{ /* start up the sound files */

	DSBUFFERDESC dsbd;
	WAVEFORMATEX pcmwf;
	unsigned int idx, idx2;
	UCHAR *ptr1, *ptr2;
	unsigned long len1, len2, len;
//	char buf[80];
	latch_byte=0;

	if (FAILED(DirectSoundCreate(NULL, &lpds, NULL)))
	{
		lpds=NULL;		// no sound
		return;
	}
	
	if (FAILED(lpds->SetCooperativeLevel(myWnd, DSSCL_NORMAL)))	// normal created a 22khz, 8 bit stereo DirectSound system
	{
		lpds->Release();
		lpds=NULL;
		return;
	}

	sound_init(AudioSampleRate);

	GenerateToneBuffer();
	GenerateSIDBuffer();

	// load the Speech DLL
	hSpeechDll=LoadLibrary("SpeechDll.dll");
	if (NULL == hSpeechDll) {
		debug_write("Failed to load speech library.");
	} else {
		SpeechInit=(void (*)(Byte*,int,int,int))GetProcAddress(hSpeechDll, "SpeechInit");
		SpeechStop=(void (*)(void))GetProcAddress(hSpeechDll, "SpeechStop");
		SpeechRead=(Byte (*)(void))GetProcAddress(hSpeechDll, "SpeechRead");
		SpeechWrite=(bool (*)(Byte, bool))GetProcAddress(hSpeechDll, "SpeechWrite");
		SpeechProcess=(void (*)(Byte*,int))GetProcAddress(hSpeechDll, "SpeechProcess");
	}

	// Zero the temporary buffers
	memset(SpeechTmp, 0, sizeof(SpeechTmp));
	nSpeechTmpPos=0;

	// Now load up the speech system
	/* start audio stream - SPEECHBUFFER buffer, 16 bit, 8khz, max vol, center */
	ZeroMemory(&pcmwf, sizeof(pcmwf));
	pcmwf.wFormatTag = WAVE_FORMAT_PCM;		// wave file
	pcmwf.nChannels=1;						// 1 channel (mono)
	pcmwf.nSamplesPerSec=SPEECHRATE;		// Should be 8khz
	pcmwf.nBlockAlign=2;					// 2 bytes per sample * 1 channel
	pcmwf.nAvgBytesPerSec=pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	pcmwf.wBitsPerSample=16;				// 16 bit samples
	pcmwf.cbSize=0;							// always zero;

	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize=sizeof(dsbd);
	dsbd.dwFlags=DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
	dsbd.dwBufferBytes=SPEECHBUFFER;
	dsbd.lpwfxFormat=&pcmwf;

	hSpeechBufferClearEvent=CreateEvent(NULL, FALSE, FALSE, NULL);

	if (FAILED(lpds->CreateSoundBuffer(&dsbd, &speechbuf, NULL))) {
		debug_write("Failed to create speech sound buffer!");
	} else {
		if (SUCCEEDED(speechbuf->Lock(0, SPEECHBUFFER*pcmwf.nBlockAlign, (void**)&ptr1, &len1, (void**)&ptr2, &len2, DSBLOCK_ENTIREBUFFER))) {
			// since we haven't started the sound, hopefully the second pointer is nil
			if (len2 != 0) {
				debug_write("Failed to lock speech buffer");
			}

			// signed 16-bit - zero the buffer
			memset(ptr1, 0, len1);

			speechbuf->Unlock(ptr1, len1, ptr2, len2);
		}

		LPDIRECTSOUNDNOTIFY8 lpDsNotify;
		if (SUCCEEDED(speechbuf->QueryInterface(IID_IDirectSoundNotify8,(LPVOID*)&lpDsNotify))) {
			DSBPOSITIONNOTIFY notify;
			notify.dwOffset=DSBPN_OFFSETSTOP;
			notify.hEventNotify=hSpeechBufferClearEvent;
			if (FAILED(lpDsNotify->SetNotificationPositions(1, &notify))) {
				debug_write("Failed to set notification positions.");
			}
			lpDsNotify->Release();
		} else {
			debug_write("Failed to get DS8 interface for speech.");
		}

		speechbuf->SetVolume(DSBVOLUME_MIN);

		if (FAILED(speechbuf->Play(0, 0, 0))) {
			debug_write("Speech DID NOT START");
		}

		if (SpeechInit) SpeechInit(SPEECH, 0x8000, SPEECHBUFFER, SPEECHRATE);
	}

	SetSoundVolumes();
}

//////////////////////////////////////////////////////////
// Start up the video system 
//////////////////////////////////////////////////////////
void startvdp()
{ 
	// call VDP Startup
	hVideoThread=NULL;
	hVideoThread=_beginthread((void (__cdecl *)(void*))VDPmain, 0, NULL);
	if (hVideoThread != -1)
		debug_write("Video Thread began...");
	else
		debug_write("Video Thread failed.");

	Sleep(100);

	// first retrace
	retrace_count=0;
}

//////////////////////////////////////////////////////////
// Non-fatal recoverable (?) error
//////////////////////////////////////////////////////////
void warn(char *x)
{ 
	// Warn will for now just dump a message into the log
	// eventually it should pop up a window and ask about
	// continuing

	// note: we assume -2 for the PC, but it could be something else!
	debug_write("%s at address >%04X, Bank >%04X, DSR >%04X", x, pCurrentCPU->GetPC()-2, (xb<<8)|(xbBank), nCurrentDSR&0xffff);
}

//////////////////////////////////////////////////////////
// Fatal error - clean up and exit
// Note that normal exit is a fatal error ;)
//////////////////////////////////////////////////////////
void fail(char *x)
{ 
	// fatal error
	char buffer[1024];
	char buf2[256];
	int idx;

	// just in case it's not set yet
	quitflag=1;

	// add to the log - not useful now, but maybe in the future when it writes to disk
	debug_write(x);

	timeEndPeriod(1);

	sprintf(buffer,"\n%s\n",x);
	sprintf(buf2,"PC-%.4X  WP-%.4X  ST-%.4X\nGROM-%.4X VDP-%.4X\n",pCurrentCPU->GetPC(),pCurrentCPU->GetWP(),pCurrentCPU->GetST(),GROMBase[0].GRMADD,VDPADD);
	strcat(buffer,buf2);
	sprintf(buf2,"Run Duration  : %d seconds\n",timercount/hzRate);
	strcat(buffer,buf2);
	sprintf(buf2,"Operation time: %d instructions processed.\n",cpucount);
	strcat(buffer,buf2);
	sprintf(buf2,"Operation time: %lu cycles processed.\n",total_cycles);
	strcat(buffer,buf2);
	sprintf(buf2,"Display frames: %d video frames displayed.\n",cpuframes);
	strcat(buffer,buf2);

	if (timercount<hzRate) timercount=hzRate;	// avoid divide by zero
	
	sprintf(buf2,"Average speed : %d instructions per second.\n",cpucount/(timercount/hzRate));
	strcat(buffer,buf2);
	sprintf(buf2,"Average speed : %lu cycles per second.\n",total_cycles/(timercount/hzRate));
	strcat(buffer,buf2);
	sprintf(buf2,"Frameskip     : %d\n",drawspeed);
	strcat(buffer,buf2);

	// dump the stats to the debug log
	OutputDebugString(buffer);

	Sleep(600);			// give the threads a little time to shut down

	if (Recording) {
		CloseAVI();
	}

	if (SpeechStop) SpeechStop();

    if (speechbuf) {
		speechbuf->Stop();
		speechbuf->Release();
		speechbuf=NULL;
	}

	if (soundbuf)
	{
		soundbuf->Stop();
		soundbuf->Release();
		soundbuf=NULL;
	}

	if (sidbuf)
	{
		sidbuf->Stop();
		sidbuf->Release();
		sidbuf=NULL;
	}

	if (lpds) {
		lpds->Release();
	}

	if (Users) {
		free(Users);
	}

	if (myWnd) {
		DestroyWindow(myWnd);
	}
	
	if (myClass) {
		UnregisterClass("TIWndClass", hInstance);
	}

	if (framedata) free(framedata);
	if (framedata2) free(framedata2);

	if (hSpeechDll) {
		FreeLibrary(hSpeechDll);
		hSpeechDll=NULL;
	}
    if (hCartPackDll) {
        FreeLibrary(hCartPackDll);
        hCartPackDll = NULL;
    }

	exit(0);
}

/////////////////////////////////////////////////////////
// Return a Word from CPU memory
/////////////////////////////////////////////////////////
Word romword(Word x, READACCESSTYPE rmw)
{ 
    x&=0xfffe;		// drop LSB

    // NOTE: putting a CRU timer mode reset here based on address breaks CS1, so must be wrong

    // This reads the LSB first. This is the correct order (verified)
    Word lsb = rcpubyte(x+1,rmw);
    Word msb = rcpubyte(x,rmw);
    return (msb<<8)+lsb;
}

/////////////////////////////////////////////////////////
// Write a Word y to CPU memory x
/////////////////////////////////////////////////////////
void wrword(Word x, Word y)
{ 
	Word nTmp;

	x&=0xfffe;		// drop LSB

	// now write the new data, LSB first
	wcpubyte(x+1,(Byte)(y&0xff));
	wcpubyte(x,(Byte)(y>>8));

    // Putting a CRU timer reset based on address here seems to have no effect

	// check breakpoints against what was written to where
	for (int idx=0; idx<nBreakPoints; idx++) {
		switch (BreakPoints[idx].Type) {
			case BREAK_EQUALS_WORD:
				if (CheckRange(idx, x)) {
					if ((y&BreakPoints[idx].Mask) == BreakPoints[idx].Data) {		// value matches
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    						TriggerBreakPoint();
                        }
					}
				}
				break;

			case BREAK_EQUALS_REGISTER:
				nTmp=pCurrentCPU->GetWP()+(BreakPoints[idx].A*2);
				if ((nTmp == x) && ((y&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
                    if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    					TriggerBreakPoint();
                    }
				}
				break;
		}
	}
}

/////////////////////////////////////////////////////////
// Window message loop
/////////////////////////////////////////////////////////
void WindowThread() {
	MSG msg;
	HACCEL hAccels;		// keyboard accelerator table for the debug window
	//char buf[128];
	static FILE *fp=NULL;
	int cnt, idx, wid;

	hAccels = LoadAccelerators(NULL, MAKEINTRESOURCE(DebugAccel));
    bWindowInitComplete = true;     // we're finally up and running

	while (!quitflag) {
		// check for messages
		if (0 == GetMessage(&msg, NULL, 0, 0)) {
			quitflag=1;
			break;
		} else {
			if (msg.message == WM_QUIT) {
				// shouldn't happen, since GetMessage should return 0
				quitflag=1;
			} 
			
			if (IsWindow(dbgWnd)) {
				if (TranslateAccelerator(dbgWnd, hAccels, &msg)) {
					// processed (must be before IsDialogMessage)
					continue;
				}
				if (IsDialogMessage(dbgWnd, &msg)) {
					// processed
					continue;
				}
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);		// this will push it to the Window procedure
		}
	}
}

/////////////////////////////////////////////////////////
// Main loop for Emulation
/////////////////////////////////////////////////////////
void __cdecl emulti(void *)
{
	quitflag=0;							// Don't quit

	while (!quitflag)
	{ 
		if ((PauseInactive)&&(!WindowActive)) {
			// we're supposed to pause when inactive, and we are not active
			// So, don't execute an instruction, and sleep a bit to relieve CPU
			// also clear the current timeslice so the machine doesn't run crazy fast when
			// we come back
			Sleep(100);
			InterlockedExchange((LONG*)&cycles_left, 0);
		} else {
			// execute one opcode
			do1();

			// GPU 
			if (bInterleaveGPU) {
				// todo: this is a hack for interleaving F18GPU with the 9900 - and it works, but.. not correct at all.
				if (pGPU->GetIdle() == 0) {
					pCurrentCPU = pGPU;
					for (int nCnt = 0; nCnt < 20; nCnt++) {	// /instructions/ per 9900 instruction - approximation!!
						do1();
						if (pGPU->GetIdle()) {
							break;
						}
						// handle step
						if (cycles_left <= 1) {
							break;
						}
					}
					pCurrentCPU = pCPU;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////
// Read and process the load files
//////////////////////////////////////////////////////////
void LoadOneImg(struct IMG *pImg, char *szFork) {
	FILE *fp;
	char *pData;
	HRSRC hRsrc;
	HGLOBAL hGlob;
	unsigned char *DiskFile=NULL;	// ROM plus 6 byte kracker header max
	char *pszFrom="resource";
	char szFilename[MAX_PATH+3]="";		// extra for parenthesis and space

	if ((NULL == pImg) || (pImg->nType == TYPE_NONE) || (pImg->nType == TYPE_UNSET)) return;

	pData=NULL;

    // if the original type was auto, then restore that
    if (pImg->nOriginalType == TYPE_AUTO) {
        pImg->nType = TYPE_AUTO;
    }
    // and back up the current type (even if we just changed it)
    pImg->nOriginalType = pImg->nType;

	int nLen=pImg->nLength;

    // kind of duplicated code - see OpenUserCart
	if (pImg->nType == TYPE_AUTO) {
		// figure out what it should be by filename, and flag the length to be filled in
		char *pPos = strrchr(pImg->szFileName, '.');
		if ((NULL == pPos) || (pPos == pImg->szFileName)) {
            // there's no extension at all. Since the advent of the FinalROM, this has become
            // a defacto standard of non-inverted ROM cartridge, so it's time to get with the times!
			debug_write("No extension for filename '%s', assuming non-inverted 378 ROM.", pImg->szFileName);
			nLen = -1;		// flag to fill in after loading
			pImg->nType = TYPE_378;
			pImg->nLoadAddr = 0x0000;
		} else {
			nLen = -1;		// flag to fill in after loading
			pImg->nLoadAddr = 0x6000;		// default except for 379 and NVRAM, fixed below
			pPos--;
			switch (*pPos) {
				case 'C':
                    debug_write("Filename specifies 'C' type - 8k cartridge ROM");
					pImg->nType = TYPE_ROM;
					break;

				case 'D':
                    debug_write("Filename specifies 'D' type - 8k XB Bank 2 ROM");
                    pImg->nType = TYPE_XB;
					break;

				case 'G':
                    debug_write("Filename specifies 'G' type - cartridge GROM");
                    pImg->nType = TYPE_GROM;
					break;

				case '3':
				case '9':
                    debug_write("Filename specifies '9' (or '3') type - inverted 379 ROM");
                    pImg->nType = TYPE_379;
					pImg->nLoadAddr = 0x0000;
					break;

				case '8':
                    debug_write("Filename specifies '8' type - non-inverted 378 ROM");
                    pImg->nType = TYPE_378;
					pImg->nLoadAddr = 0x0000;
					break;

				case 'N':
                    debug_write("Filename specifies 'N' type - non-volatile 4k MiniMem RAM");
                    pImg->nType = TYPE_NVRAM;
					pImg->nLoadAddr = 0x7000;		// assuming minimem, if not you need an INI
					pImg->nLength = nLen = 0x1000;	// in case it's not loaded
					break;

				default:
					debug_write("No AUTO character recognized, assuming non-inverted 378 ROM for filename '%s' (unrecognized type)", pImg->szFileName);
                    pImg->nType = TYPE_378;
                    pImg->nLoadAddr = 0x0000;
                    break;
            }
		}
	}

	if ((TYPE_KEYS != pImg->nType) && (TYPE_OTHER != pImg->nType)) {
		if (NULL != pImg->dwImg) {
            HMODULE hModule = NULL;
			hRsrc=FindResource(hModule, MAKEINTRESOURCE(pImg->dwImg), szFork);
            if ((NULL == hRsrc)&&(NULL != hCartPackDll)) {
                hModule = hCartPackDll;
                hRsrc=FindResource(hModule, MAKEINTRESOURCE(pImg->dwImg), szFork);
            }
			if (hRsrc) {
				int nRealLen=SizeofResource(hModule, hRsrc);
				if (nLen > nRealLen) nLen=nRealLen;

				hGlob=LoadResource(hModule, hRsrc);
				if (NULL != hGlob) {
					pData=(char*)LockResource(hGlob);
				}
			} else {
                // we can't offer very good debug at the moment...
                debug_write("Resource for selected module was not found.");
            }
		} else {
			// It's a disk file. Worse, it may be a disk file with a header. 
			// But we may be able to determine that.
			if (strlen(pImg->szFileName) == 0) {
				return;
			}
			fp=fopen(pImg->szFileName, "rb");
			if (NULL == fp) {
				debug_write("Failed to load '%s', error %d", pImg->szFileName, errno);
				return;
			}
			pszFrom="disk";
			int nRealLen=0;
            
            // get filesize
            fseek(fp, 0, SEEK_END);
            int fSize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (NULL != DiskFile) free(DiskFile);
            if (fSize > MAX_BANKSWITCH_SIZE+6) {
                fSize = MAX_BANKSWITCH_SIZE+6;
            }
            DiskFile=(unsigned char*)malloc(fSize);
            if (NULL == DiskFile) {
                debug_write("Can't allocate memory for read of '%s'!", pImg->szFileName);
                return;
            }

			while ((!feof(fp)) && (nRealLen < fSize)) {
				int nTmp = fread(&DiskFile[nRealLen], 1, fSize-nRealLen, fp);
				if (nTmp == 0) {
					debug_write("Failed to read entire file - too large or disk error. Max size = %dk!", MAX_BANKSWITCH_SIZE/1024);
					break;
				}
				nRealLen+=nTmp;
			}
			fclose(fp);

			// don't check if it is a 379 or MBX type file - this is for GRAMKracker files
			if ((nRealLen > 6) && ((pImg->nType == TYPE_ROM)||(pImg->nType == TYPE_XB)||(pImg->nType == TYPE_GROM))) {
				// Check for 6 byte header - our simple check is if
				// byte 0 is 0x00 or 0xff, and bytes 4/5 contain the
				// load address, then strip the first six bytes
				if ((DiskFile[0]==0x00) || (DiskFile[0]==0xff)) {	// a flag byte?
					if (DiskFile[4]*256+DiskFile[5] == pImg->nLoadAddr) {
						debug_write("Removing header from %s", pImg->szFileName);
						nRealLen-=6;
						memmove(DiskFile, &DiskFile[6], nRealLen);
					} 
				}
			}
			// regardless of the filetype, check for PC99 naming PHMxxxx.GRM, and remove its header
			if ((nRealLen > 6) && (strstr(pImg->szFileName,"PHM")) && (strstr(pImg->szFileName,".GRM"))) {
				if ((DiskFile[0]==0x00) || (DiskFile[0]==0xff)) {	// a flag byte?
					debug_write("PC99 filename? Removing header from %s", pImg->szFileName);
					nRealLen-=6;
					memmove(DiskFile, &DiskFile[6], nRealLen);
				}
			}

			if (nLen < 1) {
				// fill in the loaded length
				pImg->nLength = nRealLen;
				nLen = nRealLen;
			} else if (nLen != nRealLen) {
				debug_write("Warning: size mismatch on %s - expected >%04x bytes but found >%04x", pImg->szFileName, pImg->nLength, nRealLen);
			}
			if (nLen > nRealLen) nLen=nRealLen;
			pData=(char*)DiskFile;
			sprintf(szFilename, "(%s) ", pImg->szFileName);
		}
	}

    // Don't return without freeing DiskFile

	if ((pImg->nType == TYPE_KEYS) || (pImg->nType == TYPE_OTHER) || (NULL != pData)) {
		// finally ;)
		debug_write("Loading file %sfrom %s: Type %c, Bank %d, Address 0x%04X, Length 0x%04X", szFilename,  pszFrom, pImg->nType, pImg->nBank, pImg->nLoadAddr, nLen);

		switch (pImg->nType) {
			case TYPE_GROM:
				if (pImg->nLoadAddr+nLen > 65536) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=65536-pImg->nLoadAddr;
				}
				if (pImg->nBank > 0) {
					grombanking=1;
				}
				if (pImg->nBank == -1) {
					// copy to all banks
					for (int idx=0; idx<PCODEGROMBASE; idx++) {
						memcpy(&GROMBase[idx].GROM[pImg->nLoadAddr], pData, nLen);
					}
				} else {
					if ((pImg->nBank < PCODEGROMBASE) && (pImg->nBank >= 0)) {
						memcpy(&GROMBase[pImg->nBank].GROM[pImg->nLoadAddr], pData, nLen);
					} else {
						debug_write("Not loading to unsupported GROM bank %d", pImg->nBank);
					}
				}
				break;

			case TYPE_ROM:
				if ((pImg->nLoadAddr >= 0x6000) && (pImg->nLoadAddr <= 0x7fff)) {
					// cart ROM, load into the paged data
					if (pImg->nLoadAddr+nLen > 0x8000) {
						debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
						nLen=0x8000-pImg->nLoadAddr;
					}
                    // this is unlikely, but better safe than crashy
                    if (NULL == CPU2) {
                        CPU2=(Byte*)malloc(8192);
                        if (NULL == CPU2) {
                            // this will probably crash... maybe fail?
                            debug_write("Failed to allocate base RAM for cartridge, aborting load.");
                            break;
                        }
                        xb=0;
                        xbBank=0;
                    }
					// load into the first bank of paged memory (8k max)
					memcpy(&CPU2[pImg->nLoadAddr-0x6000], pData, nLen);
					// also load to main memory incase we aren't paging
					WriteMemoryBlock(pImg->nLoadAddr, pData, nLen);
					// and set up the ROM map
					memset(&ROMMAP[pImg->nLoadAddr], 1, nLen);
				} else {
					if (pImg->nLoadAddr < 0x6000) {
						if (pImg->nLoadAddr < 0x4000) {
							// non-DSR override
							if (pImg->nLoadAddr+nLen > 0x4000) {
								debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
								nLen=0x4000-pImg->nLoadAddr;
							}
						} else {
							// DSR override
							if (pImg->nLoadAddr+nLen > 0x6000) {
								debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
								nLen=0x6000-pImg->nLoadAddr;
							}
						}
					} else if (pImg->nLoadAddr+nLen > 0x10000) {
						debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
						nLen=65536-pImg->nLoadAddr;
					}
					WriteMemoryBlock(pImg->nLoadAddr, pData, nLen);
					memset(&ROMMAP[pImg->nLoadAddr], 1, nLen);
				}
				break;

			case TYPE_NVRAM:
				// for now, this is for cartridge space only, and only if not banked
				if ((pImg->nLoadAddr >= 0x6000) && (pImg->nLoadAddr <= 0x7fff)) {
					if (xb > 0) {
						debug_write("NVRAM only supported for non-paged carts, not loading.");
					} else {
						// cart ROM, load into the non-paged data
						if (pImg->nLoadAddr+nLen > 0x8000) {
							debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
							nLen=0x8000-pImg->nLoadAddr;
							// becase we save this back out, fix up the size internally
							pImg->nLength = nLen;
						}
                        // this is unlikely, but better safe than crashy
                        if (NULL == CPU2) {
                            CPU2=(Byte*)malloc(8192);
                            if (NULL == CPU2) {
                                debug_write("Failed to allocate base NVRAM for cartridge, aborting load.");
                                break;
                            }
                            xb=0;
                            xbBank=0;
                        }
						// load into the first bank of paged memory
						memcpy(&CPU2[pImg->nLoadAddr-0x6000], pData, nLen);
						// also load to main memory incase we aren't paging
						WriteMemoryBlock(pImg->nLoadAddr, pData, nLen);
						// it's RAM, so no memory map
					}
				} else {
					debug_write("NVRAM currently only supported for cartridge space, not loading to >%04X", pImg->nLoadAddr);
				}
				break;

			case TYPE_SPEECH:
				if (pImg->nLoadAddr+nLen > 65536) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=65536-pImg->nLoadAddr;
				}
				memcpy(&SPEECH[pImg->nLoadAddr], pData, nLen);
				break;

			case TYPE_XB:
				if (pImg->nLoadAddr-0x6000+nLen > 8192) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=8192-(pImg->nLoadAddr-0x6000);
				}
                // make sure we have enough space
                if ((NULL == CPU2)||(xb<1)) {
                    Byte *CNew = (Byte*)realloc(CPU2, 8192*2);
                    if (NULL == CNew) {
                        debug_write("Failed to reallocate RAM for XB cartridge, aborting load.");
                        break;
                    } else {
                        CPU2 = CNew;
                    }
                }
				// load it into the second bank of switched memory (2k in)
				memcpy(&CPU2[pImg->nLoadAddr-0x4000], pData, nLen);
				xb=1;		// one xb bank loaded
				bInvertedBanks=false;
				bUsesMBX = false;
				break;

			case TYPE_378:
			case TYPE_379:
			case TYPE_MBX:
            {
				// Non-inverted or Inverted XB style, but more than one bank! Up to 32MB mapped 8k at a time
				// not certain the intended maximum for MBX, but it has a slightly different layout
				// We still use the same loader here though.
				if (pImg->nLoadAddr+nLen > MAX_BANKSWITCH_SIZE) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=MAX_BANKSWITCH_SIZE-pImg->nLoadAddr;
				}
				// update ROM map if in bank 0
				if (pImg->nLoadAddr < 0x2000) {
					memset(&ROMMAP[pImg->nLoadAddr+0x6000], 1, min(nLen, 0x2000-pImg->nLoadAddr));
				}

                int oldXB = xb;
				xb=(pImg->nLoadAddr+nLen+8191)/8192;	// round up, this many banks are loaded
				// now we need to make it a power of 2 for masking
				switch (xb) {
					case 0:			// should be impossible (result 0)
					case 1:			// 1 bank, no switching (result 0)
						xb=0;
						break;
					
					case 2:			// 2 banks uses 1 bit (result 1)
						xb=1;
						break;

					case 3:
					case 4:			// 2-4 banks uses 2 bits (result 3)
						xb=3;
						break;

					case 5:
					case 6:
					case 7:
					case 8:			// 5-8 banks uses 3 bits (result 7)
						xb=7;
						break;

					case 9:
					case 10:
					case 11:
					case 12:
					case 13:
					case 14:
					case 15:		
					case 16:		// 9-16 banks uses 4 bits (result 15);
						xb=15;
						break;

					default:
						// the ranges are getting kind of large for switch..case at this point
						if (xb<=32) {	// 17-32 banks uses 5 bits (result 31)
							xb=31;
						} else if (xb<=64) {	// 33-64 banks uses 6 bits (result 63)
							xb=63;
						} else if (xb<=128) {	// 65-128 banks are 7 bits (result 127)
							xb=127;
						} else if (xb<=256) {	// 129-256 banks are 8 bits (result 255)
							xb=255;
						} else if (xb<=512) {	// 257-512 banks are 9 bits (result 511)
							xb=511;
						} else if (xb<=1024) {	// 513-1024 banks are 10 bits (result 1023)
							xb=1023;
						} else if (xb<=2048) {	// 1025-2048 banks are 11 bits (result 2047)
							xb=2047;
						} else if (xb<=4096) {  // 2049-4096 banks are 12 bits (result 4095)
                            // maximum size for address latching
                            xb=4095;
                        } else if (xb<=8192) {  // 4097-8192 banks are 13 bits (result 8191)
							debug_write("Enable gigacart 64MB");
                            xb=8191;
                        } else if (xb<=16384) {  // 8193-16384 banks are 14 bits (result 16383)
							xb=16383;
							debug_write("Enable gigacart 128MB with 256 bytes GROM");
                        } else if (xb<=32768) {  // 16385-32768 banks are 15 bits (result 32767)
							debug_write("Enable gigacart 256MB");
                            xb=32767;
                        } else {  // 32769-65536 banks are 16 bits (result 65535)
							debug_write("Enable gigacart 512MB");
                            xb=65535;
						}
						break;
				}

                // make sure we have enough space
                if ((NULL == CPU2)||(oldXB<xb)) {
                    Byte *CNew=(Byte*)realloc(CPU2, 8192*(xb+1));
                    if (NULL == CNew) {
                        debug_write("Failed to reallocate RAM for cartridge, aborting load.");
                        xb=0;
                        xbBank=0;
                        break;
                    } else {
                        CPU2 = CNew;
                    }
                }
				memcpy(&CPU2[pImg->nLoadAddr], pData, nLen);

                if (xb == 16383) {
                    // copy the GROM data into the GROM space
                    // copy to all banks - only 256 bytes, repeated over and over
					for (int idx=0; idx<PCODEGROMBASE; idx++) {
                        for (int adr=0x8000; adr<0xa000; adr+=256) {
                            // last 256 bytes of range
    						memcpy(&GROMBase[idx].GROM[adr], &CPU2[128*1024*1024-256], 256);
                        }
					}
                }
               
				// TYPE_378 is non-inverted, non MBX
				bInvertedBanks = false;
				bUsesMBX = false;
				if (pImg->nType == TYPE_379) {
					bInvertedBanks=true;
				} else if (pImg->nType == TYPE_MBX) {
					bUsesMBX=true;
				}
				debug_write("Loaded %d bytes, %sinverted, %sbank mask 0x%X", nLen, bInvertedBanks?"":"non-", bUsesMBX?"MBX, ":"", xb);
            }
				break;

			case TYPE_RAM:
				if (pImg->nLoadAddr+nLen > 65536) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=65536-pImg->nLoadAddr;
				}
				//memcpy(&CPU[pImg->nLoadAddr], pData, nLen);
				WriteMemoryBlock(pImg->nLoadAddr, pData, nLen);
				break;

			case TYPE_VDP:
				if (pImg->nLoadAddr+nLen > 16384) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=16384-pImg->nLoadAddr;
				}
				memcpy(&VDP[pImg->nLoadAddr], pData, nLen);
				break;

			case TYPE_DSR:	// always loads at >4000, the load address is the CRU base
				if (nLen > 8192) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=8192;
				}
				if ((pImg->nLoadAddr & 0xF000) != 0x1000) {
					debug_write("%s has invalid CRU base %04X for DSR - not loading.", pImg->szFileName, pImg->nLoadAddr);
				} else {
					memcpy(&DSR[(pImg->nLoadAddr>>8)&0x0f][0], pData, nLen);
				}
				break;

			case TYPE_DSR2:	// always loads at >4000, the load address is the CRU base
				if (nLen > 8192) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=8192;
				}
				// TODO: throw a debug warning if the address is not a valid CRU base
				memcpy(&DSR[(pImg->nLoadAddr>>8)&0x0f][0x2000], pData, nLen);
				break;

			case TYPE_PCODEG:
				if (pImg->nLoadAddr+nLen > 65536) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=65536-pImg->nLoadAddr;
				}
				memcpy(&GROMBase[PCODEGROMBASE].GROM[pImg->nLoadAddr], pData, nLen);
				break;

			case TYPE_AMS:
				// Force the AMS card to on (reset later will init it)
				sams_enabled=1;
				sams_size=3;
				// decode the RLE encoded data into the AMS memory block
				// We use both words as the size value to get a 32-bit size
				RestoreAMS((unsigned char*)pData, (pImg->nLoadAddr<<16)|nLen);
				break;

			case TYPE_KEYS:
				// szFilename is the key presses but \n needs to be replaced with enter
				if (NULL != PasteString) {
					free(PasteString);
					PasteStringHackBuffer=false;
				}
				char *p;
				while (p = strstr(pImg->szFileName, "\\n")) {
					*p='\n';
					memmove(p+1, p+2, strlen(p+1));
				}
				PasteString=(char*)malloc(strlen(pImg->szFileName)+1);
                if (NULL != PasteString) {
    				strcpy(PasteString, pImg->szFileName);
	    			PasteCount=-1;		// give it time to come up!
		    		PasteIndex=PasteString;
                } else {
                    debug_write("Paste string failed to allocate memory");
                }
				break;

			case TYPE_MPD:
				// My own hacky MPD GROM hack - up to 128k with special internal banking rules
				if (pImg->nLoadAddr+nLen > 144*1024) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=144*1024-pImg->nLoadAddr;
				}
				memcpy(&mpdGrom[pImg->nLoadAddr], pData, nLen);
				bMpdActive=true;
				debug_write("Loaded %d bytes", nLen);
				debug_write("WARNING: MPD ACTIVE. 99/4A only! Console GROMs overridden!");
				break;

			case TYPE_UBER_GROM:
				// my hacky little Uber GROM simulation
				if (pImg->nLoadAddr+nLen > 120*1024) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=120*1024-pImg->nLoadAddr;
				}
				memcpy(&UberGROM[pImg->nLoadAddr], pData, nLen);
				bUberGROMActive=true;
				grombanking=1;
				debug_write("Loaded %d bytes", nLen);
				debug_write("WARNING: Uber-GROM ACTIVE");
				break;

			case TYPE_UBER_EEPROM:
				if (pImg->nLoadAddr+nLen > 4*1024) {
					debug_write("%s overwrites memory block - truncating.", pImg->szFileName);
					nLen=4*1024-pImg->nLoadAddr;
				}
				memcpy(&UberEEPROM[pImg->nLoadAddr], pData, nLen);
				debug_write("Loaded %d bytes", nLen);
				break;

			case TYPE_OTHER:
				// loads from another Classic99 ROM group - most useful for loading
				// built-in ROMs. LoadAddr is the group, and nLength is the index.
				{
					struct CARTS *pBank = NULL;
					switch (pImg->nLoadAddr) {
						case 0:	pBank=Apps; break;
						case 1: pBank=Games; break;
						case 2: pBank=Users; break;
					}
					if (NULL == pBank) {
						debug_write("Invalid bank index %d in 'Other' cart group", pImg->nLoadAddr);
					} else {
						if (pImg->nLength >= MAXROMSPERCART) {
							debug_write("Invalid cart index %d in 'Other' cart group", pImg->nLength);
						} else {
							pCurrentHelpMsg=pBank[pImg->nLength].szMessage;
							for (int idx=0; idx<MAXROMSPERCART; idx++) {
								LoadOneImg(&pBank[pImg->nLength].Img[idx], "ROMS");
							}
							pMagicDisk=pBank[pImg->nLength].pDisk;
						}
					}
				}
				break;

			default:
				break;
		}
	}
	
	// WIN32 does not require (or even permit!) us to unlock and release these objects
    // But we do need to free DiskFile if set
    if (NULL != DiskFile) {
        free(DiskFile);
    }
}

// this searches banked cartridge space for a ROM header and
// sets xbBank to that page. Only checks first and last page.
// This is NOT what a real console does - real ROM carts
// come up in a semi-random page (although it's usually first
// or last, though you can't predict which until you try).
// And yeah, I need this because of the FinalGROMs discard of
// previous naming conventions. Hours of my life gone because
// someone doesn't like "8". >:(
void findXBbank() {
    //xbBank=rand()%xb;		// TODO: make an option

    // default to first page
    xbBank = 0;
    if (xb > 0) {
        // cartridge is in CPU2, and size is 8192*(xb+1) pages
        if (0xaa == CPU2[0]) {
            // header in first page
            debug_write("Found ROM header in page 0, setting bank. NOT REALISTIC.");
        } else if (0xaa == CPU2[8192*xb]) {
            // header in last page
            xbBank = xb;
            debug_write("Found ROM header in last page, setting bank. NOT REALISTIC.");
        } else {
            debug_write("No ROM header found in paged cart, setting bank to 0.");
        }
    }
}

void readroms() { 
	int idx;

#ifdef USE_BIG_ARRAY
	// TODO HACK
	{
		FILE *fp;
		if (strlen(g_cmdLine) > 0) {
			char *p=g_cmdLine;
			// remove quotes
			while (*p) {
				if (*p == '\"') {
					strcpy(p, p+1);
				} else {
					p++;
				}
			}
			debug_write("Reading hack %s...", g_cmdLine);
			fp = fopen(g_cmdLine, "rb");
			if (NULL == fp) {
				debug_write("Failed to open file, code %d", errno);
				BIGARRAYSIZE = 0;
			} else {
				fseek(fp, 0, SEEK_END);
				int x = ftell(fp);
				fseek(fp, 0, SEEK_SET);
				BIGARRAY = (unsigned char*)malloc(x);
				if (BIGARRAY == NULL) {
					debug_write("Failed to allocate memory for BIG ARRARY HACK");
					BIGARRAYSIZE = 0;
				} else {
					BIGARRAYSIZE = fread(BIGARRAY, 1, x, fp);
					debug_write("Read %d bytes", BIGARRAYSIZE);

                    debug_write("Copying hack to AMS...");
                    PreloadAMS(BIGARRAY, BIGARRAYSIZE);
				}
				fclose(fp);
			}
		} else {
			BIGARRAYSIZE = 0;
		}
		BIGARRAYADD=0;
	}
	// end hack
#endif

	// save any previous NVRAM
	saveroms();

	// process the dump list and remove any disk files
	CloseDumpFiles();

	// process the breakpoint list and re-open any listed files
	ReloadDumpFiles();

    // make sure there's at least a little memory at CPU2, since we are building on very old code
    // the xb mask must now always be associated with CPU2
    if (NULL == CPU2) {
        CPU2=(Byte*)malloc(8192);
        if (NULL == CPU2) {
            fail("Failed to allocate initial RAM for cartridge");
        }
        xb=0;
        xbBank=0;
    }

	// now memory
	memset(ROMMAP, 0, 65536);	// this is not RAM
	if (!bWarmBoot) {
		memrnd(CPU2, 8192*(xb+1));
		memrnd(VDP, 16384);
	}
	memset(CRU, 1, 4096);		// I think CRU is deterministic
	CRU[0]=0;	// timer control
	CRU[1]=0;	// peripheral interrupt mask
	CRU[2]=0;	// VDP interrupt mask
	CRU[3]=0;	// timer interrupt mask??
//  CRU[12-14]  // keyboard column select
//  CRU[15]     // Alpha lock 
//  CRU[24]     // audio gate (leave high)
	CRU[25]=0;	// mag tape out - needed for Robotron to work!
	CRU[27]=0;	// mag tape in (maybe all these zeros means 0 should be the default??)
	memset(DSR, 0, 16*16384);	// not normally RAM
	memset(key, 0, 256);		// keyboard

	pMagicDisk=NULL;
	pCurrentHelpMsg=NULL;
	bMpdActive = false;			// no MPD unless we load it!
	bUberGROMActive = false;	// same with UberGROM

	for (idx=0; idx<=PCODEGROMBASE; idx++) {
		// not normally RAM
		memset(GROMBase[idx].GROM, 0, sizeof(GROMBase[idx].GROM));
	}

	// Disable the PCode card (at >1F00)
	DSR[0xF][0]=0;
	xb=0;									// no XB bank loaded
	grombanking=0;							// not using grom banking
	nCurrentDSR=-1;							// no DSR paged in
	memset(nDSRBank, 0, sizeof(nDSRBank));	// not on second page of DSR
    memset(cycleCounter, 0, sizeof(cycleCounter));

	// load the always load files
	for (idx=0; idx<sizeof(AlwaysLoad)/sizeof(IMG); idx++) {
		LoadOneImg(&AlwaysLoad[idx], "ROMS");
	}

    // reconfigure for CF7 if necessary
    if (csCf7Bios.GetLength() > 0) {
        FILE *fp = fopen(csCf7Bios, "rb");
        if (NULL != fp) {
            debug_write("Replacing DSK1-3 with CF7 emulation at CRU >1000.");
            fread(DSR[0], 1, 8192, fp);
            fclose(fp);
            // make sure DSK 1-3 is a cf7Disk object, which does nothing
            for (int idx = 1; idx < 4; ++idx) {
                if (NULL != pDriveType[idx]) {
                    free(pDriveType[idx]);
                }
                pDriveType[idx] = new Cf7Disk;
            }
        } else {
            debug_write("Failed to read CF7 BIOS '%s'", csCf7Bios.GetString());
        }
    }
    if (csRamDisk.GetLength() > 0) {
        if (csCf7Bios.GetLength() > 0) {
            // A real RAMdisk can live anyway, but for now Classic99 will restrict it to >1000
            debug_write("Can not enable RAMdisk if CF7 is active - disable CF7.");
        } else {
            // the actual bios is read from the file
            // the default handling should work for it...? Kinda hacky still...
            // we don't NEED a RAMdisk, so this will stay unofficial hack like CF7
            FILE* fp = fopen(csRamDisk, "rb");
            if (NULL == fp) {
                // no RAMdisk file yet
                memset(DSR[0], 0, 8192);
            } else {
                // first 8k is the disk BIOS (okay if it's not loaded yet)
                fread(DSR[0], 1, 8192, fp);
                fclose(fp);
                // TODO: we could replace the TI handling per disk, but I don't think we need to...
                // We could read the ROS settings if we wanted to...
                debug_write("WARNING: RAMdisk will override Classic99 disk configuration");
            }
        }
    }

	// load the appropriate system ROMs - each system is a cart structure that contains MAXROMSPERCART ROMs
	for (idx=0; idx<MAXROMSPERCART; idx++) {
		LoadOneImg(&Systems[nSystem].Img[idx], "ROMS");
	}

	// if there is a cart plugged in, load that too
	if (nCart != -1) {
		struct CARTS *pBank=NULL;

 		switch (nCartGroup) {
		case 0:	
			if (nCart < 100) {
				pBank=Apps; 
			}
			break;
		case 1: 
			if (nCart < 100) {
				pBank=Games; 
			}
			break;
		case 2: 
			if (nCart < nTotalUserCarts) {
				pBank=Users; 
			}
			break;
		}

		if (pBank) {
			pCurrentHelpMsg=pBank[nCart].szMessage;
			for (idx=0; idx<MAXROMSPERCART; idx++) {
				LoadOneImg(&pBank[nCart].Img[idx], "ROMS");
			}
			pMagicDisk=pBank[nCart].pDisk;
		}
	}

	// check for a ROM open request on the command line
	// we then nuke the command line so it doesn't get stuck
	// in a loop re-processing it on every reset (which is part
	// of the cart load)
	{
		FILE *fp;
		if (strlen(g_cmdLine) > 0) {
			char *p=g_cmdLine;
			// remove quotes
			while (*p) {
				if (*p == '\"') {
					strcpy(p, p+1);
				} else {
					p++;
				}
			}
			// note that since quotes are nuked (even earlier than
			// here with the big file hack), we can't currently
			// support multiple commands, so "-rom" is the only
			// valid input, and the rest is a filename
			if (0 == strncmp(g_cmdLine, "-rom", 4)) {
				p = g_cmdLine+5;
				PostMessage(myWnd, WM_COMMAND, ID_CART_USER_OPEN, (LPARAM)p);
				// don't process it again
				g_cmdLine[0] = '\0';
			}
		}
	}

    // set xbBank if paging
    findXBbank();

#ifdef _DEBUG
    // a sanity test for me - check the DSRs make sense
    // This is mostly for Ami99DSK

    for (int dsr=0; dsr<0x10; ++dsr) {
        if (DSR[dsr][0] != 0xaa) continue;

        // verify the DSR header
        debug_write("DSR at CRU >1%X00", dsr);

        int i;
        int x = DSR[dsr][4]*256+DSR[dsr][5]-0x4000;
        if (x > 0) {
            debug_write("Power-up Chain:");
            do {
                int next = DSR[dsr][x]*256+DSR[dsr][x+1];
                int run = DSR[dsr][x+2]*256+DSR[dsr][x+3];
                debug_write("  >%04X >%04X", next, run);
                x = next-0x4000;
            } while (x>0);
        }

        x = DSR[dsr][6]*256+DSR[dsr][7]-0x4000;
        if (x > 0) {
            debug_write("Program List (should not exist):");
            do {
                char buf[257];
                int next = DSR[dsr][x]*256+DSR[dsr][x+1];
                int run = DSR[dsr][x+2]*256+DSR[dsr][x+3];
                int len = DSR[dsr][x+4];
                memset(buf, 0, sizeof(buf));
                for (i=0; i<len; ++i) {
                    buf[i] = DSR[dsr][x+5+i];
                }
                debug_write("  >%04X >%04X : %s", next, run, buf);
                x = next-0x4000;
            } while (x>0);
        }

        x = DSR[dsr][8]*256+DSR[dsr][9]-0x4000;
        if (x > 0) {
            debug_write("DSR List:");
            do {
                char buf[257];
                int next = DSR[dsr][x]*256+DSR[dsr][x+1];
                int run = DSR[dsr][x+2]*256+DSR[dsr][x+3];
                int len = DSR[dsr][x+4];
                memset(buf, 0, sizeof(buf));
                for (i=0; i<len; ++i) {
                    buf[i] = DSR[dsr][x+5+i];
                }
                debug_write("  >%04X >%04X : %s", next, run, buf);
                x = next-0x4000;
            } while (x>0);
        }

        x = DSR[dsr][10]*256+DSR[dsr][11]-0x4000;
        if (x > 0) {
            debug_write("Subprogram List:");
            do {
                char buf[257];
                int next = DSR[dsr][x]*256+DSR[dsr][x+1];
                int run = DSR[dsr][x+2]*256+DSR[dsr][x+3];
                int len = DSR[dsr][x+4];
                memset(buf, 0, sizeof(buf));
                if (len == 1) {
                    // this isn't necessarily guaranteed, but covers current needs
                    sprintf(buf, ">%02X", DSR[dsr][x+5]);
                    // special hack to verify Classic99's DSR
                    if (dsr == 1) {
                        int code = (run - 0x4800)/2;
                        if (code != DSR[dsr][x+5]) {
                            strcat(buf, " (WRONG)");
                        }
                    }
                } else {
                    for (i=0; i<len; ++i) {
                        buf[i] = DSR[dsr][x+5+i];
                    }
                }
                debug_write("  >%04X >%04X : %s", next, run, buf);
                x = next-0x4000;
            } while (x>0);
        }

        x = DSR[dsr][12]*256+DSR[dsr][13]-0x4000;
        if (x > 0) {
            debug_write("ISR Chain:");
            do {
                int next = DSR[dsr][x]*256+DSR[dsr][x+1];
                int run = DSR[dsr][x+2]*256+DSR[dsr][x+3];
                debug_write("  >%04X >%04X", next, run);
                x = next-0x4000;
            } while (x>0);
        }
    }
#endif

    if (bEnableAppMode) {
        // patch the boot GROMs as appropriate
        // graphics are at 0x950
        const unsigned char hlLogo[] = {
            0x01,0x03,0x04,0x0c,0x14,0x03,0x21,0x02,
            0xfc,0x83,0x80,0x80,0x41,0x20,0xd8,0x24,
            0x00,0x00,0xc0,0x20,0x7e,0xc4,0x74,0x2a,
            0x22,0x02,0x22,0x06,0x28,0x10,0x30,0x18,
            0xd2,0x12,0xd1,0x89,0x08,0x05,0x04,0x05,
            0x22,0x22,0x1c,0x08,0x88,0x08,0x88,0x10,
            0x08,0x06,0x01,0x00,0x01,0x01,0x02,0x07,
            0x06,0xfc,0xd5,0xae,0x58,0xa0,0x80,0x00,
            0xa0,0x40,0x80,0x00,0x00,0x00,0x00,0x00
        };
        memcpy(&GROMBase[0].GROM[0x950], hlLogo, 9*8);
        // This text is in the unused space
        // TEXAS INSTRUMENTS -> POWERED BY
        memcpy(&GROMBase[0].GROM[0x1800], "POWERED  BY", 11);
        memcpy(&GROMBase[0].GROM[0x1900], "CLASSIC99", 9);
        // TODO: move this line up to the 'press any key' line from where it is
        memcpy(&GROMBase[0].GROM[0x1A00], "HTTP://HARMLESSLION.COM", 23);

        // patch the startup code to draw it
        GROMBase[0].GROM[0x16A]=11;     // powered by - change length and start
        GROMBase[0].GROM[0x16c]=0x2b;
        GROMBase[0].GROM[0x16d]=0x18;
        GROMBase[0].GROM[0x16e]=0x00;

        GROMBase[0].GROM[0x178]=9;     // classic99 - change length and start
        GROMBase[0].GROM[0x17a]=0x6c;
        GROMBase[0].GROM[0x17b]=0x19;
        GROMBase[0].GROM[0x17c]=0x00;

        GROMBase[0].GROM[0x171]=23;     // url - change length and start
        GROMBase[0].GROM[0x173]=0xc5;
        GROMBase[0].GROM[0x174]=0x1A;
        GROMBase[0].GROM[0x175]=0x00;

        // patch the menu code, in case the author chose not to autostart
        GROMBase[0].GROM[0x265]=11;     // powered by - change length
        GROMBase[0].GROM[0x268]=0x18;
        GROMBase[0].GROM[0x269]=0x00;

        GROMBase[0].GROM[0x26c]=9;      // classic99 - change length
        GROMBase[0].GROM[0x26f]=0x19;
        GROMBase[0].GROM[0x270]=0x00;

        // if skip title is set, patch the GROM to do that
        if (bSkipTitle) {
            // skip the initial beep (replace with a clr vdp@>0000)
            GROMBase[0].GROM[0xb5]=0x86;
            GROMBase[0].GROM[0xb6]=0xa0;
            GROMBase[0].GROM[0xb7]=0x00;

            // skip the format to post keypress (br >017d)
            GROMBase[0].GROM[0x115]=0x41;
            GROMBase[0].GROM[0x116]=0x7d;

            // skip the keypress wait
            GROMBase[0].GROM[0x1b1]=0x41;
            GROMBase[0].GROM[0x1b2]=0xb7;
        }

        // if autostart is set, patch the GROM to do that
        if (nAutoStartCart) {
            // skip the formats
            GROMBase[0].GROM[0x249]=0x42;
            GROMBase[0].GROM[0x24a]=0x71;

            // patch out the index number (4 bytes)
            GROMBase[0].GROM[0x295]=0x86;
            GROMBase[0].GROM[0x296]=0x74;
            GROMBase[0].GROM[0x297]=0x86;
            GROMBase[0].GROM[0x298]=0x74;

            // patch out the FOR text
            GROMBase[0].GROM[0x29d]=1;
            GROMBase[0].GROM[0x2a0]=0x18;
            GROMBase[0].GROM[0x2a1]=0x08;

            // 8 bytes to patch out writing the cart name to the screen
            // replacing it with clr @>8374 four times
            GROMBase[0].GROM[0x2c6]=0x86;
            GROMBase[0].GROM[0x2c7]=0x74;
            GROMBase[0].GROM[0x2c8]=0x86;
            GROMBase[0].GROM[0x2c9]=0x74;
            GROMBase[0].GROM[0x2ca]=0x86;
            GROMBase[0].GROM[0x2cb]=0x74;
            GROMBase[0].GROM[0x2cc]=0x86;
            GROMBase[0].GROM[0x2cd]=0x74;

            // 7 bytes for an alternate copy (last is a scan)
            GROMBase[0].GROM[0x2d9]=0x86;
            GROMBase[0].GROM[0x2da]=0x74;
            GROMBase[0].GROM[0x2db]=0x86;
            GROMBase[0].GROM[0x2dc]=0x74;
            GROMBase[0].GROM[0x2dd]=0x86;
            GROMBase[0].GROM[0x2de]=0x74;
            GROMBase[0].GROM[0x2df]=0x03;

            // overwrite the scan with ST xx at @>8375 (BE 45 xx)
            // we can overwrite the SUB, that makes it easier
            // if it's invalid, well, you're going to get a lot of beeping ;)
            GROMBase[0].GROM[0x2F9] = 0xBE;
            GROMBase[0].GROM[0x2FA] = 0x75;
            GROMBase[0].GROM[0x2FB] = nAutoStartCart+0x30;  // make ASCII minus 1

#if 0
            // and mute the accept beep, we just ALL >20, SCAN
            // why does the console crash with this one?
            GROMBase[0].GROM[0x30A] = 0x07;
            GROMBase[0].GROM[0x30b] = 0x20;
            GROMBase[0].GROM[0x30c] = 0x03;
#endif
        }
    }
}

extern unsigned char UberEEPROM[4*1024];
void saveroms()
{
	// if there is a cart plugged in, see if there is any NVRAM to save
	if (nCart != -1) {
		struct CARTS *pBank=NULL;

 		switch (nCartGroup) {
		case 0:	
			if (nCart < 100) {
				pBank=Apps; 
			}
			break;
		case 1: 
			if (nCart < 100) {
				pBank=Games; 
			}
			break;
		case 2: 
			if (nCart < nTotalUserCarts) {
				pBank=Users; 
			}
			break;
		}

		if ((pBank)&&(nvRamUpdated)) {
			for (int idx=0; idx<MAXROMSPERCART; idx++) {
				if (pBank[nCart].Img[idx].nType == TYPE_NVRAM) {
					// presumably all the data is correct, length and so on
					FILE *fp = fopen(pBank[nCart].Img[idx].szFileName, "wb");
					if (NULL == fp) {
						debug_write("Failed to write NVRAM file '%s', code %d", pBank[nCart].Img[idx].szFileName, errno);
					} else {
						unsigned char buf[8192];
						debug_write("Saving NVRAM file '%s', addr >%04X, len >%04X", pBank[nCart].Img[idx].szFileName, pBank[nCart].Img[idx].nLoadAddr, pBank[nCart].Img[idx].nLength);
						ReadMemoryBlock(pBank[nCart].Img[idx].nLoadAddr, buf, min(sizeof(buf), pBank[nCart].Img[idx].nLength));
						fwrite(buf, 1, pBank[nCart].Img[idx].nLength, fp);
						fclose(fp);
					}
				} else if (pBank[nCart].Img[idx].nType == TYPE_UBER_EEPROM) {
					// presumably all the data is correct, length and so on
					FILE *fp = fopen(pBank[nCart].Img[idx].szFileName, "wb");
					if (NULL == fp) {
						debug_write("Failed to write UBER_EEPROM file '%s', code %d", pBank[nCart].Img[idx].szFileName, errno);
					} else {
						debug_write("Saving UBER EEPROM file '%s', addr >%04X, len >%04X", pBank[nCart].Img[idx].szFileName, pBank[nCart].Img[idx].nLoadAddr, pBank[nCart].Img[idx].nLength);
						fwrite(UberEEPROM, 1, min(sizeof(UberEEPROM), pBank[nCart].Img[idx].nLength), fp);
						fclose(fp);
					}
                }
			}
		}
	}

	// reset the value
	nvRamUpdated = false;
}

//////////////////////////////////////////////////////////
// Interpret a single instruction
// Note: op_X doesn't come here anymore as this function
// is too busy for the recursive call.
//////////////////////////////////////////////////////////
void do1()
{
	// TODO: instead of doing ALL the keyboard checks EVERY instruction, maybe
	// we can split some of them off to a rotating check? Only breakpoints need
	// to be checked EVERY instruction, and even then, maybe not EVERY.

	// used for emulating idle and halts (!READY) better
	bool nopFrame = false;

	// handle end of frame processing (some emulator, some hardware)
	if (end_of_frame)
	{
		pCurrentCPU->ResetCycleCount();

		// put this before setting the draw event to reduce conflict, though it makes it a frame behind
		if (Recording) {
			WriteFrame();
		}

		int nNumFrames = retrace_count / (drawspeed+1);	// get count so we can update counters (ignore remainder)
		if (fJoystickActiveOnKeys > 0) {
			fJoystickActiveOnKeys -= nNumFrames;
			if (fJoystickActiveOnKeys < 0) {
				fJoystickActiveOnKeys = 0;
			}
		}
		cpuframes+=nNumFrames;
		retrace_count-=nNumFrames*(drawspeed+1);
		timercount+=nNumFrames*(drawspeed+1);

		end_of_frame=0;								// No matter what, this tick is passed!
	}

	if (pCurrentCPU == pCPU) {
		// Somewhat better 9901 timing - getting close! (Actually this looks pretty good now)
		// The 9901 timer decrements every 64 periods, with the TI having a 3MHz clock speed
		// Thus, it decrements 46875 times per second. If CRU bit 3 is on, we trigger an
		// interrupt when it expires. 
        // TODO: so if the timer is starting at zero, which I assume it does (though it's not
        // clear, can we write a test program to find out?), then I assume the decrementer will
        // wrap around. This is an oddball case but we'll try it.
        int nTimerCnt=CRUTimerTicks>>6;		// /64
		if (nTimerCnt) {
            if (timer9901 == 0) {
                // handle the wraparound.. since we /started/ at
                // zero, we didn't yet count down TO it
                // TODO: I have not confirmed whether starttimer9901 should start at 0 or 0x3fff,
                //       if it's not 0, then this may be off by 1 tick on the initialized console.
                // 14 bit timer
			    timer9901=0x4000 - nTimerCnt;
            } else {
    			timer9901-=nTimerCnt;
            }
			CRUTimerTicks-=(nTimerCnt<<6);	// *64

			if (timer9901 < 1) {
				timer9901=starttimer9901+timer9901;
                timer9901 &= 0x3fff;    // only 14 bits!
// 				debug_write("9901 timer expired, int requested");
				timer9901IntReq=1;	
			}

            // it's less stress on the emulator to do this on entry to clock mode,
            // but it's more correct here. Decisions, decisions...
            if (CRU[0] != 1) {
                // transfer the timer when not in clock mode
                timer9901Read = timer9901;
            }
		}

		// Check if the VDP or CRU wants an interrupt (99/4A has only level 1 interrupts)
		// When we have peripheral card interrupts, they are masked on CRU[1]
		if ((((VDPINT)&&(CRU[2]))||((timer9901IntReq)&&(CRU[3]))) && ((pCurrentCPU->GetST()&0x000f) >= 1) && (!skip_interrupt)) {
//			if (cycles_left >= 22) {					// speed throttling
				pCurrentCPU->TriggerInterrupt(0x0004,2);    // TODO: what level do I want to throw here? They are all mask 2, right?
//			}
            // the if cycles_left doesn't work because if we don't take it now, we'll
            // execute other instructions (at least one more!) instead of stopping...
            // so we'll just take it and pay for it later.
		}

		// If an idle is set
		if ((pCurrentCPU->GetIdle())||(pCurrentCPU->GetHalt())) {
			nopFrame = true;
		}
	}

    // Make control by itself stop talking, but only if no other key was pressed. So it will stop on release.
    static int ctrlState = 0;   // 0=not pressed, 1=down, 2=cancelled
    if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
        if (ctrlState == 0) ctrlState = 1;
    } else {
        if (ctrlState == 1) {
            // ctrl was pressed and released with no other key - stop talking
            debug_write("Control pressed and released - stop talking.");
            ScreenReader::ShutUp();
        }
        ctrlState = 0;
    }

    // some shortcut keys that are always active...
    // these all require control to be active
    // launch debug dialog (with control)
	if (key[VK_HOME]) 
	{
		if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
            if (ctrlState) ctrlState = 2;

		    if (NULL == dbgWnd) {
			    PostMessage(myWnd, WM_COMMAND, ID_EDIT_DEBUGGER, 0);
			    // the dialog focus switch may cause a loss of the up event, so just fake it now
			    // TODO: this should not be needed with the filter in decode(), right??
			    decode(0xe0);	// extended key
			    decode(0xf0);
			    decode(VK_HOME);
		    }
    		key[VK_HOME]=0;
        }
	}

	// edit->paste (with control)
	if (key[VK_F1]) {
    	if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
            if (ctrlState) ctrlState = 2;
            PostMessage(myWnd, WM_COMMAND, ID_EDITPASTE, 0);
            key[VK_F1] = 0;
        }
    }

    // copy screen to clipboard (with control)
	if (key[VK_F2]) {
        // we'll try to use the screen offset byte - 0x83d3
        // we'll explicitly check for only 0x60, otherwise 0
    	if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
            if (ctrlState) ctrlState = 2;
            PostMessage(myWnd, WM_COMMAND, ID_EDIT_COPYSCREEN, 0);
            key[VK_F2]=0;
        }
	}

	// read screen once (with control)
	if (key[VK_F4]) {
    	if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
            if (ctrlState) ctrlState = 2;
			ScreenReader::ReadScreenOnce();
            key[VK_F4]=0;
        }
	}

	// toggle continuous screen reader (with control)
	if (key[VK_F9]) {
    	if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
            if (ctrlState) ctrlState = 2;
			PostMessage(myWnd, WM_COMMAND, ID_SCREENREADER_CONTINUOUS, 0);
            key[VK_F9]=0;
        }
	}

	// stop talking (with control)
	if (key[VK_F10]) {
    	if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
            if (ctrlState) ctrlState = 2;
			ScreenReader::ShutUp();
            key[VK_F10]=0;
        }
	}

	// check alt+f4 if enabled
	if (enableAltF4) {
		if (GetAsyncKeyState(VK_MENU)&0x8000) {
			if (key[VK_F4]) {
				PostMessage(myWnd, WM_QUIT, 0, 0);
				key[VK_F4] = 0;
			}
		}
	}
	// check F10 menu if enabled
	if (enableF10Menu) {
		if (key[VK_F10]) {
			// This magic sequence from https://stackoverflow.com/questions/256719/how-to-programmatically-activate-the-menu-in-windows-mobile
			PostMessage((HWND)myWnd, WM_SYSCOMMAND, SC_KEYMENU, 0);
			key[VK_F10] = 0;
		}
	}
	
	// speedKeys doesn't include slow because slow is too slow to be useful
	// to a non-debugger. Those people can change the CPU speed in config instead.
	// ... someone will complain someday. ;) 5/5/2021
	if (enableSpeedKeys) {
		// CPU normal
		if (key[VK_F6]) {
			key[VK_F6]=0;
			SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_NORMAL, 0);
		}

		// cpu overdrive
		if (key[VK_F7]) {
			key[VK_F7]=0;
			SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUOVERDRIVE, 0);
		}

		// system maximum
		if (key[VK_F8]) {
			key[VK_F8]=0;
			SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_SYSTEMMAXIMUM, 0);
		}

		// toggle turbo
		if (key[VK_F11]) {
			key[VK_F11] = 0;

			if (ThrottleMode == THROTTLE_SYSTEMMAXIMUM) {
				// running in fast forward, return to normal
				ThrottleMode = THROTTLE_NORMAL;
				DoPlay();
			}
			else {
				DoFastForward();
			}
		}
	} // speedkeys

	// Control keys - active only with the debug view open in PS/2 mode
	// nopFrame must be set before now!
	if ((!gDisableDebugKeys) && (NULL != dbgWnd) && (!nopFrame)) {
		// breakpoint handling
		if ((max_cpf > 0) && (nStepCount == 0)) {
			// used for timing
			static unsigned long nFirst=0;
			static unsigned long nMax=0, nMin=0xffffffff;
			static int nCount=0;
			static unsigned long nTotal=0;
			Word PC = pCurrentCPU->GetPC();

			for (int idx=0; idx<nBreakPoints; idx++) {
				switch (BreakPoints[idx].Type) {
					case BREAK_PC:
						if (CheckRange(idx, PC)) {
							TriggerBreakPoint();
						}
						break;

					// timing instead of breakpoints
                    // TODO: multiple timers, proper memory placement
					case BREAK_RUN_TIMER:
						if ((BreakPoints[idx].Bank != -1) && (xbBank != BreakPoints[idx].Bank)) {
							break;
						}
						if (PC == BreakPoints[idx].A) {
							nFirst=total_cycles;
                            cycleCountOn=true;
                            // hate this hack
                            if (gResetTimer) {
                                gResetTimer = false;
			                    nMax=0;
                                nMin=0xffffffff;
			                    nCount=0;
			                    nTotal=0;
                                memset(cycleCounter, 0, sizeof(cycleCounter));
                            }
						} else if (PC == BreakPoints[idx].B) {
                            cycleCountOn = false;
							if (nFirst!=0) {
								if (total_cycles<nFirst) {
									debug_write("Counter Wrapped, no statistics");
								} else {
									unsigned long nTime=total_cycles-nFirst;
									if (nTime > 0) {
										if (nTime>nMax) nMax=nTime;
										if (nTime<nMin) nMin=nTime;
										nCount++;
										nTotal+=nTime;
										if (nTotal<nTime) {
											nTotal=0;
											nCount=0;
										} else {
											debug_write("Timer: %u CPU cycles - Min: %u  Max: %u  Average(%u): %u", nTime, nMin, nMax, nCount, nTotal/nCount);
										}
									}
								}
								nFirst=0;
							}
						}
						break;
				}
			}

			// check for the step over
			if (bStepOver) {
				// if return address wasn't set by the last instruction, just stop
				// otherwise, we have to wait for it. The address +2 covers the case
				// of a subroutine that takes a single data operand as arguments, so modifies
				// the return address.
				if ((pCurrentCPU->GetReturnAddress() == 0) || (PC == pCurrentCPU->GetReturnAddress()) || (PC == pCurrentCPU->GetReturnAddress()+2)) {
					bStepOver=false;
					TriggerBreakPoint();
				}
			}
		}

		// pause/play
		if (key[VK_F1]) {
            // in the case where the current CPU can not break but the other can, just
            // hold onto this key...
            if (!pCurrentCPU->enableDebug) {
                if ((pCPU->enableDebug)||(pGPU->enableDebug)) {
                    // just wait for the context switch
                    return;
                } else {
                    // no debugging, so discard the key
                    debug_write("Neither processor is enabled to breakpoint!");
        			key[VK_F1]=0;
                    return;
                }
            }

			if (0 == max_cpf) {
				// already paused, restore
				DoPlay();
			} else {
				// running normal, so pause
				DoPause();
			}
			key[VK_F1]=0;
		}
		
		// step
		if (key[VK_F2]) {
			DoStep();
			key[VK_F2]=0;
		}

		// step over
		if (key[VK_F3]) {
			DoStepOver();
			key[VK_F3]=0;
		}

		// do automatic screenshot, unfiltered (ctrl), or set CPU slow (normal)
		if (key[VK_F5]) {
			key[VK_F5]=0;

			if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
                if (ctrlState) ctrlState = 2;
				SaveScreenshot(true, false);
			} else {
				SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUSLOW, 0);
			}
		}

		// auto screenshot, filtered (ctrl), or CPU normal (normal)
		if (key[VK_F6]) {
			key[VK_F6]=0;

			if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
                if (ctrlState) ctrlState = 2;
				SaveScreenshot(true, true);
			} else {
				SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_NORMAL, 0);
			}
		}

		// toggle sprites (ctrl) / cpu overdrive (normal)
		if (key[VK_F7]) {
			key[VK_F7]=0;

			if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
                if (ctrlState) ctrlState = 2;
				SendMessage(myWnd, WM_COMMAND, ID_LAYERS_DISABLESPRITES, 0);
			} else {
				SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUOVERDRIVE, 0);
			}
		}

		// toggle background (ctrl) / system maximum (normal)
		if (key[VK_F8]) {
			key[VK_F8]=0;

			if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
                if (ctrlState) ctrlState = 2;
				SendMessage(myWnd, WM_COMMAND, ID_LAYERS_DISABLEBACKGROUND, 0);
			} else {
				SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_SYSTEMMAXIMUM, 0);
			}
		}

		// tell VDP to draw character set
		if (key[VK_F9]) {
			VDPDebug=!VDPDebug;
			key[VK_F9]=0;
			redraw_needed=REDRAW_LINES;
		}

		// dump main memory
		if (key[VK_F10]) {
			key[VK_F10]=0;
			DoMemoryDump();
		}
		
		// toggle turbo (same as speedkeys)
		if (key[VK_F11]) {
			key[VK_F11] = 0;

			if (ThrottleMode == THROTTLE_SYSTEMMAXIMUM) {
				// running in fast forward, return to normal
				DoPlay();
			}
			else {
				DoFastForward();
			}
		}

		// LOAD interrupt or RESET (ctrl)
		if (key[VK_F12]) {
			key[VK_F12]=0;
			
			if (GetAsyncKeyState(VK_CONTROL)&0x8000) {
                if (ctrlState) ctrlState = 2;
				SendMessage(myWnd, WM_COMMAND, ID_FILE_RESET, 0);
			} else {
				DoLoadInterrupt();
			}
		}
	}

	bool bOperate = true;
	if (cycles_left <= 0) bOperate=false;
	// force run of the other CPU if we're stepping
	if ((cycles_left == 1) && (!pCurrentCPU->enableDebug)) bOperate=true;

	if (bOperate) {
		cpucount++;					// increment instruction counter (TODO: just remove, pointless today)
 
		////////////////// System Patches - not active for GPU

		if (pCurrentCPU == pCPU) {
			// Keyboard hacks - requires original 99/4A ROMS!
			// Now we can check for all three systems - we look for the
			// address where KSCAN stores the character at >8375
			// longer buffer hack is enabled ONLY for system 1 (TI-99/4A)
			if ( ((nSystem == 0) && (pCurrentCPU->GetPC()==0x356)) ||
			 ((nSystem == 1) && (pCurrentCPU->GetPC()==0x478)) ||
			 ((nSystem == 2) && (pCurrentCPU->GetPC()==0x478)) ) {
				if (NULL != PasteString) {
					static int nOldSpeed = THROTTLE_NONE;

					if (nOldSpeed == THROTTLE_NONE) {
						// set overdrive during pasting, then go back to normal
						nOldSpeed = ThrottleMode;
						SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUOVERDRIVE, 0);
					}

					if (PasteString==PasteIndex) SetWindowText(myWnd, "Classic99 - Pasting (ESC Cancels)");	

					if (key[VK_ESCAPE]) {
						// the rest of the cleanup will happen below
						*PasteIndex='\0';
					}

                    // TODO: all the writes in here will have CPU timing implications, but pasting happens
                    // in overdrive anyway, so I guess it doesn't matter
					if ((rcpubyte(0x8374, ACCESS_FREE)==0)||(rcpubyte(0x8374, ACCESS_FREE)==5)) {		// Check for pastestring - note keyboard is still active
						if (*PasteIndex) {
							if (*PasteIndex==10) {
								// CRLF to CR, LF to CR
								if ((PasteIndex==PasteString)||(*(PasteIndex-1)!=13)) {
									*PasteIndex=13;
								}
							}

							if (PasteCount==0) {
								if (((*PasteIndex>31)&&(*PasteIndex<127))||(*PasteIndex==13)) {
									if (nSystem == 0) {
										// TI-99/4 code is different - it expects to get the character
										// from GROM, so we need to hack 8375 after it's written
										Word WP = pCurrentCPU->GetWP();
										wcpubyte(0x8375, toupper(*PasteIndex));
										wcpubyte(WP, rcpubyte(0x837c, ACCESS_FREE) | 0x20);	/* R0 must contain the status byte */
									} else {
										if ((PasteStringHackBuffer)&&(nSystem==1)) {	// for normal TI-99/4A only - need to verify for 2.2
											wcpubyte(0x835F, 0x5d);		// max length for BASIC continuously set - infinite string length! Use with care!
										}
										Word WP = pCurrentCPU->GetWP();
										wcpubyte(WP, *PasteIndex);					/* set R0 (byte) with keycode */
										wcpubyte(WP+12, rcpubyte(0x837c, ACCESS_FREE) | 0x20);	/* R6 must contain the status byte (it gets overwritten) */
									}
								}
								if (PasteCount<1) {
									wcpubyte(0x837c, rcpubyte(0x837c, ACCESS_FREE)|0x20);
								}
								PasteCount++;
								PasteIndex++;
							} else {
								// no response - key up!
								PasteCount=0;
							}
						}
					
						if ('\0' == *PasteIndex) {
							PasteIndex=NULL;
							free(PasteString);
							PasteString=NULL;
							PasteStringHackBuffer=false;
							SetWindowText(myWnd, szDefaultWindowText);

							switch (nOldSpeed) {
								default:
								case THROTTLE_NORMAL:
									SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_NORMAL, 0);
									break;
								case THROTTLE_SLOW:
									SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUSLOW, 0);
									break;
								case THROTTLE_OVERDRIVE:
									SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUOVERDRIVE, 0);
									break;
								case THROTTLE_SYSTEMMAXIMUM:
									SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_SYSTEMMAXIMUM, 0);
									break;
							}
							nOldSpeed = THROTTLE_NONE;
						}
					}
				}
			}

			// intercept patches for Classic99 DSK1 emulation
			// These patches handle the custom DSR:
			// >4800 - DSK or clipboard file access
			// >4810 - DSK powerup
			// >4820->482C - SBRLNK - Special subprograms
			// NOTE: Classic99 DSR must not use 0x5FF0-0x5FFF - used for TI disk controller hardware
			if ((nCurrentDSR == 1) && (nDSRBank[1] == 0) && (pCurrentCPU->GetPC() >= 0x4800) && (pCurrentCPU->GetPC() <= 0x5FEF)) {
				Word WP = pCurrentCPU->GetWP();
				bool bRet = HandleDisk();
				// the disk system may have switched in the TI disk controller, in which case we
				// will actually execute code instead of faking in. So in that case, don't return!
				if (nDSRBank[1] == 0) {
					if (bRet) {
						// if all goes well, increment address by 2
						// Note the powerup routine won't do that :)
						// return address in R11.. TI is a bit silly
						// and if we don't increment by 2, it's considered
						// an error condition
						wrword(WP+22, romword(WP+22)+2);
					}
					pCurrentCPU->SetPC(romword(WP+22));
				}
				return;
			}
			// turn off TI disk DSR if no longer active
			if ((nDSRBank[1] > 0) && ((nCurrentDSR != 1)||(pCurrentCPU->GetPC() < 0x4000))) nDSRBank[1] = 0;
			// check for sector access hook
			if ((nCurrentDSR == 1) && (nDSRBank[1] > 0) && (pCurrentCPU->GetPC() == 0x40e8)) {
				HandleTICCSector();
			}
		}
        // Check for TIPISim access hook
		// These patches handle the custom DSR
		// NOTE: TIPISIM DSR must not use 0x5FF8-0x5FFF - used for TIPI interface hardware
		if ((nCurrentDSR == 2) && (pCurrentCPU->GetPC() >= 0x4800) && (pCurrentCPU->GetPC() <= 0x5FF8)) {
			Word WP = pCurrentCPU->GetWP();
			bool bRet = HandleTIPI();
			if (bRet) {
				// if all goes well, increment address by 2
				// Note the powerup routine won't do that :)
				// return address in R11.. TI is a bit silly
				// and if we don't increment by 2, it's considered
				// an error condition
                // TODO: not sure if this is true for CALL, but CALL
                // can just return false in that case
				wrword(WP+22, romword(WP+22)+2);
			}
			pCurrentCPU->SetPC(romword(WP+22)); // basically this is a B*R11
            return;
		}

        EnterCriticalSection(&csDisasm);
		    if ((!nopFrame) && ((!bStepOver) || (nStepCount))) {
			    if (pCurrentCPU->enableDebug) {
				    // Update the disassembly trace
				    memmove(&Disasm[0], &Disasm[1], (DEBUGLINES-1)*sizeof(Disasm[0]));	// TODO: really should be a ring buffer
				    Disasm[DEBUGLINES-1].pc=pCurrentCPU->GetPC();
				    if (pCurrentCPU == pGPU) {
					    Disasm[DEBUGLINES-1].bank = -1;
				    } else {
					    Disasm[DEBUGLINES-1].bank = xbBank;
				    }
			    }
			    // will fill in cycles below
		    }

		    // disasm running log
            // TODO: doesn't support GPU run - we can add that with a second file handle
            if ((NULL != fpDisasm) && (pCurrentCPU == pCPU)) {
                int pc = pCurrentCPU->GetPC();
                if ((disasmLogType == 0) || (pc >= 0x2000)) {
			        char buf[1024];
			        sprintf(buf, "%04X   ", pc);
			        Dasm9900(&buf[5], pCurrentCPU->GetPC(), xbBank);
			        fprintf(fpDisasm, "(%d) %s\n", xbBank, buf);
                }
            }
        LeaveCriticalSection(&csDisasm);

		// TODO: is this true? Is the LOAD interrupt disabled when the READY line is blocked?
		if ((pCurrentCPU == pCPU) && (!nopFrame)) {
			// this is a bad pattern, this repeated pCurrentCPU == pCPU, we can clean this up a lot
			if ((doLoadInt)&&(!skip_interrupt)) {
				pCurrentCPU->TriggerInterrupt(0xfffc,0);    // non maskable, what does that do to the interrupt level?
				doLoadInt=false;
				// load interrupt also releases IDLE
				pCurrentCPU->StopIdle();
			}
		}

        Word oldWP = pCurrentCPU->GetWP();
        Word oldST = pCurrentCPU->GetST();
		Word in = pCurrentCPU->ExecuteOpcode(nopFrame);

        if (pCurrentCPU == pCPU) {
            updateTape(pCurrentCPU->GetCycleCount());
			updateDACBuffer(pCurrentCPU->GetCycleCount());
			// count down the skip
            if (skip_interrupt > 0) --skip_interrupt;

            // update VDP too
			updateVDP(pCurrentCPU->GetCycleCount());

			// and check for VDP address race
			if (vdpwroteaddress > 0) {
				vdpwroteaddress -= pCurrentCPU->GetCycleCount();
				if (vdpwroteaddress < 0) vdpwroteaddress=0;
			}

			// some debug help for DSR overruns
			// TODO: this is probably not quite right (AMS?), but should work for now
			int top = (staticCPU[0x8370] << 8) + staticCPU[0x8371];
			if (top != filesTopOfVram) {
				if ((pCurrentCPU->GetPC() < 0x4000) || (pCurrentCPU->GetPC() > 0x5FFF)) {
					// top of VRAM changed, not in a DSR, so write a warning
					// else this is in a DSR, so we'll assume it's legit
					debug_write("(Non-DSR) Top of VRAM pointer at >8370 changed to >%04X by PC >%04X", top, pCurrentCPU->GetPC());
				}
				updateCallFiles(top);
			}
		}

		if ((!nopFrame) && ((!bStepOver) || (nStepCount))) {
			if (pCurrentCPU->enableDebug) {
                EnterCriticalSection(&csDisasm);
    				Disasm[DEBUGLINES-1].cycles = pCurrentCPU->GetCycleCount();
                    if (cycleCountOn) {
                        cycleCounter[Disasm[DEBUGLINES-1].pc] += Disasm[DEBUGLINES-1].cycles;
                    }
                LeaveCriticalSection(&csDisasm);
			}
		}

		if ((!nopFrame) && (nStepCount > 0)) {
			nStepCount--;
		}

		if (pCurrentCPU == pCPU) {
			// Slow down autorepeat using a timer - requires 99/4A GROM
			// We check if the opcode was "MOVB 2,*3+", (PC >025e, but we don't assume that) 
			// the 99/4A keyboard is on, and the GROM Address
			// has been incremented to the next instruction (IE: we just incremented the
			// repeat counter). If so, we only allow the increment at a much slower rate
			// based on the interrupt timer (for real time slowdown).
			// This doesn't work in XB!
			if ((ThrottleMode > THROTTLE_NORMAL) && (slowdown_keyboard) && (in == 0xdcc2) && ((keyboard==KEY_994A)||(keyboard==KEY_994A_PS2)) && (GROMBase[0].GRMADD == 0x2a62)) {
				if ((ticks%10) != 0) {
					WriteMemoryByte(0x830D, ReadMemoryByte(0x830D, ACCESS_FREE) - 1, false);
				}
			} // todo: ELSE??
			// but this one does (note it will trigger for ANY bank-switched cartridge that uses this code at this address...)
			if ((ThrottleMode > THROTTLE_NORMAL) && (slowdown_keyboard) && (in == 0xdcc2) && ((keyboard==KEY_994A)||(keyboard==KEY_994A_PS2)) && (GROMBase[0].GRMADD == 0x6AB6) && (xb)) {
				if ((ticks%10) != 0) {
					WriteMemoryByte(0x8300, ReadMemoryByte(0x8300, ACCESS_FREE) - 1, false);
				}
			}
		}

		if (pCurrentCPU == pCPU) {
			int nLocalCycleCount = pCurrentCPU->GetCycleCount();
			InterlockedExchangeAdd((LONG*)&cycles_left, -nLocalCycleCount);
			unsigned long old=total_cycles;
			InterlockedExchangeAdd((LONG*)&total_cycles, nLocalCycleCount);
			if ((old&0x80000000)&&(!(total_cycles&0x80000000))) {
				total_cycles_looped=true;
				speech_cycles=total_cycles;
			} else {
				// at 5 times per frame that's 300 updates per second, which at 8khz is 26.6 samples
				if ((max_cpf > 0) && (SPEECHUPDATETIMESPERFRAME > 0)) {
					if (total_cycles - speech_cycles >= (unsigned)(max_cpf/SPEECHUPDATETIMESPERFRAME)) {
						while (total_cycles - speech_cycles >= (unsigned)(max_cpf/SPEECHUPDATETIMESPERFRAME)) {
							static int nCnt=0;
							int nSamples=26;
							// should only be once
							++nCnt;
							if (nCnt > 2) {	// handle 2/3
								nCnt=0;
							} else {
								nSamples++;
							} 
							SpeechUpdate(nSamples);
							speech_cycles+=(max_cpf/SPEECHUPDATETIMESPERFRAME);
						}
					}
				}
			}

			CRUTimerTicks+=nLocalCycleCount;

			// see if we can resolve a halt condition
			int bits = pCurrentCPU->GetHalt();
			if (bits) {
				// speech is blocking
				if (bits & (1<<HALT_SPEECH)) {
					// this will automatically unlock it if it is accepted
					// 0x9400 is the speech base address
					wspeechbyte(0x9400, CPUSpeechHaltByte);
				}

				// that's all we have
			}
		}

		pCurrentCPU->ResetCycleCount();

        // check for breakpoints on WP and ST
        Word newWP = pCurrentCPU->GetWP();
        Word newST = pCurrentCPU->GetST();
		for (int idx=0; idx<nBreakPoints; idx++) {
			switch (BreakPoints[idx].Type) {
				case BREAK_WP:
				    if ((newWP != oldWP) && ((pCurrentCPU->GetWP()&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    					    TriggerBreakPoint();
                        }
				    }
					break;
				case BREAK_ST:
				    if ((newST != oldST) && ((pCurrentCPU->GetST()&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    					    TriggerBreakPoint();
                        }
				    }
					break;
            }
        }

		if ((!nopFrame) && (bDebugAfterStep)) {
			bDebugAfterStep=false;
			draw_debug();
			redraw_needed = REDRAW_LINES;
		}
	} else {
		// Go to sleep till it's time to work again, timeout wait after 50ms
		// (so if we're very slow or the event dies, we keep running anyway)
		if (bDebugAfterStep) {
			bDebugAfterStep=false;
			draw_debug();
			redraw_needed = REDRAW_LINES;
		}

		WaitForSingleObject(hWakeupEvent, 50);
	}
}

// "legally" update call files from the DSR
void updateCallFiles(int newTop) {
	filesTopOfVram = newTop;
}

// verify the top of VRAM pointer so we can warn about disk problems
// this is called when a DSR is executed
void verifyCallFiles() {
	int nTop = (staticCPU[0x8370]<<8) | staticCPU[0x8371];

	if (nTop > 0x3be3) {
		// there's not enough memory for even 1 disk buffer, so never mind
		return;
	}

	// validate the header for disk buffers - P-Code Card crashes with Stack Overflow without these
	// user programs can have this issue too, of course, if they use the TI CC
	if ((VDP[nTop+1] != 0xaa) ||	// valid header
		(VDP[nTop+2] != 0x3f) ||	// top of RAM, MSB (not validating LSB since CF7 can change it)**
		(VDP[nTop+4] != 0x11) ||	// CRU of disk controller (TODO: we assume >1100 today)
		(VDP[nTop+5] > 9)) {		// number of files, we support 0-9
									// ** - actually the previous value of highest free address from 8370
			char buf[256];
			sprintf(buf, "Invalid file buffer header at >%04X. Bytes >%02X,>%02X,>%02X,>%02X,>%02X",
				nTop, VDP[nTop+1], VDP[nTop+2], VDP[nTop+3], VDP[nTop+4], VDP[nTop+5]);
			debug_write(buf);
			if (BreakOnDiskCorrupt) {
				TriggerBreakPoint();
				MessageBox(myWnd, "Disk access is about to crash - the emulator has been paused. Check debug log for details.", "Classic99 Error", MB_OK);
			}
	}
}

//////////////////////////////////////////////////////
// Read a single byte from CPU memory
//////////////////////////////////////////////////////
Byte rcpubyte(Word x,READACCESSTYPE rmw) {
	// TI CPU memory map
	// >0000 - >1fff  Console ROM
	// >2000 - >3fff  Low bank RAM
	// >4000 - >5fff  DSR ROMs
	// >6000 - >7fff  Cartridge expansion
	// >8000 - >9fff  Memory-mapped devices & CPU scratchpad
	// >a000 - >ffff  High bank RAM
	// 
	// All is fine and dandy, except the memory-mapped devices, and the
	// fact that writing to the cartridge port with XB/379 in place causes
	// a bank switch. In this emulator, that will only happen if 'xb'
	// is greater than 0. A custom DSR will be written to be loaded which
	// will appear to the "TI" to support all valid devices, and it will
	// be loaded into the DSR space.

    // TODO HACK FOR PCODE RELOCATE
//    if ((x<0x6000)&&(x>0x3fff)&&(nCurrentDSR==-1)) {
//        TriggerBreakPoint();
//    }

	// no matter what kind of access, update the heat map
	UpdateHeatmap(x);

	if (rmw == ACCESS_READ) {
		// Check for read or access breakpoints
		for (int idx=0; idx<nBreakPoints; idx++) {
			switch (BreakPoints[idx].Type) {
				case BREAK_ACCESS:
				case BREAK_READ:
					if (CheckRange(idx, x)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    						TriggerBreakPoint();
                        }
					}
					break;
			}
		}
    }

	switch (x & 0xe000) {
		case 0x8000:
			switch (x & 0xfc00) {
				case 0x8000:				// scratchpad RAM - 256 bytes repeating.
					// no wait states
					return ReadMemoryByte(x | 0x0300, rmw);	// I map it all to >83xx
				case 0x8400:				// Don't read the sound chip (can hang a real TI? maybe only on early ones?)
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					return 0;
				case 0x8800:				// VDP read data
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					if (x&1) {
						// don't respond on odd addresses
						return 0;
					}
					return(rvdpbyte(x,rmw));
				case 0x8c00:				// VDP write data
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					if (x&1) {
						// don't respond on odd addresses
						return 0;
					}
					return 0;
				case 0x9000:				// Speech read data
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					if (x&1) {
						// don't respond on odd addresses
						return 0;
					}
                    // timing handled in rspeechbyte
					return(rspeechbyte(x));
				case 0x9400:				// Speech write data
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					if (x&1) {
						// don't respond on odd addresses
						return 0;
					}
					return 0;
				case 0x9800:				// read GROM data
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					if (x&1) {
						// don't respond on odd addresses
						return 0;
					}
					{
						// always access console GROMs to keep them in sync
						Byte nRet = rgrmbyte(x,rmw);
						return nRet;
					}
				case 0x9c00:				// write GROM data
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					return 0;
				default:					// We shouldn't get here, but just in case...
					if (rmw != ACCESS_FREE) {
						if ((x & 0x01) == 0) {					// this is a wait state
							pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
																// be right now that the CPU emulation does all Word accesses
						}
					}
					return 0;
			}
		case 0x0000:					// console ROM
			// no wait states
			return ReadMemoryByte(x, rmw);

		case 0x2000:					// normal CPU RAM
		case 0xa000:					// normal CPU RAM
		case 0xc000:					// normal CPU RAM
			if ((rmw != ACCESS_FREE)&&(!Fast16BitRam)) {	// also check 16-bit RAM flag. TODO: this also affects AMS!!
				if ((x & 0x01) == 0) {					// this is a wait state
					pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
														// be right now that the CPU emulation does all Word accesses
				}
			}
			return ReadMemoryByte(x, rmw);

		case 0xe000:					// normal CPU RAM
#ifdef USE_GIGAFLASH
			// TODO: not sure how Fast16BitRam would affects this. Probably shouldn't.
            readE000(x,rmw);            // but never returns anything valid
#endif
			if ((rmw != ACCESS_FREE)&&(!Fast16BitRam)) {	// also check 16-bit RAM flag. TODO: this also affects AMS!!
				if ((x & 0x01) == 0) {					// this is a wait state
					pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
														// be right now that the CPU emulation does all Word accesses
				}
			}
			return ReadMemoryByte(x, rmw);

		case 0x4000:					// DSR ROM (with bank switching and CRU)
			if (rmw != ACCESS_FREE) {
				if ((x & 0x01) == 0) {					// this is a wait state
					pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
														// be right now that the CPU emulation does all Word accesses
				}
			}
			if (ROMMAP[x]) {			
				// someone loaded ROM here, override the DSR system
				return ReadMemoryByte(x, rmw);
			}

			if (-1 == nCurrentDSR) return 0;

			// special case for P-Code Card GROM addresses
			if (nCurrentDSR == 0xf) {
				if ((x&1) == 0) {
					// don't respond on odd addresses (confirmed)
					switch (x) {
						case 0x5bfc:		// read grom data
						case 0x5bfe:		// read grom address
							return(rpcodebyte(x));

						case 0x5ffc:		// write data
						case 0x5ffe:		// write address
							return 0;
					}
				}
				// any other case, fall through
			}

			// SAMS support
			if (nCurrentDSR == 0xe) {
				// registers are selected by A11 through A14 when in
				// the >4000->5FFF range, but this function does a little
				// extra shifting itself. (We may want more bits later for
				// Thierry's bigger AMS card hack, but not for now).
				if (MapperRegistersEnabled()) {
					// 0000 0000 000x xxx0
					Byte reg = (x & 0x1e) >> 1;
					bool hiByte = ((x & 1) == 0);		// 16 bit registers!
					return ReadMapperRegisterByte(reg, hiByte);
				}
				return 0;
			}

			// TI Disk controller, if active
			if ((nCurrentDSR == 0x01) && (nDSRBank[1] > 0) && (x>=0x5ff0) && (x<=0x5fff)) {
				return ReadTICCRegister(x);
			}

			// RS232/PIO
			if (nCurrentDSR == 0x3) {
				// currently we aren't mapping any ROM space - this will change!
				return ReadRS232Mem(x-0x4000);
			}

            // CF7
            if ((nCurrentDSR == 0x00) && (csCf7Bios.GetLength() > 0) && (x>=0x5e00) && (x<0x5f00)) {
                return read_cf7(x);
            }

            // Ramdisk
            if ((nCurrentDSR == 0x00) && (csRamDisk.GetLength() > 0)) {
                return read_ramdisk(x);
            }

            // just access memory
			if (nDSRBank[nCurrentDSR]) {
				return DSR[nCurrentDSR][x-0x2000];	// page 1: -0x4000 for base, +0x2000 for second page
			} else {
				return DSR[nCurrentDSR][x-0x4000];	// page 0: -0x4000 for base address
			}
			break;

		case 0x6000:					// cartridge ROM
			if (rmw != ACCESS_FREE) {
				if ((x & 0x01) == 0) {					// this is a wait state
					pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
														// be right now that the CPU emulation does all Word accesses
				}
			}
#ifdef USE_GIGAFLASH
        {
            Byte xx = read6000(x,rmw);
            return xx;
        }
#endif
#ifdef USE_BIG_ARRAY
			if (BIGARRAYSIZE > 0) {
				// TODO BIG HACK - FAKE CART HARDWARE FOR VIDEO TEST
				if (x == 0x7fff) {
					if (BIGARRAYADD >= BIGARRAYSIZE) {
						// TODO: real hardware probably won't do this either.
						BIGARRAYADD = 0;
					}
					Byte ret = BIGARRAY[BIGARRAYADD++];
					return ret;
				}
				if (x == 0x7ffb) {
					// destructive address read - MSB first for consistency
					Byte ret = (BIGARRAYADD >> 24) & 0xff;
					BIGARRAYADD <<= 8;
					debug_write("(Read) Big array address now 0x%08X", BIGARRAYADD);
					return ret;
				}
				if (x == 0x6000) {
					// TODO: real hardware probably will not do this. Don't count on the address being reset.
					debug_write("Reset big array address");
					BIGARRAYADD = 0;
				}
				// END TODO
			}
#endif
			if (!bUsesMBX) {
				// XB is supposed to only page the upper 4k, but some Atari carts seem to like it all
				// paged. Most XB dumps take this into account so only full 8k paging is implemented.
				if (xb) {
                    // make sure xbBank never exceeds xb
					return(CPU2[(xbBank<<13)+(x-0x6000)]);	// cartridge bank 2 and up
				} else {
					return ReadMemoryByte(x, rmw);			// cartridge bank 1
				}
			} else {
				// MBX is weird. The lower 4k is fixed, but the top 1k of that is RAM
				// The upper 4k is bank switched. Address >6FFE has a bank switch
				// register updated from the data bus.
				if ((x>=0x6C00)&&(x<0x6FFE)) {
					return mbx_ram[x-0x6c00];				// MBX RAM
				} else if (x < 0x6c00) {
					return CPU2[x-0x6000];					// MBX fixed ROM
				} else {
					return(CPU2[(xbBank<<13)+(x-0x6000)]);	// MBX paged ROM	// TODO: isn't this 8k paging? Why does this work? What do the ROMs look like?
				}
				// anything else is ignored
			}
			break;

		default:						// We shouldn't get here, but just in case...
			if (rmw != ACCESS_FREE) {
				if ((x & 0x01) == 0) {					// this is a wait state
					pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
														// be right now that the CPU emulation does all Word accesses
				}
			}
			return 0;
	}
}

//////////////////////////////////////////////////////////
// Write a byte to CPU memory
//////////////////////////////////////////////////////////
void wcpubyte(Word x, Byte c)
{
	// no matter what kind of access, update the heat map
	UpdateHeatmap(x);

//    if ((x>=0x6000)&&(x<0x8000)) {
//        debug_write("Cartridge bank switch >%04X = >%02X", x, c);
//    }

	// Check for write or access breakpoints
	for (int idx=0; idx<nBreakPoints; idx++) {
		switch (BreakPoints[idx].Type) {
			case BREAK_ACCESS:
			case BREAK_WRITE:
				if (CheckRange(idx, x)) {
                    if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    					TriggerBreakPoint();
                    }
				}
				break;

			case BREAK_DISK_LOG:
				if (CheckRange(idx, x)) {
					if ((BreakPoints[idx].Data>0)&&(BreakPoints[idx].Data<10)) {
						if (NULL != DumpFile[BreakPoints[idx].Data]) {
							fputc(c, DumpFile[BreakPoints[idx].Data]);
						}
					}
				}
				break;
		}
	}

	if ((x & 0x01) == 0) {					// this is a wait state (we cancel it below for ROM and scratchpad)
		pCurrentCPU->AddCycleCount(4);		// we can't do half of a wait, so just do it for the even addresses. This should
											// be right now that the CPU emulation does all Word accesses
	}

#ifndef USE_GIGAFLASH
    // check for cartridge banking
	if ((x>=0x6000)&&(x<0x8000)) {
#ifdef USE_BIG_ARRAY
		if ((x == 0x7ffd) && (BIGARRAYSIZE > 0)) {
			// write the address register
			BIGARRAYADD = (BIGARRAYADD<<8) | c;
			debug_write("(Write) Big array address now 0x%08X", BIGARRAYADD);
			goto checkmem;
		} else
#endif
		
		if ((xb) && (ROMMAP[x])) {		// trap ROM writes and check for XB bank switch
            // collect bits from address and data buses - x is address, c is data
            int bits = (c<<13)|(x&0x1fff);
			if (bInvertedBanks) {
				// uses inverted address lines!
				xbBank=(((~bits)>>1)&xb);		// XB bank switch, up to 4096 banks
			} else if (bUsesMBX) {
				// MBX is weird. The lower 4k is fixed, but the top 1k of that is RAM
				// The upper 4k is bank switched. Address >6FFE has a bank switch
				// register updated from the data bus. (Doesn't use 'bits')
				if ((x>=0x6C00)&&(x<0x6FFE)) {
					mbx_ram[x-0x6c00] = c;
				} else if (x==0x6ffe) {
					xbBank = c&xb;
					// the theory is this also writes to RAM
					mbx_ram[x-0x6c00] = c;
				}
				// anything else is ignored
			} else {
				xbBank=(((bits)>>1)&xb);		// XB bank switch, up to 4096 banks
			}
			goto checkmem;
		}
		// else it's RAM there
	}
#endif

	switch (x & 0xe000) {
		case 0x8000:
			switch (x & 0xfc00) {
				case 0x8000:				// scratchpad RAM - 256 bytes repeating.
				if ((x & 0x01) == 0) {		// never mind for scratchpad
					pCurrentCPU->AddCycleCount(-4);		// we can't do half of a wait, so just do it for the even addresses. This should
														// be right now that the CPU emulation does all Word accesses
				}
				WriteMemoryByte((x | 0x0300), c, false);  // I map it all to >83xx
				break;
			case 0x8400:				// Sound write data
				if (x&1) {
					// don't respond on odd addresses
					return;
				}
				wsndbyte(c);
                // sound chip writes eat ~28 additional cycles (verified hardware, reads do not)
				pCurrentCPU->AddCycleCount(28);
				break;
			case 0x8800:				// VDP read data
				break;
			case 0x8c00:				// VDP write data
				if (x&1) {
					// don't respond on odd addresses
					return;
				}
				wvdpbyte(x,c);
				break;
			case 0x9000:				// Speech read data
				break;
			case 0x9400:				// Speech write data
				if (x&1) {
					// don't respond on odd addresses
					return;
				}
                // timing handled in wspeechbyte
				wspeechbyte(x,c);
				break;
			case 0x9800:				// read GROM data
				break;
			case 0x9c00:				// write GROM data
				if (x&1) {
					// don't respond on odd addresses
					return;
				}
				wgrmbyte(x,c);
				break;
			default:					// We shouldn't get here, but just in case...
				break;
		}
		break;

		case 0x0000:					// console ROM
			if ((x & 0x01) == 0) {		// never mind for ROM
				pCurrentCPU->AddCycleCount(-4);		// we can't do half of a wait, so just do it for the even addresses. This should
													// be right now that the CPU emulation does all Word accesses
			}
			// fall through
		case 0x2000:					// normal CPU RAM
		case 0xa000:					// normal CPU RAM
		case 0xc000:					// normal CPU RAM
			if (!ROMMAP[x]) {
				WriteMemoryByte(x, c, false);
			}
			break;

        case 0xe000:					// normal CPU RAM
#ifdef USE_GIGAFLASH
            writeE000(x,c);
#endif
			if (!ROMMAP[x]) {
				WriteMemoryByte(x, c, false);
			}
			break;

		case 0x6000:					// Cartridge RAM (ROM is trapped above)
#ifdef USE_GIGAFLASH
            write6000(x,c);
#endif
			// but we test it anyway. Just in case. ;) I think the above is just for bank switches
			if (!ROMMAP[x]) {
				WriteMemoryByte(x, c, false);
				nvRamUpdated = true;
			}
			break;

		case 0x4000:					// DSR ROM
			switch (nCurrentDSR) {
			case -1:
				// no DSR, so might be SID card
				if (NULL != write_sid) {
                    int reg = (x-0x5800)/2;
                    if (reg < 29) SidCache[reg] = c;
					write_sid(x, c);
				}
				break;

            case 0x0:
                // CF7, if it's loaded
                if (csCf7Bios.GetLength() > 0) {
                    write_cf7(x, c);
                } else if (csRamDisk.GetLength() > 0) {
                    // Ramdisk, if it's loaded
                    return write_ramdisk(x, c);
                }
                break;

			case 0x1:
				// TI Disk controller, if switched in
				if (nDSRBank[1] > 0) {
					WriteTICCRegister(x, c);
				}
				break;
			
			case 0x3:
				// RS232/PIO card
				{
					// currently we aren't mapping any ROM space - this will change!
					WriteRS232Mem(x-0x4000, c);
					goto checkmem;
				}
				break;

			case 0xe:
				// SAMS support
				{
					// registers are selected by A11 through A14 when in
					// the >4000->5FFF range, but this function does a little
					// extra shifting itself. (We may want more bits later for
					// Thierry's bigger AMS card hack, but not for now).
					if (MapperRegistersEnabled()) {
						// 0000 0000 000x xxx0
						Byte reg = (x & 0x1e) >> 1;
						bool hiByte = ((x & 1) == 0);	// registers are 16 bit!
						WriteMapperRegisterByte(reg, c, hiByte, false);
					}
					return;
				}
				break;

			case 0xf:
				// special case for P-Code Card GROM addresses
				{
					if ((x&1) == 0) {
					// don't respond on odd addresses (confirmed)
						switch (x) {
							case 0x5bfc:		// read grom data
							case 0x5bfe:		// read grom address
								break;

							case 0x5ffc:		// write grom data
							case 0x5ffe:		// write grom address
  								wpcodebyte(x,c);
								goto checkmem;
						}
					}
					// any other case, fall through, but we currently treat all DSR memory as ROM
				}
				break;
			}
	}

checkmem:
	// check breakpoints against what was written to where
	for (int idx=0; idx<nBreakPoints; idx++) {
		switch (BreakPoints[idx].Type) {
			case BREAK_EQUALS_BYTE:
				if ((CheckRange(idx, x)) && ((c&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
                    if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    					TriggerBreakPoint();
                    }
				}
				break;
		}
	}
}

//////////////////////////////////////////////////////
// Read a byte from the speech synthesizer
//////////////////////////////////////////////////////
Byte rspeechbyte(Word x)
{
	Byte ret=0;

	if ((SpeechRead)&&(SpeechEnabled)) {
		ret=SpeechRead();
        // speech chip, if attached, reads eat 48 additional cycles (verified hardware)
		pCurrentCPU->AddCycleCount(48);
	}
	return ret;
}

//////////////////////////////////////////////////////
// Write a byte to the speech synthesizer
//////////////////////////////////////////////////////
void wspeechbyte(Word x, Byte c)
{
	static int cnt = 0;

	if ((SpeechWrite)&&(SpeechEnabled)) {
		if (!SpeechWrite(c, CPUSpeechHalt)) {
			if (!CPUSpeechHalt) {
//				debug_write("Speech halt triggered.");
				CPUSpeechHalt=true;
				CPUSpeechHaltByte=c;
				pCPU->StartHalt(HALT_SPEECH);
				cnt = 0;
			} else {
				cnt++;
			}
		} else {
			// must be unblocked!
			if (CPUSpeechHalt) {
//				debug_write("Speech halt cleared at %d cycles.", cnt*10);
			}
			// always clear it, just to be safe
			pCPU->StopHalt(HALT_SPEECH);
			CPUSpeechHalt = false;
			cnt = 0;
			// speech chip, if attached, writes eat 64 additional cycles (verified hardware)
			// but we don't eat those cycles if we are halted, to allow finer grain resolution
			// of the halt...?
			pCurrentCPU->AddCycleCount(64);
		}
	}
}

//////////////////////////////////////////////////////
// Speech Update function - runs every x instructions
// Pass in number of samples to process.
// This is called 300 times per second
//////////////////////////////////////////////////////
void SpeechUpdate(int nSamples) {
	if ((speechbuf==NULL) || (SpeechProcess == NULL)) {
		// nothing to write to, so don't bother to halt
		CPUSpeechHalt=false;
		pCPU->StopHalt(HALT_SPEECH);
		return;
	}

	if (nSpeechTmpPos+nSamples >= SPEECHRATE*2) {
		// in theory, this should not happen, though it may if we
		// run the 9900 very fast
//			debug_write("Speech buffer full... dropping");
		return;
	}

	EnterCriticalSection(&csSpeechBuf);
	SpeechProcess((unsigned char*)&SpeechTmp[nSpeechTmpPos], nSamples);
	nSpeechTmpPos+=nSamples;
	LeaveCriticalSection(&csSpeechBuf);
}

// this is called 60 times per second
void SpeechBufferCopy() {
	DWORD iRead, iWrite;
	Byte *ptr1, *ptr2;
	DWORD len1, len2;
	static DWORD lastWrite=0;

	// mixing with the main audio buffer
	return;

	if (nSpeechTmpPos == 0) {
		// no data to write
		return;
	}

	EnterCriticalSection(&csSpeechBuf);

	// just for statistics
	speechbuf->GetCurrentPosition(&iRead, &iWrite);
	if (iWrite > lastWrite) {
		debug_write("Speech write gap of %d (read=%d, write=%d, expected %d)", iWrite-lastWrite, iRead, iWrite, lastWrite);
	} else if (iWrite < lastWrite) {
		debug_write("Speech write over of %d (read=%d, write=%d, expected %d)", lastWrite-iWrite, iRead, iWrite, lastWrite);
	} else {
		debug_write("Speech write ok");
	}
	lastWrite=iWrite+nSpeechTmpPos*2;	// this is where we ENDED last write

	// Note: flags make lock ignore the write cursor, but it should be about the same
	if (SUCCEEDED(speechbuf->Lock(iWrite, nSpeechTmpPos*2, (void**)&ptr1, &len1, (void**)&ptr2, &len2, 0 /*DSBLOCK_FROMWRITECURSOR*/))) 
	{
		memcpy(ptr1, SpeechTmp, len1);
		if (len2 > 0) {							// handle wraparound
			memcpy(ptr2, &SpeechTmp[len1/2], len2);
		}
		speechbuf->Unlock(ptr1, len1, ptr2, len2);	
		
		// reset the buffer
		nSpeechTmpPos=0;
	} else {
//		debug_write("Speech buffer lock failed");
		// don't reset the buffer, we may get it next time
	}

	LeaveCriticalSection(&csSpeechBuf);
}

//////////////////////////////////////////////////////
// Increment VDP Address
//////////////////////////////////////////////////////
void increment_vdpadd() 
{
	VDPADD=(++VDPADD) & 0x3fff;
}

//////////////////////////////////////////////////////
// Return the actual 16k address taking the 4k mode bit
// into account.
//////////////////////////////////////////////////////
int GetRealVDP() {
	int RealVDP;

	// force 4k drams (16k bit not emulated)
//	return VDPADD&0x0fff;

	// The 9938 and 9958 don't honor this bit, they always assume 128k (which we don't emulate, but we can at least do 16k like the F18A)
	// note that the 128k hack actually does support 128k now... as needed. So if bEnable128k is on, we take VDPREG[14]&0x07 for the next 3 bits.
	// (VDPREG[1]&0x80) is the 16k mode bit. Technically the F18A should ignore it too.
	if ((bEnable80Columns) || (VDPREG[1]&0x80)) {
		// 16k mode - address is 7 bits + 7 bits, so use it raw
		// xx65 4321 0654 3210
		// This mask is not really needed because VDPADD already tracks only 14 bits
		RealVDP = VDPADD;	// & 0x3FFF;

		if (bEnable128k) {
			RealVDP|=(VDPREG[14]&0x07)<<14;
		}
		
	} else {
		// 4k mode -- address is 6 bits + 6 bits, but because of the 16k RAMs,
		// it gets padded back up to 7 bit each for row and col
		// The actual method used is a little complex to describe (although
		// I'm sure it's simple in silicon). The lower 6 bits are used as-is.
		// The next /7/ bits are rotated left one position.. not really sure
		// why they didn't just do a 6 bit shift and lose the top bit, but
		// this does seem to match every test I throw at it now. Finally, the
		// 13th bit (MSB for the VDP) is left untouched. There are no fixed bits.
		// Test values confirmed on real console:
		// 1100 -> 0240
		// 1810 -> 1050
		// 2210 -> 2410
		// 2211 -> 2411
		// 2240 -> 2280
		// 3210 -> 2450
		// 3810 -> 3050
		// Of course, only after working all this out did I look at Sean Young's
		// document, which describes this same thing from the hardware side. Those
		// notes confirm mine.
		//
		//         static bits       shifted bits           rotated bit
		RealVDP = (VDPADD&0x203f) | ((VDPADD&0x0fc0)<<1) | ((VDPADD&0x1000)>>6);
		// thanks to JasonACT for spotting a bug in this math ;)
	}

	// force 8k DRAMs (strip top row bit - this should be right - console doesn't work though)
//	RealVDP&=0x1FFF;

// To watch the console VDP RAM detect code
//	if (GROMBase[0].GRMADD < 0x100) {
//		debug_write("VDP Address prefetch %02X from %04X, real address %04X", vdpprefetch, VDPADD-1, RealVDP-1);
//	}

	return RealVDP;
}

//////////////////////////////////////////////////////////////
// Read from VDP chip
//////////////////////////////////////////////////////////////
Byte rvdpbyte(Word x, READACCESSTYPE rmw)
{ 
	unsigned short z;

	if ((x>=0x8c00) || (x&1))
	{
		return(0);											// write address
	}

    if (pCurrentCPU->GetST()&0xf) {
        debug_write("Warning: PC >%04X reading VDP with LIMI %d", pCurrentCPU->GetPC(), pCurrentCPU->GetST()&0xf);
    }

	if (x&0x0002)
	{	/* read status */

		// This works around code that requires the VDP state to be able to change
		// DURING an instruction (the old code, the VDP state and the VDP interrupt
		// would both change and be recognized between instructions). With this
		// approach, we can update the VDP in the future, then run the instruction
		// against the updated VDP state. This allows Lee's fbForth random number
		// code to function, which worked by watching for the interrupt bit while
		// leaving interrupts enabled.
		updateVDP(-pCurrentCPU->GetCycleCount());

		// The F18A turns off DPM if any status register is read
		if ((bF18AActive)&&(bF18ADataPortMode)) {
			bF18ADataPortMode = 0;
			F18APaletteRegisterNo = 0;
			debug_write("F18A Data port mode off (status register read).");
		}

        // also make sure the DPM flipflop is reset
        F18APaletteRegisterData = -1;

        // Added by RasmusM
		if ((bF18AActive) && (F18AStatusRegisterNo > 0)) {
			return getF18AStatus();
		}
		// RasmusM added end
		z=VDPS;				// does not affect prefetch or address (tested on hardware)
		VDPS&=0x1f;			// top flags are cleared on read (tested on hardware)
		vdpaccess=0;		// reset byte flag

		// track polling for interrupt
		if (statusUpdateRead) {
			// new frame, first access since frame was reset by reading the status register, so remember when it was
			statusUpdateRead = false;
			statusReadLine = vdpscanline;
			// calculate scanlines since interrupt pulse
			if (vdpscanline > 192+27) {
				statusReadCount = 262*statusFrameCount + (vdpscanline-192-27);
			} else {
				statusReadCount = 262*statusFrameCount + (vdpscanline+(262-(192+27)));
			}
		}
		if (z & VDPS_INT) {
			// we cleared an interrupt, so go ahead and allow it again
			statusUpdateRead = true;
			statusFrameCount = 0;
		} 

		// TODO: hack to make Miner2049 work. If we are reading the status register mid-frame,
		// and neither 5S or F are set, return a random sprite index as if we were counting up.
		// Remove this when the proper scanline VDP is in. (Miner2049 cares about bit 0x02)
		// (This is still not valid to remove because sprites are still not processed in real time)
		if ((z&(VDPS_5SPR|VDPS_INT)) == 0) {
			// This search code borrowed from the sprite draw code
			int highest=31;
			int SAL=((VDPREG[5]&0x7f)<<7);

			// find the highest active sprite
			for (int i1=0; i1<32; i1++)			// 32 sprites 
			{
				if (VDP[SAL+(i1<<2)]==0xd0)
				{
					highest=i1-1;
					break;
				}
			}
			if (highest > 0) {
				//z=(z&0xe0)|(rand()%highest);
				z=(z&0xe0)|(highest);
			}
		}

		return((Byte)z);
	}
	else
	{ /* read data */
		int RealVDP;

		if ((vdpwroteaddress > 0) && (pCurrentCPU == pCPU)) {
			// todo: need some defines - 0 is top border, not top blanking
			if ((vdpscanline >= 13) && (vdpscanline < 192+13) && (VDPREG[1]&0x40)) {
				debug_write("Warning - may be reading VDP too quickly after address write at >%04X!", pCurrentCPU->GetPC());
				vdpwroteaddress = 0;
			}
		}

		// if we are reading, we are probably not clearing the screen, so clear the counter
		// this way scrolling a screen that is mostly empty won't look like a clear
		lastVideoByte = 255;
		lastVideoCount = 0;

		vdpaccess=0;		// reset byte flag (confirmed in hardware)
		RealVDP = GetRealVDP();
		UpdateHeatVDP(RealVDP);

		if (rmw == ACCESS_READ) {
			// Check for breakpoints
			for (int idx=0; idx<nBreakPoints; idx++) {
				switch (BreakPoints[idx].Type) {
					case BREAK_READVDP:
						if (CheckRange(idx, VDPADD-1)) {
                            if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
	    						TriggerBreakPoint();
                            }
						}
						break;
				}
			}
		}

		// VDP Address is +1, so we need to check -1
		if ((g_bCheckUninit) && (vdpprefetchuninited)) {
			TriggerBreakPoint();
			// we have to remember if the prefetch was initted, since there are other things it could have
			char buf[128];
			sprintf(buf, "Breakpoint - reading uninitialized VDP memory at >%04X (or other prefetch)", (RealVDP-1)&0x3fff);
			MessageBox(myWnd, buf, "Classic99 Debugger", MB_OK);
		}

		z=vdpprefetch;
		vdpprefetch=VDP[RealVDP];
		vdpprefetchuninited = (VDPMemInited[RealVDP] == 0);
		increment_vdpadd();
		return ((Byte)z);
	}
}

///////////////////////////////////////////////////////////////
// Write to VDP chip
///////////////////////////////////////////////////////////////
void wvdpbyte(Word x, Byte c)
{
	int RealVDP;
	
	if (x<0x8c00 || (x&1)) 
	{
        debug_write("INVALID VDP WRITE IGNORED TO ADDRESS >%04X (PC >%04X)", x, pCurrentCPU->GetPC());
		return;							/* not going to write at that block */
	}

    if (pCurrentCPU->GetST()&0xf) {
        debug_write("Warning: PC >%04X writing VDP with LIMI %d", pCurrentCPU->GetPC(), pCurrentCPU->GetST()&0xf);
    }

    if (x&0x0002)
	{	/* write address */
		// count down access cycles to help detect write address/read vdp overruns (there may be others but we don't think so!)
		// anyway, we need 8uS or we warn
		if (max_cpf > 0) {
			// TODO: this is still wrong. Since the issue is mid-instruction, we need to
			// count this down either at each phase, or calculate better what it needs
			// to be before the read. Where this is now, it will subtract the cycles from
			// this instruction, when it probably shouldn't. The write to the VDP will
			// happen just 4 (5?) cycles before the end of the instruction (multiplexer)
#if 0
			if (hzRate == HZ50) {
				vdpwroteaddress = (HZ50 * max_cpf) * 8 / 1000000;
			} else {
				vdpwroteaddress = (HZ60 * max_cpf) * 8 / 1000000;
			}
#endif
		}
		if (0 == vdpaccess) {
			// LSB (confirmed in hardware)
			VDPADD = (VDPADD & 0xff00) | c;
			vdpaccess = 1;
		} else {
			// MSB - flip-flop is reset and triggers action (confirmed in hardware)
			VDPADD = (VDPADD & 0x00FF) | (c<<8);
			vdpaccess = 0;

            // check if the user is probably trying to do DSR filename tracking
            // This is a TI disk controller side effect and shouldn't be relied
            // upon - particularly since it involved investigating freed buffers ;)
            if (((VDPADD == 0x3fe1)||(VDPADD == 0x3fe2)) && (GetSafeCpuWord(0x8356,0) == 0x3fe1)) {
                debug_write("Software may be trying to track filenames using deleted TI VDP buffers... (>8356)");
                if (BreakOnDiskCorrupt) TriggerBreakPoint();
            }

            // check what to do with the write
			if (VDPADD&0x8000) { 
				int nReg = (VDPADD&0x3f00)>>8;
				int nData = VDPADD&0xff;

				if (bF18Enabled) {
					if ((nReg == 57) && (nData == 0x1c)) {
						// F18A unlock sequence? Supposed to be twice but for now we'll just take equal
						// TODO: that's hacky and it's wrong. Fix it. 
						if (VDPREG[nReg] == nData) {	// but wait -- isn't this already verifying twice? TODO: Double-check procedure
							bF18AActive = true;
							debug_write("F18A Enhanced registers unlocked.");
						} else {
							VDPREG[nReg] = nData;
						}
						return;
					}
				} else {
					// this is hacky ;)
					bF18AActive = false;
				}
				if (bF18AActive) {
					// check extended registers. 
					// TODO: the 80 column stuff below should be included in the F18 specific stuff, but it's not right now

					// The F18 has a crapload of registers. But I'm only interested in a few right now, the rest can fall through

					// TODO: a lot of these side effects need to move to wVDPReg
					// Added by RasmusM
					if (nReg == 15) {
						// Status register select
						//debug_write("F18A status register 0x%02X selected", nData & 0x0f);
						F18AStatusRegisterNo = nData & 0x0f;
						return;
					}
					if (nReg == 47) {
						// Palette control
						bF18ADataPortMode = (nData & 0x80) != 0;
						bF18AAutoIncPaletteReg = (nData & 0x40) != 0;
						F18APaletteRegisterNo = nData & 0x3f;
						F18APaletteRegisterData = -1;
						if (bF18ADataPortMode) {
							debug_write("F18A Data port mode on.");
						}
						else {
							debug_write("F18A Data port mode off.");
						}
						return;
					}
					if (nReg == 49) {
                        VDPREG[nReg] = nData;

                        // Enhanced color mode
						F18AECModeSprite = nData & 0x03;
						F18ASpritePaletteSize = 1 << F18AECModeSprite;	
						debug_write("F18A Enhanced Color Mode 0x%02X selected for sprites", nData & 0x03);
						// TODO: read remaining bits: fixed tile enable, 30 rows, ECM tiles, real sprite y coord, sprite linking. 
						return;
					}
					// RasmusM added end

                    if (nReg == 50) {
                        VDPREG[nReg] = nData;
                        // TODO: other reg50 bits
                        // 0x01 - Tile Layer 2 uses sprite priority (when 0, always on top of sprites)
                        // 0x02 - use per-position attributes instead of per-name in text modes (DONE)
                        // 0x04 - show virtual scanlines (regardless of jumper)
                        // 0x08 - report max sprite (VR30) instead of 5th sprite
                        // 0x10 - disable all tile 1 layers (GM1,GM2,MCM,T40,T80)
                        // 0x20 - trigger GPU on VSYNC
                        // 0x40 - trigger GPU on HSYNC
                        // 0x80 - reset VDP registers to poweron defaults (DONE)
                        if (nReg & 0x80) {
                            debug_write("Resetting F18A registers");
                            vdpReset(false);
                        }
                    }

					if (nReg == 54) {
						// GPU PC MSB
						VDPREG[nReg] = nData;
						return;
					}
					if (nReg == 55) {
						// GPU PC LSB -- writes trigger the GPU
						VDPREG[nReg] = nData;
						pGPU->SetPC((VDPREG[54]<<8)|VDPREG[55]);
						debug_write("GPU PC LSB written, starting GPU at >%04X", pGPU->GetPC());
						pGPU->StopIdle();
						if (!bInterleaveGPU) {
							pCurrentCPU = pGPU;
						}
						return;
					}
					if (nReg == 56) {
						// GPU control register
						if (nData & 0x01) {
							if (pGPU->idling) {
								// going to run the code anyway to be sure, but only debug on transition
								debug_write("GPU GO bit written, starting GPU at >%04X", pGPU->GetPC());
							}
							pGPU->StopIdle();
							if (!bInterleaveGPU) {
								pCurrentCPU = pGPU;
							}
						} else {
							if (!pGPU->idling) {
								debug_write("GPU GO bit cleared, stopping GPU at >%04X", pGPU->GetPC());
							}
							pGPU->StartIdle();
							pCurrentCPU = pCPU;		// probably redundant
						}
						return;
					}
				}

				if (bEnable80Columns) {
					// active only when 80 column is enabled
					// special hack for RAM... good lord.
					if ((bEnable128k) && (nReg == 14)) {
						VDPREG[nReg] = nData&0x07;
						redraw_needed=REDRAW_LINES;
						return;
					}

				}

				if (bF18AActive) {
					nReg &= 0x3f;
					wVDPreg((Byte)(nReg),(Byte)(nData));
				} else {
					if (nReg&0xf8) {
						debug_write("Warning: writing >%02X to VDP register >%X ignored (PC=>%04X)", nData, nReg, pCPU->GetPC());
						if (bEnable80Columns) {
							// TODO: why do I need this? Does the F18A ignore regs 8-F when locked? Test case is TurboForth 80 column.
							return;
						}
					}
					// verified correct against real hardware - register is masked to 3 bits
					nReg &= 0x07;
					wVDPreg((Byte)(nReg),(Byte)(nData));
				}

				// and tell the VDP we need a full screen redraw
				redraw_needed=REDRAW_LINES;
			}

			// And the address remains set even when the target is a register
			if ((VDPADD&0xC000)==0) {	// prefetch inhibit? Verified on hardware - either bit inhibits.
				RealVDP = GetRealVDP();
				vdpprefetch=VDP[RealVDP];
				vdpprefetchuninited = (VDPMemInited[RealVDP] == 0);
				increment_vdpadd();
			} else {
				VDPADD&=0x3fff;			// writing or register, just mask the bits off
			}
		}
		// verified on hardware - write register does not update the prefetch buffer
	}
	else
	{	/* write data */
		// TODO: cold reset incompletely resets the F18 state - after TI Scramble runs we see a sprite on the master title page

		// Added by RasmusM
		// Write data to F18A palette registers
		if (bF18AActive && bF18ADataPortMode) {
			if (F18APaletteRegisterData == -1) {
				// Read first byte
				F18APaletteRegisterData = c;
			}
			else {
				// Read second byte
				{
					int r=(F18APaletteRegisterData & 0x0f);
					int g=(c & 0xf0)>>4;
					int b=(c & 0x0f);
					F18APalette[F18APaletteRegisterNo] = (r<<20)|(r<<16)|(g<<12)|(g<<8)|(b<<4)|b;	// double up each palette gun, suggestion by Sometimes99er
					redraw_needed = REDRAW_LINES;
				}
				debug_write("F18A palette register >%02X set to >%04X", F18APaletteRegisterNo, F18APalette[F18APaletteRegisterNo]);
				if (bF18AAutoIncPaletteReg) {
					F18APaletteRegisterNo++;
				}
				// The F18A turns off DPM after each register is written if auto increment is off
				// or after writing to last register if auto increment in on
				if ((!bF18AAutoIncPaletteReg) || (F18APaletteRegisterNo == 64)) {
					bF18ADataPortMode = 0;
					F18APaletteRegisterNo = 0;
					debug_write("F18A Data port mode off (auto).");
				}
				F18APaletteRegisterData = -1;
			}
            redraw_needed=REDRAW_LINES;
			return;
		}
		// RasmusM added end

		vdpaccess=0;		// reset byte flag (confirmed in hardware)

		RealVDP = GetRealVDP();
		UpdateHeatVDP(RealVDP);
		VDP[RealVDP]=c;
		VDPMemInited[RealVDP]=1;

		// before the breakpoint, check and emit debug if we messed up the disk buffers
		{
			int nTop = (staticCPU[0x8370]<<8) | staticCPU[0x8371];
			if (nTop <= 0x3be3) {
				// room for at least one disk buffer
				bool bFlag = false;

				if ((RealVDP == nTop+1) && (c != 0xaa)) {	bFlag = true;	}
				if ((RealVDP == nTop+2) && (c != 0x3f)) {	bFlag = true;	}
				if ((RealVDP == nTop+4) && (c != 0x11)) {	bFlag = true;	}
				if ((RealVDP == nTop+5) && (c > 9))		{	bFlag = true;	}

				if (bFlag) {
					debug_write("VDP disk buffer header corrupted at PC >%04X", pCurrentCPU->GetPC());
				}
			}
		}

		// check breakpoints against what was written to where - still assume internal address
		for (int idx=0; idx<nBreakPoints; idx++) {
			switch (BreakPoints[idx].Type) {
				case BREAK_EQUALS_VDP:
					if ((CheckRange(idx, VDPADD)) && ((c&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    						TriggerBreakPoint();
                        }
					}
					break;

				case BREAK_WRITEVDP:
					if (CheckRange(idx, VDPADD)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    						TriggerBreakPoint();
                        }
					}
					break;
			}
		}

		// check for probable screen clear for the speech system
		// we assume the SIT is set up - it should be by now!
		if (c == lastVideoByte) {
			if ((RealVDP>=SIT)&&(RealVDP<SIT+960)) {
				++lastVideoCount;
				// TODO: for some reason when entering TI BASIC, it only does 767 on the ALL and then starts reading?
				if (lastVideoCount >= 767) {
					ScreenReader::ClearHistory();
					lastVideoCount = 0;
				}
			}
		} else {
			lastVideoByte = c;
		}

		// verified on hardware
		vdpprefetch=c;
		vdpprefetchuninited = true;		// is it? you are reading back what you wrote. Probably not deliberate

		increment_vdpadd();
		redraw_needed=REDRAW_LINES;
	}
}

////////////////////////////////////////////////////////////////
// Write to VDP Register
////////////////////////////////////////////////////////////////
void wVDPreg(Byte r, Byte v)
{ 
	int t;

	if (r > 58) {
		debug_write("Writing VDP register more than 58 (>%02X) ignored...", r);
		return;
	}

	VDPREG[r]=v;

	// check breakpoints against what was written to where
	for (int idx=0; idx<nBreakPoints; idx++) {
		switch (BreakPoints[idx].Type) {
			case BREAK_EQUALS_VDPREG:
				if ((r == BreakPoints[idx].A) && ((v&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
                    if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    					TriggerBreakPoint();
                    }
				}
				break;
		}
	}

	if (r==7)
	{	/* color of screen, set color 0 (trans) to match */
		/* todo: does this save time in drawing the screen? it's dumb */
		t=v&0xf;
		if (t) {
			F18APalette[0]=F18APalette[t];
		} else {
			F18APalette[0]=0x000000;	// black
		}
		redraw_needed=REDRAW_LINES;
	}

	if (!bEnable80Columns) {
		// warn if setting 4k mode - the console ROMs actually do this often! However,
		// this bit is not honored on the 9938 and later, so is usually set to 0 there
		if ((r == 1) && ((v&0x80) == 0)) {
			// ignore if it's a console ROM access - it does this to size VRAM
			if (pCurrentCPU->GetPC() > 0x2000) {
				debug_write("WARNING: Setting VDP 4k mode at PC >%04X", pCurrentCPU->GetPC());
			}
		}
	}

	// check for speech clear on screen clear write
	// has to be here because the clear may be too fast for the speech
	// update to detect it.
	if ((r == 1)&&((v&0x40)==0)) {
		// screen blank
		ScreenReader::ClearHistory();
	}

	// for the F18A GPU, copy it to RAM
	VDP[0x6000+r]=v;
}

////////////////////////////////////////////////////////////////
// Write a byte to the sound chip
// Nice notes at http://www.smspower.org/maxim/docs/SN76489.txt
////////////////////////////////////////////////////////////////
void wsndbyte(Byte c)
{
	static int oldFreq[3]={0,0,0};						// tone generator frequencies

	if (NULL == lpds) return;

	// 'c' contains the byte currently being written to the sound chip
	// all functions are 1 or 2 bytes long, as follows					
	//
	// BYTE		BIT		PURPOSE											
	//	1		0		always '1' (latch bit)							
	//			1-3		Operation:	000 - tone 1 frequency				
	//								001 - tone 1 volume					
	//								010 - tone 2 frequency				
	//								011 - tone 2 volume					
	//								100 - tone 3 frequency				
	//								101 - tone 3 volume					
	//								110 - noise control					
	//								111 - noise volume					
	//			4-7		Least sig. frequency bits for tone, or volume	
	//					setting (0-F), or type of noise.				
	//					(volume F is off)								
	//					Noise set:	4 - always 0						
	//								5 - 0=periodic noise, 1=white noise 
	//								6-7 - shift rate from table, or 11	
	//									to get rate from voice 3.		
	//	2		0-1		Always '0'. This byte only used for frequency	
	//			2-7		Most sig. frequency bits						
	//
	// Commands are instantaneous

	// Latch anytime the high bit is set
	// This byte still immediately changes the channel
	if (c&0x80) {
		latch_byte=c;
	}

	switch (c&0xf0)										// check command
	{	
	case 0x90:											// Voice 1 vol
	case 0xb0:											// Voice 2 vol
	case 0xd0:											// Voice 3 vol
	case 0xf0:											// Noise volume
		setvol((c&0x60)>>5, c&0x0f);
		break;

	case 0xe0:
		setfreq(3, c&0x07);                             // Noise - get type
		break;

	case 0x80:											// Voice 1 frequency
	case 0xa0:											// Voice 2 frequency
	case 0xc0:											// Voice 3 frequency
    {
        int nChan=(latch_byte&0x60)>>5;     // was just set above
        oldFreq[nChan]&=0xfff0;
        oldFreq[nChan]|=c&0x0f;
		setfreq(nChan, oldFreq[nChan]);
    }
		break;
        
	default:											// Any other byte (ie: 0x80 not set)
    {
		int nChan=(latch_byte&0x60)>>5;
        // latch clear - data to whatever is latched
        // TODO: re-verify this on hardware, it doesn't agree with the SMS Power doc
        // as far as the volume and noise goes!
        if (latch_byte&0x10) {
            // volume register
            setvol(nChan, c&0x0f);
        } else if (nChan==3) {
            // noise register
            setfreq(3, c&0x07);
        } else {
            // tone generator - most significant bits
            oldFreq[nChan]&=0xf;
            oldFreq[nChan]|=(c&0x3f)<<4;
            setfreq(nChan, oldFreq[nChan]);
        }
    }
		break;
	}
}

//////////////////////////////////////////////////////////////////
// GROM base 0 (console GROMS) manage all address operations
//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
// Increment the GROM address - handle wraparound
//////////////////////////////////////////////////////////////////
void IncrementGROMAddress(Word &adrRef) {
    int base = adrRef&0xe000;
    adrRef = ((++adrRef)&0x1fff) | base;
}

//////////////////////////////////////////////////////////////////
// Read a data byte from GROM with no side effects (debugger)
// TODO: MPD and UberGROM may have side effects?
//////////////////////////////////////////////////////////////////
Byte ReadSafeGrom(int nBase, int adr) {
	Byte z;

	if ((Word)(adr) < 0x6000) {
		// console GROMs always respond
		nBase=0;
	}

	z=GROMBase[nBase].GROM[adr&0xffff];

	// a test for the Distorter project - special cases - GROM base is always 0 for console GROMs!
	if (bMpdActive) {
		z=GetMpdOverride(adr, z);
		// the rest of the MPD works like MESS does, copying data into the GROM array. Less efficient, better for debug though
	}
	if ((bUberGROMActive) && ((Word)(adr) >= 0x6000)) {
		z=UberGromRead(adr, nBase);
	}

	return(z);
}

//////////////////////////////////////////////////////////////////
// Read a byte from GROM
//////////////////////////////////////////////////////////////////
Byte ReadValidGrom(int nBase, Word x, bool sideEffects) {
	Byte z;

	// NOTE: Classic99 does not emulate the 6k GROM behaviour that causes mixed
	// data in the range between 6k and 8k - because Classic99 assumes all GROMS
	// are actually 8k devices. It will return whatever was in the ROM data it
	// loaded (or zeros if no data was loaded there). I don't intend to reproduce
	// this behaviour (but I can certainly conceive of using it for copy protection,
	// if only real GROMs could still be manufactured...)
    // TODO: actually, we can easily incorporate it by generating the extra 2k when we
    // load a 6k grom... but it would only work if we loaded 6k groms instead of
    // the combined banks lots of people use. But at least we'd do it.

	// the -1 accounts for the prefetch to get the data we're going to read
    // account for GROM wraparound
	int nPrevAddress = (((GROMBase[0].GRMADD&0x1fff)-1)&0x1fff) | (GROMBase[0].GRMADD&0xe000);
	if (((Word)(nPrevAddress) < 0x6000) && (!((bankedConsoleGROMs) && (grombanking)))) {
		// console GROMs always respond, unless the optional banked console GROMs is on AND we have loaded GROM banks
		nBase=0;
	}

//	if (nBase > 0) {
//		debug_write("Read GROM base %d(>%04X), >%04x, >%02x", nBase, x, GROMBase[0].GRMADD, GROMBase[nBase].grmdata);
//	}

	// Note that UberGROM access time (in the pre-release version) was 15 cycles (14.6), but it
	// does not apply as long as other GROMs are in the system (and they have to be due to lack
	// of address counter.) So this is still valid.

	if (x&0x0002)
	{
		// address
		GROMBase[0].grmaccess=2;
		z=(GROMBase[0].GRMADD&0xff00)>>8;
		// read is destructive
		GROMBase[0].GRMADD=(((GROMBase[0].GRMADD&0xff)<<8)|(GROMBase[0].GRMADD&0xff));		
		// TODO: Is the address incremented anyway? ie: if you keep reading, what do you get?

        // GROM read address always adds about 13 cycles
		pCurrentCPU->AddCycleCount(13);

		return(z);
	}
	else
	{
		// data
		UpdateHeatGROM(GROMBase[0].GRMADD);

        // this saves some debug off for Rich
        GROMBase[0].LastRead = GROMBase[0].GRMADD;
        GROMBase[0].LastBase = nBase;

		GROMBase[0].grmaccess=2;
		z=GROMBase[nBase].grmdata;

		// a test for the Distorter project - special cases - GROM base is always 0 for console GROMs!
		if (bMpdActive) {
			z=GetMpdOverride(nPrevAddress, z);
			// the rest of the MPD works like MESS does, copying data into the GROM array. Less efficient, better for debug though
		}
		if ((bUberGROMActive) && ((Word)(nPrevAddress) >= 0x6000)) {
			z=UberGromRead(nPrevAddress, nBase);
		}

		// update all bases prefetch
		for (int idx=0; idx<PCODEGROMBASE; idx++) {
			GROMBase[idx].grmdata=GROMBase[idx].GROM[GROMBase[0].GRMADD];
		}

        // TODO: This is not correct emulation for the gigacart, which ACTUALLY maintains
        // an 8-bit address latch and a 1 bit select (for GROM >8000)
        // But it's enough to let me test some theories...
        IncrementGROMAddress(GROMBase[0].GRMADD);

        // GROM read data always adds about 19 cycles
		pCurrentCPU->AddCycleCount(19);

		return(z);
	}
}

Byte rgrmbyte(Word x, READACCESSTYPE rmw)
{
	int nBank;

	if (x>=0x9c00)
	{
		return(0);										// write address
	}

	if (grombanking) {
		nBank=(x&0x3ff)>>2;								// maximum possible range to >9BFF - not all supported here though
		if (nBank >= PCODEGROMBASE) {
			debug_write("Warning: Invalid GROM base 0x%04X read", x);
		}
        // TODO: Classic99 always supports 16 GROM bases. Note that not all
        // carts will, so technically this mask needs to be configurable.
        nBank &= 0xf;
	} else {
		nBank=0;
	}

	if (rmw == ACCESS_READ) {
		// Check for breakpoints
		for (int idx=0; idx<nBreakPoints; idx++) {
			switch (BreakPoints[idx].Type) {
				case BREAK_READGROM:
					if (CheckRange(idx, GROMBase[0].GRMADD-1)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    						TriggerBreakPoint();
                        }
					}
					break;
			}
		}
	}

	return ReadValidGrom(nBank, x, true);
}

//////////////////////////////////////////////////////////////////
// Write a byte to GROM
//////////////////////////////////////////////////////////////////
void WriteValidGrom(int nBase, Word x, Byte c) {
//	if (nBase > 0) {
//		debug_write("Write GROM base %d(>%04X), >%04x, >%02x, %d", nBase, x, GROMBase[0].GRMADD, c, GROMBase[0].grmaccess);
//	}

	// Note that UberGROM access time (in the pre-release version) was 15 cycles (14.6), but it
	// does not apply as long as other GROMs are in the system (and they have to be due to lack
	// of address counter.) So this is still valid.

	if (x&0x0002)
	{
		GROMBase[0].GRMADD=(GROMBase[0].GRMADD<<8)|(c);						// write GROM address
		GROMBase[0].grmaccess--;
		if (GROMBase[0].grmaccess==0)
		{ 
			GROMBase[0].grmaccess=2;										// prefetch emulation

            // second GROM address write adds about 21 cycles (verified)
    		pCurrentCPU->AddCycleCount(21);
			
			// update MPD so it can reset if needed
			if (bMpdActive) {
				MpdHookNewAddress(GROMBase[0].GRMADD);
			}

			// update all bases prefetch
			for (int idx=0; idx<PCODEGROMBASE; idx++) {
				GROMBase[idx].grmdata=GROMBase[idx].GROM[GROMBase[0].GRMADD];
			}

            // TODO: This is not correct emulation for the gigacart, which ACTUALLY maintains
            // an 8-bit address latch and a 1 bit select (for GROM >8000)
            // But it's enough to let me test some theories...
            IncrementGROMAddress(GROMBase[0].GRMADD);
		} else {
            // first GROM address write adds about 15 cycles (verified)
    		pCurrentCPU->AddCycleCount(15);
        }

		// GROM writes do not affect the prefetches, and have the same
		// side effects as reads (they increment the address and perform a
		// new prefetch)
	}
	else
	{
		UpdateHeatGROM(GROMBase[0].GRMADD);

		// Check for breakpoints
		for (int idx=0; idx<nBreakPoints; idx++) {
			switch (BreakPoints[idx].Type) {
				case BREAK_WRITEGROM:
					if (CheckRange(idx, GROMBase[0].GRMADD-1)) {
                        if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
    						TriggerBreakPoint();
                        }
					}
					break;
			}
		}

		GROMBase[0].grmaccess=2;

		// Since all GRAM devices were hacks, they apparently didn't handle prefetch the same
		// way as I expected. Because of prefetch, the write address goes to the GROM address
		// minus one. Well, they were hacks in hardware, I'll just do a hack here.
        // note because we do real GROM wraparound now, that we need to assume the wraparound
        // is in the same GROM, not the entire space.
		int nRealAddress = (((GROMBase[0].GRMADD&0x1fff)-1)&0x1fff) | (GROMBase[0].GRMADD&0xe000);

        // MPD overrides the GRAM switch below
		if (bMpdActive) {
			MpdHookGROMWrite(nRealAddress, c);
		}
		if ((bUberGROMActive) && ((Word)(nRealAddress) >= 0x6000)) {
			UberGromWrite(nRealAddress, nBase, c);
		}
 
		if (GROMBase[0].bWritable[(nRealAddress&0xE000)>>13]) {
			// Allow it! The user is crazy! :)
			GROMBase[nBase].GROM[nRealAddress]=c;
		}
		// update all bases prefetch
		for (int idx=0; idx<PCODEGROMBASE; idx++) {
			GROMBase[idx].grmdata=GROMBase[idx].GROM[GROMBase[0].GRMADD];
		}
        // TODO: This is not correct emulation for the gigacart, which ACTUALLY maintains
        // an 8-bit address latch and a 1 bit select (for GROM >8000)
        // But it's enough to let me test some theories...
   		IncrementGROMAddress(GROMBase[0].GRMADD);

        // GROM data writes add about 22 cycles (verified)
   		pCurrentCPU->AddCycleCount(22);
	}
}


void wgrmbyte(Word x, Byte c)
{
	int nBank;

	if (x<0x9c00) 
	{
		return;											// read address
	}

	if (grombanking) {
		nBank=(x&0x3ff)>>2;								// maximum possible range to >9BFF - not all supported here though
		if (nBank >= PCODEGROMBASE) {
			debug_write("Warning: Invalid GROM base 0x%04X write", x);
		}
        // TODO: Classic99 always supports 16 GROM bases. Note that not all
        // carts will, so technically this mask needs to be configurable.
        nBank &= 0xf;
	} else {
		nBank=0;
	}

	return WriteValidGrom(nBank, x, c);
}

//////////////////////////////////////////////////////////////////
// Read a byte from P-Code GROM
//////////////////////////////////////////////////////////////////
Byte rpcodebyte(Word x)
{
	Byte z;

	if (x>=0x5ffc)
	{
		return(0);										// write address
	}

	// PCODE GROMs are distinct from the rest of the system
#if 0
	// code I was using to reverse pCode for cartridge port
	if ((pCurrentCPU->PC >= 0x404c)&&(pCurrentCPU->PC <= 0x4050)) {			// dummy read
	} else if ((pCurrentCPU->PC >= 0x4070)&&(pCurrentCPU->PC <= 0x4094)) {	// copy loop 1
	} else if ((pCurrentCPU->PC >= 0x59b4)&&(pCurrentCPU->PC <= 0x59ca)) {	// read word
	} else if ((pCurrentCPU->PC >= 0x584a)&&(pCurrentCPU->PC <= 0x588a)) {	// VDP copy loop
	} else if ((pCurrentCPU->PC >= 0x4190)&&(pCurrentCPU->PC <= 0x41b6)) {	// big/little endian read
	} else if ((pCurrentCPU->PC >= 0x8300)&&(pCurrentCPU->PC <= 0x8348)) {	// scratchpad data fetch functions
	} else if ((pCurrentCPU->PC >= 0x535e)&&(pCurrentCPU->PC <= 0x536a)) {	// byte data fetch
	} else if ((pCurrentCPU->PC >= 0x5290)&&(pCurrentCPU->PC <= 0x52a4)) {	// two compare loops
	} else if ((pCurrentCPU->PC >= 0x4f70)&&(pCurrentCPU->PC <= 0x4f9e)) {	// read a byte from an address
	} else if ((pCurrentCPU->PC >= 0x4bba)&&(pCurrentCPU->PC <= 0x4bc8)) {	// copy loop
	} else if ((pCurrentCPU->PC >= 0x53be)&&(pCurrentCPU->PC <= 0x53d0)) {	// another copy loop
	} else if ((pCurrentCPU->PC >= 0x5118)&&(pCurrentCPU->PC <= 0x5130)) {	// more copy loop
	} else {
		debug_write("Read PCODE GROM (>%04X), >%04x, >%02x", x, GROMBase[PCODEGROMBASE].GRMADD, GROMBase[PCODEGROMBASE].grmdata);
		TriggerBreakPoint();
	}
#endif

	if (x&0x0002)
	{
		// address
		GROMBase[PCODEGROMBASE].grmaccess=2;
		z=(GROMBase[PCODEGROMBASE].GRMADD&0xff00)>>8;
		// read is destructive
		GROMBase[PCODEGROMBASE].GRMADD=(((GROMBase[PCODEGROMBASE].GRMADD&0xff)<<8)|(GROMBase[PCODEGROMBASE].GRMADD&0xff));		
		// TODO: Is the address incremented anyway? ie: if you keep reading, what do you get?
		return(z);
	}
	else
	{
		// data
		UpdateHeatGROM(GROMBase[PCODEGROMBASE].GRMADD);	// todo: maybe a separate P-Code color?

		GROMBase[PCODEGROMBASE].grmaccess=2;
		z=GROMBase[PCODEGROMBASE].grmdata;

		// update just this prefetch
		GROMBase[PCODEGROMBASE].grmdata=GROMBase[PCODEGROMBASE].GROM[GROMBase[PCODEGROMBASE].GRMADD];
		IncrementGROMAddress(GROMBase[PCODEGROMBASE].GRMADD);
		return(z);
	}
}

//////////////////////////////////////////////////////////////////
// Write a byte to P-Code GROM
//////////////////////////////////////////////////////////////////
void wpcodebyte(Word x, Byte c)
{
	if (x<0x5ffc) 
	{
		return;											// read address
	}

	// PCODE GROMs are distinct from the rest of the system
	if ((pCurrentCPU->PC >= 0x4058)&&(pCurrentCPU->PC <= 0x405e)) {	// first address set
	} else if ((pCurrentCPU->PC >= 0x59b4)&&(pCurrentCPU->PC <= 0x59ca)) {	// read word
	} else if ((pCurrentCPU->PC >= 0x584a)&&(pCurrentCPU->PC <= 0x588a)) {	// VDP copy loop
	} else if ((pCurrentCPU->PC >= 0x4190)&&(pCurrentCPU->PC <= 0x41b6)) {	// big/little endian read
	} else if ((pCurrentCPU->PC >= 0x4158)&&(pCurrentCPU->PC <= 0x4166)) {	// set address
	} else if ((pCurrentCPU->PC >= 0x4f70)&&(pCurrentCPU->PC <= 0x4f9e)) {	// read a byte from an address
	} else if ((pCurrentCPU->PC >= 0x50b2)&&(pCurrentCPU->PC <= 0x50cc)) {	// read a byte from an address
	} else {
		debug_write("Write PCODE GROM (>%04X), >%04x, >%02x, %d", x, GROMBase[PCODEGROMBASE].GRMADD, c, GROMBase[PCODEGROMBASE].grmaccess);
		TriggerBreakPoint();
	}


	if (x&0x0002)
	{
		GROMBase[PCODEGROMBASE].GRMADD=(GROMBase[PCODEGROMBASE].GRMADD<<8)|(c);						// write GROM address
		GROMBase[PCODEGROMBASE].grmaccess--;
		if (GROMBase[PCODEGROMBASE].grmaccess==0)
		{ 
			GROMBase[PCODEGROMBASE].grmaccess=2;										// prefetch emulation
			
			// update just this prefetch
			GROMBase[PCODEGROMBASE].grmdata=GROMBase[PCODEGROMBASE].GROM[GROMBase[PCODEGROMBASE].GRMADD];
			IncrementGROMAddress(GROMBase[PCODEGROMBASE].GRMADD);
		}
		// GROM writes do not affect the prefetches, and have the same
		// side effects as reads (they increment the address and perform a
		// new prefetch)
	}
	else
	{
		UpdateHeatGROM(GROMBase[PCODEGROMBASE].GRMADD);		// todo: another color for pCode?

		GROMBase[PCODEGROMBASE].grmaccess=2;

//		debug_write("Writing to PCODE GROM!!");	// not supported!

		// update just this prefetch
		GROMBase[PCODEGROMBASE].grmdata=GROMBase[PCODEGROMBASE].GROM[GROMBase[PCODEGROMBASE].GRMADD];
		IncrementGROMAddress(GROMBase[PCODEGROMBASE].GRMADD);
	}
}

//////////////////////////////////////////////////////////////////
// Set bank for SuperSpace CRU method (currently leans on 379 code)
//////////////////////////////////////////////////////////////////
void SetSuperBank() {
	// NOTE: only 8 banks supported here (64k)
	// Does not work with all CRU-based carts (different paging schemes?)
	// TODO: May be because some (Red Baron) write to ROM, and I don't disable
	// the ROM-based banking here, which I should.

	// TODO: according to MAME, the Superspace pages at these addresses,
	// but the comment blocks are pretty confusing. If I could get some
	// superspace code, then maybe I could /try/ it...

	// What SHOULD this do if multiple CRU bits were set?
	// Right now we take the lowest one.
	
	if (CRU[0x0401]) {
		// is this not also true if all zeros written?
		xbBank=0;	
	} else if (CRU[0x0403]) {
		xbBank=1;
	} else if (CRU[0x0405]) {
		xbBank=2;
	} else if (CRU[0x0407]) {
		xbBank=3;
	} else if (CRU[0x0409]) {
		xbBank=4;
	} else if (CRU[0x040b]) {
		xbBank=5;
	} else if (CRU[0x040d]) {
		xbBank=6;
	} else if (CRU[0x040f]) {
		xbBank=7;
	}
	xbBank&=xb;

	// debug helper for me
//	TriggerBreakPoint();
}

//////////////////////////////////////////////////////////////////
// Set bank for PopCart CRU method (currently leans on 379 code)
//////////////////////////////////////////////////////////////////
void SetSuperBank2() {
    // based on the data written, lots of 1s, this doesn't seem right
	if (CRU[0x0580]) {
		// is this not also true if all zeros written?
		xbBank=0;	
	} else if (CRU[0x0581]) {
		xbBank=1;
	} else if (CRU[0x0582]) {
		xbBank=2;
	} else if (CRU[0x0583]) {
		xbBank=3;
	} else if (CRU[0x0584]) {
		xbBank=4;
	} else if (CRU[0x0585]) {
        // this is accessed by Toggle RAM, which is for mapping in SuperCart RAM
		xbBank=5;
	} else if (CRU[0x0586]) {
		xbBank=6;
	} else if (CRU[0x0587]) {
		xbBank=7;
	}
	xbBank&=xb;

	// debug helper for me
//	TriggerBreakPoint();
}


//////////////////////////////////////////////////////////////////
// Write a bit to CRU
//
// Despite the following, I finally do have the 9901 data manual ;)
//
// Better 9901 CRU notes:
// There are 32 bits, from 0-31:
//
// 0     is a mode select for clock or interrupt mode - see below
// 1-15  represent interrupt pins !INT1 through !INT15. Some physical pins are shared with I/O pins. Reads read the bit, writes set the interrupt mask (Active high)
// 16-31 represent I/O pins P0-P15. Some physical pins are shared with interrupt pins. Reads read the bit, writes change to output mode (until reset).
//
// Bit 0:
//  When set to 1, bits 1-15 enable clock logic.
//  When set to 0, interrupt logic is enabled.
//
// Clock Logic:
//  Bits 1-14 function as two 14-bit real time Clock Buffer registers - one read-only, one write-only.
//  Note that interrupt and I/O logic continues as normal on other bits.
//  Writes will go to the "clock load buffer" (startTimer9901)
//  Reads will come from the "clock read buffer" (timer9901Read)
//  There is also a "clock counter register" (timer9901)
//  Clock counter /always/ decrements, no matter what else is going on. However, the interrupt is normally disabled.
//  When you write any value into the clock load buffer, and the load buffer result is non-zero, the clock counter starts decrementing from that value.
//  An interrupt is generated when the counter counts down TO zero, and this reloads the clock load buffer.
//  The timer interrupt is level 3 (same as !INT3), thus the bit 3 interrupt mask must be set.
//  In clock mode, bit 15 reflects the state of INTREQ - it will be high if any interrupt is pending.
//  (TODO: this is a big emulation gotcha, since we could check all interrupts here, not only timer)
//  Any write to !INT3 mask bit will clear the pending interrupt.
//  Clock read buffer does not update while in clock mode, (either transferred on entry, or latched on entry)
//  Explicitly in the doc, the clock read buffer is updated on exit from clock mode, and every decrement.
//  Reset is done by a low signal on the !RST1 pin, or by writing 0 to bit 15 when in clock mode (CRU[0]==1)
//  Reset clears all interrupts, and resets all bits to output (with no interrupt mask?)
//
// Interrupt Logic:
//  When in interrupt mode, bits 1-15 access the interrupt pins. Reads return the state of the pin.
//  Writes control the interrupt mask for that pin. (1 is enabled, and interrupts are active LOW).
//  Enabling the interrupt mask when the pin in low will trigger an interrupt. (TODO: I sort of do this piecemeal....)
//  The highest priority active interrupt (lowest index) is output on pins IC0-IC3. (TODO: does the TI99 use these? Probably not!)
//
// I/O:
//  Bits 1-6 are input only.
//  At reset, bits 7-15 (23-31) are in input mode.
//  After any write to an I/O pin, the pin is switched to output mode (until reset!)
//  Note that 7-15 are input-only bits, to change those pins you need to write to 23-31. Writing to 7-15 loads the interrupt mask (or the clock).
//  Reading an output pin returns the current output value.
//
//  Also check against https://www.ninerpedia.org/wiki/Programmable_Systems_Interface_TMS_9901
//
//////////////////////////////////////////////////////////////////
void wcru(Word ad, int bt)
{
	if (ad>=0x800) {
		// DSR space
		if (NULL != SetSidBanked) {
			SetSidBanked(false);	// SID is disabled no matter the write
		}
		ad<<=1;		// put back into familiar space. A bit wasteful, but devices aren't high performance
		if (bt) {
			// bit 0 enables the DSR rom, so we'll check that first
			if ((ad&0xff) == 0) {
                static int nLastSetDSR = 0;
				int nTmp = (ad>>8)&0xf;
				if ((nCurrentDSR != -1) && (nTmp != nCurrentDSR)) {
					debug_write("WARNING! DSR Conflict between >1%X00 and >1%X00 at PC >%04X", nCurrentDSR, nTmp, pCurrentCPU->GetPC());
                    if (nLastSetDSR) debug_write("         DSR was last turned on at PC >%04X", nLastSetDSR);
				}
				nCurrentDSR=nTmp;
                nLastSetDSR=pCurrentCPU->GetPC();
//				debug_write("Enabling DSR at >%04x", ad);
				// there may also be device-dependent behaviour! Don't exit.
			}
			switch (ad&0xff00) {
            case 0x1000:    // RAMdisk, if active
                if (csRamDisk.GetLength() > 0) {
                    return WriteRamdiskCRU((ad & 0xff) >> 1, bt);
                }
                break;

			case 0x1300:	// RS232/PIO card
				WriteRS232CRU((ad&0xff)>>1, bt);
				break;

			case 0x1e00:	// AMS Memory card
				// the CRU bit is taken only from A12-A14, and only 0 and 1 are valid
				// 0000 0000 0000 1110
				{
					int nCRUBit = (ad&0x000e)>>1;
					switch (nCRUBit) {
						case 0: 
							EnableMapperRegisters(true);
							break;
						case 1:
							SetMemoryMapperMode(Map);
							break;
						// nothing else is wired up!
					}
				}
				break;

			case 0x1f00:	// pCode card
				if ((ad&0xff) == 0x80) {
					// bank switch
					debug_write("Switching P-Code to bank 2");
					nDSRBank[0xf]=1;
				}
				break;

            default:
//                debug_write("Write unsupported CRU 0x%04X = %d", ad, bt);
                break;
			}
		} else {
			// bit 0 enables the DSR rom, so we'll check that first
			if ((ad&0xff) == 0) {
				if (((ad>>8)&0xf) == nCurrentDSR) {
					nCurrentDSR=-1;
//					debug_write("Disabling DSR at >%04x", ad);
					// may be device-dependent behaviour, don't exit
				}
			}
			// else, it's device dependent
			switch (ad&0xff00) {
            case 0x1000:    // RAMdisk, if active
                if (csRamDisk.GetLength() > 0) {
                    return WriteRamdiskCRU((ad & 0xff) >> 1, bt);
                }
                break;
            case 0x1300:	// RS232/PIO card
				WriteRS232CRU((ad&0xff)>>1, bt);
				break;

			case 0x1e00:	// memory card
				// the CRU bit is taken only from A12-A14, and only 0 and 1 are valid
				// 0000 0000 0000 1110
				{
					int nCRUBit = (ad&0x000e)>>1;
					switch (nCRUBit) {
						case 0: 
							EnableMapperRegisters(false);
							break;
						case 1:
							SetMemoryMapperMode(Passthrough);
							break;
						// nothing else is wired up!
					}
				}
				break;

			case 0x1f00:	// pCode card
				if ((ad&0xff) == 0x80) {
					// bank switch
					debug_write("Switching P-Code to bank 1");
					nDSRBank[0xf]=0;
				}
				break;

            default:
//                debug_write("Write unsupported CRU 0x%04X = %d", ad, bt);
                break;
			}
		}
		return;
	} else if (ad >= 0x400) {
        // normally unmapped space, some special carts
        ad = (ad&0xfff);

		if (bt) {											// write the data
            CRU[ad]=1;
            switch (ad) {
                case 0x040f:
                    // super-space cart piggybacked on 379 code for now
                    debug_write("SuperSpace CRU write 0x%04X = %d", ad, bt);
                    SetSuperBank();
                    break;

                case 0x587:
                    debug_write("Popcart CRU write 0x%04X = %d", ad, bt);
                    SetSuperBank2();
                    break;

                default:
//                        debug_write("Write unsupported CRU I/O 0x%04X = %d", ad, bt);
                    break;
            }
		} else {
            CRU[ad]=0;
            switch (ad) {
                case 0x040f:
                    // super-space cart piggybacked on 379 code for now
                    debug_write("SuperSpace CRU write 0x%04X = %d", ad, bt);
                    SetSuperBank();
                    break;

                case 0x587:
                    debug_write("Popcart CRU write 0x%04X = %d", ad, bt);
                    SetSuperBank2();
                    break;

                default:
//                        debug_write("Write unsupported CRU I/O 0x%04X = %d", ad, bt);
                    break;
            }
		}
        
	} else {
		if (NULL != SetSidBanked) {
			SetSidBanked(true);		// SID is enabled no matter the write
		}

		ad=(ad&0x01f);										// get actual CRU line (0-31, within the 1k space)

        // debug interrupts lines
        if ((ad>0)&&(ad<16)) {
            if (bt != CRU[ad]) {
                debug_write("CRU interrupt change on %d to %d at PC >%04X", ad, bt, pCurrentCPU->GetPC());
            }
        }

//		debug_write("Write CRU 0x%x with %d", ad, bt);

        // cassette bits:
        // 22 - CS1 control (1 = motor on)
        // 23 - CS2 control (1 = motor on)
        // 24 - audio gate  (1 = silence)
        // 25 - audio out (shared)
        // 26 - audio in  (CS1 only)
        // most of these we just store and let other code deal with them

		if (bt) {											// write the data
			if ((ad>0)&&(ad<16)&&(CRU[0]==1)) {
				if (ad == 15) {
					// writing 1 has no effect
				} else {
					// writing to CRU 9901 timer. Any non-zero result starts the timer immediately
					Word mask=0x01<<(ad-1);
                    int oldtimer = starttimer9901;
					starttimer9901|=mask;
                    if (oldtimer != starttimer9901) {
//                        debug_write("9901 timer now set to %d", starttimer9901);
                    }
                    if (starttimer9901 != 0) {
                        // per adam doc, we start decrementing now (non-zero value)
                        timer9901=starttimer9901;
                    }
				}
			} else {
				CRU[ad]=1;
				switch (ad) {
                    case 0:
                        // timer mode (update readback reg)
                        // We don't need to do anything here, since we update the read register when we're supposed to
                        break;

					case 3:
						// timer interrupt bit
                        if (timer9901IntReq) {
    						timer9901IntReq=0;
//                            debug_write("9901 timer interrupt cleared");
                        }
						break;

                    case 2:     // vdp interrupt
                    case 21:    // alpha lock
                        // just remember it
                        break;

                    case 18:
						CRU_TOGGLES += 0.1;		// TODO: technically, it should be a change in the value of these three bits
												// but this works well enough for now. This noise is picked up in the audio.
						if ((CRU[18]==1)&&(CRU[19]==0)&&(CRU[20]==1)) {
							// column selected arbitrarily ;)
							ScreenReader::CheckUpdateSpeechOutput();	// check screen reader on keyboard scan
						}
                    case 19:					
                    case 20:
                        // keyboard column select
//                        debug_write("Keyboard column now: %d", (CRU[0x14]==0 ? 1 : 0) | (CRU[0x13]==0 ? 2 : 0) | (CRU[0x12]==0 ? 4 : 0));
                        break;

                    case 22:
                        // CS1 motor on
                        setTapeMotor(true);
                        break;

                    case 23:
                        // CS2 motor on
                        debug_write("CS2 is not supported.");
                        break;
					
                    default:
//                        debug_write("Write unsupported CRU I/O 0x%04X = %d", ad, bt);
                        break;
				}
			}
		} else {
			if ((ad>=0)&&(ad<16)&&(CRU[0]==1)) {
				if (ad == 15) {
					// writing 0 is a soft reset of the 9901 - it resets
					// all I/O pins to pure input, clears interrupts, but does not affect the timer
					// it only has this effect in timer mode, but is not a timer function.
                    // TODO: the docs don't say it doesn't affect the timer...?
                    // TODO: need to track I/O pins, could affect keyboard/etc
                    // But we should clear the interrupt masks
                    debug_write("9901 soft reset");
                    // TODO: should it exit clock mode?
	                memset(CRU, 1, 4096);		// I think CRU is deterministic
	                CRU[0]=0;	// timer control
	                CRU[1]=0;	// peripheral interrupt mask
	                CRU[2]=0;	// VDP interrupt mask
	                CRU[3]=0;	// timer interrupt mask??
                //  CRU[12-14]  // keyboard column select
                //  CRU[15]     // Alpha lock 
                //  CRU[24]     // audio gate (leave high)
	                CRU[25]=0;	// mag tape out - needed for Robotron to work!
	                CRU[27]=0;	// mag tape in (maybe all these zeros means 0 should be the default??)
				} else if (ad == 0) {
                    // TODO: I think datasheet says time resets and runs any time it's not zero?
					// Turning off timer mode - start timer (but don't reset it, as proven by camelForth)
                    // Not sure this matters to this emulation, but Adam doc notes that the 9901 will exit
                    // clock mode if it ever sees "a 1 on select line S0", even when chip select is not active.
                    // this may be the bit I mention below that Thierry noted as well.
                    // Ninerpedia notes this is TEMPORARY only /while/ A10 is high. But has all the same effects
                    // as exiting/entering on purpose
					CRU[ad]=0;
                    // We definitely do NOT reset the timer here - and doing so breaks both camelForth and fbForth
                    // docs explicitly say to update the read register here, for no reason...
                    // also updates every decrement
                    timer9901Read=timer9901;
					CRUTimerTicks=0;
//					debug_write("Exitting 9901 timer mode at %d ticks", timer9901);
				} else {
					// writing to CRU 9901 timer
                    // V9T9 doesn't update starttimer9901 until release from clock mode (double-buffered), but it probably doesn't matter to most apps
					Word mask=0x01<<(ad-1);
                    int oldtimer = starttimer9901;
					starttimer9901&=~mask;
                    if (oldtimer != starttimer9901) {
//                        debug_write("9901 timer now set to %d", starttimer9901);
                    }
                    // restart if the resulting value is non-zero - this fixes the conflict I had with tape not
                    // working if I didn't restart on exit from clock mode.
                    if (starttimer9901 != 0) {
                        timer9901 = starttimer9901;
                    }
				}
			} else {
				CRU[ad]=0;
				switch (ad) {
					case 3:
						// timer interrupt bit
                        if (timer9901IntReq) {
    						timer9901IntReq=0;
                            debug_write("9901 timer request cleared");
                        }
						break;

                    case 2:     // vdp interrupt
                    case 21:    // alpha lock
                        // just remember it
                        break;

                    case 18:
                    case 19:
                    case 20:
                        // keyboard column select
//                        debug_write("Keyboard column now: %d", (CRU[0x14]==0 ? 1 : 0) | (CRU[0x13]==0 ? 2 : 0) | (CRU[0x12]==0 ? 4 : 0));
                        break;

                    case 22:
                        // CS1 motor off
                        setTapeMotor(false);
                        break;
					
                    default:
//                        debug_write("Write unsupported CRU I/O 0x%04X = %d", ad, bt);
                        break;
				}
			}
		}
		if ((ad > 15) && (ad < 31) && (CRU[0] == 1)) {
			// exit timer mode
            debug_write("9901 exitting timer mode for CRU access");
			wcru(0,0);
		}
		// There's another potential case for automatic exit of timer mode
		// if a value from 16-31 appears on A10-A15 (remember A15 is LSB),
		// Thierry Nouspikel says that the 9901 will see this and exit 
		// timer mode as well, even though it's not a CRU operation.
        // Looking more closely at the Osborne doc, it seems more picky than
        // that. The 9901 will see a CRU access only when the 3 MSB of the address
        // bus are zero AND MEMEN is inactive (ie: NOT a memory cycle) AND
        // its CE line is active. I have not yet looked at the schematic to
        // check the CE mapping, but probably it's based on the CRU control line.
        // but, the Osborne doc /also/ says any memory address with A4 high will do
        // this. That's 0x0800. But this is strange, since it means huge areas of 
        // memory space would be unable to read the timer register on real hardware.
        // Are we really just that lucky??
	}
}

//////////////////////////////////////////////////////////////////
// Read a bit from CRU
//////////////////////////////////////////////////////////////////
int CheckJoysticks(Word ad, int col) {
	int joyX, joyY, joyFire;
	int joy1col, joy2col;
	int ret=1;

    // TODO: the joysticks appear not to work if a key pressed
    // on the same column is pressed.
    // This is confirmed. It appears to be because the keys are directly
    // connected to the 9901, but the joystick runs through a diode. So
    // not enough current makes it through the diode if a key on the same column
    // is pressed and so the joystick has no response.

	// Read external hardware
	joyX=0;
	joyY=0;
	joyFire=0;

	if (nSystem != 0) {
		// 99/4A
		joy1col=4;
		joy2col=0;
	} else {
		// 99/4
		joy1col=2;
		joy2col=4;
	}

	if ((col == joy1col) || (col == joy2col))				// reading joystick
	{	
		// TODO: This still reads the joystick many times for a single scan, but it's better ;)
		if (fJoy) {
			int device = -1;
			int index = 0;

			// allow for joystick number 1-16
			if (col==joy2col) {
				index = 1;	// else it's joystick 1, so index 0
			}

			if ((joyStick[index].mode > 0)&&(joyStick[index].mode <= 16)) {
				device = (joyStick[index].mode-1)+JOYSTICKID1;
			}

			if (device!=-1) {
				// check if we previously disabled this stick
				if ((installedJoysticks&(1<<(device-JOYSTICKID1))) == 0) {
					return 1;
				}

				memset(&myJoy, 0, sizeof(myJoy));
				myJoy.dwSize=sizeof(myJoy);
				myJoy.dwFlags=JOY_RETURNALL | JOY_USEDEADZONE;
				MMRESULT joyret = joyGetPosEx(device, &myJoy);
				if (JOYERR_NOERROR == joyret) {
					// TODO: we do all this work to calculate both axes plus the buttons, but we only
					// actually want one direction OR the button, so we should make the joystick read smarter, cache
					// myJoy for a frame or so and just pull out the part we actually need...

					if (0!=(myJoy.dwButtons & joyStick[index].btnMask)) {
						joyFire=1;
					}

					unsigned int half = (joyStick[index].maxXDead-joyStick[index].minXDead)/2;
					unsigned int axis;
					switch(joyStick[index].Xaxis) {
						case 0: axis = myJoy.dwXpos; break;
						case 1: axis = myJoy.dwYpos; break;
						case 2: axis = myJoy.dwZpos; break;
						case 3: axis = myJoy.dwRpos; break;
						case 4: axis = myJoy.dwUpos; break;
						case 5: axis = myJoy.dwVpos; break;
						case 6: 
							if (myJoy.dwPOV <= 35999) {
								// use sin() (x component)			 degrees*100 to radians
								axis = (unsigned int)(sin(myJoy.dwPOV * 0.000174533)*(half*2)+half+joyStick[index].minXDead);
							} else {
								axis = joyStick[index].minXDead+half;
							}
							break;
						case 7: 
							if (myJoy.dwPOV <= 35999) {
								// use cos() (y component)			 degrees*100 to radians
								axis = (unsigned int)(cos(myJoy.dwPOV * 0.000174533)*(half*2)+half+joyStick[index].minXDead);
							} else {
								axis = joyStick[index].minXDead+half;
							}
							break;

						default: axis = joyStick[index].minXDead+half;
					}
					if (axis<joyStick[index].minXDead) {
						joyX=-4;
					} else if (axis>joyStick[index].maxXDead) {
						joyX=4;
					}

					half = (joyStick[index].maxYDead-joyStick[index].minYDead)/2;
					switch(joyStick[index].Yaxis) {
						case 0: axis = myJoy.dwXpos; break;
						case 1: axis = myJoy.dwYpos; break;
						case 2: axis = myJoy.dwZpos; break;
						case 3: axis = myJoy.dwRpos; break;
						case 4: axis = myJoy.dwUpos; break;
						case 5: axis = myJoy.dwVpos; break;
						case 6: 
							if (myJoy.dwPOV <= 35999) {
								// use sin() (x component)			 degrees*100 to radians, inverted for y
								axis = (unsigned int)((-sin(myJoy.dwPOV * 0.000174533))*(half*2)+half+joyStick[index].minYDead);
							} else {
								axis = joyStick[index].minYDead+half;
							}
							break;
						case 7:
							if (myJoy.dwPOV <= 35999) {
								// use cos() (y component)			 degrees*100 to radians, inverted for y
								axis = (unsigned int)((-cos(myJoy.dwPOV * 0.000174533))*(half*2)+half+joyStick[index].minYDead);
							} else {
								axis = joyStick[index].minYDead+half;
							}
							break;
						default: axis = joyStick[index].minYDead+half;
					}
					if (axis<joyStick[index].minYDead) {
						joyY=4;
					} else if (axis>joyStick[index].maxYDead) {
						joyY=-4;
					}
				} else {
					// disable this joystick so we don't slow to a crawl
					// trying to access it. We'll check again on a reset
					debug_write("Disabling joystick %d - error %d reading it.", (device-JOYSTICKID1)+1, joyret);
					installedJoysticks&=~(1<<(device-JOYSTICKID1));
				}
			} else {	// read the keyboard
				// if just activating the joystick, so make sure there's no fctn-arrow keys active
				// just forcibly turn them off! Should only need to do this once

				if (key[VK_TAB]) {
					joyFire=1;
					if (0 == fJoystickActiveOnKeys) {
						decode(0xf0);	// key up
						decode(VK_TAB);
					}
				}
				if (key[VK_LEFT]) {
					joyX=-4;
					if (0 == fJoystickActiveOnKeys) {
						decode(0xe0);	// extended
						decode(0xf0);	// key up
						decode(VK_LEFT);
					}
				}
				if (key[VK_RIGHT]) {
					joyX=4;
					if (0 == fJoystickActiveOnKeys) {
						decode(0xe0);	// extended
						decode(0xf0);	// key up
						decode(VK_RIGHT);
					}
				}
				if (key[VK_UP]) {
					joyY=4;
					if (0 == fJoystickActiveOnKeys) {
						decode(0xe0);	// extended
						decode(0xf0);	// key up
						decode(VK_UP);
					}
				}
				if (key[VK_DOWN]) {
					joyY=-4;
					if (0 == fJoystickActiveOnKeys) {
						decode(0xe0);	// extended
						decode(0xf0);	// key up
						decode(VK_DOWN);
					}
				}

				fJoystickActiveOnKeys=180;		// frame countdown! Don't use PS2 arrow keys for this many frames
			}
		}

		// TODO: this is where we actually extract the data we actually care about...
		if (ad == 3)
		{	
			if (joyFire)	// button reads normally
			{
				ret=0;
			}
		}
		else
		{
			switch (ad-3) {						// Check real joystick
				case 1: if (joyX ==-4) ret=0; break;
				case 2: if (joyX == 4) ret=0; break;
				case 3: if (joyY ==-4) ret=0; break;
				case 4: if (joyY == 4) ret=0; break;
			}
		}
	}
	return ret;
}

#if 0
// ** BIG TODO - 9901 CRU IMPROVEMENTS **
CRU Read test (Using Mizapf's XB test to read the first 32 bits)

CALL INIT
CALL LOAD(12288,4,224,131,196,2,1,27,10,6,1,22,254,4,204,2,1,50,200,52,49,2,44,0,32,52,49,4,91)
CALL LOAD(-31804,48,0)
CALL PEEK(13000,A,B,C,D)
PRINT A;B;C;D


BIT	HW	C99	Purpose						Status
-------------------						----------------------
0	0	0	9901 Control				Implemented
1	1	1	External INT1				Partially implemented
2	0	0	VDP sync					Implemented
3	1	0	Clock, keyboard enter, fire	Implemented, but wrong?
4	1	0	Keyboard l, left			Implemented, but wrong?
5	1	1	Keyboard p, right			Implemented
6	1	1	keyboard 0, down			Implemented
7	1	1	keyboard shift, up			Implemented
8	1	1	keyboard space				Implemented
9	1	0	keyboard q					Implemented, but wrong?
10	1	1	keyboard l					Implemented
11	0	1	unused						Implemented as loopback, but wrong?
12	1	1	reserved					Implemented as loopback -- pulled HIGH on hardware
13	0	1	unused						Implemented as loopback, but wrong?
14	0	1	unused						Implemented as loopback, but wrong?
15	1	1	unused						Implemented as loopback
16	0	0	reserved					Implemented as loopback
17	0	0	reserved					Implemented as loopback - checked on the 99/4 but seems not wired up
18	0	0	keyboard select bit 2		Implemented as loopback
19	0	0	keyboard select bit 1		Implemented as loopback
20	0	0	keyboard select bit 0		Implemented as loopback
21	1	0	alpha lock					Implemented, but wrong?
22	1	0	CS1 control					Hard coded to return 1, so why is it 0?
23	1	0	CS2 control					Hard coded to return 1, so why is it 0?
24	0	0	Audio gate					Hard coded to return 1, so why is it 0?
25	0	0	Tape out					Implemented as loopback
26	1	0	???							Skipped in E/A manual, but implemented as loopback, but wrong.
27	0	0	Tape in						Implemented
28	1	0	unused						Implemented as loopback, but wrong?
29	1	0	unused						Implemented as loopback, but wrong?
30	1	0	unused						Implemented as loopback, but wrong?
31	1	0	unused						Implemented as loopback, but wrong?

The loopback, but wrong ones may simply have never been written to, since I default all CRU to 1. Maybe these bits need to default to 0. We should properly characterize when we rewrite the 9901.
#endif
int rcru(Word ad)
{
	int ret,col;									// temp variables

	if ((CRU[0]==1)&&(ad<16)&&(ad>0)) {				// read elapsed time from timer
		if (ad == 15) {
			// this reflects the state of the interrupt request PIN, so 
            // it's a little more complex than just one interrupt. Technically, it's
            // ALL interrupts that run through the 9901, and is affected by the mask.
            // For now, we just have the VDP, eventually we'll have others.
            return
                (
                    (timer9901IntReq && CRU[3]) ||  // timer interrupt !INT3
                    (VDPINT && CRU[2])              // VDP Interrupt !INT2
                );
		}
		Word mask=0x01<<(ad-1);
		if (timer9901Read & mask) {
			return 1;
		} else {
			return 0;
		}
	}

	if ((ad > 15) && (ad < 31) && (CRU[0] == 1)) {
		// exit timer mode
		wcru(0,0);
	}

	// only certain CRU bits are implemented, check them
	if (ad >= 0x0800) {
		ad<<=1;		// puts it back into a familiar space - wasteful but easier to deal with

		switch (ad&0xff00) {
            case 0x1000:    // RAMdisk, if active
                if (csRamDisk.GetLength() > 0) {
                    return ReadRamdiskCRU((ad & 0xff) >> 1);
                }
                return 1;
			case 0x1100:	// disk controller
				if (nDSRBank[1] != 0) {
					// TICC paged in 
					return ReadTICCCRU((ad&0xff)>>1);
				}
				return 1;	// "false"

			case 0x1300:	// RS232/PIO card
				return ReadRS232CRU((ad&0xff)>>1);

			default:
				// no other cards supported yet
//                debug_write("Read unsupported CRU 0x%04X", ad);
				return 1;	// "false"
		}
	} else if (ad >= 0x0400) {
        // some add-on cards, no special read support here
        return CRU[ad];
    }

	// The CRU bits >0000 through >001f are repeated through the whole 1k range from 0000-0400!
	ad=(ad&0x001f);										// get actual CRU line
	ret=1;												// default return code (false)

	// are we checking VDP interrupt?
    // it should reflect on bit 15 in clock mode, too, IF !INT2 is enabled in the mask (done above)
	if (ad == 0x02) {		// that's the only int we have
		// TODO: should I put this on clock mode bit 15? Who would do that?
		if (statusUpdateRead) {
			// new frame, first access since frame was reset by reading the status register, so remember when it was
			statusUpdateRead = false;
			statusReadLine = vdpscanline;
			// calculate scanlines since interrupt pulse
			if (vdpscanline > 192+27) {
				statusReadCount = 262*statusFrameCount + (vdpscanline-192-27);
			} else {
				statusReadCount = 262*statusFrameCount + (vdpscanline+(262-(192+27)));
			}
		}
			
		if (VDPINT) {
			return 0;		
		} else {
			return 1;
		}
	}
    // TODO: it should reflect on bit 15 in clock mode, too, IF !INT2 is enabled in the mask
	if (ad == 0x01) {		// this would be a peripheral card interrupt
		// todo: we don't have any, though!
		return 1;
	}

    // cassette support
    if (ad == 27) {
        // tape input (the outputs can't be read back, technically)
        if (getTapeBit()) {
            return 0;   // inverted logic
        } else {
            return 1;   // this also preserves the Perfect Push 'tick' on audio gate
        }
    }
	// JasonACT says that returning a fixed value for this is wrong
	// He insists that reading these bits when reading the keyboard, 
	// as Robotron does, is not oddball behaviour and should be
	// properly emulated. This one's for you buddy! ;)
    //if ((ad >= 22) && (ad < 25)) {
    //    // these are the cassette CRU output bits
    //    return 1;
    //}

	// no other hardware devices at this time, check keyboard/joysticks

	// keyboard reads as an array. Bit addresses hex 24, 26 and 28 set the line to	
	// scan (columns). Bit addreses hex 6-14 are used for return. 0 means on.		
	// The address was divided by 2 before being given to the routine	

	// Some hacks here for 99/4 scanning
	if ((ad>=0x03)&&(ad<=0x0a))
	{	
		col=(CRU[0x14]==0 ? 1 : 0) | (CRU[0x13]==0 ? 2 : 0) | (CRU[0x12]==0 ? 4 : 0);	// get column

		if (keyboard==KEY_994A_PS2) {
			// for 99/4A only, not 99/4
			unsigned char in;

			in=CheckTIPolling(col|((CRU[0x15]==0)?8:0));	// add in bit 4 for alpha lock scanning

			if (0xff != in) {
				// (ad-3) is the row number we are checking (bit #)
				if (0 == (in & (1<<(ad-3)))) {
					ret=0;
				} else {
					ret=1;
				}
				return ret;
			}

			// else, try joysticks
			return CheckJoysticks(ad, col);
		}

		// not PS/2, use the old method
		if ((ad==0x07)&&(CRU[0x15]==0))					// is it ALPHA LOCK?
		{	
			ret=0;
			if (GetKeyState(VK_CAPITAL) & 0x01)			// check CAPS LOCK (on?)
			{	
				ret=1;									// set Alpha Lock off (invert caps lock)
			}

			return ret;
		}

		// Either joysticks or keyboard - try joysticks first
		ret = CheckJoysticks(ad, col);
		if (1 == ret) {
			// if nothing else matched, try the keyboard array
			if (key[KEYS[keyboard][col][ad-3]])	{			// normal key
				ret=0;
			}
		}
	}
	if ((ad>=11)&&(ad<=31)) {
		// this is an I/O pin - return whatever was last written
		ret = CRU[ad];
        //debug_write("Read CRU I/O 0x%04X (got %d)", ad, ret);
    }

	return(ret);
}

/////////////////////////////////////////////////////////////////////////
// Write a line to the debug buffer displayed on the debug screen
// We no longer display duplicated lines to the internal debug log.
// The external debug log will see them though.
/////////////////////////////////////////////////////////////////////////
void debug_write(char *s, ...)
{
	char buf[1024];
	static char lastbuf[1024] = "";

	_vsnprintf(buf, 1023, s, (char*)((&s)+1));
	buf[1023]='\0';

	// let all lines reach the external debugger
	OutputDebugString(buf);
	OutputDebugString("\n");

	// but skip duples to the internal log
	if (0 == strcmp(buf, lastbuf)) {
		return;
	}

	// critical section needed to avoid conflict with the display code
	EnterCriticalSection(&DebugCS);
    
    strcpy(lastbuf, buf);

	// trim to the internal debug size
	buf[DEBUGLEN-1]='\0';
	
	memcpy(&lines[0][0], &lines[1][0], (DEBUGLINES-1)*DEBUGLEN);			// scroll data
	strncpy(&lines[DEBUGLINES-1][0], buf, DEBUGLEN);				        // copy in new line
	memset(&lines[DEBUGLINES-1][strlen(buf)], 0x20, DEBUGLEN-strlen(buf));	// clear rest of line
	lines[DEBUGLINES-1][DEBUGLEN-1]='\0';							        // zero terminate

	LeaveCriticalSection(&DebugCS);

	bDebugDirty=true;												        // flag redraw
}

// Simple thread that watches the event and clears the buffer, then
// restarts playback
void __cdecl SpeechBufThread(void *) {
	DWORD ret; 
	UCHAR *ptr1, *ptr2;
	unsigned long len1, len2;

	for (;;) {
		if (INVALID_HANDLE_VALUE == hSpeechBufferClearEvent) {
			Sleep(150);
			ret=WAIT_TIMEOUT;
		} else {
			ret=WaitForSingleObject(hSpeechBufferClearEvent, 150);
		}
		if (WAIT_OBJECT_0 == ret) {
			if (SUCCEEDED(speechbuf->Lock(0, 0, (void**)&ptr1, &len1, (void**)&ptr2, &len2, DSBLOCK_ENTIREBUFFER))) {
				// since we haven't started the sound, hopefully the second pointer is nil
				if (len2 != 0) {
					debug_write("Failed to lock speech buffer");
				}
				// signed 16-bit - zero the buffer
				memset(ptr1, 0, len1);

				speechbuf->Unlock(ptr1, len1, ptr2, len2);
			}

			if (FAILED(speechbuf->Play(0, 0, 0))) {
				debug_write("Speech DID NOT START");
			}
		}
		if (quitflag) break;
	}
}

//////////////////////////////////////////////////////////////
// 'Retrace' counter for timing - runs at 50 or 60 hz
// Uses 'hzRate'. Coded for lessor CPU usage, may be
// less accurate on a small scale but about the same
// over time.
//////////////////////////////////////////////////////////////
extern int dac_pos;
extern double dacupdatedistance;
void __cdecl TimerThread(void *)
{
	MY_LARGE_INTEGER nStart, nEnd, nFreq, nAccum;
	static unsigned long old_total_cycles=0;
	static int oldThrottleMode = THROTTLE_NONE;
	static int nVDPFrames = 0;
	bool bDrawDebug=false;
	long nOldCyclesLeft = 0;
	int oldHzRate = 0;
	HANDLE timer = CreateWaitableTimer(NULL, false, NULL);
	
	// Ensure the scheduler won't move us around on multicore machines
	SetThreadAffinityMask(GetCurrentThread(), 0x01);
	timeBeginPeriod(1);

	time(&STARTTIME);
	if (FALSE == QueryPerformanceCounter((LARGE_INTEGER*)&nStart)) {
		debug_write("Failed to query performance counter, error 0x%08x", GetLastError());
		MessageBox(myWnd, "Unable to run timer system.", "Classic99 Error", MB_ICONSTOP|MB_OK);
		ExitProcess(-1);
	}

	nAccum.QuadPart=0;

	while (quitflag==0) {
		// Check if the system speed has changed
		// This is actually kind of lame - we should use a message or make the vars global
		if (ThrottleMode != oldThrottleMode) {
			oldThrottleMode = ThrottleMode;
			old_total_cycles=total_cycles;
			nAccum.QuadPart=0;
		}
		if (hzRate != oldHzRate) {
			LARGE_INTEGER due;
			due.QuadPart=-1;		// now, essentially
			if (!SetWaitableTimer(timer, &due, 8, NULL, NULL, FALSE)) {	// we can wake up at any speed, the loop below works out real time
                debug_write("The waitable timer failed - code %d", GetLastError());
            }
			oldHzRate = hzRate;
		}

		// process debugger, if active
		processDbgPackets();

		// handle breakpoint
		if (max_cpf == 0) {
			QueryPerformanceCounter((LARGE_INTEGER*)&nStart);
			nAccum.QuadPart=0;
			Sleep(10);
			continue;
		}
         
		if ((PauseInactive)&&(!WindowActive)) {
			// Reduce CPU usage when inactive (hack)
			Sleep(100);
		} else {
			// if hzRate==50, then it's 20000us per frame
			// if hzRate==60, then it's 16666us per frame - .6. overall this runs a little slow, but it is within the 5% tolerance (99.996%)
			// our actual speeds are 50hz and 62hz, 62hz is 99% of 62.6hz, calculated via the datasheet
			// 62hz is 16129us per frame (fractional is .03, irrelevant here, so it works out nicer too)
			switch (ThrottleMode) {
				default:
                    // TODO: using the old trick of triggering early (twice as often) helps, but still wrong
					WaitForSingleObject(timer, 1000);	// this is 16.12, rounded to 16, so 99% of 99% is still 99% (99.2% of truth)
					nVDPFrames = 0;
					break;
				case THROTTLE_OVERDRIVE:
					Sleep(1);	// minimal sleep for load's sake
					// Do not set nVDPFrames to 0 here
					break;
				case THROTTLE_SYSTEMMAXIMUM:
					// We do the exchange here since the loop below may not run
					InterlockedExchange((LONG*)&cycles_left, max_cpf*100);
					nVDPFrames = 0;
					break;
			}

			if (FALSE == QueryPerformanceCounter((LARGE_INTEGER*)&nEnd)) {
				debug_write("Failed to query performance counter, error 0x%08x", GetLastError());
				MessageBox(myWnd, "Unable to run timer system.", "Classic99 Error", MB_ICONSTOP|MB_OK);
				ExitProcess(-1);
			}
			if (nEnd.QuadPart<nStart.QuadPart) {
				// We wrapped around. This should be a once in a lifetime event, so rather
				// than go nuts, just skip this frame
				//debug_write("Timer wrapped around");
				nStart.QuadPart=nEnd.QuadPart;
				continue;
			}
			QueryPerformanceFrequency((LARGE_INTEGER*)&nFreq);

			// Work out how long we actually slept for, in microseconds
			nAccum.QuadPart+=(((nEnd.QuadPart-nStart.QuadPart)*1000000i64)/nFreq.QuadPart);	
			nStart.QuadPart=nEnd.QuadPart;					// don't lose any time
			
			// convert nFreq into one frame of time
			nFreq.QuadPart=(hzRate==HZ60) ? /*16129i64*/ 16666i64 : 20000i64;

			unsigned long cycles_used = nOldCyclesLeft - cycles_left;

			while (nAccum.QuadPart >= nFreq.QuadPart) {
				nVDPFrames++;
				// this makes us run the right number of frames, and should account for fractions better
				nAccum.QuadPart-=nFreq.QuadPart;

				if (max_cpf > 0) {
					// to prevent runaway, if the CPU is not executing for some reason, don't increment
					// this handles the case where Windows is blocking the main thread (which doesn't happen anymore)
					if ((nOldCyclesLeft != cycles_left) || (max_cpf == 1)) {
						switch (ThrottleMode) {
						case THROTTLE_SLOW:
						case THROTTLE_NORMAL:
							InterlockedExchangeAdd((LONG*)&cycles_left, max_cpf);
							break;

						case THROTTLE_OVERDRIVE:
						case THROTTLE_SYSTEMMAXIMUM:
							InterlockedExchange((LONG*)&cycles_left, max_cpf * cfg_overdrive);
							break;
						}

						nOldCyclesLeft = cycles_left;
					}
				}
				if ((nVDPFrames > 10) && (max_cpf > 0)) {
					// more than a 1/6 second behind - just drop it
					nAccum.QuadPart = 0;
					InterlockedExchange((LONG*)&cycles_left, max_cpf);
					nVDPFrames = 1;
					debug_write("Too far behind, drop frames");
				}
			}
			SetEvent(hWakeupEvent);		// wake up CPU if it's sleeping

			if (total_cycles_looped) {
				//debug_write("Total cycles looped"); - this does happen
				total_cycles_looped=false;
				old_total_cycles=0;		// mistiming, but survives the wrap.
				// very very fast machines may someday break this loop
			}

			// copy over the speech buffer -- only need to do this once
			// if our timing is right this should always work out about right
			SpeechBufferCopy();

			// This set the VDP processing rate. This is based on CPU cycles except
			// in overdrive, which tries to maintain approximately real time despite
			// CPU cycle count.
			if (ThrottleMode != THROTTLE_OVERDRIVE) {
				// this side is used in normal mode
				while (old_total_cycles+(hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF) <= total_cycles) {
					Counting();					// update counters & VDP interrupt
					old_total_cycles+=(hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF);
					bDrawDebug=true;
				}
			} else {
				// this side is used in overdrive
				if (nVDPFrames > 0) {
					static MY_LARGE_INTEGER last;
					// run one frame every time we're able (and it's needed)
					Counting();					// update counters & VDP interrupt
					nVDPFrames=0;
					bDrawDebug=true;

#if 0
					QueryPerformanceFrequency((LARGE_INTEGER*)&nFreq);
					if ((nStart.QuadPart - last.QuadPart)*1000000i64/nFreq.QuadPart < 16000i64) {
						debug_write("VDP Force frame after %I64d microseconds, %ld cycles used", (nStart.QuadPart - last.QuadPart)*1000000i64/nFreq.QuadPart, cycles_used);
					}
					last.QuadPart = nStart.QuadPart;
#endif

					vdpForceFrame();

				}
			}

            // TODO: a hack of sorts, but DAC and sound update can end up running at different rates,
            // leading to out of sync audio. So empty the dac buffer, no matter how much we used
//            if (dac_pos < 100) {
//                debug_write("dac_pos at %d", dac_pos);
//                dac_pos = 0;
//            }

			if ((bDrawDebug)&&(dbgWnd)) {
				if (max_cpf > 0) {
					draw_debug();
				}
			}
		}	
	}

	time(&ENDTIME);
	debug_write("Seconds: %ld, ticks: %ld", (long)ENDTIME-STARTTIME, ticks);

	timeEndPeriod(1);
	debug_write("Ending Timer Thread");
}

////////////////////////////////////////////////////////////////
// Timer calls this function each tick
////////////////////////////////////////////////////////////////
void Counting()
{
	ticks++;
	retrace_count++;
	//end_of_frame=1;		// one frame of time has elapsed, do our processing

	// update sound buffer -- eventually we should instead move this to generate from the
	// scanline based VDP (the one not written yet, hehe)
	static struct StreamData soundDat, sidDat;

	EnterCriticalSection(&csAudioBuf);

	if (NULL != soundbuf) {
		UpdateSoundBuf(soundbuf, sound_update, &soundDat);
	}
	if ((NULL != sidbuf) && (NULL != sid_update)) {
		UpdateSoundBuf(sidbuf, sid_update, &sidDat);
	}

	LeaveCriticalSection(&csAudioBuf);
}

// Debug step helpers
void DoPause() {
	if (0 != max_cpf) {
		TriggerBreakPoint(true,false);
	}
}

void DoStep() {
	if (0 == max_cpf) {
		InterlockedExchange((LONG*)&cycles_left, 1);	// allow one instruction through
		bDebugAfterStep=true;
		nStepCount=1;
		SetEvent(hWakeupEvent);		// wake up CPU if it's sleeping
	}
}

void DoStepOver() {
    // TODO: does not work with banks - convert breakpoint system to
    // a standard and then this can be a "temporary" breakpoint that
    // deletes itself once its hit.
	if (0 == max_cpf) {
		max_cpf=cfg_cpf;
		SetWindowText(myWnd, szDefaultWindowText);
		InterlockedExchange((LONG*)&cycles_left, max_cpf);
		pCurrentCPU->SetReturnAddress(0);
		bDebugAfterStep=true;
		bStepOver=true;
		nStepCount=1;
		SetEvent(hWakeupEvent);		// wake up CPU if it's sleeping
	}
}

void DoPlay() {
	if (0 == max_cpf) {
		max_cpf=cfg_cpf;
		nStepCount=1;
		SetWindowText(myWnd, szDefaultWindowText);
		InterlockedExchange((LONG*)&cycles_left, max_cpf);
		SetSoundVolumes();
		UpdateMakeMenu(dbgWnd, 0);
	}

	// Passing '1' here tells the window handler not to change the
	// speed again - otherwise we race with the message pump. This
	// was causing breakpoints that were very close together to be
	// lost. The '1' makes it a visual update only.
	// TODO: this might be true anymore (or need to be?)
	switch (ThrottleMode) {
	default:
	case THROTTLE_NORMAL:
		PostMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_NORMAL, 1);
		break;

	case THROTTLE_SLOW:
		PostMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUSLOW, 1);
		break;

	case THROTTLE_OVERDRIVE:
		PostMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUOVERDRIVE, 1);
		break;

	case THROTTLE_SYSTEMMAXIMUM:
		PostMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_SYSTEMMAXIMUM, 1);
		break;
	}

	SetEvent(hWakeupEvent);		// wake up CPU if it's sleeping
}

void DoFastForward() {
	DoPlay();		// wake up clean, then accelerate
	PostMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_SYSTEMMAXIMUM, 0);
}

void DoLoadInterrupt() {
	if (pCurrentCPU == pCPU) {
		if ((0 != romword(0xfffc)) && (0 != romword(0xfffe))) {
			Word x1;

			// only if a vector is set - this is RAM so should be safe
			// do a BLWP to 0xFFFC
			debug_write("Load interrupt triggered with valid vector");

			doLoadInt = true;
		
		} else {
			debug_write("Ignoring load interrupt as no vector loaded.");
		}
	} else {
		debug_write("No LOAD during GPU execution");
	}
}

void DoMemoryDump() {
	// TODO: Add GUI to select CPU RAM, AMS RAM and VDP RAM
	if (IDYES == MessageBox(myWnd, "Dump memory TO MEMDUMP.BIN and VDPDUMP.BIN?", "Classic99 Dump RAM", MB_YESNO)) {
		FILE *fp=fopen("MEMDUMP.BIN", "wb");
		if (NULL != fp) {
			unsigned char buf[8192];
			for (int idx=0; idx<65536; idx++) {
				buf[idx%8192]=ReadMemoryByte((Word)idx, ACCESS_FREE);
				if (idx%8192 == 8191) {
					fwrite(buf, 1, 8192, fp);
				}
			}
			fclose(fp);
		}
		fp=fopen("VDPDUMP.BIN", "wb");
		if (NULL != fp) {
			fwrite(VDP, 1, 16384, fp);
			// write the VDP registers at the end of the file (8 bytes more)
			for (int idx=0; idx<8; idx++) {
				fputc(VDPREG[idx], fp);
			}
			fclose(fp);
		}
		debug_write("Dumped memory to MEMDUMP.BIN and VDPDUMP.BIN");
	}
}

void TriggerBreakPoint(bool bForce, bool openDebugger) {
	if ((!pCurrentCPU->enableDebug)&&(!bForce)) {
		return;
	}

    EnterCriticalSection(&csDisasm);
        if (NULL != fpDisasm) {
            if ((disasmLogType == 0) || (pCurrentCPU->GetPC() > 0x2000)) {
                fprintf(fpDisasm, "**** Breakpoint triggered\n");
            }
        }
    LeaveCriticalSection(&csDisasm);

	SetWindowText(myWnd, "Classic99 - Breakpoint. F1 - Continue, F2 - Step, F3 - Step Over");
	max_cpf=0;
	MuteAudio();
	InterlockedExchange(&cycles_left, 0);
	UpdateMakeMenu(dbgWnd, 1);
	draw_debug();
	redraw_needed = REDRAW_LINES;

    // dump the cycle counting file if it's got anything in it
    for (int idx=0; idx<0x10000; idx+=2) {
        if (cycleCounter[idx]) {
            FILE *fp=fopen("CycleCounts.txt", "w");
            if (NULL != fp) {
                bool blank=false;
                for (int i2=idx; i2<0x10000; ) {
                    if (cycleCounter[i2]) {
			            char buf[1024];
                        blank=true;
			            sprintf(buf, "%04X ", i2);
			            int tmp = Dasm9900(&buf[5], i2, 0);   // TODO: can't help the bank right now
			            fprintf(fp, "(%d) %-33s %d cycles\n", 0, buf, cycleCounter[i2]);
                        i2+=tmp;
                    } else {
                        if (blank) {
                            fprintf(fp, "(...)\n");
                            blank=false;
                        }
                        i2+=2;
                    }
                }
                fclose(fp);
                debug_write("Wrote CycleCounts.txt for timed segment");
                memset(cycleCounter, 0, sizeof(cycleCounter));
            }
            break;
        }
    }

	// finally, open the debugger if it's not open (unless we are told not to!)
	if ((openDebugger) && (NULL == dbgWnd)) {
		// Send so that we wait for the reply
	    SendMessage(myWnd, WM_COMMAND, ID_EDIT_DEBUGGER, 0);
		bDebugDirty=true;
		SetEvent(hDebugWindowUpdateEvent);
	}
}

void memrnd(void *pRnd, int nCnt) {
	// fill memory with a random pattern 
	// We use this to randomly set RAM rather than
	// assume it's always powered up as zeroed

	// however, users have requested this be an option, not forced ;)
	if (bScrambleMemory) {
		for (int i=0; i<nCnt; i++) {
			*((unsigned char *)pRnd+i) = rand()%256;
		}
	} else {
		memset(pRnd, 0, nCnt);
	}
}

// 64k heatmap only
int nHeatMap[0x10000];		
extern HWND hHeatMap;

// only the CPU heatmap worries about displaying it
void UpdateHeatVDP(int Address) {
	// we do a little trick here to flip it vertically (and truncate to 16 bit)
	// this helps with Windows liking upside down bitmaps
	Address=(Address&0xff) | (0xff00-(Address&0xff00));
	nHeatMap[Address&0xffff]|=0xff;		// if we assume 0RGB format, this is max blue (no matter what it was before)
}

void UpdateHeatGROM(int Address) {
	// we do a little trick here to flip it vertically (and truncate to 16 bit)
	// this helps with Windows liking upside down bitmaps
	Address=(Address&0xff) | (0xff00-(Address&0xff00));
	nHeatMap[Address]|=0xff00;		// if we assume 0RGB format, this is max green (no matter what it was before)
}

void UpdateHeatmap(int Address) {
	static int nCnt=0;
	static int nIdx=0;
	static LARGE_INTEGER tLast = { 0,0 };
	static LARGE_INTEGER tSpeed = { 0,0 };

	// we do a little trick here to flip it vertically (and truncate to 16 bit)
	// this helps with Windows liking upside down bitmaps
	Address=(Address&0xff) | (0xff00-(Address&0xff00));
	nHeatMap[Address]|=0xff0000;		// if we assume 0RGB format, this is max red (no matter what it was before)

	// in order to refresh the heatmap nicely, every cycle we will fade out a few pixels
	// The idea is, we have 65536 pixels. We want a pixel to reach 0 in about 3 seconds.
	// Each pixel has 256 levels. So, that's 16,777,216 pixels in 3 seconds, 5,592,405 pixels
	// in 1 second. The CPU clock is 3000000 cycles per second, and each access cycle is two bytes,
	// (not really, but that's okay here), so 2 pixels per access is enough. Since that's not
	// really true, we'll do a few and see how that goes.
	for (int i=0; i<HeatMapFadeSpeed; i++) {
		int r,g,b;
		r=(nHeatMap[nIdx]&0xff0000);
		r=(r-0x00010000)&0xff0000;
		if (r == 0xff0000) r=0;

		g=(nHeatMap[nIdx]&0xff00);
		g=(g-0x00000100)&0xff00;
		if (g == 0xff00) g=0;

		b=(nHeatMap[nIdx]&0xff);
		b=(b-0x00000001)&0xff;
		if (b == 0xff) b=0;

		nHeatMap[nIdx]=r|g|b;

		nIdx++;
		if (nIdx >= 0xffff) nIdx=0;
	}
	
	// every 50,000 cycles, draw the heatmap. this is roughly 1/60th of a second. 
	// We use a mask for 65536 cycles, so it works out more like 45 times a second. it's fine.
	if (((++nCnt)&0xffff) != 0) {
		return;
	}
	// sort of safety, make sure it's not too frequent - some systems don't like that!
	if (tSpeed.QuadPart == 0ui64) {
		QueryPerformanceFrequency(&tSpeed);
	} else {
		LARGE_INTEGER tNow;
		QueryPerformanceCounter(&tNow);
		if (tNow.QuadPart - tLast.QuadPart < (tSpeed.QuadPart)/hzRate) {
			return;
		}
		tLast=tNow;
	}

	// dump it to the window, if the window is up
	// I sorta wish this was on a different thread, maybe VDP thread?
	if (NULL != hHeatMap) {
		BITMAPINFO myInfo;

		myInfo.bmiHeader.biSize=sizeof(myInfo.bmiHeader);
		myInfo.bmiHeader.biWidth=256;
		myInfo.bmiHeader.biHeight=256;
		myInfo.bmiHeader.biPlanes=1;
		myInfo.bmiHeader.biBitCount=32;
		myInfo.bmiHeader.biCompression=BI_RGB;
		myInfo.bmiHeader.biSizeImage=0;
		myInfo.bmiHeader.biXPelsPerMeter=1;
		myInfo.bmiHeader.biYPelsPerMeter=1;
		myInfo.bmiHeader.biClrUsed=0;
		myInfo.bmiHeader.biClrImportant=0;

//		HDC myDC=GetDC(hHeatMap);
//		SetDIBitsToDevice(myDC, 0, 0, 256, 256, 0, 0, 0, 256, nHeatMap, &myInfo, DIB_RGB_COLORS);
//		ReleaseDC(hHeatMap, myDC);
        
        BITMAP structBitmapHeader;
        memset( &structBitmapHeader, 0, sizeof(BITMAP) );

		HDC myDC=GetDC(hHeatMap);

            HGDIOBJ hBitmap = GetCurrentObject(myDC, OBJ_BITMAP);
            GetObject(hBitmap, sizeof(BITMAP), &structBitmapHeader);
            // we use width twice to get a square output that preserves the Close button
            StretchDIBits(myDC, 0, 0, structBitmapHeader.bmWidth, structBitmapHeader.bmWidth, 0, 0, 256, 256, nHeatMap, &myInfo, DIB_RGB_COLORS, SRCCOPY);

        ReleaseDC(hHeatMap, myDC);
	}
}

// set the window style to alter the menu and title bar settings
// title is only hidden in full screen mode
void SetMenuMode(bool showTitle, bool showMenu) {
    DWORD dwRemove = WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    DWORD dwStyle = ::GetWindowLong(myWnd, GWL_STYLE);
    bool changed = false;

    // Hide the menu bar, change styles and position and redraw
    //::LockWindowUpdate(myWnd); // prevent intermediate redrawing
    if (showMenu) {
        if (::GetMenu(myWnd) == NULL) {
            ::SetMenu(myWnd, myMenu);
            changed = true;
        }
    } else {
        if (::GetMenu(myWnd) != NULL) {
            ::SetMenu(myWnd, NULL);
            changed = true;
        }
    }
    if (showTitle) {
        if ((dwStyle & dwRemove) == 0) {
            ::SetWindowLong(myWnd, GWL_STYLE, dwStyle | dwRemove);
            changed = true;
        }
    } else {
        if ((dwStyle & dwRemove) != 0) {
            ::SetWindowLong(myWnd, GWL_STYLE, dwStyle & ~dwRemove);
            changed = true;
        }
    }
    //::LockWindowUpdate(NULL); // allow redrawing
    if (changed) {
        ::SetWindowPos(myWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
    }
}
