//
// (C) 2018 Mike Brent aka Tursi aka HarmlessLion.com
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
// M.Brent
///////////////////////////////////////////////////

#include <Windows.h>
#include <stdio.h>
#include <tiemul.h>
#include "addons/SDCard.h"

// quick and dirty tape support
// load only for the moment... if it works we can talk about adding save...
unsigned char *pTapeBuf = NULL;
int tape_buf_size = 0;
double tapeupdateddistance = 0.0;
int tape_pos = 0;
bool tape_motor_on = false;
int tape_motor_state = 0;   // not ready to turn on

// defines
// 8khz was not enough resolution.. 16khz seems to work
static const int TapeSampleRate = 16000;  // we resample to this, unsigned byte
static const int cutoff = 0x12;          // at what cutoff do we treat it as a 1?

// from the rest of the emulator - even though we do this I doubt the
// tape will work at any speed but normal....
extern int max_cpf, hzRate;
extern double nDACLevel;
extern CRITICAL_SECTION TapeCS;

bool getTapeBit() {
    // this is valid even if the motor is stopped
    bool ret = false;
    EnterCriticalSection(&TapeCS);
    if ((tape_pos < tape_buf_size) && (pTapeBuf != NULL)) {
        ret = (pTapeBuf[tape_pos] >= cutoff);
    }
    LeaveCriticalSection(&TapeCS);

    return ret;
}

void setTapeMotor(bool isOn) {
    // because we've got a sort of an auto-play situation in Classic99,
    // we won't actually set the tape on until the console turns it off, then on
    // This is not correct for hardware, but we don't have a physical PLAY button. ;)
    if (!isOn) {
        // console has turned tape off, so we can prime it
        tape_motor_on = false;
        tape_motor_state = 1;
        debug_write("CS1 tape motor off, ready to play.");
    } else {
        if (tape_motor_state == 1) {
            tape_motor_on = true;
            debug_write("CS1 tape motor on.");
        } else {
            debug_write("CS1 tape motor on suppressed - waiting for off->on transition.");
        }
        tape_motor_state = 0;
    }
}

void forceTapeMotor(bool isOn) {
    // i don't care, either way I eject the SD card
    debug_write("Ejecting SD card");
    SDEjectCard();

    // this one come via user command and so overrides the bit
    // That makes sense for stop, but whether it makes sense
    // for start depends on whether our suppression is active
    // Either way, we do it and just report what it was.
    // I set Motor state same as above, but not sure if that's right
    if (!isOn) {
        tape_motor_on = false;
        tape_motor_state = 1;
        debug_write("CS1 tape motor off by user command.");
    } else {
        tape_motor_on = true;
        tape_motor_state = 0;
        debug_write("CS1 tape motor on by user command, console bit is %s.", CRU[22] ? "on":"off");
    }
}

void updateTape(int nCPUCycles) {
	static int totalCycles = 0;

    EnterCriticalSection(&TapeCS);

    // if there's no tape loaded, or it's stopped, don't bother
    if ((!tape_motor_on) || (tape_pos >= tape_buf_size) || (pTapeBuf == NULL)) {
        LeaveCriticalSection(&TapeCS);

        if (CRU[24] == 0) {
            // if audio gate is set (inverted logic), play out the cassette audio input
            nDACLevel = 0.75;
        } else {
            nDACLevel = 0.0;
        }
        return;
    }

	if (max_cpf < DEFAULT_60HZ_CPF) {
        LeaveCriticalSection(&TapeCS);
		totalCycles = 0;
		return;	// don't do it if running slow
	}

	// because it is an int (nominally 3,000,000 - this makes the slider work)
	int CPUCYCLES = max_cpf * hzRate;

	totalCycles+=nCPUCycles;
	double fdist = (double)totalCycles / ((double)CPUCYCLES/TapeSampleRate);
	if (fdist < 1.0) {
        LeaveCriticalSection(&TapeCS);
        return;		// don't even bother
    }
	tapeupdateddistance += fdist;
	totalCycles = 0;		        // we used them all (fractions saved!)

	int distance = (int)tapeupdateddistance;
	tapeupdateddistance -= distance;
	if (tape_pos + distance < tape_buf_size) {
        tape_pos += distance;
	}

    if (CRU[24] == 0) {
        // if audio gate is set (inverted logic), play out the cassette audio input
        nDACLevel = ((double)pTapeBuf[tape_pos] / 256.0);
    } else {
        nDACLevel = 0.0;
    }

    LeaveCriticalSection(&TapeCS);
}

void LoadTape() {
    // todo: overloaded for load SD

    // stop the tape - this is not correct for hardware but allows our little
    // state machine to function for auto-play
    tape_motor_state = 0;
    tape_motor_on = false;
    debug_write("CS1 tape motor off (per emulator, not hardware CRU)");

    // load, resample to TapeSampleRate, 8-bit unsigned mono
    EnterCriticalSection(&TapeCS);

    if (pTapeBuf) {
        tape_buf_size = 0;
        free(pTapeBuf);
        pTapeBuf = NULL;
    }

	OPENFILENAME ofn;                  // Structure for filename dialog
	char nbuf[256], nbuf2[256];

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize    = sizeof(OPENFILENAME);
	ofn.hwndOwner      = NULL;
	ofn.lpstrFilter    = "Any file\0*\0\0"; 
	strcpy(nbuf, "");
	ofn.lpstrFile      = nbuf;
	ofn.nMaxFile       = 256;
	strcpy(nbuf2, "");
	ofn.lpstrFileTitle = nbuf2;
	ofn.nMaxFileTitle  = 256;
	ofn.Flags          = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    char szTmpDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, szTmpDir);
    BOOL ret = GetOpenFileName(&ofn);
    SetCurrentDirectory(szTmpDir);

	if (!ret) {
        LeaveCriticalSection(&TapeCS);
        return;
    }

    debug_write("loading SD card ....");
    if (!SDInsertCard(ofn.lpstrFile)) {
        debug_write("** Failed to mount SD card.");
    }

    LeaveCriticalSection(&TapeCS);
}

