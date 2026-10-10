//
// Classic99 <-> pico9918-core bridge
//
// (C) 2026 Troy Schrapel (visrealm)
//
// See tivdp_pico9918.h. pico9918-core is MIT licensed and lives under
// 3rdparty/pico9918-core; this file is Classic99's and follows Classic99's
// license.
//

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <windows.h>

#include "tiemul.h"
#include "cpu9900.h"
#include "tivdp_pico9918.h"

#include "pico9918.h"
#include "pico9918_frame.h"
#include "pico9918_util.h"
#include "pico9918_config.h"
#include "gpu/gpu.h"
#include "overlay/diag.h"

#include "pico9918_core_version.h"

extern struct _break BreakPoints[];
extern int nBreakPoints;
extern bool bIgnoreConsoleBreakpointHits;
extern CPU9900 * volatile pCurrentCPU;

extern int Scramble4kVDP(int addr);		// Tiemul.cpp
extern int cfg_cpf;						// Tiemul.cpp - with hzRate, the 9900's rate in Hz

extern int statusReadLine;
extern int statusReadCount;
extern int statusFrameCount;
extern bool statusUpdateRead;

int bUsePico9918 = 0;
int nVdpChip = P9918_CHIP_F18A;         // what Classic99 has always emulated
unsigned int *p9918framedata = NULL;

// the PICO9918_INST macros require this name; not static, the debug TU needs it
pico9918_t *tms9918 = NULL;

static bool engaged = false;
static Byte *pClassic99VDP = NULL;   // Classic99's own buffer, while we borrow VDP[]

static unsigned int gpuIps = 0;          // 0 when the chip has no GPU
static unsigned int gpuPerCycle = 0;     // 16.16 GPU instructions per 9900 cycle
static int gpuPerCycleRate = 0;          // the 9900 rate gpuPerCycle was derived from
static long long gpuBalance = 0;         // 16.16 GPU instructions run beyond the 9900's elapsed cycles
static int gpuLastCpuCycles = 14;        // what one 9900 instruction costs, for p9918CpuStepDue

// The core hands back BGR12; Classic99 blits 0RGB.
static unsigned int bgr12_0rgb[4096];

PICO9918_FRAME_ASSERT_GEOMETRY(P9918_WIDTH);
static PICO9918_FRAME_LINE_BUFFER(linebuf, P9918_WIDTH);

static pico9918_scanline_params_t scanParams;
static pico9918_frame_display_t   displayCfg;
static pico9918_frame_geometry_t  geometry;

// Classic99 stretches the cycles per line for 50Hz, so the field is always
// 262 calls long however fast they arrive.
#define P9918_MACHINE_LINES 262

static unsigned int linesPerCall = 1;
static unsigned int fieldLines   = P9918_MACHINE_LINES;
static unsigned int curLine      = 0;

//////////////////////////////////////////////////////////////
// The PICO9918 personality's settings block. A board keeps
// this in flash and the Configurator writes it, so it has to
// survive a run. PICO9918 only - an F18A has no config port.
//////////////////////////////////////////////////////////////
#define P9918_CONFIG_FILE ".\\PICO9918.cfg"

// The only thing about the block the library cannot know: the board revision as
// a packed nibble pair, v1.0 here, which the RP2040 model byte is derived from.
#define P9918_CONFIG_HW_VERSION 0x10

static bool configPending = false;      // guest changed it, not yet written
static int  runningChip = -1;           // the chip the last reset settled on

bool p9918Active() {
	return engaged;
}

bool p9918InterruptPin() {
	return pico9918_interrupt_status(tms9918);
}

static pico9918_chip_t coreChip(int chip) {
	switch (chip) {
		case P9918_CHIP_TMS9918:      return PICO9918_CHIP_TMS9918;
		case P9918_CHIP_TMS9918A:     return PICO9918_CHIP_TMS9918A;
		case P9918_CHIP_F18A:         return PICO9918_CHIP_F18A;
		case P9918_CHIP_PICO9918:     return PICO9918_CHIP_PICO9918;
		case P9918_CHIP_PICO9918_PRO: return PICO9918_CHIP_PICO9918_PRO;
	}
	return PICO9918_CHIP_F18A;
}

// the personalities with the config port and the overlays
static bool chipIsPico(int chip) {
	return (chip == P9918_CHIP_PICO9918) || (chip == P9918_CHIP_PICO9918_PRO);
}

