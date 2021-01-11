#ifndef AMS_H
#define AMS_H

/* 
Code by Joe Delekto! Thanks Joe!
AMS support for Classic99

NOTES:

SAMS Uses:
>2000 to >3FFF

SAMS/AMS Uses:
>A000 to >FFFF

CRU Address of Card: >1E00

CRU Bit 0:
	0 - Can't Change Mapper Registers
	1 - Can Change Mapper Registers

CRU Bit 1:
	0 - Pass Mode (Normal 9900 Memory Pages)
	1 - Map Mode (Use Value in Mapper Register)

When >1E00 CRU Bit 1 is Set to 1, map a portion of the 12 bits in the page to the following base addresses

>4000 - >0000 (unused)
>4002 - >1000 (unused)
>4004 - >2000 (available for SAMS/TAMS)
>4006 - >3000 (available for SAMS/TAMS)
>4008 - >4000 (unused)
>400A - >5000 (unused)
>400C - >6000 (unused)
>400E - >7000 (unused)
>4010 - >8000 (unused)
>4012 - >9000 (unused)
>4014 - >A000 (available)
>4016 - >B000 (available)
>4018 - >C000 (available)
>401A - >D000 (available)
>401C - >E000 (available)
>401E - >F000 (available)
*/

enum EmulationMode
{
	None,		// Original TI Mode (>0000 through >FFFF straight)
	Ams,		// Original AMS (also known as AEMS) (not currently supported)
	Sams,		// SouthWest 99er's Super AMS
	Tams		// Thierry Nouspikel's (or Total) AMS (not currently supported)
};

enum AmsMemorySize
{
	Mem128k,	// 128k AMS/SAMS Card
	Mem256k,	// 256k AMS/SAMS Card
	Mem512k,	// 512k AMS/SAMS Card
	Mem1024k	// 1024k AMS/SAMS Card
};

enum MapperMode
{
	Passthrough,		// Passthrough (Normal mapped >0000 through >FFFF 64k Address space)
	Map 				// Use value from Mapper register to extend the 12 LSB
};

/* AMS/SAMS initialization and shutdown */
void InitializeMemorySystem(EmulationMode cardMode);
void SetAmsMemorySize(AmsMemorySize size);
EmulationMode MemoryEmulationMode();
void ShutdownMemorySystem();

/* Enable Mapper Mode, SAMS and Register Modification */
void SetMemoryMapperMode(MapperMode mode);
void EnableMapperRegisters(bool enable);
bool MapperRegistersEnabled();

/* Read/Write a value to the mapper register */
void WriteMapperRegisterByte(Byte reg, Byte value, bool highByte, bool force);
Byte ReadMapperRegisterByte(Byte reg, bool highByte);

/* Read/Write a single byte to AMS/SAMS memory */
Byte ReadRawAMS(int address);
Byte ReadMemoryByte(Word address, READACCESSTYPE rmw = ACCESS_READ);
void WriteMemoryByte(Word address, Byte value, bool allowWrite);

/* Read/Write a block of data to AMS/SAMS memory */
Byte* ReadMemoryBlock(Word address, void* vData, Word length);
Byte* WriteMemoryBlock(Word address, void* vData, Word length);

/* state management - simple for now */
void RestoreAMS(unsigned char *pData, int nLen);
void PreloadAMS(unsigned char *pData, int nLen);

#endif // AMS_H
