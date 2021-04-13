/*
From Matt - check this decrement:
2:15 PM] dnotq: The counters don't consider the 0-count as it's own state.  It was very interesting that I literally took the AY up-counters, changed them to count down (changed ++ to -), and changed the reset when count >= period condition to load-period when count = 0, and they just worked.
[2:16 PM] dnotq: It took me a while to realize this, since when you mentioned 0-count is maximum period, that threw me for a bit.
[2:17 PM] tursilion: yeah, I said that. But I think the SN was the first and everyone else cloned them ;)
[2:17 PM] dnotq: But the "compare-to-0 and load" is done during the period of the count.  Easier to show than to explain in English.
[2:18 PM] dnotq: It was just neat to see the same mechanism work in both directions.  Everyone else is loading the count - 1 and crap like that into their counters, or making special cases for the 0 count, etc.
[2:19 PM] tursilion: huh, weird. I don't understand your "during the period of the count" bit, as you implied ;)
[2:21 PM] dnotq: It means that the reset-and-count happens in the same time period as a single count.  So when you hit zero, the period is loaded, then decremented immediately.  So the counter being "zero" is never try for a full count period.
[2:21 PM] dnotq: This is what allows a count of 1 to actually cut the frequency in half.
[2:22 PM] dnotq: If zero was true for a full count cycle, a count of one would be: 1, 0 (toggle), 1, 0 (toggle).  And that is actually a divide by 2.
[2:26 PM] dnotq: It is done in the real IC via the asynchronous nature of the transistors, and the way any signal transition can be a clock, set, or reset signal.  But I reproduced the functionality in synchronous HDL and the counters just work as they should.  No special tests, edge cases, loading or comparing period-1, etc.
[2:27 PM] tursilion: ah, I see. That makes sense :) I should check if Classic99 does that right
*/


// Testing on a real TI:
// The sound chip itself outputs an always positive sound wave peaking roughly at 2.8v.
// Output values are roughly 1v peak-to-peak (I measured 0 attenuation at roughly 720mV)
// However, it's hard to get good readings in the TI console due to lots of high frequency
// noise, which is also roughly 1v peak to peak and starts at around 1khz or 2khz
// The center point at output was closer to 1v, nevertheless, it's all positive.
// Two interesting points - rather than a center point, the maximum voltage appeared
// to be consistent, and the minimum voltage crept higher as attenuation increased.
// The other interesting point is that MESS seems to output all negative voltage,
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
// Futher note: the TI CPU is halted by the sound chip during the write,
// so not only is overrun impossible, but the system is halted for a significant
// time. This is now emulated on the CPU side.
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
void rampVolume(LPDIRECTSOUNDBUFFER ds, long newVol);       // to reduce up/down clicks

// hack for now - a little DAC buffer for cassette ticks and CPU modulation
// fixed size for now
extern double nDACLevel;
unsigned char dac_buffer[1024*1024];
int dac_pos = 0;
double dacupdatedistance = 0.0;
double dacramp = 0.0;       // a little slider to ramp in the DAC volume so it doesn't click so loudly at reset

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

double nOutput[4]={1.0,1.0,1.0,1.0};	// output scale
// logarithmic scale (linear isn't right!)
// the SMS Power example, (I convert below to percentages)
int sms_volume_table[16]={
   32767, 26028, 20675, 16422, 13045, 10362,  8231,  6538,
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

    resetDAC();

    // and set up the audio rate
	AudioSampleRate=freq;

    // and finally tell the SID plugin
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

// this #if is here for the Apple2 experiment...
#if 1
// fill the output audio buffer with signed 16-bit values
// nAudioIn contains a fixed value to add to all samples (used to mix in the casette audio)
// (this emu doesn't run speech through there, though, speech gets its own buffer for now)
// TODO: I don't use this anymore and I think it needs to be removed.... (what did I mean by this??)
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
            // Further Testing with the chip that SMS Power's doc covers (SN76489)
            // 0 outputs a 1024 count tone, just like the TI, but 1 DOES output a flat line.
            // On the TI (SN76494, I think), 1 outputs the highest pitch (count of 1)
            // However, my 99/4 pics show THAT machine with an SN76489! 
            // My plank TI has an SN94624 (early name? TMS9919 -> SN94624 -> SN76494 -> SN76489)
            // And my 2.2 QI console has an SN76494!
            // So maybe we can't say with certainty which chip is in which machine?
            // Myths and legends:
            // - SN76489 grows volume from 2.5v down to 0 (matches my old scopes of the 494), but SN76489A grows volume from 0 up.
            // - SN76496 is the same as the SN7689A but adds the Audio In pin (all TI used chips have this, even the older ones)
            // So right now, I believe there are two main versions, differing largely by the behaviour of count 0x001:
            // Original (high frequency): TMS9919, SN94624, SN76494?
            // New (flat line): SN76489, SN76489A, SN76496
			nCounter[idx]-=nClocksPerSample;
			while (nCounter[idx] <= 0) {    // TODO: should be able to do this without a loop, it would be faster (well, in the rare cases it needs to loop)!
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
				+ ((dac_buffer[newdacpos++] / 255.0)*dacramp);
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
        if (dacramp < 1.0) {
            dacramp+=0.01;  // slow, slow ramp in
            if (dacramp > 1.0) dacramp = 1.0;
        }
	} else {
		dac_pos = 0;
	}
}

