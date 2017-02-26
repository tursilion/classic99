
/* sms_ntsc 0.2.2. http://www.slack.net/~ant/ */
/* compilable in C or C++; just change the file extension */

#include "sms_ntsc.h"

#include <assert.h>
#include <math.h>

/* Based on algorithm by NewRisingSun */
/* Copyright (C) 2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details. You should have received a copy of the GNU Lesser General
Public License along with this module; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

enum { disable_correction = 0 }; /* for debugging */

/* macro constants are used instead of enum in some places to work around compiler bug */

#define rgb_unit 0x100

/* begin mostly common NES/SNES/SMS code */

sms_ntsc_setup_t const sms_ntsc_monochrome = { 0,-1, 0, 0,.2,  0, .2,-.2,-.2,-1, 0,  0 };
sms_ntsc_setup_t const sms_ntsc_composite  = { 0, 0, 0, 0, 0,  0,.25,  0,  0, 0, 0,  0 };
sms_ntsc_setup_t const sms_ntsc_svideo     = { 0, 0, 0, 0, 0,  0,.25, -1, -1, 0, 0,  0 };
sms_ntsc_setup_t const sms_ntsc_rgb        = { 0, 0, 0, 0,.2,  0,.70, -1, -1,-1, 0,  0 };

enum { alignment_count = 3 }; /* different pixel alignments with respect to yiq quads */

enum { kernel_half = 16 };
enum { kernel_size = kernel_half * 2 + 1 };
#define rescale_in 8
enum { rescale_out = 7 };

struct ntsc_impl_t
{
	float to_rgb [6];
	float brightness;
	float contrast;
	float artifacts;
	float fringing;
	float kernel [rescale_out * kernel_size * 2];
};

#define PI 3.14159265358979323846f

static void init_ntsc_impl( struct ntsc_impl_t* impl, sms_ntsc_setup_t const* setup )
{
	float kernels [kernel_size * 2];
	
	impl->brightness = (float) setup->brightness * (0.4f * rgb_unit);
	impl->contrast   = (float) setup->contrast   * 0.4f + 1.0f;
	impl->artifacts  = (float) setup->artifacts  * 0.4f + 0.4f;
	impl->fringing   = (float) setup->fringing   * 0.5f + 0.5f;
	
	/* generate luma (y) filter using sinc kernel */
	{
		/* sinc with rolloff (dsf) */
		/* double precision avoids instability */
		double const rolloff = 1 + setup->sharpness * 0.004;
		double const maxh = 256;
		double const pow_a_n = pow( rolloff, maxh );
		float sum;
		int i;
		/* quadratic mapping to reduce negative (blurring) range */
		double to_angle = setup->resolution + 1;
		to_angle = PI / maxh * 0.20 * (to_angle * to_angle + 1);
		
		kernels [kernel_size * 3 / 2] = (float) maxh;
		for ( i = 0; i < kernel_half * 2 + 1; i++ )
		{
			int x = i - kernel_half;
			double angle = x * to_angle;
			/* instability occurs at center point with rolloff very close to 1.0 */
			if ( x || pow_a_n > 1.01 || pow_a_n < 0.99 )
			{
				double rolloff_cos_a = rolloff * cos( angle );
				double num = 1 - rolloff_cos_a -
						pow_a_n * cos( maxh * angle ) +
						pow_a_n * rolloff * cos( (maxh - 1) * angle );
				double den = 1 - rolloff_cos_a - rolloff_cos_a + rolloff * rolloff;
				double dsf = num / den;
				kernels [kernel_size * 3 / 2 - kernel_half + i] = (float) dsf;
			}
		}
		
		/* apply blackman window and find sum */
		sum = 0;
		for ( i = 0; i < kernel_half * 2 + 1; i++ )
		{
			float x = PI * 2 / (kernel_half * 2) * i;
			float blackman = 0.42f - 0.5f * (float) cos( x ) + 0.08f * (float) cos( x * 2 );
			sum += (kernels [kernel_size * 3 / 2 - kernel_half + i] *= blackman);
		}
		
		/* normalize kernel */
		sum = 1.0f / sum;
		for ( i = 0; i < kernel_half * 2 + 1; i++ )
		{
			int x = kernel_size * 3 / 2 - kernel_half + i;
			kernels [x] *= sum;
			assert( kernels [x] == kernels [x] ); /* catch numerical instability */
		}
	}
	
	/* generate chroma (iq) filter using gaussian kernel */
	{
		float const cutoff_factor = -0.03125f;
		float cutoff = (float) setup->bleed;
		int i;
		
		if ( cutoff < 0 )
		{
			/* keep extreme value accessible only near upper end of scale (1.0) */
			cutoff *= cutoff;
			cutoff *= cutoff;
			cutoff *= cutoff;
			cutoff *= -30.0f / 0.65f;
		}
		cutoff = cutoff_factor - 0.65f * cutoff_factor * cutoff;
		
		for ( i = -kernel_half; i <= kernel_half; i++ )
			kernels [kernel_size / 2 + i] = (float) exp( i * i * cutoff );
		
		/* normalize even and odd phases separately */
		for ( i = 0; i < 2; i++ )
		{
			float sum = 0;
			int x;
			for ( x = i; x < kernel_size; x += 2 )
				sum += kernels [x];
			
			sum = 1.0f / sum;
			for ( x = i; x < kernel_size; x += 2 )
			{
				kernels [x] *= sum;
				assert( kernels [x] == kernels [x] ); /* catch numerical instability */
			}
		}
	}
	
	/* generate linear rescale kernels */
	{
		int i;
		for ( i = 0; i < rescale_out; i++ )
		{
			float* out = &impl->kernel [i * kernel_size * 2];
			float second = 1.0f / rescale_in * (i + 1);
			float first  = 1.0f - second;
			int x;
			*out++ = kernels [0] * first;
			for ( x = 1; x < kernel_size * 2; x++ )
				*out++ = kernels [x] * first + kernels [x - 1] * second;
		}
	}
	
	/* setup decoder matrix */
	{
		static float const default_decoder [6] =
			{ 0.956f, 0.621f, -0.272f, -0.647f, -1.105f, 1.702f };
		float hue = (float) setup->hue * PI;
		float sat = (float) setup->saturation;
		float const* in = setup->decoder_matrix;
		if ( !in )
			in = default_decoder;
		sat = (sat < 0 ? sat : sat * 0.41f) + 1;
		{
			float s = (float) sin( hue ) * sat;
			float c = (float) cos( hue ) * sat;
			float* out = impl->to_rgb;
			int n;
			for ( n = 3; n; --n )
			{
				float i = *in++;
				float q = *in++;
				*out++ = i * c - q * s;
				*out++ = i * s + q * c;
			}
		}
	}
}

