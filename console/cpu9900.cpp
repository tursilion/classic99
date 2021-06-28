//
// (C) 2021 Mike Brent aka Tursi aka HarmlessLion.com
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

// TODO: (somewhere)
// CRUCLK is not available at the cartridge port of QI consoles, so we should
// disable it on cartridges for v2.2 consoles, if we ever do it.

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#include <stdio.h>
#include <windows.h>
#include <vector>
#include "tiemul.h"
#include "cpu9900.h"
#include "..\addons\F18A.h"
#include "..\resource.h"

extern bool BreakOnIllegal;                         // true if we should trigger a breakpoint on bad opcode
extern int enableDebugOpcodes;
extern CPU9900 * volatile pCurrentCPU;
extern CPU9900 *pCPU, *pGPU;
extern int bInterleaveGPU;
extern FILE *fpDisasm;          // file pointer for logging disassembly, if active
extern int disasmLogType;
extern CRITICAL_SECTION csDisasm;

// we set skip_interrupt to 2 because it's decremented after every instruction - including
// the one that sets it!
#define SET_SKIP_INTERRUPT skip_interrupt=2

/////////////////////////////////////////////////////////////////////
// Status register defines
/////////////////////////////////////////////////////////////////////
#if 0
// defined in tiemul.h
#define BIT_LGT 0x8000
#define BIT_AGT 0x4000
#define BIT_EQ  0x2000
#define BIT_C   0x1000
#define BIT_OV  0x0800
#define BIT_OP  0x0400
#define BIT_XOP 0x0200
#endif

#define ST_LGT (ST & BIT_LGT)                       // Logical Greater Than
#define ST_AGT (ST & BIT_AGT)                       // Arithmetic Greater Than
#define ST_EQ  (ST & BIT_EQ)                        // Equal
#define ST_C   (ST & BIT_C)                         // Carry
#define ST_OV  (ST & BIT_OV)                        // Overflow
#define ST_OP  (ST & BIT_OP)                        // Odd Parity
#define ST_X   (ST & BIT_XOP)                       // Set during an XOP instruction
#define ST_INTMASK (ST&0x000f)                      // Interrupt mask (the TI uses only values 0 and 2)

#define set_LGT (ST|=0x8000)                        // Logical Greater than: >0x0000
#define set_AGT (ST|=0x4000)                        // Arithmetic Greater than: >0x0000 and <0x8000
#define set_EQ  (ST|=0x2000)                        // Equal: ==0x0000
#define set_C   (ST|=0x1000)                        // Carry: carry occurred during operation
#define set_OV  (ST|=0x0800)                        // Overflow: overflow occurred during operation
#define set_OP  (ST|=0x0400)                        // Odd parity: word has odd number of '1' bits
#define set_XOP (ST|=0x0200)                        // Executing 'XOP' function

#define reset_LGT (ST&=0x7fff)                      // Clear the flags
#define reset_AGT (ST&=0xbfff)
#define reset_EQ  (ST&=0xdfff)
#define reset_C   (ST&=0xefff)
#define reset_OV  (ST&=0xf7ff)
#define reset_OP  (ST&=0xfbff)
#define reset_XOP (ST&=0xfdff)

// Group clears
#define reset_EQ_LGT (ST&=0x5fff)
#define reset_LGT_AGT_EQ (ST&=0x1fff)
#define reset_LGT_AGT_EQ_OP (ST&=0x1bff)
#define reset_EQ_LGT_AGT_OV (ST&=0x17ff)
#define reset_EQ_LGT_AGT_C (ST&=0x0fff)
#define reset_EQ_LGT_AGT_C_OV (ST&=0x7ff)
#define reset_EQ_LGT_AGT_C_OV_OP (ST&=0x3ff)

// Assignment masks
#define mask_EQ_LGT (BIT_EQ|BIT_LGT)
#define mask_LGT_AGT_EQ (BIT_LGT|BIT_AGT|BIT_EQ)
#define mask_LGT_AGT_EQ_OP (BIT_LGT|BIT_AGT|BIT_EQ|BIT_OP)
#define mask_LGT_AGT_EQ_OV (BIT_LGT|BIT_AGT|BIT_EQ|BIT_OV)
#define mask_LGT_AGT_EQ_OV_C (BIT_LGT|BIT_AGT|BIT_EQ|BIT_OV|BIT_C)      // carry here used for INC and NEG only

// Status register lookup table (hey, what's another 64k these days??) -- shared
Word WStatusLookup[64*1024];
Word BStatusLookup[256];

// Note: Post-increment is a trickier case that it looks at first glance.
// For operations like MOV R3,*R3+ (from Corcomp's memory test), the address
// value is written to the same address before the increment occurs.
// There are even trickier cases in the console like MOV *R3+,@>0008(R3),
// where the post-increment happens before the destination address calculation.
// Then in 2021 we found MOV *R0+,R1, where R0 points to itself, and found
// that the increment happens before *R0 is fetched for the move.
// Thus it appears the steps need to happen in this order:
//
// 1) Calculate source address
// 2) Handle source post-increment
// 3) Get Source data
// 4) Calculate destination address
// 5) Handle Destination post-increment
// 6) Store destination data
//
// Only the following instruction formats support post-increment:
// FormatI
// FormatIII (src only) (destination can not post-increment)
// FormatIV (src only) (has no destination)
// FormatVI (src only) (has no destination)
// FormatIX (src only) (has no destination)

// force the PC to always be 15-bit, can't store a 16-bit PC
#define ADDPC(x) { PC+=(x); PC&=0xfffe; }

/////////////////////////////////////////////////////////////////////
// Inlines for getting source and destination addresses
/////////////////////////////////////////////////////////////////////
#define FormatI { Td=(in&0x0c00)>>10; Ts=(in&0x0030)>>4; D=(in&0x03c0)>>6; S=(in&0x000f); B=(in&0x1000)>>12; fixS(); }
#define FormatII { D=(in&0x00ff); }
#define FormatIII { Td=0; Ts=(in&0x0030)>>4; D=(in&0x03c0)>>6; S=(in&0x000f); B=0; fixS(); }
#define FormatIV { D=(in&0x03c0)>>6; Ts=(in&0x0030)>>4; S=(in&0x000f); B=(D<9); fixS(); }           // No destination (CRU ops)
#define FormatV { D=(in&0x00f0)>>4; S=(in&0x000f); S=WP+(S<<1); }
#define FormatVI { Ts=(in&0x0030)>>4; S=in&0x000f; B=0; fixS(); }                                   // No destination (single argument instructions)
#define FormatVII {}                                                                                // no argument
#define FormatVIII_0 { D=(in&0x000f); D=WP+(D<<1); }
#define FormatVIII_1 { D=(in&0x000f); D=WP+(D<<1); S=ROMWORD(PC); ADDPC(2); }
#define FormatIX  { D=(in&0x03c0)>>6; Ts=(in&0x0030)>>4; S=(in&0x000f); B=0; fixS(); }              // No destination here (dest calc'd after call) (DIV, MUL, XOP)

CPU9900::CPU9900() {
    buildcpu();
    pType="9900";
    enableDebug=true;
}

void CPU9900::reset() {
    // Base cycles: 26
    // 5 memory accesses:
    //  Read WP
    //  Write WP->R13
    //  Write PC->R14
    //  Write ST->R15
    //  Read PC
    
    // TODO: Read WP & PC are obvious, what are the other 3? Does it do a TRUE BLWP? Can we see where a reset came from?
    // It matches the LOAD interrupt, so maybe yes! Test above theory on hardware.
    
    StopIdle();
    halted = 0;         // clear all possible halt sources
    nReturnAddress=0;

    TriggerInterrupt(0x0000,0);             // reset vector is 22 cycles
    AddCycleCount(4);                       // reset is slightly more work than a normal interrupt

    X_flag=0;                               // not currently executing an X instruction
    ST=(ST&0xfff0);                         // disable interrupts

    spi_reset();                            // reset the F18A flash interface
}

/////////////////////////////////////////////////////////////////////
// Wrapper functions for memory access
/////////////////////////////////////////////////////////////////////
Byte CPU9900::RCPUBYTE(Word src) {
    Word ReadVal=romword(src);
    if (src&1) {
        return ReadVal&0xff;
    } else {
        return ReadVal>>8;
    }
}

void CPU9900::WCPUBYTE(Word dest, Byte c) {
    Word ReadVal=romword(dest, ACCESS_RMW); // read-before-write needed, of course!
    if (dest&1) {
        wrword(dest, (Word)((ReadVal&0xff00) | c));
    } else {
        wrword(dest, (Word)((ReadVal&0x00ff) | (c<<8)));
    }
}

Word CPU9900::ROMWORD(Word src, READACCESSTYPE rmw=ACCESS_READ) {
    // nothing special here yet
    return romword(src, rmw);
}

void CPU9900::WRWORD(Word dest, Word val) {
    wrword(dest, val);
}

Word CPU9900::GetSafeWord(int x, int bank) {
    x&=0xfffe;
    return (GetSafeByte(x, bank)<<8)|GetSafeByte(x+1, bank);
}

// Read a byte withOUT triggering the hardware - for monitoring
Byte CPU9900::GetSafeByte(int x, int bank) {
    return GetSafeCpuByte(x, bank);
}

//////////////////////////////////////////////////////////////////////////
// Get addresses for the destination and source arguments
// Note: the format code letters are the official notation from Texas
// instruments. See their TMS9900 documentation for details.
// (Td, Ts, D, S, B, etc)
// Note that some format codes set the destination type (Td) to
// '4' in order to skip unneeded processing of the Destination address
//////////////////////////////////////////////////////////////////////////
void CPU9900::fixS()
{
    int temp,t2;                                                    // temp vars

    switch (Ts)                                                     // source type
    { 
    case 0: S=WP+(S<<1);                                            // 0 extra memory cycles
            break;                                                  // register                     (R1)            Address is the address of the register

    case 1: 
            S=ROMWORD(WP+(S<<1));                                   // 1 extra memory cycle: read register
            AddCycleCount(4); 
            break;                                                  // register indirect            (*R1)           Address is the contents of the register

    case 2: 
            if (S) {                                                // 2 extra memory cycles: read register, read argument
                S=ROMWORD(PC)+ROMWORD(WP+(S<<1));                   // indexed                      (@>1000(R1))    Address is the contents of the argument plus the
            } else {                                                //                                              contents of the register
                S=ROMWORD(PC);                                      // 1 extra memory cycle: read argument
            }                                                       // symbolic                     (@>1000)        Address is the contents of the argument
            ADDPC(2); 
            AddCycleCount(8);
            break;

    case 3: 
            t2=WP+(S<<1);                                           // 2 extra memory cycles: read register, write register
            temp=ROMWORD(t2); 
            S=temp;
            // After we have the final address, we can increment the register (so MOV *R0+ returns the post increment if R0=adr(R0))
            WRWORD(t2, temp + (B == 1 ? 1:2));                      // do count this write
            AddCycleCount((B==1?6:8));                              // (add 1 if byte, 2 if word)   (*R1+)          Address is the contents of the register, which
            break;                                                  // register indirect autoincrement              is incremented by 1 for byte or 2 for word ops
    }
}

