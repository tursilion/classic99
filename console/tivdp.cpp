//
// (C) 2005-2014 Mike Brent aka Tursi aka HarmlessLion.com
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
///////////////////////////////////////////////////////
// Classic99 VDP Routines
// M.Brent
///////////////////////////////////////////////////////


#if 0
F18A Reset from Matt:

0 in all registers, except:

VR1 = >40 (no blanking, the 4K/16K bit is ignored in the F18A)
VR3 = >10 (color table at >0400)
VR4 = >01 (pattern table at >0800)
VR5 = >0A (sprite table at >0500)
VR6 = >02 (sprite pattern table at >1000)
VR7 = >1F (fg=black, bg=white)
VR30 = sprite_max (set from external jumper setting)
VR48 = 1 (increment set to 1)
VR51 = 32 (stop sprite to max)
VR54 = >40 (GPU PC MSB)
VR55 = >00 (GPU PC LBS)
VR58 = 6 (GROMCLK divider)

The real 9918A will set all VRs to 0, which basically makes the screen black, blank, and off, 4K VRAM selected, and no interrupts. 

Note that nothing restores the F18A palette registers to the power-on defaults, other than a power on.

As for the GPU, the VR50 >80 reset will *not* stop the GPU, and if the GPU code is modifying VDP registers, then it can overwrite the reset values.  However, since the reset does clear VR50, the horizontal and vertical interrupt enable bits will be cleared, and thus the GPU will not be triggered on those events.

The reset also changes VR54 and VR55, but they are *not* loaded to the GPU PC (program counter).  The only events that change the GPU PC are:

* normal GPU instruction execution.
* the external hardware reset.
* writing to VR55 (GPU PC LSB).
* writing >00 to VR56 (load GPU PC from VR54 and VR55, then idle).

#endif


#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <windows.h>
#include <ddraw.h>
#include <commctrl.h>
#include <commdlg.h>
#include <atlstr.h>
#include <time.h>

#include "tiemul.h"
#include "..\resource.h"
#include "..\2xSaI\2xSaI.h"
#include "..\FilterDLL\sms_ntsc.h"
#include "cpu9900.h"

// 16-bit 0rrrrrgggggbbbbb values
//int TIPALETTE[16]={ 
//	0x0000, 0x0000, 0x1328, 0x2f6f, 0x295d, 0x3ddf, 0x6949, 0x23be,
//	0x7d4a, 0x7def, 0x6b0a, 0x7330, 0x12c7, 0x6577, 0x6739, 0x7fff,
//};
// 32-bit 0RGB colors
//unsigned int TIPALETTE[16] = {
//	0x00000000,0x00000000,0x0020C840,0x0058D878,0x005050E8,0x007870F8,0x00D05048,
//	0x0040E8F0,0x00F85050,0x00F87878,0x00D0C050,0x00E0C880,0x0020B038,0x00C858B8,
//	0x00C8C8C8,0x00F8F8F8
//};
// 12-bit 0RGB colors (we shift up to 24 bit when we load it)
const int F18APaletteReset[64] = {
	// standard 9918A colors
	0x0000,  // 0 Transparent
	0x0000,  // 1 Black
	0x02C3,  // 2 Medium Green
	0x05D6,  // 3 Light Green
	0x054F,  // 4 Dark Blue
	0x076F,  // 5 Light Blue
	0x0D54,  // 6 Dark Red
	0x04EF,  // 7 Cyan
	0x0F54,  // 8 Medium Red
	0x0F76,  // 9 Light Red
	0x0DC3,  // 10 Dark Yellow
	0x0ED6,  // 11 Light Yellow
	0x02B2,  // 12 Dark Green
	0x0C5C,  // 13 Magenta
	0x0CCC,  // 14 Gray
	0x0FFF,  // 15 White
	// EMC1 version of palette 0
	0x0000,  // 0 Black
	0x02C3,  // 1 Medium Green
	0x0000,  // 2 Black
	0x054F,  // 3 Dark Blue
	0x0000,  // 4 Black
	0x0D54,  // 5 Dark Red
	0x0000,  // 6 Black
	0x04EF,  // 7 Cyan
	0x0000,  // 8 Black
	0x0CCC,  // 9 Gray
	0x0000,  // 10 Black
	0x0DC3,  // 11 Dark Yellow
	0x0000,  // 12 Black9
	0x0C5C,  // 13 Magenta
	0x0000,  // 14 Black
	0x0FFF,  // 15 White
	// IBM CGA colors
	0x0000,  // 0 >000000 ( 0 0 0) black
	0x000A,  // 1 >0000AA ( 0 0 170) blue
	0x00A0,  // 2 >00AA00 ( 0 170 0) green
	0x00AA,  // 3 >00AAAA ( 0 170 170) cyan
	0x0A00,  // 4 >AA0000 (170 0 0) red
	0x0A0A,  // 5 >AA00AA (170 0 170) magenta
	0x0A50,  // 6 >AA5500 (170 85 0) brown
	0x0AAA,  // 7 >AAAAAA (170 170 170) light gray
	0x0555,  // 8 >555555 ( 85 85 85) gray
	0x055F,  // 9 >5555FF ( 85 85 255) light blue
	0x05F5,  // 10 >55FF55 ( 85 255 85) light green
	0x05FF,  // 11 >55FFFF ( 85 255 255) light cyan
	0x0F55,  // 12 >FF5555 (255 85 85) light red
	0x0F5F,  // 13 >FF55FF (255 85 255) light magenta
	0x0FF5,  // 14 >FFFF55 (255 255 85) yellow
	0x0FFF,  // 15 >FFFFFF (255 255 255) white
	// ECM1 version of palette 2
	0x0000,  // 0 >000000 ( 0 0 0) black
	0x0555,  // 1 >555555 ( 85 85 85) gray
	0x0000,  // 2 >000000 ( 0 0 0) black
	0x000A,  // 3 >0000AA ( 0 0 170) blue
	0x0000,  // 4 >000000 ( 0 0 0) black
	0x00A0,  // 5 >00AA00 ( 0 170 0) green
	0x0000,  // 6 >000000 ( 0 0 0) black
	0x00AA,  // 7 >00AAAA ( 0 170 170) cyan
	0x0000,  // 8 >000000 ( 0 0 0) black
	0x0A00,  // 9 >AA0000 (170 0 0) red
	0x0000,  // 10 >000000 ( 0 0 0) black
	0x0A0A,  // 11 >AA00AA (170 0 170) magenta
	0x0000,  // 12 >000000 ( 0 0 0) black
	0x0A50,  // 13 >AA5500 (170 85 0) brown
	0x0000,  // 14 >000000 ( 0 0 0) black
	0x0FFF   // 15 >FFFFFF (255 255 255) white
};

char *digpat[10][5] = {
	"111",
	"101",
	"101",
	"101",
	"111",

    "010",
	"110",
	"010",
	"010",
	"111",

    "111",
	"001",
	"111",
	"100",
	"111",

    "111",
	"001",
	"011",
	"001",
	"111",

    "101",
	"101",
	"111",
	"001",
	"001",

    "111",
	"100",
	"111",
	"001",
	"111",

    "111",
	"100",
	"111",
	"101",
	"111",

    "111",
	"001",
	"001",
	"010",
	"010",

    "111",
	"101",
	"111",
	"101",
	"111",

    "111",
	"101",
	"111",
	"001",
	"111"
};

// this draws a full frame for overdrive. It must be negative and more than
// a reasonable number of CPU instructions, cause smaller magnitude negative
// numbers mean the half-instruction update that we do...
#define FULLFRAME (-1000000)

// TODO: this is only for tiles, and only for ECM0
#define GETPALETTEVALUE(n) F18APalette[(bF18AActive?((VDPREG[0x18]&03)<<4)+(n) : (n))]

int SIT;									// Screen Image Table
int CT;										// Color Table
int PDT;									// Pattern Descriptor Table
int SAL;									// Sprite Allocation Table
int SDT;									// Sprite Descriptor Table
int CTsize;									// Color Table size in Bitmap Mode
int PDTsize;								// Pattern Descriptor Table size in Bitmap Mode

Byte VDP[128*1024];							// Video RAM (16k, except for now we are faking the rest of the VDP address space for F18A (todo: only 18k on real chip))
Byte SprColBuf[256][192];					// Sprite Collision Buffer
Byte HeatMap[256*256*3];					// memory access heatmap (red/green/blue = CPU/VDP/GROM)
int SprColFlag;								// Sprite collision flag
int bF18AActive = 0;						// was the F18 activated?
int bF18Enabled = 1;						// is it even enabled?
int bInterleaveGPU = 1;						// whether to run the GPU and the CPU together (impedes debug - temporary option)

IDirectDraw7 *lpdd=NULL;					// DirectDraw object
LPDIRECTDRAWSURFACE7 lpdds=NULL;			// Primary surface
LPDIRECTDRAWSURFACE7 ddsBack=NULL;			// Back buffer
LPDIRECTDRAWCLIPPER  lpDDClipper=NULL;		// Window clipper
DDSURFACEDESC2 CurrentDDSD;					// current back buffer settings

// deprecated directdraw functions we need to manually extract from ddraw.dll
typedef HRESULT (WINAPI* LPDIRECTDRAWCREATEEX )( GUID FAR * lpGuid, LPVOID  *lplpDD, REFIID  iid,IUnknown FAR *pUnkOuter );

int FilterMode=0;							// Current filter mode
int nDefaultScreenScale=1;					// default screen scale multiplier
int nXSize=256, nYSize=192;					// custom sizing
int TVFiltersAvailable=0;					// Depends on whether we can load the Filter DLL
int TVScanLines=1;							// Whether to draw scanlines or not
int VDPDebug=0;								// When set, displays all 256 chars
int bShowFPS=0;								// whether to show FPS
int bEnable80Columns=1;						// Enable the beginnings of the 80 column mode - to replace someday with F18A
int bEnable128k=0;							// disabled by default - it's a non-real-world combination of F18 and 9938, so HACK.
#define TV_WIDTH (602+32)					// how wide is TV mode really?

// keyboard debug - done inline like the FPS
int bShowKeyboard=0;                        // when set, draw the keyboard debug
extern unsigned char capslock, lockedshiftstate;
extern unsigned char scrolllock,numlock;
extern unsigned char ticols[8];
extern int fJoystickActiveOnKeys;
extern unsigned char ignorecount;
extern unsigned char fctnrefcount,shiftrefcount,ctrlrefcount;

// audio per-frame logging
extern int logAudio;
extern void writeAudioLogState();

// menu display
extern int bEnableAppMode;
extern void SetMenuMode(bool showTitle, bool showMenu);

sms_ntsc_t tvFilter;						// TV Filter structure
sms_ntsc_setup_t tvSetup;					// TV Setup structure
HMODULE hFilterDLL;							// Handle to Filter DLL
void (*sms_ntsc_init)(sms_ntsc_t *ntsc, sms_ntsc_setup_t const *setup);	// pointer to init function
void (*sms_ntsc_blit)(sms_ntsc_t const *ntsc, unsigned int const *sms_in, long in_row_width, int in_width, int height, void *rgb_out, long out_pitch);
																		// pointer to blit function
void (*sms_ntsc_scanlines)(void *pFrame, int nWidth, int nStride, int nHeight);

HMODULE hHQ4DLL;							// Handle to HQ4x DLL
void (*hq4x_init)(void);
void (*hq4x_process)(unsigned char *pBufIn, unsigned char *pBufOut);

HANDLE Video_hdl[2];						// Handles for Display/Blit events
unsigned int *framedata;					// The actual pixel data
unsigned int *framedata2;					// Filtered pixel data
BITMAPINFO myInfo;							// Bitmapinfo header for the DIB functions
BITMAPINFO myInfo2;							// Bitmapinfo header for the DIB functions
BITMAPINFO myInfo32;						// Bitmapinfo header for the DIB functions
BITMAPINFO myInfoTV;						// Bitmapinfo header for the DIB functions
BITMAPINFO myInfo80Col;						// Bitmapinfo header for the DIB functions
HDC tmpDC;									// Temporary DC for StretchBlt to work from

int redraw_needed;							// redraw flag
int end_of_frame;							// end of frame flag (move this to tiemul.cpp, not used in VDP)
int skip_interrupt;							// flag for some instructions TODO: WTF are these two CPU flags doing in VDP?
int doLoadInt;								// execute a LOAD after this instruction
Byte VDPREG[59];							// VDP read-only registers (9918A has 8, we define 9 to support 80 cols, and the F18 has 59 (!) (and 16 status registers!))
Byte VDPS;									// VDP Status register

int fullscreenX;							// current res of full screen X (cause GetSystemMetrics is slow)
int fullscreenY;							// current res of full screen Y

// Added by RasmusM
int F18AStatusRegisterNo = 0;				// F18A Status register number
int F18AECModeSprite = 0;					// F18A Enhanced color mode for sprites (0 = normal, 1 = 1 bit color mode, 2 = 2 bit color mode, 3 = 3 bit color mode)
int F18ASpritePaletteSize = 16;				// F18A Number of entries in each palette: 2, 4, 8 (depends on ECM)
int bF18ADataPortMode = 0;					// F18A Data-port mode
int bF18AAutoIncPaletteReg = 0;				// F18A Auto increment palette register
int F18APaletteRegisterNo = 0;				// F18A Palette register number
int F18APaletteRegisterData = -1;			// F18A Temporary storage of data written to palette register
int F18APalette[64];
// RasmusM added end

Word VDPADD;								// VDP Address counter
int vdpaccess;								// VDP address write flipflop (low/high)
int vdpwroteaddress;						// VDP (instruction) countdown after writing an address (weak test)
int vdpscanline;							// current line being processed, 0-262
											// I think it's more or less right:
											// 0-26 = top blanking
											// 27-219 = 192 lines of display
											// 220-261 = bottom blanking + vblank
