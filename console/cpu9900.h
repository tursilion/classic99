//
// (C) 2012 Mike Brent aka Tursi aka HarmlessLion.com
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
/////////////////////////////////////////////////////////////////////
// Classic99 - TMS9900 CPU Routines
// M.Brent
// The TMS9900 is a 16-bit CPU by Texas Instruments, with a 16
// bit data and 16-bit address path, capable of addressing
// 64k of memory. All reads and writes are word (16-bit) oriented.
// Byte access is simulated within the CPU by reading or writing
// the entire word, and manipulating only the requested byte.
// This is not currently emulated here. The CPU uses external
// RAM for all user registers. There are 16 user registers, R0-R15,
// and the memory base for these registers may be anywhere in
// memory, set by the Workspace Pointer. The CPU also has a Program
// Counter and STatus register internal to it.
// This emulation generates a lookup table of addresses for each
// opcode. It's not currently intended for use outside of Classic99
// and there may be problems with dependancy on other parts of the
// code if it is moved to another project.
// Word is defined to be an unsigned 16-bit integer (__int16)
// Byte is defined to be an unsigned 8-bit integer (__int8)
/////////////////////////////////////////////////////////////////////

// Like the FAQ says, be nice to people!
class CPU9900;
typedef void (CPU9900::*CPU990Fctn)(void);			// now function pointers are just "CPU9900Fctn" type
#define CALL_MEMBER_FN(object, ptr) ((object)->*(ptr))

// we need more than one of these now - time for a class
class CPU9900 {
public:		// type protection later. Make work today.
	// CPU variables
	Word PC;											// Program Counter
	Word WP;											// Workspace Pointer
	Word X_flag;										// Set during an 'X' instruction, 0 if not active, else address of PC after the X (ignoring arguments if any)
	Word ST;											// Status register
	Word in,D,S,Td,Ts,B;								// Opcode interpretation
	int nCycleCount;									// Used in CPU throttle
	Byte nPostInc[2];									// Register number to increment, ORd with 0x80 for 2, or 0x40 for 1
	const char *pType;

	CPU990Fctn opcode[65536];							// CPU Opcode address table

	int idling;											// set when an IDLE occurs
	int halted;											// set when the CPU is halted by external hardware (in this emulation, we spin NOPs)
	int nReturnAddress;									// return address for step over
	bool enableDebug;									// whether enabling debug on this CPU
//	volatile signed long cycles_left;					// cycles_left on this CPU
//	int max_cpf, cfg_cpf;								// performance on this CPU

	CPU9900();
	virtual void reset();

	/////////////////////////////////////////////////////////////////////
	// Wrapper functions for memory access
	/////////////////////////////////////////////////////////////////////
	virtual Byte RCPUBYTE(Word src);
	virtual void WCPUBYTE(Word dest, Byte c);
	virtual Word ROMWORD(Word src, READACCESSTYPE rmw);
	virtual void WRWORD(Word dest, Word val);

	virtual Word GetSafeWord(int x, int bank);
	virtual Byte GetSafeByte(int x, int bank);

	virtual	void TriggerInterrupt(Word vector, Byte level);

	void post_inc(int nWhich);

	//////////////////////////////////////////////////////////////////////////
	// Get addresses for the destination and source arguments
	// Note: the format code letters are the official notation from Texas
	// instruments. See their TMS9900 documentation for details.
	// (Td, Ts, D, S, B, etc)
	// Note that some format codes set the destination type (Td) to
	// '4' in order to skip unneeded processing of the Destination address
	//////////////////////////////////////////////////////////////////////////
	void fixS();
	void fixD();

	/////////////////////////////////////////////////////////////////////////
	// Check parity in the passed byte and set the OP status bit
	/////////////////////////////////////////////////////////////////////////
	void parity(Byte x);

	// Helpers for what used to be global variables
	void StartIdle();
	void StopIdle();
	int  GetIdle();
	void StartHalt(int source);
	void StopHalt(int source);
	int  GetHalt();
	void SetReturnAddress(Word x);
	int GetReturnAddress();
	void ResetCycleCount();
	void AddCycleCount(int val);
	int  GetCycleCount();
	void SetCycleCount(int x);
	Word GetPC();
	void SetPC(Word x);
	Word GetST();
	void SetST(Word x);
	Word GetWP();
	void SetWP(Word x);
	Word GetX();
	void SetX(Word x);
	Word ExecuteOpcode(bool nopFrame);

