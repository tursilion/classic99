// SpeechDll.cpp : Defines the entry point for the DLL application.
//
// The actual chip in a TI Speech Synth is a CD2501ENL, which appears
// to match the TMS5100 (according to MAME docs)

#include "stdafx.h"
#include "tms5110.h"
#include "spchroms.h"

device_t *pChip=NULL;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		OutputDebugString("SpeechDLL for Classic99 version 2.0\n");
		OutputDebugString("Free code by the MESS team, ported by Tursi\n");
	}
    return TRUE;
}

void SpeechInit(unsigned char *pROM, int nRomLen, int nBufferLen, int nEmulationRate) {
	if (NULL == pChip) {
		pChip=tms5110_create();
	}
	if (NULL == pChip) {
		return;
	}

	spchroms_config(pROM, nRomLen);

	DEVICE_START_CALL(tms5100);

	tms5110_set_read(pChip, spchroms_read);
	tms5110_set_load_address(pChip, spchroms_load_address);
	tms5110_set_read_and_branch(pChip, spchroms_read_and_branch);
}

void SpeechStop() {
	tms5220_destroy(pChip);
	pChip=NULL;
}

unsigned char SpeechRead() {
	return tms5220_status_read(pChip);
}

// returns false if speak external FIFO is full (TI should halt CPU until audio processed)
bool SpeechWrite(unsigned char Byte, bool fRetry) {
	return tms5220_data_write(pChip, Byte, fRetry);
}

void SpeechProcess(unsigned char *pBuf, int nMax) {
	// get the data - 16 bit
	tms5220_process(pChip, (INT16*)pBuf, nMax);
}

void debug_write(char *s, ...)
{
	char buf[1024];

	vsnprintf_s(buf, 1024, _TRUNCATE, s, (char*)((&s)+1));

	OutputDebugString(buf);
}
