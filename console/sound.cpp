// Testing on a real TI:
// The sound chip itself outputs an always positive sound wave peaking roughly at 2.8v.
// Output values are roughly 1v peak-to-peak (I measured 0 attenuation at roughly 720mV)
// However, it's hard to get good readings in the TI console due to lots of high frequency
// noise, which is also roughly 1v peak to peak and starts at around 1khz or 2khz
// The center point at output was closer to 1v, nevertheless, it's all positive.
// Two interesting points - rather than a center point, the maximum voltage appeared
// to be consistent, and the minimum voltage crept higher as attenuation increased.
// The other interseting point is that MESS seems to output all negative voltage,
// but in the end where in the range it lands doesn't matter so much (in fact my
// positive/negative swing probably doesn't even matter, but we can fix it).
// I measured these approximate values. They are less accurate as they get smaller
// because of the high noise (a sound chip out of circuit on an AVR or such
// would be a good experiment). But we can line them up with the approximate
// scale and see if it makes sense:
//
// From 0-15, I read (mV):
// 720, 600, 488, 407, 384, 328, 272, 232,
// 176, 160, 136, 120, 112, 104, 72, 0
//
// Again, measured by hand in a noisy system, so just use as very loose confirmation
// data, rather than anything exact.
//
// As a potential thought -- centering our output on zero instead of making it all positive
// means we play more nicely with the Windows system (ie: no click, no DC offset), and
// so it may be worth leaving it that way. It should still sound the same.
//

//
// (C) 2009 Mike Brent aka Tursi aka HarmlessLion.com
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
//****************************************************
// Tursi's own from-scratch TMS9919 emulation
// Because everyone uses the MAME source.
//
// Based on documentation on the SN76489 from SMSPOWER
// and datasheets for the SN79489/SN79486/SN79484,
// and recordings from an actual TI99 console (gasp!)
//
// Decay to zero is roughly 1% of maximum every 500uS
// My math suggests that the volume would thus decay by
// 0.01 every 1431.818 clocks (real clocks, not clock/16)
//
// The clock dividers in this file don't look correct according
// to the datasheets, but till I find more accurate details
// on the 9919 itself I'll leave them be. And they work.

// The 15 bit shift register is more or less simulated here -
// I need to do some measurements on the real TI and see if
// I can figure out from the doc what the real chip is doing --
// by using the user-controlled rate I can set the shift register
// to a desired speed and thus measure it.

// One interesting note from this... for getting "bass" from the
// periodic register, the periodic output is going to be
// ticking at (frequency count / 2) / 15 
// The tick count for tones is 111860.78125Hz (exactly on NTSC - are PAL consoles detuned?)
// The tick count for periodic noise if 15-bit is about 3728.6927
//
// It's a little harder to measure the white noise register, we'd have
// to work out where the tap points are by looking at the pattern.
//
// Note that technically, the datasheet says the chip needs 32 cycles to
// deal with the audio data, but we assume immediate. Also, note that
// the dB attenuation is +/- 1dB, but we assume perfect accuracy. In short,
// the chip is really pretty crummy, but we emulate a perfect one.
// Note anyway that 32 cycles at 3.58MHz is 8.9uS - just /slightly/
// slower than the fastest 9900 write cycle (MOVB Rn,*Rm = 26 cycles = 8.63uS)
// On the later chip that took a 500kHz clock (not used in the TI?)
// only 4 clocks were needed, so it was 8.0uS to respond.
// This means the normally used sound chip can potentially /just barely/
// be overrun by the CPU, but we should be fine with this emulation.
// (In truth, the time it takes to fetch the next instruction should make it
// impossible to overrun.)
//
// Another neat tidbit from RL -- don't track the fractional loss from
// the timing counters -- your ear can hear when they are added in, and
// you get periodic noise, depending on the frequency being played!
//

#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include "sound.h"
#include "tiemul.h"

