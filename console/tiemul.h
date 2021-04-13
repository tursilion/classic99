//
// (C) 2019 Mike Brent aka Tursi aka HarmlessLion.com
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
///////////////////////////////////////////////////
// Classic99 - header
// M.Brent
///////////////////////////////////////////////////

#include <atlstr.h>

// Defines
#define VERSION "QI399.042"
#define DEBUGLEN 120

typedef unsigned __int8 UINT8;
typedef unsigned __int8 Byte;
typedef unsigned __int16 Word;
typedef unsigned __int32 DWord;

// 3MHz seems to be correct via measurement - 5% slack is allowed per datasheet
#define CLOCK_MHZ 3000000
// note these actual timings as calculated from the VDP datasheet
// calculated it as 62.6 - but a long term interrupt count test gave 59.9
// so back to 60
#define HZ60 60
// calculate it as 50.23
#define HZ50 50
#define DEFAULT_60HZ_CPF (CLOCK_MHZ/HZ60)
#define DEFAULT_50HZ_CPF (CLOCK_MHZ/HZ50)
#define SLOW_CPF (10)
#define SPEECHRATE 8000	
#define SPEECHBUFFER 16000
#define MAX_BREAKPOINTS 10
#define MAXROMSPERCART	32
#define MAXUSERCARTS 1000
#define MAX_MRU 10

// note: not enough to change this - you also need to change the
// paging/padding calculation in the loader!
#define MAX_BANKSWITCH_SIZE (512*1024*1024)

// VDP status flags
#define VDPS_INT	0x80
#define VDPS_5SPR	0x40
#define VDPS_SCOL	0x20

// CPU status flags
#define BIT_LGT 0x8000
#define BIT_AGT 0x4000
#define BIT_EQ  0x2000
#define BIT_C   0x1000
#define BIT_OV  0x0800
#define BIT_OP  0x0400
#define BIT_XOP 0x0200
#define INTMASK 0x000F

// CPU halt sources (0-30)
#define HALT_SPEECH 0

// breakpoint types occupy the least significant byte (to allow the rest to hold data)
enum {
	BREAK_NONE = 0,
	BREAK_PC,
	BREAK_ACCESS,
	BREAK_WRITE,
	BREAK_WRITEVDP,
	BREAK_WRITEGROM,
	BREAK_READ,
	BREAK_READVDP,
	BREAK_READGROM,
	BREAK_EQUALS_WORD,
	BREAK_EQUALS_BYTE,
	BREAK_EQUALS_VDP,
	BREAK_EQUALS_VDPREG,
	BREAK_EQUALS_REGISTER,
	BREAK_RUN_TIMER,
	BREAK_DISK_LOG,
    BREAK_READAMS,
    BREAK_WRITEAMS,
    BREAK_EQUALS_AMS,
    BREAK_WP,
    BREAK_ST
};

#define BlitEvent Video_hdl[0]

#define TYPE_AUTO		'*'		// Magic! Actually, uses the autodetection - V9T9 filename based! Values can be zeroed.
#define TYPE_MBX		'!'		// MBX ROM (bank switched with RAM)
#define TYPE_379		'9'		// Packed banks accessed by writing to ROM space (inverted like XB, but all one file) ('3' is supported as legacy)
#define TYPE_378		'8'		// Packed banks accessed by writing to ROM space (NOT inverted like XB, but all one file)
#define TYPE_AMS		'A'		// RLE encoded memory dump from AMS card (RLE byte is 0 followed by a byte of zero runs)
#define TYPE_ROM		'C'		// CPU ROM
#define TYPE_NVRAM		'N'		// cartridge space has NVRAM - order of this parameter matters
#define TYPE_DSR		'D'		// DSR memory (CRU must be set)
#define TYPE_DSR2		'E'		// Paged DSR (from.. pcode?)
#define TYPE_GROM		'G'		// Standard GROM
#define TYPE_KEYS		'K'		// Paste string for the keyboard
#define TYPE_MPD		'M'		// Multiple Personality Distorter ROM - this enables lots of special code and may not be generally useful!
#define TYPE_OTHER		'O'		// loads from another Classic99 group (cart group/index)
#define TYPE_PCODEG		'P'		// P-Code card GROMs
#define TYPE_RAM		'R'		// RAM (CPU memory not read-only)
#define TYPE_SPEECH		'S'		// Speech ROM
#define TYPE_UBER_GROM	'U'		// My UBER GROM (simulation - just enough for testing)
#define TYPE_UBER_EEPROM 'T'	// UberGROM EEPROM memory (note: not saved back!)
#define TYPE_VDP		'V'		// VDP memory
#define TYPE_XB			'X'		// XB page 2 (full 8k available)
#define TYPE_NONE		' '
#define TYPE_UNSET		0

