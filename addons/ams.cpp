/* Code by Joe Delekto! Thanks Joe! */
/* AMS support for Classic99 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <windows.h>

#include "..\console\tiemul.h"
#include "ams.h"

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
//
// Note from Rich that explains the "default" mapping being needed. When mapping is off, it automatically
// uses the first 16 pages:
//
// RXB does not use pages 0 to 15 as stated way back in 2000 these would be used for future expansions.
// Pages 2,3,A,B,C,D,E,F are used in PASS mode or MAP mode so are just used for normal 32 Memory thus left alone.
// Pages 0,1,4,5,6,7,8,9 are totally ignored.
// Now RXB 2020 on you can use these pages but in Documents it states that is not a good idea.
// I do believe devices like TIPI and some others do use some of these pages like 0 and 1 but not entirely sure this is the case.


// define sizes for memory arrays 
static int MaxMapperPages	    = 256;      // MUST be a power of 2
static const int MaxPageSize	= 0x1000;	// 4k

// define sizes for register arrays
static const int MaxMapperRegisters = 0x10;

static MapperMode mapperMode = Passthrough;		// Default to not mapping addresses
Word mapperRegisters[MaxMapperRegisters] = { 0 };
Byte *systemMemory;                             // can be huge, so malloc it
Byte staticCPU[0x10000] = { 0 };	            // 64k memory map, holds ROMs and scratchpad, all else is in AMS systemMemory

static bool mapperRegistersEnabled = false;
static bool bWarnedMapper = false;

// references into C99
extern bool bWarmBoot;

// breakpoints
extern struct _break BreakPoints[];
extern int nBreakPoints;

bool InitializeMemorySystem(int sams_pages)
{
    // if you get stupid, you get 1MB
    // nobody has tested over 32MB (8192 pages)
    // the full size of 65536 would be 262MB
    if (sams_pages > 65536) sams_pages=256;

	// Classic99 specific debug call
	debug_write("Initializing AMS size %dk", (MaxMapperPages*4096)/1024);
    if (MaxMapperPages > 256) {
        debug_write("WARNING: Classic software may not work with AMS >1MB");
    }

    if (NULL != systemMemory) {
        // in case the size changed, reallocate - we should get the same block back otherwise
        free(systemMemory);
    }
    MaxMapperPages = sams_pages;
    if (MaxMapperPages == 0) {
        // allocate 64k - TODO: someday, configure this on/off too
        systemMemory = (Byte*)malloc(64*1024);
    } else {
        systemMemory = (Byte*)malloc(MaxMapperPages * MaxPageSize);
    }
    if (NULL == systemMemory) {
        debug_write("Unable to allocate %dk for system memory, abort!", MaxMapperPages * MaxPageSize / 1024);
        return false;
    }
    memset(systemMemory, 0, MaxMapperPages * MaxPageSize);

	// 3. Set up initial register values for pass-through and map modes with default page numbers
	for (int reg = 0; reg < MaxMapperRegisters; reg++)
	{
        // TODO: make an ini option to force the register config on startup when the zeros are in
        mapperRegisters[reg] = 0;
	}

	if (!bWarmBoot) {
		memrnd(systemMemory, MaxMapperPages * MaxPageSize);
	}

    // re-enable the warning on reset
    bWarnedMapper = false;
    return true;
}

void ShutdownMemorySystem()
{
	// 3. Set up initial register values for pass-through and map modes with default page numbers
	for (int reg = 0; reg < MaxMapperRegisters; reg++)
	{
		mapperRegisters[reg] = (reg << 8);
	}

	free(systemMemory);
    systemMemory = NULL;
}

void SetMemoryMapperMode(MapperMode mode)
{
    if (MaxMapperPages == 0) {
        mapperMode = Passthrough;
        return;
    }
	mapperMode = mode;
	//debug_write("Set AMS mapper mode to %s (%d)", mode==Map ? "Map":"Passthrough", mode);
}

// TODO: placeholder function - dump the registers to the debug log for now
void dumpMapperRegisters() {
    debug_write("AMS Mappers:");
    for (int idx=0; idx<MaxMapperRegisters; ++idx) {
	    DWord wMask = MaxMapperPages-1;
	    DWord pageExtension;

        if (MaxMapperPages > 256) {
            // use all 16 bits, but byte swapped (for compatibility)
            DWORD value = ((mapperRegisters[idx]&0xff)<<8)|((mapperRegisters[idx]&0xff00)>>8);
            // mask it down to only the actually valid bits
            pageExtension = (value & wMask);
        } else {
            // need the high byte of the value
            DWORD value = mapperRegisters[idx] & (wMask << 8);
            pageExtension = value >> 8;
        }
	
        bool isRam = ((idx >= 2)&&(idx<=3)) || ((idx>=10)&&(idx<=15));
        debug_write("%X: >%04X (%04X -> %06X) %c", idx, mapperRegisters[idx], 0x1000*idx, pageExtension<<12, isRam ? ' ' : '*');
    }
    debug_write("(* = not mappable)");
}

void EnableMapperRegisters(bool enabled)
{
    if (MaxMapperPages == 0) {
        mapperRegistersEnabled = false;
        return;
    }

    mapperRegistersEnabled = enabled;
	//debug_write("AMS Mapper registers %s", enabled?"activate":"deactivate");
}

bool MapperRegistersEnabled()
{
	return mapperRegistersEnabled;
}

// Only call if mapper registers enabled. Reg must be 0-F
void WriteMapperRegisterByte(Byte reg, Byte value, bool highByte, bool force)
{
    if (MaxMapperPages == 0) {
        return;
    }

	reg &= 0x0F;

	if ((mapperRegistersEnabled)||(force))
	{
		if (highByte)
		{
			mapperRegisters[reg] = ((mapperRegisters[reg] & 0x00FF) | (value << 8));
		}
		else
		{
			mapperRegisters[reg] = ((mapperRegisters[reg] & 0xFF00) | value);
		}
		//debug_write("AMS Register %X now >%04X", reg, mapperRegisters[reg]);
	}
}

// Only call if mapper registers enabled. Reg must be 0-F
Byte ReadMapperRegisterByte(Byte reg, bool highByte)
{
    if (MaxMapperPages == 0) {
        return 0;
    }

	reg &= 0x0F;

	if (mapperRegistersEnabled)
	{
		if (highByte)
		{					
			return (Byte)((mapperRegisters[reg] & 0xFF00) >> 8);
		}
		return (Byte)(mapperRegisters[reg] & 0x00FF);
	}

	// If registers not enabled just return what's at the register address
	debug_write("Reading disabled AMS registers!");
	Word address = ((0x4000 + (reg << 1)) + (highByte ? 0 : 1));
	return ReadMemoryByte(address);
}

// raw access to the array for the debugger
Byte ReadRawAMS(int address) {
    if (address >= MaxMapperPages * MaxPageSize) {
        return 0;
    }
    return systemMemory[address];
}
void WriteRawAMS(int address, int value) {
    if (address >= MaxMapperPages * MaxPageSize) {
        return;
    }
    systemMemory[address] = value&0xff;
}

Byte ReadMemoryByte(Word address, READACCESSTYPE rmw)
{
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
	DWord pageOffset = ((DWord)address & 0x0000F000) >> 12;
	bool bIsMappable = (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));
    bool bTrueAccess = (rmw == ACCESS_READ);

    if (!bIsMappable)  {
	    // TODO: this only works with the console ROM and scratchpad now, not the AMS RAM
	    if ((g_bCheckUninit) && (bTrueAccess) && (0 == CPUMemInited[address]) && (0 == ROMMAP[address])) {
		    TriggerBreakPoint();
		    char buf[128];
		    sprintf(buf, "Breakpoint - reading uninitialized CPU memory at >%04X", address);
		    MessageBox(myWnd, buf, "Classic99 Debugger", MB_OK);
	    }
	    return staticCPU[address];
    }
    if (MaxMapperPages == 0) {
        // emulate a 32k card instead
        return systemMemory[address];
    }

    DWord wMask = MaxMapperPages-1;
	DWord pageBase = ((DWord)address & 0x00000FFF);  // offset within 4k pages
	DWord pageExtension = pageOffset;                // default is the top nibble of the 16 bit address (non-mapped mode)
	bool bIsMapMode = (mapperMode == Map);
    // note we can't combine the staticCPU above into this because it will malfunction if those pages are mapped

    // If not map mode, then the "page extension" is simply the top nibble of the actual address
	if (bIsMapMode)
	{
        if (MaxMapperPages > 256) {
            // use all 16 bits, but byte swapped (for compatibility)
            DWORD value = ((mapperRegisters[pageOffset]&0xff)<<8)|((mapperRegisters[pageOffset]&0xff00)>>8);
            // mask it down to only the actually valid bits
            pageExtension = (value & wMask);
        } else {
            // regular AMS is just the upper byte - wMask of 0xff gives 256x4k or 1MB
            DWORD value = mapperRegisters[pageOffset] & (wMask<<8);
	    	pageExtension = value >> 8;
            // check for non-mirrors mapper bytes - that may not work on real hardware
            if (!bWarnedMapper) {
                if ((mapperRegisters[pageOffset]&0xff) != ((mapperRegisters[pageOffset]&0xff00)>>8)) {
                    // The need to write the same byte in both bytes of the word to the
                    // registers is likely caused by some cards having the mapper respond to
                    // both even and odd addresses. In that case, whichever byte is written
                    // last will take precedence. On the TI that would be the MSB, which is
                    // why the example code I got was using that. (On the Geneve, though,
                    // it'd end up being the LSB, which would make the desire to write the
                    // same byte twice make sense - works on both).
                    bWarnedMapper = true;
                    debug_write("Warning: Non-mirrored mapper writes may not work on cards <=1MB (reg %d 0x%04X)", pageOffset, mapperRegisters[pageOffset]);
                }
            }
        }
	}
	
    // reconstruct the new address - 12 bits from the original address and 4 or more bits from the address registers
	DWord mappedAddress = (pageExtension << 12) | pageBase;

    if (mappedAddress <= MaxMapperPages * MaxPageSize) {
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
        // this should never happen, we have a bug if so
        debug_write("AMS is asking for out of range memory...");
        return 0;
    }

}

// allowWrite = do the write, even if it is ROM! Otherwise only if it is RAM.
void WriteMemoryByte(Word address, Byte value, bool allowWrite)
{
	DWord pageOffset = ((DWord)address & 0x0000F000) >> 12;
	bool bIsMappable = (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));

    // if it's not managed by the AMS, use staticCPU array
    if (!bIsMappable) {
		if (allowWrite || (!ROMMAP[address]))
		{
			CPUMemInited[address] = 1;
			staticCPU[address] = value;
		}
        return;
    }
    if (MaxMapperPages == 0) {
        // emulate a 32k card instead
        CPUMemInited[address] = 1;
        systemMemory[address] = value&0xff;
        return;
    }

    DWord wMask = MaxMapperPages-1;
	DWord pageBase = ((DWord)address & 0x00000FFF);
	DWord pageExtension = pageOffset;
	bool bIsMapMode = (mapperMode == Map);

	if (bIsMapMode)
	{			
        if (MaxMapperPages > 256) {
            // use all 16 bits, but byte swapped (for compatibility)
            DWORD value = ((mapperRegisters[pageOffset]&0xff)<<8)|((mapperRegisters[pageOffset]&0xff00)>>8);
            // mask it down to only the actually valid bits
            pageExtension = (value & wMask);
        } else {
            // regular AMS is just the upper byte - wMask of 0xff gives 256x4k or 1MB
            DWORD value = mapperRegisters[pageOffset] & (wMask<<8);
            pageExtension = value >> 8;
        }
	}
	
    // reconstruct the final address - 12 bits of base and 4 or more bits of mapper
	DWord mappedAddress = (pageExtension << 12) | pageBase;

    if (mappedAddress <= MaxMapperPages * MaxPageSize) {
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
        // this is an emulator bug if it happens
        debug_write("AMS is writing out of range memory...");
        return;
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
    if (nLen > MaxMapperPages * MaxPageSize) nLen = MaxMapperPages * MaxPageSize;
    memcpy(systemMemory, pData, nLen);
}