Byte vdpprefetch,vdpprefetchuninited;		// VDP Prefetch
unsigned long hVideoThread;					// thread handle
int hzRate;									// flag for 50 or 60hz
int Recording;								// Flag for AVI recording
int MaintainAspect;							// Flag for Aspect ratio
int StretchMode;							// Setting for video stretching
int bUse5SpriteLimit;						// whether the sprite flicker is on
bool bDisableBlank, bDisableSprite, bDisableBackground;	// other layers :)
bool bDisableColorLayer, bDisablePatternLayer;          // bitmap only layers

extern int fontX, fontY;					// Font dimensions
extern HANDLE hDebugWindowUpdateEvent;		// debug draw event
extern int nSystem;							// which system we are using (detect 99/4)
extern int drawspeed;						// frameskip... sorta. Not sure this is still valuable
extern int nVideoLeft, nVideoTop;
extern int max_cpf;							// current CPU performance
extern CPU9900 *pGPU;
extern int statusFrameCount;

//////////////////////////////////////////////////////////
// Helpers for the TV controls
//////////////////////////////////////////////////////////
void GetTVValues(double *hue, double *sat, double *cont, double *bright, double *sharp) {
	*hue=tvSetup.hue;
	*sat=tvSetup.saturation;
	*cont=tvSetup.contrast;
	*bright=tvSetup.brightness;
	*sharp=tvSetup.sharpness;
}

void SetTVValues(double hue, double sat, double cont, double bright, double sharp) {
	tvSetup.hue=hue;
	tvSetup.saturation=sat;
	tvSetup.contrast=cont;
	tvSetup.brightness=bright;
	tvSetup.sharpness=sharp;
	if (sms_ntsc_init) {
		if (!TryEnterCriticalSection(&VideoCS)) {
			return;		// do it later
		}
		sms_ntsc_init(&tvFilter, &tvSetup);
		LeaveCriticalSection(&VideoCS);
	}
}

//////////////////////////////////////////////////////////
// Get table addresses from Registers
// We return reg0 since we do the bitmap filter here now
//////////////////////////////////////////////////////////
int gettables(int isLayer2)
{
	int reg0 = VDPREG[0];
	if (nSystem == 0) {
		// disable bitmap for 99/4
		reg0&=~0x02;
	}
	if (!bEnable80Columns) {
		// disable 80 columns if not enabled
		reg0&=~0x04;
	}

	/* Screen Image Table */
	if ((bEnable80Columns) && (reg0 & 0x04)) {
		// in 80-column text mode, the two LSB are some kind of mask that we here ignore - the rest of the register is larger
		// The 9938 requires that those bits be set to 11, therefore, the F18A treats 11 and 00 both as 00, but treats
		// 01 and 10 as their actual values. (Okay, that is a bit weird.) That said, the F18A still only honours the least
		// significant 4 bits and ignores the rest (the 9938 reads 7 bits instead of 4, masking as above).
		// So anyway, the goal is F18A support, but the 9938 mask would be 0x7C instead of 0x0C, and the shift was only 8?
		// TODO: check the 9938 datasheet - did Matthew get it THAT wrong? Or does the math work out anyway?
		// Anyway, this works for table at >0000, which is most of them.
		SIT=(VDPREG[2]&0x0F);
		if ((SIT&0x03)==0x03) SIT&=0x0C;	// mask off a 0x03 pattern, 0x00,0x01,0x02 left alone
		SIT<<=10;
	} else {
		SIT=((VDPREG[2]&0x0f)<<10);
	}
	/* Sprite Attribute List */
	SAL=((VDPREG[5]&0x7f)<<7);
	/* Sprite Descriptor Table */
	SDT=((VDPREG[6]&0x07)<<11);

	// The normal math for table addresses isn't quite right in bitmap mode
	// The PDT and CT have different math and a size setting
	if (reg0&0x02) {
		// this is for bitmap modes
		CT=(VDPREG[3]&0x80) ? 0x2000 : 0;
		CTsize=((VDPREG[3]&0x7f)<<6)|0x3f;
		PDT=(VDPREG[4]&0x04) ? 0x2000 : 0;
		PDTsize=((VDPREG[4]&0x03)<<11);
		if (VDPREG[1]&0x10) {	// in Bitmap text, we fill bits with 1, as there is no color table
			PDTsize|=0x7ff;
		} else {
			PDTsize|=(CTsize&0x7ff);	// In other bitmap modes we get bits from the color table mask
		}
	} else {
		// this is for non-bitmap modes
		/* Colour Table */
		CT=VDPREG[3]<<6;
		/* Pattern Descriptor Table */
		PDT=((VDPREG[4]&0x07)<<11);
		CTsize=32;
		PDTsize=2048;

        if (isLayer2) {
            // get the F18A layer information
            // TODO: bigger tables are possible with F18A
		    /* Colour Table */
		    CT=VDPREG[11]<<6;
		    CTsize=32;
            /* Screen Image Table */
		    SIT=((VDPREG[10]&0x0f)<<10);

            /* Pattern Descriptor Table */
            // TODO: Matt says there's a second pattern table, but I don't see it in the docs.
		    //PDT=((VDPREG[4]&0x07)<<11);
		    //PDTsize=2048;
        }
	}

    return reg0;
}

// called from tiemul
void vdpReset(bool isCold) {
    // on cold reset, reload everything. On warm reset (F18A only), we don't reset the palette
    if (isCold) {
	    // todo: move the other system-level init (what does the VDP do?) into here
	    memcpy(F18APalette, F18APaletteReset, sizeof(F18APalette));
	    // convert from 12-bit to 24-bit
	    for (int idx=0; idx<64; idx++) {
		    int r = (F18APalette[idx]&0xf00)>>8;
		    int g = (F18APalette[idx]&0xf0)>>4;
		    int b = (F18APalette[idx]&0xf);
		    F18APalette[idx] = (r<<20)|(r<<16)|(g<<12)|(g<<8)|(b<<4)|b;	// double up each palette gun, suggestion by Sometimes99er
	    }
    }
    bF18AActive = false;
	redraw_needed = REDRAW_LINES;
    memset(VDPREG, 0, sizeof(VDPREG));
}

////////////////////////////////////////////////////////////
// Startup and run VDP graphics interface
////////////////////////////////////////////////////////////
void VDPmain()
{	
	DWORD ret;
	HDC myDC;

	Init_2xSaI(888);

	// load the Filter DLL
	TVFiltersAvailable=0;
	hFilterDLL=LoadLibrary("FilterDll.dll");
	if (NULL == hFilterDLL) {
		debug_write("Failed to load filter library.");
	} else {
		sms_ntsc_init=(void (*)(sms_ntsc_t*,sms_ntsc_setup_t const *))GetProcAddress(hFilterDLL, "sms_ntsc_init");
		sms_ntsc_blit=(void (*)(sms_ntsc_t const *, unsigned int const *, long, int, int, void *, long))GetProcAddress(hFilterDLL, "sms_ntsc_blit");
		sms_ntsc_scanlines=(void (*)(void *, int, int, int))GetProcAddress(hFilterDLL, "sms_ntsc_scanlines");
		if ((NULL == sms_ntsc_blit) || (NULL == sms_ntsc_init) || (NULL == sms_ntsc_scanlines)) {
			debug_write("Failed to find entry points in filter library.");
			FreeLibrary(hFilterDLL);
			sms_ntsc_blit=NULL;
			sms_ntsc_init=NULL;
			sms_ntsc_scanlines=NULL;
			hFilterDLL=NULL;
		} else {
			// Some of these are set up by SetTVValues()
//			tvSetup.hue=0;			// -1.0 to +1.0
//			tvSetup.saturation=0;	// -1.0 to +1.0
//			tvSetup.contrast=0;		// -1.0 to +1.0
//			tvSetup.brightness=0;	// -1.0 to +1.0
//			tvSetup.sharpness=0;	// -1.0 to +1.0
			tvSetup.gamma=0;		// -1.0 to +1.0
			tvSetup.resolution=0;	// -1.0 to +1.0
			tvSetup.artifacts=0;	// -1.0 to +1.0
			tvSetup.fringing=0;		// -1.0 to +1.0
			tvSetup.bleed=0;		// -1.0 to +1.0
			tvSetup.decoder_matrix=NULL;
			tvSetup.palette_out=NULL;
			sms_ntsc_init(&tvFilter, &tvSetup);
			TVFiltersAvailable=1;
		}
	}

	hHQ4DLL=LoadLibrary("HQ4xDll.dll");
	if (NULL == hHQ4DLL) {
		debug_write("Failed to load HQ4 library.");
	} else {
		hq4x_init=(void (*)(void))GetProcAddress(hHQ4DLL, "hq4x_init");
		hq4x_process=(void (*)(unsigned char *pBufIn, unsigned char *pBufOut))GetProcAddress(hHQ4DLL, "hq4x_process");
		if ((NULL == hq4x_init) || (NULL == hq4x_process)) {
			debug_write("Failed to find entry points in HQ4x library.");
			FreeLibrary(hHQ4DLL);
			hq4x_init=NULL;
			hq4x_process=NULL;
			hHQ4DLL=NULL;
		} else {
			hq4x_init();
		}
	}

	myInfo.bmiHeader.biSize=sizeof(myInfo.bmiHeader);
	myInfo.bmiHeader.biWidth=256+16;
	myInfo.bmiHeader.biHeight=192+16;
	myInfo.bmiHeader.biPlanes=1;
	myInfo.bmiHeader.biBitCount=32;
	myInfo.bmiHeader.biCompression=BI_RGB;
	myInfo.bmiHeader.biSizeImage=0;
	myInfo.bmiHeader.biXPelsPerMeter=1;
	myInfo.bmiHeader.biYPelsPerMeter=1;
	myInfo.bmiHeader.biClrUsed=0;
	myInfo.bmiHeader.biClrImportant=0;

	memcpy(&myInfo2, &myInfo, sizeof(myInfo));
	myInfo2.bmiHeader.biWidth=512+32;
	myInfo2.bmiHeader.biHeight=384+29;

	memcpy(&myInfoTV, &myInfo2, sizeof(myInfo2));
	myInfoTV.bmiHeader.biWidth=TV_WIDTH;

	memcpy(&myInfo32, &myInfo, sizeof(myInfo));
	myInfo32.bmiHeader.biWidth*=4;
	myInfo32.bmiHeader.biHeight*=4;
	myInfo32.bmiHeader.biBitCount=32;

	memcpy(&myInfo80Col, &myInfo, sizeof(myInfo));
	myInfo80Col.bmiHeader.biWidth=512+16;

	myDC=GetDC(myWnd);
	tmpDC=CreateCompatibleDC(myDC);
	ReleaseDC(myWnd, myDC);

	SetupDirectDraw(false);

	// now we create a waitable object and sit on it - the main thread
	// will tell us when we should redraw the screen.
	BlitEvent=CreateEvent(NULL, false, false, NULL);
	if (NULL == BlitEvent)
		debug_write("Blit Event Creation failed");

	// layers all enabled at start
	bDisableBlank=false;
	bDisableSprite=false;
	bDisableBackground=false;

	debug_write("Starting video loop");
	redraw_needed=REDRAW_LINES;

	while (quitflag==0)
	{
		if ((ret=WaitForMultipleObjects(1, Video_hdl, false, 100)) != WAIT_TIMEOUT)
		{
			if (WAIT_FAILED==ret)
				ret=GetLastError();

			if (WAIT_OBJECT_0 == ret) {
				doBlit();
                continue;
			}

			// Don't ever spend all our time doing this!
			Sleep(5);		// rounds up to quantum
		}
	}

	if (NULL != hFilterDLL) {
		sms_ntsc_blit=NULL;
		sms_ntsc_init=NULL;
		sms_ntsc_scanlines=NULL;
		FreeLibrary(hFilterDLL);
		hFilterDLL=NULL;
	}
	takedownDirectDraw();
	DeleteDC(tmpDC);
	CloseHandle(BlitEvent);
}

// used by the GetChar and capture functions
// returns 32, 40 or 80, or -1 if invalid
int getCharsPerLine() {
	int nCharsPerLine=32;	// default for graphics mode

	int reg0 = gettables(0);

	if (!(VDPREG[1] & 0x40))		// Disable display
	{
		return -1;
	}

	if ((VDPREG[1] & 0x18)==0x18)	// MODE BITS 2 and 1
	{
		return -1;
	}

	if (VDPREG[1] & 0x10)			// MODE BIT 2
	{
		if (reg0 & 0x02) {			// BITMAP MODE BIT
			return -1;
		}
		
		nCharsPerLine=40;
		if (reg0&0x04) {
			// 80 column text (512+16 pixels across instead of 256+16)
			nCharsPerLine = 80;
		}
	}

	if (VDPREG[1] & 0x08)			// MODE BIT 1
	{
		return -1;
	}

	if (reg0 & 0x02) {			// BITMAP MODE BIT
		return -1;
	}

    return nCharsPerLine;
}


// Passed Windows mouse based X and Y, figure out the char under
// the pointer. If it's not a text mode or it's not printable, then
// return -1. Due to screen borders, we have a larger area than
// the TI actually displays.
char VDPGetChar(int x, int y, int width, int height) {
	double nCharWidth=34.0;	// default for graphics mode (32 chars plus 2 chars of border)
	int ch;
	int nCharsPerLine=getCharsPerLine();

    if (nCharsPerLine == -1) {
        return -1;
    }

    // update chars width (number of chars across entire screen)
    if (nCharsPerLine == 40) {
		nCharWidth=45.3;			// text mode (40 chars plus 5 chars of border)
    } else if (nCharsPerLine == 80) {
    	nCharWidth = 88.0;
    }

	// If it wasn't text and we got here, then it's multicolor
	// nCharWidth now has the number of columns across. There are
	// always 24+2 rows.
	double nXWidth, nYHeight;

	nYHeight=height/26.0;
	nXWidth=width/nCharWidth;

	if ((nXWidth<1.0) || (nYHeight<1.0)) {
		// screen is too small to differentiate (this is unlikely)
		return -1;
	}

	int row=(int)(y/nYHeight)-1;
	if ((row < 0) || (row > 23)) {
		return -1;
	}

	int col;
	if (nCharWidth > 39.0) {
		// text modes
		col=(int)((x/nXWidth)-2.6);
		if ((col < 0) || (col >= nCharsPerLine)) {
			return -1;
		}
	} else {
		col=(int)(x/nXWidth)-1;
		if ((col < 0) || (col > 31)) {
			return -1;
		}
	}

	ch=VDP[SIT+(row*nCharsPerLine)+col];

	if (isprint(ch)) {
		return ch;
	}

	// this is really hacky but it should work ;)
	// handle the TI BASIC character offset
	if ((ch >= 0x80) && (isprint(ch-0x60))) {
		return ch-0x60;
	}

	return -1;
}