// these structs are manually copied into the cartpack project
struct IMG {
	DWORD dwImg;			// resource ID, NULL for disk type
	int  nLoadAddr;
	int  nLength;
	char nType;
	int  nBank;
	char szFileName[1024];	// filename if on disk, only if dwImg is NULL
};
struct DISKS {			// These are checked if a file can't be found in the real DSK1 ;)
	char szName[64];
	DWORD dwImg;		// resource ID
};
struct CARTS {
	char szName[MAX_PATH];
	struct IMG Img[MAXROMSPERCART];
	struct DISKS *pDisk;
	const char *szMessage;
	unsigned int nUserMenu;		// used in user carts only
};

// KEYBOARDS
enum {
	KEY_994,
	KEY_994A,
	KEY_994A_PS2
};

// debug
struct history {
	Word pc;										// last address for disasm
	int cycles;										// cycle count for disasm
	int bank;										// bank for disasm (-1 for GPU)
};

// Variables
#define REDRAW_LINES 262
extern int redraw_needed;							// redraw flag
extern int end_of_frame;							// end of frame flag
extern int skip_interrupt;							// flag for some instructions
extern int doLoadInt;								// flag for LOAD interrupt
extern Byte VDPREG[59];								// VDP read-only registers
extern Byte VDPS;									// VDP Status register
// Added by RasmusM
extern int F18AStatusRegisterNo;					// F18A Status register number
extern int F18AECModeSprite;						// F18A Enhanced color mode for sprites
extern int F18ASpritePaletteSize;					// Number of entries in each palette: 2, 4, 8 (depends on ECM)
extern int bF18ADataPortMode;						// F18A Data-port mode
extern int bF18AAutoIncPaletteReg;					// F18A Auto increment palette register
extern int F18APaletteRegisterNo;					// F18A Palette register number
extern int F18APaletteRegisterData;					// F18A Temporary storage of data written to palette register
extern int F18APalette[];							// 64 F18A palette registers
// RasmusM added end
#define VDPINT ((VDPS&VDPS_INT) && (VDPREG[1]&0x20))	// VDP hardware interrupt pin and mask
extern Word VDPADD;									// VDP Address counter
extern int vdpaccess;								// VDP access counter
extern int vdpwroteaddress;
extern int vdpscanline;
extern Byte vdpprefetch, vdpprefetchuninited;		// VDP prefetch (and if it was read from initialized RAM)
extern unsigned long hVideoThread;					// thread handle
extern int hzRate;									// flag for 50 or 60hz
extern int Recording;								// Flag for AVI recording
extern int RecordFrame;								// Current frame recorded (currently we only write 1/4 of the frames)
extern int MaintainAspect;							// Flag for Aspect ratio
extern int StretchMode;								// Setting for video stretching
extern int bUse5SpriteLimit;						// whether the sprite flicker is on
extern Byte VDP[128*1024];							// Video RAM
extern int bF18AActive;
extern int bF18Enabled;
extern HANDLE Video_hdl[2];							// Handles for Display/Blit events
extern unsigned int *framedata;						// The actual pixel data
extern unsigned int *framedata2;					// Filtered frame data
extern int FilterMode;								// Current filter mode
extern int nDefaultScreenScale;						// default screen scaling multiplier
extern int nXSize, nYSize;							// custom sizing

extern unsigned int CalculatedAudioBufferSize;
extern int AudioSampleRate;

extern HWND myWnd;									// Handle to windows
extern volatile HWND dbgWnd;						// Handle to windows
extern HDC myDC;									// Handle to Device Context
extern CRITICAL_SECTION VideoCS, DebugCS;			// Synchronization CS
extern int fontX, fontY;							// Non-proportional font x and y size
extern DWORD g_dwMyStyle;							// window style
extern volatile int quitflag;						// exit flag
extern int nCurrentDSR;								// Which DSR Bank are we on?
extern int nDSRBank[16];							// Is the DSR bank switched?

extern char key[256];								// keyboard state buffer