void CPU9900::fixD()
{
    int temp,t2;                                                    // temp vars

    switch (Td)                                                     // destination type 
    {                                                               // same as the source types
    case 0: 
            D=WP+(D<<1);                                            // 0 extra memory cycles
            break;                                                  // register

    case 1: D=ROMWORD(WP+(D<<1));                                   // 1 extra memory cycle: read register 
            AddCycleCount(4);
            break;                                                  // register indirect

    case 2: 
            if (D) {                                                // 2 extra memory cycles: read register, read argument 
                D=ROMWORD(PC)+ROMWORD(WP+(D<<1));                   // indexed 
            } else {
                D=ROMWORD(PC);                                      // 1 extra memory cycle: read argument
            }                                                       // symbolic
            ADDPC(2);
            AddCycleCount(8);
            break;

    case 3: 
            t2=WP+(D<<1);                                           // 2 extra memory cycles: read register, write register
            temp=ROMWORD(t2);
            D=temp; 
            // After we have the final address, we can increment the register (so MOV *R0+ returns the post increment if R0=adr(R0))
            WRWORD(t2, temp + (B == 1 ? 1:2));                      // do count this write - add 1 if byte, 2 if word
            AddCycleCount((B==1?6:8)); 
            break;                                                  // register indirect autoincrement
    }
}

/////////////////////////////////////////////////////////////////////////
// Check parity in the passed byte and set the OP status bit
/////////////////////////////////////////////////////////////////////////
void CPU9900::parity(Byte x)
{
    int z;                                                          // temp vars

    for (z=0; x; x&=(x-1)) z++;                                     // black magic?
    
    if (z&1)                                                        // set bit if an odd number
        set_OP; 
    else 
        reset_OP;
}

// Helpers for what used to be global variables
void CPU9900::StartIdle() {
    idling = 1;
}
void CPU9900::StopIdle() {
    idling = 0;
}
int  CPU9900::GetIdle() {
    return idling;
}
// TODO: right now only speech can halt the CPU, but there are multiple
// possible sources. Instead of a global start/stop, each possible system
// should be able to register a halt and turn it on and off, then let
// the CPU decide if it's halted from ALL the flags (could use a bitflag
// for faster testing)
void CPU9900::StartHalt(int source) {
    halted |= (1<<source);
}
void CPU9900::StopHalt(int source) {
    halted &= ~(1<<source);
}
int  CPU9900::GetHalt() {
    return halted;
}
void CPU9900::SetReturnAddress(Word x) {
    nReturnAddress = x;
}
int CPU9900::GetReturnAddress() {
    return nReturnAddress;
}
void CPU9900::ResetCycleCount() {
    InterlockedExchange((LONG*)&nCycleCount, 0);
}
void CPU9900::AddCycleCount(int val) {
    InterlockedExchangeAdd((LONG*)&nCycleCount, val);
}
int CPU9900::GetCycleCount() {
    return nCycleCount;
}
void CPU9900::SetCycleCount(int x) {
    InterlockedExchange((LONG*)&nCycleCount, x);
}
void CPU9900::TriggerInterrupt(Word vector, Byte level) {
    // Base cycles: 22
    // 5 memory accesses:
    //  Read WP
    //  Write WP->R13
    //  Write PC->R14
    //  Write ST->R15
    //  Read PC

    // debug helper if logging
    EnterCriticalSection(&csDisasm);
        if (NULL != fpDisasm) {
            if ((disasmLogType == 0) || (pCurrentCPU->GetPC() > 0x2000)) {
                fprintf(fpDisasm, "**** Interrupt Trigger, vector >%04X (%s), level %d\n",
                    vector, 
                    (vector==0)?"reset":(vector==4)?"console":(vector=0xfffc)?"load":"unknown",
                    level);
            }
        }
    LeaveCriticalSection(&csDisasm);

    // no more idling!
    StopIdle();
    
    // I don't think this is legal on the F18A
    Word NewWP = ROMWORD(vector);

    WRWORD(NewWP+26,WP);                // WP in new R13 
    WRWORD(NewWP+28,PC);                // PC in new R14 
    WRWORD(NewWP+30,ST);                // ST in new R15 

    // lower the interrupt level
    if (level == 0) {
        ST=(ST&0xfff0);
    } else {
        ST=(ST&0xfff0) | (level-1);
    }

    Word NewPC = ROMWORD(vector+2);

    /* now load the correct workspace, and perform a branch and link to the address */
    SetWP(NewWP);
    SetPC(NewPC);

    AddCycleCount(22);
    SET_SKIP_INTERRUPT;                 // you get one instruction to turn interrupts back off
                                        // this is true for all BLWP-like operations
}
Word CPU9900::GetPC() {
    return PC;
}
void CPU9900::SetPC(Word x) {           // should rarely be externally used (Classic99 uses it for disk emulation)
    if (x&0x0001) {
        debug_write("Warning: setting odd PC address from >%04X", PC);
    }

    // the PC is 15 bits wide - confirmed via BigFoot game which relies on this
    PC=x&0xfffe;
}
Word CPU9900::GetST() {
    return ST;
}
void CPU9900::SetST(Word x) {
    ST=x;
}
Word CPU9900::GetWP() {
    return WP;
}
void CPU9900::SetWP(Word x) {
    // TODO: confirm on hardware - is the WP also 15-bit?
    // we can test using BLWP and see what gets stored
    WP=x&0xfffe;
}
Word CPU9900::GetX() {
    return X_flag;
}
void CPU9900::SetX(Word x) {
    X_flag=x;
}

Word CPU9900::ExecuteOpcode(bool nopFrame) {
    if (nopFrame) {
        in = 0x10FF;                // JMP $ - NOP, but because we don't ADDPC below it doesn't move ;)
    } else {
        in=ROMWORD(PC);
        ADDPC(2);
    }

    CALL_MEMBER_FN(this, opcode[in])();

    return in;
}

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
// emulation and the cycle counting! You'll also break the F18A. ;)
// The 9900 can only do word access.
/////////////////////////////////////////////////////////////////////
#define wcpubyte #error Do not use in this file
#define rcpubyte #error Do not use in this file
#define romword  #error Do not use in this file
#define wrword   #error Do not use in this file

// a little macro to handle byte manipulation inline
// just so I don't get one-off bugs ;)
// theOp - operation to perform
// Assumes xD is the word data var, and x2 is the byte data var, 
// x3 is the byte result, and D is the address
// use is like: xD=ROMWORD(adr); BYTE_OPERATION(x3=x2-x1); WRWORD(D,xD);
#define BYTE_OPERATION(theOp) \
    if (D&1) {                      \
        x2 = (xD&0xff); /*LSB*/     \
        theOp;                      \
        xD = (xD&0xff00) | x3;      \
    } else {                        \
        x2 = (xD>>8);  /*MSB*/      \
        theOp;                      \
        xD = (xD&0xff) | (x3<<8);   \
    }

void CPU9900::op_a()
{
    // Add words: A src, dst

    // TODO: all timing needs revision, however, I'm going to start by documenting the cycles
    // In case I care in the future, every machine step is 2 cycles -- so a four cycle memory
    // access (which is indeed normal) is 2 cycles on the bus, and 2 cycles of thinking

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Word x1,x2,x3;

    AddCycleCount(14);

    FormatI;
    x1=ROMWORD(S);  // read source

    fixD();
    x2=ROMWORD(D);  // read dest
    x3=x2+x1; 
    WRWORD(D,x3);   // write dest
                                                                                        // most of these are the same for every opcode.
    reset_EQ_LGT_AGT_C_OV;                                                              // We come out with either EQ or LGT, never both
    ST|=WStatusLookup[x3]&mask_LGT_AGT_EQ;

    if (x3<x2) set_C;                                                                   // if it wrapped around, set carry
    if (((x1&0x8000)==(x2&0x8000))&&((x3&0x8000)!=(x2&0x8000))) set_OV;                 // if it overflowed or underflowed (signed math), set overflow
}

void CPU9900::op_ab()
{ 
    // Add bytes: A src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Byte x1,x2,x3;
    Word xD;

    AddCycleCount(14);

    FormatI;
    x1=RCPUBYTE(S);     // read source 

    fixD();
    xD=ROMWORD(D);      // read destination

    BYTE_OPERATION(x3=x2+x1);   // xD->x2, result->xD, uses D
    
    WRWORD(D,xD);       // write destination
    
    reset_EQ_LGT_AGT_C_OV_OP;
    ST|=BStatusLookup[x3]&mask_LGT_AGT_EQ_OP;

    if (x3<x2) set_C;
    if (((x1&0x80)==(x2&0x80))&&((x3&0x80)!=(x2&0x80))) set_OV;
}

void CPU9900::op_abs()
{ 
    // ABSolute value: ABS src
    
    // MSB == 0
    // Base cycles: 12
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read source

    // MSB == 1
    // Base cycles: 14
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1,x2;

    AddCycleCount(12);

    FormatVI;
    x1=ROMWORD(S);      // read source

    if (x1&0x8000) {
        x2=(~x1)+1;     // if negative, make positive
        WRWORD(S,x2);   // write source
        AddCycleCount(2);
    }

    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;
}

void CPU9900::op_ai()
{ 
    // Add Immediate: AI src, imm
    
    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Word x1,x3;
    
    AddCycleCount(14);

    FormatVIII_1;   // read source
    x1=ROMWORD(D);  // read dest

    x3=x1+S;
    WRWORD(D,x3);   // write dest

    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x3]&mask_LGT_AGT_EQ;

    if (x3<x1) set_C;
    if (((x1&0x8000)==(S&0x8000))&&((x3&0x8000)!=(S&0x8000))) set_OV;
}

void CPU9900::op_dec()
{ 
    // DECrement: DEC src

    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source
    
    Word x1;
    
    AddCycleCount(10);

    FormatVI;
    x1=ROMWORD(S);  // read source

    x1--;
    WRWORD(S,x1);   // write source

    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x1!=0xffff) set_C;
    if (x1==0x7fff) set_OV;
}

void CPU9900::op_dect()
{ 
    // DECrement by Two: DECT src
    
    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1;
    
    AddCycleCount(10);

    FormatVI;
    x1=ROMWORD(S);  // read source

    x1-=2;
    WRWORD(S,x1);   // write source
    
    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x1<0xfffe) set_C;
    if ((x1==0x7fff)||(x1==0x7ffe)) set_OV;
}

