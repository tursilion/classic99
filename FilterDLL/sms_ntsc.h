
/* Sega Master System/Game Gear NTSC composite video to RGB emulator/blitter */

/* sms_ntsc 0.2.2 */

#ifndef SMS_NTSC_H
#define SMS_NTSC_H

/* Image parameters, ranging from -1.0 to 1.0 */
typedef struct sms_ntsc_setup_t
{
	/* Basic parameters */
	double hue;        /* -1 = -180 degrees, +1 = +180 degrees */
	double saturation; /* -1 = grayscale, +1 = oversaturated colors */
	double contrast;
	double brightness;
	double sharpness;  /* edge contrast enhancement/blurring */
	
	/* Advanced parameters */
	double gamma;
	double resolution; /* image resolution */
	double artifacts;  /* artifacts caused by color changes */
	double fringing;   /* color artifacts caused by brightness changes */
	double bleed;      /* color bleed (color resolution reduction) */
	float const* decoder_matrix; /* optional RGB decoder matrix, 6 elements */
	
	unsigned char* palette_out;  /* optional RGB palette out, 3 bytes per color */
} sms_ntsc_setup_t;

/* Video format presets */
extern sms_ntsc_setup_t const sms_ntsc_composite; /* color bleeding + artifacts */
extern sms_ntsc_setup_t const sms_ntsc_svideo;    /* color bleeding only */
extern sms_ntsc_setup_t const sms_ntsc_rgb;       /* crisp image */
extern sms_ntsc_setup_t const sms_ntsc_monochrome;/* desaturated + artifacts */

enum { sms_ntsc_palette_size = 4096 };

/* Initialize and adjust parameters. Can be called multiple times on the same
sms_ntsc_t object. Caller must allocate memory for nes_ntsc_t. Can pass 0
for either parameter. */
typedef struct sms_ntsc_t sms_ntsc_t;
// Commented out by M.Brent 20 Apr 2006
//void sms_ntsc_init( sms_ntsc_t* ntsc, sms_ntsc_setup_t const* setup );

/* Blit one or more rows of pixels. Input pixel format is set by SMS_NTSC_IN_FORMAT
and output RGB depth is set by SMS_NTSC_OUT_DEPTH. Both default to 16-bit RGB.
In_row_width is the number of pixels to get to the next input row. Out_pitch
is the number of *bytes* to get to the next output row. */
// Commented out by M.Brent 20 Apr 2006
//void sms_ntsc_blit( sms_ntsc_t const* ntsc, unsigned short const* sms_in,
//		long in_row_width, int in_width, int height, void* rgb_out, long out_pitch );

/* Number of output pixels written by blitter for given input width. */
#define SMS_NTSC_OUT_WIDTH( in_width ) \
	(((in_width) / sms_ntsc_in_chunk + 1) * sms_ntsc_out_chunk)

/* Number of input pixels that will fit within given output width. Might be
rounded down slightly; use SMS_NTSC_OUT_WIDTH() on result to find rounded
value. */
#define SMS_NTSC_IN_WIDTH( in_width ) \
	(((in_width) / sms_ntsc_out_chunk - 1) * sms_ntsc_in_chunk + 2)


/* Interface for user-defined custom blitters */

enum { sms_ntsc_in_chunk    = 3 }; /* number of input pixels read per chunk */
enum { sms_ntsc_out_chunk   = 7 }; /* number of output pixels generated per chunk */
enum { sms_ntsc_black       = 0 }; /* palette index for black */

/* Begin outputting row and start three pixels. First pixel will be cut off a bit.
Use sms_ntsc_black for unused pixels. Declares variables, so must be before first
statement in a block (unless you're using C++). */
#define SMS_NTSC_BEGIN_ROW( ntsc, pixel0, pixel1, pixel2 ) \
	unsigned const sms_pixel0_ = (pixel0);\
	ntsc_rgb_t const* kernel0  = SMS_NTSC_IN_FORMAT( ntsc, sms_pixel0_ );\
	unsigned const sms_pixel1_ = (pixel1);\
	ntsc_rgb_t const* kernel1  = SMS_NTSC_IN_FORMAT( ntsc, sms_pixel1_ );\
	unsigned const sms_pixel2_ = (pixel2);\
	ntsc_rgb_t const* kernel2  = SMS_NTSC_IN_FORMAT( ntsc, sms_pixel2_ );\
	ntsc_rgb_t const* kernelx0;\
	ntsc_rgb_t const* kernelx1 = kernel0;\
	ntsc_rgb_t const* kernelx2 = kernel0

/* Begin input pixel */
#define SMS_NTSC_COLOR_IN( in_index, ntsc, color_in ) {\
	unsigned n;\
	kernelx##in_index = kernel##in_index;\
	kernel##in_index = (n = (color_in), SMS_NTSC_IN_FORMAT( ntsc, n ));\
}

/* Generate output pixel. Bits can be 24, 16, 15, or 32 (treated as 24):
24: RRRRRRRR GGGGGGGG BBBBBBBB
16:          RRRRRGGG GGGBBBBB
15:           RRRRRGG GGGBBBBB
 0: xxxRRRRR RRRxxGGG GGGGGxxB BBBBBBBx (raw format; x = junk bits)   */