// some Classic99 stuff
extern LPDIRECTSOUNDBUFFER soundbuf;						// sound chip audio buffer
extern LPDIRECTSOUNDBUFFER sidbuf;							// sid blaster audio buffer
extern LPDIRECTSOUNDBUFFER speechbuf;						// speech audio buffer
extern int hzRate;		// 50 or 60 fps (HZ50 or HZ60)
extern int Recording;
extern int max_cpf;
extern void WriteAudioFrame(void *pData, int nLen);

// hack for now - a little DAC buffer for cassette ticks and CPU modulation
// fixed size for now
extern double nDACLevel;
unsigned char dac_buffer[1024*1024];
int dac_pos = 0;
double dacupdatedistance = 0.0;

// external debug helper
void debug_write(char *s, ...);

// SID support
void (*InitSid)() = NULL;
void (*sid_update)(short *buf, double nAudioIn, int nSamples) = NULL;
void (*write_sid)(Word ad, Byte dat) = NULL;
void (*SetSidFrequency)(int) = NULL;
void (*SetSidEnable)(bool) = NULL;
void (*SetSidBanked)(bool) = NULL;
bool (*GetSidEnable)(void) = NULL;
SID* (*GetSidPointer)(void) = NULL;
HMODULE hSIDDll = NULL;
void PrepareSID();

// helper for audio buffers
CRITICAL_SECTION csAudioBuf;

// value to fade every clock/16
// this value compares with the recordings I made of the noise generator back in the day
// This is good, but why doesn't this match the math above, though?
#define FADECLKTICK (0.001/9.0)

int nClock = 3579545;					// NTSC, PAL may be at 3546893? - this is divided by 16 to tick
int nCounter[4]= {0,0,0,1};				// 10 bit countdown timer
double nOutput[4]={1.0,1.0,1.0,1.0};	// output scale
int nNoisePos=1;						// whether noise is positive or negative (white noise only)
unsigned short LFSR=0x4000;				// noise shifter (only 15 bit)
int nRegister[4]={0,0,0,0};				// frequency registers
int nVolume[4]={0,0,0,0};				// volume attenuation
double nFade[4]={1.0,1.0,1.0,1.0};		// emulates the voltage drift back to 0 with FADECLKTICK (TODO: what does this mean with a non-zero center?)
										// we should test this against an external chip with a clean circuit.
int max_volume;

// audio
int AudioSampleRate=22050;				// in hz
unsigned int CalculatedAudioBufferSize=22080*2;	// round audiosample rate up to a multiple of 60 (later hzRate is used)

// The tapped bits are XORd together for the white noise
// generator feedback.
// These are the BBC micro version/Coleco? Need to check
// against a real TI noise generator. 
// This matches what MAME uses (although MAME shifts the other way ;) )
int nTappedBits=0x0003;

// logarithmic scale (linear isn't right!)
// the SMS Power example, (I convert below to percentages)
int sms_volume_table[16]={
   32767, 26028, 20675, 16422, 13045, 10362,  8231,  6568,
    5193,  4125,  3277,  2603,  2067,  1642,  1304,     0
};
double nVolumeTable[16];

// return 1 or 0 depending on odd parity of set bits
// function by Dave aka finaldave. Input value should
// be no more than 16 bits.
int parity(int val) {
	val^=val>>8;
	val^=val>>4;
	val^=val>>2;
	val^=val>>1;
	return val&1;
};

// prepare the sound emulation. 
// freq - output sound frequency in hz
void sound_init(int freq) {
	// I don't want max to clip, so I bias it slightly low
#if 0
	double nVol=0.9375;
	// linear scale - may not be correct
	for (int idx=0; idx<16; idx++) {
		nVolumeTable[idx]=nVol;
		nVol-=0.0625;
	}
#else
	// use the SMS power Logarithmic table
	for (int idx=0; idx<16; idx++) {
		// this magic number makes maximum volume (32767) become about 0.9375
		nVolumeTable[idx]=(double)(sms_volume_table[idx])/34949.3333;	
	}
#endif

	AudioSampleRate=freq;
	if (NULL != SetSidFrequency) {
		SetSidFrequency(freq);
	}
}