#else
// Apple testing...
// Overall, I think this is fairly decent. Yeah, it gets crappy with really
// complex music, but I don't know how much we can expect out of the 1-bit speaker.
// It's good enough that I almost shipped a release of Classic99 with it turned on ;)
unsigned char nOutputApp[4]={1,1,1,1};		// output scale
unsigned char app_volume_table[16] = {
   31, 25, 20, 16, 13, 10,  8,  6,
   5,  4,  3,  3,  2,  2,  1,  0
}; 
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
        /**/
        bool signchanged = false;   // only used in one output mode idea, which does not work
        int loudest = 15;
        for (int idx=0; idx<4; ++idx) {
            if (nVolume[idx] < loudest) loudest=nVolume[idx];
        }
        loudest*=2;
        if (loudest > 14) loudest=14;
        /**/

		// tone channels
		for (idx=0; idx<3; idx++) {
			// on the TI/Coleco, a freq of 0 is really 0x400,
			// not a flat line like on the SMS
			nCounter[idx]-=nClocksPerSample;
			while (nCounter[idx] <= 0) {
				nCounter[idx]+=(nRegister[idx]?nRegister[idx]:0x400);
				nOutputApp[idx]=!nOutputApp[idx];
                if (nVolume[idx] <= loudest) {
                    signchanged = true;
                }
			}
		}

		// noise channel 
		nCounter[3]-=nClocksPerSample;
		while (nCounter[3] <= 0) {
            if (nVolume[3] < loudest) {
                signchanged = true;
            }

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
						nOutputApp[3]=1;
					} else {
						if (nOutputApp[3]) {
							noisedir=!noisedir;
						}
						nOutputApp[3]=0;
					}
				} else {
					// periodic noise - tap bit 0 (again, BBC Micro)
					// Compared against TI samples, this looks right
					if (LFSR&0x0001) {
						in=0x4000;	// (15 bit shift)
						nOutputApp[3]=1;
					} else {
						if (nOutputApp[3]) {
							noisedir=!noisedir;
						}
						nOutputApp[3]=0;
					}
				}
				LFSR>>=1;
				LFSR|=in;
			}
		}

		// write sample
		nSamples--;

		// Calculate this tick
		int cnt = 0;
		if (nOutputApp[0]) {
			cnt+=app_volume_table[nVolume[0]];
		} else {
			cnt-=app_volume_table[nVolume[0]];
		}
        if (nRegister[1]!=nRegister[0]) {
            // don't add if the frequency was already playing on an earlier channel
		    if (nOutputApp[1]) {
			    cnt+=app_volume_table[nVolume[1]];
		    } else {
			    cnt-=app_volume_table[nVolume[1]];
		    }
        }
        if ((nRegister[2]!=nRegister[0])&&(nRegister[2]!=nRegister[1])) {
            // don't add if the frequency was already playing on an earlier channel
		    if (nOutputApp[2]) {
			    cnt+=app_volume_table[nVolume[2]];
		    } else {
			    cnt-=app_volume_table[nVolume[2]];
		    }
        }
		if (nOutputApp[3]) {
			if (noisedir) {
				cnt+=app_volume_table[nVolume[3]];
			} else {
				cnt-=app_volume_table[nVolume[3]];
			}
		}
		// now, we have in idx a value from -64 to +64
static int sampleCnt = 0;

        // These are both awful on complex sounds and both
        // work pretty well on simple ones (including chords)
#if 0
		// we are only concerned with its sign
		// if the sign of the output has changed, 'tick' the speaker
        // very noisey - basically the cassette without a dead zone
		if (oldout != (idx&0x80)) {
			oldout = idx&0x80;
//			idx=SPKR;
            olddelta = oldout;
		}
#elif 1
		// if the sign of the output delta has changed, 'tick' the speaker
        // require a minimum delta before we react - this helps a bit

        // This sounds the cleanest of them so far, but it does not cope well
        // when the same frequency is played on multiple channels, adds noise.
        // But it handles chords the best

        idx = (unsigned char)(cnt&0xff);
        int delta = idx-oldout;
        if (delta < 0) delta=-delta;
        if (delta > 8) {
		    if (((idx-oldout)&0x80) != olddelta) {
			    olddelta=(idx-oldout)&0x80;
		    }
		    oldout = idx;
        }
#elif 0
        // how about delta with a drift to zero?
        // this works, but not sure if it's better than
        // the other delta. It still sometimes drops voices.
       
        int delta = cnt-sampleCnt;
        if (delta < 0) {
            delta=-delta;
            sampleCnt--;
        } else if (delta > 0) {
            sampleCnt++;
        }
        if (delta > 8) {
            // ticking instead of setting makes the sound very muddied and wrong
		    oldout = (unsigned char)cnt;
        }