extern Byte *CPU2;				                    // Cartridge space bank-switched (malloc'd, don't exceed xbmask)
extern Byte ROMMAP[65536];							// Write-protect map of CPU space (todo: switch to bits to save RAM)
extern Byte CRU[4096];								// CRU space
extern Byte DSR[16][16384];							// 16 CRU bases, up to 16k each (ROM >4000 space)

extern Byte CPUMemInited[65536];					// not going to support AMS yet -- might switch to bits, but need to sort out AMS memory usage (16MB vs 1MB?)
extern Byte VDPMemInited[128*1024];
extern bool g_bCheckUninit;

struct GROMType {
	Byte GROM[65536];								// GROM space
	bool bWritable[8];								// which bases are writable
	Word GRMADD;									// GROM Address counter
	Byte grmaccess,grmdata;							// GROM Prefetch emulation
    int LastRead, LastBase;                         // Reports last access (stored on base 0 only)
};

extern struct GROMType GROMBase[17];				// support 16 GROM bases (there is room for 256 of them!), plus 1 for PCODE
#define PCODEGROMBASE 16							// which base we'll use for PCODE (highest + 1)

void memrnd(void *pRnd, int nCnt);

extern int PauseInactive;							// what to do when the window is inactive
extern int SpeechEnabled;							// whether or not speech is enabled
extern volatile int ThrottleMode;					// system throttling mode

extern char lines[34][DEBUGLEN];					// debug lines
extern bool bDebugDirty;

extern char *PasteString;							// Used for Edit->Paste
extern char *PasteIndex;
extern bool PasteStringHackBuffer;

extern volatile int xbBank;							// Cartridge bank switch
extern int xb;										// Is second bank (XB) loaded?
extern int grombanking;								// Are multiple GROM bases loaded?
extern HBITMAP hHeatBmp;							// reference to the heatmap Bitmap

struct _break {
	int Type;		// what kind of break point
	int A;			// usually address of the breakpoint
	int B;			// end of a range, bank, or unused
	int Bank;		// which bank to watch (cart only today, -1 for ignore)
	int Data;		// data, if needed (usually what to match)
	int Mask;		// optional mask to filter against (specify in braces) A mask of 0 is 0xffff
};

enum READACCESSTYPE {
    ACCESS_READ = 0,    // normal read
    ACCESS_RMW,         // read-before-write access, do not breakpoint
    ACCESS_FREE         // internal access, do not count or breakpoint
};

// Function prototypes
bool CheckRange(int nBreak, int x);

int InitAvi(bool bWithAudio);
void WriteFrame();
void WriteAudioFrame(void *pData, int nLen);
void CloseAVI();
void ConfigAVI();
void SaveScreenshot(bool bAuto, bool bFiltered);
void SetupSams(int sams_mode, int sams_size);

int getCharsPerLine();
char VDPGetChar(int x, int y, int width, int height);
CString captureScreen(int offsetByte);
void GetTVValues(double *hue, double *sat, double *cont, double *bright, double *sharp);
void SetTVValues(double hue, double sat, double cont, double bright, double sharp);
void VDPmain(void);
void vdpReset(bool isCold);
HRESULT InitDirectDraw( HWND hWnd );
void VDPdisplay(int scanline);
void updateVDP(int cycleCount);
void vdpForceFrame();
int  gettables(int isLayer2);
void draw_debug(void);
void VDPgraphics(int scanline, int isLayer2);
void VDPgraphicsII(int scanline, int isLayer2);
void VDPtext(int scanline, int isLayer2);
void VDPtextII(int scanline, int isLayer2);
void VDPtext80(int scanline, int isLayer2);
void VDPillegal(int scanline, int isLayer2);
void VDPmulticolor(int scanline, int isLayer2);
void VDPmulticolorII(int scanline, int isLayer2);
unsigned char getF18AStatus();
void debug_write(char *s, ...);
void doBlit(void);
void RenderFont(void);
void DrawSprites(int scanline);
void SetupDirectDraw(bool fullscreen);
void takedownDirectDraw();
int ResizeBackBuffer(int w, int h);
void UpdateHeatVDP(int Address);
void UpdateHeatGROM(int Address);
void UpdateHeatmap(int Address);

LONG_PTR FAR PASCAL myproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AudioBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK OptionsBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK KBMapProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK HeatMapProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK TVBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK GramBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void LaunchDebugWindow();
void pixel(int x, int y, int col);
void pixel80(int x, int y, int col);
void bigpixel(int x, int y, int col);
void spritepixel(int x, int y, int c);
// Added by RasmusM
int pixelMask(int addr, int F18ASpriteColorLine[]);
// RasmusM added end

