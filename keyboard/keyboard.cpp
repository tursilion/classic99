//
// (C) 2004 Mike Brent aka Tursi aka HarmlessLion.com
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
// Keyboard routines for Classic99 - separate file in hopes of
// this data being useful on the real thing!
//
// The TI Keyboard works by selecting a column, and reading a row
// For the purpose of this table, the mapping to the keyboard connector
// doesn't directly match the numbering, which is based on the software.
// So (99/4A):
// Col		Pin		Purpose
// 0		n/c		Joystick 2
// 1		15		MJU74FRV
// 2		8		/;P01AQZ
// 3		13		.L092SWX
// 4		n/c		Joystick 1
// 5		14		,KI83DEC
// 6		9		NHY65GTB
// 7		12		=<space><cr><nc><fctn><shift><ctrl><nc>
//
// Row		Pin		Purpose
// 0		5		=.,mn/<fire>
// 1		4		<space>LKJH;<left>
// 2		1		<enter>OIUYP<right>
// 3		2		<nc>98760<down>
// 4		7		<fctn>23451<up><alpha lock>
// 5		3		<shift>SDFGA
// 6		10		<ctrl>WERTQ
// 7		11		<nc>XCVBZ

// Scancode Mapping table
// TODO: The emulator really should use a virtual key mapping table, so it works on non-US keyboards
// But we'll do scancodes now to proof of concept the PS2 interface

#include <windows.h>

// an Atmel thing
#define PROGMEM

