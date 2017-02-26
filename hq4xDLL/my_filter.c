// Just a wrapper to set the defines up :)
// This code by M.Brent

#include <windows.h>

#include "Image.h"

void InitLUTs();
void hq4x_32( unsigned char * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );

// Some DLL attach code for printing info
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		OutputDebugString("HQ4X for Classic99 version 2.0\n");
		OutputDebugString("LGPL code Copyright (C) 2003 MaxSt ( maxst@hiend3d.com ), modified by Tursi\n");
	}
    return TRUE;
}

void hq4x_init() {
	InitLUTs();
}

void hq4x_process(unsigned char *pBufIn, unsigned char *pBufOut) {
	// this now takes a 32-bit image in, and returns a 32-bit image
	// hard coded for Classic99 - 256+16,192+16 input @ 32 bit, x4 output @ 32bit
	hq4x_32( pBufIn, pBufOut, 256+16, 192+16, (256+16)*4*4 );
}
