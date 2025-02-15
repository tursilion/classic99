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
#include <vector>
#include <string>
#include "..\console\tiemul.h"
#include "screenReader.h"

#include "diff.h"
using namespace orgsyscall::yavom;  // thanks to Amos Brocco

// TODO: continuous read might start to impact emulation performance,
// as the dictionary grows sorting the screen may take longer and longer.
// Moving it to the work thread is the ideal, but we need to capture the VDP
// view of the screen on the caller, since the emulation has paused for us
// and it's supposed to be a good time to capture it.

// used for the screen reader output
static std::vector<std::string> dictionary;  // this will grow forever.. but it shouldn't get too big? (we clear it with screen clear now)
static std::vector<int> oldList;             // last screen's list
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

namespace ScreenReader {

// this function actually feeds speech.
// We break it up this way to allow the speech
// to be non-blocking, as well as to allow the backlog
// to be cancelled.
void __cdecl ReaderThreadFunc(void *) {
	CString workStr;

	// speech init: https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ms720163(v=vs.85)
	// this doesn't honor the system settings - how can we make it do so?
	if (FAILED(::CoInitialize(NULL))) {
		debug_write("** Com initialize failed - no promises.");
		pVoice = NULL;
		return;
	} else {
		// creates a default voice
		HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
		if (!SUCCEEDED(hr)) {
			debug_write("Screen reader failed to initialize COM");
			pVoice = NULL;
			return;
		}
		// set system default voice - not working?
		if (S_OK != pVoice->SetVoice(NULL)) {
			debug_write("Failed to set default system voice.");
		}
	}

	if (pVoice == NULL) {
		debug_write("Screen reader unexpectedly has NULL voice");
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
			//debug_write("%s", workStr);
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

    debug_write("Speech dictionary size at exit: %d words", dictionary.size());
}

// add a word index to newList, adding to dictionary if necessary
// hold the mutex when calling this function!
// This function may also translate certain annoying words
void addWord(std::string &word, std::vector<int> &newList, std::vector<int> &rowList, int row) {
    // end of a word, is there a word to deal with?
    if (!word.empty()) {
		// some pocket translations - these are probably Classic99 specific, except maybe IT
		
		// keeping punctuation here may be a mistake...
		if (word == "IT") word = "it";			// say "IT" as "it", not as "I.T."
		else if (word == "IT.") word = "it.";	// say "IT" as "it", not as "I.T."
		else if (word == "IT,") word = "it,";	// say "IT" as "it", not as "I.T."
		else if (word == "TI") word = "T I";	// say "TI" as "T.I., not as "tie"
		else if (word == "TI-99/4A") word = "T I-99/4A";	// say "TI" as "T.I., not as "tie"
		else if (word == "TI-99") word = "T I-99";	// say "TI" as "T.I., not as "tie"
		else if (word == "TI/001.2") word = "T I/001.2";	// say "TI" as "T.I., not as "tie" (Scott Adams built-in)
		else if (word == "TI.") word = "T I.";	// say "TI" as "T.I., not as "tie"
		else if (word == "TI,") word = "T I,";	// say "TI" as "T.I., not as "tie"

        // yes. Determine an index for it
        unsigned int index = 0;
        while (index < dictionary.size()) {
            if (dictionary[index] == word) {
                break;
            }
            ++index;
        }
        if (index >= dictionary.size()) {
            dictionary.push_back(word);
            index = dictionary.size()-1;
        }
        newList.push_back(index);
        rowList.push_back(row);

        word.clear();
    }
}

// initialize the speech system
bool initScreenReader() {
	// do this first!
	InitializeCriticalSection(&csSpeech);

	// clear the old buffer
	ClearHistory();

	lastTime = time(NULL)-1;
	readerThread = _beginthread(ReaderThreadFunc, 0, NULL);

	return true;
}

// in cont mode, we check for diffs and screen scrolls, otherwise we don't
CString fetchScreenBuffer(bool contMode) {
	char newBuf[2080];	// for 80x26

	// check if the screen is disabled - if so, all bets are off
	int charsPerLine = getCharsPerLine();
	if (charsPerLine == -1) {
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
	memset(newBuf, ' ', sizeof(newBuf));
	for (int outPos = 0; outPos < csOut.GetLength(); ++outPos) {
		if (csOut[outPos] == '\r') {
			c = 0;
			++r;
			if (r>23) break;	// no 26 line support yet
			++outPos;	// to also skip the \n
			continue;
		}
		char x = csOut[outPos];
		if ((x >= ' ') && (x < '~')) {
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

    // strip some annoying sequences
	for (int r = 0; r < 24; ++r) {	// no 26 line support
		for (int c = 0; c < charsPerLine-2; ++c) {
			int off = r*charsPerLine+c;
        	// strip anything repeated 3 or more times (most likely graphics)
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
            // strip a solo underscore (sometimes used as a cursor)
            if ((newBuf[off] == '_') && (newBuf[off+1] <= ' ')) {
                newBuf[off] = ' ';
            }
		}
	}

	bool emit = false;

	// new version that uses the myers diff process to generate csOut
    csOut.Empty();

    if (!contMode) {
        // we want the whole screen, so keep the old code for that
	    emit = false;
	    for (int r = 0; r < 24; ++r) {	// no 26 line support
		    int line = 0;
		    char lastchar = ' ';
		    CString csLine;
		    for (int c = 0; c < charsPerLine; ++c) {
			    int off = r*charsPerLine+c;
				// now work on the new character
				char newchar = newBuf[off];
				// we can't really tell if something is a letter or graphics, but, if
				// we check for solid blocks we can at least filter out the master title page
				if ((0 == memcmp(&VDP[(newchar-offset)*8+PDT], "\0\0\0\0\0\0\0\0", 8)) ||
					(0 == memcmp(&VDP[(newchar-offset)*8+PDT], "\xff\xff\xff\xff\xff\xff\xff\xff", 8))) {
					newchar = ' ';
				}
				if ((newchar >= ' ') && (newchar < '~')) {
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
		    if (line > 0) {
			    csLine += " ";
			    csOut += csLine;
			    emit = true;
		    }
	    }
    } else {
        // use the Myers diff algorithm to find additions. We don't care about
        // deletions or sames. The one gotcha: it needs to be words that we
        // diff on, not characters. Otherwise we may get some weird output.
        std::vector<int> newList;       // list for this screen
        std::vector<int> rowList;       // line mode (after enter) reads full lines - this tracks the line for each word

		// process this under the lock
		EnterCriticalSection(&csSpeech);

			// so first, we need to make a dictionary. I guess we'll associate
			// punctuation with words... it should generally work fine even if
			// they run together because mixing up a single line is quite rare
			// The dictionary vector as built will build forever, but I don't
			// think it will ever get excessively large because the vocaularly
			// used on the TI isn't all that big in general, even in text games.
			std::string word;
			for (int r = 0; r<24; ++r) {        // TOOD: no 26 line support
				for (int c = 0; c < charsPerLine; ++c) {
					// check for whitespace - keep the punctuation!
					int idx = r*charsPerLine+c;
					int newchar = newBuf[idx];
					// we can't really tell if something is a letter or graphics, but, if
					// we check for solid blocks we can at least filter out the master title page
					if ((0 == memcmp(&VDP[(newchar-offset)*8+PDT], "\0\0\0\0\0\0\0\0", 8)) ||
						(0 == memcmp(&VDP[(newchar-offset)*8+PDT], "\xff\xff\xff\xff\xff\xff\xff\xff", 8))) {
						newchar = ' ';
					}
					if ((newchar<=' ')||(newchar>='~')) {
						addWord(word, newList, rowList, r);
					} else {
						// not whitespace, so add it to the word
						word += newchar;
					}
				}
				// end of row also marks end of word
				addWord(word, newList, rowList, r);
			}

			// now we have a vector of words - diff against the old one
			auto moves = myers(oldList, newList);

			csOut = "";
			// use only additions to build a new output list
			for (const auto &m : moves) {
				if (std::get<0>(m) == OP::INSERT) {
					auto inserts = std::get<3>(m);
					for (int idx : inserts) {
						// DON'T MIX APIS. Then you don't need crap like this. Sheesh.
						csOut += dictionary[idx].c_str();
						csOut += ' ';
					}
				}
			}

		// safe to release the lock
		LeaveCriticalSection(&csSpeech);

        if (csOut.GetLength() > 0) {
            debug_write("SAY: %s", csOut.GetString());
            emit = true;
        }

        oldList = newList;
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

	// The lock release between fetch and storing in speechList is okay, csOut is
	// not dependent upon the dictionary or screen buffers
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
			debug_write("Continuous screen read enabled");
			EnterCriticalSection(&csSpeech);
				speechList.push("Continuous screen read enabled.");
			LeaveCriticalSection(&csSpeech);
			continuousRead = cont;	// turn it on last
		} else {
			continuousRead = cont;	// turn it off first
			debug_write("Continuous screen read disabled");
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
		if (cnt > 0) debug_write("Skipped %d sentences on shut up...", cnt);
	LeaveCriticalSection(&csSpeech);
}

// call this when the screen is cleared, and we will delete the diff buffer,
// so that all text displayed is considered new.
void ClearHistory() {
	EnterCriticalSection(&csSpeech);
		dictionary.clear();		// since we are starting from scratch, we can wipe the dictionary
		oldList.clear();
	LeaveCriticalSection(&csSpeech);
	debug_write("Screen reader history cleared.");
}

}	// namespace
