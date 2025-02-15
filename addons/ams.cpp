/* Code by Joe Delekto! Thanks Joe! */
/* AMS support for Classic99 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <windows.h>

#include "..\console\tiemul.h"
#include "ams.h"

// temporary hack - might be right, needs testing
// This will enable 32MB AMS (or whatever is configured below!)
// NOTE: AMSTEST4 can't handle this, we need a config option
#define ENABLE_HUGE_AMS 

// TODO: need to make the AMS emulation more closely resemble real
// hardware - so no more pre-init, and handle the bytes written
// properly based on the size of memory (ie: one byte or two? Which
// byte, first or second? etc).
//
// Original cards, then, used a 74LS612. What do the new cards use?
// I think it's still a 74LS612...
// 
// Notes from Ksarul:
// The original AMS and SAMS cards only used one byte of the 12-bit address space, making the four high-order bits 
// invisible to programs and the users. In order to get the results they needed, they had to write the address byte
// twice to ensure that the address they wanted was populating the registers. The first write was shifted to the 
// high-order register when the low order byte was written in--but it had no function as there were no outputs tied
// to the four high-order bits. This is a requirement of the 74LS612, as it MUST have both bytes written to it for 
// it to work. The original design reflected this limitation of the hardware--it is definitely NOT a bug, it is 
// a feature that programmers have to wrap their minds around to get the card to respond properly.
//
// Expansion to 4M uses two of those high-order bits, and expansion to 16M would use all four, so that high-order 
// byte now actually DOES something when it is shifted into the registers. Tursi's variant actually uses one 
// additional high-order bit that isn't available on the 74LS612 to expand the space to 32M. 
//
// Rich, I have a thought: when writing to set the registers on a 1M SAMS, if you write zeroes in the first 
// byte sent (the high-order byte), it should not try to switch you to bizarro pages that don't exist, as now 
// there won't be spurious ones in the high-order byte. Same goes for the high-order byte in larger cards, always 
// make sure unused bits are set to zero in the first byte sent to set the registers. Following this convention 
// at all times should allow just a single SAMS management routine (set up a variable that identifies card size 
// and mask all necessary initial bits to zero, which will also eliminate potential issues with cards smaller 
// than 1M too).
//
// Note from FALCOR4: With the 1M and smaller cards, when you execute a register write, the LSbyte is written to 
// the LS612 but when the TI next writes the MSByte it overwrites the LSbyte that it just wrote which makes the 
// LSbyte of the WORD irrelevant.  That's why it really doesn't matter what you have in the LSbyte of a WORD that 
// you write to the SAMS card if it is 1M or smaller.  However, it does matter with the 4M card (and any other 
// larger increments that may come along), which has a separate latch that captures the LSbyte of a WORD that is 
// written to the registers.  That's why it's recommended that software assumes a larger card and to be fully 
// compatible with any SAMS.

// define sizes for memory arrays 
static const int MaxMapperPages	= 0x2000;       // MUST be a power of 2
static const int MaxPageSize	= 0x1000;		// TODO: wait.. this is 32MB, but we only support a maximum of 1MB. Why so much data?

// define sizes for register arrays
static const int MaxMapperRegisters = 0x10;

static EmulationMode emulationMode = None;		// Default to no emulation mode
static AmsMemorySize memorySize = Mem128k;		// Default to 128k AMS Card
static MapperMode mapperMode = Map;				// Default to mapping addresses

Word mapperRegisters[MaxMapperRegisters] = { 0 };
static Byte systemMemory[MaxMapperPages * MaxPageSize] = { 0 };
Byte staticCPU[0x10000] = { 0 };				// 64k for the base memory

static bool mapperRegistersEnabled = false;

// references into C99
extern bool bWarmBoot;

// breakpoints
extern struct _break BreakPoints[];
extern int nBreakPoints;

void InitializeMemorySystem(EmulationMode cardMode)
{
	// Classic99 specific debug call
#ifdef ENABLE_HUGE_AMS
	debug_write("Initializing AMS mode %d, size %dk", cardMode, MaxMapperPages*4096);
#else
    // it's hard-coded below to 1MB no matter what the rest says...
	debug_write("Initializing AMS mode %d, size %dk", cardMode, 256*4096);
#endif

	// 1. Save chosen card mode
	emulationMode = cardMode;

	// 3. Set up initial register values for pass-through and map modes with default page numbers
	for (int reg = 0; reg < MaxMapperRegisters; reg++)
	{
		// TODO: this is wrong, it should be zeroed, but right now the rest of the
		// emulation relies on these values being set. Which probably means that
		// the passthrough settings are also wrong. ;)
        // TODO: make an ini option to force the register config on startup when the zeros are in
        mapperRegisters[reg] = (reg << 8);
	}

#ifdef ENABLE_HUGE_AMS
        // todo: this is also wrong, it's for testing Ralph's project
//        mapperRegisters[10] = 0;    // map first page of AMS to 0xA000
#endif

	if (!bWarmBoot) {
		memrnd(systemMemory, sizeof(systemMemory));
		memrnd(staticCPU, sizeof(staticCPU));
	}
}

void SetAmsMemorySize(AmsMemorySize size)
{
	//debug_write("Set AMS size to %d", size);
	memorySize = size;
}

EmulationMode MemoryEmulationMode()
{
	return emulationMode;
}

void ShutdownMemorySystem()
{
	// 3. Set up initial register values for pass-through and map modes with default page numbers
	for (int reg = 0; reg < MaxMapperRegisters; reg++)
	{
		mapperRegisters[reg] = (reg << 8);
	}

	memrnd(systemMemory, sizeof(systemMemory));
	memrnd(staticCPU, sizeof(staticCPU));
}

void SetMemoryMapperMode(MapperMode mode)
{
	switch (emulationMode)
	{
	case Ams:
	case Sams:
	case Tams:
		mapperMode = mode;
		//debug_write("Set AMS mapper mode to %d", mode);
		break;

	case None:
	default:
		mapperMode = Passthrough;
		//debug_write("Set AMS mapper mode to PASSTHROUGH");
	}
}

// TODO: placeholder function - dump the registers to the debug log for now
void dumpMapperRegisters() {
    debug_write("AMS Mappers:");
    for (int idx=0; idx<MaxMapperRegisters; ++idx) {
        // TODO: all this addressing BS needs to be in a separate function too... ;) Copied from the read function...
	    DWord wMask = 0x0000FF00;  // TODO: set for appropriate memory size	
	    DWord pageExtension;

        #ifdef ENABLE_HUGE_AMS
            wMask = MaxMapperPages-1;
            // use all 16 bits, but byte swapped (for compatibility)
            DWORD value = ((mapperRegisters[idx]&0xff)<<8)|((mapperRegisters[idx]&0xff00)>>8);
            // mask it down to only the actually valid bits
            pageExtension = (value & wMask);
        #else
            // regular AMS is just the upper byte - wMask of 0xff gives 256x4k or 1MB
		    pageExtension = (DWord)((mapperRegisters[idx] & wMask) >> 8);
        #endif
	
        bool isRam = ((idx >= 2)&&(idx<=3)) || ((idx>=10)&&(idx<=15));
        debug_write("%X: >%04X (%04X -> %06X) %c", idx, mapperRegisters[idx], 0x1000*idx, pageExtension<<12, isRam ? ' ' : '*');
    }
    debug_write("(* = not mappable)");
}

void EnableMapperRegisters(bool enabled)
{
	switch (emulationMode)
	{
	case Ams:
	case Sams:
	case Tams:
		mapperRegistersEnabled = enabled;
		//debug_write("AMS Mapper registers active");
		break;

	case None:
	default:
		mapperRegistersEnabled = false;
		//debug_write("AMS Mapper registers disabled");
	}
}

bool MapperRegistersEnabled()
{
	return mapperRegistersEnabled;
}

// Only call if mapper registers enabled. Reg must be 0-F
void WriteMapperRegisterByte(Byte reg, Byte value, bool highByte, bool force)
{
	reg &= 0x0F;

	if ((mapperRegistersEnabled)||(force))
	{
		switch (emulationMode)
		{
			case Ams:
			case Sams:  /* only for now */
			case Tams:				
				if (highByte)
				{
					mapperRegisters[reg] = ((mapperRegisters[reg] & 0x00FF) | (value << 8));
				}
				else
				{
					mapperRegisters[reg] = ((mapperRegisters[reg] & 0xFF00) | value);
				}
				//debug_write("AMS Register %X now >%04X", reg, mapperRegisters[reg]);
				break;

			case None:
				// fallthrough
			default:
				break;
		}
	}
}

