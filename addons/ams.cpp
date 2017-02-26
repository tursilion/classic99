/* Code by Joe Delekto! Thanks Joe! */
/* AMS support for Classic99 */

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <windows.h>

#include "tiemul.h"
#include "ams.h"

// define sizes for memory arrays
static const int MaxMapperPages	= 0x1000;
static const int MaxPageSize	= 0x1000;		// TODO: wait.. this is 16MB, but we only support a maximum of 1MB. Why so much data?

// define sizes for register arrays
static const int MaxMapperRegisters = 0x10;

static EmulationMode emulationMode = None;		// Default to no emulation mode
static AmsMemorySize memorySize = Mem128k;		// Default to 128k AMS Card
static MapperMode mapperMode = Map;				// Default to mapping addresses

static Word mapperRegisters[MaxMapperRegisters] = { 0 };
static Byte systemMemory[MaxMapperPages * MaxPageSize] = { 0 };
Byte staticCPU[0x10000] = { 0 };				// 64k for the base memory

static bool mapperRegistersEnabled = false;

// references into C99
extern bool bWarmBoot;

void InitializeMemorySystem(EmulationMode cardMode)
{
	// Classic99 specific debug call
	debug_write("Initializing AMS mode %d, size %dk", cardMode, 128*(1<<memorySize));

	// 1. Save chosen card mode
	emulationMode = cardMode;

	// 3. Set up initial register values for pass-through and map modes with default page numbers
	for (int reg = 0; reg < MaxMapperRegisters; reg++)
	{
		// TODO: this is wrong, it should be zeroed, but right now the rest of the
		// emulation relies on these values being set. Which probably means that
		// the passthrough settings are also wrong. ;)
		mapperRegisters[reg] = (reg << 8);
	}

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
void WriteMapperRegisterByte(Byte reg, Byte value, bool highByte)
{
	reg &= 0x0F;

	if (mapperRegistersEnabled)
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
				debug_write("AMS Register %X now >%04X", reg, mapperRegisters[reg]);
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

Byte ReadMemoryByte(Word address, bool bTrueAccess)
{
	DWord wMask = 0x0000FF00;  // TODO: set for appropriate memory size	
	DWord pageBase = ((DWord)address & 0x00000FFF);
	DWord pageOffset = ((DWord)address & 0x0000F000) >> 12;
	DWord pageExtension = pageOffset;

	bool bIsMapMode = (mapperMode == Map);
	bool bIsRAM = (!ROMMAP[address]) && (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));
	bool bIsMappable = (((pageOffset >= 0x2) && (pageOffset <= 0x3)) || ((pageOffset >= 0xA) && (pageOffset <= 0xF)));

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
		pageExtension = (DWord)((mapperRegisters[pageOffset] & wMask) >> 8);
	}
	
	DWord mappedAddress = (pageExtension << 12) | pageBase;

	if (bIsRAM || bIsMappable)
	{
		return systemMemory[mappedAddress];
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

	if (bIsMapMode && bIsMappable)
	{			
		pageExtension = (DWord)((mapperRegisters[pageOffset] & wMask) >> 8);
	}
	
	DWord mappedAddress = (pageExtension << 12) | pageBase;

	if (bIsMapMode && bIsMappable)
	{
		systemMemory[mappedAddress] = value;			
	}
	else
	{
		if (bIsRAM || bIsMappable)
		{
			CPUMemInited[mappedAddress&0xffff] = 1;
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
		*(data + memIndex) = ReadMemoryByte((address + memIndex) & 0xFFFF);
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