//////////////////////////////////////////////////////////
// Perform drawing of a single line
// Determines which screen mode to draw
//////////////////////////////////////////////////////////
void VDPdisplay(int scanline)
{
	int idx;
	DWORD longcol;
	DWORD *plong;
	int nMax;

	// reduce noisy lines
	EnterCriticalSection(&VideoCS);

	int reg0 = gettables(0);

	int gfxline = scanline - 27;	// skip top border

	if (redraw_needed) {
		// count down scanlines to redraw
		--redraw_needed;

		// draw blanking area
		if ((vdpscanline >= 0) && (vdpscanline < 192+27+24)) {
			// extra hack - we only have 8 pixels of border on each side, unlike the real VDP
			int tmplin = (199-(vdpscanline - 19-8));		// 27-8 = 19, don't remember where 199 comes from though...
			if ((tmplin >= 0) && (tmplin < 192+16)) {
				plong=(DWORD*)framedata;
				longcol=GETPALETTEVALUE(VDPREG[7]&0xf);
				if ((reg0&0x04)&&(VDPREG[1]&0x10)&&(bEnable80Columns)) {
					// 80 column text
					nMax = (512+16)/4;
				} else {
					// all other modes
					nMax = (256+16)/4;
				}
				plong += (tmplin*nMax*4);

				for (idx=0; idx<nMax; idx++) {
					*(plong++)=longcol;
					*(plong++)=longcol;
					*(plong++)=longcol;
					*(plong++)=longcol;
				}
			}
		}

		if (!bDisableBlank) {
			if (!(VDPREG[1] & 0x40)) {	// Disable display
				LeaveCriticalSection(&VideoCS);
				return;
			}
		}

		if ((!bDisableBackground) && (gfxline < 192) && (gfxline >= 0)) {
            for (int isLayer2=0; isLayer2<2; ++isLayer2) {
                reg0 = gettables(isLayer2);

                if ((VDPREG[1] & 0x18)==0x18)	// MODE BITS 2 and 1
			    {
				    VDPillegal(gfxline, isLayer2);
			    } else if (VDPREG[1] & 0x10)			// MODE BIT 2
			    {
				    if (reg0 & 0x02) {			// BITMAP MODE BIT
					    VDPtextII(gfxline, isLayer2);	// undocumented bitmap text mode
				    } else if (reg0 & 0x04) {	// MODE BIT 4 (9938)
					    VDPtext80(gfxline, isLayer2);	// 80-column text, similar to 9938/F18A
				    } else {
					    VDPtext(gfxline, isLayer2);		// regular 40-column text
				    }
			    } else if (VDPREG[1] & 0x08)				// MODE BIT 1
			    {
				    if (reg0 & 0x02) {				// BITMAP MODE BIT
					    VDPmulticolorII(gfxline, isLayer2);	// undocumented bitmap multicolor mode
				    } else {
					    VDPmulticolor(gfxline, isLayer2);
				    }
			    } else if (reg0 & 0x02) {					// BITMAP MODE BIT
				    VDPgraphicsII(gfxline, isLayer2);		// documented bitmap graphics mode
			    } else {
				    VDPgraphics(gfxline, isLayer2);
			    }

                // Tile layer 2, if applicable
                // TODO: sprite priority is not taken into account
			    if ((!bF18AActive) || ((VDPREG[49]&0x80)==0)) {
                    break;
                }
            }
		} else {
            // This case is hit if nothing else is being drawn, otherwise the graphics modes call DrawSprites
			// as long as mode bit 2 is not set, sprites are okay
			if ((bF18AActive) || ((VDPREG[1] & 0x10) == 0)) {
				DrawSprites(gfxline);
			}
		}
	} else {
		// we have to redraw the sprites even if the screen didn't change, so that collisions are updated
		// as the CPU may have cleared the collision bit
		// as long as mode bit 2 (text) is not set, and the display is enabled, sprites are okay
		if ((bF18AActive) || ((VDPREG[1] & 0x10) == 0)) {
			if ((bDisableBlank) || (VDPREG[1] & 0x40)) {
				DrawSprites(gfxline);
			}
		}
	}

	LeaveCriticalSection(&VideoCS);
}

//////////////////////////////////////////////////////////
// Perform drawing by elapsed CPU time
// Determines which screen mode to draw, and where
//////////////////////////////////////////////////////////
void updateVDP(int cycleCount)
{
	static double nCycles = 0;

	// Do we need to care about hzRate? I think we just
	// care about max_cpf. The VDP is always byte based, so
	// based on the cycle count we can always figure out where
	// we are. Can any instruction take long enough that
	// we need to loop? Longest is DIV at up to 148 cycles,
	// although it's very possible for hardware like the speech
	// synth to cause longer delays. 
	// at 60hz (fastest scan, I think):
	// frame: 50,000 cycles
	// line: 190.83969 cycles
	// byte: 5.96374 cycles
	// So, yeah. They all take more than one byte. :)

    // despite the above, max_cpf fluctates for too many things so we're just
    // going to try to keep drawing, and see if that works better. So we'll use
    // hzRate after all with the fixed values.

	// counts from datasheet
	double cyclesPerLine = ((hzRate==HZ50?DEFAULT_50HZ_CPF:DEFAULT_60HZ_CPF) / 262.0);	// 262 lines, 342 pixel clocks per line, max_cpf could change by config
	// this will just be scanlines for convenience, not pixel (or byte) accurate
	double newCycles;

	// handle overdrive
    if (ThrottleMode == THROTTLE_OVERDRIVE) {
        // overdrive ONLY draws full frames, so if it's not FULLFRAME, ignore it
        // the reason for this is to keep the VDP running 60fps when we don't
        // actually have a preset clockspeed to know how many cycles make a
        // single scanline. We can fix this later when we do timing properly.
		if (cycleCount != FULLFRAME) {
			return;
		} else {
            // when it is time to draw a fullframe, then we just force the full cycle
            // per frame count to get it out.
			cycleCount = max_cpf;
			newCycles = cycleCount;
		}
	} else {
        // in other, normal modes, we add the number of cycles received and
        // then we go ahead and process below
        // If it's a negative number (and FULLFRAME needs to be way too
        // negative to count), then we discount it after processing so
        // when the full instruction count comes in, we account for the
        // cycles already dealt with.
        if (cycleCount == FULLFRAME) {
            // should never happen....
            return;
        } else {
            if (cycleCount < 0) {
    		    newCycles = nCycles - cycleCount;   // add a negative number
            } else {
    		    newCycles = nCycles + cycleCount;
            }
        }
	}

	while (newCycles > cyclesPerLine) {
		++vdpscanline;
		if (vdpscanline == 192+27) {
			// set the vertical interrupt
			VDPS|=VDPS_INT;
			end_of_frame = 1;
			statusFrameCount++;
			if (logAudio) writeAudioLogState();
		} else if (vdpscanline > 261) {
			vdpscanline = 0;
			SetEvent(BlitEvent);
		}
		// update the GPU
		// first GPU scanline is first line of active display
		// the blanking is not quite right. We expect the scanline
		// to be set before the line is buffered, and blank to
		// be set after it's cached. Since the GPU doesn't really
		// interleave, we always set blanking true and play with
		// the scanline so it works. TODO: fix that
		int gpuScanline = vdpscanline - 27;		// this value is correct for scanline pics

		if (gpuScanline < 0) gpuScanline+=262;
		if ((gpuScanline > 255)||(gpuScanline < 0)) {
			VDP[0x7000]=255;
		} else {
			VDP[0x7000]=gpuScanline;
		}
		VDP[0x7001] = 0x01;		// hblank OR vblank

		// are we off the screen?
		if (vdpscanline < 192+27+24) {
			// nope, we can process this one
			VDPdisplay(vdpscanline);
		}
		newCycles -= cyclesPerLine;

		// break infinite loop during pause and cart loads
		if (max_cpf == 0) {
			if (!redraw_needed) {
				// don't build up any slack
				newCycles = 0.0;	
				break;
			}
		}
	}
	// save remainder
	nCycles = newCycles;
    
    // if it was a half cycle, then subtract it again so we don't double up
    if ((cycleCount < 0) && (cycleCount != FULLFRAME)) {
        nCycles += cycleCount;  // subtract a negative number
    }
}

// for the sake of overdrive, force out a single frame
void vdpForceFrame() {
	updateVDP(FULLFRAME);
}

//////////////////////////////////////////////////////
// Draw a debug screen 
//////////////////////////////////////////////////////
void draw_debug()
{
	if (NULL != dbgWnd) {
		SetEvent(hDebugWindowUpdateEvent);
	}
}

/////////////////////////////////////////////////////////
// Draw graphics mode
// Layer 2 for F18A second tile layer!
/////////////////////////////////////////////////////////
void VDPgraphics(int scanline, int isLayer2)
{
	int t,o;				// temp variables
	int i2;					// temp variables
	int p_add;
	int fgc, bgc, c;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x07;

	o=(scanline/8)*32;			// offset in SIT

	//for (i1=0; i1<192; i1+=8)		// y loop
	{ 
		for (i2=0; i2<256; i2+=8)	// x loop
		{ 
			if (VDPDebug) {
				ch=o&0xff;              // debug character simply increments
                if (o&0x100) {
                    // sprites in the second block
					if (VDPREG[1]&0x02) {
						// double-size sprites, group them as such
						// 0..2..4..6..8....62
						// 1..3..5..7..9....63
						// 64.66.68.........126
						// 65.67.69.........127
						int band=(o&0xff)/64;
						int off=(o/32)&1;
						int x=(o%32)*2;
						ch = band*64+x+off;
					}
                    p_add=SDT+(ch<<3)+i3;   // calculate pattern address
                    fgc = 15-(VDPREG[7]&0x0f);  // color the opposite of the screen color
                    bgc = 0;                // transparent background
                } else {
			        p_add=PDT+(ch<<3)+i3;   // calculate pattern address
			        c = ch>>3;              // divide by 8 for color table
			        fgc=VDP[CT+c];          // extract color
			        bgc=fgc&0x0f;           // extract background color
			        fgc>>=4;                // mask foreground color
                }
			} else {
				ch=VDP[SIT+o];          // look up character
			    p_add=PDT+(ch<<3)+i3;   // calculate pattern address
			    c = ch>>3;              // divide by 8 for color table
			    fgc=VDP[CT+c];          // extract color
			    bgc=fgc&0x0f;           // extract background color
			    fgc>>=4;                // mask foreground color
			}
			o++;

			//for (i3=0; i3<8; i3++)
			{	
				t=VDP[p_add];
	
	            if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                    // for layer 2, we have to drop transparent pixels,
                    // but I don't want to slow down the normal draw that much
			        //for (i3=0; i3<8; i3++)
			        {	
                        // fix it so that bgc is transparent, then we only draw on that test
                        if (fgc==0) {
                            t=~t;
                            fgc=bgc;
                            //bgc=0;  // not needed, not going to use it
                        }
                        if (fgc != 0) {     // skip if fgc is also transparent
				            if (t&80) pixel(i2,i1+i3,fgc);
				            if (t&40) pixel(i2+1,i1+i3,fgc);
				            if (t&20) pixel(i2+2,i1+i3,fgc);
				            if (t&10) pixel(i2+3,i1+i3,fgc);
				            if (t&8) pixel(i2+4,i1+i3,fgc);
				            if (t&4) pixel(i2+5,i1+i3,fgc);
				            if (t&2) pixel(i2+6,i1+i3,fgc);
				            if (t&1) pixel(i2+7,i1+i3,fgc);
                        }
			        }
                } else {
    			    pixel(i2,i1+i3,(t&0x80 ? fgc : bgc ));
				    pixel(i2+1,i1+i3,(t&0x40 ? fgc : bgc ));
				    pixel(i2+2,i1+i3,(t&0x20 ? fgc : bgc ));
				    pixel(i2+3,i1+i3,(t&0x10 ? fgc : bgc ));
				    pixel(i2+4,i1+i3,(t&0x08 ? fgc : bgc ));
				    pixel(i2+5,i1+i3,(t&0x04 ? fgc : bgc ));
				    pixel(i2+6,i1+i3,(t&0x02 ? fgc : bgc ));
				    pixel(i2+7,i1+i3,(t&0x01 ? fgc : bgc ));
                }
			}
		}
	}

	DrawSprites(scanline);

}