// Standard table, keyscan code to row/col
// row1,col1, row2,col2 (if needed), 0xff if not
// Note that these tables do not go to 255, only to 127,
// and that code 0x7F in the flat and shifted tables is
// *really* code 0x83, but it was moved to save space
// in the Atmel
const signed char scan2ti994aflat[128][4] PROGMEM = {
{ -1, -1,   -1, -1 },     // 00
{  4,  7,    3,  3 },     // 01
{ -1, -1,   -1, -1 },     // 02
{  4,  7,    4,  6 },     // 03
{  4,  7,    4,  5 },     // 04
{  4,  7,    4,  2 },     // 05
{  4,  7,    4,  3 },     // 06
{  6,  7,    4,  3 },     // 07
{ -1, -1,   -1, -1 },     // 08
{  4,  7,    3,  2 },     // 09
{  4,  7,    3,  5 },     // 0A
{  4,  7,    3,  6 },     // 0B
{  4,  7,    4,  1 },     // 0C
{  4,  7,    3,  1 },     // 0D
{  4,  7,    7,  5 },     // 0E
{ -1, -1,   -1, -1 },     // 0F
{ -1, -1,   -1, -1 },     // 10
{  4,  7,   -1, -1 },     // 11
{  5,  7,   -1, -1 },     // 12
{ -1, -1,   -1, -1 },     // 13
{  6,  7,   -1, -1 },     // 14
{  6,  2,   -1, -1 },     // 15
{  4,  2,   -1, -1 },     // 16
{ -1, -1,   -1, -1 },     // 17
{ -1, -1,   -1, -1 },     // 18
{ -1, -1,   -1, -1 },     // 19
{  7,  2,   -1, -1 },     // 1A
{  5,  3,   -1, -1 },     // 1B
{  5,  2,   -1, -1 },     // 1C
{  6,  3,   -1, -1 },     // 1D
{  4,  3,   -1, -1 },     // 1E
{ -1, -1,   -1, -1 },     // 1F
{ -1, -1,   -1, -1 },     // 20
{  7,  5,   -1, -1 },     // 21
{  7,  3,   -1, -1 },     // 22
{  5,  5,   -1, -1 },     // 23
{  6,  5,   -1, -1 },     // 24
{  4,  1,   -1, -1 },     // 25
{  4,  5,   -1, -1 },     // 26
{ -1, -1,   -1, -1 },     // 27
{ -1, -1,   -1, -1 },     // 28
{  1,  7,   -1, -1 },     // 29
{  7,  1,   -1, -1 },     // 2A
{  5,  1,   -1, -1 },     // 2B
{  6,  6,   -1, -1 },     // 2C
{  6,  1,   -1, -1 },     // 2D
{  4,  6,   -1, -1 },     // 2E
{ -1, -1,   -1, -1 },     // 2F
{ -1, -1,   -1, -1 },     // 30
{  0,  6,   -1, -1 },     // 31
{  7,  6,   -1, -1 },     // 32
{  1,  6,   -1, -1 },     // 33
{  5,  6,   -1, -1 },     // 34
{  2,  6,   -1, -1 },     // 35
{  3,  6,   -1, -1 },     // 36
{ -1, -1,   -1, -1 },     // 37
{ -1, -1,   -1, -1 },     // 38
{ -1, -1,   -1, -1 },     // 39
{  0,  1,   -1, -1 },     // 3A
{  1,  1,   -1, -1 },     // 3B
{  2,  1,   -1, -1 },     // 3C
{  3,  1,   -1, -1 },     // 3D
{  3,  5,   -1, -1 },     // 3E
{ -1, -1,   -1, -1 },     // 3F
{ -1, -1,   -1, -1 },     // 40
{  0,  5,   -1, -1 },     // 41
{  1,  5,   -1, -1 },     // 42
{  2,  5,   -1, -1 },     // 43
{  2,  3,   -1, -1 },     // 44
{  3,  2,   -1, -1 },     // 45
{  3,  3,   -1, -1 },     // 46
{ -1, -1,   -1, -1 },     // 47
{ -1, -1,   -1, -1 },     // 48
{  0,  3,   -1, -1 },     // 49
{  0,  2,   -1, -1 },     // 4A
{  1,  3,   -1, -1 },     // 4B
{  1,  2,   -1, -1 },     // 4C
{  2,  2,   -1, -1 },     // 4D
{  5,  7,    0,  2 },     // 4E
{ -1, -1,   -1, -1 },     // 4F
{ -1, -1,   -1, -1 },     // 50
{ -1, -1,   -1, -1 },     // 51
{  4,  7,    2,  3 },     // 52
{ -1, -1,   -1, -1 },     // 53
{  4,  7,    6,  1 },     // 54
{  0,  7,   -1, -1 },     // 55
{ -1, -1,   -1, -1 },     // 56
{ -1, -1,   -1, -1 },     // 57
{  4,  8,   -1, -1 },     // 58
{  5,  7,   -1, -1 },     // 59
{  2,  7,   -1, -1 },     // 5A
{  4,  7,    6,  6 },     // 5B
{ -1, -1,   -1, -1 },     // 5C
{  4,  7,    7,  2 },     // 5D
{ -1, -1,   -1, -1 },     // 5E
{ -1, -1,   -1, -1 },     // 5F
{ -1, -1,   -1, -1 },     // 60
{ -1, -1,   -1, -1 },     // 61
{ -1, -1,   -1, -1 },     // 62
{ -1, -1,   -1, -1 },     // 63
{ -1, -1,   -1, -1 },     // 64
{ -1, -1,   -1, -1 },     // 65
{  4,  7,    5,  3 },     // 66
{ -1, -1,   -1, -1 },     // 67
{ -1, -1,   -1, -1 },     // 68
{  4,  2,   -1, -1 },     // 69
{ -1, -1,   -1, -1 },     // 6A
{  4,  1,   -1, -1 },     // 6B
{  3,  1,   -1, -1 },     // 6C
{ -1, -1,   -1, -1 },     // 6D
{ -1, -1,   -1, -1 },     // 6E
{ -1, -1,   -1, -1 },     // 6F
{  3,  2,   -1, -1 },     // 70
{  0,  3,   -1, -1 },     // 71
{  4,  3,   -1, -1 },     // 72
{  4,  6,   -1, -1 },     // 73
{  3,  6,   -1, -1 },     // 74
{  3,  5,   -1, -1 },     // 75
{  4,  7,    3,  3 },     // 76
{  6,  7,    6,  3 },     // 77
{  6,  7,    4,  2 },     // 78
{  5,  7,    0,  7 },     // 79
{  4,  5,   -1, -1 },     // 7A
{  5,  7,    0,  2 },     // 7B
{  5,  7,    3,  5 },     // 7C
{  3,  3,   -1, -1 },     // 7D
{  6,  7,    7,  6 },     // 7E
{  4,  7,    3,  1 },     // 7F - this is really 0x83 - special case!!
};
// Shifted table, keyscan code to row/col
// row1,col1, row2,col2 (if needed), 0xff if not
// Note this table matches the flat table where the code is not different
// Note that 0x7F must be special cased as 0x83 - this
// is to save memory! 0x7F-0x82 are not used
const signed char scan2ti994ashift[128][4] PROGMEM = {
{ -1, -1,   -1, -1 },     // 00
{  4,  7,    3,  3 },     // 01
{ -1, -1,   -1, -1 },     // 02
{  4,  7,    4,  6 },     // 03
{  4,  7,    4,  5 },     // 04
{  4,  7,    4,  2 },     // 05
{  4,  7,    4,  3 },     // 06
{  6,  7,    4,  3 },     // 07
{ -1, -1,   -1, -1 },     // 08
{  4,  7,    3,  2 },     // 09
{  4,  7,    3,  5 },     // 0A
{  4,  7,    3,  6 },     // 0B
{  4,  7,    4,  1 },     // 0C
{  4,  7,    3,  1 },     // 0D
{  4,  7,    6,  3 },     // 0E
{ -1, -1,   -1, -1 },     // 0F
{ -1, -1,   -1, -1 },     // 10
{  4,  7,   -1, -1 },     // 11
{  5,  7,   -1, -1 },     // 12
{ -1, -1,   -1, -1 },     // 13
{  6,  7,   -1, -1 },     // 14
{  6,  2,    5,  7 },     // 15
{  4,  2,    5,  7 },     // 16
{ -1, -1,   -1, -1 },     // 17
{ -1, -1,   -1, -1 },     // 18
{ -1, -1,   -1, -1 },     // 19
{  7,  2,    5,  7 },     // 1A
{  5,  3,    5,  7 },     // 1B
{  5,  2,    5,  7 },     // 1C
{  6,  3,    5,  7 },     // 1D
{  4,  3,    5,  7 },     // 1E
{ -1, -1,   -1, -1 },     // 1F
{ -1, -1,   -1, -1 },     // 20
{  7,  5,    5,  7 },     // 21
{  7,  3,    5,  7 },     // 22
{  5,  5,    5,  7 },     // 23
{  6,  5,    5,  7 },     // 24
{  4,  1,    5,  7 },     // 25
{  4,  5,    5,  7 },     // 26
{ -1, -1,   -1, -1 },     // 27
{ -1, -1,   -1, -1 },     // 28
{  1,  7,   -1, -1 },     // 29
{  7,  1,    5,  7 },     // 2A
{  5,  1,    5,  7 },     // 2B
{  6,  6,    5,  7 },     // 2C
{  6,  1,    5,  7 },     // 2D
{  4,  6,    5,  7 },     // 2E
{ -1, -1,   -1, -1 },     // 2F
{ -1, -1,   -1, -1 },     // 30
{  0,  6,    5,  7 },     // 31
{  7,  6,    5,  7 },     // 32
{  1,  6,    5,  7 },     // 33
{  5,  6,    5,  7 },     // 34
{  2,  6,    5,  7 },     // 35
{  3,  6,    5,  7 },     // 36
{ -1, -1,   -1, -1 },     // 37
{ -1, -1,   -1, -1 },     // 38
{ -1, -1,   -1, -1 },     // 39
{  0,  1,    5,  7 },     // 3A
{  1,  1,    5,  7 },     // 3B
{  2,  1,    5,  7 },     // 3C
{  3,  1,    5,  7 },     // 3D
{  3,  5,    5,  7 },     // 3E
{ -1, -1,   -1, -1 },     // 3F
{ -1, -1,   -1, -1 },     // 40
{  0,  5,    5,  7 },     // 41
{  1,  5,    5,  7 },     // 42
{  2,  5,    5,  7 },     // 43
{  2,  3,    5,  7 },     // 44
{  3,  2,    5,  7 },     // 45
{  3,  3,    5,  7 },     // 46
{ -1, -1,   -1, -1 },     // 47
{ -1, -1,   -1, -1 },     // 48
{  0,  3,    5,  7 },     // 49
{  4,  7,    2,  5 },     // 4A
{  1,  3,    5,  7 },     // 4B
{  1,  2,    5,  7 },     // 4C
{  2,  2,    5,  7 },     // 4D
{  4,  7,    2,  1 },     // 4E
{ -1, -1,   -1, -1 },     // 4F
{ -1, -1,   -1, -1 },     // 50
{ -1, -1,   -1, -1 },     // 51
{  4,  7,    2,  2 },     // 52
{ -1, -1,   -1, -1 },     // 53
{  4,  7,    5,  1 },     // 54
{  0,  7,    5,  7 },     // 55
{ -1, -1,   -1, -1 },     // 56
{ -1, -1,   -1, -1 },     // 57
{  4,  8,   -1, -1 },     // 58
{  5,  7,   -1, -1 },     // 59
{  2,  7,   -1, -1 },     // 5A
{  4,  7,    5,  6 },     // 5B
{ -1, -1,   -1, -1 },     // 5C
{  4,  7,    5,  2 },     // 5D
{ -1, -1,   -1, -1 },     // 5E
{ -1, -1,   -1, -1 },     // 5F
{ -1, -1,   -1, -1 },     // 60
{ -1, -1,   -1, -1 },     // 61
{ -1, -1,   -1, -1 },     // 62
{ -1, -1,   -1, -1 },     // 63
{ -1, -1,   -1, -1 },     // 64
{ -1, -1,   -1, -1 },     // 65
{  4,  7,    5,  3 },     // 66
{ -1, -1,   -1, -1 },     // 67
{ -1, -1,   -1, -1 },     // 68
{  4,  2,   -1, -1 },     // 69
{ -1, -1,   -1, -1 },     // 6A
{  4,  1,   -1, -1 },     // 6B
{  3,  1,   -1, -1 },     // 6C
{ -1, -1,   -1, -1 },     // 6D
{ -1, -1,   -1, -1 },     // 6E
{ -1, -1,   -1, -1 },     // 6F
{  3,  2,   -1, -1 },     // 70
{  0,  3,   -1, -1 },     // 71
{  4,  3,   -1, -1 },     // 72
{  4,  6,   -1, -1 },     // 73
{  3,  6,   -1, -1 },     // 74
{  3,  5,   -1, -1 },     // 75
{  6,  7,    0,  3 },     // 76
{  6,  7,    6,  3 },     // 77
{  6,  7,    4,  2 },     // 78
{  5,  7,    0,  7 },     // 79
{  4,  5,   -1, -1 },     // 7A
{  5,  7,    0,  2 },     // 7B
{  5,  7,    3,  5 },     // 7C
{  3,  3,   -1, -1 },     // 7D
{  6,  7,    7,  6 },     // 7E
{  4,  7,    3,  1 },     // 7F
};