// change the frequency counter on a channel
// chan - channel 0-3 (3 is noise)
// freq - frequency counter (0-1023) or noise code (0-7)
void setfreq(int chan, int freq) {
	if ((chan < 0)||(chan > 3)) return;

	if (chan==3) {
		// limit noise 
		freq&=0x07;
		nRegister[3]=freq;

		// reset shift register
		LFSR=0x4000;	//	(15 bit)
		switch (nRegister[3]&0x03) {
			// these values work but check the datasheet dividers
			case 0: nCounter[3]=0x10; break;
			case 1: nCounter[3]=0x20; break;
			case 2: nCounter[3]=0x40; break;
			// even when the count is zero, the noise shift still counts
			// down, so counting down from 0 is the same as wrapping up to 0x400
			case 3: nCounter[3]=(nRegister[2]?nRegister[2]:0x400); break;		// is never zero!
		}
	} else {
		// limit freq
		freq&=0x3ff;
		nRegister[chan]=freq;
		// don't update the counters, let them run out on their own
	}
}

// change the volume on a channel
// chan - channel 0-3
// vol - 0 (loudest) to 15 (silent)
void setvol(int chan, int vol) {
	if ((chan < 0)||(chan > 3)) return;

	nVolume[chan]=vol&0xf;
}

// fill the output audio buffer with signed 16-bit values
// nAudioIn contains a fixed value to add to all samples (used to mix in the casette audio)
// (this emu doesn't run speech through there, though, speech gets its own buffer for now)
// TODO: I don't use this anymore and I think it needs to be removed....
void sound_update(short *buf, double nAudioIn, int nSamples) {
	// nClock is the input clock frequency, which runs through a divide by 16 counter
	// The frequency does not divide exactly by 16
	// AudioSampleRate is the frequency at which we actually output bytes
	// multiplying values by 1000 to improve accuracy of the final division (so ms instead of sec)
	double nTimePerClock=1000.0/(nClock/16.0);
	double nTimePerSample=1000.0/(double)AudioSampleRate;
	int nClocksPerSample = (int)(nTimePerSample / nTimePerClock + 0.5);		// +0.5 to round up if needed
	int newdacpos = 0;
	int inSamples = nSamples;

	while (nSamples) {
		// emulate drift to zero
		for (int idx=0; idx<4; idx++) {
			if (nFade[idx] > 0.0) {
				nFade[idx]-=FADECLKTICK*nClocksPerSample;
				if (nFade[idx] < 0.0) nFade[idx]=0.0;
			} else {
				nFade[idx]=0.0;
			}
		}

		// tone channels

		for (int idx=0; idx<3; idx++) {
			// SMS Power says 0 or 1 is a flat output
			// SMS Power is wrong, however, or at least, it
			// doesn't apply to the 9919. Testing with the real
			// TI shows that 0 is the lowest pitch, and 1 is the highest.
			nCounter[idx]-=nClocksPerSample;
			while (nCounter[idx] <= 0) {
				nCounter[idx]+=(nRegister[idx]?nRegister[idx]:0x400);
				nOutput[idx]*=-1.0;
				nFade[idx]=1.0;
			}
			// A little check to eliminate high frequency tones
			// If the frequency is greater than 1/2 the sample rate,
			// then mute it (we'll do that with the nFade value.) 
			// Noises can't get higher than audible frequencies (even with high user defined rates),
			// so we don't need to worry about them.
			if ((nRegister[idx] != 0) && (nRegister[idx] <= (int)(111860.0/(double)(AudioSampleRate/2)))) {
				// this would be too high a frequency, so we'll merge it into the DAC channel (elsewhere)
				// and kill this channel. The reason is that the high frequency ends up
				// creating artifacts with the lower frequency output rate, and you don't
				// get an inaudible tone but awful noise
				nFade[idx]=0.0;
				//nAudioIn += nVolumeTable[nVolume[idx]];	// not strictly right, the high frequency output adds some distortion. But close enough.
				//if (nAudioIn >= 1.0) nAudioIn = 1.0;	// clip
			}
		}

		// noise channel 
		nCounter[3]-=nClocksPerSample;
		while (nCounter[3] <= 0) {
			switch (nRegister[3]&0x03) {
				case 0: nCounter[3]+=0x10; break;
				case 1: nCounter[3]+=0x20; break;
				case 2: nCounter[3]+=0x40; break;
				// even when the count is zero, the noise shift still counts
				// down, so counting down from 0 is the same as wrapping up to 0x400
				// same is with the tone above :)
				case 3: nCounter[3]+=(nRegister[2]?nRegister[2]:0x400); break;		// is never zero!
			}
			nNoisePos*=-1;
			double nOldOut=nOutput[3];
			// Shift register is only kicked when the 
			// Noise output sign goes from negative to positive
			if (nNoisePos > 0) {
				int in=0;
				if (nRegister[3]&0x4) {
					// white noise - actual tapped bits uncertain?
					// This doesn't currently look right.. need to
					// sample a full sequence of TI white noise at
					// a known rate and study the pattern.
					if (parity(LFSR&nTappedBits)) in=0x4000;
					if (LFSR&0x01) {
#if 1
						// the SMSPower documentation says it never goes negative,
						// but (my very old) recordings say white noise does goes negative,
						// and periodic noise doesn't. Need to sit down and record these
						// noises properly and see what they really do. 
						// For now I am going to swing negative to play nicely with
						// the tone channels. 
						// TODO: I need to verify noise vs tone on a clean system.
						// need to test for 0 because periodic noise sets it
						if (nOutput[3] == 0.0) {
							nOutput[3] = 1.0;
						} else {
							nOutput[3]*=-1.0;
						}
#else
						nOutput[3]=1.0;
					} else {
						nOutput[3]=0.0;
#endif
					}
				} else {
					// periodic noise - tap bit 0 (again, BBC Micro)
					// Compared against TI samples, this looks right
					if (LFSR&0x0001) {
						in=0x4000;	// (15 bit shift)
						// TODO: verify periodic noise as well as white noise
						// always positive
						nOutput[3]=1.0;
					} else {
						nOutput[3]=0.0;
					}
				}
				LFSR>>=1;
				LFSR|=in;
			}
			if (nOldOut != nOutput[3]) {
				nFade[3]=1.0;
			}
		}

		// write sample
		nSamples--;
		double output;

		// using division (single voices are quiet!)
		// write out one sample
		output = nOutput[0]*nVolumeTable[nVolume[0]]*nFade[0] +
				nOutput[1]*nVolumeTable[nVolume[1]]*nFade[1] +
				nOutput[2]*nVolumeTable[nVolume[2]]*nFade[2] +
				nOutput[3]*nVolumeTable[nVolume[3]]*nFade[3]
				+ (dac_buffer[newdacpos++] / 255.0);
		// output is now between 0.0 and 5.0, may be positive or negative
		output/=5.0;	// you aren't supposed to do this when mixing. Sorry. :)
		if (newdacpos >= dac_pos) {
			// not enough DAC samples!
			newdacpos--;
		}

		short nSample=(short)((double)0x7fff*output); 
		*(buf++)=nSample; 

#if 0
		static FILE *fp=NULL;
		if (NULL == fp) {
			fp=fopen("C:\\new\\audio.raw", "wb");
		}
		fwrite(&nSample, 2, 1, fp);
#endif

	}
	// roll the dac output buffer
	if (inSamples < dac_pos) {
		memmove(dac_buffer, &dac_buffer[newdacpos], dac_pos-newdacpos);
		dac_pos-=newdacpos;
	} else {
		dac_pos = 0;
	}
}