// Only call if mapper registers enabled. Reg must be 0-F
Byte ReadMapperRegisterByte(Byte reg, bool highByte)
{
	reg &= 0x0F;

	if (mapperRegistersEnabled)
	{
		switch (emulationMode)
		{
			case Ams:
			case Sams:
			case Tams:
				if (highByte)
				{					
					return (Byte)((mapperRegisters[reg] & 0xFF00) >> 8);
				}
				return (Byte)(mapperRegisters[reg] & 0x00FF);

			case None:
				break;

			default:
				break;
		}
	}

	// If registers not enabled or not emulating, just return what's at the register address
	//debug_write("Reading disabled AMS registers!");
	Word address = ((0x4000 + (reg << 1)) + (highByte ? 0 : 1));

	return ReadMemoryByte(address);
}

// raw access to the array for the debugger
Byte ReadRawAMS(int address) {
    address &= 0xfffff;     // TODO: assumes 1MB limit
    return systemMemory[address];
}
void WriteRawAMS(int address, int value) {
    address &= 0xfffff;     // TODO: assumes 1MB limit
    systemMemory[address] = value&0xff;
}

Byte ReadMemoryByte(Word address, READACCESSTYPE rmw)
{
	DWord wMask = 0x0000FF00;  // TODO: set for appropriate memory size	
	DWord pageBase = ((DWord)address & 0x00000FFF);
	DWord pageOffset = ((DWord)address & 0x0000F000) >> 12;
	DWord pageExtension = pageOffset;

    bool bTrueAccess = (rmw == ACCESS_READ);
	bool bIsMapMode = (mapperMode == Map);
	bool bIsRAM = (!ROMMAP[address]) && (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));
	bool bIsMappable = (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));

