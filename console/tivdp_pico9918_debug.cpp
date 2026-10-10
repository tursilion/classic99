//
// Classic99 - pico9918-core bridge, debugger half
//
// (C) 2026 Troy Schrapel (visrealm)
//
// Separate TU because this is the integration's only use of pico9918-core's
// debugger surface, which the library builds only when asked for it
// (PICO9918_DEBUG_API, wired on in classic99.vcxproj).
//

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <windows.h>

#include "tiemul.h"
#include "cpu9900.h"
#include "tivdp_pico9918.h"

#include "pico9918.h"
#include "gpu/gpu.h"
#include "pico9918_debug.h"

extern pico9918_t *tms9918;

extern bool bDisableBlank, bDisableSprite, bDisableBackground;	// tivdp.cpp
extern bool bDisableColorLayer, bDisablePatternLayer;

extern struct _break BreakPoints[];
extern int nBreakPoints;
extern CPU9900 *pGPU;
extern bool bDebugGpuFocus;		// Tiemul.cpp

#if PICO9918_BUILD_STEP_CALLBACK

static bool           gpuBreakPending;
static bool           gpuBreakResuming;
static unsigned short gpuBreakAt;
static bool           gpuStepOverActive;
static unsigned short gpuStepOverAt;

static void gpuBreakHere(unsigned short pc) {
	gpuBreakAt        = pc;
	gpuBreakResuming  = true;
	gpuBreakPending   = true;
	gpuStepOverActive = false;
}

static bool gpuBreakpoint(pico9918_t *t, unsigned short pc, void *userdata) {
	if (gpuBreakResuming) {
		gpuBreakResuming = false;
		if (pc == gpuBreakAt) return true;
	}

	// +2: a subroutine that takes an inline data word returns past it
	if (gpuStepOverActive &&
		((pc == gpuStepOverAt) || (pc == (unsigned short)(gpuStepOverAt + 2)))) {
		gpuBreakHere(pc);
		return false;
	}

	if (!pGPU->enableDebug) return true;

	for (int idx = 0; idx < nBreakPoints; ++idx) {
		const int type = BreakPoints[idx].Type;
		if (((type == BREAK_PC) || (type == BREAK_GPUPC)) && (CheckRange(idx, pc))) {
			gpuBreakHere(pc);
			return false;
		}
	}

	return true;
}

#endif

// Not from inside the callback: the core writes the GPU's PC, registers and
// workspace back only when the slice returns.
void p9918ServiceGpuBreak() {
#if PICO9918_BUILD_STEP_CALLBACK
	if (!gpuBreakPending) return;
	gpuBreakPending = false;

	TriggerBreakPoint(true);		// forced - pCurrentCPU is the 9900, this break is the GPU's
	bDebugGpuFocus = true;
#endif
}

void p9918SyncGpuDebug() {
#if PICO9918_BUILD_STEP_CALLBACK
	gpuBreakPending   = false;
	gpuBreakResuming  = false;
	gpuStepOverActive = false;

	if (!p9918Active()) return;

	pico9918_debug_set_step_callback(tms9918, gpuBreakpoint, NULL);
#endif
}

// Not NULL at the call below: that would fall through to the instance callback and
// break on the machine we have already stopped. The budget ends the slice.
static bool gpuStepOne(pico9918_t *t, unsigned short pc, void *userdata) {
	return true;
}

bool p9918StepGpu() {
	if (!p9918Active()) return false;
	if (!pico9918_debug_gpu_armed(tms9918)) return false;

#if PICO9918_BUILD_STEP_CALLBACK
	gpuBreakResuming  = false;
	gpuBreakPending   = false;
	gpuStepOverActive = false;
#endif

	pico9918_debug_gpu_step_n(tms9918, 1, gpuStepOne, NULL);
	p9918GpuCredit();
	p9918SyncShadow();
	bDebugGpuFocus = true;

	return true;
}