#if 0
// Apple testing...
//unsigned char nOutput[4]={1,1,1,1};		// output scale
//unsigned char sms_volume_table[16] = {
   //31, 25, 20, 16, 13, 10,  8,  6,
    //5,  4,  3,  3,  2,  2,  1,  0
 //};
void sound_update(short *buf, double nAudioIn, int nSamples) {
	// nClock is the input clock frequency, which runs through a divide by 16 counter
	// The frequency does not divide exactly by 16
	// AudioSampleRate is the frequency at which we actually output bytes
	double nTimePerClock=1.0/(nClock/16.0);
	double nTimePerSample=1.0/(double)AudioSampleRate;
	int nClocksPerSample = (int)(nTimePerSample / nTimePerClock + 0.5);		// +0.5 to round up if needed

	unsigned char idx;
	static unsigned char oldout=0x80;
	static unsigned char olddelta = 0;
	static unsigned char noisedir=1;

	while (nSamples) {
		// tone channels
		for (idx=0; idx<3; idx++) {
			// on the TI/Coleco, a freq of 0 is really 0x400,
			// not a flat line like on the SMS
			nCounter[idx]-=nClocksPerSample;
			while (nCounter[idx] <= 0) {
				nCounter[idx]+=(nRegister[idx]?nRegister[idx]:0x400);
				nOutput[idx]=!nOutput[idx];
			}
		}

		// noise channel 
		nCounter[3]-=nClocksPerSample;
		while (nCounter[3] <= 0) {
			switch (nRegister[3]&0x03) {
				case 0: nCounter[3]+=0x10; break;
				case 1: nCounter[3]+=0x20; break;
				case 2: nCounter[3]+=0x40; break;
				// even when the count is zero, the noise shift still counts
				// down, so counting down from 0 is the same as wrapping up to 0x400
				// same is with the tone above :)
				case 3: nCounter[3]+=(nRegister[2]?nRegister[2]:0x400); break;		// is never zero!
			}
			nNoisePos= !nNoisePos;
			// Shift register is only kicked when the 
			// Noise output sign goes from negative to positive
			if (nNoisePos) {
				unsigned int in=0;
				if (nRegister[3]&0x4) {
					// white noise - actual tapped bits uncertain?
					// tapped bits are 0x03 - if they are
					// different, then inject a new 0x4000 bit
					if ((LFSR&0x0001) != ((LFSR&0x0002)>>1)) {
						in = 0x4000;
					}
					if (LFSR&0x01) {
						nOutput[3]=1;
					} else {
						if (nOutput[3]) {
							noisedir=!noisedir;
						}
						nOutput[3]=0;
					}
				} else {
					// periodic noise - tap bit 0 (again, BBC Micro)
					// Compared against TI samples, this looks right
					if (LFSR&0x0001) {
						in=0x4000;	// (15 bit shift)
						nOutput[3]=1;
					} else {
						if (nOutput[3]) {
							noisedir=!noisedir;
						}
						nOutput[3]=0;
					}
				}
				LFSR>>=1;
				LFSR|=in;
			}
		}

		// write sample
		nSamples--;

		// Calculate this tick
		idx = 0;
		if (nOutput[0]) {
			idx+=sms_volume_table[nVolume[0]];
		} else {
			idx-=sms_volume_table[nVolume[0]];
		}
		if (nOutput[1]) {
			idx+=sms_volume_table[nVolume[1]];
		} else {
			idx-=sms_volume_table[nVolume[1]];
		}
		if (nOutput[2]) {
			idx+=sms_volume_table[nVolume[2]];
		} else {
			idx-=sms_volume_table[nVolume[2]];
		}
		if (nOutput[3]) {
			if (noisedir) {
				idx+=sms_volume_table[nVolume[3]];
			} else {
				idx-=sms_volume_table[nVolume[3]];
			}
		}
		// now, we have in idx a value from -64 to +64
#if 0
		// we are only concerned with its sign
		// if the sign of the output has changed, 'tick' the speaker
		if (oldout != (idx&0x80)) {
			oldout = idx&0x80;
//			idx=SPKR;
		}
#else
		// if the sign of the output delta has changed, 'tick' the speaker
		if (((idx-oldout)&0x80) != olddelta) {
			olddelta=(idx-oldout)&0x80;
		}
		oldout = idx;
#endif
		double output = (oldout&0x80)?0.5:-0.5;

		short nSample=(short)((double)0x7fff*output); 
			*(buf++)=nSample; 
	}

}
#endif


