// cartpack.cpp : Defines the exported functions for the DLL application.
//

#include <windows.h>
#include "resource.h"

// these are also in tiemul.h in the main project
#define MAXROMSPERCART	32
struct IMG {
	DWORD dwImg;			// resource ID, NULL for disk type
	int  nLoadAddr;
	int  nLength;
	char nType;
    char nOriginalType;     // used for reload of AUTO type
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
#define TYPE_GROM		'G'		// Standard GROM
#define TYPE_ROM		'C'		// CPU ROM
#define TYPE_XB			'X'		// XB page 2 (full 8k available)
#define TYPE_378		'8'		// Packed banks accessed by writing to ROM space (NOT inverted like XB, but all one file)
#define TYPE_379		'9'		// Packed banks accessed by writing to ROM space (inverted like XB, but all one file) ('3' is supported as legacy)
#define TYPE_NVRAM		'N'		// cartridge space has NVRAM - order of this parameter matters
#define TYPE_DSR		'D'		// DSR memory (CRU must be set)
#define TYPE_DSR2		'E'		// Paged DSR (from.. pcode?)
#define TYPE_UBER_GROM	'U'		// My UBER GROM (simulation - just enough for testing)
#define TYPE_UBER_EEPROM 'T'	// UberGROM EEPROM memory (note: not saved back!)
#define TYPE_RAM		'R'		// RAM (CPU memory not read-only)

// These actual structures must reflect this DLL

// Extra files to support certain cartridges
// These files, when the list is loaded, can be loaded as if they were
// on the disk without the disk actually needing it
// Currently these can only be loaded program image files!
// They completely ignore the disk now so can override any disk (may be good or bad?)
struct DISKS Disk_EA[] = {
	{	"ASSM1",	IDR_ASSM1	},
	{	"ASSM2",	IDR_ASSM2	},
	{	"EDIT1",	IDR_EDIT1	},
	{	"",				0			},
};

struct DISKS Disk_SSA[] = {
	{	"ACER_C",	IDR_ACERC	},
	{	"ACER_P",	IDR_ACERP	},
	{	"SSD",		IDR_SSD		},
	{	"SSE",		IDR_SSE		},
	{	"",				0			},
};

struct DISKS Disk_Tunnels[] = {
	{	"PENNIES",	IDR_PENNIES	},
	{	"QUEST",	IDR_QUEST	},

	{	"",				0			},
};

struct DISKS Disk_ScottAdams[] = {
	{	"ADVENTURE",IDR_ADVENTURE	},
	{	"COUNT",	IDR_COUNT	    },
	{	"FUNHOUSE",	IDR_FUNHOUSE	},
	{	"GHOSTTOWN",IDR_GHOSTTOWN	},
	{	"MISSION",	IDR_MISSION 	},
	{	"ODYSSEY",	IDR_ODYSSEY	    },
	{	"PIRATE",	IDR_PIRATE	    },
	{	"PYRAMID",	IDR_PYRAMID 	},
	{	"SAVAGE1",	IDR_SAVAGE1 	},
	{	"SAVAGE2",	IDR_SAVAGE2 	},
	{	"SORCERER",	IDR_SORCERER	},
	{	"SORCEROR",	IDR_SORCERER	},  // my own common typo
	{	"VOODOO",	IDR_VOODOO  	},
	{	"VOYAGE",	IDR_VOYAGE  	},

