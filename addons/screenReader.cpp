//
// (C) 2022 Mike Brent aka Tursi aka HarmlessLion.com
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
///////////////////////////////////////////////////////
// Classic99 SAPI Screen Reader
// M.Brent
///////////////////////////////////////////////////////

#include <atlbase.h>
//You may derive a class from CComModule and use it if you want to override something,
//but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>
#include <sapi.h>
#include <time.h>
#include <queue>
#include "tiemul.h"
#include "screenReader.h"

// used for the screen reader output
static char oldBuf[2080];		// big enough for 80 column text mode
static ISpVoice *pVoice = NULL;
static time_t lastTime;
static uintptr_t readerThread;
static volatile bool continuousRead = false;

// VDP RAM for checking character patterns
extern Byte VDP[];
extern int PDT;

// system quit flag
extern volatile int quitflag;

// list of strings to speak
static std::queue<CString> speechList;
static CRITICAL_SECTION csSpeech;

// this function actually feeds speech.
// We break it up this way to allow the speech
// to be non-blocking, as well as to allow the backlog
// to be cancelled.
void __cdecl ReaderThreadFunc(void *) {
	CString workStr;

	// speech init: https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ms720163(v=vs.85)
	// this doesn't honor the system settings - how can we make it do so?
	if (FAILED(::CoInitialize(NULL))) {
		debug_write("** Com initialize failed - no promises.\n");
		pVoice = NULL;
		return;
	} else {
		// creates a default voice
		HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
		if (!SUCCEEDED(hr)) {
			debug_write("Screen reader failed to initialize COM\n");
			pVoice = NULL;
			return;
		}
		// set system default voice - not working?
		if (S_OK != pVoice->SetVoice(NULL)) {
			debug_write("Failed to set default system voice.\n");
		}
	}

	if (pVoice == NULL) {
		debug_write("Screen reader unexpectedly has NULL voice\n");
		return;
	}

	while (quitflag == 0) {
		workStr = "";

		EnterCriticalSection(&csSpeech);
			if (!speechList.empty()) {
				workStr = speechList.front();
				speechList.pop();
			}
		LeaveCriticalSection(&csSpeech);

		if (workStr.GetLength() > 0) {
			//debug_write("%s\n", workStr);
			pVoice->Speak(CStringW(workStr), SPF_IS_NOT_XML, NULL);
		} else {
			Sleep(200);
		}
	}

	// teardown more cleanly
	if (NULL != pVoice) {
		pVoice->Release();
		pVoice = NULL;
	}
}

// initialize the speech system
bool initScreenReader() {
	// clear the old buffer
	memset(oldBuf, 0, sizeof(oldBuf));
	lastTime = time(NULL)-1;

	InitializeCriticalSection(&csSpeech);

	readerThread = _beginthread(ReaderThreadFunc, 0, NULL);

	return true;
}

