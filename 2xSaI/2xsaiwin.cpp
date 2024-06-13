/* 
* This code borrowed from Snes9x. Original message left as per request,
* but all that's really borrowed is the graphics smoothing routines.
* Adapted 2/28/14 by Tursi for 32-bit source images, removed unused code
*
*
* Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
*
* (c) Copyright 1996 - 2001 Gary Henderson (gary.henderson@ntlworld.com) and
*                           Jerremy Koot (jkoot@snes9x.com)
*
* Super FX C emulator code 
* (c) Copyright 1997 - 1999 Ivar (ivar@snes9x.com) and
*                           Gary Henderson.
* Super FX assembler emulator code (c) Copyright 1998 zsKnight and _Demo_.
*
* DSP1 emulator code (c) Copyright 1998 Ivar, _Demo_ and Gary Henderson.
* C4 asm and some C emulation code (c) Copyright 2000 zsKnight and _Demo_.
* C4 C code (c) Copyright 2001 Gary Henderson (gary.henderson@ntlworld.com).
*
* DOS port code contains the works of other authors. See headers in
* individual files.
*
* Snes9x homepage: http://www.snes9x.com
*
* Permission to use, copy, modify and distribute Snes9x in both binary and
* source form, for non-commercial purposes, is hereby granted without fee,
* providing that this license information and copyright notice appear with
* all copies and any derived work.
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event shall the authors be held liable for any damages
* arising from the use of this software.
*
* Snes9x is freeware for PERSONAL USE only. Commercial users should
* seek permission of the copyright holders first. Commercial use includes
* charging money for Snes9x or software derived from Snes9x.
*
* The copyright holders request that bug fixes and improvements to the code
* should be forwarded to them so everyone can benefit from the modifications
* in future versions.
*
* Super NES and Super Nintendo Entertainment System are trademarks of
* Nintendo Co., Limited and its subsidiary companies.
*/

#include "2xSaI.h"

static uint32 colorMask = 0x00FEFEFE;
static uint32 lowPixelMask = 0x00010101;
static uint32 qcolorMask = 0x00FCFCFC;
static uint32 qlowpixelMask = 0x00030303;

int Init_2xSaI(uint32 BitFormat)
{		
	if (BitFormat == 888) 
	{
		colorMask	= 0x00FEFEFE;	// note: ONE pixel, original code expected TWO
		lowPixelMask= 0x00010101;
		qcolorMask	= 0x00FCFCFC;
		qlowpixelMask=0x00030303;
	}
	else
	{
		return 0;
	}
	return 1;
}

STATIC inline int GetResult1(uint32 A, uint32 B, uint32 C, uint32 D, uint32 /*E*/)
{
	int x = 0; 
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r+=1; 
	if (y <= 1) r-=1;
	return r;
}

STATIC inline int GetResult2(uint32 A, uint32 B, uint32 C, uint32 D, uint32 /*E*/) 
{
	int x = 0; 
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r-=1; 
	if (y <= 1) r+=1;
	return r;
}


STATIC inline int GetResult(uint32 A, uint32 B, uint32 C, uint32 D)
{
	int x = 0; 
	int y = 0;
	int r = 0;
	if (A == C) x+=1; else if (B == C) y+=1;
	if (A == D) x+=1; else if (B == D) y+=1;
	if (x <= 1) r+=1; 
	if (y <= 1) r-=1;
	return r;
}


STATIC inline uint32 INTERPOLATE(uint32 A, uint32 B)
{
	if (A !=B)
	{
		return ( ((A & colorMask) >> 1) + ((B & colorMask) >> 1) + (A & B & lowPixelMask) );
	}
	else return A;
}


STATIC inline uint32 Q_INTERPOLATE(uint32 A, uint32 B, uint32 C, uint32 D)
{
	uint32 x = ((A & qcolorMask) >> 2) +
		((B & qcolorMask) >> 2) +
		((C & qcolorMask) >> 2) +
		((D & qcolorMask) >> 2);
	uint32 y = (A & qlowpixelMask) +
		(B & qlowpixelMask) +
		(C & qlowpixelMask) +
		(D & qlowpixelMask);
	y = (y>>2) & qlowpixelMask;
	return x+y;
}

