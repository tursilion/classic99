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

// Corcomp-compatible clock device access
class ClockDisk : public BaseDisk {
public:
	ClockDisk();
	~ClockDisk();

	// basic DSR operations
	virtual void Startup();								// powerup routine
//	virtual bool SetFiles(int n);						// base class ok

	// emulation functions
//	virtual void SetPath(const char *pszPath);			// base class ok
	virtual int  GetDiskType() { return DISK_CLOCK; }	// return the disk type
//	virtual bool CheckOpenFiles(bool ignoreInput);		// base class ok
//	virtual void CloseAllFiles();						// base class ok
//	virtual void SetOption(int nOption, int nValue);	// no options
//	virtual bool GetOption(int nOption, int &nValue);
	virtual FileInfo *AllocateFileInfo();
	virtual FileInfo *FindFileInfo(CString csFile);
	virtual CString BuildFilename(FileInfo *pFile);

	// disk support
//	virtual bool Flush(FileInfo *pFile);				// not supported
	virtual bool TryOpenFile(FileInfo *pFile);
//	virtual bool CreateOutputFile(FileInfo *pFile);		// not supported
	virtual bool BufferFile(FileInfo *pFile);
//	virtual CString GetDiskName();						// not supported

	// standard PAB opcodes
	virtual FileInfo *Open(FileInfo *pFile);
//	virtual bool Close(FileInfo *pFile);				// base class ok
	virtual bool Read(FileInfo *pFile);
	virtual bool Write(FileInfo *pFile);
//	virtual bool Restore(FileInfo *pFile);				// base class ok

//	virtual bool Load(FileInfo *pFile);					// not supported
//	virtual bool Save(FileInfo *pFile);					// not supported
//	virtual bool Delete(FileInfo *pFile);				// not supported
//	virtual bool Scratch(FileInfo *pFile);				// not supported
//	virtual void MapStatus(FileInfo *src, FileInfo *dest);// base class ok
//	virtual bool GetStatus(FileInfo *pFile);			// base class ok

	// SBRLNK opcodes (files is handled by shared handler)
//	virtual bool ReadSector(FileInfo *pFile);			// not supported
//	virtual bool WriteSector(FileInfo *pFile);			// not supported
//	virtual bool ReadFileSectors(FileInfo *pFile);		// not supported
//	virtual bool WriteFileSectors(FileInfo *pFile);		// not supported
//	virtual bool FormatDisk(FileInfo *pFile);			// not supported
//	virtual bool ProtectFile(FileInfo *pFile);			// not supported
//	virtual bool UnProtectFile(FileInfo *pFile);		// not supported
//  virtual bool RenameFile(FileInfo *pFile, const char *csNewName, bool isDir) override;       // not supported
//  virtual bool SetSubDir(FileInfo *pFile) override;   // not supported
//	virtual bool CreateDirectory(FileInfo *pFile) override;                         // not supported
//	virtual bool DeleteDirectory(FileInfo *pFile) override;                         // not supported

	// class specific data
};