void SetSoundVolumes() {
	// set overall volume (this is not a sound chip effect, it's directly related to DirectSound)
	// it sets the maximum volume of all channels
	EnterCriticalSection(&csAudioBuf);

	int nRange=(MIN_VOLUME*max_volume)/100;	// negative
	if (NULL != soundbuf) {
		soundbuf->SetVolume(MIN_VOLUME - nRange);
	}
	if (NULL != sidbuf) {
		sidbuf->SetVolume(MIN_VOLUME - nRange);
	}
	if (NULL != speechbuf) {
		speechbuf->SetVolume(MIN_VOLUME - nRange);
	}

	LeaveCriticalSection(&csAudioBuf);
}

void MuteAudio() {
	// set overall volume to muted (this is not a sound chip effect, it's directly related to DirectSound)
	// it sets the maximum volume of all channels
	EnterCriticalSection(&csAudioBuf);

	if (NULL != soundbuf) {
		soundbuf->SetVolume(DSBVOLUME_MIN);
	}
	if (NULL != sidbuf) {
		sidbuf->SetVolume(DSBVOLUME_MIN);
	}
	if (NULL != speechbuf) {
		speechbuf->SetVolume(DSBVOLUME_MIN);
	}

	LeaveCriticalSection(&csAudioBuf);
}