/////////////////////////////////////////////////////////
// Draw bitmap graphics mode
/////////////////////////////////////////////////////////
void VDPgraphicsII(int scanline, int isLayer2)
{
	int t,o;				// temp variables
	int i2;					// temp variables
	int p_add, c_add;
	int fgc, bgc;
	int table, Poffset, Coffset;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x07;

    if (isLayer2) {
        // I don't think you can do bitmap layer 2??
        return;
    }

	o=(scanline/8)*32;			// offset in SIT
//	table=0; Poffset=0; Coffset=0;

//	for (i1=0; i1<192; i1+=8)		// y loop
	{ 
//		if ((i1==64)||(i1==128)) {
//			table++;
//			Poffset=table*0x800;
//			Coffset=table*0x800;
//		}
		table = i1/64;
		Poffset=table*0x800;
		Coffset=table*0x800;

		for (i2=0; i2<256; i2+=8)	// x loop
		{ 
			if (VDPDebug) {
				ch=o&0xff;
			} else {
				ch=VDP[SIT+o];
			}
			
    		p_add=PDT+(((ch<<3)+Poffset)&PDTsize)+i3;
	    	c_add=CT+(((ch<<3)+Coffset)&CTsize)+i3;
			o++;

//			for (i3=0; i3<8; i3++)
			{	
                if (bDisablePatternLayer) {
                    t = 0x00;   // show background colors
                } else {
    				t=VDP[p_add];
                }
                if (bDisableColorLayer) {
                    fgc=15; // white
                    bgc=1;  // black
                } else {
    				fgc=VDP[c_add];
	    			bgc=fgc&0x0f;
    				fgc>>=4;
                }
				{
					pixel(i2,i1+i3,(t&0x80 ? fgc : bgc ));
					pixel(i2+1,i1+i3,(t&0x40 ? fgc : bgc ));
					pixel(i2+2,i1+i3,(t&0x20 ? fgc : bgc ));
					pixel(i2+3,i1+i3,(t&0x10 ? fgc : bgc ));
					pixel(i2+4,i1+i3,(t&0x08 ? fgc : bgc ));
					pixel(i2+5,i1+i3,(t&0x04 ? fgc : bgc ));
					pixel(i2+6,i1+i3,(t&0x02 ? fgc : bgc ));
					pixel(i2+7,i1+i3,(t&0x01 ? fgc : bgc ));
				}
			}
		}
	}

	DrawSprites(scanline);

}

////////////////////////////////////////////////////////////////////////
// Draw text mode 40x24
////////////////////////////////////////////////////////////////////////
void VDPtext(int scanline, int isLayer2)
{ 
	int t,o;
	int i2;
	int fgc, bgc, p_add;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x07;

	o=(scanline/8)*40;			// offset in SIT

	t=VDPREG[7];
	bgc=t&0xf;
	fgc=t>>4;
    if (isLayer2) {
        bgc=0;
    }

//	for (i1=0; i1<192; i1+=8)					// y loop
	{ 
		for (i2=8; i2<248; i2+=6)				// x loop
		{ 
			if (VDPDebug) {
				ch = o & 0xff;
			} else {
				ch=VDP[SIT+o];
			}

            if ((bF18AActive) && (VDPREG[50]&0x02)) {
                // per-cell attributes, so update the colors
                if (isLayer2) {
                    t = VDP[VDPREG[11]*64 + o];
                    // BG is transparent (todo is that true?)
	                fgc=t>>4;
                } else {
                    t = VDP[VDPREG[3]*64 + o];
                }
	            bgc=t&0xf;
	            fgc=t>>4;
            }

			p_add=PDT+(ch<<3)+i3;
			o++;

            if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                // for layer 2, we have to drop transparent pixels,
                // but I don't want to slow down the normal draw that much
			    //for (i3=0; i3<8; i3++)
			    {	
				    t=VDP[p_add];
                    if (fgc != 0) {     // skip if fgc is also transparent
				        if (t&0x80) pixel(i2,i1+i3,fgc);
				        if (t&0x40) pixel(i2+1,i1+i3,fgc);
				        if (t&0x20) pixel(i2+2,i1+i3,fgc);
				        if (t&0x10) pixel(i2+3,i1+i3,fgc);
				        if (t&0x8) pixel(i2+4,i1+i3,fgc);
				        if (t&0x4) pixel(i2+5,i1+i3,fgc);
                    }
			    }
            } else {
    //			for (i3=0; i3<8; i3++)		// 6 pixels wide
			    {	
				    t=VDP[p_add];
				    pixel(i2,i1+i3,  (t&0x80 ? fgc : bgc ));
				    pixel(i2+1,i1+i3,(t&0x40 ? fgc : bgc ));
				    pixel(i2+2,i1+i3,(t&0x20 ? fgc : bgc ));
				    pixel(i2+3,i1+i3,(t&0x10 ? fgc : bgc ));
				    pixel(i2+4,i1+i3,(t&0x08 ? fgc : bgc ));
				    pixel(i2+5,i1+i3,(t&0x04 ? fgc : bgc ));
			    }
            }
		}
	}

    // no sprites in text mode, unless f18A unlocked
    if ((bF18AActive) && (!isLayer2)) {
        // todo: layer 2 has sprite dependency concerns
	    DrawSprites(scanline);
    }
}

////////////////////////////////////////////////////////////////////////
// Draw bitmap text mode 40x24
////////////////////////////////////////////////////////////////////////
void VDPtextII(int scanline, int isLayer2)
{ 
	int t,o;
	int i2;
	int fgc,bgc, p_add;
	int table, Poffset;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x07;

	o=(scanline/8)*40;							// offset in SIT

	t=VDPREG[7];
	bgc=t&0xf;
	fgc=t>>4;
    if (isLayer2) {
        bgc=0;
    }

	table=0; Poffset=0;

//	for (i1=0; i1<192; i1+=8)					// y loop
	{ 
//		if ((i1==64)||(i1==128)) {
//			table++;
//			Poffset=table*0x800;
//		}
		table = i1/64;
		Poffset=table*0x800;

		for (i2=8; i2<248; i2+=6)				// x loop
		{ 
			if (VDPDebug) {
				ch=o&0xff;
			} else {
				ch=VDP[SIT+o];
			}

			p_add=PDT+(((ch<<3)+Poffset)&PDTsize)+i3;
			o++;

            if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                // for layer 2, we have to drop transparent pixels,
                // but I don't want to slow down the normal draw that much
			    //for (i3=0; i3<8; i3++)
			    {	
				    t=VDP[p_add];
                    if (fgc != 0) {     // skip if fgc is also transparent
				        if (t&0x80) pixel80(i2,i1+i3,fgc);
				        if (t&0x40) pixel80(i2+1,i1+i3,fgc);
				        if (t&0x20) pixel80(i2+2,i1+i3,fgc);
				        if (t&0x10) pixel80(i2+3,i1+i3,fgc);
				        if (t&0x8) pixel80(i2+4,i1+i3,fgc);
				        if (t&0x4) pixel80(i2+5,i1+i3,fgc);
                    }
			    }
            } else {
    //			for (i3=0; i3<8; i3++)		// 6 pixels wide
			    {	
				    t=VDP[p_add];
				    pixel(i2,i1+i3,(t&0x80 ?   fgc : bgc ));
				    pixel(i2+1,i1+i3,(t&0x40 ? fgc : bgc ));
				    pixel(i2+2,i1+i3,(t&0x20 ? fgc : bgc ));
				    pixel(i2+3,i1+i3,(t&0x10 ? fgc : bgc ));
				    pixel(i2+4,i1+i3,(t&0x08 ? fgc : bgc ));
				    pixel(i2+5,i1+i3,(t&0x04 ? fgc : bgc ));
			    }
            }
		}
	}
	// no sprites in text mode
}

////////////////////////////////////////////////////////////////////////
// Draw text mode 80x24 (note: 80x26.5 mode not supported, blink not supported)
////////////////////////////////////////////////////////////////////////
void VDPtext80(int scanline, int isLayer2)
{ 
	int t,o;
	int i2;
	int fgc,bgc, p_add;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x07;

	o=(scanline/8)*80;				// offset in SIT

	t=VDPREG[7];
	bgc=t&0xf;
	fgc=t>>4;
    if (isLayer2) {
        bgc=0;
    }

//	for (i1=0; i1<192; i1+=8)					// y loop
	{ 
		for (i2=8; i2<488; i2+=6)				// x loop
		{ 
			if (VDPDebug) {
				ch=o&0xff;
			} else {
				ch=VDP[SIT+o];
			}

            if ((bF18AActive) && (VDPREG[50]&0x02)) {
                // per-cell attributes, so update the colors
                if (isLayer2) {
                    t = VDP[VDPREG[11]*64 + o];
                } else {
                    t = VDP[VDPREG[3]*64 + o];
                }
	            bgc=t&0xf;
	            fgc=t>>4;
            }

			p_add=PDT+(ch<<3)+i3;
			o++;

            if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                // for layer 2, we have to drop transparent pixels,
                // but I don't want to slow down the normal draw that much
			    //for (i3=0; i3<8; i3++)
			    {	
				    t=VDP[p_add];
                    if (fgc != 0) {     // skip if fgc is also transparent
				        if (t&0x80) pixel80(i2,i1+i3,fgc);
				        if (t&0x40) pixel80(i2+1,i1+i3,fgc);
				        if (t&0x20) pixel80(i2+2,i1+i3,fgc);
				        if (t&0x10) pixel80(i2+3,i1+i3,fgc);
				        if (t&0x8) pixel80(i2+4,i1+i3,fgc);
				        if (t&0x4) pixel80(i2+5,i1+i3,fgc);
                    }
			    }
            } else {
                //			for (i3=0; i3<8; i3++)		// 6 pixels wide
			    {	
				    t=VDP[p_add];
				    pixel80(i2,i1+i3,(t&0x80   ? fgc : bgc ));
				    pixel80(i2+1,i1+i3,(t&0x40 ? fgc : bgc ));
				    pixel80(i2+2,i1+i3,(t&0x20 ? fgc : bgc ));
				    pixel80(i2+3,i1+i3,(t&0x10 ? fgc : bgc ));
				    pixel80(i2+4,i1+i3,(t&0x08 ? fgc : bgc ));
				    pixel80(i2+5,i1+i3,(t&0x04 ? fgc : bgc ));
			    }
            }
		}
	}
    // no sprites in text mode, unless f18A unlocked
    // TODO: sprites don't render correctly in the wider 80 column mode...
    if ((bF18AActive) && (!isLayer2)) {
        // todo: layer 2 has sprite dependency concerns
	    DrawSprites(scanline);
    }
}

////////////////////////////////////////////////////////////////////////
// Draw Illegal mode (similar to text mode)
////////////////////////////////////////////////////////////////////////
void VDPillegal(int scanline, int isLayer2)
{ 
	int t;
	int i2;
	int fgc,bgc;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x07;
	(void)scanline;		// scanline is irrelevant

	t=VDPREG[7];
	bgc=t&0xf;
	fgc=t>>4;

	// Each character is made up of rows of 4 pixels foreground, 2 pixels background
//	for (i1=0; i1<192; i1+=8)					// y loop
	{ 
		for (i2=8; i2<248; i2+=6)				// x loop
		{ 
            if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                // for layer 2, we have to drop transparent pixels,
                // but I don't want to slow down the normal draw that much
			    //for (i3=0; i3<8; i3++)
			    {	
                    // fix it so that bgc is transparent, then we only draw on that test
                    if (fgc!=0) {
				        pixel(i2,i1+i3,fgc);
				        pixel(i2+1,i1+i3,fgc);
				        pixel(i2+2,i1+i3,fgc);
				        pixel(i2+3,i1+i3,fgc);
                    }
                    if (bgc!=0) {
				        pixel(i2+4,i1+i3,bgc);
				        pixel(i2+5,i1+i3,bgc);
                    }
			    }
            } else {
    //			for (i3=0; i3<8; i3++)				// 6 pixels wide
			    {	
				    pixel(i2,i1+i3  ,fgc);
				    pixel(i2+1,i1+i3,fgc);
				    pixel(i2+2,i1+i3,fgc);
				    pixel(i2+3,i1+i3,fgc);
				    pixel(i2+4,i1+i3,bgc);
				    pixel(i2+5,i1+i3,bgc);
			    }
            }
		}
	}
	// no sprites in this mode
}

/////////////////////////////////////////////////////
// Draw Multicolor Mode
/////////////////////////////////////////////////////
void VDPmulticolor(int scanline, int isLayer2) 
{
	int o;				// temp variables
	int i2;				// temp variables
	int p_add;
	int fgc, bgc;
	int off;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x04;
	const int i4 = scanline&0x03;

	o=(scanline/8)*32;			// offset in SIT
	off=(scanline>>2)&0x06;		// offset in pattern

//	for (i1=0; i1<192; i1+=8)									// y loop
	{ 
		for (i2=0; i2<256; i2+=8)								// x loop
		{ 
			if (VDPDebug) {
				ch=o&0xff;
			} else {
				ch=VDP[SIT+o];
			}

			p_add=PDT+(ch<<3)+off+(i3>>2);
			o++;

//			for (i3=0; i3<7; i3+=4)
			{	
				fgc=VDP[p_add];
				bgc=fgc&0x0f;
				fgc>>=4;
	
                if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                    // for layer 2, we have to drop transparent pixels,
                    // but I don't want to slow down the normal draw that much
			        //for (i3=0; i3<8; i3++)
			        {	
                        if (fgc != 0) {     // skip if fgc is also transparent
					        pixel(i2,i1+i3+i4,fgc);
					        pixel(i2+1,i1+i3+i4,fgc);
					        pixel(i2+2,i1+i3+i4,fgc);
					        pixel(i2+3,i1+i3+i4,fgc);
                        }
                        if (bgc != 0) {
					        pixel(i2+4,i1+i3+i4,bgc);
					        pixel(i2+5,i1+i3+i4,bgc);
					        pixel(i2+6,i1+i3+i4,bgc);
					        pixel(i2+7,i1+i3+i4,bgc);
                        }
			        }
                } else {
    //				for (i4=0; i4<4; i4++) 
				    {
					    pixel(i2,i1+i3+i4,fgc);
					    pixel(i2+1,i1+i3+i4,fgc);
					    pixel(i2+2,i1+i3+i4,fgc);
					    pixel(i2+3,i1+i3+i4,fgc);
					    pixel(i2+4,i1+i3+i4,bgc);
					    pixel(i2+5,i1+i3+i4,bgc);
					    pixel(i2+6,i1+i3+i4,bgc);
					    pixel(i2+7,i1+i3+i4,bgc);
				    }
                }
			}
		}
//		off+=2;
//		if (off>7) off=0;
	}

	DrawSprites(scanline);

	return;
}