void p9918ReconcileChip() {
	if ((nVdpChip < 0) || (nVdpChip >= P9918_CHIP_COUNT)) {
		nVdpChip = P9918_CHIP_F18A;
	}

	// Classic99's VDP has no config port or overlays, and always decodes M3.
	if (!bUsePico9918) {
		if (chipIsPico(nVdpChip))            nVdpChip = P9918_CHIP_F18A;
		if (nVdpChip == P9918_CHIP_TMS9918)  nVdpChip = P9918_CHIP_TMS9918A;
	}
}

// Both engines take the chip at the reset, so bF18Enabled moves with it there
// rather than when the menu is clicked.
static void latchChip() {
	runningChip = nVdpChip;
	bF18Enabled = ((nVdpChip == P9918_CHIP_F18A) || (chipIsPico(nVdpChip))) ? 1 : 0;
}

bool p9918SettingsPending() {
	if ((bUsePico9918 != 0) != engaged) {
		return true;
	}
	return nVdpChip != runningChip;
}

void p9918SyncShadow() {
	if (!engaged) return;

	memcpy(VDPREG, VDP+PICO9918_MAP_REGISTERS, sizeof(VDPREG));
	VDPS = pico9918_peek_status(tms9918);

	// R57's stored byte cannot answer this: the device unlocks on the value
	// arriving twice, so one write and two leave the same byte.
	const int unlocked = pico9918_unlocked(tms9918) ? 1 : 0;
	if (unlocked != bF18AActive) {
		bF18AActive = unlocked;
		debug_write("PICO9918: F18A enhanced registers %s.", unlocked ? "unlocked" : "locked");
	}
}

// Through the core's own transform, so nibble order lives in one place.
static void buildPixelMap() {
	for (unsigned int v = 0; v < 4096; ++v) {
		bgr12_0rgb[v] = pico9918_pixel_rgb888((PICO9918_PIXEL_T)v);
	}
}

// Both legacy writers of F18APalette[] are bypassed under the core. A pram
// entry is byte-swapped RGB444, 0xGB0R; F18APalette is 0xRRGGBB, nibbles doubled.
static void syncPalette() {
	const unsigned short *pram = (const unsigned short *)(VDP + PICO9918_MAP_PRAM);

	for (int i = 0; i < 64; ++i) {
		unsigned short p = pram[i];
		int r = (p      ) & 0x0f;
		int b = (p >>  8) & 0x0f;
		int g = (p >> 12) & 0x0f;
		F18APalette[i] = (r<<20)|(r<<16)|(g<<12)|(g<<8)|(b<<4)|b;
	}
}

static void configureFrame() {
	scanParams.hVirtualPixels       = P9918_WIDTH;
	scanParams.interlaced           = false;
	scanParams.interlacedFieldOrder = 0;

	displayCfg.displayPixels  = P9918_HEIGHT;
	displayCfg.interlaced     = false;
	displayCfg.vPixelScale    = 2;
	displayCfg.vVirtualPixels = P9918_HEIGHT / 2;

	geometry = pico9918_frame_geometry(tms9918, &displayCfg);

	// two output rows per rendered line normally, one each under double-rows
	linesPerCall = (displayCfg.vPixelScale >= 2) ? 1 : 2;
	fieldLines   = P9918_MACHINE_LINES * linesPerCall;
	scanParams.vVirtualPixels = displayCfg.vVirtualPixels;
}

//////////////////////////////////////////////////////////////
// Settings block: load, stamp, persist
//////////////////////////////////////////////////////////////
static void configStore() {
	if (!engaged) {
		configPending = false;      // no instance to save from
		return;
	}

	uint8_t *cfg = pico9918_config(tms9918);
	pico9918_config_prepare_save(cfg, P9918_CONFIG_HW_VERSION);   // identity and the initialised marker

	FILE *fp = fopen(P9918_CONFIG_FILE, "wb");
	if (NULL == fp) {
		debug_write("PICO9918: could not write %s", P9918_CONFIG_FILE);
		return;
	}

	size_t wrote = fwrite(cfg, 1, PICO9918_CONFIG_BYTES, fp);
	fclose(fp);

	if (wrote != PICO9918_CONFIG_BYTES) {
		debug_write("PICO9918: short write to %s", P9918_CONFIG_FILE);
		return;                     // still pending, so the next flush retries
	}

	configPending = false;
}