#define HOR
#define VER
void Super2xSaI(uint8 *srcPtr, uint32 srcPitch,
	uint8 *deltaPtr,
	uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	uint32 *dP;
	uint32 *bP;

	uint32 Nextline = srcPitch >> 2;

	for (height; height; height-=1)
	{
		bP = (uint32 *) srcPtr;
		dP = (uint32 *) dstPtr;
		for (uint32 finish = width; finish; finish -= 1 )
		{
			uint32 color4, color5, color6;
			uint32 color1, color2, color3;
			uint32 colorA0, colorA1, colorA2, colorA3,
				colorB0, colorB1, colorB2, colorB3,
				colorS1, colorS2;
			uint32 product1a, product1b,
				product2a, product2b;

			//---------------------------------------    B1 B2
			//                                         4  5  6 S2
			//                                         1  2  3 S1
			//                                           A1 A2

			colorB0 = *(bP- Nextline - 1);
			colorB1 = *(bP- Nextline);
			colorB2 = *(bP- Nextline + 1);
			colorB3 = *(bP- Nextline + 2);

			color4 = *(bP - 1);
			color5 = *(bP);
			color6 = *(bP + 1);
			colorS2 = *(bP + 2);

			color1 = *(bP + Nextline - 1);
			color2 = *(bP + Nextline);
			color3 = *(bP + Nextline + 1);
			colorS1 = *(bP + Nextline + 2);

			colorA0 = *(bP + Nextline + Nextline - 1);
			colorA1 = *(bP + Nextline + Nextline);
			colorA2 = *(bP + Nextline + Nextline + 1);
			colorA3 = *(bP + Nextline + Nextline + 2);


			//--------------------------------------
			if (color2 == color6 && color5 != color3)
			{
				product2b = product1b = color2;
			}
			else
				if (color5 == color3 && color2 != color6)
				{
					product2b = product1b = color5;
				}
				else
					if (color5 == color3 && color2 == color6 && color5 != color6)
					{
						int r = 0;

						r += GetResult (color6, color5, color1, colorA1);
						r += GetResult (color6, color5, color4, colorB1);
						r += GetResult (color6, color5, colorA2, colorS1);
						r += GetResult (color6, color5, colorB2, colorS2);

						if (r > 0)
							product2b = product1b = color6;
						else
							if (r < 0)
								product2b = product1b = color5;
							else
							{
								product2b = product1b = INTERPOLATE (color5, color6);
							}

					}
					else
					{

#ifdef VER
						if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
							product2b = Q_INTERPOLATE (color3, color3, color3, color2);
						else
							if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
								product2b = Q_INTERPOLATE (color2, color2, color2, color3);
							else
#endif
								product2b = INTERPOLATE (color2, color3);

#ifdef VER
						if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
							product1b = Q_INTERPOLATE (color6, color6, color6, color5);
						else
							if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
								product1b = Q_INTERPOLATE (color6, color5, color5, color5);
							else
#endif
								product1b = INTERPOLATE (color5, color6);
					}

#ifdef HOR
					if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
						product2a = INTERPOLATE (color2, color5);
					else
						if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
							product2a = INTERPOLATE(color2, color5);
						else
#endif
							product2a = color2;

#ifdef HOR
					if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
						product1a = INTERPOLATE (color2, color5);
					else
						if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
							product1a = INTERPOLATE(color2, color5);
						else
#endif
							product1a = color5;

					*(dP) = product1a;						*(dP+1) = product1b;
					*(dP+(dstPitch>>2)) = product2a; 		*(dP+(dstPitch>>2)+1) = product2b;

					bP += 1;
					dP += 2;
		}//end of for ( finish= width etc..)

		dstPtr += dstPitch << 1;
		srcPtr += srcPitch;
		deltaPtr += srcPitch;
	}; //endof: for (height; height; height--)
}