// Extended scancode (0xE0) table, keyscan code to row/col
// row1,col1, row2,col2 (if needed), 0xff if not
// there is no corresponding shifted table,
// and no special case at the end.
const signed char scan2ti994aextend[128][4] PROGMEM = {
{ -1, -1,   -1, -1 },     // 00
{ -1, -1,   -1, -1 },     // 01
{ -1, -1,   -1, -1 },     // 02
{ -1, -1,   -1, -1 },     // 03
{ -1, -1,   -1, -1 },     // 04
{ -1, -1,   -1, -1 },     // 05
{ -1, -1,   -1, -1 },     // 06
{ -1, -1,   -1, -1 },     // 07
{ -1, -1,   -1, -1 },     // 08
{ -1, -1,   -1, -1 },     // 09
{ -1, -1,   -1, -1 },     // 0A
{ -1, -1,   -1, -1 },     // 0B
{ -1, -1,   -1, -1 },     // 0C
{ -1, -1,   -1, -1 },     // 0D
{ -1, -1,   -1, -1 },     // 0E
{ -1, -1,   -1, -1 },     // 0F
{  6,  7,    0,  6 },     // 10
{  4,  7,   -1, -1 },     // 11
{  6,  7,    5,  2 },     // 12
{ -1, -1,   -1, -1 },     // 13
{  6,  7,   -1, -1 },     // 14
{  4,  7,    4,  1 },     // 15
{ -1, -1,   -1, -1 },     // 16
{ -1, -1,   -1, -1 },     // 17
{  6,  7,    6,  6 },     // 18
{ -1, -1,   -1, -1 },     // 19
{ -1, -1,   -1, -1 },     // 1A
{ -1, -1,   -1, -1 },     // 1B
{ -1, -1,   -1, -1 },     // 1C
{ -1, -1,   -1, -1 },     // 1D
{ -1, -1,   -1, -1 },     // 1E
{  5,  7,   -1, -1 },     // 1F
{  6,  7,    5,  3 },     // 20
{  6,  7,    2,  5 },     // 21
{ -1, -1,   -1, -1 },     // 22
{  6,  7,    5,  6 },     // 23
{ -1, -1,   -1, -1 },     // 24
{ -1, -1,   -1, -1 },     // 25
{ -1, -1,   -1, -1 },     // 26
{  5,  7,   -1, -1 },     // 27
{  6,  7,    6,  1 },     // 28
{ -1, -1,   -1, -1 },     // 29
{ -1, -1,   -1, -1 },     // 2A
{  6,  7,    1,  3 },     // 2B
{ -1, -1,   -1, -1 },     // 2C
{ -1, -1,   -1, -1 },     // 2D
{ -1, -1,   -1, -1 },     // 2E
{  6,  7,    7,  2 },     // 2F
{  6,  7,    6,  2 },     // 30
{ -1, -1,   -1, -1 },     // 31
{  6,  7,    1,  6 },     // 32
{ -1, -1,   -1, -1 },     // 33
{  6,  7,    5,  1 },     // 34
{ -1, -1,   -1, -1 },     // 35
{ -1, -1,   -1, -1 },     // 36
{  6,  7,    7,  5 },     // 37
{  6,  7,    2,  2 },     // 38
{ -1, -1,   -1, -1 },     // 39
{  6,  7,    2,  3 },     // 3A
{  4,  7,    4,  1 },     // 3B
{ -1, -1,   -1, -1 },     // 3C
{ -1, -1,   -1, -1 },     // 3D
{ -1, -1,   -1, -1 },     // 3E
{  6,  7,    5,  5 },     // 3F
{  6,  7,    0,  1 },     // 40
{ -1, -1,   -1, -1 },     // 41
{ -1, -1,   -1, -1 },     // 42
{ -1, -1,   -1, -1 },     // 43
{ -1, -1,   -1, -1 },     // 44
{ -1, -1,   -1, -1 },     // 45
{ -1, -1,   -1, -1 },     // 46
{ -1, -1,   -1, -1 },     // 47
{  6,  7,    1,  5 },     // 48
{ -1, -1,   -1, -1 },     // 49
{  0,  2,   -1, -1 },     // 4A
{ -1, -1,   -1, -1 },     // 4B
{ -1, -1,   -1, -1 },     // 4C
{  4,  7,    3,  6 },     // 4D
{ -1, -1,   -1, -1 },     // 4E
{ -1, -1,   -1, -1 },     // 4F
{  6,  7,    1,  1 },     // 50
{ -1, -1,   -1, -1 },     // 51
{ -1, -1,   -1, -1 },     // 52
{ -1, -1,   -1, -1 },     // 53
{ -1, -1,   -1, -1 },     // 54
{ -1, -1,   -1, -1 },     // 55
{ -1, -1,   -1, -1 },     // 56
{ -1, -1,   -1, -1 },     // 57
{ -1, -1,   -1, -1 },     // 58
{ -1, -1,   -1, -1 },     // 59
{  2,  7,   -1, -1 },     // 5A
{ -1, -1,   -1, -1 },     // 5B
{ -1, -1,   -1, -1 },     // 5C
{ -1, -1,   -1, -1 },     // 5D
{  6,  7,    6,  5 },     // 5E
{ -1, -1,   -1, -1 },     // 5F
{ -1, -1,   -1, -1 },     // 60
{ -1, -1,   -1, -1 },     // 61
{ -1, -1,   -1, -1 },     // 62
{ -1, -1,   -1, -1 },     // 63
{ -1, -1,   -1, -1 },     // 64
{ -1, -1,   -1, -1 },     // 65
{ -1, -1,   -1, -1 },     // 66
{ -1, -1,   -1, -1 },     // 67
{ -1, -1,   -1, -1 },     // 68
{  6,  7,    7,  1 },     // 69
{ -1, -1,   -1, -1 },     // 6A
{  4,  7,    5,  3 },     // 6B
{  6,  7,    2,  1 },     // 6C
{ -1, -1,   -1, -1 },     // 6D
{ -1, -1,   -1, -1 },     // 6E
{ -1, -1,   -1, -1 },     // 6F
{  4,  7,    4,  3 },     // 70
{  4,  7,    4,  2 },     // 71
{  4,  7,    7,  3 },     // 72
{ -1, -1,   -1, -1 },     // 73
{  4,  7,    5,  5 },     // 74
{  4,  7,    6,  5 },     // 75
{ -1, -1,   -1, -1 },     // 76
{ -1, -1,   -1, -1 },     // 77
{ -1, -1,   -1, -1 },     // 78
{ -1, -1,   -1, -1 },     // 79
{  4,  7,    4,  1 },     // 7A
{ -1, -1,   -1, -1 },     // 7B
{  6,  7,    5,  2 },     // 7C
{  4,  7,    3,  6 },     // 7D
{ -1, -1,   -1, -1 },     // 7E
{ -1, -1,   -1, -1 },     // 7F
};

