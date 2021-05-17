
/*****************************************************************************
 *
 *	 9900dasm.c
 *	 TMS 9900 disassembler
 *
 *	 Copyright (c) 1998 John Butler, all rights reserved.
 *	 Based on 6502dasm.c 6502/65c02/6510 disassembler by Juergen Buchmueller
 *
 *	 - This source code is released as freeware for non-commercial purposes.
 *	 - You are free to use and redistribute this code in modified or
 *	   unmodified form, provided you list me in the credits.
 *	 - If you modify this source code, you must add a notice to each modified
 *	   source file that it has been changed.  If you're a nice person, you
 *	   will clearly mark each change too.  :)
 *	 - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *****************************************************************************/

// Modified by Tursi for Classic99 
#include <stdio.h>
#include <windows.h>

#include "tiemul.h"
#include "cpu9900.h"
#include "..\addons\ams.h"

// config value for the Classic99 debug opcodes
extern int enableDebugOpcodes;

// Although no longer used by the disassembler -- the debugger still uses these "safe" functions.
Word GetSafeCpuWord(int x, int bank) {
	x&=0xfffe;
	return (GetSafeCpuByte(x, bank)<<8)|GetSafeCpuByte(x+1, bank);
}

// Read a byte withOUT triggering the hardware - for monitoring
// make sure bank never exceeds xb
Byte GetSafeCpuByte(int x, int bank) {
	x&=0xffff;
	switch (x & 0xe000) {
	case 0x8000:
		if ((x & 0xfc00)==0x8000) {						// scratchpad RAM - 256 bytes repeating.
			return ReadMemoryByte(x|0x0300, ACCESS_FREE);		// I map it all to >83xx
		}
		break;
	
	case 0x4000:										// DSR ROM (with bank switching and CRU)
		if (ROMMAP[x]) {
			return ReadMemoryByte(x, ACCESS_FREE);
		}
		
		if (-1 == nCurrentDSR) {
			return 0;
		}
		
		if (nDSRBank[nCurrentDSR]) {
			return DSR[nCurrentDSR][x-0x2000];			// -8k for base, +4k for second page
		} else {
			return DSR[nCurrentDSR][x-0x4000];
		}
		break;

	case 0x6000:										// cartridge ROM
		// XB is supposed to only page the upper 4k, but some Atari carts seem to like it all
		// paged. Most XB dumps take this into account so only full 8k paging is implemented.
		if (xb) {
            if (bank == -1) {
                // this is hacky... -1 means GPU. So a bug somewhere... (TODO)
                bank=0;
            }
			return(CPU2[(bank<<13)+(x-0x6000)]);		// cartridge bank 2
		} else {
			return ReadMemoryByte(x, ACCESS_FREE);			// cartridge bank 1
		}
		break;
	}

	return ReadMemoryByte(x, ACCESS_FREE);
}

extern CPU9900 * volatile pCurrentCPU;
extern CPU9900 *pGPU, *pCPU;
#define RDOP(A, bank) ((bank) == -1 ? pGPU->GetSafeWord(A, bank) : pCPU->GetSafeWord(A, bank) )
#define RDWORD(A, bank) ((bank) == -1 ? pGPU->GetSafeWord(A, bank) : pCPU->GetSafeWord(A, bank) )

#define BITS_0to3	((OP>>12) & 0xf)
#define BITS_2to5	((OP>>10) & 0xf)
#define BITS_5to9	((OP>>6) & 0x1f)
#define BITS_3to7	((OP>>8) & 0x1f)
#define BITS_6to10	((OP>>5) & 0x1f)

#define BITS_0to1	((OP>>14) & 0x3)
#define BITS_0to4	((OP>>11) & 0x1f)
#define BITS_0to2	((OP>>13) & 0x7)
#define BITS_0to5	((OP>>10) & 0x3f)

#define	MASK	0x0000ffff
#define OPBITS(n1,n2)	((OP>>(15-(n2))) & (MASK>>(15-((n2)-(n1)))))