/*ONLY use with 640x480x16 or higher resolutions*/
/*Only use this if 2*width * 2*height fits on the current screen*/
void SuperEagle(uint8 *srcPtr, uint32 srcPitch,
	uint8 *deltaPtr,
	uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	uint32 *dP;
	uint32 *bP;

	uint32 Nextline = srcPitch >> 2;

	for (height; height; height-=1)
	{
		bP = (uint32 *) srcPtr;
		dP = (uint32 *) dstPtr;
		for (uint32 finish = width; finish; finish -= 1 )
		{

			uint32 color4, color5, color6;
			uint32 color1, color2, color3;
			uint32 colorA0, colorA1, colorA2, colorA3,
				colorB0, colorB1, colorB2, colorB3,
				colorS1, colorS2;
			uint32 product1a, product1b,
				product2a, product2b;

			colorB0 = *(bP- Nextline - 1);
			colorB1 = *(bP- Nextline);
			colorB2 = *(bP- Nextline + 1);
			colorB3 = *(bP- Nextline + 2);

			color4 = *(bP - 1);
			color5 = *(bP);
			color6 = *(bP + 1);
			colorS2 = *(bP + 2);

			color1 = *(bP + Nextline - 1);
			color2 = *(bP + Nextline);
			color3 = *(bP + Nextline + 1);
			colorS1 = *(bP + Nextline + 2);

			colorA0 = *(bP + Nextline + Nextline - 1);
			colorA1 = *(bP + Nextline + Nextline);
			colorA2 = *(bP + Nextline + Nextline + 1);
			colorA3 = *(bP + Nextline + Nextline + 2);


			//--------------------------------------
			if (color2 == color6 && color5 != color3)
			{
				product1b = product2a = color2;
				if ((color1 == color2 && color6 == colorS2) ||
					(color2 == colorA1 && color6 == colorB2))
				{
					product1a = INTERPOLATE (color2, color5);
					product1a = INTERPOLATE (color2, product1a);
					product2b = INTERPOLATE (color2, color3);
					product2b = INTERPOLATE (color2, product2b);
				}
				else
				{
					product1a = INTERPOLATE (color5, color6);
					product2b = INTERPOLATE (color2, color3);
				}
			}
			else
				if (color5 == color3 && color2 != color6)
				{
					product2b = product1a = color5;
					if ((colorB1 == color5 && color3 == colorA2) ||
						(color4 == color5 && color3 == colorS1))
					{
						product1b = INTERPOLATE (color5, color6);
						product1b = INTERPOLATE (color5, product1b);
						product2a = INTERPOLATE (color5, color2);
						product2a = INTERPOLATE (color5, product2a);
					}
					else
					{
						product1b = INTERPOLATE (color5, color6);
						product2a = INTERPOLATE (color2, color3);
					}
				}
				else
					if (color5 == color3 && color2 == color6 && color5 != color6)
					{
						 int r = 0;

						r += GetResult (color6, color5, color1, colorA1);
						r += GetResult (color6, color5, color4, colorB1);
						r += GetResult (color6, color5, colorA2, colorS1);
						r += GetResult (color6, color5, colorB2, colorS2);

						if (r > 0)
						{
							product1b = product2a = color2;
							product1a = product2b = INTERPOLATE (color5, color6);
						}
						else
							if (r < 0)
							{
								product2b = product1a = color5;
								product1b = product2a = INTERPOLATE (color5, color6);
							}
							else
							{
								product2b = product1a = color5;
								product1b = product2a = color2;
							}
					}
					else
					{

						if ((color2 == color5) || (color3 == color6))
						{
							product1a = color5;
							product2a = color2;
							product1b = color6;
							product2b = color3;

						}
						else
						{
							product1b = product1a = INTERPOLATE (color5, color6);
							product1a = INTERPOLATE (color5, product1a);
							product1b = INTERPOLATE (color6, product1b);

							product2a = product2b = INTERPOLATE (color2, color3);
							product2a = INTERPOLATE (color2, product2a);
							product2b = INTERPOLATE (color3, product2b);
						}
					}

					*(dP) = product1a;						*(dP+1) = product1b;
					*(dP+(dstPitch>>2)) = product2a; 		*(dP+(dstPitch>>2)+1) = product2b;

					bP += 1;
					dP += 2;
		}//end of for ( finish= width etc..)

		dstPtr += dstPitch << 1;
		srcPtr += srcPitch;
		deltaPtr += srcPitch;
	}; //endof: for (height; height; height--)
}