/* kernel generation */

enum { rgb_kernel_size = sms_ntsc_entry_size / alignment_count };

static float const rgb_offset = rgb_unit * 2 + 0.5f;
static ntsc_rgb_t const ntsc_rgb_bias = rgb_unit * 2 * ntsc_rgb_builder;

#define TO_RGB( y, i, q, to_rgb ) ( \
	((int) (y + to_rgb [0] * i + to_rgb [1] * q) << 21) +\
	((int) (y + to_rgb [2] * i + to_rgb [3] * q) << 11) +\
	((int) (y + to_rgb [4] * i + to_rgb [5] * q) <<  1)\
)

typedef struct pixel_info_t
{
	int offset;
	float negate;
	float kernel [4];
} pixel_info_t;

#define PIXEL_OFFSET_( ntsc, scaled ) \
	(kernel_size / 2 + ntsc + (scaled != 0) + (rescale_out - scaled) % rescale_out + \
			(kernel_size * 2 * scaled))

#define PIXEL_OFFSET( ntsc, scaled ) \
	PIXEL_OFFSET_( ((ntsc) - (scaled) / rescale_out * rescale_in),\
			(((scaled) + rescale_out * 10) % rescale_out) ),\
	(1.0f - (((ntsc) + 100) & 2))

/* Generate pixel at all burst phases and column alignments */
static void gen_kernel( struct ntsc_impl_t* impl, float y, float i, float q, ntsc_rgb_t* out )
{
	static pixel_info_t const pixels [alignment_count] = {
		{ PIXEL_OFFSET( -4, -9 ), { 1.0000f, 1.0000f,  .6667f,  .0000f } },
		{ PIXEL_OFFSET( -2, -7 ), {  .3333f, 1.0000f, 1.0000f,  .3333f } },
		{ PIXEL_OFFSET(  0, -5 ), {  .0000f,  .6667f, 1.0000f, 1.0000f } },
	};
	
	/* Encode yiq into *two* composite signals (to allow control over artifacting).
	Convolve these with kernels which: filter respective components, apply
	sharpening, and rescale horizontally. Convert resulting yiq to rgb and pack
	into integer. */
	pixel_info_t const* pixel = pixels;
	do
	{
		/* negate is -1 when composite starts at odd multiple of 2 */
		float const yy = y * impl->fringing * pixel->negate;
		float const ic0 = (i + yy) * pixel->kernel [0];
		float const qc1 = (q + yy) * pixel->kernel [1];
		float const ic2 = (i - yy) * pixel->kernel [2];
		float const qc3 = (q - yy) * pixel->kernel [3];
		
		float const factor = impl->artifacts * pixel->negate;
		float const ii = i * factor;
		float const yc0 = (y + ii) * pixel->kernel [0];
		float const yc2 = (y - ii) * pixel->kernel [2];
		
		float const qq = q * factor;
		float const yc1 = (y + qq) * pixel->kernel [1];
		float const yc3 = (y - qq) * pixel->kernel [3];
		
		float const* k = &impl->kernel [pixel->offset];
		int n;
		for ( n = rgb_kernel_size; n; --n )
		{
			float i = k[0]*ic0 + k[2]*ic2;
			float q = k[1]*qc1 + k[3]*qc3;
			float y = k[kernel_size+0]*yc0 + k[kernel_size+1]*yc1 +
			          k[kernel_size+2]*yc2 + k[kernel_size+3]*yc3 + rgb_offset;
			if ( k >= &impl->kernel [kernel_size * 2 * (rescale_out - 1)] )
				k -= kernel_size * 2 * (rescale_out - 1) + 2;
			else
				k += kernel_size * 2 - 1;
			*out++ = TO_RGB( y, i, q, impl->to_rgb ) - ntsc_rgb_bias;
		}
	}
	while ( pixel++ < &pixels [alignment_count - 1] );
}