enum opcodes {
	_a=0,	_ab,	_c,		_cb,	_s,		_sb,	_soc,	_socb,	_szc,	_szcb,
	_mov,	_movb,	_coc,	_czc,	_xor,	_mpy,	_div,	_xop,	_b,		_bl,
	_blwp,	_clr,	_seto,	_inv,	_neg,	_abs,	_swpb,	_inc,	_inct,	_dec,
	_dect,	_x,		_ldcr,	_stcr,	_sbo,	_sbz,	_tb,	_jeq,	_jgt,	_jh,
	_jhe,	_jl,	_jle,	_jlt,	_jmp,	_jnc,	_jne,	_jno,	_joc,	_jop,
	_sla,	_sra,	_src,	_srl,	_ai,	_andi,	_ci,	_li,	_ori,	_lwpi,
	_limi,	_stst,	_stwp,	_rtwp,	_idle,	_rset,	_ckof,	_ckon,	_lrex,	_ill,
	// additional opcodes for the F18A (mb)
	_spi_en, _spi_ds, _spi_out, _spi_in, _pix,	_call,  _ret,   _push,  _pop,   _slc,
	// debugger opcodes
	_c99_norm, _c99_ovrd, _c99_smax, _c99_brk, _c99_quit, _c99_dbg,
};


static const char *token[]=
{
	"a",	"ab",	"c",	"cb",	"s",	"sb",	"soc",	"socb",	"szc",	"szcb",
	"mov",	"movb",	"coc",	"czc",	"xor",	"mpy",	"div",	"xop",	"b",	"bl",
	"blwp",	"clr",	"seto",	"inv",	"neg",	"abs",	"swpb",	"inc",	"inct",	"dec",
	"dect",	"x",	"ldcr",	"stcr",	"sbo",	"sbz",	"tb",	"jeq",	"jgt",	"jh",
	"jhe",	"jl",	"jle",	"jlt",	"jmp",	"jnc",	"jne",	"jno",	"joc",	"jop",
	"sla",	"sra",	"src",	"srl",	"ai",	"andi",	"ci",	"li",	"ori",	"lwpi",
	"limi",	"stst",	"stwp",	"rtwp",	"idle",	"rset",	"ckof",	"ckon",	"lrex", "ill",
	// F18A
	"spie", "spid", "spio", "spii", "pix",  "call", "ret",  "push", "pop",  "slc",
	"c99norm","c99ovrd","c99smax","c99brk","c99dbg"
};


// 9900 tables
static const enum opcodes ops0to3[64]=	// only 16 entries, padded to 32 each
{
	_ill,	_ill,	_ill,	_ill,	_szc,	_szcb,	_s,		_sb,	/*0000-0111*/
	_c,		_cb,	_a,		_ab,	_mov,	_movb,	_soc,	_socb,	/*1000-1111*/
	_ill,	_ill,	_ill,	_ill,	_szc,	_szcb,	_s,		_sb,	/*0000-0111*/
	_c,		_cb,	_a,		_ab,	_mov,	_movb,	_soc,	_socb,	/*1000-1111*/

	// F18A
	_ill,	_ill,	_ill,	_ill,	_szc,	_szcb,	_s,		_sb,	/*0000-0111*/
	_c,		_cb,	_a,		_ab,	_mov,	_movb,	_soc,	_socb,	/*1000-1111*/
	_ill,	_ill,	_ill,	_ill,	_szc,	_szcb,	_s,		_sb,	/*0000-0111*/
	_c,		_cb,	_a,		_ab,	_mov,	_movb,	_soc,	_socb	/*1000-1111*/

};


static const enum opcodes ops2to5[64]=	// only 16 entries, padded to 32 each
{
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*0000-0111*/
	_coc,	_czc,	_xor,	_xop,	_ldcr,	_stcr,	_mpy,	_div,	/*1000-1111*/
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*0000-0111*/
	_coc,	_czc,	_xor,	_xop,	_ldcr,	_stcr,	_mpy,	_div,	/*1000-1111*/

	// F18A
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*0000-0111*/
	_coc,	_czc,	_xor,	_pix,	_spi_out,_spi_in,_mpy,	_div,	/*1000-1111*/
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*0000-0111*/
	_coc,	_czc,	_xor,	_pix,	_spi_out,_spi_in,_mpy,	_div	/*1000-1111*/
};


static const enum opcodes ops5to9[64]=
{
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*00000-00111*/
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*01000-01111*/
	_blwp,	_b,		_x,		_clr,	_neg,	_inv,	_inc,	_inct,	/*10000-10111*/
	_dec,	_dect,	_bl,	_swpb,	_seto,	_abs,	_ill,	_ill,	/*11000-11111*/

	// F18A
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*00000-00111*/
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*01000-01111*/
	_ill,	_b,		_x,		_clr,	_neg,	_inv,	_inc,	_inct,	/*10000-10111*/
	_dec,	_dect,	_bl,	_swpb,	_seto,	_abs,	_ill,	_ill	/*11000-11111*/
};


