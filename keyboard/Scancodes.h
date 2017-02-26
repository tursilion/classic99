//
// (C) 2007 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this software.
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
// Commercial use means ANY distribution for payment, whether or
// not for profit.
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//

// Standard table, keyscan code to row/col
// row1,col1, row2,col2 (if needed), 0xff if not
// Note that 0x7F must be special cased as 0x83 - this
// is to save memory! 0x7F-0x82 are not used

// these column definitions are somewhat off, we correct
// for that when we actually map the values

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
{ -1, -1,   -1, -1 },     // 58	(Caps Lock is manually handled)
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
{ -1, -1,   -1, -1 },     // 58	(Caps Lock is manually handled)
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
{  4,  7,    4,  1 },     // 7E
{ -1, -1,   -1, -1 },     // 7F
};

// Sequence of scancodes for copyright statement - formatted so it can be displayed in TI BASIC
// Please do not remove this feature. Note: text columns don't line up with the bytes.
const unsigned char copyright[] PROGMEM = {
0x12, 0x2D, 0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x12,    // REM 
0x46, 0xF0, 0x46, 0x21, 0xF0, 0x21, 0x45, 0xF0, 0x45, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x72, 0xF0,    // (C) 
0x72, 0x70, 0xF0, 0x70, 0x70, 0xF0, 0x70, 0x6C, 0xF0, 0x6C, 0x29, 0xF0, 0x29, 0x32, 0xF0, 0x32,    // 2007 
0x35, 0xF0, 0x35, 0x29, 0xF0, 0x29, 0x12, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x49, 0xF0, 0x49, 0x12,    // by M.
0x32, 0xF0, 0x32, 0xF0, 0x12, 0x2D, 0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x31, 0xF0, 0x31, 0x2C, 0xF0,    // Bren
0x2C, 0x29, 0xF0, 0x29, 0x1C, 0xF0, 0x1C, 0x42, 0xF0, 0x42, 0x1C, 0xF0, 0x1C, 0x5A, 0xF0, 0x5A,    // t aka
0x12, 0x2D, 0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x12,    // .REM 
0x2C, 0xF0, 0x2C, 0xF0, 0x12, 0x3C, 0xF0, 0x3C, 0x2D, 0xF0, 0x2D, 0x1B, 0xF0, 0x1B, 0x43, 0xF0,    // Turs
0x43, 0x4E, 0xF0, 0x4E, 0x29, 0xF0, 0x29, 0x33, 0xF0, 0x33, 0x1C, 0xF0, 0x1C, 0x2D, 0xF0, 0x2D,    // i- ha
0x3A, 0xF0, 0x3A, 0x4B, 0xF0, 0x4B, 0x24, 0xF0, 0x24, 0x1B, 0xF0, 0x1B, 0x1B, 0xF0, 0x1B, 0x4B,    // rmless
0xF0, 0x4B, 0x43, 0xF0, 0x43, 0x44, 0xF0, 0x44, 0x31, 0xF0, 0x31, 0x49, 0xF0, 0x49, 0x21, 0xF0,    // lion.
0x21, 0x44, 0xF0, 0x44, 0x3A, 0xF0, 0x3A, 0x5A, 0xF0, 0x5A, 0x12, 0x2D, 0xF0, 0x2D, 0x24, 0xF0,    // com.R
0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x12, 0x4B, 0xF0, 0x4B, 0xF0, 0x12, 0x43,    // EM L
0xF0, 0x43, 0x21, 0xF0, 0x21, 0x24, 0xF0, 0x24, 0x31, 0xF0, 0x31, 0x1B, 0xF0, 0x1B, 0x24, 0xF0,    // icens
0x24, 0x23, 0xF0, 0x23, 0x29, 0xF0, 0x29, 0x2B, 0xF0, 0x2B, 0x44, 0xF0, 0x44, 0x2D, 0xF0, 0x2D,    // ed fo
0x29, 0xF0, 0x29, 0x4D, 0xF0, 0x4D, 0x24, 0xF0, 0x24, 0x2D, 0xF0, 0x2D, 0x1B, 0xF0, 0x1B, 0x44,    // r pers
0xF0, 0x44, 0x31, 0xF0, 0x31, 0x1C, 0xF0, 0x1C, 0x4B, 0xF0, 0x4B, 0x5A, 0xF0, 0x5A, 0x12, 0x2D,    // onal.
0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x3C, 0xF0, 0x3C,    // REM 
0x1B, 0xF0, 0x1B, 0x24, 0xF0, 0x24, 0x29, 0xF0, 0x29, 0x44, 0xF0, 0x44, 0x31, 0xF0, 0x31, 0x4B,    // use on
0xF0, 0x4B, 0x35, 0xF0, 0x35, 0x49, 0xF0, 0x49, 0x5A, 0xF0, 0x5A, 0x12, 0x2D, 0xF0, 0x2D, 0x24,    // ly..R
0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x12, 0x2C, 0xF0, 0x2C, 0xF0, 0x12,    // EM T
0x33, 0xF0, 0x33, 0x43, 0xF0, 0x43, 0x1B, 0xF0, 0x1B, 0x29, 0xF0, 0x29, 0x1B, 0xF0, 0x1B, 0x44,    // his s
0xF0, 0x44, 0x2B, 0xF0, 0x2B, 0x2C, 0xF0, 0x2C, 0x1D, 0xF0, 0x1D, 0x1C, 0xF0, 0x1C, 0x2D, 0xF0,    // oftwa
0x2D, 0x24, 0xF0, 0x24, 0x29, 0xF0, 0x29, 0x3A, 0xF0, 0x3A, 0x1C, 0xF0, 0x1C, 0x35, 0xF0, 0x35,    // re ma
0x29, 0xF0, 0x29, 0x31, 0xF0, 0x31, 0x44, 0xF0, 0x44, 0x2C, 0xF0, 0x2C, 0x5A, 0xF0, 0x5A, 0x12,    // y not.
0x2D, 0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x32, 0xF0,    // REM 
0x32, 0x24, 0xF0, 0x24, 0x29, 0xF0, 0x29, 0x2D, 0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x4E, 0xF0, 0x4E,    // be re
0x23, 0xF0, 0x23, 0x43, 0xF0, 0x43, 0x1B, 0xF0, 0x1B, 0x2C, 0xF0, 0x2C, 0x2D, 0xF0, 0x2D, 0x43,    // -distr
0xF0, 0x43, 0x32, 0xF0, 0x32, 0x3C, 0xF0, 0x3C, 0x2C, 0xF0, 0x2C, 0x24, 0xF0, 0x24, 0x23, 0xF0,    // ibute
0x23, 0x29, 0xF0, 0x29, 0x44, 0xF0, 0x44, 0x2D, 0xF0, 0x2D, 0x5A, 0xF0, 0x5A, 0x12, 0x2D, 0xF0,    // d or.
0x2D, 0x24, 0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x1B, 0xF0, 0x1B, 0x44,    // REM s
0xF0, 0x44, 0x4B, 0xF0, 0x4B, 0x23, 0xF0, 0x23, 0x29, 0xF0, 0x29, 0x1D, 0xF0, 0x1D, 0x43, 0xF0,    // old w
0x43, 0x2C, 0xF0, 0x2C, 0x33, 0xF0, 0x33, 0x44, 0xF0, 0x44, 0x3C, 0xF0, 0x3C, 0x2C, 0xF0, 0x2C,    // ithou
0x29, 0xF0, 0x29, 0x1D, 0xF0, 0x1D, 0x2D, 0xF0, 0x2D, 0x43, 0xF0, 0x43, 0x2C, 0xF0, 0x2C, 0x2C,    // t writ
0xF0, 0x2C, 0x24, 0xF0, 0x24, 0x31, 0xF0, 0x31, 0x5A, 0xF0, 0x5A, 0x12, 0x2D, 0xF0, 0x2D, 0x24,    // ten.R
0xF0, 0x24, 0x3A, 0xF0, 0x3A, 0xF0, 0x12, 0x29, 0xF0, 0x29, 0x4D, 0xF0, 0x4D, 0x24, 0xF0, 0x24,    // EM p
0x2D, 0xF0, 0x2D, 0x3A, 0xF0, 0x3A, 0x43, 0xF0, 0x43, 0x1B, 0xF0, 0x1B, 0x1B, 0xF0, 0x1B, 0x43,    // ermiss
0xF0, 0x43, 0x44, 0xF0, 0x44, 0x31, 0xF0, 0x31, 0x29, 0xF0, 0x29, 0x2B, 0xF0, 0x2B, 0x2D, 0xF0,    // ion f
0x2D, 0x44, 0xF0, 0x44, 0x3A, 0xF0, 0x3A, 0x29, 0xF0, 0x29, 0x2C, 0xF0, 0x2C, 0x33, 0xF0, 0x33,    // rom t
0x24, 0xF0, 0x24, 0x5A, 0xF0, 0x5A, 0x12, 0x2D, 0xF0, 0x2D, 0x24, 0xF0, 0x24, 0x3A, 0xF0, 0x3A,    // he.RE
0xF0, 0x12, 0x29, 0xF0, 0x29, 0x1C, 0xF0, 0x1C, 0x3C, 0xF0, 0x3C, 0x2C, 0xF0, 0x2C, 0x33, 0xF0,    // M aut
0x33, 0x44, 0xF0, 0x44, 0x2D, 0xF0, 0x2D, 0x49, 0xF0, 0x49, 0x29, 0xF0, 0x29, 0x29, 0xF0, 0x29,    // hor. 
0x12, 0x2A, 0xF0, 0x2A, 0xF0, 0x12, 0x69, 0xF0, 0x69, 0x49, 0xF0, 0x49, 0x6B, 0xF0, 0x6B, 0x5A,    //  V1.4
0xF0, 0x5A, 0x00            																	   // .
};