/* Include only the appropriate form for the defined bitdepth         */
/* rather than doing a runtime test each pixel. - M.Brent 20 Apr 2006 */
/*
#define SMS_NTSC_RGB_OUT( x, rgb_out, bits ) {\
	ntsc_rgb_t raw =\
		kernel0  [x       ] + kernel1  [(x+12)%7+14] + kernel2  [(x+10)%7+28] +\
		kernelx0 [(x+7)%14] + kernelx1 [(x+ 5)%7+21] + kernelx2 [(x+ 3)%7+35];\
	SMS_NTSC_CLAMP_( raw );\
	if ( bits == 16 )\
		rgb_out = (raw >> 13  & 0xF800) | (raw >> 8 & 0x07E0) | (raw >> 4 & 0x001F);\
	else if ( bits == 24 || bits == 32 )\
		rgb_out = (raw >> 5 & 0xFF0000) | (raw >> 3 & 0xFF00) | (raw >> 1 & 0xFF);\
	else if ( bits == 15 )\
		rgb_out = (raw >> 14  & 0x7C00) | (raw >> 9 & 0x03E0) | (raw >> 4 & 0x001F);\
	else\
		rgb_out = raw;\
}
*/
#if SMS_NTSC_OUT_DEPTH == 16
#define SMS_NTSC_RGB_OUT( x, rgb_out, bits ) {\
	ntsc_rgb_t raw =\
		kernel0  [x       ] + kernel1  [(x+12)%7+14] + kernel2  [(x+10)%7+28] +\
		kernelx0 [(x+7)%14] + kernelx1 [(x+ 5)%7+21] + kernelx2 [(x+ 3)%7+35];\
	SMS_NTSC_CLAMP_( raw );\
	rgb_out = (raw >> 13  & 0xF800) | (raw >> 8 & 0x07E0) | (raw >> 4 & 0x001F);\
}

#elif (SMS_NTSC_OUT_DEPTH==24 || SMS_NTSC_OUT_DEPTH==32)
#define SMS_NTSC_RGB_OUT( x, rgb_out, bits ) {\
	ntsc_rgb_t raw =\
		kernel0  [x       ] + kernel1  [(x+12)%7+14] + kernel2  [(x+10)%7+28] +\
		kernelx0 [(x+7)%14] + kernelx1 [(x+ 5)%7+21] + kernelx2 [(x+ 3)%7+35];\
	SMS_NTSC_CLAMP_( raw );\
	rgb_out = (raw >> 5 & 0xFF0000) | (raw >> 3 & 0xFF00) | (raw >> 1 & 0xFF);\
}

#elif SMS_NTSC_OUT_DEPTH == 15
#define SMS_NTSC_RGB_OUT( x, rgb_out, bits ) {\
	ntsc_rgb_t raw =\
		kernel0  [x       ] + kernel1  [(x+12)%7+14] + kernel2  [(x+10)%7+28] +\
		kernelx0 [(x+7)%14] + kernelx1 [(x+ 5)%7+21] + kernelx2 [(x+ 3)%7+35];\
	SMS_NTSC_CLAMP_( raw );\
	rgb_out = (raw >> 14  & 0x7C00) | (raw >> 9 & 0x03E0) | (raw >> 4 & 0x001F);\
}

#else
#define SMS_NTSC_RGB_OUT( x, rgb_out, bits ) {\
	ntsc_rgb_t raw =\
		kernel0  [x       ] + kernel1  [(x+12)%7+14] + kernel2  [(x+10)%7+28] +\
		kernelx0 [(x+7)%14] + kernelx1 [(x+ 5)%7+21] + kernelx2 [(x+ 3)%7+35];\
	SMS_NTSC_CLAMP_( raw );\
	rgb_out = raw;\
}
#endif

/* private */

enum { sms_ntsc_entry_size  = 3 * 14 };
enum { sms_ntsc_color_count = 16 * 16 * 16 };
typedef unsigned long ntsc_rgb_t;
struct sms_ntsc_t {
	ntsc_rgb_t table [sms_ntsc_color_count] [sms_ntsc_entry_size];
};

enum { ntsc_rgb_builder = (1L << 21) | (1 << 11) | (1 << 1) };
enum { sms_ntsc_clamp_mask = ntsc_rgb_builder * 3 / 2 };
enum { sms_ntsc_clamp_add  = ntsc_rgb_builder * 0x101 };

// the desired index is eventually BGR12 (4 bits per pixel)
#define SMS_NTSC_BGR12( ntsc, n ) (ntsc)->table [n & 0xFFF]

#define SMS_NTSC_RGB32( ntsc, n ) (ntsc)->table [ ((n << 4) & 0x0f00) | ((n >> 8) & 0x00f0) | ((n >> 20) & 0x000f) ]

#define SMS_NTSC_RGB16( ntsc, n ) \
	(ntsc_rgb_t*) ((char*) (ntsc)->table +\
	((n << 10 & 0x7800) | (n & 0x0780) | (n >> 9 & 0x0078)) *\
	(sms_ntsc_entry_size * sizeof (ntsc_rgb_t) / 8))

#define SMS_NTSC_RGB15( ntsc, n ) \
	(ntsc_rgb_t*) ((char*) (ntsc)->table +\
	((n << 9 & 0x3C00) | (n & 0x03C0) | (n >> 9 & 0x003C)) *\
	(sms_ntsc_entry_size * sizeof (ntsc_rgb_t) / 4))

#define SMS_NTSC_CLAMP_( io ) {\
	ntsc_rgb_t sub = io >> 9 & sms_ntsc_clamp_mask;\
	ntsc_rgb_t clamp = sms_ntsc_clamp_add - sub;\
	io |= clamp;\
	clamp -= sub;\
	io &= clamp;\
}

#endif