static const enum opcodes ops3to7[64]=
{
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*00000-00111*/
	_sra,	_srl,	_sla,	_src,	_ill,	_ill,	_ill,	_ill,	/*01000-01111*/
	_jmp,	_jlt,	_jle,	_jeq,	_jhe,	_jgt,	_jne,	_jnc,	/*10000-10111*/
	_joc,	_jno,	_jl,	_jh,	_jop,	_sbo,	_sbz,	_tb	,	/*11000-11111*/

	// F18A
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*00000-00111*/
	_sra,	_srl,	_sla,	_src,	_call,	_push,	_slc,	_pop,	/*01000-01111*/	// note: _call and ret vary in bit 8! (call is 1)
	_jmp,	_jlt,	_jle,	_jeq,	_jhe,	_jgt,	_jne,	_jnc,	/*10000-10111*/
	_joc,	_jno,	_jl,	_jh,	_jop,	_ill,	_ill,	_ill	/*11000-11111*/
};


static const enum opcodes ops6to10[64]=
{
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*00000-00111*/
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*01000-01111*/
	_li,	_ai,	_andi,	_ori,	_ci,	_stwp,	_stst,	_lwpi,	/*10000-10111*/
	_limi,	_ill,	_idle,	_rset,	_rtwp,	_ckon,	_ckof,	_lrex,	/*11000-11111*/

	// F18A
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*00000-00111*/
	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	_ill,	/*01000-01111*/
	_li,	_ai,	_andi,	_ori,	_ci,	_ill,	_stst,	_ill,	/*10000-10111*/
	_ill,	_ill,	_idle,	_ill,	_rtwp,	_spi_en,_spi_ds,_ill	/*11000-11111*/
};


static int myPC;

static char *print_arg (int mode, int arg, int bank)
{
	static char temp[20];
	int	base;

	switch (mode)
	{
		case 0x0:	/* workspace register */
			sprintf (temp, "R%d", arg);
			break;
		case 0x1:	/* workspace register indirect */
			sprintf (temp, "*R%d", arg);
			break;
		case 0x2:	/* symbolic|indexed */
			base = RDWORD(myPC, bank); myPC+=2;
			if (arg) 	/* indexed */
				sprintf (temp, "@>%04x(R%d)", base, arg);
			else		/* symbolic (direct) */
				sprintf (temp, "@>%04x", base);
			break;
		case 0x3:	/* workspace register indirect auto increment */
			sprintf (temp, "*R%d+", arg);
			break;
	}
	return temp;
}


/*****************************************************************************
 *	Disassemble a single command and return the number of bytes it uses.
 *****************************************************************************/
