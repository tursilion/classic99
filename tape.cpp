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
	ofn.lpstrFilter    = "WAV file\0*.wav\0\0"; 
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

    debug_write("loading WAV file ....");

    FILE *fp = fopen(ofn.lpstrFile, "rb");
    if (NULL == fp) {
        LeaveCriticalSection(&TapeCS);
        debug_write("File failed to open, code %d", errno);
        return;
    }

    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // let's put an upper limit on it...
    // say 5 minutes at 48khz stereo...
    // I can't believe this, but people are passing around the cassette files
    // as 48Khz, 32-bit floating point data... so we have to be able to read floats, too...
    // no wonder the files are so bloody big...
    if (len > 5*60*48000*2*16) {
        LeaveCriticalSection(&TapeCS);
        debug_write("Can't handle wave files larger than %d bytes...", 5*60*48000*2*16);
        MessageBox(myWnd, "WAV file too large, try resampling down a bit...", "Classic99 Error", MB_OK);
        fclose(fp);
        return;
    }

    unsigned char *buf = (unsigned char*)malloc(len);
    fread(buf, 1, len, fp);
    fclose(fp);

    // read the header...
    if ((buf[0]!='R')||(buf[1]!='I')|(buf[2]!='F')|(buf[3]!='F')) {
        LeaveCriticalSection(&TapeCS);
        debug_write("WAV is missing RIFF header.");
        MessageBox(myWnd, "WAV missing RIFF header, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }
    if ((buf[8]!='W')||(buf[9]!='A')|(buf[10]!='V')|(buf[11]!='E')) {
        LeaveCriticalSection(&TapeCS);
        debug_write("WAV is missing WAVE header.");
        MessageBox(myWnd, "WAV missing WAVE header, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }
    // technically, this can be elsewhere...
    if ((buf[12]!='f')||(buf[13]!='m')|(buf[14]!='t')|(buf[15]!=' ')) {
        LeaveCriticalSection(&TapeCS);
        debug_write("WAV is missing fmt header.");
        MessageBox(myWnd, "WAV missing fmt header, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }
    int chunksize = *((int *)&buf[16]);    // little endian

    // audio format
    int fmt = *((unsigned short*)&buf[20]);
    if ((fmt != 1)&&(fmt != 3)) {
        LeaveCriticalSection(&TapeCS);
        debug_write("WAV is not tagged as PCM (1) or IEEE (3), type 0x%02X%02X.", buf[21], buf[20]);
        MessageBox(myWnd, "WAV is not tagged as PCM, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }

    int chans = buf[22] + buf[23]*256;
    if ((chans < 0) || (chans > 2)) {
        LeaveCriticalSection(&TapeCS);
        debug_write("WAV has invalid channel count of %d (want 1 or 2).", chans);
        MessageBox(myWnd, "WAV has invalid channel count, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }

    int sampleRate = *((int*)&buf[24]);
    
    int bits = buf[34] + buf[35]*256;
    if ((bits != 8) && (bits != 16) && (bits != 32) && (bits != 64)) {
        LeaveCriticalSection(&TapeCS);
        debug_write("WAV has invalid sample size of %d bits (want 8, 16, 32, or 64)", bits);
        MessageBox(myWnd, "WAV has invalid sample size, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }

    debug_write("Parsed WAVE header at %dHz, %d bits per sample, %s", sampleRate, bits, chans==1?"mono":"stereo");

    // search for DATA chunk
    int pos = 20;
    while (pos < len) {
        pos = pos + chunksize;
        if (pos >= len-8) break;
        chunksize = *((int*)&buf[pos+4]);
        if ((buf[pos]=='d')&&(buf[pos+1]=='a')&&(buf[pos+2]=='t')&&(buf[pos+3]=='a')) break;
        pos+=8;
    }
    if (pos >= len-8) {
        LeaveCriticalSection(&TapeCS);
        debug_write("Can't find data chunk in WAV");
        MessageBox(myWnd, "Can't find 'data' chunk in WAV, not using...", "Classic99 Error", MB_OK);
        free(buf);
        return;
    }

    // handle the resample... there are three sample formats plus we need to deal with mono or stereo...
    // Formats are:
    // 0,1: 8 bit unsigned - initial sample should be around 127, representing 0
    // 2,3: 8 bit signed - initial sample should be around 0, representing 0
    // 4,5: 16 bit signed - we don't try to detect
    // 6,7: 32-bit float signed
    // 8,9: 64-bit double signed
    // For stereo we'll add the channels together, since we don't know which is relevant
    int mode;
    pos+=8;     // get to the data
    if (bits == 64) {
        mode = 8;
    } else if (bits == 32) {
        mode = 6;
    } else if (bits == 16) {
        mode = 4;
    } else {
        if ((buf[pos] > 100) && (buf[pos] < 150)) {
            // assume unsigned
            debug_write("8 bit wave detected as unsigned");
            mode = 0;
        } else {
            debug_write("8 bit wave detected as signed");
            mode = 2;
        }
    }
    if (chans > 1) ++mode;

    int cntIn = 0;      // offset in bytes
    double cntRatio = (double)sampleRate / (double)TapeSampleRate;
    double sampleIn = 0.0;  // offset in samples
    int cntOut = 0;

    // calculate the output size and allocate a buffer for that...
    int samplecount = chunksize / ((bits*chans)/8);
    samplecount = int(samplecount / cntRatio) + 100;
    unsigned char *pNewBuf = (unsigned char*)malloc(samplecount);
    unsigned char *pNewBuf2 = (unsigned char*)malloc(samplecount);

    // this is all we need...
    debug_write("Converting WAVE to %dHz, 8-bit for internal use...", TapeSampleRate);

    // theses are all pretty similar loops, but it's faster to loop inside the switch...
    // I'm swinging the negative positive here... but maybe that's not helpful?
    switch (mode) {
        case 0: // unsigned 8 bit
            while ((cntOut < samplecount) && (cntIn+pos < len)) {
                int val = buf[pos+cntIn] - 127; // recenter it on zero
                if (val < 0) val=0;             // drop negative side
                val *= 2;                       // make 0-255
                if (val > 255) val=255;         // this is possible
                pNewBuf[cntOut] = val&0xff;     // save it
                pNewBuf2[cntOut] = 0;
                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*1;      // samples to bytes
            }
            break;

        case 1: // unsigned 8 bit stereo
            while ((cntOut < samplecount) && (cntIn+pos < len)) {
                int val = buf[pos+cntIn] - 127; // recenter it on zero
                if (val < 0) val=0;             // drop negative side
                val *= 2;                       // make 0-255
                if (val > 255) val=255;         // this is possible
                pNewBuf[cntOut] = val&0xff;     // save it
                
                val = buf[pos+cntIn+1] - 127;   // recenter it on zero
                if (val < 0) val=0;             // drop negative side
                val *= 2;                       // make 0-255
                if (val > 255) val=255;         // this is possible
                pNewBuf2[cntOut] = val&0xff;    // save it

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*2;      // samples to bytes
            }
            break;

        case 2: // signed 8 bit
            while ((cntOut < samplecount) && (cntIn+pos < len)) {
                int val = (signed)buf[pos+cntIn];       // already centered on zero
                if (val < 0) val=0;                     // drop negative side
                val *= 2;                               // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf[cntOut] = val&0xff;             // save it
                pNewBuf2[cntOut] = 0;

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*1;      // samples to bytes
            }
            break;

        case 3: // signed 8 bit stereo
            while ((cntOut < samplecount) && (cntIn+pos+2 < len)) {
                int val = (signed)buf[pos+cntIn];       // already centered on zero
                if (val < 0) val=0;                     // drop negative side
                val *= 2;                               // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf[cntOut] = val&0xff;             // save it

                val = (signed)buf[pos+cntIn+1];         // already centered on zero
                if (val < 0) val=0;                     // drop negative side
                val *= 2;                               // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf2[cntOut] = val&0xff;            // save it

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*2;      // samples to bytes
            }
            break;

        case 4: // signed 16 bit
            while ((cntOut < samplecount) && (cntIn+pos+2 < len)) {
                int val = *((signed short*)&buf[pos+cntIn]);   // already centered on zero
                if (val < 0) val=0;                     // drop negative side
                val /= 128;                             // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf[cntOut] = val&0xff;             // save it
                pNewBuf2[cntOut] = 0;

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*2;      // samples to bytes
            }
            break;

        case 5: // signed 16-bit stereo
            while ((cntOut < samplecount) && (cntIn+pos+4 < len)) {
                int val = *((signed short*)&buf[pos+cntIn]);   // already centered on zero
                val = -val;
                if (val < 0) val=0;                     // drop negative side
                val /= 128;                             // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf[cntOut] = val&0xff;             // save it

                val = *((signed short*)&buf[pos+cntIn+2]);   // already centered on zero
                val = -val;
                val /= 128;                             // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf2[cntOut] = val&0xff;            // save it

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*4;      // samples to bytes
            }
            break;

        case 6: // signed 32-bit float
            while ((cntOut < samplecount) && (cntIn+pos+4 < len)) {
                float val = *((float*)&buf[pos+cntIn]);   // already centered on zero
                if (val < 0) val=0;                       // drop negative side
                val *= 255;                               // make 0-255
                if (val > 255) val=255;                   // this is possible
                pNewBuf[cntOut] = (unsigned char)val;     // save it
                pNewBuf2[cntOut] = 0;

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*4;      // samples to bytes
            }
            break;

        case 7: // signed 32-bit float stereo
            while ((cntOut < samplecount) && (cntIn+pos+8 < len)) {
                float val = *((float*)&buf[pos+cntIn]);   // already centered on zero
                if (val < 0) val=0;                       // drop negative side
                val *= 255;                               // make 0-255
                if (val > 255) val=255;                   // this is possible
                pNewBuf[cntOut] = (unsigned char)val;     // save it

                val = *((float*)&buf[pos+cntIn+4]);       // already centered on zero
                val *= 255;                               // make 0-255
                if (val > 255) val=255;                   // this is possible
                pNewBuf2[cntOut] = (unsigned char)val;    // save it

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*8;      // samples to bytes
            }
            break;

        case 8: // signed 64-bit double
            while ((cntOut < samplecount) && (cntIn+pos+8 < len)) {
                double val = *((double*)&buf[pos+cntIn]);   // already centered on zero
                if (val < 0) val*=-1;                   // make positive (0-1)
                val *= 255;                             // make 0-255
                if (val > 255) val=255;                 // this is possible
                pNewBuf[cntOut] = (unsigned char)val;           // save it
                pNewBuf2[cntOut] = 0;

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*8;      // samples to bytes
            }
            break;

        case 9: // signed 64-bit double stereo
            while ((cntOut < samplecount) && (cntIn+pos+16 < len)) {
                double val = *((double*)&buf[pos+cntIn]);   // already centered on zero
                if (val < 0) val*=-1;                       // make positive (0-1)
                val *= 255;                                 // make 0-255
                if (val > 255) val=255;                     // this is possible
                pNewBuf[cntOut] = (unsigned char)val;       // save it

                val = *((double*)&buf[pos+cntIn+8]);        // already centered on zero
                val *= 255;                                 // make 0-255
                if (val > 255) val=255;                     // this is possible
                pNewBuf2[cntOut] = (unsigned char)val;      // save it

                // next sample:
                ++cntOut;
                sampleIn+=cntRatio;             // handles decimation
                cntIn = ((int)sampleIn)*16;     // samples to bytes
            }
            break;
    }

    // now do some simple auto-levelling. We will try to get the
    // maximum value up to around what I tuned with ;) 
    // zeros to peaks are supposed to be about even, so an 
    // average SHOULD work...?
    // this also decides whether to use the left or right channel ;)
    int avg1 = 0;
    int tot = 0;
    for (int idx = 0; idx<samplecount; ++idx) {
        tot += pNewBuf[idx];
    }
    avg1 = int((double)tot / samplecount + .5);

    int avg2 = 0;
    tot = 0;
    for (int idx = 0; idx<samplecount; ++idx) {
        tot += pNewBuf2[idx];
    }
    avg2 = int((double)tot / samplecount + .5);

    int avg;
    if (avg1 > avg2) {
        avg = avg1;
        pTapeBuf = pNewBuf;
        free(pNewBuf2);
    } else {
        avg = avg2;
        pTapeBuf = pNewBuf2;
        free(pNewBuf);
    }
    double scale = 29.0 / avg;
    debug_write("Avg value %d, Scaling factor %f", avg, scale);
    for (int idx = 0; idx<samplecount; ++idx) {
        pTapeBuf[idx] = int(pTapeBuf[idx]*scale+0.5);
    }
    
    debug_write("Tape conversion finished.");
    free(buf);

#if 0
    debug_write("saving test d:\\new\\test.raw");
    fp=fopen("D:\\new\\test.raw", "wb");
    if (NULL != fp) {
        fwrite(pTapeBuf, 1, samplecount, fp);
        fclose(fp);
    }
#endif

    tape_buf_size = samplecount;
    tapeupdateddistance = 0.0;
    tape_pos = 0;

    LeaveCriticalSection(&TapeCS);
}