void resetDAC() { 
	// empty the buffer and reset the pointers
	EnterCriticalSection(&csAudioBuf);
		memset(&dac_buffer[0], 0x00, sizeof(dac_buffer));
		dac_pos=0;
		dacupdatedistance=0.0;
	LeaveCriticalSection(&csAudioBuf);
}

void updateDACBuffer(int nCPUCycles) {
	static int totalCycles = 0;

	if (max_cpf < DEFAULT_60HZ_CPF) {
		totalCycles = 0;
		return;	// don't do it if running slow
	}

	// because it is an int (nominally 3,000,000 - this makes the slider work)
	int CPUCYCLES = max_cpf * hzRate;

	totalCycles+=nCPUCycles;
	double fdist = (double)totalCycles / ((double)CPUCYCLES/AudioSampleRate);
	if (fdist < 1.0) return;				// don't even bother
	dacupdatedistance += fdist;
	totalCycles = 0;		// we used them all.

	int distance = (int)dacupdatedistance;
	dacupdatedistance -= distance;
	if (dac_pos + distance >= sizeof(dac_buffer)) {
		debug_write("DAC Buffer overflow...");
		dac_pos=0;
		return;
	}
	int average = 1;
	double value = nDACLevel;

	for (int idx=0; idx<3; idx++) {
		// A little check for eliminate high frequency tones
		// If the frequency is greater than 1/2 the sample rate, it's DAC.
		// Noises can't get higher than audible frequencies (even with high user defined rates),
		// so we don't need to worry about them.
		if ((nRegister[idx] != 0) && (nRegister[idx] <= (int)(111860.0/(double)(AudioSampleRate/2)))) {
			// this would be too high a frequency, so we'll merge it into the DAC channel
			// and kill this channel. The reason is that the high frequency ends up
			// creating artifacts with the lower frequency output rate, and you don't
			// get an inaudible tone but awful noise
			value += nVolumeTable[nVolume[idx]];	// not strictly right, the high frequency output adds some distortion. But close enough.
			average++;
		}
	}
	value /= average;
	unsigned char out = (unsigned char)(value * 255);
	EnterCriticalSection(&csAudioBuf);
		memset(&dac_buffer[dac_pos], out, distance);
		dac_pos+=distance;
	LeaveCriticalSection(&csAudioBuf);
}