// Code to generate the tables */
// NOTE: This code is missing the extra shifts needed to make it work.
// All the letters, numbers, and keys who shifted the same on the TI and
// the PC that did *not* include the shift key in the shifted table
// actually needed to on real hardware. These generation tables
// have not been updated (except for fixing the definition of EQUALS).
// The actual tables above *are* fixed and in use on real hardware
// In other words, you can not regenerate the tables above from the
// code below anymore, because I suck. ;)
#if 0
// Special TI keys
#define	FCTN VK_MENU
#define	CTRL VK_CONTROL
#define	SHIFT VK_SHIFT
#define	ENTER VK_RETURN
#define COMMA VK_OEM_COMMA
#define PERIOD VK_OEM_PERIOD
#define SLASH VK_OEM_2
#define SEMI VK_OEM_1
#define EQUALS VK_OEM_PLUS
#define	ALPHA 0x7f

int ScanCode2TI[2][132][4]	= {		// normal/extended, scan code 00-83, 2 normal, 2 shifted
{	// Normal keys
	{	0xff, 0xff	, 	0xff, 0xff	},		// 00	
	{	FCTN,'9', 		0xff, 0xff	},		// 01	F9
	{	0xff, 0xff	, 	0xff, 0xff	},		// 02	
	{	FCTN,'5',		0xff, 0xff	},		// 03	F5
	{	FCTN,'3', 		0xff, 0xff	},		// 04	F3
	{	FCTN,'1',	 	0xff, 0xff	},		// 05	F1
	{	FCTN,'2', 		0xff, 0xff	},		// 06	F2
	{	CTRL,'2',		0xff, 0xff	},		// 07	F12
	{	0xff, 0xff	, 	0xff, 0xff	},		// 08
	{	FCTN,'0', 		0xff, 0xff	},		// 09	F10
	{	FCTN,'8',		0xff, 0xff	},		// 0A	F8
	{	FCTN,'6', 		0xff, 0xff	},		// 0B	F6
	{	FCTN,'4',		0xff, 0xff	},		// 0C	F4
	{	FCTN,'7', 		0xff, 0xff	},		// 0D	TAB
	{	FCTN,'C', 		FCTN,'W'},			// 0E	` ~
	{	0xff, 0xff	, 	0xff, 0xff	},		// 0F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 10
	{	FCTN, 0xff, 	0xff, 0xff	},		// 11	ALT
	{	SHIFT,0xff, 	0xff, 0xff	},		// 12	L SHIFT
	{	0xff, 0xff	, 	0xff, 0xff	},		// 13
	{	CTRL,0xff	, 	0xff, 0xff	},		// 14	L CTRL
	{	'Q', 0xff	, 	0xff, 0xff	},		// 15	Q
	{	'1', 0xff	, 	0xff, 0xff	},		// 16	1
	{	0xff, 0xff	, 	0xff, 0xff	},		// 17
	{	0xff, 0xff	, 	0xff, 0xff	},		// 18
	{	0xff, 0xff	, 	0xff, 0xff	},		// 19
	{	'Z', 0xff	, 	0xff, 0xff	},		// 1A	Z
	{	'S', 0xff	, 	0xff, 0xff	},		// 1B	S
	{	'A', 0xff	, 	0xff, 0xff	},		// 1C	A
	{	'W', 0xff	, 	0xff, 0xff	},		// 1D	W
	{	'2', 0xff	, 	0xff, 0xff	},		// 1E	2
	{	0xff, 0xff	, 	0xff, 0xff	},		// 1F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 20
	{	'C', 0xff	, 	0xff, 0xff	},		// 21	C
	{	'X', 0xff	, 	0xff, 0xff	},		// 22	X
	{	'D', 0xff	, 	0xff, 0xff	},		// 23	D
	{	'E', 0xff	, 	0xff, 0xff	},		// 24	E
	{	'4', 0xff	, 	0xff, 0xff	},		// 25	4
	{	'3', 0xff	, 	0xff, 0xff	},		// 26	3
	{	0xff, 0xff	, 	0xff, 0xff	},		// 27
	{	0xff, 0xff	, 	0xff, 0xff	},		// 28
	{	' ', 0xff	, 	0xff, 0xff	},		// 29	SPACE
	{	'V', 0xff	, 	0xff, 0xff	},		// 2A	V
	{	'F', 0xff	, 	0xff, 0xff	},		// 2B	F
	{	'T', 0xff	, 	0xff, 0xff	},		// 2C	T
	{	'R', 0xff	, 	0xff, 0xff	},		// 2D	R
	{	'5', 0xff	, 	0xff, 0xff	},		// 2E	5
	{	0xff, 0xff	, 	0xff, 0xff	},		// 2F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 30
	{	'N', 0xff	, 	0xff, 0xff	},		// 31	N
	{	'B', 0xff	, 	0xff, 0xff	},		// 32	B
	{	'H', 0xff	, 	0xff, 0xff	},		// 33	H
	{	'G', 0xff	, 	0xff, 0xff	},		// 34	G
	{	'Y', 0xff	, 	0xff, 0xff	},		// 35	Y
	{	'6', 0xff	, 	0xff, 0xff	},		// 36	6
	{	0xff, 0xff	, 	0xff, 0xff	},		// 37
	{	0xff, 0xff	, 	0xff, 0xff	},		// 38
	{	0xff, 0xff	, 	0xff, 0xff	},		// 39
	{	'M', 0xff	, 	0xff, 0xff	},		// 3A	M
	{	'J', 0xff	, 	0xff, 0xff	},		// 3B	J
	{	'U', 0xff	, 	0xff, 0xff	},		// 3C	U
	{	'7', 0xff	, 	0xff, 0xff	},		// 3D	7
	{	'8', 0xff	, 	0xff, 0xff	},		// 3E	8
	{	0xff, 0xff	, 	0xff, 0xff	},		// 3F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 40
	{	COMMA, 0xff, 	0xff, 0xff	},		// 41	,
	{	'K', 0xff	, 	0xff, 0xff	},		// 42	K
	{	'I', 0xff	, 	0xff, 0xff	},		// 43	I
	{	'O', 0xff	, 	0xff, 0xff	},		// 44	O
	{	'0', 0xff	, 	0xff, 0xff	},		// 45	0
	{	'9', 0xff	, 	0xff, 0xff	},		// 46	9
	{	0xff, 0xff	, 	0xff, 0xff	},		// 47
	{	0xff, 0xff	, 	0xff, 0xff	},		// 48
	{	PERIOD, 0xff, 	0xff, 0xff	},		// 49	.
	{	SLASH, 0xff	, 	FCTN,'I'},			// 4A	/
	{	'L', 0xff	, 	0xff, 0xff	},		// 4B	L
	{	SEMI, 0xff	, 	0xff, 0xff	},		// 4C	;
	{	'P', 0xff	, 	0xff, 0xff	},		// 4D	P
	{	SHIFT,SLASH,	FCTN,'U'},			// 4E	- _
	{	0xff, 0xff	, 	0xff, 0xff	},		// 4F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 50
	{	0xff, 0xff	, 	0xff, 0xff	},		// 51
	{	FCTN,'O', 		FCTN,'P'},			// 52	' "
	{	0xff, 0xff	, 	0xff, 0xff	},		// 53
	{	FCTN,'R', 		FCTN,'F'},			// 54	[ {
	{	EQUALS, 0xff, 	0xff, 0xff	},		// 55	=
	{	0xff, 0xff	, 	0xff, 0xff	},		// 56
	{	0xff, 0xff	, 	0xff, 0xff	},		// 57
	{	ALPHA,0xff, 	0xff, 0xff	},		// 58	CAPS
	{	SHIFT,0xff, 	0xff, 0xff	},		// 59	R SHIFT
	{	ENTER,0xff, 	0xff, 0xff	},		// 5A	RETURN
	{	FCTN,'T', 		FCTN,'G'},			// 5B	] }
	{	0xff, 0xff	, 	0xff, 0xff	},		// 5C
	{	FCTN,'Z', 		FCTN,'A'},			// 5D	\ |
	{	0xff, 0xff	, 	0xff, 0xff	},		// 5E
	{	0xff, 0xff	, 	0xff, 0xff	},		// 5F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 60
	{	0xff, 0xff	, 	0xff, 0xff	},		// 61
	{	0xff, 0xff	, 	0xff, 0xff	},		// 62
	{	0xff, 0xff	, 	0xff, 0xff	},		// 63
	{	0xff, 0xff	, 	0xff, 0xff	},		// 64
	{	0xff, 0xff	, 	0xff, 0xff	},		// 65
	{	FCTN,'S', 		0xff, 0xff	},		// 66	BACKSPACE
	{	0xff, 0xff	, 	0xff, 0xff	},		// 67
	{	0xff, 0xff	, 	0xff, 0xff	},		// 68
	{	'1',0xff	, 	0xff, 0xff	},		// 69	1 NUMPAD
	{	0xff, 0xff	, 	0xff, 0xff	},		// 6A
	{	'4',0xff	, 	0xff, 0xff	},		// 6B	4 NUMPAD
	{	'7',0xff	, 	0xff, 0xff	},		// 6C	7 NUMPAD
	{	0xff, 0xff	, 	0xff, 0xff	},		// 6D
	{	0xff, 0xff	, 	0xff, 0xff	},		// 6E
	{	0xff, 0xff	, 	0xff, 0xff	},		// 6F
	{	'0', 0xff	, 	0xff, 0xff	},		// 70	0 NUMPAD
	{	PERIOD, 0xff, 	0xff, 0xff	},		// 71	. NUMPAD
	{	'2',0xff	, 	0xff, 0xff	},		// 72	2 NUMPAD
	{	'5',0xff	, 	0xff, 0xff	},		// 73	5 NUMPAD
	{	'6',0xff	, 	0xff, 0xff	},		// 74	6 NUMPAD
	{	'8',0xff	, 	0xff, 0xff	},		// 75	8 NUMPAD
	{	FCTN,'9',		0xff, 0xff	},		// 76	ESC
	{	CTRL,'W', 		0xff, 0xff	},		// 77	NUM LOCK
	{	CTRL,'1', 		0xff, 0xff	},		// 78	F11	
	{	SHIFT,EQUALS,	0xff, 0xff	},		// 79	+ NUMPAD
	{	'3',0xff	, 	0xff, 0xff	},		// 7A	3 NUMPAD
	{	SHIFT,SLASH,	0xff, 0xff	},		// 7B	- NUMPAD
	{	SHIFT,'8', 		0xff, 0xff	},		// 7C	* NUMPAD
	{	'9',0xff	, 	0xff, 0xff	},		// 7D	9 NUMPAD
	{	CTRL,'B', 		0xff, 0xff	},		// 7E	SCROLL LOCK
	{	0xff, 0xff	, 	0xff, 0xff	},		// 7F
	{	0xff, 0xff	, 	0xff, 0xff	},		// 80
	{	0xff, 0xff	, 	0xff, 0xff	},		// 81
	{	0xff, 0xff	, 	0xff, 0xff	},		// 82
	{	FCTN,'7', 		0xff, 0xff	},		// 83	F7
},
{	// Extended keys
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 00
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 01
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 02
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 03
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 04
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 05
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 06
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 07
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 08
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 09
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 0A
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 0B
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 0C
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 0D
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 0E
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 0F
	{	CTRL,'N', 		0xff, 0xff	},		// E0 10	WWW SEARCH
	{	FCTN, 0xff, 	0xff, 0xff	},		// E0 11	R ALT
	{	CTRL,'A', 		0xff, 0xff	},		// E0 12	PRINT SCREEN (1/2)
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 13
	{	CTRL,0xff	, 	0xff, 0xff	},		// E0 14	R CONTROL
	{	FCTN,'4', 		0xff, 0xff	},		// E0 15	PREVIOUS TRACK
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 16
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 17
	{	CTRL,'T', 		0xff, 0xff	},		// E0 18	WWW FAVORITES
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 19
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 1A
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 1B
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 1C
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 1D
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 1E
	{	SHIFT,0xff,		0xff, 0xff	},		// E0 1F	L WINDOWS (mapped to SHIFT alone)
	{	CTRL,'S', 		0xff, 0xff	},		// E0 20	WWW REFRESH
	{	CTRL,'I', 		0xff, 0xff	},		// E0 21	VOLUME DOWN
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 22	
	{	CTRL,'G', 		0xff, 0xff	},		// E0 23	MUTE
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 24
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 25
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 26
	{	SHIFT,0xff  ,	0xff, 0xff	},		// E0 27	R WINDOWS (mapped to SHIFT alone)
	{	CTRL,'R', 		0xff, 0xff	},		// E0 28	WWW STOP
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 29
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 2A
	{	CTRL,'L', 		0xff, 0xff	},		// E0 2B	CALCULATOR
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 2C
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 2D
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 2E
	{	CTRL,'Z', 		0xff, 0xff	},		// E0 2F	APPS (ie: Windows Menu key)
	{	CTRL,'Q', 		0xff, 0xff	},		// E0 30	WWW FORWARD
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 31
	{	CTRL,'H', 		0xff, 0xff	},		// E0 32	VOLUME UP
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 33
	{	CTRL,'F', 		0xff, 0xff	},		// E0 34	PLAY/PAUSE
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 35
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 36
	{	CTRL,'C', 		0xff, 0xff	},		// E0 37	POWER
	{	CTRL,'P', 		0xff, 0xff	},		// E0 38	WWW BACK
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 39
	{	CTRL,'O', 		0xff, 0xff	},		// E0 3A	WWW HOME
	{	FCTN,'4', 		0xff, 0xff	},		// E0 3B	STOP
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 3C
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 3D
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 3E
	{	CTRL,'D', 		0xff, 0xff	},		// E0 3F	SLEEP
	{	CTRL,'M', 		0xff, 0xff	},		// E0 40	MY COMPUTER
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 41
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 42
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 43
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 44
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 45
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 46
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 47
	{	CTRL,'K', 		0xff, 0xff	},		// E0 48	E-MAIL
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 49
	{	SLASH,0xff	, 	0xff, 0xff	},		// E0 4A	/ NUMPAD
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 4B
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 4C
	{	FCTN,'6', 		0xff, 0xff	},		// E0 4D	NEXT TRACK
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 4E
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 4F
	{	CTRL,'J', 		0xff, 0xff	},		// E0 50	MEDIA SELECT
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 51
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 52
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 53
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 54
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 55
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 56
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 57
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 58
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 59
	{	ENTER,0xff, 	0xff, 0xff	},		// E0 5A	ENTER NUMPAD
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 5B
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 5C
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 5D
	{	CTRL,'E', 		0xff, 0xff	},		// E0 5E	WAKE
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 5F
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 60
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 61
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 62
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 63
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 64
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 65
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 66
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 67
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 68
	{	CTRL,'V', 		0xff, 0xff	},		// E0 69	END
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 6A
	{	FCTN,'S', 		0xff, 0xff	},		// E0 6B	LEFT ARROW
	{	CTRL,'U', 		0xff, 0xff	},		// E0 6C	HOME
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 6D
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 6E
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 6F
	{	FCTN,'2', 		0xff, 0xff	},		// E0 70	INSERT
	{	FCTN,'1', 		0xff, 0xff	},		// E0 71	DELETE
	{	FCTN,'X', 		0xff, 0xff	},		// E0 72	DOWN ARROW
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 73
	{	FCTN,'D', 		0xff, 0xff	},		// E0 74	RIGHT ARROW
	{	FCTN,'E', 		0xff, 0xff	},		// E0 75	UP ARROW
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 76
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 77
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 78
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 79
	{	FCTN,'4', 		0xff, 0xff	},		// E0 7A	PAGE DOWN
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 7B
	{	CTRL,'A', 		0xff, 0xff	},		// E0 7C	PRINT SCREEN (2/2)
	{	FCTN,'6', 		0xff, 0xff	},		// E0 7D	PAGE UP
	{	FCTN,'4',	 	0xff, 0xff	},		// E0 7E	CTRL-BREAK (note: generates control-4 in KSCAN but will break a BASIC program)
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 7F
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 80
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 81
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 82
	{	0xff, 0xff	, 	0xff, 0xff	},		// E0 83
},
};
	