#ifdef ENABLE_HUGE_AMS
    wMask = MaxMapperPages-1;
#endif

#if 0
	// little hack to dump AMS memory for making loader files
	// we do a quick RLE to save some memory. Step debugger in
	// to execute this code
	if (0) {
		FILE *fp=fopen("C:\\amsdump.rle", "wb");
		int nRun=0;
		for (int i=0; i<MaxMapperPages * MaxPageSize; i++) {
			if (systemMemory[i] == 0) {
				nRun++;
				if (nRun == 255) {
					fputc(0, fp);
					fputc(nRun, fp);
					nRun=0;
				}
			} else {
				if (nRun > 0) {
					fputc(0, fp);
					fputc(nRun, fp);
					nRun=0;
				}
				fputc(systemMemory[i], fp);
			}
		}
		fclose(fp);
	}
#endif

	if (bIsMapMode && bIsMappable)
	{
#ifdef ENABLE_HUGE_AMS
        // use all 16 bits, but byte swapped (for compatibility)
        DWORD value = ((mapperRegisters[pageOffset]&0xff)<<8)|((mapperRegisters[pageOffset]&0xff00)>>8);
        // mask it down to only the actually valid bits
        pageExtension = (value & wMask);
#else
        // regular AMS is just the upper byte - wMask of 0xff gives 256x4k or 1MB
		pageExtension = (DWord)((mapperRegisters[pageOffset] & wMask) >> 8);
#endif
	}
	
	DWord mappedAddress = (pageExtension << 12) | pageBase;

	if (bIsRAM && bIsMappable)
	{
        if (mappedAddress <= sizeof(systemMemory)) {
		    if (bTrueAccess) {
			    // Check for breakpoints
			    for (int idx=0; idx<nBreakPoints; idx++) {
				    switch (BreakPoints[idx].Type) {
					    case BREAK_READAMS:
						    if (CheckRange(idx, mappedAddress)) {
	    						TriggerBreakPoint();
						    }
						    break;
				    }
			    }
		    }
            return systemMemory[mappedAddress];
        } else {
            debug_write("AMS is asking for out of range memory...");
            return 0;
        }
	}

	// this only works with the console RAM, not the AMS RAM
	if ((g_bCheckUninit) && (bTrueAccess) && (0 == CPUMemInited[mappedAddress]) && (0 == ROMMAP[mappedAddress])) {
		TriggerBreakPoint();
		char buf[128];
		sprintf(buf, "Breakpoint - reading uninitialized CPU memory at >%04X", mappedAddress);
		MessageBox(myWnd, buf, "Classic99 Debugger", MB_OK);
	}
	return staticCPU[mappedAddress];
}

// allowWrite = do the write, even if it is ROM! Otherwise only if it is RAM.
void WriteMemoryByte(Word address, Byte value, bool allowWrite)
{
	DWord wMask = 0x0000FF00;  // TODO: set for appropriate memory size	
	DWord pageBase = ((DWord)address & 0x00000FFF);
	DWord pageOffset = ((DWord)address & 0x0000F000) >> 12;
	DWord pageExtension = pageOffset;

	bool bIsMapMode = (mapperMode == Map);
	bool bIsRAM = (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));
	bool bIsMappable = (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));