void UpdateSoundBuf(LPDIRECTSOUNDBUFFER soundbuf, void (*sound_update)(short *,double,int), StreamData *pDat) {
	DWORD iRead, iWrite;
	short *ptr1, *ptr2;
	DWORD len1, len2;
	static char *pRecordBuffer = NULL;
	static int nRecordBufferSize = 0;

	EnterCriticalSection(&csAudioBuf);

	// DirectSound iWrite pointer just points a 'safe distance' ahead of iRead, usually about 15ms
	// so we need to maintain our own count of where we are writing
	soundbuf->GetCurrentPosition(&iRead, &iWrite);
//	debug_write("Read/Write sound buf: %5d/%5d", iRead, iWrite);
	if (pDat->nLastWrite == 0xffffffff) {
		pDat->nLastWrite=iWrite;
	}
	
	// arbitrary - try to use a dynamic jitter buffer
	int nWriteAhead;
	if (pDat->nLastWrite<iRead) {
		nWriteAhead=pDat->nLastWrite+CalculatedAudioBufferSize-iRead;
	} else {
		nWriteAhead=pDat->nLastWrite-iRead;
	}
	nWriteAhead/=CalculatedAudioBufferSize/(hzRate);
	
	if (nWriteAhead > 29) {
		// this more likely means we actually fell behind!
		if (pDat->nMinJitterFrames < 10) {
#ifdef _DEBUG
			debug_write("Fell behind, increasing minimum jitter to %d", pDat->nMinJitterFrames);
#endif
			pDat->nMinJitterFrames++;
		} 
		if (pDat->nJitterFrames < pDat->nMinJitterFrames) {
			pDat->nJitterFrames=pDat->nMinJitterFrames;
		}
		nWriteAhead=0;
		pDat->nLastWrite=iWrite;
	}

//	debug_write("WriteAhead at %d - lastwrite %5d, iread %5d", nWriteAhead, pDat->nLastWrite, iRead);

	// update jitter buffer if we fall behind, but no more than 15 frames (that would only be 4 updates a second!)
	if ((nWriteAhead < 1) && (pDat->nJitterFrames < 15)) {
		pDat->nJitterFrames++;
#ifdef _DEBUG
//		debug_write("Grow jitter buffer to %d frames (writeahead %d)", pDat->nJitterFrames, nWriteAhead);
#endif
	} else if ((nWriteAhead > pDat->nJitterFrames+1) && (pDat->nJitterFrames > pDat->nMinJitterFrames)) {
		// maybe we can shrink the buffer?
		pDat->nJitterFrames--;
#ifdef _DEBUG
//		debug_write("Shrink jitter buffer to %d frames (writeahead %d)", pDat->nJitterFrames, nWriteAhead);
#endif
	} else if ((nWriteAhead > pDat->nMinJitterFrames/2+1) && (pDat->nMinJitterFrames > 2)) {
		pDat->nMinJitterFrames--;
#ifdef _DEBUG
		debug_write("Shrink min jitter frames to %d (writeahead %d)", pDat->nMinJitterFrames, nWriteAhead);
#endif
	}

	// check AVI buffer size is sufficient
	if (Recording) {
		if ((unsigned)nRecordBufferSize < CalculatedAudioBufferSize/(hzRate)) {
			if (NULL != pRecordBuffer) free(pRecordBuffer);
			pRecordBuffer = (char*)malloc(CalculatedAudioBufferSize/(hzRate));
			nRecordBufferSize=CalculatedAudioBufferSize/(hzRate);
		}
	}

	// doing it all right here limits the CPU's ability to interact
	// but luckily we should NORMALLY only do one frame at a time
	// as noted, the goal is to get it on a per-scanline basis
	while (nWriteAhead < pDat->nJitterFrames) {
		if (SUCCEEDED(soundbuf->Lock(pDat->nLastWrite, CalculatedAudioBufferSize/(hzRate), (void**)&ptr1, &len1, (void**)&ptr2, &len2, 0))) {
			if (len1 > 0) {
				sound_update(ptr1, nDACLevel, len1/2);		// divide by 2 for 16 bit samples
			}
			if (len2 > 0) {
				sound_update(ptr2, nDACLevel, len2/2);		// divide by 2 for 16 bit samples
			}

			if ((Recording)&&(pRecordBuffer)) {
				if (len1>0) {
					memcpy(pRecordBuffer, ptr1, len1);
				}
				if (len2>0) {
					memcpy(pRecordBuffer+len1, ptr2, len2);
				}
                
                // TODO: not sure what's wrong.. if I write a fake buffer full of audio samples, it works fine.
				// but this audio is jittery and full of gaps!
				WriteAudioFrame(pRecordBuffer, len1+len2);
			}

			// carry on
			soundbuf->Unlock(ptr1, len1, ptr2, len2);

//			debug_write("Wrote %d bytes to nLastWrite %d (%x)", len1, pDat->nLastWrite, ptr1);

			// update write pointer
			pDat->nLastWrite += CalculatedAudioBufferSize/(hzRate);
			if (pDat->nLastWrite >= CalculatedAudioBufferSize) {
				pDat->nLastWrite-=CalculatedAudioBufferSize;
			}
		} else {
			debug_write("Failed to lock sound buffer!");
			break;
		}
		nWriteAhead++;
		
#if 0
		if (nWriteAhead < pDat->nJitterFrames) {
			soundbuf->GetCurrentPosition(&iRead, &iWrite);
//			debug_write("Read/Write sound buf: %5d/%5d", iRead, iWrite);
		}
#endif
	}

	LeaveCriticalSection(&csAudioBuf);
}