	{	"",				0			},
};

struct CARTS Apps[] = {
	{	
		"Demonstration",
		{
			{	IDR_DEMOG,		0x6000, 0x8000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Diagnostics",
		{	
			{	IDR_DIAGNOSG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		"The maintenance tests are intended for use with external hardware, and are not supported. They will hang the emulator. Use File->Reset to bring it back. The checkerboard test will fail so long as the disk system is attached.",
		0
	},
						
	{	
		"Editor/Assembler",
		{
			{IDR_TIEAG,		0x6000,	0x2000,	TYPE_GROM	, 0, 0},
		},
		Disk_EA,
		"The Editor and Assembler files are built-in.",
		0
	},

	{	
		"EPSGMOD Example",
		{
			{IDR_EPSGMODG,	0x6000,	0x60C8,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Extended BASIC",
		{
			{	IDR_TIEXTG,		0x6000,	0x8000,	TYPE_GROM	, 0, 0},
			{	IDR_TIEXTC,		0x6000,	0x2000,	TYPE_ROM	, 0, 0},
			{	IDR_TIEXTD,		0x6000,	0x2000,	TYPE_XB		, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"fbForth 2.0:13 by Lee Stewart",
		{	
			{	IDR_FBFORTH,	0x0000, 0x8000,	TYPE_379	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Home Finance",
		{
			{	IDR_HOMEG,		0x6000, 0x4000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"MegaMan2 Music",
		{
			{	IDR_TIPLAYERC,	0x6000, 0x2000,	TYPE_ROM	, 0, 0},
			{	IDR_TIMUSIC,	0x6000,	0xA000,	TYPE_GROM	, 0, 0},
			{	IDR_TIMUSID,	0x6000,	0xA000,	TYPE_GROM	, 0, 1},
			{	IDR_TIMUSIE,	0x6000,	0xA000,	TYPE_GROM	, 0, 2},
			{	IDR_TIMUSIF,	0x6000,	0x5A90,	TYPE_GROM	, 0, 3},
			{	IDR_TIMM2PICP,	0x6000,	0x1800,	TYPE_GROM	, 0, 15},
			{	IDR_TIMM2PICC,	0x8000,	0x1800,	TYPE_GROM	, 0, 15},
			{	IDR_DUMMYG,		0x6000,	0x0040,	TYPE_GROM	, 0, 9},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Mini Memory",
		{	
			{	IDR_MINIMEMG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
			{	IDR_MINIMEMC,	0x6000,	0x1000,	TYPE_ROM	, 0, 0},
			{	0,			    0x7000,	0x1000,	TYPE_NVRAM	, 0, 0, "minimemNV.bin"},
		},
		NULL,
		NULL,
		0
	},

	{	
		"P-Code Card",
		{	
			{	IDR_PCODEC,		0x1F00,	0x2000,	TYPE_DSR	, 0, 0},
			{	IDR_PCODED,		0x1F00, 0x2000,	TYPE_DSR2	, 0, 0},
		},
		NULL,		// TODO: include the P-Code diskettes in the archive - convert from PC99 to standard V9T9 and check sectors
		NULL,
		0
	},

	{	
		"RXB 2025 by Rich Gilbertson",
		{
			{	IDR_RXBG,		0x6000,	0xA000,	TYPE_GROM	, 0, 0},
			{	IDR_RXB8,		0x0000,	0x6000,	TYPE_378	, 0, 0},
		},
		Disk_EA,
		NULL,
		0
	},

	{	
		"Terminal Emulator 2",
		{
			{	IDR_TE2G,	0x6000, 0xA000,	TYPE_GROM	, 0, 0},
			{	IDR_TE2C,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"TI Logo ][",
		{	
			{	IDR_LOGOG,		0x6000, 0x6000,	TYPE_GROM	, 0, 0},
			{	IDR_LOGOC,		0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"TI Workshop (379)",
		{	
			{	IDR_TIWORKSHOP,	0x0000, 0x10000,TYPE_379	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"TurboForth 1.2.1 by Mark Wills",
		{	
			{	IDR_TURBOFORTHC,	0x6000, 0x2000,TYPE_ROM	, 0, 0},
			{	IDR_TURBOFORTHD,	0x6000, 0x2000,TYPE_XB	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"XB2.7 Suite by Tony Knerr",
		{	
			{	IDR_XB27GROM,		0x0000, 0x1E000,	TYPE_UBER_GROM		, 0, 0},
			{	IDR_XB27ROM,		0x0000, 0x80000,	TYPE_378			, 0, 0},
			{	IDR_XB27EEPROM,		0x0000, 0x1000,		TYPE_UBER_EEPROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"XB2.9 GEM by Harry Wilhelm",
		{	
			{	IDR_XB29GROM,		0x6000, 0xA000,		TYPE_GROM			, 0, 0},
			{	IDR_XB29ROM,		0x0000, 0x80000,	TYPE_378			, 0, 0},
		},
		NULL,
		NULL,
		0
	},
};

struct CARTS Games[] = {
	{	
		"Alpiner",	
		{	
			{	IDR_ALPINERG,	0x6000, 0x8000,	TYPE_GROM	, 0, 0},
			{	IDR_ALPINERC,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},
	
	{	
		"A-Maze-Ing",
		{	
			{	IDR_AMAZEG,		0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"BlackJack&&Poker",
		{
			{IDR_BLACKJACK,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Car Wars",	
		{	
			{	IDR_CARWARS,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Chisholm Trail",
		{
			{	IDR_CHISHOLMG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
			{	IDR_CHISHOLMC,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Football",
		{	
			{	IDR_FOOTBALLG,	0x6000, 0x4000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Hustle",	
		{	
			{	IDR_HUSTLEG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Hunt the Wumpus",
		{
			{	IDR_WUMPUSG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},
 
	{	
		"Mind Challengers",
		{
			{	IDR_MINDG,		0x6000, 0x2000,	TYPE_GROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Munch Man",
		{	
			{	IDR_MUNCHMNG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
			{	IDR_MUNCHMNC,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},
	
	{	
		"Parsec",	
		{	
			{	IDR_PARSECG,	0x6000, 0x6000,	TYPE_GROM	, 0, 0},
			{	IDR_PARSECC,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},
						
	{	
		"Scott Adam's Adventure",
		{
			{IDR_ADVENTUREG,	0x6000,	0x1A50,	TYPE_GROM	, 0, 0},
		},
		Disk_ScottAdams,
		"Numerous adventure files are built-in.",
		0
	},

	{	
		"Super Space Acer",
		{
			{	IDR_SSALOAD,	0x6000, 0x0030,	TYPE_ROM	, 0, 0},
			{	IDR_DEMQ,		0x2000,	0x0706,	TYPE_RAM	, 0, 0},
			{	IDR_SSARAM,		0xA000,	0x5A28,	TYPE_RAM	, 0, 0},
		},
		Disk_SSA,
		NULL,
		0
	},

	{	
		"TI Invaders",
		{	
			{	IDR_TIINVADG,	0x6000, 0x8000,	TYPE_GROM	, 0, 0},
			{	IDR_TIINVADC,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Tombstone City",
		{
			{	IDR_TOMBCITG,	0x6000, 0x2000,	TYPE_GROM	, 0, 0},
			{	IDR_TOMBCITC,	0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},

	{	
		"Tunnels of Doom",
		{
			{	IDR_TUNDOOMG,	0x6000,	0xA000,	TYPE_GROM	, 0, 0},
		},
		Disk_Tunnels,
		"Select DSK1, and PENNIES for introductory quest, or QUEST for a full quest.",
		0
	},

	{	
		"Video Chess",
		{	
			{	IDR_CHESSG,		0x6000, 0x8000,	TYPE_GROM	, 0, 0},
			{	IDR_CHESSC,		0x6000,	0x2000,	TYPE_ROM	, 0, 0},
		},
		NULL,
		NULL,
		0
	},
};

// Some DLL attach code for printing info
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		OutputDebugString("Classic99 Cartpack 20250215\n");
        OutputDebugString("Code by Texas Instruments, Mike Brent, Mark Wills, Richard Lynn Gilbertson,\n");
        OutputDebugString("Tony Knerr, Lee Stewart, DataBioTics Ltd, Scott Adams, Harry Wilhelm.\n");
		OutputDebugString("All software used with permission.\n");
	}
    return TRUE;
}

// get application list
struct CARTS * get_app_array() {
    return Apps;
}

int get_app_count() {
    return sizeof(Apps)/sizeof(Apps[0]);
}

// get application list
struct CARTS * get_game_array() {
    return Games;
}

int get_game_count() {
    return sizeof(Games)/sizeof(Games[0]);
}