/////////////////////////////////////////////////////
// Draw Bitmap Multicolor Mode
// TODO not proven to be correct anymore
/////////////////////////////////////////////////////
void VDPmulticolorII(int scanline, int isLayer2) 
{
	int o;						// temp variables
	int i2;						// temp variables
	int p_add;
	int fgc, bgc;
	int off;
	int table, Poffset;
	unsigned char ch=0xff;
	const int i1 = scanline&0xf8;
	const int i3 = scanline&0x04;
	const int i4 = scanline&0x03;

	o=(scanline/8)*32;			// offset in SIT
	off=(scanline>>2)&0x06;		// offset in pattern

	table=0; Poffset=0;

//	for (i1=0; i1<192; i1+=8)									// y loop
	{ 
//		if ((i1==64)||(i1==128)) {
//			table++;
//			Poffset=table*0x800;
//		}
		table = i1/64;
		Poffset=table*0x800;

		for (i2=0; i2<256; i2+=8)								// x loop
		{ 
			if (VDPDebug) {
				ch=o&0xff;
			} else {
				ch=VDP[SIT+o];
			}

			p_add=PDT+(((ch<<3)+Poffset)&PDTsize)+i3;
			o++;

//			for (i3=0; i3<7; i3+=4)
			{	
				fgc=VDP[p_add++];
				bgc=fgc&0x0f;
				fgc>>=4;
	
                if ((isLayer2)&&((fgc==0)||(bgc==0))) {
                    // for layer 2, we have to drop transparent pixels,
                    // but I don't want to slow down the normal draw that much
			        //for (i3=0; i3<8; i3++)
			        {	
                        if (fgc != 0) {     // skip if fgc is also transparent
					        pixel(i2,i1+i3+i4,fgc);
					        pixel(i2+1,i1+i3+i4,fgc);
					        pixel(i2+2,i1+i3+i4,fgc);
					        pixel(i2+3,i1+i3+i4,fgc);
                        }
                        if (bgc != 0) {
					        pixel(i2+4,i1+i3+i4,bgc);
					        pixel(i2+5,i1+i3+i4,bgc);
					        pixel(i2+6,i1+i3+i4,bgc);
					        pixel(i2+7,i1+i3+i4,bgc);
                        }
			        }
                } else {
    //				for (i4=0; i4<4; i4++) 
				    {
					    pixel(i2,i1+i3+i4,fgc);
					    pixel(i2+1,i1+i3+i4,fgc);
					    pixel(i2+2,i1+i3+i4,fgc);
					    pixel(i2+3,i1+i3+i4,fgc);
					    pixel(i2+4,i1+i3+i4,bgc);
					    pixel(i2+5,i1+i3+i4,bgc);
					    pixel(i2+6,i1+i3+i4,bgc);
					    pixel(i2+7,i1+i3+i4,bgc);
				    }
                }
			}
		}
//		off+=2;
//		if (off>7) off=0;
	}

	DrawSprites(scanline);

	return;
}

// renders a string to the buffer - '1' is white, anything else black
// returns new pDat
unsigned int* drawTextLine(unsigned int *pDat, const char *buf) {
	for (int idx = 0; idx<(signed)strlen(buf); idx++) {
        if (buf[idx] == '1') {
    		*(pDat++)=0xffffff;
		} else {
			*(pDat++)=0;
		}
	}
    return pDat;
}

////////////////////////////////////////////////////////////////
// Stretch-blit the buffer into the active window
//
// NOTES: Graphics modes we have (and some we need)
// 272x208 -- the standard default pixel mode of the 9918A plus a fixed (incorrect) border
// 
// TODO: these modes all have different aspect ratios and none of them are properly 4:3, but we pretend they are
//
// NOTES: Graphics modes we have (and some we need)
// 272x208 -- the standard default pixel mode of the 9918A plus a fixed (incorrect) border
// 544x413 -- the double-sized filters (minus 1 scanline due to corruption)
// 1088x832 - HQ4x buffer
// 634x413 -- TV mode
// 528x208 -- 80-column mode
// These are all with 24 rows -- the F18A adds a 26.5 row mode (212 pixels) (todo: or was it 240?)
// So this adds another 20 (or 48) pixels to each mode
// One solution might be to simply render a fixed TV display and scale to fit...
// The only place it really matters if resolution changes is video recording.
// In that case, the vertical can always be the same - the extra rows just cut into overscan.
// Horizontal, unscaled, is either 272, 528 or 634. We could adapt a buffer size that
// fits all, maybe, and just adjust the amount of overscan area...?
// Alternately... maybe we just blit whatever into a fixed size video buffer (say, 2x) and be done?
////////////////////////////////////////////////////////////////
void doBlit()
{
	RECT rect1;
	int x,y;
	HRESULT ret;
	bool SecondTick = false;
	static time_t lasttime = 0;
	if (time(NULL) != lasttime) {
		SecondTick = true;
		time(&lasttime);
	}

	EnterCriticalSection(&VideoCS);

	if (bShowFPS) {
		static int cnt = 0;
		static char buf[32] = "";
		++cnt;
		if (SecondTick) {
			sprintf(buf, "%d", cnt);
			//debug_write("%d fps", cnt);
			cnt = 0;
			time(&lasttime);
		}
		// draw digits
		for (int i2=0; i2<5; i2++) {
			unsigned int *pDat = framedata + (256+16)*(6-i2);
			for (int idx = 0; idx<(signed)strlen(buf); idx++) {
                int digit = buf[idx] - '0';
                pDat = drawTextLine(pDat, digpat[digit][i2]);
				*(pDat++)=0;
			}
		}
	}
    if (bShowKeyboard) {
		// draw digits
        const char *caps[5] = {
            "111  1  111",
            "1   1 1 1 1",
            "1   111 111",
            "1   1 1 1  ",
            "111 1 1 1  "   };
        const char *lock[5] = {
            "1   111 111 1 1",
            "1   1 1 1   11 ",
            "1   1 1 1   11 ",
            "1   1 1 1   1 1",
            "111 111 111 1 1"   };
        const char *scrl[5] = {
            "111 111 111 1  ",
            "1   1   1 1 1  ",
            "111 1   11  1  ",
            "  1 1   1 1 1  ",
            "111 111 1 1 111"   };
        const char *num[5] = {
            "1 1 1 1 1 1",
            "111 1 1 111",
            "111 1 1 111",
            "111 1 1 1 1",
            "1 1 111 1 1"   };
        const char *joy[5] = {
            "  1 111 1 1",
            "  1 1 1 1 1",
            "  1 1 1 111",
            "1 1 1 1  1 ",
            "111 111  1 "   };
        const char *ign[5] = {
            "111 111 1 1",
            " 1  1   111",
            " 1  1 1 111",
            " 1  1 1 111",
            "111 111 1 1"   };
        const char *fctn[5] = {
            "111 111 111 1 1",
            "1   1    1  111",
            "11  1    1  111",
            "1   1    1  111",
            "1   111  1  1 1"   };
        const char *shift[5] = {
            "111 1 1 111 111",
            "1   1 1 1    1 ",
            "111 111 11   1 ",
            "  1 1 1 1    1 ",
            "111 1 1 1    1 "   };
        const char *ctrl[5] = {
            "111 111 111 1  ",
            "1    1  1 1 1  ",
            "1    1  11  1  ",
            "1    1  1 1 1  ",
            "111  1  1 1 111"   };
		char buf[32];

		for (int i2=0; i2<5; i2++) {
			unsigned int *pDat = framedata + (256+16)*(6-i2)+20;

            if (capslock) {
                drawTextLine(pDat, caps[i2]);
			}
            pDat += 20;

            if (lockedshiftstate) {
                drawTextLine(pDat, lock[i2]);
			}
            pDat += 20;

            if (scrolllock) {
                drawTextLine(pDat, scrl[i2]);
			}
            pDat += 20;

            if (numlock) {
                drawTextLine(pDat, num[i2]);
			}
            pDat += 20;
        
            if (fJoystickActiveOnKeys) {
                drawTextLine(pDat, joy[i2]);
			}
            pDat += 20;

            pDat = drawTextLine(pDat, ign[i2]);
            *(pDat++) = 0;
            sprintf(buf, "%d", ignorecount);
			for (int idx = 0; idx<(signed)strlen(buf); idx++) {
                int digit = buf[idx] - '0';
                pDat = drawTextLine(pDat, digpat[digit][i2]);
				*(pDat++)=0;
			}
            pDat+=4;

            pDat = drawTextLine(pDat, fctn[i2]);
            *(pDat++) = 0;
            sprintf(buf, "%d", fctnrefcount);
			for (int idx = 0; idx<(signed)strlen(buf); idx++) {
                int digit = buf[idx] - '0';
                pDat = drawTextLine(pDat, digpat[digit][i2]);
				*(pDat++)=0;
			}
            pDat+=4;

            pDat = drawTextLine(pDat, shift[i2]);
            *(pDat++) = 0;
            sprintf(buf, "%d", shiftrefcount);
			for (int idx = 0; idx<(signed)strlen(buf); idx++) {
                int digit = buf[idx] - '0';
                pDat = drawTextLine(pDat, digpat[digit][i2]);
				*(pDat++)=0;
			}
            pDat+=4;
            
            pDat = drawTextLine(pDat, ctrl[i2]);
            *(pDat++) = 0;
            sprintf(buf, "%d", ctrlrefcount);
			for (int idx = 0; idx<(signed)strlen(buf); idx++) {
                int digit = buf[idx] - '0';
                pDat = drawTextLine(pDat, digpat[digit][i2]);
				*(pDat++)=0;
			}
		}

        for (int i2=0; i2<8; i2++) {
			unsigned int *pDat = framedata + (256+16)*(9-i2)+220;
            for (int mask=1; mask<0x100; mask<<=1) {
                *(pDat++) = (ticols[i2]&mask) ? 0xffffff : 0;
                *(pDat++) = 0xffffff;
            }
        }

	}

	GetClientRect(myWnd, &rect1);
	myDC=GetDC(myWnd);
	SetStretchBltMode(myDC, COLORONCOLOR);

	// in full screen mode, GetClientRect lies about the screen size, which makes the DX blit fail
	// So, just force our assumptions. (I think it includes the menu height...)
	// TODO: I hate these numbers... fix that
	if (StretchMode == STRETCH_FULL) {
		// full screen is the desktop size
		rect1.top=0;
		rect1.left=0;
		rect1.right = fullscreenX;
		rect1.bottom = fullscreenY;
	}

	// even though aspect ratio is forced by the window resize for everything but full screen,
	// people still complain. They maximize or full screen on their 16:9 monitor and then complain
	// that the image isn't 4:3. In short, bah humbug!
	if ((MaintainAspect) && (StretchMode != STRETCH_NONE)) {
		// make sure it fits the window and is 4:3 (1.33333)
		// Since our borders are not 100%, 1.30 is a better match
		const double DesiredRatio = 1.30;
		double ratio = (double)(rect1.right - rect1.left) / (rect1.bottom - rect1.top);
		if (ratio < 1.3) {
			// screen is too narrow, need to make shorter to fit
			int height = (int)((rect1.right - rect1.left) / DesiredRatio + 0.5);
			int diff = ((rect1.bottom - rect1.top) - height) & (~1);	// make sure it's even so we can divide by 2
			if (diff > 0) {
				rect1.top += diff/2;
				rect1.bottom = rect1.top + height;
			}
		} else if (ratio > 1.34) {
			// screen is too wide, need to make thinner to fit
			int width = (int)((rect1.bottom - rect1.top) * DesiredRatio + 0.5);
			int diff = ((rect1.right - rect1.left) - width) & (~1);		// make sure it's even again
			if (diff > 0) {
				rect1.left += diff/2;
				rect1.right = rect1.left + width;
			}
		}
	}


	// TODO: hacky city - 80-column mode doesn't filter or anything, cause we'd have to change ALL the stuff below.
	if ((bEnable80Columns)&&(VDPREG[0]&0x04)&&(VDPREG[1]&0x10)) {
		// render 80 columns to the screen using DIB blit
		StretchDIBits(myDC, rect1.left, rect1.top, rect1.right-rect1.left, rect1.bottom-rect1.top, 0, 0, 512+16, 192+16, framedata, &myInfo80Col, 0, SRCCOPY);
		ReleaseDC(myWnd, myDC);
		LeaveCriticalSection(&VideoCS);
		return;
	}

	// make sure filters work before calling them
	if (FilterMode == 4) {
		if ((!TVFiltersAvailable) || (NULL == sms_ntsc_blit)) {
			MessageBox(myWnd, "Filter DLL not available - reverting to no filter.", "Classic99 Error", MB_OK);
			PostMessage(myWnd, WM_COMMAND, ID_VIDEO_FILTERMODE_NONE, 0);
			ReleaseDC(myWnd, myDC);
			LeaveCriticalSection(&VideoCS);
			return;
		}
	}
	if (FilterMode == 5) {
		if ((NULL == hHQ4DLL) || (NULL == hq4x_init)) {
			// TODO: it has been reported that the emulator just hangs after this message, does not fall back properly
			// Check the other DLLs too.
			MessageBox(myWnd, "HQ4 DLL not available - reverting to no filter.", "Classic99 Error", MB_OK);
			PostMessage(myWnd, WM_COMMAND, ID_VIDEO_FILTERMODE_NONE, 0);
			ReleaseDC(myWnd, myDC);
			LeaveCriticalSection(&VideoCS);
			return;
		}
	}

	// Do the filtering - we throw away the top and bottom 3 scanlines due to some garbage there - it's border anyway
	switch (FilterMode) {
	case 1: // 2xSaI
		_2xSaI((uint8*) framedata+((256+16)*4), ((256+16)*4), NULL, (uint8*)framedata2, (512+32)*4, 256+16, 191+16);
		break;
	case 2: // Super2xSaI
		Super2xSaI((uint8*) framedata+((256+16)*4), ((256+16)*4), NULL, (uint8*)framedata2, (512+32)*4, 256+16, 191+16);
		break;
	case 3: // SuperEagle
		SuperEagle((uint8*) framedata+((256+16)*4), ((256+16)*4), NULL, (uint8*)framedata2, (512+32)*4, 256+16, 191+16);
		break;
	case 4:	// TV filter
		// This filter outputs 602 pixels for 256 in. What we should do is resize the window
		// we eventually produce a TV_WIDTH x 384+29 image (leaving vertical the same)
		sms_ntsc_blit(&tvFilter, framedata, 256+16, 256+16, 192+16, framedata2, (TV_WIDTH)*2*4);
		if (TVScanLines) {
			sms_ntsc_scanlines(framedata2, TV_WIDTH, (TV_WIDTH)*4, 384+29);
		} else {
			// Duplicate every line instead
			for (int y=1; y<384+29; y+=2) {
				memcpy(&framedata2[y*TV_WIDTH], &framedata2[(y-1)*TV_WIDTH], sizeof(framedata2[0])*TV_WIDTH);
			}
		}
		break;
	case 5:	// HQ4x filter - super hi-def!
		{
			if (NULL != hq4x_process) {
				hq4x_process((unsigned char*)framedata, (unsigned char*)framedata2);
			}
		}
	}

	switch (StretchMode) {
	case STRETCH_DIB:	// DIB
		switch (FilterMode) {
		case 0:		// none
			StretchDIBits(myDC, rect1.left, rect1.top, rect1.right-rect1.left, rect1.bottom-rect1.top, 0, 0, 256+16, 192+16, framedata, &myInfo, 0, SRCCOPY);
			break;

		case 4:		// TV
			StretchDIBits(myDC, rect1.left, rect1.top, rect1.right-rect1.left, rect1.bottom-rect1.top, 0, 0, TV_WIDTH, 384+29, framedata2, &myInfoTV, 0, SRCCOPY);
			break;

		case 5:		// hq4x
			StretchDIBits(myDC, rect1.left, rect1.top, rect1.right-rect1.left, rect1.bottom-rect1.top, 0, 0, (256+16)*4, (192+16)*4, framedata2, &myInfo32, 0, SRCCOPY);
			break;

		default:	// all the SAI ones
			StretchDIBits(myDC, rect1.left, rect1.top, rect1.right-rect1.left, rect1.bottom-rect1.top, 0, 0, 512+32, 384+29, framedata2, &myInfo2, 0, SRCCOPY);
		}
		break;

	case 2: // DX
		if (NULL == lpdd) {
			SetupDirectDraw(false);
			if (NULL == lpdd) {
				StretchMode=STRETCH_NONE;
				break;
			}
		}

		if (DD_OK != lpdd->TestCooperativeLevel()) {
			break;
		}

		if (NULL == ddsBack) {
			StretchMode=STRETCH_NONE;
			break;
		}

		if (DD_OK == ddsBack->GetDC(&tmpDC)) {	// color depth translation
			switch (FilterMode) {
				case 0:
					// original buffer
					SetDIBitsToDevice(tmpDC, 0, 0, 256+16, 192+16, 0, 0, 0, 192+16, framedata, &myInfo, DIB_RGB_COLORS);
					break;

				case 4:
					// TV buffer
					SetDIBitsToDevice(tmpDC, 0, 0, TV_WIDTH, 384+29, 0, 0, 0, 384+29, framedata2, &myInfoTV, DIB_RGB_COLORS);
					break;
				
				case 5:
					// 4x buffer
					SetDIBitsToDevice(tmpDC, 0, 0, (256+16)*4, (192+16)*4, 0, 0, 0, (192+16)*4, framedata2, &myInfo32, DIB_RGB_COLORS);
					break;

				default:
					// 2x buffer
					SetDIBitsToDevice(tmpDC, 0, 0, 512+32, 384+29, 0, 0, 0, 384+29, framedata2, &myInfo2, DIB_RGB_COLORS);
					break;
			}
		}
		ddsBack->ReleaseDC(tmpDC);
		// rect1 contains client coordinates (with the correct size!)
		{ 
			POINT pt;
			int w = rect1.right-rect1.left;
			int h = rect1.bottom - rect1.top;
			pt.x = rect1.left;
			pt.y = rect1.top;
			ClientToScreen(myWnd, &pt);
			rect1.top = pt.y;
			rect1.bottom = pt.y + h;
			rect1.left = pt.x;
			rect1.right = pt.x + w;

			// The DirectDraw blit will draw using screen coordinates but into the client area thanks to the clipper
			if (DDERR_SURFACELOST == lpdds->Blt(&rect1, ddsBack, NULL, DDBLT_DONOTWAIT, NULL)) {	// Just go as quick as we can, don't bother waiting
				lpdd->RestoreAllSurfaces();
			}
		}
		break;

	case 3: // DX Full
		if (NULL == lpdd) {
			SetupDirectDraw(true);
			if (NULL == lpdd) {
				StretchMode=STRETCH_NONE;
				break;
			}
		}

		if (DD_OK != lpdd->TestCooperativeLevel()) {
			break;
		}
		
		if (NULL == ddsBack) {
			StretchMode=STRETCH_NONE;
			break;
		}
		if (DD_OK == ddsBack->GetDC(&tmpDC)) {	// color depth translation
			switch (FilterMode) {
				case 0:		// none
					SetDIBitsToDevice(tmpDC, 0, 0, 256+16, 192+16, 0, 0, 0, 192+16, framedata, &myInfo, DIB_RGB_COLORS);
					break;

				case 4:		// tv
					SetDIBitsToDevice(tmpDC, 0, 0, TV_WIDTH, 384+29, 0, 0, 0, 384+29, framedata2, &myInfoTV, DIB_RGB_COLORS);
					break;

				case 5:		// hq4x
					SetDIBitsToDevice(tmpDC, 0, 0, (256+16)*4, (192+16)*4, 0, 0, 0, (192+16)*4, framedata2, &myInfo32, DIB_RGB_COLORS);
					break;

				default:	// 2x
					SetDIBitsToDevice(tmpDC, 0, 0, 512+32, 384+29, 0, 0, 0, 384+29, framedata2, &myInfo2, DIB_RGB_COLORS);
					break;
			}
		}
		ddsBack->ReleaseDC(tmpDC);

		// rect1 contains client coordinates (with the correct size!)
		{ 
			POINT pt;
			int w = rect1.right-rect1.left;
			int h = rect1.bottom - rect1.top;
			pt.x = rect1.left;
			pt.y = rect1.top;
			ClientToScreen(myWnd, &pt);
			rect1.top = pt.y;
			rect1.bottom = pt.y + h;
			rect1.left = pt.x;
			rect1.right = pt.x + w;

			if (DD_OK != (ret=lpdds->Blt(&rect1, ddsBack, NULL, DDBLT_DONOTWAIT, NULL))) {
				if (DDERR_SURFACELOST == ret) {
					lpdd->RestoreAllSurfaces();
				}
			}
		}
		break;

	default:// None
		// Center it in the window, whatever size
		switch (FilterMode) {
		case 0:		// none
			x=(rect1.right-rect1.left-(256+16))/2;
			y=(rect1.bottom-rect1.top-(192+16))/2;
			x=SetDIBitsToDevice(myDC, x, y, 256+16, 192+16, 0, 0, 0, 192+16, framedata, &myInfo, DIB_RGB_COLORS);
			y=GetLastError();
			break;
		
		case 4:		// TV
			x=(rect1.right-rect1.left-(TV_WIDTH))/2;
			y=(rect1.bottom-rect1.top-(384+29))/2;
			SetDIBitsToDevice(myDC, x, y, TV_WIDTH, 384+29, 0, 0, 0, 384+29, framedata2, &myInfoTV, DIB_RGB_COLORS);
			break;

		case 5:		// hq4x
			x=(rect1.right-rect1.left-(256+16)*4)/2;
			y=(rect1.bottom-rect1.top-(192+16)*4)/2;
			x=SetDIBitsToDevice(myDC, x, y, (256+16)*4, (192+16)*4, 0, 0, 0, (192+16)*4, framedata2, &myInfo32, DIB_RGB_COLORS);
			y=GetLastError();
			break;

		default:	// 2x
			x=(rect1.right-rect1.left-(512+32))/2;
			y=(rect1.bottom-rect1.top-(384+29))/2;
			SetDIBitsToDevice(myDC, x, y, 512+32, 384+29, 0, 0, 0, 384+29, framedata2, &myInfo2, DIB_RGB_COLORS);
			break;
		}
		break;
	}

	ReleaseDC(myWnd, myDC);

	LeaveCriticalSection(&VideoCS);
}

