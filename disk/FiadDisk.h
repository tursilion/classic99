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
#pragma once

// FIAD style disk access
class FiadDisk : public BaseDisk {
public:
	FiadDisk();
	~FiadDisk();

	// basic DSR operations
	virtual void Startup();								// powerup routine
//	virtual bool SetFiles(int n);						// base class ok

	// emulation functions
//	virtual void SetPath(const char *pszPath);			// base class ok
	virtual const char *GetPath();
	virtual int  GetDiskType() { return DISK_FIAD; }	// return the disk type
//	virtual bool CheckOpenFiles(bool ignoreInput);		// base class ok
//	virtual void CloseAllFiles();						// base class ok
	virtual void SetOption(int nOption, int nValue);
	virtual bool GetOption(int nOption, int &nValue);
//	virtual FileInfo *AllocateFileInfo();				// base class ok
//	virtual FileInfo *FindFileInfo(CString csFile);		// base class ok
	virtual CString BuildFilename(FileInfo *pFile) override;

	// disk support
	virtual bool Flush(FileInfo *pFile);
	virtual bool TryOpenFile(FileInfo *pFile);
	virtual bool CreateOutputFile(FileInfo *pFile);
	virtual bool BufferFile(FileInfo *pFile);
	virtual CString GetDiskName();

	// standard PAB opcodes
	virtual FileInfo *Open(FileInfo *pFile) override;
//	virtual bool Close(FileInfo *pFile) override;				// base class ok
//	virtual bool Read(FileInfo *pFile) override;				// base class ok
//	virtual bool Write(FileInfo *pFile) override;				// base class ok
//	virtual bool Restore(FileInfo *pFile) override;				// base class ok
	virtual bool Load(FileInfo *pFile) override;
	virtual bool Save(FileInfo *pFile) override;
	virtual bool Delete(FileInfo *pFile) override;
//	virtual bool Scratch(FileInfo *pFile) override;				// not supported
//	virtual void MapStatus(FileInfo *src, FileInfo *dest);      // base class ok
//	virtual bool GetStatus(FileInfo *pFile) override;			// base class ok

    virtual bool IsSubDirSupported() override { return bSubDirApi; }  // must override for it to work

    // SBRLNK opcodes (files is handled by shared handler)
	virtual bool ReadSector(FileInfo *pFile) override;
//	virtual bool WriteSector(FileInfo *pFile) override;			// not supported
	virtual bool ReadFileSectors(FileInfo *pFile) override;
	virtual bool WriteFileSectors(FileInfo *pFile) override;
//	virtual bool FormatDisk(FileInfo *pFile) override;			// not supported
//	virtual bool ProtectFile(FileInfo *pFile) override;			// not supported
//	virtual bool UnProtectFile(FileInfo *pFile) override;		// not supported
    virtual bool RenameFile(FileInfo *pFile, const char *csNewName, bool bIsDir) override; // 10 character limit?
    virtual bool SetSubDir(FileInfo *pFile) override;   // 39 character limit
	virtual bool CreateDirectory(FileInfo *pFile) override; // 10 character limit?
	virtual bool DeleteDirectory(FileInfo *pFile) override; // 10 character limit?

	// class-specific functions
	FILE *fopen(const char *szFile, char *szMode);
	void DetectImageType(FileInfo *pFile, CString csFileName);
	virtual bool BufferFiadFile(FileInfo *pFile);
	virtual bool BufferTextFile(FileInfo *pFile);
	virtual bool BufferImgFile(FileInfo *pFile);
	bool FlushWindowsText(FileInfo *pFile);
	bool FlushFiad(FileInfo *pFile);
	void WriteFileHeader(FileInfo *pFile, FILE *fp);
	bool ReadVIB(FileInfo *pFile);
	int  GetDirectory(FileInfo *pFile, FileInfo *&Filenames);
	bool ReadFDR(FileInfo *pFile, FileInfo *Filenames);
	const char* GetAttributes(int nType);

	// configuration data
	bool bWriteV9T9;
	bool bReadTIFiles;
	bool bReadV9T9;
	bool bWriteDV80AsText;
	bool bWriteAllDVAsText;
	bool bWriteDF80AsText;
	bool bWriteAllDFAsText;
	bool bReadTxtAsDV;
	bool bAllowNoHeaderAsDF128;
	bool bAllowTxtWithoutExtension;
    bool bAllowDelete;
	bool bReadImgAsTIAP;
	bool bEnableLongFilenames;
	bool bAllowMore127Files;
    bool bSwapPeriodAndSlash;
    bool bReturnSubdirs;
    bool bCaseSensitive;
    bool bSubDirApi;

	// directory cache
	int nCachedDrive;
	FileInfo *pCachedFiles;
	time_t tCachedTime;
    CString csCachedPath;
	int nCachedCount;
};

