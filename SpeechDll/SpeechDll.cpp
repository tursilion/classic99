// SpeechDll.cpp : Defines the entry point for the DLL application.
// The TI chip is the CD2501ENL, which is the same as the TMS5200
// according to the MAME notes.

#include "stdafx.h"
#include "mame_wannabe.h"
#include "tms5220.h"
#include "spchrom.h"

tms5200_device *pChip=NULL;
speechrom_device *pRom = NULL;

// currently writes to OutputDebugString only
void debug_write(char *s, ...)
{
	char buf[1024];
	vsnprintf_s(buf, 1024, _TRUNCATE, s, (char*)((&s)+1));
	OutputDebugString("[Speech]: ");
	OutputDebugString(buf);
}

// Windows DLL management - right now just used to emit copyright info
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		OutputDebugString("SpeechDLL for Classic99 version 2.0\n");
		OutputDebugString("BSD-3-Clause by Frank Palazzolo, Aaron Giles, Jonathan Gevaryahu, Raphael Nabet, Couriersud\n");
		OutputDebugString("Michael Zapf, ported to Classic99 by Tursi\n");
	}
    return TRUE;
}

void SpeechInit(unsigned char *pROM, int nRomLen, int nBufferLen, int nEmulationRate) {
	machine_config x;
	const int clock = 640000;	// rate for 8Khz output

	if (NULL == pChip) {
		pChip=new tms5200_device(x, NULL, NULL, clock);	// rate for 8Khz output
	}
	if (NULL == pChip) {
		return;
	}
	if (NULL == pRom) {
		pRom = new speechrom_device(x, NULL, NULL, clock);
	}
	if (NULL == pRom) {
		delete pChip;
		pChip = NULL;
		return;
	}

	// set up the speech ROM
	pRom->device_start(pROM, nRomLen);

	// get it rolling
	pChip->device_clock_changed();
	pChip->device_start(pRom);
	pChip->device_reset();
}

void SpeechStop() {
	if (NULL != pChip) {
		delete pChip;
		pChip=NULL;
	}
}

unsigned char SpeechRead() {
	if (NULL != pChip) {
		// don't clear interrupt, I don't think it's wired up...
		return pChip->status_read(0);
	} else {
		return 0;
	}
}

// returns false if speak external FIFO is full (TI should halt CPU until audio processed)
// retry is simply a hint to the debug, if any, that a previous write is being retried
bool SpeechWrite(unsigned char byte, bool /*fRetry*/) {
	if (NULL != pChip) {
		return pChip->data_write(byte);
	} else {
		// if no chip, always accept the byte
		return true;
	}
}

void SpeechProcess(unsigned char *pBuf, int nMax) {
	// get the data - 16 bit
	if (NULL != pChip) {
		pChip->process((INT16*)pBuf, nMax);
	}
}