/*ONLY use with 640x480x16 or higher resolutions*/
/*Only use this if 2*width * 2*height fits on the current screen*/
void _2xSaI(uint8 *srcPtr, uint32 srcPitch,
	uint8 *deltaPtr,
	uint8 *dstPtr, uint32 dstPitch, int width, int height)
{
	uint32 *dP;
	uint32 *bP;

	uint32 Nextline = srcPitch >> 2;

	for (height; height; height-=1)
	{
		bP = (uint32 *) srcPtr;
		dP = (uint32 *) dstPtr;
		for (uint32 finish = width; finish; finish -= 1 )
		{


			uint32 colorA, colorB;
			uint32 colorC, colorD,
				colorE, colorF, colorG, colorH,
				colorI, colorJ, colorK, colorL,
				colorM, colorN, colorO, colorP;
			uint32 product, product1, product2;


			//---------------------------------------
			// Map of the pixels:                    I|E F|J
			//                                       G|A B|K
			//                                       H|C D|L
			//                                       M|N O|P
			colorI = *(bP- Nextline - 1);
			colorE = *(bP- Nextline);
			colorF = *(bP- Nextline + 1);
			colorJ = *(bP- Nextline + 2);

			colorG = *(bP - 1);
			colorA = *(bP);
			colorB = *(bP + 1);
			colorK = *(bP + 2);

			colorH = *(bP + Nextline - 1);
			colorC = *(bP + Nextline);
			colorD = *(bP + Nextline + 1);
			colorL = *(bP + Nextline + 2);

			colorM = *(bP + Nextline + Nextline - 1);
			colorN = *(bP + Nextline + Nextline);
			colorO = *(bP + Nextline + Nextline + 1);
			colorP = *(bP + Nextline + Nextline + 2);

			if ((colorA == colorD) && (colorB != colorC))
			{
				if ( ((colorA == colorE) && (colorB == colorL)) ||
					((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ)) )
				{
					product = colorA;
				}
				else
				{
					product = INTERPOLATE(colorA, colorB);
				}

				if (((colorA == colorG) && (colorC == colorO)) ||
					((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM)) )
				{
					product1 = colorA;
				}
				else
				{
					product1 = INTERPOLATE(colorA, colorC);
				}
				product2 = colorA;
			}
			else
				if ((colorB == colorC) && (colorA != colorD))
				{
					if (((colorB == colorF) && (colorA == colorH)) ||
						((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI)) )
					{
						product = colorB;
					}
					else
					{
						product = INTERPOLATE(colorA, colorB);
					}

					if (((colorC == colorH) && (colorA == colorF)) ||
						((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI)) )
					{
						product1 = colorC;
					}
					else
					{
						product1 = INTERPOLATE(colorA, colorC);
					}
					product2 = colorB;
				}
				else
					if ((colorA == colorD) && (colorB == colorC))
					{
						if (colorA == colorB)
						{
							product = colorA;
							product1 = colorA;
							product2 = colorA;
						}
						else
						{
							int r = 0;
							product1 = INTERPOLATE(colorA, colorC);
							product = INTERPOLATE(colorA, colorB);

							r += GetResult1 (colorA, colorB, colorG, colorE, colorI);
							r += GetResult2 (colorB, colorA, colorK, colorF, colorJ);
							r += GetResult2 (colorB, colorA, colorH, colorN, colorM);
							r += GetResult1 (colorA, colorB, colorL, colorO, colorP);

							if (r > 0)
								product2 = colorA;
							else
								if (r < 0)
									product2 = colorB;
								else
								{
									product2 = Q_INTERPOLATE(colorA, colorB, colorC, colorD);
								}
						}
					}
					else
					{
						product2 = Q_INTERPOLATE(colorA, colorB, colorC, colorD);

						if ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ))
						{
							product = colorA;
						}
						else
							if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))
							{
								product = colorB;
							}
							else
							{
								product = INTERPOLATE(colorA, colorB);
							}

							if ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM))
							{
								product1 = colorA;
							}
							else
								if ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI))
								{
									product1 = colorC;
								}
								else
								{
									product1 = INTERPOLATE(colorA, colorC);
								}
					}

					*(dP) = colorA;						*(dP+1) = product;
					*(dP+(dstPitch>>2)) = product1; 	*(dP+(dstPitch>>2)+1) = product2;

					bP += 1;
					dP += 2;
		}//end of for ( finish= width etc..)

		dstPtr += dstPitch << 1;
		srcPtr += srcPitch;
		deltaPtr += srcPitch;
	}; //endof: for (height; height; height--)
}