// Format VI: opcode in bits 15-6, Ts in 5-4, and only Ts=10 carries a second word
static bool gpuCallReturn(unsigned short pc, unsigned short *after) {
	const unsigned short op = (unsigned short)((pico9918_gpu_mem_value(tms9918, pc) << 8) |
											    pico9918_gpu_mem_value(tms9918, pc + 1));

	switch (op & 0xffc0) {
		case 0x0680:	// BL
		case 0x0400:	// BLWP
		case 0x0c80:	// CALL, the F18A's own
			break;
		default:
			return false;
	}

	*after = (unsigned short)(pc + ((((op >> 4) & 3) == 2) ? 4 : 2));

	return true;
}

// False means no call at the GPU's PC, so the caller single-steps instead.
bool p9918ArmGpuStepOver() {
#if PICO9918_BUILD_STEP_CALLBACK
	if (!p9918Active()) return false;
	if (!pico9918_debug_gpu_armed(tms9918)) return false;

	const unsigned short pc = (unsigned short)pico9918_gpu_pc(tms9918);
	if (!gpuCallReturn(pc, &gpuStepOverAt)) return false;

	// the skip stays armed on the call, or a breakpoint there breaks again at once
	gpuBreakAt        = pc;
	gpuBreakResuming  = true;
	gpuBreakPending   = false;
	gpuStepOverActive = true;

	return true;
#else
	return false;
#endif
}

// The Layers menu, as a view over the core's renderer. Classic99 has one
// background item and the core has two tile layers, so it takes both.
void p9918SyncLayers() {
	if (!p9918Active()) return;

	unsigned int mask = 0;
	if (bDisableSprite)       mask |= PICO9918_SUPPRESS_SPRITES;
	if (bDisableBackground)   mask |= PICO9918_SUPPRESS_TILE1 | PICO9918_SUPPRESS_TILE2;
	if (bDisableColorLayer)   mask |= PICO9918_SUPPRESS_GM2_COLOUR;
	if (bDisablePatternLayer) mask |= PICO9918_SUPPRESS_GM2_PATTERN;
	if (bDisableBlank)        mask |= PICO9918_SUPPRESS_BLANKING;

	pico9918_debug_set_suppress(tms9918, mask);
}

// A debugger poke into the core's 64k map. The register and status windows are
// read-only to the span write, because storing those bytes is not all they owe:
// a register moves the mode and /INT, a status byte moves SR0's second copy.
void p9918DbgWriteMem(unsigned int addr, unsigned char v) {
	if (!p9918Active()) {
		VDP[addr] = v;
		return;
	}

	unsigned int end = 0;
	const unsigned int flags = pico9918_debug_region(addr, &end);

	if (flags & PICO9918_DEBUG_REGISTERS) {
		pico9918_debug_reg_write(tms9918, (unsigned char)(addr - PICO9918_MAP_REGISTERS), v);
	} else if (flags & PICO9918_DEBUG_STATUS) {
		pico9918_debug_status_write(tms9918, (unsigned char)(addr - PICO9918_MAP_STATUS), v);
	} else {
		pico9918_debug_write(tms9918, addr, &v, 1);
	}

	p9918SyncShadow();
}

// not the bus - that aliases VR30 onto R6 on a locked device
void p9918DbgWriteReg(unsigned char r, unsigned char v) {
	if (!p9918Active()) return;

	pico9918_debug_reg_write(tms9918, r, v);
	p9918SyncShadow();
}

unsigned char p9918DbgStatus(unsigned char reg) {
	if (!p9918Active()) return 0;
	return pico9918_status_value(tms9918, (pico9918_status_register_t)(reg & 0x0f));
}

unsigned int p9918DbgGpuPC() {
	if (!p9918Active()) return 0xffff;
	return pico9918_gpu_pc(tms9918);
}

bool p9918DbgGpuArmed() {
	return p9918Active() && pico9918_debug_gpu_armed(tms9918);
}

unsigned int p9918DbgGpuReg(unsigned char reg) {
	if (!p9918Active()) return 0;
	return pico9918_gpu_reg_value(tms9918, reg);
}

unsigned int p9918DbgGpuStatus() {
	if (!p9918Active()) return 0;
	return pico9918_gpu_status(tms9918);
}
