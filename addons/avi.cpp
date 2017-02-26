//
// (C) 2004 Mike Brent aka Tursi aka HarmlessLion.com
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
// avitest.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <vfw.h>
#include <stdio.h>
#include <stdlib.h>

PAVIFILE myAvi;																// pointer to AVI handle
PAVISTREAM myStream;														// pointer to AVI Stream
PAVISTREAM myAudioStream;													// pointer to AVI Stream
AVISTREAMINFO si;															// info about the stream (video)
AVISTREAMINFO ai;															// info about the stream (audio)
BITMAPINFOHEADER bi, *biout;												// format of a frame
COMPVARS myComp;
long frame;																	// frame to write
long audioframe;
static bool bUsingAudio = false;
int RecordFrame = 0;

CRITICAL_SECTION csAVI;

extern unsigned int *framedata;											// data to write (in words, each is 1 pixel)
extern int Recording;
extern int AudioSampleRate;
extern HWND myWnd;

extern int InitAvi(bool bWithAudio);
extern void WriteFrame();
extern void CloseAVI();
extern void debug_write(char*, ...);
extern void ConfigAVI();

extern char AVIFileName[];

int InitAvi(bool bWithAudio);
void WriteFrame();
void CloseAVI();

int InitAvi(bool bWithAudio)
{
	int ret;																	// return code from functions
	DWORD dwFormatSize;
	void *h;
	static bool bInited=false;

	if (!bInited) {
		InitializeCriticalSection(&csAVI);
	}

	EnterCriticalSection(&csAVI);

	AVIFileInit();																// init AVI library
	
	bUsingAudio = bWithAudio;
	debug_write("AVI: %s, Audio %dHz, %sabled", AVIFileName, AudioSampleRate, bWithAudio?"En":"Dis");

	ret=AVIFileOpen(&myAvi, AVIFileName, OF_CREATE | OF_WRITE, NULL);			// open and create the file
	
	if (0 != ret)
	{
		MessageBox(myWnd, "Can't open AVI file", "Classic99 Error", MB_OK);
		debug_write("Failed to open AVI, code %d", ret);
		AVIFileExit();															// exit AVI library
		LeaveCriticalSection(&csAVI);
		return 1;
	}

	si.fccType=streamtypeVIDEO;													// video stream
	si.fccHandler = mmioFOURCC('M', 'S', 'V', 'C');								// (MSVC) 4 character code of compressor
	si.dwFlags=NULL;
	si.dwCaps=NULL;
	si.wPriority=0;
	si.wLanguage=NULL;
	si.dwScale=1;
	si.dwRate=15;																// 15 fps
	si.dwStart=0;
	si.dwLength=0;																// how long?
	si.dwInitialFrames=0;
	si.dwSuggestedBufferSize=0;
	si.dwQuality=1000;															// default quality, else 0-10000
	si.dwSampleSize=0;															// video streams vary in sample size
	si.dwEditCount=0;
	si.dwFormatChangeCount=0;
	strcpy(si.szName,"Classic99Movie");											// description

	ret=AVIFileCreateStream(myAvi, &myStream, &si);								// create the video stream
	
	if (0 != ret)
	{
		MessageBox(myWnd, "Can't create video AVI stream", "Classic99 Error", MB_OK);
		debug_write("Failed to create video stream, code %d", ret);

		AVIFileRelease(myAvi);													// release the file
		AVIFileExit();															// exit AVI library
		LeaveCriticalSection(&csAVI);

		return 1;
	}
	
	if (bUsingAudio) {
		ai.fccType=streamtypeAUDIO;													// audio stream
		ai.fccHandler = 0;															// 4 character code of compressor (not used for audio)
		ai.dwFlags=NULL;
		ai.dwCaps=NULL;
		ai.wPriority=0;
		ai.wLanguage=NULL;
		ai.dwScale=2;																// equals blockalign in wave format
		ai.dwRate=AudioSampleRate*ai.dwScale;										// data rate (used for positioning stream?
		ai.dwStart=0;
		ai.dwLength=0;																// how long?
		ai.dwInitialFrames=0;
		ai.dwSuggestedBufferSize=0;
		ai.dwQuality=-1;															// default quality, else 0-10000
		ai.dwSampleSize=2;															// 16-bit samples
		ai.dwEditCount=0;
		ai.dwFormatChangeCount=0;
		strcpy(ai.szName,"Classic99Sound");											// description

		ret=AVIFileCreateStream(myAvi, &myAudioStream, &ai);						// create the audio stream
		
		if (0 != ret)
		{
			MessageBox(myWnd, "Can't create audio AVI stream", "Classic99 Error", MB_OK);
			debug_write("Failed to create audio stream, code %d", ret);

			AVIFileRelease(myAvi);													// release the file
			AVIFileExit();															// exit AVI library
			LeaveCriticalSection(&csAVI);

			return 1;
		}

		static WAVEFORMATEX pcmwf;
		ZeroMemory(&pcmwf, sizeof(pcmwf));
		pcmwf.wFormatTag = WAVE_FORMAT_PCM;		// wave file
		pcmwf.nChannels=1;						// 1 channel (mono)
		pcmwf.nSamplesPerSec=AudioSampleRate;	// 22khz
		pcmwf.nBlockAlign=2;					// 2 bytes per sample * 1 channel
		pcmwf.nAvgBytesPerSec=pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
		pcmwf.wBitsPerSample=16;				// 16 bit samples
		pcmwf.cbSize=0;							// always zero (extra data size, not struct size)

		ret=AVIStreamSetFormat(myAudioStream, 0, &pcmwf, sizeof(WAVEFORMATEX));
		if (0 != ret)
		{
			MessageBox(myWnd, "Can't set audio AVI Format", "Classic99 Error", MB_OK);
			debug_write("Failed to set audio AVI format, code %d", ret);
			CloseAVI();
			LeaveCriticalSection(&csAVI);
			
			return 1;
		}
	}

	bi.biSize=sizeof(bi);
	bi.biWidth=256+16;
	bi.biHeight=192+16;
	bi.biPlanes=1;
	bi.biBitCount=32;
	bi.biCompression=BI_RGB;
	bi.biSizeImage=0;
	bi.biXPelsPerMeter=0;
	bi.biYPelsPerMeter=0;
	bi.biClrUsed=0;
	bi.biClrImportant=0;

	ZeroMemory(&myComp, sizeof(COMPVARS));
	myComp.cbSize=sizeof(COMPVARS);

	// turns out AVISaveOptions can do this too, but this works
	if (!ICCompressorChoose(myWnd, ICMF_CHOOSE_DATARATE | ICMF_CHOOSE_KEYFRAME, &bi, NULL, &myComp, NULL))
	{
		debug_write("Cancelled compression request");
		CloseAVI();
		LeaveCriticalSection(&csAVI);

		return 1;
	}

	// and AVIMakeCompressedStream does stuff in here....
	dwFormatSize = ICCompressGetFormatSize(myComp.hic, &bi); 
	h = GlobalAlloc(GHND, dwFormatSize); 
	biout = (LPBITMAPINFOHEADER)GlobalLock(h); 
	ICCompressGetFormat(myComp.hic, &bi, biout); 

	ret=AVIStreamSetFormat(myStream, 0, biout, sizeof(BITMAPINFOHEADER));
	if (0 != ret)
	{
		MessageBox(myWnd, "Can't set AVI Format (maybe incompatible compressor?)", "Classic99 Error", MB_OK);
		debug_write("Failed to set AVI format, code %d", ret);

		CloseAVI();
		LeaveCriticalSection(&csAVI);
		
		return 1;
	}

 
	if (!ICSeqCompressFrameStart(&myComp, (BITMAPINFO*)&bi))
	{
		MessageBox(myWnd, "Can't start AVI compression (maybe incompatible compressor?)", "Classic99 Error", MB_OK);
		debug_write("Failed to start AVI compression");

		CloseAVI();
		LeaveCriticalSection(&csAVI);

		return 1;
	}

	frame=0;
	audioframe=0;

	if (NULL == framedata)
	{
		CloseAVI();
		LeaveCriticalSection(&csAVI);

		return 1;
	}
	
	LeaveCriticalSection(&csAVI);
	return 0;
}