void p9918FlushConfig() {
	if (configPending) {
		configStore();
	}
}

// The library owns the defaults, the validation and the migration. Not a zeroed
// block - the palette defaults are not zero, and apply() would unpack it to black.
static void configLoad() {
	uint8_t *cfg = pico9918_config(tms9918);

	pico9918_config_defaults(cfg);

	FILE *fp = fopen(P9918_CONFIG_FILE, "rb");
	if (NULL != fp) {
		if (fread(cfg, 1, PICO9918_CONFIG_BYTES, fp) != PICO9918_CONFIG_BYTES) {
			pico9918_config_defaults(cfg);
		}
		fclose(fp);
	}

	if (pico9918_config_validate(cfg, P9918_CONFIG_HW_VERSION)) {
		configPending = true;       // migrated - so a failed write is retried, not dropped
		configStore();
	}

	// now, not at the next frame end - and cancels the apply pico9918_set_chip
	// asked for, which would otherwise reseed the registers a frame later
	pico9918_config_apply_now(tms9918, true);
}

// The firmware defers display settings to a block the user confirms after a
// reboot; here they all land on the same DIB, so a save is just a save.
static void configSaved(pico9918_t *inst, uint8_t *live, uint8_t key, void *userdata) {
	(void)inst; (void)userdata;
	switch (key) {
		case PICO9918_CONF_SAVE_TO_FLASH:   // normal save
		case PICO9918_CONF_SAVE_FORCED:     // reset to defaults
		case PICO9918_CONF_PENDING_CONFIRM: // user accepted a display change
			pico9918_config_refresh_pending_mirror(live, PICO9918_PENDING_STATE_CONFIRMED);
			configPending = true;
			debug_write("PICO9918: settings changed, saving to %s", P9918_CONFIG_FILE);
			break;

		case PICO9918_CONF_PENDING_CANCEL:
			// not a save - the stored block is untouched
			live[PICO9918_CONF_PENDING_STATE] = PICO9918_PENDING_STATE_CONFIRMED;
			break;
	}
}

// Fires if the startup diagnostics screen overrode the stored settings.
static void configReload(pico9918_t *inst, void *userdata) {
	(void)inst; (void)userdata;
	if (!engaged) return;
	configLoad();
	pico9918_diag_config_updated(tms9918);
}

// Retained by pointer, so a literal. A board scans out at a fixed rate; here the
// field is produced once per emulated frame, so it is the machine's.
static const char *diagOutputRate() {
	return (hzRate == HZ50) ? "@50" : "@60";
}

static bool diagReady = false;

// The diagnostics panel's host half. Strings are retained by pointer.
static void diagSetup() {
	static char firmware[16];

	if (!diagReady) {
		pico9918_diag_init();
		diagReady = true;
	}

	sprintf(firmware, "%u.%u.%u", PICO9918_CORE_VER_MAJOR,
			PICO9918_CORE_VER_MINOR, PICO9918_CORE_VER_PATCH);
	pico9918_diag_set_version_info("1.0", firmware);
	pico9918_diag_set_output_name("480P ", diagOutputRate());
	pico9918_diag_set_clock_hz(252000000.0f);
}

// Video->50hz moves without a reset, and the panel is only written at one. The FPS row
// follows on its own - pico9918_frame_end is handed the rate every frame.
static void diagNoteRate() {
	static int lastHz = 0;

	if ((!diagReady) || (hzRate == lastHz)) return;

	lastHz = hzRate;
	pico9918_diag_set_output_name(NULL, diagOutputRate());
}

//////////////////////////////////////////////////////////////
// Bring the core up, and lend it Classic99's VDP pointer
//////////////////////////////////////////////////////////////
static bool engage() {
	if (engaged) return true;

	if (NULL == p9918framedata) {
		p9918framedata = (unsigned int*)malloc(P9918_WIDTH*P9918_HEIGHT*sizeof(unsigned int));
		if (NULL == p9918framedata) {
			debug_write("PICO9918: failed to allocate the %dx%d frame buffer", P9918_WIDTH, P9918_HEIGHT);
			return false;
		}
		memset(p9918framedata, 0, P9918_WIDTH*P9918_HEIGHT*sizeof(unsigned int));
	}

	tms9918 = pico9918_new();
	if (NULL == tms9918) {
		debug_write("PICO9918: failed to create a VDP instance");
		return false;
	}
	pico9918_gpu_init(tms9918);
	pico9918_gpu_set_config_save_callback(tms9918, configSaved, NULL);
	pico9918_frame_set_config_reload_callback(tms9918, configReload, NULL);
	pico9918_reset(tms9918);

	pClassic99VDP = VDP;
	VDP = (Byte*)tms9918;

	buildPixelMap();
	configureFrame();

	engaged = true;
	return true;
}

