//
// Classic99 <-> pico9918-core bridge
//
// (C) 2026 Troy Schrapel (visrealm)
//
// pico9918-core (https://github.com/visrealm/pico9918-core) is the TMS9918A /
// F18A / PICO9918 emulation the PICO9918 firmware itself runs. This bridge lets
// Classic99 run that core in place of console/tivdp.cpp.
//
// This file is Classic99's, and follows Classic99's license. pico9918-core
// itself is MIT and lives under 3rdparty/pico9918-core.
//

#ifndef TIVDP_PICO9918_H
#define TIVDP_PICO9918_H

// a whole VGA field - borders, blanking and the CRT dim are in the picture
#define P9918_WIDTH  640
#define P9918_HEIGHT 480

// Video -> Engine. Read at cold reset; ini video/UsePico9918.
extern int bUsePico9918;

// Video -> VDP. Read at cold reset; ini video/VdpChip.
//
// Classic99's own numbers - they are in every existing classic99.ini, so they
// never move. Mapped to pico9918_chip_t in coreChip() rather than cast.
#define P9918_CHIP_TMS9918A     0
#define P9918_CHIP_F18A         1
#define P9918_CHIP_PICO9918     2
#define P9918_CHIP_TMS9918      3   // pre-A: no Graphics II
#define P9918_CHIP_PICO9918_PRO 4   // RP2350 board: 8bpp 80 column text
#define P9918_CHIP_COUNT        5
extern int nVdpChip;

// Fold an out-of-range or engine-incompatible selection back to something the
// running engine can be. The reset is what puts bF18Enabled with it.
void p9918ReconcileChip();

// True when the settings above no longer describe what is running.
bool p9918SettingsPending();

// P9918_WIDTH*P9918_HEIGHT of 32-bit 0RGB, bottom-up. NULL until engaged.
extern unsigned int *p9918framedata;

bool p9918Active();

// Engage or disengage to match bUsePico9918, and reset the core. isCold picks
// a power-on reset over the F18A's VR50 soft reset. Engaging re-points VDP[] at
// the core's own VRAM, which is laid out as the F18A's - the map Classic99 uses.
void p9918Reset(bool isCold);

void p9918Shutdown();

// Persist the settings block if the guest changed it. Cheap when it did not.
void p9918FlushConfig();

// The host bus - >8C02 / >8C00 and >8802 / >8800.
// (unsigned char rather than Byte, so this header stands on its own)
void p9918WriteAddr(unsigned char c);
void p9918WriteData(unsigned char c);
unsigned char p9918ReadStatus();
unsigned char p9918ReadData();

// A register write from outside the guest's stream - the TIPI loader. Goes
// through the bus, so it takes the unlock gate and the aliasing with it.
// The two-byte latch is per instance, so only call this with the guest at rest.
void p9918WriteReg(unsigned char r, unsigned char v);

// One emulated scanline, from updateVDP().
void p9918Scanline();

// Classic99 paces the GPU, not the library, so the two interleave per 9900
// instruction rather than a scanline's worth at a time.
void p9918GpuCycles(int cycles);
void p9918GpuCredit();
bool p9918CpuStepDue();

// Copy the core's register file, status and unlock latch into VDPREG[], VDPS
// and bF18AActive.
void p9918SyncShadow();

// Debugger half - tivdp_pico9918_debug.cpp, the integration's only user of the
// core's debugger surface.
void          p9918SyncLayers();
void          p9918SyncGpuDebug();
void          p9918ServiceGpuBreak();
bool          p9918StepGpu();
bool          p9918ArmGpuStepOver();
void          p9918DbgWriteMem(unsigned int addr, unsigned char v);
void          p9918DbgWriteReg(unsigned char r, unsigned char v);
unsigned char p9918DbgStatus(unsigned char reg);
unsigned int  p9918DbgGpuPC();
bool          p9918DbgGpuArmed();
unsigned int  p9918DbgGpuReg(unsigned char reg);
unsigned int  p9918DbgGpuStatus();

// The /INT pin, for VDPINT. An F18A has a second source - the scanline
// interrupt in SR1 - that the SR0 shadow cannot show.
bool p9918InterruptPin();

#endif
