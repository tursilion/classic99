#pragma once

#include "FiadDisk.h"

// I think this will mostly sit on top of the FiadDisk handlers...
bool HandleTIPI();

// Web file FIAD access - this is a subclass of FIAD
// since MOST of the functionality will be the same,
// excluding only buffering and writes.
class TipiWebDisk : public FiadDisk {
public:
	TipiWebDisk();
	~TipiWebDisk();

	// basic DSR operations
//	virtual void Startup();								// base class ok
//	virtual bool SetFiles(int n);						// base class ok

	// emulation functions
//	virtual void SetPath(const char *pszPath);			// base class ok
//	virtual const char *GetPath();                      // base class ok (unused)
	virtual int  GetDiskType() { return DISK_TIPIWEB; }	// return the disk type
//	virtual bool CheckOpenFiles(bool ignoreInput);		// base class ok
//	virtual void CloseAllFiles();						// base class ok
//	virtual void SetOption(int nOption, int nValue);    // no options
//	virtual bool GetOption(int nOption, int &nValue);
//	virtual FileInfo *AllocateFileInfo();				// base class ok
//	virtual FileInfo *FindFileInfo(CString csFile);		// base class ok
//	virtual CString BuildFilename(FileInfo *pFile);		// base class ok (unused)

	// disk support
	virtual bool Flush(FileInfo *pFile);
	virtual bool TryOpenFile(FileInfo *pFile);
	virtual bool CreateOutputFile(FileInfo *pFile);
	virtual bool BufferFile(FileInfo *pFile);
//	virtual CString GetDiskName();                      // base class ok (unused)

	// standard PAB opcodes
	virtual FileInfo *Open(FileInfo *pFile);
//	virtual bool Close(FileInfo *pFile);				// base class ok
//	virtual bool Read(FileInfo *pFile);					// base class ok
//	virtual bool Write(FileInfo *pFile);				// base class ok
//	virtual bool Restore(FileInfo *pFile);				// base class ok
	virtual bool Load(FileInfo *pFile);
	virtual bool Save(FileInfo *pFile);
//	virtual bool Delete(FileInfo *pFile);				// not supported
//	virtual bool Scratch(FileInfo *pFile);				// not supported
//	virtual void MapStatus(FileInfo *src, FileInfo *dest);// base class ok
//	virtual bool GetStatus(FileInfo *pFile);			// base class ok

	// SBRLNK opcodes (files is handled by shared handler)
//	virtual bool ReadSector(FileInfo *pFile);           // not supported
//	virtual bool WriteSector(FileInfo *pFile);			// not supported
//	virtual bool ReadFileSectors(FileInfo *pFile);      // not supported
//	virtual bool WriteFileSectors(FileInfo *pFile);     // not supported
//	virtual bool FormatDisk(FileInfo *pFile);			// not supported
//	virtual bool ProtectFile(FileInfo *pFile);			// not supported
//	virtual bool UnProtectFile(FileInfo *pFile);		// not supported
//	virtual bool RenameFile(FileInfo *pFile);			// not supported

    // Class specific functions (as needed)
    virtual void DetectImageType(FileInfo *pFile);
    virtual bool BufferFiadFile(FileInfo *pFile);
};