static void disengage() {
	if (!engaged) return;

	p9918FlushConfig();

	if (NULL != pClassic99VDP) {
		VDP = pClassic99VDP;
		pClassic99VDP = NULL;
	}
	pico9918_destroy(tms9918);
	tms9918 = NULL;
	engaged = false;
	debug_write("PICO9918: core disengaged");
}

//////////////////////////////////////////////////////////////
// Reset - also where the engine is engaged or dropped, since
// the two engines own VDP RAM differently
//////////////////////////////////////////////////////////////
void p9918Reset(bool isCold) {
	p9918ReconcileChip();
	latchChip();

	if (!bUsePico9918) {
		disengage();
		return;
	}

	if (!engaged) {
		if (!engage()) {
			// fall back rather than run with no VDP at all
			bUsePico9918 = 0;
			p9918ReconcileChip();
			latchChip();
			return;
		}
		// engage() has already reset the core
	} else {
		p9918FlushConfig();
		pico9918_reset(tms9918);
		pico9918_gpu_init(tms9918);
	}

	// seeded at the requested chip, not stepped down to it - the config port
	// the load below needs is gated on being a PICO9918
	pico9918_set_chip(tms9918, coreChip(nVdpChip));

	if (chipIsPico(nVdpChip)) {
		diagSetup();

		if (isCold) {
			configLoad();       // power-on: the only thing that reads storage
		} else {
			// VR50 is the guest resetting the VDP, not the board rebooting -
			// flash survives it, so re-read nothing, just put the registers back
			pico9918_config_apply_now(tms9918, true);
		}
		pico9918_diag_config_updated(tms9918);
	}

	debug_write("PICO9918: core engaged as %s",
				(nVdpChip == P9918_CHIP_PICO9918_PRO) ? "a PICO9918 PRO" :
				(nVdpChip == P9918_CHIP_PICO9918)     ? "a PICO9918" :
				(nVdpChip == P9918_CHIP_F18A)         ? "an F18A" :
				(nVdpChip == P9918_CHIP_TMS9918)      ? "a TMS9918" : "a TMS9918A");

	// power-on clears VRAM, which the core deliberately does not do itself
	if (isCold) {
		memset(VDP, 0, 0x4000);
	}

	curLine = 0;
	configureFrame();

	gpuIps = (nVdpChip == P9918_CHIP_PICO9918_PRO) ? PICO9918_GPU_IPS_PRO :
			 (nVdpChip == P9918_CHIP_PICO9918)     ? PICO9918_GPU_IPS_CLASSIC :
			 (nVdpChip == P9918_CHIP_F18A)         ? PICO9918_GPU_IPS_F18A : 0;

	// zero, not gpuIps: p9918GpuCycles runs the GPU, and a clock here would run it
	// again off the library's own scanline service
	pico9918_gpu_set_clock(tms9918, 0);
	gpuBalance = 0;
	gpuPerCycleRate = 0;

	p9918SyncShadow();
	p9918SyncLayers();
	p9918SyncGpuDebug();
}

void p9918Shutdown() {
	disengage();
	if (NULL != p9918framedata) {
		free(p9918framedata);
		p9918framedata = NULL;
	}
}

//////////////////////////////////////////////////////////////
// Host bus. The legacy path's bookkeeping is kept - heatmap,
// VDP breakpoints, the address counter the debugger shows -
// but the state that matters is the core's.
//////////////////////////////////////////////////////////////

// Follows the core's increment rule (VR48) so the two cannot drift.
static void trackIncrement() {
	VDPADD = (VDPADD + (signed char)VDP[PICO9918_MAP_REGISTERS+0x30]) & 0x3fff;
}

// Where the byte lands, for the heatmap and the initialised-memory tracking.
// Mirrors pico9918_cpu_vram_addr_impl(), which normal running may not call.
// Not GetRealVDP(), which folds in the 80 column and 128k hacks.
static int realVdpAddress() {
	int addr = VDPADD & 0x3fff;

	if (((nVdpChip == P9918_CHIP_TMS9918) || (nVdpChip == P9918_CHIP_TMS9918A)) &&
		(0 == (VDP[PICO9918_MAP_REGISTERS+1] & 0x80))) {
		addr = Scramble4kVDP(addr);
	}

	return addr;
}