void CPU9900::op_div()
{ 
    // DIVide: DIV src, dst
    // Dest, a 2 word number, is divided by src. The result is stored as two words at the dst:
    // the first is the whole number result, the second is the remainder

    // ST4 (OV) is to be set:
    // Base cycles: 16
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source MSW
    //  Read dest

    // ST4 (OV) is not to be set:
    // Base cycles: 92 - 124
    // 6 memory accesses:
    //  Read instruction (already done)
    //  Read source MSW
    //  Read dest
    //  Read source LSW
    //  Write dest quotient
    //  Write dest remainder

    // Sussing out the cycle count. It is likely a shift and test
    // approach, due to the 16 bit cycle variance. We know the divisor
    // is larger than the most significant word, ie: the total output
    // of the first 16 bits of the 32-bit result MUST be >0000, so
    // the algorithm likely starts with the first bit of the LSW, and
    // shifts through up to 16 cycles, aborting early if the remaining
    // value is smaller than the divisor (or if it's zero, but that
    // would early out in far fewer cases).
    //
    // For example (using 4 bits / 2 bits = 2 bits):
    //  DDDD
    // / VV
    //  ---
    //    1 (subtract from DDDx if set)
    //
    //  0DDD
    // /  VV
    //   ---
    //     2
    //
    // 12 are the bits in the result, and DDD goes to the remainder
    //
    // Proofs:
    // 8/2 -> Overflow (10xx >= 10)
    // 4/2 -> Ok (01xx < 10) -> 010x / 10 = '1' -> 010-10=0 -> 10 > 00 so early out, remaining bits 0 -> 10, remainder 0, 1 clock
    // 1/1 -> Ok (00xx < 01) -> 000x / 01 = '0' -> x001 / 01 = '1' -> 001-01=0 -> 01>00 so finished (either way) -> 01, remainder 1, 2 clocks
    // 5/2 -> Ok (01xx < 10) -> 010 / 10 = '1' -> 010-10=0 -> 10 > 001 so done -> 10, remainder 1, 1 clock
    // 
    // TODO: We should be able to prove on real hardware that the early out works like this (greater than, and not 0) with a few choice test cases

    Word x1,x2; 
    unsigned __int32 x3;
    
    // minimum possible when division does not occur
    AddCycleCount(16);

    FormatIX;
    x2=ROMWORD(S);  // read source MSW

    D=WP+(D<<1);
    x3=ROMWORD(D);  // read dest
    
    // E/A: When the source operand is greater than the first word of the destination
    // operand, normal division occurs. If the source operand is less than or equal to
    // the first word of the destination operand, normal division results in a quotient
    // that cannot be represented in a 16-bit word. In this case, the computer sets the
    // overflow status bit, leaves the destination operand unchanged, and cancels the
    // division operation.
    if (x2>x3)                      // x2 can not be zero because they're unsigned                                      
    { 
        x3=(x3<<16)|ROMWORD(D+2);   // read source LSW
#if 0
        x1=(Word)(x3/x2);
        WRWORD(D,x1);
        x1=(Word)(x3%x2);
        WRWORD(D+2,x1);
#else
        // lets try it the iterative way, should be able to afford it
        // tested with 10,000,000 random combinations, should be accurate :)
        unsigned __int32 mask = (0xFFFF8000);   // 1 extra bit, as noted above
        unsigned __int32 divisor = (x2<<15);    // slide up into place
        int cnt = 16;                           // need to fill 16 bits
        x1 = 0;                                 // initialize quotient, remainder will end up in x3 LSW
        while (x2 <= x3) {
            x1<<=1;
            if ((x3&mask) >= divisor) {
                x1|=1;
                x3-=divisor;
            }
            mask>>=1;
            divisor>>=1;
            --cnt;
            AddCycleCount(1);
        }
        if (cnt < 0) {
            debug_write("Warning: Division bug. Send to Tursi if you can.");
        }
        while (cnt-- > 0) {
            // handle the early-out case
            x1<<=1;
        }
        WRWORD(D,x1);               // write dest quotient
        WRWORD(D+2,x3&0xffff);      // write dest remainder
#endif
        reset_OV;
        AddCycleCount(92-16);       // base ticks
    }
    else
    {
        set_OV;                     // division wasn't possible - change nothing
    }
}

void CPU9900::op_inc()
{ 
    // INCrement: INC src
    
    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1;
    
    AddCycleCount(10);

    FormatVI;
    x1=ROMWORD(S);      // read source
    
    x1++;
    WRWORD(S,x1);       // write source
    
    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV_C;
}

void CPU9900::op_inct()
{ 
    // INCrement by Two: INCT src
    
    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1;
    
    AddCycleCount(10);

    FormatVI;
    x1=ROMWORD(S);      // read source
    
    x1+=2;
    WRWORD(S,x1);       // write source
    
    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x1<2) set_C;
    if ((x1==0x8000)||(x1==0x8001)) set_OV;
}

void CPU9900::op_mpy()
{ 
    // MultiPlY: MPY src, dst
    // Multiply src by dest and store 32-bit result

    // Base cycles: 52
    // 5 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest MSW
    //  Write dest LSW

    Word x1; 
    unsigned __int32 x3;
    
    AddCycleCount(52);

    FormatIX;
    x1=ROMWORD(S);      // read source
    
    D=WP+(D<<1);
    x3=ROMWORD(D);      // read dest
    x3=x3*x1;
    WRWORD(D,(Word)(x3>>16));       // write dest MSW
    WRWORD(D+2,(Word)(x3&0xffff));  // write dest LSW
}

void CPU9900::op_neg()
{ 
    // NEGate: NEG src

    // Base cycles: 12
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1;
    
    AddCycleCount(12);

    FormatVI;
    x1=ROMWORD(S);  // read source

    x1=(~x1)+1;
    WRWORD(S,x1);   // write source

    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV_C;
}

void CPU9900::op_s()
{ 
    // Subtract: S src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Word x1,x2,x3;
    
    AddCycleCount(14);

    FormatI;
    x1=ROMWORD(S);  // read source

    fixD();
    x2=ROMWORD(D);  // read dest
    x3=x2-x1;
    WRWORD(D,x3);   // write dest

    reset_EQ_LGT_AGT_C_OV;
    ST|=WStatusLookup[x3]&mask_LGT_AGT_EQ;

    // any number minus 0 sets carry.. my theory is that converting 0 to the two's complement
    // is causing the carry flag to be set.
    if ((x3<x2) || (x1==0)) set_C;
    if (((x1&0x8000)!=(x2&0x8000))&&((x3&0x8000)!=(x2&0x8000))) set_OV;
}

void CPU9900::op_sb()
{ 
    // Subtract Byte: SB src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Byte x1,x2,x3;
    Word xD;
    
    AddCycleCount(14);

    FormatI;
    x1=RCPUBYTE(S);     // read source

    fixD();
    xD = ROMWORD(D);    // read dest

    BYTE_OPERATION(x3=x2-x1);   // xD->x2, result->xD, uses D

    WRWORD(D, xD);      // write dest

    reset_EQ_LGT_AGT_C_OV_OP;
    ST|=BStatusLookup[x3]&mask_LGT_AGT_EQ_OP;

    // any number minus 0 sets carry.. my theory is that converting 0 to the two's complement
    // is causing the carry flag to be set.
    if ((x3<x2) || (x1==0)) set_C;
    if (((x1&0x80)!=(x2&0x80))&&((x3&0x80)!=(x2&0x80))) set_OV;
}

void CPU9900::op_b()
{ 
    // Branch: B src
    // Unconditional absolute branch

    // Base cycles: 8
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read source (confirmed by system design book)

    AddCycleCount(8);

    FormatVI;
    ROMWORD(S);     // read source (unused)
    SetPC(S);
}

void CPU9900::op_bl()
{   
    // Branch and Link: BL src
    // Essentially a subroutine jump - return address is stored in R11
    // Note there is no stack, and no official return function.
    // A return is simply B *R11. Some assemblers define RT as this.

    // Base cycles: 12
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write return

    AddCycleCount(12);

    FormatVI;
    ROMWORD(S);         // read source (unused)
    if (0 == GetReturnAddress()) {
        SetReturnAddress(PC);
    }
    WRWORD(WP+22,PC);   // write return
    SetPC(S);
}

void CPU9900::op_blwp()
{ 
    // Branch and Load Workspace Pointer: BLWP src
    // A context switch. The src address points to a 2 word table.
    // the first word is the new workspace address, the second is
    // the address to branch to. The current Workspace Pointer,
    // Program Counter (return address), and Status register are
    // stored in the new R13, R14 and R15, respectively
    // Return is performed with RTWP

    // Base cycles: 26
    // 6 memory accesses:
    //  Read instruction (already done)
    //  Read WP
    //  Write WP->R13
    //  Write PC->R14
    //  Write ST->R15
    //  Read PC

    // Note that there is no "read source" (BLWP R0 /does/ branch with >8300, it doesn't fetch R0)
    // TODO: We need to time out this instruction and verify that analysis.

    Word x1;
    
    AddCycleCount(26);

    FormatVI;
    if (0 == GetReturnAddress()) {
        SetReturnAddress(PC);
    }
    x1=WP;
    SetWP(ROMWORD(S));      // read WP
    WRWORD(WP+26,x1);       // write WP->R13
    WRWORD(WP+28,PC);       // write PC->R14
    WRWORD(WP+30,ST);       // write ST->R15
    SetPC(ROMWORD(S+2));    // read PC

    // TODO: is it possible to conceive a test where the BLWP vectors being written affects
    // where it jumps to? That is - can we prove the above order of operation is correct
    // by placing the workspace and the vector such that they overlap, and then seeing
    // where it actually jumps to on hardware?

    SET_SKIP_INTERRUPT;
}

