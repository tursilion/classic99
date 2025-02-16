//
// (C) 2013 Mike Brent aka Tursi aka HarmlessLion.com
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

// helper functions
int ReadTICCCRU(int adr);
void WriteTICCCRU(int adr, int bit);
int ReadTICCRegister(int address);
void WriteTICCRegister(int address, int val);
void HandleTICCSector();

// Image style disk access (redirected to using TI DSR)
// TODO: Today limited to reading sector-based disks only
// TODO: also hidden from the end users until I'm ready for it, it's pretty hacky.
class TICCDisk : public ImageDisk {
public:
	TICCDisk();
	~TICCDisk();

	// Most of the functions in here won't be used anymore - we derive from imagedisk
	// just so we can make use of the sector access functions.

	// basic DSR operations
	virtual void Startup();								// powerup routine
	//virtual bool SetFiles(int n);						// CALL FILES equivalent

	// emulation functions
	virtual int  GetDiskType() { return DISK_TICC; }	// return the disk type

	// lower level than usual!
	void dsrlnk(int nDrive);
	void sbrlnk(int nOpCode);
	void readsectorwrap();
	void writesectorwrap();

// Seems to me I should find a way to hook these in...?
//	virtual bool CheckOpenFiles(bool ignoreInput);		// base class ok
//	virtual void CloseAllFiles();						// base class ok
};