// TODO: will not work for 80 column mode!
void WriteFrame()
{
	BOOL key;
	void *data;
	long len;

	EnterCriticalSection(&csAVI);

	// write a frame to the AVI
	// I want only every 4th frame.
	RecordFrame++;
	if (RecordFrame < 4) {
		LeaveCriticalSection(&csAVI);
		return;
	}
	RecordFrame=0;

	if ((myStream) && (framedata))
	{
		// frame is 272*208
		len=226304;		//	272x208x4 (32-bit)
		data=ICSeqCompressFrame(&myComp, 0, framedata, &key, &len);						// compress the frame
		if (NULL != data) {
			HRESULT ret = AVIStreamWrite(myStream, frame++, 1, data, len, NULL, NULL, NULL);	// write 1 frame
			if (ret != 0) {
				debug_write("Failed to write to stream, code %d", ret);
			}
		} else {
			debug_write("Failed to compress frame.");
		}
	}
	else
	{
		debug_write("Can't write frame (myStream: 0x%08X, framedata: 0x%08X", myStream, framedata);
		CloseAVI();
	}
	
	LeaveCriticalSection(&csAVI);
}

// pointer to audio buffer, nLen is number of bytes (not samples)
void WriteAudioFrame(void *pData, int nLen)
{
	static unsigned char *pBuffer = NULL;
	static int nBufferLen = 0;
	static int nBufferPos = 0;

	if (!bUsingAudio) return;

	EnterCriticalSection(&csAVI);

	// we also use the record frame counting here, it ranges from 0-3, and we should only write when it's zero
	if (NULL == pBuffer) {
		nBufferLen = nLen*4;
		pBuffer=(unsigned char*)malloc(nBufferLen);
		nBufferPos=0;
	}
	if (nBufferPos + nLen >= nBufferLen) {
		nBufferLen = nLen*4;
		pBuffer=(unsigned char*)realloc(pBuffer, nBufferLen);
		if (nBufferPos + nLen >= nBufferLen) {
			// coding error!! don't crash
			LeaveCriticalSection(&csAVI);
			return;
		}
	}
	memcpy(pBuffer+nBufferPos, pData, nLen);
	nBufferPos+=nLen;
	if (RecordFrame != 0) {
		LeaveCriticalSection(&csAVI);
		return;
	}

	if (myAudioStream)
	{
		// 16-bit samples
		LONG wrSamp, wrByte;
		HRESULT ret = AVIStreamWrite(myAudioStream, audioframe, nBufferPos/2, pBuffer, nBufferPos, 0, &wrSamp, &wrByte);	// write 1 frame
		audioframe+=nBufferPos/2;
//		debug_write("Asked for %d samp, %d byte -- got %d samp, %d byte", nLen/2, nLen, wrSamp, wrByte);
		nBufferPos = 0;
		if (ret != 0) {
			debug_write("Failed to write to audio stream, code %d", ret);
		}
	}
	else
	{
		debug_write("Can't write audio frame.");
		CloseAVI();
	}

	LeaveCriticalSection(&csAVI);
}


void CloseAVI()
{
	// prevents the rest of the code from coming back in here
	Recording=0;

	EnterCriticalSection(&csAVI);

	ICSeqCompressFrameEnd(&myComp);
	ICCompressorFree(&myComp);

	if (NULL != myStream) {
		AVIStreamRelease(myStream);													// release the stream
		myStream = NULL;		// just to flag not active anymore
	}
	if (NULL != myAudioStream) {
		AVIStreamRelease(myAudioStream);											// release the stream
		myAudioStream = NULL;
	}

	AVIFileRelease(myAvi);														// release the file
	AVIFileExit();																// exit AVI library

	LeaveCriticalSection(&csAVI);
}