void CPU9900::op_jeq()
{ 
    // Jump if equal: JEQ dsp
    // Conditional relative branch. The displacement is a signed byte representing
    // the number of words to branch

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (ST_EQ) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jgt()
{ 
    // Jump if Greater Than: JGT dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (ST_AGT) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jhe()
{ 
    // Jump if High or Equal: JHE dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if ((ST_LGT)||(ST_EQ)) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jh()
{ 
    // Jump if High: JH dsp
    
    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if ((ST_LGT)&&(!ST_EQ)) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jl()
{
    // Jump if Low: JL dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if ((!ST_LGT)&&(!ST_EQ)) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jle()
{ 
    // Jump if Low or Equal: JLE dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if ((!ST_LGT)||(ST_EQ)) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jlt()
{ 
    // Jump if Less Than: JLT dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if ((!ST_AGT)&&(!ST_EQ)) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jmp()
{ 
    // JuMP: JMP dsp
    // (unconditional)
    
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(10);

    FormatII;
    if (X_flag) {
        SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
    }
    if (D&0x80) {
        D=128-(D&0x7f);
        ADDPC(-(D+D));
    } else {
        ADDPC(D+D);
    }
}

void CPU9900::op_jnc()
{ 
    // Jump if No Carry: JNC dsp
    
    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (!ST_C) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jne()
{ 
    // Jump if Not Equal: JNE dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (!ST_EQ) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jno()
{ 
    // Jump if No Overflow: JNO dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (!ST_OV) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_jop()
{ 
    // Jump on Odd Parity: JOP dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (ST_OP) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_joc()
{ 
    // Jump On Carry: JOC dsp

    // Jump taken:
    // Base cycles: 10
    // 1 memory access:
    //  Read instruction (already done)

    // Jump not taken:
    // Base cycles: 8
    // 1 memory access:
    //  Read instruction (already done)
    AddCycleCount(8);   // base count for jump not taken

    FormatII;
    if (ST_C) 
    {
        if (X_flag) {
            SetPC(X_flag);  // Update offset - it's relative to the X, not the opcode
        }

        if (D&0x80) {
            D=128-(D&0x7f);
            ADDPC(-(D+D));
        } else {
            ADDPC(D+D);
        }
        AddCycleCount(10-8);
    }
}

void CPU9900::op_rtwp()
{ 
    // ReTurn with Workspace Pointer: RTWP
    // The matching return for BLWP, see BLWP for description

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read ST<-R15
    //  Read WP<-R13
    //  Read PC<-R14

    AddCycleCount(14);

    FormatVII;

    ST=ROMWORD(WP+30);          // ST<-R15
    SetPC(ROMWORD(WP+28));      // PC<-R14 (order matter?)
    SetWP(ROMWORD(WP+26));      // WP<-R13 -- needs to be last!

    // TODO: does this need to skip interrupt? Datasheet doesn't say so
    //SET_SKIP_INTERRUPT;
}

void CPU9900::op_x()
{ 
    // eXecute: X src
    // The argument is interpreted as an instruction and executed

    // Base cycles: 8 - added to the execution time of the instruction minus 4 clocks and 1 memory
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read source

    if (X_flag!=0) 
    {
        warn("Recursive X instruction!!!!!");
        // While it will probably work (recursive X), I don't like the idea ;)
        // Barry Boone says that it does work, although if you recursively
        // call X in a register (ie: X R4 that contains X R4), you will lock
        // up the CPU so bad even the LOAD interrupt can't recover it.
        // We don't emulate that lockup here in Classic99, but of course it
        // will just spin forever.
        // TODO: we should try this ;)
    }
    AddCycleCount(8-4); // For X, add this time to the execution time of the instruction found at the source address, minus 4 clock cycles and 1 memory access. 
                        // we already accounted for the memory access (the instruction is going to be already in S)

    FormatVI;
    in=ROMWORD(S);      // read source

    X_flag=PC;          // set flag and save true post-X address for the JMPs (AFTER X's oprands but BEFORE the instruction's oprands, if any)

    CALL_MEMBER_FN(this, opcode[in])();

    X_flag=0;           // clear flag
}

void CPU9900::op_xop()
{ 
    // eXtended OPeration: XOP src ???
    // The CPU maintains a jump table starting at 0x0040, containing BLWP style
    // jumps for each operation. In addition, the new R11 gets a copy of the address of
    // the source operand.
    // Apparently not all consoles supported both XOP 1 and 2 (depends on the ROM)
    // so it is probably rarely, if ever, used on the TI99.
    
    // Base cycles: 36
    // 8 memory accesses:
    //  Read instruction (already done)
    //  Read source (unused)
    //  Read WP
    //  Write Src->R11
    //  Write WP->R13
    //  Write PC->R14
    //  Write ST->R15
    //  Read PC

    Word x1;

    AddCycleCount(36);

    FormatIX;
    D&=0xf;

    ROMWORD(S);         // read source (unused)

    x1=WP;
    SetWP(ROMWORD(0x0040+(D<<2)));  // read WP
    WRWORD(WP+22,S);                // write Src->R11
    WRWORD(WP+26,x1);               // write WP->R13
    WRWORD(WP+28,PC);               // write PC->R14
    WRWORD(WP+30,ST);               // write ST->R15
    SetPC(ROMWORD(0x0042+(D<<2)));  // read PC
    set_XOP;

    SET_SKIP_INTERRUPT;
}

void CPU9900::op_c()
{ 
    // Compare words: C src, dst
    
    // Base cycles: 14
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest

    Word x3,x4;     // unsigned 16 bit

    AddCycleCount(14);

    FormatI;
    x3=ROMWORD(S);  // read source

    fixD();
    x4=ROMWORD(D);  // read dest

    reset_LGT_AGT_EQ;
    if (x3>x4) set_LGT;
    if (x3==x4) set_EQ;
    if ((x3&0x8000)==(x4&0x8000)) {
        if (x3>x4) set_AGT;
    } else {
        if (x4&0x8000) set_AGT;
    }
}

void CPU9900::op_cb()
{ 
    // Compare Bytes: CB src, dst

    // Base cycles: 14
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest

    Byte x3,x4;

    AddCycleCount(14);

    FormatI;
    x3=RCPUBYTE(S);     // read source

    fixD();
    x4=RCPUBYTE(D);     // read dest
  
    reset_LGT_AGT_EQ_OP;
    if (x3>x4) set_LGT;
    if (x3==x4) set_EQ;
    if ((x3&0x80)==(x4&0x80)) {
        if (x3>x4) set_AGT;
    } else {
        if (x4&0x80) set_AGT;
    }
    ST|=BStatusLookup[x3]&BIT_OP;
}

void CPU9900::op_ci()
{ 
    // Compare Immediate: CI src, imm
    
    // Base cycles: 14
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest

    Word x3;

    AddCycleCount(14);

    FormatVIII_1;       // read source
    x3=ROMWORD(D);      // read dest
  
    reset_LGT_AGT_EQ;
    if (x3>S) set_LGT;
    if (x3==S) set_EQ;
    if ((x3&0x8000)==(S&0x8000)) {
        if (x3>S) set_AGT;
    } else {
        if (S&0x8000) set_AGT;
    }
}

void CPU9900::op_coc()
{ 
    // Compare Ones Corresponding: COC src, dst
    // Basically comparing against a mask, if all set bits in the src match
    // set bits in the dest (mask), the equal bit is set

    // Base cycles: 14
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest

    Word x1,x2,x3;

    AddCycleCount(14);

    FormatIII;
    x1=ROMWORD(S);  // read source

    fixD();
    x2=ROMWORD(D);  // read dest
    
    x3=x1&x2;
  
    if (x3==x1) set_EQ; else reset_EQ;
}

void CPU9900::op_czc()
{ 
    // Compare Zeros Corresponding: CZC src, dst
    // The opposite of COC. Each set bit in the dst (mask) must
    // match up with a zero bit in the src to set the equals flag

    // Base cycles: 14
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest

    Word x1,x2,x3;

    AddCycleCount(14);

    FormatIII;
    x1=ROMWORD(S);  // read source

    fixD();
    x2=ROMWORD(D);  // read dest
    
    x3=x1&x2;
  
    if (x3==0) set_EQ; else reset_EQ;
}

void CPU9900::op_ldcr()
{ 
    // LoaD CRu - LDCR src, dst
    // Writes dst bits serially out to the CRU registers
    // The CRU is the 9901 Communication chip, tightly tied into the 9900.
    // It's serially accessed and has 4096 single bit IO registers.
    // It thinks 0 is true and 1 is false.
    // All addresses are offsets from the value in R12, which is divided by 2

    // Base cycles: 20 + 2 per count (count of 0 represents 16)
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source (byte access if count = 1-8)
    //  Read R12

    Word x1,x3,cruBase; 
    int x2;
    
    AddCycleCount(20);  // base count

    FormatIV;
    if (D==0) D=16;     // this also makes the timing of (0=52 cycles) work - 2*16=32+20=52
    x1=(D<9 ? RCPUBYTE(S) : ROMWORD(S));    // read source
  
    x3=1;

    // CRU base address - R12 bits 3-14 (0=MSb)
    // 0001 1111 1111 1110
    cruBase=(ROMWORD(WP+24)>>1)&0xfff;      // read R12
    for (x2=0; x2<D; x2++)
    { 
        wcru(cruBase+x2, (x1&x3) ? 1 : 0);
        x3=x3<<1;
    }

    AddCycleCount(2*D);

    // TODO: the data manual says this return is not true - test
    // whether a word load affects the other status bits
    //  if (D>8) return;

    reset_LGT_AGT_EQ;
    if (D<9) {
        reset_OP;
        ST|=BStatusLookup[x1&0xff]&mask_LGT_AGT_EQ_OP;
    } else {
        ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;
    }
}

void CPU9900::op_sbo()
{ 
    // Set Bit On: SBO src
    // Sets a bit in the CRU
    
    // Base cycles: 12
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read R12

    Word add;

    AddCycleCount(12);

    FormatII;
    add=(ROMWORD(WP+24)>>1)&0xfff;      // read R12
    if (D&0x80) {
        add-=128-(D&0x7f);
    } else {
        add+=D;
    }
    wcru(add,1);
}

void CPU9900::op_sbz()
{ 
    // Set Bit Zero: SBZ src
    // Zeros a bit in the CRU

    // Base cycles: 12
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read R12

    Word add;

    AddCycleCount(12);

    FormatII;
    add=(ROMWORD(WP+24)>>1)&0xfff;      // read R12
    if (D&0x80) {
        add-=128-(D&0x7f);
    } else {
        add+=D;
    }
    wcru(add,0);
}

void CPU9900::op_stcr()
{ 
    // STore CRU: STCR src, dst
    // Stores dst bits from the CRU into src

    // Base cycles: C=0:60, C=1-7:42, C=8:44, C=9-15:58
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read R12
    //  Read source
    //  Write source (byte access if count = 1-8)

    Word x1,x3,x4, cruBase; 
    int x2;

    AddCycleCount(42);  // base value

    FormatIV;
    if (D==0) D=16;
    x1=0; x3=1;
  
    cruBase=(ROMWORD(WP+24)>>1)&0xfff;  // read R12
    for (x2=0; x2<D; x2++)
    { 
        x4=rcru(cruBase+x2);
        if (x4) 
        {
            x1=x1|x3;
        }
        x3<<=1;
    }

    if (D<9) 
    {
        WCPUBYTE(S,(Byte)(x1&0xff));  // Read source, write source
    }
    else 
    {
        ROMWORD(S, ACCESS_RMW);       // read source (wasted)
        WRWORD(S,x1);                 // write source
    }

    if (D<8) {
//      AddCycleCount(42-42);
    } else if (D < 9) {
        AddCycleCount(44-42);
    } else if (D < 16) {
        AddCycleCount(58-42);
    } else {
        AddCycleCount(60-42);
    }

    // TODO: the data manual says this return is not true - test
    // whether a word load affects the other status bits
    //if (D>8) return;

    reset_LGT_AGT_EQ;
    if (D<9) {
        reset_OP;
        ST|=BStatusLookup[x1&0xff]&mask_LGT_AGT_EQ_OP;
    } else {
        ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;
    }
}

void CPU9900::op_tb()
{ 
    // Test Bit: TB src
    // Tests a CRU bit

    // Base cycles: 12
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read R12

    Word add;

    AddCycleCount(12);

    FormatII;
    add=(ROMWORD(WP+24)>>1)&0xfff;  // read R12
    if (D&0x80) {
        add-=128-(D&0x7f);
    } else {
        add+=D;
    }

    if (rcru(add)) set_EQ; else reset_EQ;
}

// These instructions are valid 9900 instructions but are invalid on the TI-99, as they generate
// improperly decoded CRU instructions.

void CPU9900::op_ckof()
{ 
    // Base cycles: 12
    // 1 memory accesses:
    //  Read instruction (already done)

    AddCycleCount(12);

    FormatVII;
    warn("ClocK OFf instruction encountered!");                 // not supported on 99/4A
    // This will set A0-A2 to 110 and pulse CRUCLK (so not emulated)
}

void CPU9900::op_ckon()
{ 
    // Base cycles: 12
    // 1 memory accesses:
    //  Read instruction (already done)

    AddCycleCount(12);

    FormatVII;
    warn("ClocK ON instruction encountered!");                  // not supported on 99/4A
    // This will set A0-A2 to 101 and pulse CRUCLK (so not emulated)
}

void CPU9900::op_idle()
{
    // Base cycles: 12
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(12);

    FormatVII;
    warn("IDLE instruction encountered!");                      // not supported on 99/4A
    // This sets A0-A2 to 010, and pulses CRUCLK until an interrupt is received
    // Although it's not supposed to be used on the TI, at least one game
    // (Slymoids) uses it - perhaps to sync with the VDP? So we'll emulate it someday

    // TODO: we can't do this today. Everything is based on CPU cycles, which means
    // when the CPU stops, so does the VDP, 9901, etc, so no interrupt ever comes in
    // to wake up the system. This will be okay when the VDP is the timing source.
//  SetIdle();
}

void CPU9900::op_rset()
{
    // Base cycles: 12
    // 1 memory accesses:
    //  Read instruction (already done)

    AddCycleCount(12);

    FormatVII;
    warn("ReSET instruction encountered!");                     // not supported on 99/4A
    // This will set A0-A2 to 011 and pulse CRUCLK (so not emulated)
    // However, it does have an effect, it zeros the interrupt mask
    ST&=0xfff0;
}

void CPU9900::op_lrex()
{
    // Base cycles: 12
    // 1 memory accesses:
    //  Read instruction (already done)

    AddCycleCount(12);

    FormatVII;
    warn("Load or REstart eXecution instruction encountered!"); // not supported on 99/4A
    // This will set A0-A2 to 111 and pulse CRUCLK (so not emulated)
}

void CPU9900::op_li()
{
    // Base cycles: 12
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read immediate
    //  Write register

    // note no read-before write!

    // Load Immediate: LI src, imm

    AddCycleCount(12);

    FormatVIII_1;       // read immediate
    WRWORD(D,S);        // write register
    
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[S]&mask_LGT_AGT_EQ;
}

void CPU9900::op_limi()
{ 
    // Load Interrupt Mask Immediate: LIMI imm
    // Sets the CPU interrupt mask

    // Base cycles: 16
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read immediate

    AddCycleCount(16);

    FormatVIII_1;               // read immediate
    ST=(ST&0xfff0)|(S&0xf);
}

void CPU9900::op_lwpi()
{ 
    // Load Workspace Pointer Immediate: LWPI imm
    // changes the Workspace Pointer

    // Base cycles: 10
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Read immediate

    AddCycleCount(10);

    FormatVIII_1;       // read immediate
    SetWP(S);
}

void CPU9900::op_mov()
{ 
    // MOVe words: MOV src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Word x1;

    AddCycleCount(14);

    FormatI;
    x1=ROMWORD(S);              // read source
    
    fixD();
    ROMWORD(D, ACCESS_RMW);     // read dest (wasted)
    WRWORD(D,x1);               // write dest
  
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;
}

void CPU9900::op_movb()
{ 
    // MOVe Bytes: MOVB src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Byte x1;

    AddCycleCount(14);

    FormatI;
    x1=RCPUBYTE(S);     // read source
    
    fixD();
    WCPUBYTE(D,x1);     // read dest, write dest
    
    reset_LGT_AGT_EQ_OP;
    ST|=BStatusLookup[x1]&mask_LGT_AGT_EQ_OP;
}

void CPU9900::op_stst()
{ 
    // STore STatus: STST src
    // Copy the status register to memory

    // Base cycles: 8
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Write dest

    AddCycleCount(8);

    FormatVIII_0;
    WRWORD(D,ST);   // write dest
}

void CPU9900::op_stwp()
{ 
    // STore Workspace Pointer: STWP src
    // Copy the workspace pointer to memory

    // Base cycles: 8
    // 2 memory accesses:
    //  Read instruction (already done)
    //  Write dest

    AddCycleCount(8);

    FormatVIII_0;
    WRWORD(D,WP);   // write dest
}

void CPU9900::op_swpb()
{ 
    // SWaP Bytes: SWPB src
    // swap the high and low bytes of a word

    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1,x2;

    AddCycleCount(10);

    FormatVI;
    x1=ROMWORD(S);      // read source

    x2=((x1&0xff)<<8)|(x1>>8);
    WRWORD(S,x2);       // write source
}

void CPU9900::op_andi()
{ 
    // AND Immediate: ANDI src, imm

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read immediate
    //  Read dest
    //  Write dest

    Word x1,x2;

    AddCycleCount(14);

    FormatVIII_1;       // read immediate

    x1=ROMWORD(D);      // read dest
    x2=x1&S;
    WRWORD(D,x2);       // write dest
    
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x2]&mask_LGT_AGT_EQ;
}

void CPU9900::op_ori()
{ 
    // OR Immediate: ORI src, imm

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read immediate
    //  Read dest
    //  Write dest

    Word x1,x2;

    AddCycleCount(14);

    FormatVIII_1;   // read immediate

    x1=ROMWORD(D);  // read dest
    x2=x1|S;
    WRWORD(D,x2);   // write dest
  
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x2]&mask_LGT_AGT_EQ;
}

void CPU9900::op_xor()
{ 
    // eXclusive OR: XOR src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Word x1,x2,x3;

    AddCycleCount(14);

    FormatIII;
    x1=ROMWORD(S);  // read source

    fixD();
    x2=ROMWORD(D);  // read dest
  
    x3=x1^x2;
    WRWORD(D,x3);   // write dest
  
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x3]&mask_LGT_AGT_EQ;
}

void CPU9900::op_inv()
{ 
    // INVert: INV src

    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    Word x1;

    AddCycleCount(10);

    FormatVI;

    x1=ROMWORD(S);  // read source
    x1=~x1;
    WRWORD(S,x1);   // write source

    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;
}

void CPU9900::op_clr()
{ 
    // CLeaR: CLR src
    // sets word to 0

    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    AddCycleCount(10);

    FormatVI;
    ROMWORD(S, ACCESS_RMW);     // read source (wasted)
    WRWORD(S,0);                // write source
}

void CPU9900::op_seto()
{ 
    // SET to One: SETO src
    // sets word to 0xffff

    // Base cycles: 10
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    AddCycleCount(10);

    FormatVI;
    ROMWORD(S, ACCESS_RMW);     // read source (wasted)
    WRWORD(S,0xffff);           // write source
}

void CPU9900::op_soc()
{ 
    // Set Ones Corresponding: SOC src, dst
    // Essentially performs an OR - setting all the bits in dst that
    // are set in src

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Word x1,x2,x3;

    AddCycleCount(14);

    FormatI;
    x1=ROMWORD(S);      // read source

    fixD();
    x2=ROMWORD(D);      // read dest
    x3=x1|x2;
    WRWORD(D,x3);       // write dest
  
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x3]&mask_LGT_AGT_EQ;
}

void CPU9900::op_socb()
{ 
    // Set Ones Corresponding, Byte: SOCB src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Byte x1,x2,x3;
    Word xD;

    AddCycleCount(14);

    FormatI;
    x1=RCPUBYTE(S);     // read source

    fixD();
    xD=ROMWORD(D);      // read dest

    BYTE_OPERATION(x3=x1|x2);   // xD->x2, result->xD, uses D

    WRWORD(D,xD);       // write dest

    reset_LGT_AGT_EQ_OP;
    ST|=BStatusLookup[x3]&mask_LGT_AGT_EQ_OP;
}

void CPU9900::op_szc()
{ 
    // Set Zeros Corresponding: SZC src, dst
    // Zero all bits in dest that are zeroed in src

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest
    
    Word x1,x2,x3;

    AddCycleCount(14);

    FormatI;
    x1=ROMWORD(S);      // read source

    fixD();
    x2=ROMWORD(D);      // read dest
    x3=(~x1)&x2;
    WRWORD(D,x3);       // write dest
  
    reset_LGT_AGT_EQ;
    ST|=WStatusLookup[x3]&mask_LGT_AGT_EQ;
}

void CPU9900::op_szcb()
{ 
    // Set Zeros Corresponding, Byte: SZCB src, dst

    // Base cycles: 14
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Read dest
    //  Write dest

    Byte x1,x2,x3;
    Word xD;

    AddCycleCount(14);

    FormatI;
    x1=RCPUBYTE(S);     // read source

    fixD();
    xD=ROMWORD(D);      // read dest

    BYTE_OPERATION(x3=(~x1)&x2);   // xD->x2, result->xD, uses D

    WRWORD(D,xD);       // write dest

    reset_LGT_AGT_EQ_OP;
    ST|=BStatusLookup[x3]&mask_LGT_AGT_EQ_OP;
}

void CPU9900::op_sra()
{ 
    // Shift Right Arithmetic: SRA src, dst
    // For the shift instructions, a count of '0' means use the
    // value in register 0. If THAT is zero, the count is 16.
    // The arithmetic operations preserve the sign bit

    // Count != 0
    // Base cycles: 12 + 2*count
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    // Count = 0
    // Base cycles: 20 + 2*count (in R0 least significant nibble, 0=16)
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read R0
    //  Read source
    //  Write source

    Word x1,x3,x4; 
    int x2;
    
    AddCycleCount(12);  // base value

    FormatV;
    if (D==0)
    { 
        D=ROMWORD(WP) & 0xf;    // read R0
        if (D==0) D=16;
        AddCycleCount(8);
    }
    x1=ROMWORD(S);              // read source
    x4=x1&0x8000;
    x3=0;
  
    for (x2=0; x2<D; x2++)
    { 
        x3=x1&1;   /* save carry */
        x1=x1>>1;  /* shift once */
        x1=x1|x4;  /* extend sign bit */
    }
    WRWORD(S,x1);               // write source
  
    reset_EQ_LGT_AGT_C;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x3) set_C;

    AddCycleCount(2*D);
}

void CPU9900::op_srl()
{ 
    // Shift Right Logical: SRL src, dst
    // The logical shifts do not preserve the sign

    // Count != 0
    // Base cycles: 12 + 2*count
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    // Count = 0
    // Base cycles: 20 + 2*count (in R0 least significant nibble, 0=16)
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read R0
    //  Read source
    //  Write source

    Word x1,x3; 
    int x2;
    AddCycleCount(12);      // base value

    FormatV;
    if (D==0)
    { 
        D=ROMWORD(WP)&0xf;  // read R0
        if (D==0) D=16;
        AddCycleCount(8);
    }
    x1=ROMWORD(S);          // read source
    x3=0;
  
    for (x2=0; x2<D; x2++)
    { 
        x3=x1&1;
        x1=x1>>1;
    }
    WRWORD(S,x1);           // write source

    reset_EQ_LGT_AGT_C;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x3) set_C;

    AddCycleCount(2*D);
}

void CPU9900::op_sla()
{ 
    // Shift Left Arithmetic: SLA src, dst

    // Count != 0
    // Base cycles: 12 + 2*count
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    // Count = 0
    // Base cycles: 20 + 2*count (in R0 least significant nibble, 0=16)
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read R0
    //  Read source
    //  Write source

    Word x1,x3,x4; 
    int x2;
    AddCycleCount(12);      // base value

    FormatV;
    if (D==0)
    { 
        D=ROMWORD(WP)&0xf;  // read R0
        if (D==0) D=16;
        AddCycleCount(8);
    }
    x1=ROMWORD(S);          // read source
    x4=x1&0x8000;
    reset_EQ_LGT_AGT_C_OV;

    x3=0;
    for (x2=0; x2<D; x2++)
    { 
        x3=x1&0x8000;
        x1=x1<<1;
        if ((x1&0x8000)!=x4) set_OV;
    }
    WRWORD(S,x1);           // write source
  
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x3) set_C;

    AddCycleCount(2*D);
}

void CPU9900::op_src()
{ 
    // Shift Right Circular: SRC src, dst
    // Circular shifts pop bits off one end and onto the other
    // The carry bit is not a part of these shifts, but it set
    // as appropriate

    // Count != 0
    // Base cycles: 12 + 2*count
    // 3 memory accesses:
    //  Read instruction (already done)
    //  Read source
    //  Write source

    // Count = 0
    // Base cycles: 20 + 2*count (in R0 least significant nibble, 0=16)
    // 4 memory accesses:
    //  Read instruction (already done)
    //  Read R0
    //  Read source
    //  Write source

    Word x1,x4;
    int x2;
    AddCycleCount(12);      // base value

    FormatV;
    if (D==0)
    { 
        D=ROMWORD(WP)&0xf;  // read R0
        if (D==0) D=16;
        AddCycleCount(8);
    }
    x1=ROMWORD(S);          // read source
    for (x2=0; x2<D; x2++)
    { 
        x4=x1&0x1;
        x1=x1>>1;
        if (x4) 
        {
            x1=x1|0x8000;
        }
    }
    WRWORD(S,x1);           // write source
  
    reset_EQ_LGT_AGT_C;
    ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ;

    if (x4) set_C;

    AddCycleCount(2*D);
}

void CPU9900::op_bad()
{ 
    char buf[128];

    // Base cycles: 6
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(6);

    FormatVII;
    sprintf(buf, "Illegal opcode (%04X)", in);
    warn(buf);                  // Don't know this Opcode
    SwitchToThread();           // these have a habit of taking over the emulator in crash situations :)
    if (BreakOnIllegal) TriggerBreakPoint();
}

////////////////////////////////////////////////////////////////////////
// debug only opcodes, enabled with enableDebugOpcodes
// Since these are technically illegal opcodes, they will take the same
// 6 cycles here so that running on hardware has comparable performance.

void CPU9900::op_norm() {
    // Base cycles: 6
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(6);
    FormatVII;

    debug_write("CODE triggered CPU normal at PC >%04X", GetPC());
	SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_NORMAL, 0);
}

void CPU9900::op_ovrd() {
    // Base cycles: 6
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(6);
    FormatVII;

    debug_write("CODE triggered overdrive at PC >%04X", GetPC());
	SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_CPUOVERDRIVE, 0);
}

void CPU9900::op_smax() {
    // Base cycles: 6
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(6);
    FormatVII;

    debug_write("CODE triggered system maximum at PC >%04X", GetPC());
	SendMessage(myWnd, WM_COMMAND, ID_CPUTHROTTLING_SYSTEMMAXIMUM, 0);
}

void CPU9900::op_brk() {
    // Base cycles: 6
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(6);
    FormatVII;

    debug_write("CODE triggered breakpoint at PC >%04X.", GetPC());
    TriggerBreakPoint(true);
}

void CPU9900::op_quit() {
    // Base cycles: 6
    // 1 memory accesses:
    //  Read instruction (already done)
    AddCycleCount(6);
    FormatVII;

    debug_write("CODE executed QUIT opcode at PC >%04X.", GetPC());
    PostMessage(myWnd, WM_QUIT, 0, 0);
}

void CPU9900::op_dbg() {
    // dbg is technically an illegal opcode followed by a JMP, so
    // it's 6+10 = 16 cycles. The Classic99 part is free ;)

    // Base cycles: 16
    // 2 memory accesses:
    //  Read dbg instruction (already done)
    //  Read jmp instruction (emulation skips this and reads the argument instead, should be the same timing)
    AddCycleCount(16);
    ADDPC(2);           // skip over the JMP instruction
    FormatVIII_1;       // for-cost read the argument into S (matches timing of hardware which reads the JMP instead)
                        // also gets the WR address into D
    char buf[128+6];
    strcpy(buf, "CODE: ");
    // verify the heck out of this string...
    // first, size
    int size = 0;
    for (int idx=0; idx<128; ++idx) {
        if (S+idx > 0xffff) {
            break;
        }
        buf[idx+6] = GetSafeCpuByte(S+idx,xbBank);
        if (buf[idx+6] == 0) {
            size = idx+6;
            break;
        }
    }
    if (size == 0) {
        debug_write("Can't write debug from >%04X because string not NUL terminated.", GetPC()-4);
        return;
    }
    // now make sure there's no more than one percent symbol, ignoring %%
    int pos = -1;
    for (int idx=0; idx<size; ++idx) {
        if (buf[idx] != '%') continue;
        if (buf[idx+1] == '%') {
            // double percent is okay
            ++idx;
            continue;
        }
        if (pos > -1) {
            debug_write("Can't write debug from >%04X because too many formats", GetPC()-4);
            return;
        }
        pos = idx;
    }

    // see if it's a type we trust to print an int
    if (pos >  -1) {
        // legal characters in a specifier are %.- numbers and letters
        while ((buf[pos])&&((isalnum(buf[pos]))||(strchr("%.-",buf[pos])))) {
            if (buf[pos] == 'l') {
                debug_write("Can't write debug from >%04X because long modifier specified", GetPC()-4);
                return;
            }
            ++pos;
        }
        --pos;
        if (NULL == strchr("nuxXc", buf[pos])) {
            debug_write("Can't write debug from >%04X because untrusted type '%c'", GetPC()-4, buf[pos]);
            return;
        }
    }

    // all right, that should be good
    debug_write(buf, GetSafeWord(D, xbBank));
}

////////////////////////////////////////////////////////////////////////
// functions that are different on the F18A
// (there will be more than just this!)
void CPU9900::op_idleF18() {
    // GPU goes to sleep
    // In this broken implementation, we switch context back to the host CPU
    // TODO: do this properly.
    FormatVII;
    debug_write("GPU Encountered IDLE, switching back to CPU");
    StartIdle();
    if (!bInterleaveGPU) {
        pCurrentCPU = pCPU;
    }
    //AddCycleCount(??);
}

void CPU9900::op_callF18() {
    Word x2;

    // 0C80 call    00001 100 1x Ts SSSS
    // CALL <gas> = (R15) <= PC , R15 <= R15 - 2 , PC <= gas

    FormatVI;
    x2=ROMWORD(WP+30);      // get R15
    if (0 == GetReturnAddress()) {
        SetReturnAddress(PC);
    }
    WRWORD(x2,PC);

    x2-=2;
    WRWORD(WP+30, x2);      // update R15

    SetPC(S);

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_retF18(){
    Word x1;

    // 0C00 ret     00001 100 0x xx xxxx
    // RET        = R15 <= R15 + 2 , PC <= (R15)    
    FormatVII;

    x1=ROMWORD(WP+30);      // get R15
    x1+=2;
    WRWORD(WP+30, x1);      // update R15
    SetPC(ROMWORD(x1));     // get PC   TODO: is the F18A GPU PC also 15 bits, or 16?

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_pushF18(){
    Word x1,x2;

    // Push the word on the stack
    // 0D00 push    00001 101 0x Ts SSSS
    // PUSH <gas> = (R15) <= (gas) , R15 <= R15 - 2

    FormatVI;
    x1=ROMWORD(S);
    x2=ROMWORD(WP+30);      // get R15
    WRWORD(x2, x1);

    x2-=2;
    WRWORD(WP+30, x2);      // update R15

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_popF18(){
    Word x1,x2;

    // POP the word from the stack
    // the stack pointer post-decrements (per Matthew)
    // so here we pre-increment!

    // 0F00 pop     00001 111 0x Td DDDD
    // POP  <gad> = R15 <= R15 + 2 , (gad) <= (R15)

    FormatVI;               // S is really D in this one...
    x2=ROMWORD(WP+30);      // get R15
    x2+=2;
    WRWORD(WP+30, x2);      // update R15

    x1=ROMWORD(x2);
    WRWORD(S, x1);

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_slcF18(){
    // TODO: this one seems misdefined? It only has a source address, and no count??
    // Wasn't it removed from the final??

    Word x1,x2;

    // 0E00 slc     00001 110 0x Ts SSSS

    FormatVI;
    x1=ROMWORD(S);

    // circular left shift (TODO: once? does it rotate through carry??)
    x2=x1&0x8000;
    x1<<=1;
    if (x2) x1|=0x0001;
    WRWORD(S, x1);

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_pixF18(){
    // PIX is a funny instruction. It has a huge candy-machine interface that works with
    // Bitmap mode, with the new bitmap overlay, and it can perform logic operations. It's
    // almost a mini-blitter.

    Word x1,x2,ad;

    FormatIX;
    D=WP+(D<<1);

    // SRC = XXXXXXXX YYYYYYYY
    // Command bits in destination:
    // Format: MAxxRWCE xxOOxxPP
    // M  - 1 = calculate the effective address for GM2 instead of the new bitmap layer (todo: so do nothing else??)         
    //      0 = use the remainder of the bits for the new bitmap layer pixels
    // A  - 1 = retrieve the pixel's effective address instead of setting a pixel   (todo: so do nothing else??)       
    //      0 = read or set a pixel according to the other bits
    // R  - 1 = read current pixel into PP, only after possibly writing PP         
    //      0 = do not read current pixel into PP
    // W  - 1 = do not write PP         
    //      0 = write PP to current pixel
    // C  - 1 = compare OO with PP according to E, and write PP only if true         
    //      0 = always write
    // E  - 1 = only write PP if current pixel is equal to OO         
    //      0 = only write PP if current pixel is not equal to OO
    // OO   pixel to compare to existing pixel
    // PP   new pixel to write, and previous pixel when reading
    //
    // The destination parameter is the PIX instruction as indicated above.  
    // If you use the M or A operations, the destination register will contain the address 
    // after the instruction has executed.  If you use the R operation, the read pixel will 
    // be in PP (over writes the LSbits).  You can read and write at the same time, in which 
    // case the PP bits are written first and then replaced with the original pixel bits
    //
    
    x1=ROMWORD(S);
    x2=ROMWORD(D);

    if (x2 & 0x8000) {
        // calculate BM2 address:
        // 00PYYYYY00000YYY +
        //     0000XXXXX000
        // ------------------
        // 00PY YYYY XXXX XYYY
        //
        // Note: Bitmap GM2 address /includes/ the offset from VR4 (pattern table), so to use
        // it for both pattern and color tables, put the pattern table at >0000
        ad = ((VDPREG[4]&0x04) ? 0x2000 : 0) |          // P
             ((x1&0x00F8) << 5) |                       // YYYYY
             ((x1&0xF800) >> 8) |                       // XXXXX
             ((x1&0x0007));                             // YYY
    } else {
        // calculate overlay address -- I don't have the math for this.
        // TODO: Is it chunky or planar? I assume chunky, 2 bits per pixel, linear.
        // TODO: I don't have the reference in front of me to know what registers do what (size, start address, etc)
        // so.. do this later.
        ad = 0;     // todo: put actual math in place
    }

    // only parse the other bits if M and A are zero
    if ((x2 & 0xc000) == 0) {
        // everything in here thus assumes overlay mode and the pixel is at AD.

        unsigned char pix = RCPUBYTE(ad);   // get the byte
        unsigned char orig = pix;           // save it
        // TODO: if we are 2 bits per pixel, there is still masking to do??
        pix &= 0x03;        // TODO: this is wrong, get the correct pixel into the LSb's
        bool bComp = (pix == ((x2&0x0030)>>4));     // compare the pixels
        unsigned char newpix = x2&0x0003;           // new pixel
        bool bWrite = (x2&0x0400)!=0;               // whether to write

        // TODO: are C and E dependent on W being set? I am assuming yes.
        if ((bWrite)&&(x2&0x0200)) {                // C - compare active (only important if we are writing anyway?)
            if (x2&0x0100) {
                // E is set, comparison must be true
                if (!bComp) bWrite=false;
            } else {
                // E is clear, comparison must be false
                if (bComp) bWrite=false;
            }
        }

        if (bWrite) {
            // TODO: properly merge the pixel (newpix) back in to orig
            WCPUBYTE(ad, (orig&0xfc) | newpix);
        }
        if (x2 & 0x0800) {
            // read is set, so save the original read pixel color in PP
            x2=(x2&0xFFFC) | pix;
            WRWORD(D, x2);          // write it back
        }
    } else {
        // user only wants the address
        WRWORD(D, ad);
    }

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_csonF18(){
    // chip select to the EEPROM
    FormatVII;
    
    spi_flash_enable(true);

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_csoffF18(){
    // chip select to the EEPROM off (TODO)
    FormatVII;
    
    spi_flash_enable(false);

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_spioutF18(){
    // based on LDCR
    // The count is ignored, this is always an 8-bit byte operation
    Byte x1;

    // force the bit count to be 8, no matter what it really was
    // This is needed so the FormatIV instruction correctly interprets
    // this as a byte operation in all cases.
    in = (in&(~0x03c0)) | (8<<6);

    FormatIV;
    x1=RCPUBYTE(S);

    // Always 8 bits
    spi_write_data(x1, 8);

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_spiinF18(){
    // based on STCR
    // The count is ignored, this is always an 8-bit byte operation
    Byte x1;

    // force the bit count to be 8, no matter what it really was
    // This is needed so the FormatIV instruction correctly interprets
    // this as a byte operation in all cases.
    in = (in&(~0x03c0)) | (8<<6);

    FormatIV;
    
    // Always 8 bits
    x1 = spi_read_data(8);
    WCPUBYTE(S,(Byte)(x1&0xff));  

    // TODO: does it affect any status flags??
    //reset_EQ_LGT_AGT_C_OV;
    //ST|=WStatusLookup[x1]&mask_LGT_AGT_EQ_OV;

    //AddCycleCount(??);
}

void CPU9900::op_rtwpF18(){
    // Almost the same. Used by interrupt code only. Does not touch R13 as there is no WP.
    // ReTurn with Workspace Pointer: RTWP
    // TODO: what interrupt code? what am I doing here?

    FormatVII;
    ST=ROMWORD(WP+30);
    SetPC(ROMWORD(WP+28));

    //AddCycleCount(??);        // TODO: 
}

////////////////////////////////////////////////////////////////////////
// Fill the CPU Opcode Address table
// WARNING: called more than once, so be careful about anything you can't do twice!
////////////////////////////////////////////////////////////////////////
void CPU9900::buildcpu()
{
    Word in,x,z;
    unsigned int i;

    for (i=0; i<65536; i++)
    { 
        in=(Word)i;

        x=(in&0xf000)>>12;
        switch(x)
        { 
        case 0: opcode0(in);        break;
        case 1: opcode1(in);        break;
        case 2: opcode2(in);        break;
        case 3: opcode3(in);        break;
        case 4: opcode[in]=&CPU9900::op_szc;    break;
        case 5: opcode[in]=&CPU9900::op_szcb; break;
        case 6: opcode[in]=&CPU9900::op_s;  break;
        case 7: opcode[in]=&CPU9900::op_sb; break;
        case 8: opcode[in]=&CPU9900::op_c;  break;
        case 9: opcode[in]=&CPU9900::op_cb; break;
        case 10:opcode[in]=&CPU9900::op_a;  break;
        case 11:opcode[in]=&CPU9900::op_ab; break;
        case 12:opcode[in]=&CPU9900::op_mov;    break;
        case 13:opcode[in]=&CPU9900::op_movb; break;
        case 14:opcode[in]=&CPU9900::op_soc;    break;
        case 15:opcode[in]=&CPU9900::op_socb; break;
        default: opcode[in]=&CPU9900::op_bad;
        }
    } 

    // check for special debug opcodes
    if (enableDebugOpcodes) {
        for (int idx = 0x0110; idx < 0x0130; ++idx) {
            if (idx == 0x115) idx=0x120;    // skip unused ones

            if (opcode[idx] != &CPU9900::op_bad) {
                debug_write("===============================");
                debug_write("Opcode %X is already assigned, can't use for debug! Programmer error.", idx);
                debug_write("===============================");
                exit(-1);
            }
        }

        opcode[0x0110] = &CPU9900::op_norm;
        opcode[0x0111] = &CPU9900::op_ovrd;
        opcode[0x0112] = &CPU9900::op_smax;
        opcode[0x0113] = &CPU9900::op_brk;
        opcode[0x0114] = &CPU9900::op_quit;
        for (int idx=0x120; idx<=0x12f; ++idx) {
            opcode[idx] = &CPU9900::op_dbg;
        }
    }

    // build the Word status lookup table
    for (i=0; i<65536; i++) {
        WStatusLookup[i]=0;
        // LGT
        if (i>0) WStatusLookup[i]|=BIT_LGT;
        // AGT
        if ((i>0)&&(i<0x8000)) WStatusLookup[i]|=BIT_AGT;
        // EQ
        if (i==0) WStatusLookup[i]|=BIT_EQ;
        // C
        if (i==0) WStatusLookup[i]|=BIT_C;
        // OV
        if (i==0x8000) WStatusLookup[i]|=BIT_OV;
    }
    // And byte
    for (i=0; i<256; i++) {
        Byte x=(Byte)(i&0xff);
        BStatusLookup[i]=0;
        // LGT
        if (i>0) BStatusLookup[i]|=BIT_LGT;
        // AGT
        if ((i>0)&&(i<0x80)) BStatusLookup[i]|=BIT_AGT;
        // EQ
        if (i==0) BStatusLookup[i]|=BIT_EQ;
        // C
        if (i==0) BStatusLookup[i]|=BIT_C;
        // OV
        if (i==0x80) BStatusLookup[i]|=BIT_OV;
        // OP
        for (z=0; x; x&=(x-1)) z++;                     // black magic?
        if (z&1) BStatusLookup[i]|=BIT_OP;              // set bit if an odd number
    }

    nCycleCount = 0;
}

///////////////////////////////////////////////////////////////////////////
// CPU Opcode 0 helper function
///////////////////////////////////////////////////////////////////////////
void CPU9900::opcode0(Word in)
{
    Word x;

    x=(in&0x0f00)>>8;

    switch(x)
    { 
    case 2: opcode02(in);       break;
    case 3: opcode03(in);       break;
    case 4: opcode04(in);       break;
    case 5: opcode05(in);       break;
    case 6: opcode06(in);       break;
    case 7: opcode07(in);       break;
    case 8: opcode[in]=&CPU9900::op_sra;    break;
    case 9: opcode[in]=&CPU9900::op_srl;    break;
    case 10:opcode[in]=&CPU9900::op_sla;    break;
    case 11:opcode[in]=&CPU9900::op_src;    break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

////////////////////////////////////////////////////////////////////////////
// CPU Opcode 02 helper function
////////////////////////////////////////////////////////////////////////////
void CPU9900::opcode02(Word in)
{ 
    Word x;

    x=(in&0x00e0)>>4;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_li; break;
    case 2: opcode[in]=&CPU9900::op_ai; break;
    case 4: opcode[in]=&CPU9900::op_andi; break;
    case 6: opcode[in]=&CPU9900::op_ori;    break;
    case 8: opcode[in]=&CPU9900::op_ci; break;
    case 10:opcode[in]=&CPU9900::op_stwp; break;
    case 12:opcode[in]=&CPU9900::op_stst; break;
    case 14:opcode[in]=&CPU9900::op_lwpi; break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

////////////////////////////////////////////////////////////////////////////
// CPU Opcode 03 helper function
////////////////////////////////////////////////////////////////////////////
void CPU9900::opcode03(Word in)
{ 
    Word x;

    x=(in&0x00e0)>>4;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_limi; break;
    case 4: opcode[in]=&CPU9900::op_idle; break;
    case 6: opcode[in]=&CPU9900::op_rset; break;
    case 8: opcode[in]=&CPU9900::op_rtwp; break;
    case 10:opcode[in]=&CPU9900::op_ckon; break;
    case 12:opcode[in]=&CPU9900::op_ckof; break;
    case 14:opcode[in]=&CPU9900::op_lrex; break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

///////////////////////////////////////////////////////////////////////////
// CPU Opcode 04 helper function
///////////////////////////////////////////////////////////////////////////
void CPU9900::opcode04(Word in)
{ 
    Word x;

    x=(in&0x00c0)>>4;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_blwp; break;
    case 4: opcode[in]=&CPU9900::op_b;  break;
    case 8: opcode[in]=&CPU9900::op_x;  break;
    case 12:opcode[in]=&CPU9900::op_clr;    break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

//////////////////////////////////////////////////////////////////////////
// CPU Opcode 05 helper function
//////////////////////////////////////////////////////////////////////////
void CPU9900::opcode05(Word in)
{ 
    Word x;

    x=(in&0x00c0)>>4;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_neg;    break;
    case 4: opcode[in]=&CPU9900::op_inv;    break;
    case 8: opcode[in]=&CPU9900::op_inc;    break;
    case 12:opcode[in]=&CPU9900::op_inct; break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

////////////////////////////////////////////////////////////////////////
// CPU Opcode 06 helper function
////////////////////////////////////////////////////////////////////////
void CPU9900::opcode06(Word in)
{ 
    Word x;

    x=(in&0x00c0)>>4;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_dec;    break;
    case 4: opcode[in]=&CPU9900::op_dect; break;
    case 8: opcode[in]=&CPU9900::op_bl; break;
    case 12:opcode[in]=&CPU9900::op_swpb; break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

////////////////////////////////////////////////////////////////////////
// CPU Opcode 07 helper function
////////////////////////////////////////////////////////////////////////
void CPU9900::opcode07(Word in)
{ 
    Word x;

    x=(in&0x00c0)>>4;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_seto; break;
    case 4: opcode[in]=&CPU9900::op_abs;    break;
    default: opcode[in]=&CPU9900::op_bad;
    }   
}

////////////////////////////////////////////////////////////////////////
// CPU Opcode 1 helper function
////////////////////////////////////////////////////////////////////////
void CPU9900::opcode1(Word in)
{ 
    Word x;

    x=(in&0x0f00)>>8;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_jmp;    break;
    case 1: opcode[in]=&CPU9900::op_jlt;    break;
    case 2: opcode[in]=&CPU9900::op_jle;    break;
    case 3: opcode[in]=&CPU9900::op_jeq;    break;
    case 4: opcode[in]=&CPU9900::op_jhe;    break;
    case 5: opcode[in]=&CPU9900::op_jgt;    break;
    case 6: opcode[in]=&CPU9900::op_jne;    break;
    case 7: opcode[in]=&CPU9900::op_jnc;    break;
    case 8: opcode[in]=&CPU9900::op_joc;    break;
    case 9: opcode[in]=&CPU9900::op_jno;    break;
    case 10:opcode[in]=&CPU9900::op_jl; break;
    case 11:opcode[in]=&CPU9900::op_jh; break;
    case 12:opcode[in]=&CPU9900::op_jop;    break;
    case 13:opcode[in]=&CPU9900::op_sbo;    break;
    case 14:opcode[in]=&CPU9900::op_sbz;    break;
    case 15:opcode[in]=&CPU9900::op_tb; break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

////////////////////////////////////////////////////////////////////////
// CPU Opcode 2 helper function
////////////////////////////////////////////////////////////////////////
void CPU9900::opcode2(Word in)
{ 
    Word x;

    x=(in&0x0c00)>>8;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_coc; break;
    case 4: opcode[in]=&CPU9900::op_czc; break;
    case 8: opcode[in]=&CPU9900::op_xor; break;
    case 12:opcode[in]=&CPU9900::op_xop; break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

////////////////////////////////////////////////////////////////////////
// CPU Opcode 3 helper function
////////////////////////////////////////////////////////////////////////
void CPU9900::opcode3(Word in)
{ 
    Word x;

    x=(in&0x0c00)>>8;

    switch(x)
    { 
    case 0: opcode[in]=&CPU9900::op_ldcr; break;
    case 4: opcode[in]=&CPU9900::op_stcr; break;
    case 8: opcode[in]=&CPU9900::op_mpy;    break;
    case 12:opcode[in]=&CPU9900::op_div;    break;
    default: opcode[in]=&CPU9900::op_bad;
    }
}

///// F18A implementation/override class

// TODO: the only speed rating we have is 150-200nS per instruction on average. Actual
// timing information is not currently available. This is probably good
// enough for a rough start.
// Some details that are available:
// 100MHz clock
// jump takes 5 clocks
// instructions with 2 symbolic addresses take 20 clocks
//
// Stack operations use R15 as the stack pointer - always a word operation on EVEN address (so top is >47FE)
//
// the GPU auto-starts on reset (on VDP reset!) after loading the bitstream from EPROM, which pre-sets all RAM.
// TODO: I need a dump of the bitstream from Matthew to include, and I need a load routine and to start the GPU.
// 
// TODO: Disassembler needs to know about the changed opcodes

GPUF18A::GPUF18A() {
    // build default 9900
    buildcpu();
    pType="F18A";
}

void GPUF18A::buildcpu() {
    CPU9900::buildcpu();

    // override with F18A replacements

    // new opcodes
    // CALL 0C80 - 0000 1100 10Ts SSSS
    for (int idx=0x0C80; idx<=0x0CBF; idx++) {
        opcode[idx] = &CPU9900::op_callF18;
    }

    // RET  0C00 - 0000 1100 0000 0000
    opcode[0x0c00] = &CPU9900::op_retF18;

    // PUSH 0D00 - 0000 1101 00Ts SSSS
    for (int idx=0x0D00; idx<=0x0D3F; idx++) {
        opcode[idx]=&CPU9900::op_pushF18;
    }

    // POP  0F00 - 0000 1111 00Td DDDD
    for (int idx=0x0F00; idx<=0x0f3F; idx++) {
        opcode[idx]=&CPU9900::op_popF18;
    }

    // SLC  0E00 - 0000 1110 00Ts SSSS
    for (int idx=0x0E00; idx<=0x0E3F; idx++) {
        opcode[idx]=&CPU9900::op_slcF18;
    }

    // Modified opcodes
    
    // IDLE = IDLE     Forces the GPU state machine to the idle state, restart with a trigger from host
    opcode[0x0340] = &CPU9900::op_idleF18;

    //TODO: be smart about these later
    for (int idx=0; idx<0xffff; idx++) {
        // XOP  = PIX       The new dedicated pixel plotting instruction
        if (opcode[idx] == &CPU9900::op_xop) opcode[idx]=&CPU9900::op_pixF18;

        // CKON = SPI !CE Sets the chip enable line to the SPI Flash ROM low (enables the ROM)
        if (opcode[idx] == &CPU9900::op_ckon) opcode[idx]=&CPU9900::op_csonF18;

        // CKOF = SPI CE  Sets the chip enable line to the SPI Flash ROM high (disables the ROM)
        if (opcode[idx] == &CPU9900::op_ckof) opcode[idx]=&CPU9900::op_csoffF18;

        // LDCR = SPI OUT Writes a byte (always a byte operation) to the SPI Flash ROM
        if (opcode[idx] == &CPU9900::op_ldcr) opcode[idx]=&CPU9900::op_spioutF18;

        // STCR = SPI IN  Reads a byte (always a byte operation) from the SPI Flash ROM
        if (opcode[idx] == &CPU9900::op_stcr) opcode[idx]=&CPU9900::op_spiinF18;

        // RTWP = RTWP     Modified, does not use R13, only performs R14->PC, R15->status flags
        if (opcode[idx] == &CPU9900::op_rtwp) opcode[idx]=&CPU9900::op_rtwpF18;

        // Unimplemented
        if (opcode[idx] == &CPU9900::op_sbo) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_sbz) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_tb) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_blwp) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_stwp) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_lwpi) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_limi) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_rset) opcode[idx]=&CPU9900::op_bad;
        if (opcode[idx] == &CPU9900::op_lrex) opcode[idx]=&CPU9900::op_bad;
    }
}

void GPUF18A::reset() {
    StartIdle();
    nReturnAddress=0;

    SetWP(0xff80);          // just a dummy, out of the way place for them. F18A doesn't have a WP
    SetPC(0);               // it doesn't run automatically, either (todo: but it is supposed to!)
    X_flag=0;               // not currently executing an X instruction (todo: does it have X?)
    ST=(ST&0xfff0);         // disable interrupts (todo: does it have interrupts?)
    SetCycleCount(26);      // not that it's a big deal, but that's how long reset takes ;)

    // TODO: GPU /does/ autostart, but we have to load the bitstream from eeprom first (so we need a stopidle - not the startidle above)
}

// There are no side-effects to reading anything from the F18A,
// so we won't bother implementing the word/rbw actions
// There are WRITE side effects, but till we have the registers
// implemented it doesn't matter.
// TODO: does the F18 /actually/ implement word-only access or can it do TRUE bytes? word accesses are aligned, bytes are true bytes
// TODO: do reads differ from writes in that respect? no
// TODO: do F18 writes perform a read-before-write in any case? (does it matter? there are no side-effects. Just timing.)

// NOTE: GPU accessing VDP registers or VDP RAM is slow compared to the palette registers..
Byte GPUF18A::RCPUBYTE(Word src) {
    UpdateHeatVDP(src);     // todo: maybe GPU vdp writes can be a different color

    // map the regisgers.
    // TODO: what happens when these values are read as words?
    switch ((src&0xf000)>>12) {
    case 0:
    case 1:
    case 2:
    case 3:
        //   -- VRAM 14-bit, 16K @ >0000 to >3FFF (0011 1111 1111 1111)
        // standard 16k VDP RAM - no mapping
        break;

    case 4:
        // 4k GPU RAM
        //   -- GRAM 11-bit, 2K  @ >4000 to >47FF (0100 x111 1111 1111)
        // Mirrored twice in 8k space
        src&=0xf7ff;
        break;

    case 5:
        // 16-bit color registers
        //   -- PRAM  7-bit, 128 @ >5000 to >5x7F (0101 xxxx x111 1111)
        // Mirrored numerous times
        src &= 0xf07f;
        break;

    case 6:
        // 8-bit VDP registers
        //   -- VREG  6-bit, 64  @ >6000 to >6x3F (0110 xxxx xx11 1111)
        // TODO: what happens on 16-bit read?
        src&=0xf03f;
        break;

    case 7:
        // current scanline in even byte
        // blanking bit in odd byte
        //   -- current scanline @ >7000 to >7xx0 (0111 xxxx xxxx xxx0) --
        //   -- blanking         @ >7001 to >7xx1 (0111 xxxx xxxx xxx1) --
        src&=0xf001;
        break;

    case 8:
        //   -- DMA              @ >8000 to >8xx7 (1000 xxxx xxxx 0111) --
        // TODO: not implemented
        src&=0xf00f;
        break;

    case 0x0a:
        //   -- F18A version     @ >A000 to >Axxx (1010 xxxx xxxx xxxx) --
        return 0x18;

    case 0x0b:
        //   -- GPU status data  @ >B000 to >Bxxx (1011 xxxx xxxx xxxx) --
        // TODO: is this right - is GDATA still supported?
        if (pGPU->GetIdle() == 0) {
            return 0x80 | VDP[0xb000];
        } else {
            return VDP[0xb000];
        }

    case 0xf:
        // registers are at >ff80, so just let these through
        // >FF80 to >FF9F - GPU registers R0-R15 (Classic99 hack, I think!)
        break;

    default:
        // ignoring the rest for now
        return 0;
    }

    return VDP[src];
}

void GPUF18A::WCPUBYTE(Word dest, Byte c) {
    // map the registers.
    // TODO: what happens when these values are written as words? I think it should work
    switch ((dest&0xf000)>>12) {
    case 0:
    case 1:
    case 2:
    case 3:
        //   -- VRAM 14-bit, 16K @ >0000 to >3FFF (0011 1111 1111 1111)
        // standard 16k VDP RAM - no mapping
        break;

    case 4:
        // 4k GPU RAM
        //   -- GRAM 11-bit, 2K  @ >4000 to >47FF (0100 x111 1111 1111)
        // Mirrored twice in 8k space
        dest&=0xf7ff;
        break;

    case 5:
        {
            // 16-bit color registers
            //   -- PRAM  7-bit, 128 @ >5000 to >5x7F (0101 xxxx x111 1111)
            // Mirrored numerous times
            dest &= 0xf07f;
            // we do this write first, because the registers are 16-bit,
            // and so we read the resulting value back out in the next bit
            VDP[dest&0xf07f] = c;

            // write VDP palette
            int reg = (dest&0x7f)/2;

            // the palette register is doubled up, so we need to deal with that
            int r=(VDP[dest&0xf07e] & 0x0f);
            int g=(VDP[(dest&0xf07e)+1] & 0xf0)>>4;
            int b=(VDP[(dest&0xf07e)+1] & 0x0f);
            F18APalette[reg] = (r<<20)|(r<<16)|(g<<12)|(g<<8)|(b<<4)|b; // double up each palette gun, suggestion by Sometimes99er
            redraw_needed = REDRAW_LINES;
        }
        break;

    case 6:
        // 8-bit VDP registers
        //   -- VREG  6-bit, 64  @ >6000 to >6x3F (0110 xxxx xx11 1111)
        // write VDP register
        wVDPreg(dest&0x3f,c);
        return;

    case 7:
        // current scanline in even byte
        // blanking bit in odd byte
        //   -- current scanline @ >7000 to >7xx0 (0111 xxxx xxxx xxx0) --
        //   -- blanking         @ >7001 to >7xx1 (0111 xxxx xxxx xxx1) --
        // not writable?
        return;

    case 8:
        //   -- DMA              @ >8000 to >8xx7 (1000 xxxx xxxx 0111) --
        // TODO: not implemented
        dest&=0xf00f;
        break;

    case 0x0a:
        //   -- F18A version     @ >A000 to >Axxx (1010 xxxx xxxx xxxx) --
        // not writable
        return;

    case 0x0b:
        //   -- GPU status data  @ >B000 to >Bxxx (1011 xxxx xxxx xxxx) --
        // TODO: is this how we write GDATA?
        VDP[0xb000] = c&0x7f;
        return;

    case 0xf:
        // registers are at >ff80, so just let these through
        // >FF80 to >FF9F - GPU registers R0-R15 (Classic99 hack, I think!)
        break;

    default:
        // ignoring the rest
        return;
    }

    UpdateHeatVDP(dest);        // todo: maybe GPU vdp writes can be a different color
    VDP[dest]=c;
    VDPMemInited[dest]=1;
    if (dest < 0x4000) redraw_needed=REDRAW_LINES;      // to avoid redrawing because of GPU R0-R15 registers changing
}

Word GPUF18A::ROMWORD(Word src, READACCESSTYPE rmw=ACCESS_READ) {
    (void)rmw;
    src&=0xfffe;
    return (RCPUBYTE(src)<<8) | RCPUBYTE(src+1);
//  return (VDP[(src)]<<8) | VDP[(src+1)];
} 

void GPUF18A::WRWORD(Word dest, Word val) {
    dest&=0xfffe;
    WCPUBYTE(dest, val>>8);
    WCPUBYTE(dest+1, val&0xff);
//  VDP[dest]= val>>8;
//  VDP[dest+1]= val&0xff;
}

Word GPUF18A::GetSafeWord(int x, int) {
    // bank is irrelevant
    return ROMWORD(x);
}

// Read a byte withOUT triggering the hardware - for monitoring
Byte GPUF18A::GetSafeByte(int x, int) {
    // bank is irrelevant
    return RCPUBYTE(x);
}

void GPUF18A::TriggerInterrupt(Word /*vector*/, Byte /*level*/) {
    // do nothing, there are no external interrupts
}