	////////////////////////////////////////////////////////////////////
	// Classic99 - 9900 CPU opcodes
	// Opcode functions follow
	// one function for each opcode (the mneumonic prefixed with "op_")
	// src - source address (register or memory - valid types vary)
	// dst - destination address
	// imm - immediate value
	// dsp - relative displacement
	////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// DO NOT USE wcpubyte or rcpubyte in here! You'll break the RMW
	// emulation and the cycle counting! The 9900 can only do word access.
	/////////////////////////////////////////////////////////////////////
	void op_a();
	void op_ab();
	void op_abs();
	void op_ai();
	void op_dec();
	void op_dect();
	void op_div();
	void op_inc();
	void op_inct();
	void op_mpy();
	void op_neg();
	void op_s();
	void op_sb();
	void op_b();
	void op_bl();
	void op_blwp();
	void op_jeq();
	void op_jgt();
	void op_jhe();
	void op_jh();
	void op_jl();
	void op_jle();
	void op_jlt();
	void op_jmp();
	void op_jnc();
	void op_jne();
	void op_jno();
	void op_jop();
	void op_joc();
	void op_rtwp();
	void op_x();
	void op_xop();
	void op_c();
	void op_cb();
	void op_ci();
	void op_coc();
	void op_czc();
	void op_ldcr();
	void op_sbo();
	void op_sbz();
	void op_stcr();
	void op_tb();

	// These instructions are valid 9900 instructions but are invalid on the TI-99, as they generate
	// improperly decoded CRU instructions.
	void op_ckof();
	void op_ckon();
	void op_idle();
	void op_rset();
	void op_lrex();

	// back to legal instructions
	void op_li();
	void op_limi();
	void op_lwpi();
	void op_mov();
	void op_movb();
	void op_stst();
	void op_stwp();
	void op_swpb();
	void op_andi();
	void op_ori();
	void op_xor();
	void op_inv();
	void op_clr();
	void op_seto();
	void op_soc();
	void op_socb();
	void op_szc();
	void op_szcb();
	void op_sra();
	void op_srl();
	void op_sla();
	void op_src();

	// not an opcode - illegal opcode handler
	void op_bad();

	// debug only opcodes, enabled with enableDebugOpcodes
	void op_norm();
	void op_ovrd();
	void op_smax();
	void op_brk();
	void op_dbg();
	void op_quit();

	// F18 specific versions of opcodes (here to make the function pointers work better)
	void op_idleF18();
	void op_callF18();
	void op_retF18();
	void op_pushF18();
	void op_popF18();
	void op_slcF18();
	void op_pixF18();
	void op_csonF18();
	void op_csoffF18();
	void op_spioutF18();
	void op_spiinF18();
	void op_rtwpF18();

	////////////////////////////////////////////////////////////////////////
	// Fill the CPU Opcode Address table
	////////////////////////////////////////////////////////////////////////
	virtual void buildcpu();
	void opcode0(Word in);
	void opcode02(Word in);
	void opcode03(Word in);
	void opcode04(Word in);
	void opcode05(Word in);
	void opcode06(Word in);
	void opcode07(Word in);
	void opcode1(Word in);
	void opcode2(Word in);
	void opcode3(Word in);

};

// a few overrides for the F18A GPU
// Note that F18A specific versions of opcodes are up in the CPU9900 class
// for function pointer reasons.
class GPUF18A:public CPU9900 {
public:		// type protection later. Make work today.
	GPUF18A();
	void reset() override;
	void buildcpu() override;

	/////////////////////////////////////////////////////////////////////
	// Wrapper functions for memory access
	/////////////////////////////////////////////////////////////////////
	Byte RCPUBYTE(Word src) override;
	void WCPUBYTE(Word dest, Byte c) override;
	Word ROMWORD(Word src, READACCESSTYPE rmw) override;
	void WRWORD(Word dest, Word val) override;
	void TriggerInterrupt(Word vector, Byte level) override;
	
	Word GetSafeWord(int x, int bank) override;
	Byte GetSafeByte(int x, int bank) override;
};