// in cont mode, we check for diffs and screen scrolls, otherwise we don't
CString fetchScreenBuffer(bool contMode) {
	char newBuf[2080];	// for 80x26

	// check if the screen is disabled - if so, all bets are off
	int charsPerLine = getCharsPerLine();
	if (charsPerLine == -1) {
		// bitmap, illegal, or disabled mode
		if (contMode) {
			memset(oldBuf, 0, sizeof(oldBuf));
		}
		return "";
	}

	// infer the screen offset - this is probably pretty hacky...
	// but either 32 or 128 should be space... anything else is
	// dunno!
	int offset = 0;
	if ((0 != memcmp(&VDP[PDT+(32*8)], "\0\0\0\0\0\0\0\0", 8)) &&
		(0 == memcmp(&VDP[PDT+(128*8)], "\0\0\0\0\0\0\0\0", 8))) {
		// 32 is NOT a space, but 128 IS
		offset = -96;
	}

    // build an output string - unknown chars will be '.', Line ending is \r\n
    CString csOut;
    csOut = captureScreen(offset, ' ');

	// convert to buffer
	int r = 0;
	int c = 0;
	memset(newBuf, 0, sizeof(newBuf));
	for (int outPos = 0; outPos < csOut.GetLength(); ++outPos) {
		if (csOut[outPos] == '\r') {
			c = 0;
			++r;
			if (r>23) break;	// no 26 line support yet
			++outPos;	// to also skip the \n
			continue;
		}
		char x = csOut[outPos];
		if ((x >= ' ') && (x <= '~')) {
			newBuf[r*charsPerLine+c] = x;
		} else {
			newBuf[r*charsPerLine+c] = ' ';
		}
		++c;
		if (c >= charsPerLine) {
			// unlikely, bug...
			c = 0;
		}
	}

	// strip anything repeated more than 3 times (most likely graphics)
	for (int r = 0; r < 24; ++r) {	// no 26 line support
		for (int c = 0; c < charsPerLine-2; ++c) {
			int off = r*charsPerLine+c;
			if ((newBuf[off]!=0)&&(newBuf[off] != ' ')&&(newBuf[off] == newBuf[off+1]) && (newBuf[off] == newBuf[off+2])) {
				// replace the whole string
				char c = newBuf[off];
				while (newBuf[off] == c) {
					newBuf[off++] = ' ';
					if (off >= 24*charsPerLine) {
						break;
					}
				}
			}
		}
	}

	// fast (hopefully) check for ANY diff
	if (contMode) {
		if (0 == memcmp(oldBuf, newBuf, charsPerLine*24)) {
			return "";
		}

		// check for scrolled screen (up scroll only)
		for (int idx = 1; idx < 23; ++idx) {
			int start = idx*charsPerLine;
			int cnt = (24-idx-1)*charsPerLine;
			if (cnt <= 0) continue;

			// check lower parts against top of oldbuf
			// check all but the bottom line, cause input prompts are sometimes cleared
			if (0 == memcmp(&oldBuf[start], &newBuf[0], cnt)) {
				// there must be SOMETHING other than spaces for it to count
				bool ok = false;
				for (int idx=0; idx<cnt; ++idx) {
					if ((newBuf[idx] > ' ')&&(newBuf[idx] <= '~')) {
						ok = true;
						break;
					}
				}
				if (ok) {
					debug_write("Screen scrolled %d lines\n", idx);
					memmove(&oldBuf[0], &oldBuf[idx*charsPerLine], cnt);
					memset(&oldBuf[cnt], 0, idx*charsPerLine);
					break;
				}
			}
		}
	}

	// now export a diff (or string)
	csOut.Empty();
	bool emit = false;
	for (int r = 0; r < 24; ++r) {	// no 26 line support
		int line = 0;
		char lastchar = ' ';
		CString csLine;
		for (int c = 0; c < charsPerLine; ++c) {
			int off = r*charsPerLine+c;
			if ((oldBuf[off] != newBuf[off]) || (!contMode)) {
				// on the first diff, we need to wipe the rest of the line
				if (contMode) {
					memset(&oldBuf[off], 0, charsPerLine-c);
				}
				// now work on the new character
				char newchar = newBuf[off];
				// we can't really tell if something is a letter or graphics, but, if
				// we check for solid blocks we can at least filter out the master title page
				if ((0 == memcmp(&VDP[(newchar-offset)*8+PDT], "\0\0\0\0\0\0\0\0", 8)) ||
					(0 == memcmp(&VDP[(newchar-offset)*8+PDT], "\xff\xff\xff\xff\xff\xff\xff\xff", 8))) {
					newchar = ' ';
				}
				if ((newchar >= ' ') && (newchar <= '~')) {
					if ((newchar != ' ') || (lastchar !=  ' ')) {
						csLine += newchar;
						if (newchar > ' ') {
							++line;
						}
						lastchar = newchar;
					}
				} else {
					if (lastchar != ' ') {
						csLine += ' ';
						lastchar = ' ';
					}
				}
			}
		}
		if (line > 0) {
			csLine += " ";
			csOut += csLine;
			emit = true;
		}
	}

	if (contMode) {
		memcpy(oldBuf, newBuf, sizeof(oldBuf));
	}

	if (emit) {
		return csOut;
	} else {
		return "";
	}
}

// Screen Reader feed function
// it looks for diffs to the screen and generates hopefully text
// that we can eventually feed to the microsoft speech synth
// Note this could be really annoying when scrolling or when
// it's wrong, so let's make sure there is a hotkey to disable
// also a hotkey to read the whole screen would be ideal
void CheckUpdateSpeechOutput() {
	if (NULL == pVoice) {
		return;
	}
	if (!continuousRead) {
		return;
	}

	// no more than once per second
	time_t now = time(NULL);
	if (now > lastTime) {
		lastTime = now;
	} else {
		return;
	}

	CString csOut = fetchScreenBuffer(true);

	if (csOut.GetLength() > 0) {
		// store the string in the list
		EnterCriticalSection(&csSpeech);
			speechList.push(csOut);
		LeaveCriticalSection(&csSpeech);
	}
}

// a simpler mode that reads on demand
void ReadScreenOnce() {
	CString csOut = fetchScreenBuffer(false);
	if (csOut.GetLength() > 0) {
		// store the string in the list
		EnterCriticalSection(&csSpeech);
			speechList.push(csOut);
		LeaveCriticalSection(&csSpeech);
	}
}

// set the continuous on/off flag
void SetContinuousRead(bool cont) {
	if (continuousRead != cont) {
		if (cont) {
			debug_write("Continuous screen read enabled\n");
			EnterCriticalSection(&csSpeech);
				speechList.push("Continuous screen read enabled.");
			LeaveCriticalSection(&csSpeech);
			continuousRead = cont;	// turn it on last
		} else {
			continuousRead = cont;	// turn if off first
			debug_write("Continuous screen read disabled\n");
			ShutUp();
			EnterCriticalSection(&csSpeech);
				speechList.push("Continuous screen read disabled.");
			LeaveCriticalSection(&csSpeech);
		}
	}
}
bool GetContinuousRead() {
	return continuousRead;
}

// clear out the buffer and stop the current read out
void ShutUp() {
	EnterCriticalSection(&csSpeech);
		while (!speechList.empty()) {
			speechList.pop();
		}
		// try to skip off the end of the current sentence
		ULONG cnt = 0;
		pVoice->Skip(L"Sentence", 9999, &cnt);
		debug_write("Skipped %d sentences on shut up...\n", cnt);
	LeaveCriticalSection(&csSpeech);
}