// Windows code table - VK to US Set2 Scancodes. Hopefully this
// gives us keyboard layout independence :)
// NOTE: it does not. Damn it.
unsigned char VK2ScanCode[256] = {
// From WinUser.h
// Scancodes from http://www.computer-engineering.org/ps2keyboard/scancodes2.html
0,		// unassigned - do not use
0,		//#define VK_LBUTTON        0x01
0,		//#define VK_RBUTTON        0x02
0,		//#define VK_CANCEL         0x03
0,		//#define VK_MBUTTON        0x04    /* NOT contiguous with L & RBUTTON */
0,		//#define VK_XBUTTON1       0x05    /* NOT contiguous with L & RBUTTON */
0,		//#define VK_XBUTTON2       0x06    /* NOT contiguous with L & RBUTTON */
0,								//* 0x07 : unassigned
0x66,	//#define VK_BACK           0x08
0x0d,	//#define VK_TAB            0x09
0,								//* 0x0A - 0x0B : reserved
0,
0,		//#define VK_CLEAR          0x0C
0x5a,	//#define VK_RETURN         0x0D
0,		//							0x0E
0,		//							0x0F
0,		//#define VK_SHIFT          0x10
0,		//#define VK_CONTROL        0x11
0,		//#define VK_MENU           0x12
0,		//#define VK_PAUSE          0x13
0,		//#define VK_CAPITAL        0x14 (caps lock)
0,		//#define VK_KANA           0x15
0,		//							0x16
0,		//#define VK_JUNJA          0x17
0,		//#define VK_FINAL          0x18
0,		//#define VK_HANJA          0x19
0,		//							0x1a
0x76,	//#define VK_ESCAPE         0x1B
0,		//#define VK_CONVERT        0x1C
0,		//#define VK_NONCONVERT     0x1D
0,		//#define VK_ACCEPT         0x1E
0,		//#define VK_MODECHANGE     0x1F
0x29,	//#define VK_SPACE          0x20
0x7d,	//#define VK_PRIOR          0x21
0x7a,	//#define VK_NEXT           0x22
0x69,	//#define VK_END            0x23
0x6c,	//#define VK_HOME           0x24
0x6b,	//#define VK_LEFT           0x25
0x75,	//#define VK_UP             0x26
0x74,	//#define VK_RIGHT          0x27
0x72,	//#define VK_DOWN           0x28
0,		//#define VK_SELECT         0x29
0,		//#define VK_PRINT          0x2A
0,		//#define VK_EXECUTE        0x2B
0,		//#define VK_SNAPSHOT       0x2C
0x70,	//#define VK_INSERT         0x2D
0x71,	//#define VK_DELETE         0x2E
0,		//#define VK_HELP           0x2F
// * VK_0 - VK_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
0x45,	// 0
0x16,	// 1
0x1e,	// 2
0x26,	// 3
0x25,	// 4
0x2e,	// 5
0x36,	// 6
0x3d,	// 7
0x3e,	// 8
0x46,	// 9
0,		// 0x3a-0x3f?
0,
0,
0,
0,
0,
0,		// * 0x40 : unassigned
// * VK_A - VK_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
0x1c,	// A
0x32,	// B
0x21,	// C
0x23,	// D
0x24,	// E
0x2b,	// F
0x34,	// G
0x33,	// H
0x43,	// I
0x3b,	// J
0x42,	// K
0x4b,	// L
0x3a,	// M
0x31,	// N
0x44,	// O
0x4d,	// P
0x15,	// Q
0x2d,	// R
0x1b,	// S
0x2c,	// T
0x3c,	// U
0x2a,	// V
0x1d,	// W
0x22,	// X
0x35,	// Y
0x1a,	// Z
0x1f,	//#define VK_LWIN           0x5B
0x27,	//#define VK_RWIN           0x5C
0x2f,	//#define VK_APPS           0x5D
0,		//						  * 0x5E : reserved
0,		//#define VK_SLEEP          0x5F
0x70,	//#define VK_NUMPAD0        0x60
0x69,	//#define VK_NUMPAD1        0x61
0x72,	//#define VK_NUMPAD2        0x62
0x7a,	//#define VK_NUMPAD3        0x63
0x6b,	//#define VK_NUMPAD4        0x64
0x73,	//#define VK_NUMPAD5        0x65
0x74,	//#define VK_NUMPAD6        0x66
0x6c,	//#define VK_NUMPAD7        0x67
0x75,	//#define VK_NUMPAD8        0x68
0x7d,	//#define VK_NUMPAD9        0x69
0x7c,	//#define VK_MULTIPLY       0x6A
0x79,	//#define VK_ADD            0x6B
0,		//#define VK_SEPARATOR      0x6C
0x7b,	//#define VK_SUBTRACT       0x6D
0x71,	//#define VK_DECIMAL        0x6E
0x4a,	//#define VK_DIVIDE         0x6F
0x05,	//#define VK_F1             0x70
0x06,	//#define VK_F2             0x71
0x04,	//#define VK_F3             0x72
0x0c,	//#define VK_F4             0x73
0x03,	//#define VK_F5             0x74
0x0b,	//#define VK_F6             0x75
0x83,	//#define VK_F7             0x76
0x0a,	//#define VK_F8             0x77
0x01,	//#define VK_F9             0x78
0x09,	//#define VK_F10            0x79
0x78,	//#define VK_F11            0x7A
0x07,	//#define VK_F12            0x7B
0,		//#define VK_F13            0x7C
0,		//#define VK_F14            0x7D
0,		//#define VK_F15            0x7E
0,		//#define VK_F16            0x7F
0,		//#define VK_F17            0x80
0,		//#define VK_F18            0x81
0,		//#define VK_F19            0x82
0,		//#define VK_F20            0x83
0,		//#define VK_F21            0x84
0,		//#define VK_F22            0x85
0,		//#define VK_F23            0x86
0,		//#define VK_F24            0x87
0,		// * 0x88 - 0x8F : unassigned
0,
0,
0,
0,
0,
0,
0,
0,		//#define VK_NUMLOCK        0x90
0,		//#define VK_SCROLL         0x91
0,		//#define VK_OEM_NEC_EQUAL  0x92   // '=' key on numpad
0,		//#define VK_OEM_FJ_MASSHOU 0x93   // 'Unregister word' key
0,		//#define VK_OEM_FJ_TOUROKU 0x94   // 'Register word' key
0,		//#define VK_OEM_FJ_LOYA    0x95   // 'Left OYAYUBI' key
0,		//#define VK_OEM_FJ_ROYA    0x96   // 'Right OYAYUBI' key
0,		// * 0x97 - 0x9F : unassigned
0,
0,
0,
0,
0,
0,
0,
0,
0,		//#define VK_LSHIFT         0xA0
0,		//#define VK_RSHIFT         0xA1
0,		//#define VK_LCONTROL       0xA2
0,		//#define VK_RCONTROL       0xA3
0,		//#define VK_LMENU          0xA4
0,		//#define VK_RMENU          0xA5
0,		//#define VK_BROWSER_BACK        0xA6
0,		//#define VK_BROWSER_FORWARD     0xA7
0,		//#define VK_BROWSER_REFRESH     0xA8
0,		//#define VK_BROWSER_STOP        0xA9
0,		//#define VK_BROWSER_SEARCH      0xAA
0,		//#define VK_BROWSER_FAVORITES   0xAB
0,		//#define VK_BROWSER_HOME        0xAC
0,		//#define VK_VOLUME_MUTE         0xAD
0,		//#define VK_VOLUME_DOWN         0xAE
0,		//#define VK_VOLUME_UP           0xAF
0,		//#define VK_MEDIA_NEXT_TRACK    0xB0
0,		//#define VK_MEDIA_PREV_TRACK    0xB1
0,		//#define VK_MEDIA_STOP          0xB2
0,		//#define VK_MEDIA_PLAY_PAUSE    0xB3
0,		//#define VK_LAUNCH_MAIL         0xB4
0,		//#define VK_LAUNCH_MEDIA_SELECT 0xB5
0,		//#define VK_LAUNCH_APP1         0xB6
0,		//#define VK_LAUNCH_APP2         0xB7
0,		// * 0xB8 - 0xB9 : reserved
0,
0x4c,	//#define VK_OEM_1          0xBA   // ';:' for US
0x55,	//#define VK_OEM_PLUS       0xBB   // '+' any country
0x41,	//#define VK_OEM_COMMA      0xBC   // ',' any country
0x4e,	//#define VK_OEM_MINUS      0xBD   // '-' any country
0x49,	//#define VK_OEM_PERIOD     0xBE   // '.' any country
0x4a,	//#define VK_OEM_2          0xBF   // '/?' for US
0x0e,	//#define VK_OEM_3          0xC0   // '`~' for US
0,		// * 0xC1 - 0xD7 : reserved
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,		// * 0xD8 - 0xDA : unassigned
0,
0,
0x54,	//#define VK_OEM_4          0xDB  //  '[{' for US
0x5d,	//#define VK_OEM_5          0xDC  //  '\|' for US
0x5b,	//#define VK_OEM_6          0xDD  //  ']}' for US
0x52,	//#define VK_OEM_7          0xDE  //  ''"' for US
0,		//#define VK_OEM_8          0xDF
0,								//* 0xE0 : reserved (do not use)
0,		//#define VK_OEM_AX         0xE1  //  'AX' key on Japanese AX kbd
0,		//#define VK_OEM_102        0xE2  //  "<>" or "\|" on RT 102-key kbd.
0,		//#define VK_ICO_HELP       0xE3  //  Help key on ICO
0,		//#define VK_ICO_00         0xE4  //  00 key on ICO
0,		//#define VK_PROCESSKEY     0xE5
0,		//#define VK_ICO_CLEAR      0xE6
0,		//#define VK_PACKET         0xE7
0,								//* 0xE8 : unassigned
0,		//#define VK_OEM_RESET      0xE9
0,		//#define VK_OEM_JUMP       0xEA
0,		//#define VK_OEM_PA1        0xEB
0,		//#define VK_OEM_PA2        0xEC
0,		//#define VK_OEM_PA3        0xED
0,		//#define VK_OEM_WSCTRL     0xEE
0,		//#define VK_OEM_CUSEL      0xEF
0,		//#define VK_OEM_ATTN       0xF0	(do not use)
0,		//#define VK_OEM_FINISH     0xF1
0,		//#define VK_OEM_COPY       0xF2
0,		//#define VK_OEM_AUTO       0xF3
0,		//#define VK_OEM_ENLW       0xF4
0,		//#define VK_OEM_BACKTAB    0xF5
0,		//#define VK_ATTN           0xF6
0,		//#define VK_CRSEL          0xF7
0,		//#define VK_EXSEL          0xF8
0,		//#define VK_EREOF          0xF9
0,		//#define VK_PLAY           0xFA
0,		//#define VK_ZOOM           0xFB
0,		//#define VK_NONAME         0xFC
0,		//#define VK_PA1            0xFD
0,		//#define VK_OEM_CLEAR      0xFE
0								//* 0xFF : reserved
};