#elif 0
        // try to copy the cassette circuit which looks at zero crossings with a ~10% dead zone
        
        // tick for change - this is better then the delta was...
        // need to tick on every change else the frequency is wrong
        // We need the dead zone else it's pretty awful sounding
        // (Actually with the correct scaling this is really bad... muffled)
#define DEADZONE 4
        if (cnt > DEADZONE) {
            if (olddelta < 0) {
                oldout = ~oldout;
                olddelta = 1;
            }
        } else if (cnt < -DEADZONE) {
            if (olddelta >= 0) {
                oldout = ~oldout;
                olddelta = -1;
            }
        }
#elif 0
        // if /any/ channel transitioned, then tick
        // this does NOT work, sounds more like a modem than harmony...
        if (signchanged) {
            if (oldout&0x80) {
                oldout = 0;
            } else {
                oldout = 0x80;
            }
	    }
#else
        // play out the four voices statically with arpeggio
        // I doubt the Apple can do that much...
        // 800 at 44100 samples/second is about 55hz cycling
        // frankly this isn't too bad... it sounds the most
        // correct, but the stuttering is kind of annoying
#define TONELEN 800
        switch (((++sampleCnt)/TONELEN)%4) {
        case 0: if ((nOutputApp[0])&&(nVolume[0] < 12)) oldout=0x80; else oldout=0x00; break;
        case 1: if ((nOutputApp[1])&&(nVolume[1] < 12)) oldout=0x80; else oldout=0x00; break;
        case 2: if ((nOutputApp[2])&&(nVolume[2] < 12)) oldout=0x80; else oldout=0x00; break;
        case 3:
		    if ((nOutputApp[3])&&(nVolume[3] < 12)) {
			    if (noisedir) {
				    oldout=0x80;
			    } else {
			        oldout=0;
			    }
		    }
        }
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
		rampVolume(soundbuf, MIN_VOLUME - nRange);
	}
	if (NULL != sidbuf) {
		rampVolume(sidbuf, MIN_VOLUME - nRange);
	}
	if (NULL != speechbuf) {
		rampVolume(speechbuf, MIN_VOLUME - nRange);
	}

	LeaveCriticalSection(&csAudioBuf);
}

void MuteAudio() {
	// set overall volume to muted (this is not a sound chip effect, it's directly related to DirectSound)
	// it sets the maximum volume of all channels
	EnterCriticalSection(&csAudioBuf);

	if (NULL != soundbuf) {
		rampVolume(soundbuf, DSBVOLUME_MIN);
	}
	if (NULL != sidbuf) {
		rampVolume(sidbuf, DSBVOLUME_MIN);
	}
	if (NULL != speechbuf) {
		rampVolume(speechbuf, DSBVOLUME_MIN);
	}

	LeaveCriticalSection(&csAudioBuf);
}

void rampVolume(LPDIRECTSOUNDBUFFER ds, long newVol) {
    // want to finish in this many steps, as few as possible
    // There is probably no harm to this, but it does NOT affect the startup click...
    // It makes some impact on reset (but the click on finishing reset still happens)
    // and seems to resolve the shutdown click. It does slow things a bit.
    // The main click is caused by the DAC, so, I've got to do a little extra ramp
    // when the system first starts up just for that. That one we'll do live.
    // TODO: still not convinced of this, causes a flutter when it fades out.
    // Also causes a delay in processing, times every channel involved...
    const long step = (DSBVOLUME_MAX-DSBVOLUME_MIN);    // todo: normally divide this by number of steps, with it set to one step it's doing pretty much nothing
    long vol = newVol;
    
    if (NULL == ds) return;
    if (newVol > DSBVOLUME_MAX) newVol = DSBVOLUME_MAX;
    if (newVol < DSBVOLUME_MIN) newVol = DSBVOLUME_MIN;

    ds->GetVolume(&vol);

    // only one of these loops should run
    while (vol > newVol) {
        vol -= step;
        if (vol < newVol) vol=newVol;
        ds->SetVolume(vol);
        Sleep(10);
    }
    while (vol < newVol) {
        vol += step;
        if (vol > newVol) vol=newVol;
        ds->SetVolume(vol);
        Sleep(10);
    }
}

void resetDAC() { 
	// empty the buffer and reset the pointers
    MuteAudio();
	EnterCriticalSection(&csAudioBuf);
		memset(&dac_buffer[0], (unsigned char)(nDACLevel*255), sizeof(dac_buffer));
		dac_pos=0;
		dacupdatedistance=0.0;
        dacramp=0.0;
	LeaveCriticalSection(&csAudioBuf);
    SetSoundVolumes();
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
        if (ThrottleMode == THROTTLE_NORMAL) {
    		debug_write("DAC Buffer overflow...");
        }
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
		debug_write("Grow jitter buffer to %d frames (writeahead %d)", pDat->nJitterFrames, nWriteAhead);
#endif
	} else if ((nWriteAhead > pDat->nJitterFrames+1) && (pDat->nJitterFrames > pDat->nMinJitterFrames)) {
		// maybe we can shrink the buffer?
		pDat->nJitterFrames--;
#ifdef _DEBUG
		debug_write("Shrink jitter buffer to %d frames (writeahead %d)", pDat->nJitterFrames, nWriteAhead);
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