////////////////////////////////////////////////////////////
// SID Interface

// TODO: this pulls in a Windows 'SID' structure, not the actual SID chip
//SID *g_mySid = NULL;

// try to load the SID DLL
void PrepareSID() {
	// load the Speech DLL
	hSIDDll=LoadLibrary("SIDDll.dll");
	if (NULL == hSIDDll) {
		debug_write("Failed to load SID library.");
	} else {
		InitSid=(void (*)(void))GetProcAddress(hSIDDll, "InitSid");
		sid_update=(void (*)(short*,double,int))GetProcAddress(hSIDDll, "sid_update");
		write_sid=(void (*)(Word,Byte))GetProcAddress(hSIDDll, "write_sid");
		SetSidFrequency=(void (*)(int))GetProcAddress(hSIDDll, "SetSidFrequency");
		SetSidEnable=(void (*)(bool))GetProcAddress(hSIDDll, "SetSidEnable");
		SetSidBanked=(void (*)(bool))GetProcAddress(hSIDDll, "SetSidBanked");
		GetSidEnable=(bool (*)(void))GetProcAddress(hSIDDll, "GetSidEnable");
        // not available in all versions of the DLL, optional
        GetSidPointer=(SID* (*)(void))GetProcAddress(hSIDDll, "GetSidPointer");

		if ((NULL == InitSid) || (NULL == sid_update) || (NULL == write_sid) || (NULL == SetSidFrequency) || (NULL == SetSidEnable) || (NULL == SetSidBanked) || (NULL == GetSidEnable)) {
			debug_write("Failed to find all functions, skipping SID DLL");
			hSIDDll = NULL;
		}
	}
}