// The library's own spelling of the two-byte pair - reversing the order
// addresses a different register instead of failing.
void p9918WriteReg(unsigned char r, unsigned char v) {
	if (!engaged) return;

	pico9918_write_register_value(tms9918, (pico9918_register_t)r, v);
	p9918SyncShadow();
	p9918ServiceGpuBreak();
}

void p9918WriteAddr(unsigned char c) {
	if (0 == vdpaccess) {
		VDPADD = (VDPADD & 0xff00) | c;
		vdpaccess = 1;
	} else {
		VDPADD = (VDPADD & 0x00ff) | (c<<8);
		vdpaccess = 0;

		if (VDPADD & 0x8000) {
			// register write
			int nReg  = (VDPADD & 0x3f00) >> 8;
			int nData = VDPADD & 0xff;

			for (int idx=0; idx<nBreakPoints; idx++) {
				if (BreakPoints[idx].Type == BREAK_EQUALS_VDPREG) {
					if ((nReg == BreakPoints[idx].A) &&
						((nData & BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
						if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
							TriggerBreakPoint();
						}
					}
				}
			}
		} else if ((VDPADD & 0xC000) == 0) {
			trackIncrement();       // read setup - the core prefetches too
		}
		VDPADD &= 0x3fff;
	}

	pico9918_write_addr(tms9918, c);
	p9918SyncShadow();
	p9918ServiceGpuBreak();
}

void p9918WriteData(unsigned char c) {
	int RealVDP = realVdpAddress();

	vdpaccess = 0;
	UpdateHeatVDP(RealVDP);
	VDPMemInited[RealVDP] = 1;

	for (int idx=0; idx<nBreakPoints; idx++) {
		switch (BreakPoints[idx].Type) {
			case BREAK_EQUALS_VDP:
				if ((CheckRange(idx, VDPADD)) && ((c & BreakPoints[idx].Mask) == BreakPoints[idx].Data)) {
					if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
						TriggerBreakPoint();
					}
				}
				break;

			case BREAK_WRITEVDP:
				if (CheckRange(idx, VDPADD)) {
					if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
						TriggerBreakPoint();
					}
				}
				break;
		}
	}

	pico9918_write_data(tms9918, c);
	trackIncrement();
	p9918SyncShadow();
	p9918ServiceGpuBreak();
}

unsigned char p9918ReadStatus() {
	// run the VDP forward, so a polling instruction sees the state it would
	updateVDP(-pCurrentCPU->GetCycleCount());

	vdpaccess = 0;

	// which register R#15 selected, before the port read consumes it - only
	// SR0 carries the frame interrupt, so only SR0 is a poll of it
	const bool readingSR0 = ((VDP[PICO9918_MAP_REGISTERS+0x0f] & 0x0f) == 0);

	Byte z = pico9918_read_status(tms9918);

	if (!readingSR0) {
		p9918SyncShadow();
		return z;
	}

	// interrupt-poll tracking the debugger displays
	if (statusUpdateRead) {
		statusUpdateRead = false;
		statusReadLine = vdpscanline;
		if (vdpscanline > 192+27) {
			statusReadCount = 262*statusFrameCount + (vdpscanline-192-27);
		} else {
			statusReadCount = 262*statusFrameCount + (vdpscanline+(262-(192+27)));
		}
	}
	if (z & VDPS_INT) {
		statusUpdateRead = true;
		statusFrameCount = 0;
	}

	p9918SyncShadow();
	return z;
}

unsigned char p9918ReadData() {
	int RealVDP = realVdpAddress();

	vdpaccess = 0;
	UpdateHeatVDP(RealVDP);

	for (int idx=0; idx<nBreakPoints; idx++) {
		if (BreakPoints[idx].Type == BREAK_READVDP) {
			if (CheckRange(idx, VDPADD-1)) {
				if ((!bIgnoreConsoleBreakpointHits) || (pCurrentCPU->GetPC() > 0x1fff)) {
					TriggerBreakPoint();
				}
			}
		}
	}

	// the byte the guest gets now was fetched at the previous address, hence
	// testing the flag before setting it, and naming VDPADD-1
	if ((g_bCheckUninit) && (vdpprefetchuninited)) {
		TriggerBreakPoint();
		char buf[128];
		sprintf(buf, "Breakpoint - reading uninitialized VDP memory at >%04X (or other prefetch)", (RealVDP-1)&0x3fff);
		MessageBox(myWnd, buf, "Classic99 Debugger", MB_OK);
	}
	vdpprefetchuninited = (VDPMemInited[RealVDP] == 0);

	Byte z = pico9918_read_data(tms9918);
	trackIncrement();
	p9918SyncShadow();
	return z;
}