static void correct_errors( ntsc_rgb_t color, ntsc_rgb_t* out )
{
	int i;
	for ( i = 0; i < rgb_kernel_size / 2; i++ )
	{
		ntsc_rgb_t error = color -
				out [i           ] -
				out [i + 3    +28] -
				out [i + 5    +14] -
				out [i + 7       ] -
				out [(i+10)%14+28] -
				out [(i+12)%14+14];
		if ( disable_correction ) { out [i] += ntsc_rgb_bias; continue; }
		/* distributing error gave poor visual results */
		out [i + 3 + 28] += error;
	}
}
/* end common code */

void sms_ntsc_init( sms_ntsc_t* ntsc, sms_ntsc_setup_t const* setup )
{
	float to_float [16];
	
	/* init impl */
	int entry;
	struct ntsc_impl_t impl;
	if ( !setup )
		setup = &sms_ntsc_composite;
	init_ntsc_impl( &impl, setup );
	
	/* gamma table */
	{
		float gamma = 1.0f - (float) setup->gamma * 0.5f;
		int i;
		for ( i = 0; i < 16; i++ )
			to_float [i] = (float) pow( (1 / 15.0f) * i, gamma ) * rgb_unit;
	}
	
	for ( entry = 0; entry < sms_ntsc_color_count; entry++ )
	{
		/* calc yiq for color */
		float b = to_float [entry >> 8 & 0x0F];
		float g = to_float [entry >> 4 & 0x0F];
		float r = to_float [entry      & 0x0F];
		
		float y = r * 0.303f + g * 0.586f + b * 0.111f;
		float i = r * 0.596f - g * 0.275f - b * 0.321f;
		float q = r * 0.212f - g * 0.523f + b * 0.311f;
		y = y * impl.contrast + impl.brightness;
		
		/* generate kernel */
		if ( ntsc )
			gen_kernel( &impl, y, i, q, ntsc->table [entry] );
		
		y += rgb_offset;
		{
			ntsc_rgb_t rgb = TO_RGB( y, i, q, impl.to_rgb );
			
			unsigned char* out = setup->palette_out;
			if ( out )
			{
				ntsc_rgb_t clamped = rgb;
				SMS_NTSC_CLAMP_( clamped );
				out += entry * 3;
				out [0] = clamped >> 21 & 0xFF;
				out [1] = clamped >> 11 & 0xFF;
				out [2] = clamped >>  1 & 0xFF;
			}
			
			if ( ntsc )
				correct_errors( rgb, ntsc->table [entry] );
		}
	}
}

/* Disable 'restrict' keyword by default. If your compiler supports it, put

	#define restrict restrict

somewhere in a config header, or the equivalent in the command-line:

	-Drestrict=restrict

If your compiler supports a non-standard version, like __restrict, do this:

	#define restrict __restrict

Enabling this if your compiler supports it will allow better optimization. */
#ifndef restrict
	#define restrict
#endif

/* Default to 16-bit RGB input */
#ifndef SMS_NTSC_IN_FORMAT
	#define SMS_NTSC_IN_FORMAT SMS_NTSC_RGB16
#endif

/* Default to 16-bit RGB output */
#ifndef SMS_NTSC_OUT_DEPTH
	#define SMS_NTSC_OUT_DEPTH 16
#endif

#include <limits.h>

#if SMS_NTSC_OUT_DEPTH > 16
	#if UINT_MAX == 0xFFFFFFFF
		typedef unsigned int  sms_ntsc_out_t;
	#elif ULONG_MAX == 0xFFFFFFFF
		typedef unsigned long sms_ntsc_out_t;
	#else
		#error "Need 32-bit int type"
	#endif
#else
	#if USHRT_MAX == 0xFFFF
		typedef unsigned short sms_ntsc_out_t;
	#else
		#error "Need 16-bit int type"
	#endif
#endif

/* useful if you have a linker which doesn't remove unused code from executable */
#ifndef SMS_NTSC_NO_BLITTERS

/* Use this as a starting point for writing your own blitter. To allow easy upgrades
to new versions of this library, put your blitter in a separate source file rather
than modifying this one directly. */

void sms_ntsc_blit( sms_ntsc_t const* ntsc, unsigned short const* sms_in, long in_row_width,
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
		unsigned short const* line_in = sms_in;
		SMS_NTSC_BEGIN_ROW( ntsc, sms_ntsc_black,
				line_in [0] & extra2, line_in [extra2 & 1] & extra1 );
		sms_ntsc_out_t* line_out = (sms_ntsc_out_t*) rgb_out;
		int n;
		line_in += in_extra;
		
		/* blit main chunks, each using 3 input pixels to generate 7 output pixels */
		for ( n = chunk_count; n; --n )
		{
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

#endif