int Dasm9900 (char *buffer, int pc, int bank)
{
	int	OP, opc;
	int sarg, darg, smode, dmode;
	int offset;

	myPC = pc;
	OP = RDOP(myPC, bank); myPC+=2;

	if (pCurrentCPU == pGPU) {
		offset=32;
	} else {
		offset=0;
	}

	// classic99 opcodes have no addressing modes per sae
	if ((enableDebugOpcodes) && (OP >= 0x0110) && (OP <= 0x0114)) {
		// debugger opcode
		switch (OP) {
			case 0x0110: sprintf(buffer, "c99_norm"); break;
			case 0x0111: sprintf(buffer, "c99_ovrd"); break;
			case 0x0112: sprintf(buffer, "c99_smax"); break;
			case 0x0113: sprintf(buffer, "c99_brk");  break;
			case 0x0114: sprintf(buffer, "c99_quit");  break;
			case 0x0120:
			case 0x0121:
			case 0x0122:
			case 0x0123:
			case 0x0124:
			case 0x0125:
			case 0x0126:
			case 0x0127:
			case 0x0128:
			case 0x0129:
			case 0x012a:
			case 0x012b:
			case 0x012c:
			case 0x012d:
			case 0x012e:
			case 0x012f:
			{
				// dbg sequence is 012r,1001,adr
				int jmp = RDWORD(myPC, bank); myPC += 2;
				int adr = RDWORD(myPC, bank); myPC += 2;
				sprintf(buffer, "c99_dbg(%d) >%04x,R%d", jmp - 0x1000, adr, OP&0xf);
			}
			break;
		}
	} else if ((opc = ops0to3[BITS_0to3+offset]) != _ill)
	{
		smode = OPBITS(10,11);
		sarg = OPBITS(12,15);
		dmode = OPBITS(4,5);
		darg = OPBITS(6,9);

 		sprintf (buffer, "%-4s ", token[opc]);
		strcat (buffer, print_arg (smode, sarg, bank));
		strcat (buffer, ",");
		strcat (buffer, print_arg (dmode, darg, bank));
	}
	else if (BITS_0to1==0 && (opc = ops2to5[BITS_2to5+offset]) != _ill)
	{
		smode = OPBITS(10,11);
		sarg = OPBITS(12,15);
		darg = OPBITS(6,9);

		if (darg==0 && (opc==_ldcr || opc==_stcr))
			darg = 16;

		if (opc==_xop || opc==_ldcr || opc==_stcr)
			sprintf (buffer, "%-4s %s,%d", token[opc], print_arg (smode, sarg, bank), darg);
		else	/* _coc, _czc, _xor, _mpy, _div */
			sprintf (buffer, "%-4s %s,R%d", token[opc], print_arg (smode, sarg, bank), darg);
	}
	else if (BITS_0to2==0 && (opc = ops3to7[BITS_3to7+offset]) != _ill)
	{
		switch (opc)
		{
			case _sra: case _srl: case _sla: case _src:
				sarg = OPBITS(12,15);
				darg = OPBITS(8,11);

				sprintf (buffer, "%-4s R%d,%d", token[opc], sarg, darg);
				break;
			case _jmp: case _jlt: case _jle: case _jeq: case _jhe: case _jgt:
			case _jne: case _jnc: case _joc: case _jno: case _jl:  case _jh: case _jop:
				{
					signed char displacement;

					displacement = (signed char)OPBITS(8,15);
					sprintf (buffer, "%-4s >%04x", token[opc], 0xffff & (myPC + displacement * 2));
				}
				break;
			case _sbo: case _sbz: case _tb:
				{
					signed char displacement;

					displacement = (signed char)OPBITS(8,15);
					sprintf (buffer, "%-4s >%04x", token[opc], 0xffff & displacement);
				}
				break;
				
			case _call:	case _push: case _pop: case _slc:
				// todo: is this right? you can pop to any memory address? 
				// check if it's really ret
				if ((opc==_call) && ((OP&0x0080)==0)) {
					// it's actually RET (no args)
					opc=_ret;
					sprintf(buffer, "%-4s", token[opc]);
				} else {
					// it's CALL or one of the others (formatted like B)
					smode = OPBITS(10,11);
					sarg = OPBITS(12,15);
					sprintf (buffer, "%-4s %s", token[opc], print_arg (smode, sarg, bank));
				}
				break;
		}
	}
	else if (BITS_0to4==0 && (opc = ops5to9[BITS_5to9+offset]) != _ill)
	{
		smode = OPBITS(10,11);
		sarg = OPBITS(12,15);

		sprintf (buffer, "%-4s %s", token[opc], print_arg (smode, sarg, bank));
	}
	else if (BITS_0to5==0 && (opc = ops6to10[BITS_6to10+offset]) != _ill)
	{
		switch (opc)
		{
			case _li:   case _ai:   case _andi: case _ori:  case _ci:
				darg = OPBITS(12,15);
				sarg = RDWORD(myPC, bank); myPC+=2;

				sprintf (buffer, "%-4s R%d,>%04x", token[opc], darg, sarg);
				break;
			case _lwpi: case _limi:
				sarg = RDWORD(myPC, bank); myPC+=2;

				sprintf (buffer, "%-4s >%04x", token[opc], sarg);
				break;
			case _stwp: case _stst:
				sarg = OPBITS(12,15);

				sprintf (buffer, "%-4s R%d", token[opc], sarg);
				break;
			case _idle: case _rset: case _rtwp: case _ckon: case _ckof: case _lrex:
				sprintf (buffer, "%-4s", token[opc]);
				break;
		}
	}
	else
		sprintf (buffer, "data >%04x", OP);

	return myPC - pc;
}