//////////////////////////////////////////////////////////////
// One virtual (VGA) line, written out vPixelScale times
//////////////////////////////////////////////////////////////
// bottom-up, matching the positive-height BITMAPINFO framedata uses
static void blitLine(unsigned int outRow) {
	unsigned int *dest = p9918framedata + (P9918_HEIGHT-1-outRow)*P9918_WIDTH;
	for (unsigned int x = 0; x < P9918_WIDTH; ++x) {
		dest[x] = bgr12_0rgb[linebuf[x] & 0x0fff];
	}
}

static void virtualLine() {
	if (curLine < scanParams.vVirtualPixels) {
		unsigned int scale = displayCfg.vPixelScale ? displayCfg.vPixelScale : 1;

		for (unsigned int rep = 0; rep < scale; ++rep) {
			const unsigned int dy = curLine*scale + rep;
			if (dy >= P9918_HEIGHT) continue;

			// the frame module rather than bare scan_line(): borders, SR3, the
			// line interrupt, the GPU trigger, the badge and the CRT dim
			if (pico9918_frame_output_line(tms9918, dy, &scanParams, linebuf)) {
				blitLine(dy);
			} else if (dy > 0) {
				// unchanged: this row is still the last one's
				memcpy(p9918framedata + (P9918_HEIGHT-1-dy)*P9918_WIDTH,
					   p9918framedata + (P9918_HEIGHT-dy)*P9918_WIDTH,
					   P9918_WIDTH*sizeof(unsigned int));
			}
		}

		if (curLine == geometry.triggerScanline) {
			pico9918_frame_end_of_scanline(tms9918);
		}
	}

	if (curLine == scanParams.vVirtualPixels) {
		pico9918_frame_porch(tms9918);
	}

	if (++curLine >= fieldLines) {
		curLine = 0;
		syncPalette();
		diagNoteRate();
		geometry = pico9918_frame_end(tms9918, 40.0f,
									  (hzRate == HZ50) ? 50.0f : 60.0f, &displayCfg);
		linesPerCall = (displayCfg.vPixelScale >= 2) ? 1 : 2;
		fieldLines   = P9918_MACHINE_LINES * linesPerCall;
		scanParams.vVirtualPixels = displayCfg.vVirtualPixels;
	}
}

void p9918GpuCycles(int cycles) {
	if ((!engaged) || (0 == gpuIps)) return;

	// cfg_cpf, not max_cpf: a stop zeroes max_cpf, and F2 still runs an instruction
	const int rate = cfg_cpf * hzRate;
	if (rate != gpuPerCycleRate) {
		gpuPerCycleRate = rate;
		gpuPerCycle = (rate > 0) ? (unsigned int)((((unsigned long long)gpuIps) << 16) / rate) : 0;
	}

	gpuLastCpuCycles = cycles;
	gpuBalance -= (long long)cycles * gpuPerCycle;
	if (gpuBalance >= 0) return;

	const unsigned int n = (unsigned int)((-gpuBalance) >> 16);
	if (0 == n) return;
	gpuBalance += ((long long)n) << 16;
	pico9918_gpu_step_n(tms9918, n);
}

void p9918GpuCredit() {
	gpuBalance += 1 << 16;
}

bool p9918CpuStepDue() {
	return gpuBalance >= (long long)gpuLastCpuCycles * gpuPerCycle;
}

void p9918Scanline() {
	if (!engaged) return;

	EnterCriticalSection(&VideoCS);

	// latched - a mode change mid-loop rewrites linesPerCall
	unsigned int calls = linesPerCall;
	for (unsigned int i = 0; i < calls; ++i) {
		virtualLine();
	}

	LeaveCriticalSection(&VideoCS);

	p9918SyncShadow();
	p9918ServiceGpuBreak();
}
