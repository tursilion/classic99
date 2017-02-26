// Just a wrapper to set the defines up :)
// This code by M.Brent, based on code by Shay Green
// all updated 3/3/14 for 32-bit color

#include <windows.h>
#define SMS_NTSC_IN_FORMAT SMS_NTSC_RGB32
#define SMS_NTSC_OUT_DEPTH 32
#define SMS_NTSC_NO_BLITTERS

#pragma warning (disable:4244)	// disable warning about implicit type conversions
#include "sms_ntsc.c"

// Some DLL attach code for printing info
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		OutputDebugString("FilterDLL for Classic99 version 1.0\n");
		OutputDebugString("LGPL code by Shay Green, modified by Tursi\n");
	}
    return TRUE;
}

// Add scanlines to every odd line, based on 15-bit display
// Based on the 16 bit code in demo.c
void sms_ntsc_scanlines(void *pFrame, int nWidth, int nStride, int nHeight) {
	/* interpolate and darken between scanlines */
	int y;
	for ( y = 1; y < nHeight - 1; y += 2 )
	{
		unsigned char* io = ((unsigned char*)pFrame) + y * nStride;
		int n;
		for ( n = nWidth; n; --n )
		{
			unsigned int prev = *(unsigned int*) (io - nStride);
			unsigned int next = *(unsigned int*) (io + nStride);
			/* mix 32-bit rgb without losing low bits */
			int r=(prev&0xff0000)+(next&0xff0000); 
			int g=(prev&0xff00)+(next&0xff00); 
			int b=(prev&0xff)+(next&0xff); 
			if (r>0xff0000) r=0xff0000; 
			if (g>0xff00) g=0xff00;
			if (b>0xff) b=0xff;
			/* darken and merge back together */
			*(unsigned int*) io = ((r&0xfe0000)>>1)|((g&0xfe00)>>1)|((b&0xfe)>>1);
			io += 4;
		}
	}
}

// Same as the blitter in sms_ntsc.c, except that this one assumes it can count backwards 3 pixels from
// the beginning of each row to prime the filters
void sms_ntsc_blit( sms_ntsc_t const* ntsc, unsigned int const* sms_in, long in_row_width,
		int in_width, int height, void* rgb_out, long out_pitch )
{
	int const chunk_count = in_width / sms_ntsc_in_chunk;
	/* handle extra 0, 1, or 2 pixels by placing them at beginning of row */
	int const in_extra = in_width - chunk_count * sms_ntsc_in_chunk;
	unsigned const extra2 = (unsigned) -(in_extra >> 1 & 1); /* (unsigned) -1 = ~0 */
	unsigned const extra1 = (unsigned) -(in_extra & 1) | extra2;

	while ( height-- )
	{
		/* begin row and read up to two pixels */
		unsigned int const* line_in = sms_in;
		SMS_NTSC_BEGIN_ROW( ntsc, sms_ntsc_black,
				line_in [0] & extra2, line_in [extra2 & 1] & extra1 );
		sms_ntsc_out_t* line_out = (sms_ntsc_out_t*) rgb_out;
		int n;
		line_in += in_extra;
		
		/* blit main chunks, each using 3 input pixels to generate 7 output pixels */
		for ( n = chunk_count; n; --n )
		{
			/* Added 20 Apr 2006 by M.Brent - prime the pump a bit */
			/* YOU WOULD NEVER DO THIS NORMALLY, but Classic99 jumps in */
			/* about 20+ pixels into the line, so we can prime the */
			/* pump by counting backwards a bit. */
			SMS_NTSC_COLOR_IN(0, ntsc, line_in[-3]);
			SMS_NTSC_COLOR_IN(1, ntsc, line_in[-2]);
			SMS_NTSC_COLOR_IN(2, ntsc, line_in[-1]);
			/* End terrible hack that will crash your code! ;) */

			/* order of input and output pixels must not be altered */
			SMS_NTSC_COLOR_IN( 0, ntsc, line_in [0] );
			SMS_NTSC_RGB_OUT( 0, line_out [0], SMS_NTSC_OUT_DEPTH );
			SMS_NTSC_RGB_OUT( 1, line_out [1], SMS_NTSC_OUT_DEPTH );
			
			SMS_NTSC_COLOR_IN( 1, ntsc, line_in [1] );
			SMS_NTSC_RGB_OUT( 2, line_out [2], SMS_NTSC_OUT_DEPTH );
			SMS_NTSC_RGB_OUT( 3, line_out [3], SMS_NTSC_OUT_DEPTH );
			
			SMS_NTSC_COLOR_IN( 2, ntsc, line_in [2] );
			SMS_NTSC_RGB_OUT( 4, line_out [4], SMS_NTSC_OUT_DEPTH );
			SMS_NTSC_RGB_OUT( 5, line_out [5], SMS_NTSC_OUT_DEPTH );
			SMS_NTSC_RGB_OUT( 6, line_out [6], SMS_NTSC_OUT_DEPTH );
			
			line_in  += 3;
			line_out += 7;
		}
		
		/* finish final pixels without starting any new ones */
		SMS_NTSC_COLOR_IN( 0, ntsc, sms_ntsc_black );
		SMS_NTSC_RGB_OUT( 0, line_out [0], SMS_NTSC_OUT_DEPTH );
		SMS_NTSC_RGB_OUT( 1, line_out [1], SMS_NTSC_OUT_DEPTH );
		
		SMS_NTSC_COLOR_IN( 1, ntsc, sms_ntsc_black );
		SMS_NTSC_RGB_OUT( 2, line_out [2], SMS_NTSC_OUT_DEPTH );
		SMS_NTSC_RGB_OUT( 3, line_out [3], SMS_NTSC_OUT_DEPTH );
		
		SMS_NTSC_COLOR_IN( 2, ntsc, sms_ntsc_black );
		SMS_NTSC_RGB_OUT( 4, line_out [4], SMS_NTSC_OUT_DEPTH );
		SMS_NTSC_RGB_OUT( 5, line_out [5], SMS_NTSC_OUT_DEPTH );
		SMS_NTSC_RGB_OUT( 6, line_out [6], SMS_NTSC_OUT_DEPTH );
		
		/* advance line pointers */
		sms_in  += in_row_width;
		rgb_out = (char*) rgb_out + out_pitch;
	}
}
