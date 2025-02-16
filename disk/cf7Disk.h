#pragma once

// quick emulation of the CF7 device

typedef unsigned char Byte;
typedef unsigned short Word;

// I wrote these before I created the class. Oh well ;) Fix it later. TODO.
Byte read_cf7(Word x);
void write_cf7(Word x, Byte c);

class Cf7Disk : public BaseDisk {
public:
	Cf7Disk();
	~Cf7Disk();

	// basic DSR operations
//	virtual void Startup();								// base class ok
//	virtual bool SetFiles(int n);						// base class ok

	// emulation functions
//	virtual void SetPath(const char *pszPath);			// base class ok
	virtual int  GetDiskType() { return DISK_TICC; }	// We just want to look like a TI controller, we don't handle ANYTHING
//	virtual bool CheckOpenFiles(bool ignoreInput);		// base class ok
//	virtual void CloseAllFiles();						// base class ok
//	virtual void SetOption(int nOption, int nValue);	// no options
//	virtual bool GetOption(int nOption, int &nValue);
//	virtual FileInfo *AllocateFileInfo();
//	virtual FileInfo *FindFileInfo(CString csFile);
//	virtual CString BuildFilename(FileInfo *pFile);

	// disk support
//	virtual bool Flush(FileInfo *pFile);
//	virtual bool TryOpenFile(FileInfo *pFile);
//	virtual bool CreateOutputFile(FileInfo *pFile);
//	virtual bool BufferFile(FileInfo *pFile);
//	virtual CString GetDiskName();						// not supported

	// standard PAB opcodes
//	virtual FileInfo *Open(FileInfo *pFile);
//	virtual bool Close(FileInfo *pFile);				// base class ok
//	virtual bool Read(FileInfo *pFile);					// base class ok
//	virtual bool Write(FileInfo *pFile);				// base class ok
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
//	virtual bool RenameFile(FileInfo *pFile);			// not supported
};