// Build the tables
extern int KEYS[2][8][8];

// Build the TI99/4A Table (only one I'm sure of)
#include <stdio.h>

FILE *fp;

// Three tables - standard, shifted, and extended
void BuildKeyScanTables() {
	int idx;
	char flattable[256][4];

	memset(flattable, 0xff, sizeof(flattable));

	fp=fopen("C:\\TI994ATBL.txt", "w");

	printf("Flat table...\n");

	fprintf(fp, "// Standard table, keyscan code to row/col\n");
	fprintf(fp, "// row1,col1, row2,col2 (if needed), 0xff if not\n");
	fprintf(fp, "const char scan2ti994aflat[256][4] = {\n");
	for (idx=0; idx<0x84; idx++) {
		int r1=-1,r2=-1,c1=-1,c2=-1;

		if (ScanCode2TI[0][idx][0]!=0xff) {
			if (ScanCode2TI[0][idx][0]==ALPHA) {
				r1=4;
				c1=8;		// out of range - it has it's own pin
			} else {
				for (r1=0; r1<8; r1++) {
					for (c1=0; c1<8; c1++) {
						if (ScanCode2TI[0][idx][0] == KEYS[1][c1][r1]) {
							break;
						}
					}
					if (c1<8) {
						break;
					}
				} 
				if (r1>7) {
					printf("Warning, no match on row %d for first key\n", idx);
					r1=-1;
					c1=-1;
				}
				if (ScanCode2TI[0][idx][1]!=0xff) {
					for (r2=0; r2<8; r2++) {
						for (c2=0; c2<8; c2++) {
							if (ScanCode2TI[0][idx][1] == KEYS[1][c2][r2]) {
								break;
							}
						}
						if (c2<8) {
							break;
						}
					}
					if (r2>7) {
						printf("Warning, no match on row %d for second key\n", idx);
						r2=-1;
						c2=-1;
					}
				}
			}
		}
		fprintf(fp, "{ %2d, %2d,   %2d, %2d },     // %02X\n", r1, c1, r2, c2, idx);
		flattable[idx][0]=r1;
		flattable[idx][1]=c1;
		flattable[idx][2]=r2;
		flattable[idx][3]=c2;
	}
	for (idx=0x84; idx<=0xff; idx++) {
		fprintf(fp, "{ %2d, %2d,   %2d, %2d },     // %02X\n", -1, -1, -1, -1, idx);
	}
	fprintf(fp, "};\n");

	printf("Shifted table...\n");
	
	fprintf(fp, "// Shifted table, keyscan code to row/col\n");
	fprintf(fp, "// row1,col1, row2,col2 (if needed), 0xff if not\n");
	fprintf(fp, "// Note this table matches the flat table where the code is not different\n");
	fprintf(fp, "const char scan2ti994ashift[256][4] = {\n");
	for (idx=0; idx<0x84; idx++) {
		int r1=-1,r2=-1,c1=-1,c2=-1;

		if (ScanCode2TI[0][idx][2]==0xff) {
			r1=flattable[idx][0];
			c1=flattable[idx][1];
			r2=flattable[idx][2];
			c2=flattable[idx][3];
		} else {
			if (ScanCode2TI[0][idx][2]==ALPHA) {
				r1=4;
				c1=8;		// out of range - it has it's own pin
			} else {
				for (r1=0; r1<8; r1++) {
					for (c1=0; c1<8; c1++) {
						if (ScanCode2TI[0][idx][2] == KEYS[1][c1][r1]) {
							break;
						}
					}
					if (c1<8) {
						break;
					}
				}
				if (r1>7) {
					printf("Warning, no match on row %d for first key\n", idx);
					r1=-1;
					c1=-1;
				}
				if (ScanCode2TI[0][idx][3]!=0xff) {
					for (r2=0; r2<8; r2++) {
						for (c2=0; c2<8; c2++) {
							if (ScanCode2TI[0][idx][3] == KEYS[1][c2][r2]) {
								break;
							}
						}
						if (c2<8) {
							break;
						}
					}
					if (r2>7) {
						printf("Warning, no match on row %d for second key\n", idx);
						r2=-1;
						c2=-1;
					}
				}
			}
		}
		fprintf(fp, "{ %2d, %2d,   %2d, %2d },     // %02X\n", r1, c1, r2, c2, idx);
	}
	for (idx=0x84; idx<=0xff; idx++) {
		fprintf(fp, "{ %2d, %2d,   %2d, %2d },     // %02X\n", -1, -1, -1, -1, idx);
	}
	fprintf(fp, "};\n");

	printf("Extended table...\n");

	fprintf(fp, "// Extended scancode (0xE0) table, keyscan code to row/col\n");
	fprintf(fp, "// row1,col1, row2,col2 (if needed), 0xff if not\n");
	fprintf(fp, "// there is no corresponding shifted table\n");
	fprintf(fp, "const char scan2ti994aextend[256][4] = {\n");
	for (idx=0; idx<0x83; idx++) {
		int r1=-1,r2=-1,c1=-1,c2=-1;

		if (ScanCode2TI[1][idx][0]!=0xff) {
			if (ScanCode2TI[1][idx][0]==ALPHA) {
				r1=4;
				c1=8;		// out of range - it has it's own pin
			} else {
				for (r1=0; r1<8; r1++) {
					for (c1=0; c1<8; c1++) {
						if (ScanCode2TI[1][idx][0] == KEYS[1][c1][r1]) {
							break;
						}
					}
					if (c1<8) {
						break;
					}
				}
				if (r1>7) {
					printf("Warning, no match on row %d for first key\n", idx);
					r1=-1;
					c1=-1;
				}
				if (ScanCode2TI[1][idx][1]!=0xff) {
					for (r2=0; r2<8; r2++) {
						for (c2=0; c2<8; c2++) {
							if (ScanCode2TI[1][idx][1] == KEYS[1][c2][r2]) {
								break;
							}
						}
						if (c2<8) {
							break;
						}
					}
					if (r2>7) {
						printf("Warning, no match on row %d for second key\n", idx);
						r2=-1;
						c2=-1;
					}
				}
			}
		}
		fprintf(fp, "{ %2d, %2d,   %2d, %2d },     // %02X\n", r1, c1, r2, c2, idx);
	}
	for (idx=0x84; idx<=0xff; idx++) {
		fprintf(fp, "{ %2d, %2d,   %2d, %2d },     // %02X\n", -1, -1, -1, -1, idx);
	}
	fprintf(fp, "};\n");

	printf("Done\n");
	fclose(fp);

	exit(0);
}
#endif
