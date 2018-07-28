//
// (C) 2011 Mike Brent aka Tursi aka HarmlessLion.com
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

// for the save cart functions
struct options {
	options() {
		StartHigh = EndHigh = 0;
		StartLow = EndLow = 0;
		StartVDP = EndVDP = 0;
		Boot = 0;
		Name[0] = '\0';
		FileName[0]='\0';
		bEA = bCharSet = bCharA1 = bKeyboard = bGROM8K = bDisableF4 = bVDPRegs = bInvert = false;
		FirstGROM = 0;
	};

	int StartHigh, EndHigh;
	int StartLow, EndLow;
	int StartVDP, EndVDP;
	int Boot;
	char Name[256];
	char FileName[256];
	bool bEA, bCharSet, bCharA1, bKeyboard, bGROM8K;
	bool bDisableF4, bVDPRegs, bInvert;
	int FirstGROM;
};

BOOL CALLBACK CartDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void DoMakeDlg(HWND hwnd);
void DoMakeEA5(HWND hwnd, struct options opt);
void DoMakeCopyCart(HWND hwnd, struct options opt);
void DoMakeGromCart(HWND hwnd, struct options opt);
void DoMakeBasicCart(HWND hwnd, struct options opt);