//////////////////////////////////////////////////////////
// Draw Sprites into the backbuffer
//////////////////////////////////////////////////////////
void DrawSprites(int scanline)
{
	int i1, i2, i3, xx, yy, pat, col, p_add, t, sc;
	int highest;
	int curSAL;

	// a hacky, but effective 4-sprite-per-line limitation emulation
	// We can do this right when we have scanline based VDP (TODO: or even later)
	char nLines[192];
	char bSkipScanLine[32][32];		// 32 sprites, 32 lines max
	// fifth sprite on a scanline, or last sprite processed in table
	// note that this value counts up as
	// it processes the sprite list (or at least,
	// it stores the last sprite processed).
	// Once latched, it does not change unless the status bit is cleared.
	// That behaviour (except the TODO) verified on hardware.
	// TODO:	does it count in real time? That is, if there are no overruns, is the value semi-random? Apparently yes!
	// TODO:	does it also latch when F (frame/vdp sync) is set? MAME does this. For now we latch only for 5S (I can't really do 'F' right now,
	//			it's always set when I get to this function!) Datasheet says yes though.
	// Ah ha: datasheet says:
	// "The 5S status flag in the status register is set to a 1 whenever there are five or more sprites on a horizontal
	// line (lines 0 to 192) and the frame flag is equal to a 0.  The 5S status flag is cleared to a 0 after the 
	// status register is read or the VDP is externally reset.  The number of the fifth sprite is placed into the 
	// lower 5 bits of the status register when the 5S flag is set and is valid whenever the 5S flag is 1.  The 
	// setting of the fifth sprite flag will not cause an interrupt"
	// Real life test data:
	// 4 sprites in sprite list on same line (D0 on sprite 4): A4 after a frame, 04 if read again immediately (F,C, last sprite 4)
	// 5 sprites: E4 after frame (F,5,C, 5th sprite 4), 05 if read again immediately (last sprite 5)
	// 6 sprites: E4 after frame (F,5,C, 5th sprite 4), 06 if read again immediately (last sprite 6)
	// 11 sprites on 2 lines (0-5,6-10): E4 (F,5,C, 5th sprite 4), 0B (last sprite 11)
	// 10 sprites on 2 lines (5-9,0-4): E9 (F,5,C, 5th sprite 9), 0A (last sprite 10)
	//
	// TODO: so why does Miner2049 fail during execution? And why did Rasmus see higher numbers than
	// he expected to see in Bouncy? Miner2049 does not check collisions unless it sees a status bit of >02,
	// which is a clear bug (they probably meant >20 - collision). This probably worked because there is a
	// pretty good likelihood of getting a count with that value in it.
	//
	// The real-world case I did not test: all 32-sprites are active, what is the 5th sprite index when
	// it is not latched by 5S, at the end of the frame. 0 works with Bouncy, but 1F works with Miner.
	// Theory: the correct value is probably 00 (>1F + 1 wrapped to 5 bits), and Miner relies on the
	// semi-random nature of reading the flag mid-frame instead of only at the end of a frame. We can't
	// reproduce this very well without a line-by-line VDP.
	//
	// TODO: fix Miner2049 without a hack.
	// TODO: and do all the above properly, now that we're scanline based
	int b5OnLine=-1;

	if (bDisableSprite) {
		return;
	}

	// check if b5OnLine is already latched, and set it if so.
	if (VDPS & VDPS_5SPR) {
		b5OnLine = VDPS & 0x1f;
	}

	memset(nLines, 0, sizeof(nLines));
	memset(bSkipScanLine, 0, sizeof(bSkipScanLine));

	// set up the draw
	memset(SprColBuf, 0, 256*192);
	SprColFlag=0;
	
	highest=31;

	// find the highest active sprite
	for (i1=0; i1<32; i1++)			// 32 sprites 
	{
		yy=VDP[SAL+(i1<<2)];
		if (yy==0xd0)
		{
			highest=i1-1;
			break;
		}
	}
	
	if (bUse5SpriteLimit) {
		// go through the sprite table and check if any scanlines are obliterated by 4-per-line
		i3=8;							// number of sprite scanlines
		if (VDPREG[1] & 0x2) {			 // TODO: Handle F18A ECM where sprites are doubled individually
			// double-sized
			i3*=2;
		}
		if (VDPREG[1]&0x01)	{
			// magnified sprites
			i3*=2;
		}
        int max = 5;                    // 9918A - fifth sprite is lost
        if (bF18AActive) {
            max = VDPREG[0x33];         // F18A - configurable value
            if (max == 0) max = 5;      // assume jumper set to 9918A mode
        }
		for (i1=0; i1<=highest; i1++) {
			curSAL=SAL+(i1<<2);
			yy=VDP[curSAL]+1;				// sprite Y, it's stupid, cause 255 is line 0 - TODO: THIS IS OFF BY 1, WE ARE DRAWING AT 254, instead of 255
			if (yy>225) yy-=256;			// fade in from top
			t=yy;
			for (i2=0; i2<i3; i2++,t++) {
				if ((t>=0) && (t<=191)) {
					nLines[t]++;
					if (nLines[t]>=max) {
						if (t == scanline) {
							if (b5OnLine == -1) b5OnLine=i1;
						}
						bSkipScanLine[i1][i2]=1;
					}
				}
			}
		}
	}

	// now draw
	for (i1=highest; i1>=0; i1--)	
	{	
		curSAL=SAL+(i1<<2);
		yy=VDP[curSAL++]+1;				// sprite Y, it's stupid, cause 255 is line 0 
		if (yy>225) yy-=256;			// fade in from top: TODO: is this right??
		xx=VDP[curSAL++];				// sprite X 
		pat=VDP[curSAL++];				// sprite pattern
		int dblSize = F18AECModeSprite ? VDP[curSAL] & 0x10 : VDPREG[1] & 0x2;
		if (dblSize) {
			pat=pat&0xfc;				// if double-sized, it must be a multiple of 4
		}
		col=VDP[curSAL]&0xf;			// sprite color 
	
		if (VDP[curSAL++]&0x80)	{		// early clock
			xx-=32;
		}

		// Even transparent sprites get drawn into the collision buffer
		p_add=SDT+(pat<<3);
		sc=0;						// current scanline
		
		// Added by Rasmus M
		// TODO: For ECM 1 we need one more bit from R24 (Mike: is that ECM? I think it's always!)
		int paletteBase = F18AECModeSprite ? (col >> (F18AECModeSprite - 2)) * F18ASpritePaletteSize : 0;
		int F18ASpriteColorLine[8]; // Colors indices for each of the 8 pixels in a sprite scan line

		if (VDPREG[1]&0x01)	{		// magnified sprites
			for (i3=0; i3<16; i3++)
			{	
				t = pixelMask(p_add, F18ASpriteColorLine);	// Modified by RasmusM. Sets up the F18ASpriteColorLine[] array.

				if ((!bSkipScanLine[i1][sc]) && (yy+i3 == scanline)) {
					if (t&0x80) 
						bigpixel(xx, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
					if (t&0x40)
						bigpixel(xx+2, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
					if (t&0x20)
						bigpixel(xx+4, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
					if (t&0x10)
						bigpixel(xx+6, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
					if (t&0x08)
						bigpixel(xx+8, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
					if (t&0x04)
						bigpixel(xx+10, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
					if (t&0x02)
						bigpixel(xx+12, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
					if (t&0x01)
						bigpixel(xx+14, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);
				}

				if (dblSize)		// double-size sprites, need to draw 3 more chars 
				{	
					t = pixelMask(p_add + 8, F18ASpriteColorLine);	// Modified by RasmusM
	
					if ((!bSkipScanLine[i1][sc+16]) && (yy+i3+16 == scanline)) {
						if (t&0x80)
							bigpixel(xx, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
						if (t&0x40)
							bigpixel(xx+2, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
						if (t&0x20)
							bigpixel(xx+4, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
						if (t&0x10)
							bigpixel(xx+6, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
						if (t&0x08)
							bigpixel(xx+8, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
						if (t&0x04)
							bigpixel(xx+10, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
						if (t&0x02)
							bigpixel(xx+12, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
						if (t&0x01)
							bigpixel(xx+14, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);

						t = pixelMask(p_add + 24, F18ASpriteColorLine);	// Modified by RasmusM
						if (t&0x80)
							bigpixel(xx+16, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
						if (t&0x40)
							bigpixel(xx+18, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
						if (t&0x20)
							bigpixel(xx+20, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
						if (t&0x10)
							bigpixel(xx+22, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
						if (t&0x08)
							bigpixel(xx+24, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
						if (t&0x04)
							bigpixel(xx+26, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
						if (t&0x02)
							bigpixel(xx+28, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
						if (t&0x01)
							bigpixel(xx+30, yy+i3+16, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);
					}

					if ((!bSkipScanLine[i1][sc]) && (yy+i3 == scanline)) {
						t = pixelMask(p_add + 16, F18ASpriteColorLine);	// Modified by RasmusM
						if (t&0x80)
							bigpixel(xx+16, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
						if (t&0x40)
							bigpixel(xx+18, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
						if (t&0x20)
							bigpixel(xx+20, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
						if (t&0x10)	
							bigpixel(xx+22, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
						if (t&0x08)
							bigpixel(xx+24, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
						if (t&0x04)
							bigpixel(xx+26, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
						if (t&0x02)
							bigpixel(xx+28, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
						if (t&0x01)
							bigpixel(xx+30, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);
					}
				}
				sc++;
				p_add += i3&0x01;
			}
		} else {
			for (i3=0; i3<8; i3++)
			{	
				t = pixelMask(p_add++, F18ASpriteColorLine);	// Modified by RasmusM

				if ((!bSkipScanLine[i1][sc]) && (yy+i3 == scanline)) {
					if (t&0x80)
						spritepixel(xx, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
					if (t&0x40)
						spritepixel(xx+1, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
					if (t&0x20)
						spritepixel(xx+2, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
					if (t&0x10)
						spritepixel(xx+3, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
					if (t&0x08)
						spritepixel(xx+4, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
					if (t&0x04)
						spritepixel(xx+5, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
					if (t&0x02)
						spritepixel(xx+6, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
					if (t&0x01)
						spritepixel(xx+7, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);
				}

				if (dblSize)		// double-size sprites, need to draw 3 more chars 
				{	
					t = pixelMask(p_add + 7, F18ASpriteColorLine);	// Modified by RasmusM

					if ((!bSkipScanLine[i1][sc+8]) && (yy+i3+8 == scanline)) {
						if (t&0x80)
							spritepixel(xx, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
						if (t&0x40)
							spritepixel(xx+1, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
						if (t&0x20)
							spritepixel(xx+2, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
						if (t&0x10)
							spritepixel(xx+3, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
						if (t&0x08)
							spritepixel(xx+4, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
						if (t&0x04)
							spritepixel(xx+5, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
						if (t&0x02)
							spritepixel(xx+6, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
						if (t&0x01)
							spritepixel(xx+7, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);

						t = pixelMask(p_add + 23, F18ASpriteColorLine);	// Modified by RasmusM
						if (t&0x80)
							spritepixel(xx+8, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
						if (t&0x40)
							spritepixel(xx+9, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
						if (t&0x20)
							spritepixel(xx+10, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
						if (t&0x10)
							spritepixel(xx+11, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
						if (t&0x08)
							spritepixel(xx+12, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
						if (t&0x04)
							spritepixel(xx+13, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
						if (t&0x02)
							spritepixel(xx+14, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
						if (t&0x01)
							spritepixel(xx+15, yy+i3+8, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);
					}

					if ((!bSkipScanLine[i1][sc]) && (yy+i3 == scanline)) {
						t = pixelMask(p_add + 15, F18ASpriteColorLine);	// Modified by RasmusM
						if (t&0x80)
							spritepixel(xx+8, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[0] : col);
						if (t&0x40)
							spritepixel(xx+9, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[1] : col);
						if (t&0x20)
							spritepixel(xx+10, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[2] : col);
						if (t&0x10)	
							spritepixel(xx+11, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[3] : col);
						if (t&0x08)
							spritepixel(xx+12, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[4] : col);
						if (t&0x04)
							spritepixel(xx+13, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[5] : col);
						if (t&0x02)
							spritepixel(xx+14, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[6] : col);
						if (t&0x01)
							spritepixel(xx+15, yy+i3, F18AECModeSprite ? paletteBase + F18ASpriteColorLine[7] : col);
					}
				}
				sc++;
			}
		}
	}
	// Set the VDP collision bit
	if (SprColFlag) {
		VDPS|=VDPS_SCOL;
	}
	if (b5OnLine != -1) {
		VDPS|=VDPS_5SPR;
		VDPS&=(VDPS_INT|VDPS_5SPR|VDPS_SCOL);
		VDPS|=b5OnLine&(~(VDPS_INT|VDPS_5SPR|VDPS_SCOL));
	} else {
		VDPS&=(VDPS_INT|VDPS_5SPR|VDPS_SCOL);
		// TODO: so if all 32 sprites are in used, the 5-on-line value for not
		// having 5 on a line anywhere will be 0? (0x1f+1)=0x20 -> 0x20&0x1f = 0!
		// The correct behaviour is probably to count in realtime - that goes in with
		// the scanline code.
		VDPS|=(highest+1)&(~(VDPS_INT|VDPS_5SPR|VDPS_SCOL));
	}
}

////////////////////////////////////////////////////////////
// Draw a pixel onto the backbuffer surface
////////////////////////////////////////////////////////////
void pixel(int x, int y, int c)
{
	if ((x > 255)||(y>192)) {
		debug_write("here");
	}

	framedata[((199-y)<<8)+((199-y)<<4)+x+8]=GETPALETTEVALUE(c);
}

////////////////////////////////////////////////////////////
// Draw a pixel onto the backbuffer surface in 80 column mode
////////////////////////////////////////////////////////////
void pixel80(int x, int y, int c)
{
	framedata[((199-y)<<9)+((199-y)<<4)+x+8]=GETPALETTEVALUE(c);
}

////////////////////////////////////////////////////////////
// Draw a range-checked pixel onto the backbuffer surface
////////////////////////////////////////////////////////////
void spritepixel(int x, int y, int c)
{
	if ((y>191)||(y<0)) return;
    if ((VDPREG[1] & 0x10) == 0) {
        // normal modes
        if ((x>255)||(x<0)) return;
    } else {
        // text mode - we only get here in the F18A case, and it truncates on the text coordinates
        if ((x>=248)||(x<8)) return;
    }
	
	if (SprColBuf[x][y]) {
		SprColFlag=1;
	} else {
		SprColBuf[x][y]=1;
	}

	if (!(F18AECModeSprite ? c % F18ASpritePaletteSize : c)) return;		// don't DRAW transparent, Modified by RasmusM
	// TODO: this is probably okay but needs to be cleaned up with removal of TIPALETTE - note we do NOT use GETPALETTEVALUE
	// here because the palette index was calculated for full ECM sprites
	framedata[((199-y)<<8)+((199-y)<<4)+x+8] = F18APalette[c];	// Modified by RasmusM
	return;
}

////////////////////////////////////////////////////////////
// Draw a magnified pixel onto the backbuffer surface
////////////////////////////////////////////////////////////
void bigpixel(int x, int y, int c)
{
	spritepixel(x,y,c);
	spritepixel(x+1,y,c);
//	spritepixel(x,y+1,c);
//	spritepixel(x+1,y+1,c);
}

////////////////////////////////////////////////////////////
// Pixel mask
////////////////////////////////////////////////////////////
int pixelMask(int addr, int F18ASpriteColorLine[])
{
	int t = VDP[addr];
	if (F18AECModeSprite > 0) {
		for (int pix = 0; pix < 8; pix++) {
			F18ASpriteColorLine[pix] = ((t >> (7 - pix)) & 0x01);
		}		
		if (F18AECModeSprite > 1) {
			int t1 = VDP[addr + 0x0800]; 
			for (int pix = 0; pix < 8; pix++) {
				F18ASpriteColorLine[pix] |= ((t1 >> (7 - pix)) & 0x01) << 1;
			}		
			t |= t1;
			if (F18AECModeSprite > 2) {
				int t2 = VDP[addr + 0x1000]; 
				for (int pix = 0; pix < 8; pix++) {
					F18ASpriteColorLine[pix] |= ((t2 >> (7 - pix)) & 0x01) << 2;
				}		
				t |= t2;
			}
		}
	}
	return t;
}

////////////////////////////////////////////////////////////
// DirectX full screen enumeration callback
////////////////////////////////////////////////////////////
HRESULT WINAPI myCallBack(LPDDSURFACEDESC2 ddSurface, LPVOID pData) {
	int *c;

	c=(int*)pData;

	if (ddSurface->ddpfPixelFormat.dwRGBBitCount == (DWORD)*c) {
		*c=(*c)|0x80;
		return DDENUMRET_CANCEL;
	}
	return DDENUMRET_OK;
}

////////////////////////////////////////////////////////////
// Setup DirectDraw, with the requested fullscreen mode
// In order for Fullscreen to work, only the main thread
// may call this function!
////////////////////////////////////////////////////////////
void SetupDirectDraw(bool fullscreen) {
	int x,y,c;
	RECT myRect;

	EnterCriticalSection(&VideoCS);

	// directdraw is deprecated -- for now we can still do this, but
	// we need to replace this API with Direct3D (TODO)
    HINSTANCE hInstDDraw;
    LPDIRECTDRAWCREATEEX pDDCreate = NULL;

    hInstDDraw = LoadLibrary( "ddraw.dll" );
    if( hInstDDraw == NULL ) {
		MessageBox(myWnd, "Can't load DLL for DirectDraw 7\nClassic99 Requires DirectX 7 for DX and Full screen modes", "Classic99 Error", MB_OK);
		lpdd=NULL;
		StretchMode=STRETCH_NONE;
		goto optout;
	}

    pDDCreate = ( LPDIRECTDRAWCREATEEX )GetProcAddress( hInstDDraw, "DirectDrawCreateEx" );

	if (pDDCreate(NULL, (void**)&lpdd, IID_IDirectDraw7, NULL)!=DD_OK) {
		MessageBox(myWnd, "Unable to initialize DirectDraw 7\nClassic99 Requires DirectX 7 for DX and Full screen modes", "Classic99 Error", MB_OK);
		lpdd=NULL;
		StretchMode=STRETCH_NONE;
	} else {
		if (fullscreen) {
			DDSURFACEDESC2 myDesc;

			GetWindowRect(myWnd, &myRect);

			// select a full screen mode that equals the desktop mode
			// Limitation: primary monitor only
			x = GetSystemMetrics(SM_CXSCREEN);
			y = GetSystemMetrics(SM_CYSCREEN);
			c = 32;		// always 32-bit color now

			// save those values off
			fullscreenX = x;
			fullscreenY = y;

			// Check if mode is legal
			ZeroMemory(&myDesc, sizeof(myDesc));
			myDesc.dwSize=sizeof(myDesc);
			myDesc.dwFlags=DDSD_HEIGHT | DDSD_WIDTH;
			myDesc.dwWidth=x;
			myDesc.dwHeight=y;
			lpdd->EnumDisplayModes(0, &myDesc, (void*)&c, myCallBack);
			// If a valid mode was found, 'c' has 0x80 ORd with it
			if (0 == (c&0x80)) {
				MessageBox(myWnd, "Requested graphics mode is not supported on the primary display.", "Classic99 Error", MB_OK);
				if (lpdd) lpdd->Release();
				lpdd=NULL;
				StretchMode=STRETCH_NONE;
				MoveWindow(myWnd, myRect.left, myRect.top, myRect.right-myRect.left, myRect.bottom-myRect.top, true);
				goto optout;
			}

			c&=0x7f;	// Remove the flag bit

			if (lpdd->SetCooperativeLevel(myWnd, DDSCL_EXCLUSIVE | DDSCL_ALLOWREBOOT | DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX)!=DD_OK) {
				MessageBox(myWnd, "Unable to set cooperative level\nFullscreen DX is not available", "Classic99 Error", MB_OK);
				if (lpdd) lpdd->Release();
				lpdd=NULL;
				StretchMode=STRETCH_NONE;
				MoveWindow(myWnd, myRect.left, myRect.top, myRect.right-myRect.left, myRect.bottom-myRect.top, true);
				goto optout;
			}

			if (lpdd->SetDisplayMode(x,y,c,0,0) != DD_OK) {
				MessageBox(myWnd, "Unable to set display mode.\nRequested DX mode is not available", "Classic99 Error", MB_OK);
				MoveWindow(myWnd, myRect.left, myRect.top, myRect.right-myRect.left, myRect.bottom-myRect.top, true);
				StretchMode=STRETCH_NONE;
				goto optout;
			}

            // disable the menu
            SetMenuMode(false, false);
		} else {
			if (lpdd->SetCooperativeLevel(myWnd, DDSCL_NORMAL)!=DD_OK) {
				MessageBox(myWnd, "Unable to set cooperative level\nDX mode is not available", "Classic99 Error", MB_OK);
				if (lpdd) lpdd->Release();
				lpdd=NULL;
				StretchMode=STRETCH_NONE;
				goto optout;
			}

            // enable the menu
            SetMenuMode(true, !bEnableAppMode);
		}

		ZeroMemory(&CurrentDDSD, sizeof(CurrentDDSD));
		CurrentDDSD.dwSize=sizeof(CurrentDDSD);
		CurrentDDSD.dwFlags=DDSD_CAPS;
		CurrentDDSD.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;

		if (lpdd->CreateSurface(&CurrentDDSD, &lpdds, NULL) !=DD_OK) {
			MessageBox(myWnd, "Unable to create primary surface\nDX mode is not available", "Classic99 Error", MB_OK);
			if (lpdd) lpdd->Release();
			lpdd=NULL;
			StretchMode=STRETCH_NONE;
			goto optout;
		}

		ZeroMemory(&CurrentDDSD, sizeof(CurrentDDSD));
		CurrentDDSD.dwSize=sizeof(CurrentDDSD);
		CurrentDDSD.dwFlags=DDSD_HEIGHT | DDSD_WIDTH;
		switch (FilterMode) {
			case 0:		// none
				CurrentDDSD.dwWidth=256+16;
				CurrentDDSD.dwHeight=192+16;
				break;

			case 4:		// TV
				CurrentDDSD.dwWidth=TV_WIDTH;
				CurrentDDSD.dwHeight=384+29;
				break;

			case 5:		// hq4x
				CurrentDDSD.dwWidth=(256+16)*4;
				CurrentDDSD.dwHeight=(192+16)*4;
				break;

			default:	// others (*2)
				CurrentDDSD.dwWidth=512+32;
				CurrentDDSD.dwHeight=384+29;
				break;
		}

		if (lpdd->CreateSurface(&CurrentDDSD, &ddsBack, NULL) !=DD_OK) {
			MessageBox(myWnd, "Unable to create back buffer surface\nDX mode is not available", "Classic99 Error", MB_OK);
			ddsBack=NULL;
			lpdds->Release();
			lpdds=NULL;
			lpdd->Release();
			lpdd=NULL;
			StretchMode=STRETCH_NONE;
			goto optout;
		}

		if (!fullscreen) {
			if (lpdd->CreateClipper(0, &lpDDClipper, NULL) != DD_OK) {
				MessageBox(myWnd, "Warning: Unable to create Direct Draw Clipper", "Classic99 Warning", MB_OK);
			} else {
				if (lpDDClipper->SetHWnd(0, myWnd) != DD_OK) {
					MessageBox(myWnd, "Warning: Unable to set Clipper Window", "Classic99 Warning", MB_OK);
					lpDDClipper->Release();
					lpDDClipper=NULL;
				} else {
					if (DD_OK != lpdds->SetClipper(lpDDClipper)) {
						MessageBox(myWnd, "Warning: Unable to attach Clipper", "Classic99 Warning", MB_OK);
						lpDDClipper->Release();
						lpDDClipper=NULL;
					}
				}
			}
		}
	}
	LeaveCriticalSection(&VideoCS);
	return;

optout: ;
	takedownDirectDraw();
	LeaveCriticalSection(&VideoCS);
}

////////////////////////////////////////////////////////////
// Release all references to DirectDraw objects
////////////////////////////////////////////////////////////
void takedownDirectDraw() {	
	EnterCriticalSection(&VideoCS);

	if (NULL != lpDDClipper) lpDDClipper->Release();
	lpDDClipper=NULL;
	if (NULL != ddsBack) ddsBack->Release();
	ddsBack=NULL;
	if (NULL != lpdds) lpdds->Release();
	lpdds=NULL;
	if (NULL != lpdd) lpdd->Release();
	lpdd=NULL;

	LeaveCriticalSection(&VideoCS);
}

////////////////////////////////////////////////////////////
// Resize the back buffer
////////////////////////////////////////////////////////////
int ResizeBackBuffer(int w, int h) {
	EnterCriticalSection(&VideoCS);

	if (NULL != ddsBack) ddsBack->Release();
	ddsBack=NULL;

	if (NULL == lpdd) {
		SetupDirectDraw(false);
		if (NULL == lpdd) {
			MessageBox(myWnd, "Unable to create back buffer surface\nDX mode is not available", "Classic99 Error", MB_OK);
			ddsBack=NULL;
			StretchMode=STRETCH_NONE;
			LeaveCriticalSection(&VideoCS);
			return 1;
		}
	}

	ZeroMemory(&CurrentDDSD, sizeof(CurrentDDSD));
	CurrentDDSD.dwSize=sizeof(CurrentDDSD);
	CurrentDDSD.dwFlags=DDSD_HEIGHT | DDSD_WIDTH;
	CurrentDDSD.dwWidth=w;
	CurrentDDSD.dwHeight=h;

	if (lpdd->CreateSurface(&CurrentDDSD, &ddsBack, NULL) != DD_OK) {
		MessageBox(myWnd, "Unable to create back buffer surface\nDX mode is not available", "Classic99 Error", MB_OK);
		ddsBack=NULL;
		StretchMode=STRETCH_NONE;
		LeaveCriticalSection(&VideoCS);
		return 1;
	}

	LeaveCriticalSection(&VideoCS);
	return 0;
}

//////////////////////////////////////
// Save a screenshot - just BMP for now
// there are lots of nice helpers for others in
// 2000 and higher, but that's ok 
//////////////////////////////////////
void SaveScreenshot(bool bAuto, bool bFiltered) {
	static int nLastNum=0;
	static CString csFile;
	CString csTmp;
	OPENFILENAME ofn;
	char buf[256], buf2[256];
	BOOL bRet;

	if ((!bAuto) || (csFile.IsEmpty())) {
		memset(&ofn, 0, sizeof(OPENFILENAME));
		ofn.lStructSize=sizeof(OPENFILENAME);
		ofn.hwndOwner=myWnd;
		ofn.lpstrFilter="BMP Files\0*.bmp\0\0";
		strcpy(buf, "");
		ofn.lpstrFile=buf;
		ofn.nMaxFile=256;
		strcpy(buf2, "");
		ofn.lpstrFileTitle=buf2;
		ofn.nMaxFileTitle=256;
		ofn.Flags=OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT;

		char szTmpDir[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, szTmpDir);

		bRet = GetSaveFileName(&ofn);

		SetCurrentDirectory(szTmpDir);

		csTmp = ofn.lpstrFile;				// save the file we are opening now
		if (ofn.nFileExtension > 1) {
			csFile = csTmp.Left(ofn.nFileExtension-1);
		} else {
			csFile = csTmp;
			csTmp+=".bmp";
		}
	} else {
		int nCnt=10000;
		for (;;) {
			csTmp.Format("%s%04d.bmp", (LPCSTR)csFile, nLastNum++);
			FILE *fp=fopen(csTmp, "r");
			if (NULL != fp) {
				fclose(fp);
				nCnt--;
				if (nCnt == 0) {
					MessageBox(myWnd, "Can't take another auto screenshot without overwriting file!", "Classic99 Error", MB_OK);
					return;
				}
				continue;
			}
			break;
		}
	}

	if (bRet) {
		// we just create a 24-bit BMP file
		int nX, nY, nBits;
		unsigned char *pBuf;

		if (bFiltered) {
			switch (FilterMode) {
			case 0:		// none
				nX=256+16;
				nY=192+16;
				pBuf=(unsigned char*)framedata;
				nBits=32;
				break;
			
			case 4:		// TV
				nX=TV_WIDTH;
				nY=384+29;
				pBuf=(unsigned char*)framedata2;
				nBits=32;
				break;

			case 5:		// hq4x
				nX=(256+16)*4;
				nY=(192+16)*4;
				pBuf=(unsigned char*)framedata2;
				nBits=32;
				break;

			default:	// All SAI2x modes
				nX=512+32;
				nY=384+29;
				pBuf=(unsigned char*)framedata2;
				nBits=32;
				break;
			}
		} else {
			nX=256+16;
			nY=192+16;
			pBuf=(unsigned char*)framedata;
			nBits=32;
		}

		FILE *fp=fopen(csTmp, "wb");
		if (NULL == fp) {
			MessageBox(myWnd, "Failed to open output file", "Classic99 Error", MB_OK);
			return;
		}

		int tmp;
		fputc('B', fp);				// signature, BM
		fputc('M', fp);
		tmp=nX*nY*3+54;
		fwrite(&tmp, 4, 1, fp);		// size of file
		tmp=0;
		fwrite(&tmp, 4, 1, fp);		// four reserved bytes (2x 2 byte fields)
		tmp=26;
		fwrite(&tmp, 4, 1, fp);		// offset to data
		tmp=12;
		fwrite(&tmp, 4, 1, fp);		// size of the header (v1)
		fwrite(&nX, 2, 1, fp);		// width in pixels
		fwrite(&nY, 2, 1, fp);		// height in pixels
		tmp=1;
		fwrite(&tmp, 2, 1, fp);		// number of planes (1)
		tmp=24;
		fwrite(&tmp, 2, 1, fp);		// bits per pixel (0x18=24)

		if (nBits == 16) {
			// 16-bit 0rrr rrgg gggb bbbb values
			// TODO: not used anymore
			unsigned short *p = (unsigned short*)pBuf;

			for (int idx=0; idx<nX*nY; idx++) {
				int r,g,b;
				
				// extract colors
				r=((*p)&0x7c00)>>10;
				g=((*p)&0x3e0)>>5;
				b=((*p)&0x1f);

				// scale up from 5 bit to 8 bit
				r<<=3;
				g<<=3;
				b<<=3;

				// write out to file
				fputc(b, fp);
				fputc(g, fp);
				fputc(r, fp);

				p++;
			}
		} else {
			// 32-bit 0BGR
			for (int idx=0; idx<nX*nY; idx++) {
				int r,g,b;
				
				// extract colors
				b=*pBuf++;
				g=*pBuf++;
				r=*pBuf++;
				pBuf++;					// skip	0

				// write out to file
				fputc(b, fp);
				fputc(g, fp);
				fputc(r, fp);
			}
		}

		fclose(fp);

		CString csTmp2;
		csTmp2.Format("Classic99 - Saved %sfiltered - %s", bFiltered?"":"un", (LPCSTR)csTmp);
		SetWindowText(myWnd, csTmp2);
	}
}

// F18A Status registers
// This function must only be called when the register is not 0
// and F18A is active
unsigned char getF18AStatus() {
	F18AStatusRegisterNo &= 0x0f;

	switch (F18AStatusRegisterNo) {
	case 1:
		{
			// ID0 ID1 ID2 0 0 0 blanking HF
			// F18A ID is 0xE0
			unsigned char ret = 0xe0;
			// blanking bit
			if (VDP[0x7001]) {
				// TODO: horizontal blank is not supported since we do scanlines?
				ret |= 0x02;
			}
			// HF
			if ((VDPREG[19] != 0) && (vdpscanline == VDPREG[19])) {
				ret |= 0x01;
			}
			return ret;
		}

	case 2:
		{
			// GPUST GDATA0 GDATA1 GDATA2 GDATA3 GDATA4 GDATA5 GDATA6
			// TODO GDATA not supported (how /is/ it supported?)
			if (pGPU->GetIdle() == 0) {
				return 0x80;
			} else {
				return 0x00;
			}
		}

	case 3:
		{
			// current scanline, 0 when not active
			// TODO: is that 0 bit true??
			if ((vdpscanline >= 0) && (vdpscanline < 192+27+24)) {
				return 0;
			} else {
				return VDP[0x7000];
			}
		}
				
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
		// TODO: 64 bit counter - if it's still valid (it's not?)
		return 0;

	case 12:
	case 13:
		// Unused
		return 0;

	case 14:
		// VMAJOR0 VMAJOR1 VMAJOR2 VMAJOR3 VMINOR0 VMINOR1 VMINOR2 VMINOR3
		// TODO: we kind of lie about this...
		return 0x18;

	case 15:
		// VDP register read value - updated any time a VRAM address is set
		// TODO: what does this mean? I think it's the value that the TI would get?
		// So what would happen if the TI set this one? ;)
		return 0;
	}

	debug_write("Warning - bad register value %d", F18AStatusRegisterNo);
	return 0;
}

// captures a text or graphics mode screen (will NOT do bitmap, assuming graphics mode)
CString captureScreen(int offset, char illegalByte) {
    CString csout;
    int stride = getCharsPerLine();
    if (stride == -1) {
        return "";
    }

    gettables(0);

    for (int row = 0; row<24; ++row) {
        for (int col=0; col < stride; ++col) {
            int index = SIT+row*stride+col;
            if (index >= sizeof(VDP)) { row=25; break; }   // check for unlikely overrun case
            int c = VDP[index] + offset; 
            if ((c>=' ')&&(c < 127)) csout+=(char)c; else csout+=illegalByte;
        }
        csout+="\r\n";
    }

    return csout;
}