#ifdef ENABLE_HUGE_AMS
    wMask = MaxMapperPages-1;
#endif

	if (bIsMapMode && bIsMappable)
	{			
#ifdef ENABLE_HUGE_AMS
        // use all 16 bits, but byte swapped (for compatibility)
        DWORD value = ((mapperRegisters[pageOffset]&0xff)<<8)|((mapperRegisters[pageOffset]&0xff00)>>8);
        // mask it down to only the actually valid bits
        pageExtension = (value & wMask);
#else
        // regular AMS is just the upper byte - wMask of 0xff gives 256x4k or 1MB
		pageExtension = (DWord)((mapperRegisters[pageOffset] & wMask) >> 8);
#endif
	}
	
	DWord mappedAddress = (pageExtension << 12) | pageBase;

	if (bIsMapMode && bIsMappable)
	{
        if (mappedAddress <= sizeof(systemMemory)) {
		    // duplicated code, refactor this...
		    for (int idx=0; idx<nBreakPoints; idx++) {
			    switch (BreakPoints[idx].Type) {
				    case BREAK_EQUALS_AMS:
					    if ((CheckRange(idx, mappedAddress)) && ((value&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
    						TriggerBreakPoint();
					    }
					    break;

				    case BREAK_WRITEAMS:
					    if (CheckRange(idx, mappedAddress)) {
    						TriggerBreakPoint();
					    }
					    break;
			    }
		    }
    		systemMemory[mappedAddress] = value;
        } else {
            debug_write("AMS is writing out of range memory...");
            return;
        }
	}
	else
	{
		if (bIsRAM || bIsMappable)
		{
            if ((mappedAddress&0xffff)!=mappedAddress) debug_write("Unexpected AMS memory write, continuing.");
			CPUMemInited[mappedAddress&0xffff] = 1;
		    // duplicated code, refactor this...
		    for (int idx=0; idx<nBreakPoints; idx++) {
			    switch (BreakPoints[idx].Type) {
				    case BREAK_EQUALS_AMS:
					    if ((CheckRange(idx, mappedAddress)) && ((value&BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
    						TriggerBreakPoint();
					    }
					    break;

				    case BREAK_WRITEAMS:
					    if (CheckRange(idx, mappedAddress)) {
    						TriggerBreakPoint();
					    }
					    break;
			    }
		    }
			systemMemory[mappedAddress] = value;
		}
		else if (allowWrite || (!ROMMAP[mappedAddress]))
		{
			CPUMemInited[mappedAddress] = 1;
			staticCPU[mappedAddress] = value;
		}		
	}
}

Byte* ReadMemoryBlock(Word address, void* vData, Word length)
{
	Byte* data = (Byte*)vData;

	for (Word memIndex = 0; memIndex < length; memIndex++)
	{
		*(data + memIndex) = ReadMemoryByte((address + memIndex) & 0xFFFF, ACCESS_FREE);
	}

    return data;
}

Byte* WriteMemoryBlock(Word address, void* vData, Word length)
{
	Byte* data = (Byte*)vData;

	for (Word memIndex = 0; memIndex < length; memIndex++)
	{
		WriteMemoryByte((address + memIndex) & 0xFFFF, *(data + memIndex), true);
	}

    return data;
}

void RestoreAMS(unsigned char *pData, int nLen) {
	// little hack to restore AMS memory for automatic loader files
	// we do a quick RLE to save some memory. This is meant only for
	// use by the built-in cartridge loader.
	int i=0;
	while ((nLen>0) && (i < MaxMapperPages * MaxPageSize)) {
		if (*pData == 0) {
			if (nLen < 2) {
				warn("RLE decode error.");
				systemMemory[i++]=0;
				return;
			}
			pData++;
			for (int nRun=0; nRun<*pData; nRun++) {
				systemMemory[i++]=0;
				if (i >= MaxMapperPages * MaxPageSize) {
					warn("RLE exceeded AMS memory size.");
					return;
				}
			}
			nLen-=2;
		} else {
			systemMemory[i++]=*pData;
			nLen--;
		}
		pData++;
	}
}

void PreloadAMS(unsigned char *pData, int nLen) {
    // another hack, but doesn't do RLE. Not sure this will have long term use
    if (nLen > sizeof(systemMemory)) nLen = sizeof(systemMemory);
    memcpy(systemMemory, pData, nLen);
}

