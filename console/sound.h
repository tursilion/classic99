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

#include <dsound.h>

typedef unsigned __int8 UINT8;
typedef unsigned __int8 Byte;
typedef unsigned __int16 Word;
typedef unsigned __int32 DWord;

// must be higher than DSBVOLUME_MIN!
#define MIN_VOLUME -2000

struct StreamData {
	StreamData() {
		nLastWrite=0xffffffff;
		nJitterFrames=4;		// jitter buffer (increments on first entry, then if we fall behind!)
		nMinJitterFrames=4;		// minimum jitter buffer, increases only if we fall behind!
	};

	DWORD nLastWrite;
	int nJitterFrames;			// jitter buffer (increments on first entry, then if we fall behind!)
	int nMinJitterFrames;		// minimum jitter buffer, increases only if we fall behind!
};

extern CRITICAL_SECTION csAudioBuf;

void sound_init(int freq);
void setfreq(int chan, int freq);
void setvol(int chan, int vol);
void sound_update(short *buf, double nAudioIn, int nSamples);
void SetSoundVolumes();
void MuteAudio();
void UpdateSoundBuf(LPDIRECTSOUNDBUFFER soundbuf, void (*sound_update)(short *,double,int), StreamData *pDat);
void resetDAC();
void updateDACBuffer(int nCPUCycles);

// SID DLL interface
extern void (*InitSid)();
extern void (*sid_update)(short *buf, double nAudioIn, int nSamples);
extern void (*write_sid)(Word ad, Byte dat);
extern void (*SetSidFrequency)(int);
extern void (*SetSidEnable)(bool);
extern void (*SetSidBanked)(bool);
extern bool (*GetSidEnable)(void);
extern SID* (*GetSidPointer)(void);
void PrepareSID();