void startvdp(void);
void startsound(void);
void warn(char[]);
void fail(char[]);
Word romword(Word, READACCESSTYPE rmw=ACCESS_READ);
void wrword(Word,Word);
void __cdecl emulti(void*);
void readroms(void);
void saveroms(void);
void do1(void);
void opcode0(Word);
void opcode02(Word);
void opcode03(Word); 
void opcode04(Word);
void opcode05(Word);
void opcode06(Word);
void opcode07(Word);
void opcode1(Word);
void opcode2(Word);
void opcode3(Word);
Byte rcpubyte(Word,READACCESSTYPE rmw=ACCESS_READ);
void wcpubyte(Word,Byte);
void increment_vdpadd();
Byte rvdpbyte(Word,READACCESSTYPE);
void wvdpbyte(Word,Byte);
void pset(int dx, int dy, int c, int a, int l);
Byte rspeechbyte(Word);
void wspeechbyte(Word, Byte);
void SpeechUpdate(int nSamples);
void wVDPreg(Byte,Byte);
void wsndbyte(Byte);
Byte rgrmbyte(Word,READACCESSTYPE);
void wgrmbyte(Word,Byte);
Byte rpcodebyte(Word);
void wpcodebyte(Word,Byte);
void wcru(Word,int);
int rcru(Word);
void fixDS(void);
void parity(Byte);
void op_a(void);
void op_ab(void);
void op_abs(void);
void op_ai(void);
void op_dec(void);
void op_dect(void);
void op_div(void);
void op_inc(void);
void op_inct(void);
void op_mpy(void);
void op_neg(void);
void op_s(void);
void op_sb(void);
void op_b(void);
void op_bl(void);
void op_blwp(void);
void op_jeq(void);
void op_jgt(void);
void op_jhe(void);
void op_jh(void);
void op_jl(void);
void op_jle(void);
void op_jlt(void);
void op_jmp(void);
void op_jnc(void);
void op_jne(void);
void op_jno(void);
void op_jop(void);
void op_joc(void);
void op_rtwp(void);
void op_x(void);
void op_xop(void);
void op_c(void);
void op_cb(void);
void op_ci(void);
void op_coc(void);
void op_czc(void);
void op_ldcr(void);
void op_sbo(void);
void op_sbz(void);
void op_stcr(void);
void op_tb(void);
void op_ckof(void);
void op_ckon(void);
void op_idle(void);
void op_rset(void);
void op_lrex(void);
void op_li(void);
void op_limi(void);
void op_lwpi(void);
void op_mov(void);
void op_movb(void);
void op_stst(void);
void op_stwp(void);
void op_swpb(void);
void op_andi(void);
void op_ori(void);
void op_xor(void);
void op_inv(void);
void op_clr(void);
void op_seto(void);
void op_soc(void);
void op_socb(void);
void op_szc(void);
void op_szcb(void);
void op_sra(void);
void op_srl(void);
void op_sla(void);
void op_src(void);
void op_bad(void);

int Dasm9900 (char *buffer, int pc, int nBank);

void InitDiskDSR();
bool HandleDisk();
void updateCallFiles(int newTop);
void verifyCallFiles();

void DoPause();
void DoStep();
void DoStepOver();
void DoPlay();
void DoFastForward();
void DoMemoryDump();
void DoLoadInterrupt();
void TriggerBreakPoint(bool bForce = false);

int nodot(void);
Byte GetSafeCpuByte(int x, int bank);
Word GetSafeCpuWord(int x, int bank);

void read_sect(Byte drive, int sect, char *buffer);

// tape
void updateTape(int nCPUCycles);
void setTapeMotor(bool isOn);
void forceTapeMotor(bool isOn);
bool getTapeBit();
void LoadTape();

#if 0		// commented on main code
void read_image_file(int PAB, char *buffer, int offset, int len);
#endif

void ConsoleInterrupt(void);

void __cdecl TimerThread(void *);
void Counting();
void __cdecl SpeechBufThread(void *);
void WindowThread();

// replaces the old dual throttle
#define THROTTLE_NONE 9999
#define THROTTLE_SLOW -1
#define THROTTLE_NORMAL 0
#define THROTTLE_OVERDRIVE 1
#define THROTTLE_SYSTEMMAXIMUM 2

