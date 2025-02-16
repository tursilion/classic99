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

// max files for a driver
#define MAX_FILES 9
// max drives in the system
#define MAX_DRIVES 12
// number of drives reserved (ie: not user configurable)
#define RESERVED_DRIVES 2
// Statically mapped drives (must be less than MAX_DRIVES and counted in RESERVED_DRIVES)
#define CLIP_DRIVE_INDEX 10
#define CLOCK_DRIVE_INDEX 11

// diskname used for failure reporting
#define BAD_DISK_NAME "\x7@#NODISKNAME@#\x7"

// forward reference
typedef struct s_FILEINFO FileInfo;

// Structures
typedef struct s_FILEINFO {
	s_FILEINFO();
	// DO NOT ADD A DESTRUCTOR WITHOUT REWORKING THE FIAD DIRECTORY CACHE
    // TODO: why not? What needs reworking? Is that an old comment?

	void CopyFileInfo(FileInfo *p, bool bJustPAB);
	void SplitOptionsFromName();

	// data from the header
	int LengthSectors;
	int FileType;
	int RecordsPerSector;
	int BytesInLastSector;
	int RecordLength;
	int NumberRecords;		// note: even for variable, we translate the value on output
	
	// data from the PAB (unless it's in the header)
	int PABAddress;
	int OpCode;
	int Status;
	int DataBuffer;
	int CharCount;
	int RecordNumber;
	int ScreenOffset;
	int nDrive;
	CString csName;		// this is the important one, we match on this
    unsigned char *initData;    // used only by RAM-based input like the web system, cleared by buffer
    int initDataSize;
	
	// internal data
	int nIndex;			// never meant to change, just for debug
	int LastError;
	bool bFree;
	bool bOpen;			// indicates whether the file is open (or just transient)
	bool bDirty;		// indicates whether the file needs to be written back to disk
	int nCurrentRecord;
	int nDataSize;
	int ImageType;		// varies per driver
	int HeaderSize;		// varies per driver, only meant for FiadDisk
	int nLocalData;		// 32-bits for the driver to use as it likes
	CString csOptions;	// options string is always before the filename as "?x." - x may be longer though
	unsigned char *pData;
	// Format of pData - it is a buffer allocated dynamically
	// It contains rows of data. Each row is RecordLength bytes long 
	// (even for variable data) plus two -- these two bytes are
	// a little-endian short containing the length of the data in this
	// row (so, for fixed length data it will always be the same)
	// The buffer usually contains a little padding at the end
	// and may be extended dynamically, so should not be used
	// by multiple threads. nDataSize contains the size of the buffer.
	// Not used by LOAD and SAVE operations, only OPEN files (except in the SBR_FILEOUT opcode...)
} FileInfo;

// file types
enum {
	IMAGE_UNKNOWN,		// not yet defined
	IMAGE_TIFILES,		// TIFILES header detected
	IMAGE_V9T9,			// V9T9 header detected
	IMAGE_TEXT,			// Windows Text file
	IMAGE_IMG,			// Windows Image file (ie: any headerless file)
	IMAGE_SECTORDSK,	// File on a sector dump (V9T9) disk image
	IMAGE_TRACKDSK,		// File on a track dump (PC99) disk image (TODO: not used, used SECTORDSK)
	IMAGE_OMNIFLOP		// File on a physical floppy accessed via OmniFlop/TI99-PC (TODO: probably never)
};

// PAB error codes
#define ERR_NOERROR			0			// This also means the DSR was not found!
#define ERR_BADBAME			0			// thus the duplicate definition
#define ERR_WRITEPROTECT	1
#define ERR_BADATTRIBUTE	2
#define ERR_ILLEGALOPERATION 3
#define ERR_BUFFERFULL		4
#define ERR_READPASTEOF		5
#define ERR_DEVICEERROR		6
#define ERR_FILEERROR		7

// Status enums for PAB - these go into FileInfo::Status and come from the user
#define FLAG_TYPEMASK		(FLAG_VARIABLE|FLAG_INTERNAL)
#define FLAG_VARIABLE		0x10		// else FIXED
#define FLAG_INTERNAL		0x08		// else DISPLAY
#define	FLAG_RELATIVE		0x01		// else SEQUENTIAL

#define FLAG_MODEMASK		(FLAG_INPUT|FLAG_OUTPUT|FLAG_UPDATE|FLAG_APPEND)
#define FLAG_UPDATE			0x00
#define FLAG_OUTPUT			0x02
#define FLAG_INPUT			0x04
#define FLAG_APPEND			0x06

// Filetype enums for TIFILES (same for V9T9?) - these go into FileInfo::FileType and come from the file
#define TIFILES_VARIABLE	0x80		// else Fixed
#define TIFILES_PROTECTED	0x08		// else not protected
#define TIFILES_INTERNAL	0x02		// else Display
#define TIFILES_PROGRAM		0x01		// else Data
// others undefined - for the mask, ignore protection bit
#define TIFILES_MASK	(TIFILES_VARIABLE|TIFILES_INTERNAL|TIFILES_PROGRAM)

// return bits for the STATUS command (saved in Screen Offset byte)
#define STATUS_NOSUCHFILE	0x80
#define STATUS_PROTECTED	0x40
#define STATUS_INTERNAL		0x10
#define STATUS_PROGRAM		0x08
#define STATUS_VARIABLE		0x04
#define STATUS_DISKFULL		0x02
#define STATUS_EOF			0x01

// File operation codes
#define OP_OPEN			0
#define OP_CLOSE		1
#define OP_READ			2
#define OP_WRITE		3
#define OP_RESTORE		4
#define OP_LOAD			5
#define OP_SAVE			6
#define OP_DELETE		7
#define OP_SCRATCH		8
#define OP_STATUS		9

// SBR opcodes - note: there is some assumption these do not overlap the File operation codes
// Not that we can change them, since they are pre-defined by TI ;)
#define SBR_SECTOR		0x10
#define SBR_FORMAT		0x11
#define SBR_PROTECT		0x12
#define SBR_RENAME		0x13
#define SBR_FILEIN		0x14
#define SBR_FILEOUT		0x15
#define SBR_FILES		0x16

// file open modes (update is read+write)
#define PAB_READ		0x01
#define PAB_WRITE		0x02
#define PAB_APPEND		0x04
#define	PAB_SEQUENTIAL	0x08

// Disk types
// Can not renumber any once released, as they are saved
// in the configuration file!
enum {
	DISK_NONE,		// no disk device

	DISK_FIAD,		// files on a disk
	DISK_SECTOR,	// Sector based (V9T9 or PC99) format disk image
	DISK_TICC,		// TI Controller Card
//	DISK_CCCC,		// CorComp Controller Card (might just hack the TI controller for larger disks?)
	
	DISK_CLIPBOARD,	// Windows Clipboard access - it should be safe to let Clipboard float at the end as it's never saved
	DISK_CLOCK,		// same with the clock
    DISK_TIPIWEB,   // and TIPI
};
extern const char *szDiskTypes[];

// Option parameters (text version is pszOptionNames)
enum {
	OPT_FIAD_WRITEV9T9,		// else write TIFILES
	OPT_FIAD_READTIFILES,
	OPT_FIAD_READV9T9,
	OPT_FIAD_WRITEDV80ASTEXT,
	OPT_FIAD_WRITEALLDVASTEXT,
	OPT_FIAD_WRITEDF80ASTEXT,
	OPT_FIAD_WRITEALLDFASTEXT,
	OPT_FIAD_READTXTASDV,
	OPT_FIAD_READTXTWITHOUTEXT,
	OPT_FIAD_READIMGASTIAP,
	OPT_FIAD_ALLOWNOHEADERASDF128,
	OPT_FIAD_ENABLELONGFILENAMES,
	OPT_FIAD_ALLOWMORE127FILES,

	OPT_IMAGE_USEV9T9DSSD,	// reverse sector order for side 2 -- deprecated

	OPT_DISK_AUTOMAPDSK1,	// scan for DSK1 strings while loading, and patch them on the fly
	OPT_DISK_WRITEPROTECT,	// disallow writes to the disk

	DISK_OPT_MAXIMUM
};
extern const char *pszOptionNames[];

// One instance created per defined disk -- not necessarily the same as the real thing where
// one controller may run multiple drives, but that's okay. Eventually I intend to emulate the
// real controller, too, for those rare cases where that is needed.
class BaseDisk {
public:
	BaseDisk() {
		m_nMaxFiles=3;			// TI default
		bAutomapDSK1=false;		// off by default, can induce bugs in theory
		bWriteProtect=false;
	}
	~BaseDisk() {
		CloseAllFiles();
	}

	// Used by the disk interface only
	static void ResetPowerup() { bPowerUpRun = false; }

	// basic DSR operations
	virtual void Startup();				// powerup routine
	virtual bool SetFiles(int n);		// CALL FILES() and sbrlnk come down to here

	// emulation functions
	virtual void SetPath(const char *pszPath) { m_csPath=pszPath; }		// set the PC filesystem path for this object
	virtual const char *GetPath() { return m_csPath; }
	virtual int  GetDiskType() = 0;										// return the disk type (must override)
	virtual const char *GetDiskTypeAsString() { return szDiskTypes[GetDiskType()]; };
	virtual bool CheckOpenFiles(bool ignoreInput);
	virtual void CloseAllFiles();
	virtual void SetOption(int nOption, int nValue);
	virtual bool GetOption(int nOption, int &nValue);
	virtual FileInfo *AllocateFileInfo();
	virtual FileInfo *FindFileInfo(CString csFile);
	virtual CString BuildFilename(FileInfo *pFile);

	// disk support
    // these ones don't call unsupported because we don't need to print a message to the user
	virtual bool Flush(FileInfo *pFile) { pFile->LastError=ERR_ILLEGALOPERATION; return false; }
	virtual bool TryOpenFile(FileInfo *pFile) { pFile->LastError=ERR_ILLEGALOPERATION; return false; }
	virtual bool CreateOutputFile(FileInfo *pFile) { pFile->LastError=ERR_ILLEGALOPERATION; return false; }
	virtual bool BufferFile(FileInfo *pFile) { pFile->LastError=ERR_ILLEGALOPERATION; return false; }
	virtual CString GetDiskName() { return BAD_DISK_NAME; }		// hopefully never matches ;)

	// standard PAB opcodes - local system specific - set LastError and return false on error
	// Open must return the FileInfo object actually used (since it usually will get a new one on success)
	virtual FileInfo *Open(FileInfo *pFile) { unsupported(pFile); return NULL; }
	virtual bool Close(FileInfo *pFile);
	virtual bool Load(FileInfo *pFile) { return unsupported(pFile); }
	virtual bool Save(FileInfo *pFile) { return unsupported(pFile); }
	virtual bool Delete(FileInfo *pFile) { return unsupported(pFile); }
	// These functions work with the buffer so should not need to be overridden
	virtual bool Read(FileInfo *pFile);
	virtual bool Write(FileInfo *pFile);
	virtual bool Restore(FileInfo *pFile);
	virtual bool Scratch(FileInfo *pFile) { return unsupported(pFile); }
	virtual void MapStatus(FileInfo *src, FileInfo *dest);
	virtual bool GetStatus(FileInfo *pFile);

	// SBRLNK opcodes (files is handled by shared handler) - these all have special inputs and returns
	// All functions assume that LastError is set and false is returned if an error occurrs

	// ReadSector: nDrive=Drive#, DataBuffer=VDP address, RecordNumber=Sector to read
	// Must return the sector number in RecordNumber if no error.
	virtual bool ReadSector(FileInfo *pFile) { return unsupported(pFile); }

	// WriteSector: nDrive=Drive#, DataBuffer=VDP address, RecordNumber=Sector to read
	// Must return the sector number in RecordNumber if no error.
	virtual bool WriteSector(FileInfo *pFile) { return unsupported(pFile); }

	// These functions both take in the same parameters:
	// LengthSectors - number of sectors to read/write
	// csName - Name of the file to access
	// DataBuffer - address of data in VDP
	// RecordNumber - first sector to read/write
	// If 0 sectors spectified, the following are returned by ReadFileSectors, 
	// or used to create the file with WriteFileSectors. They are the same as in a PAB.
	// FileType, RecordsPerSector, BytesInLastSector, RecordLength, NumberRecords
	// On return, LengthSectors must contain the actual number of sectors read/written,
	// leave as 0 if 0 was originally specified.
	virtual bool ReadFileSectors(FileInfo *pFile) { return unsupported(pFile); }
	virtual bool WriteFileSectors(FileInfo *pFile) { return unsupported(pFile); }
	
	// FormatDisk:	nDrive=Drive#, NumberRecords=# tracks per side, DataBuffer=VDP Address,
	//				RecordsPerSector=density, LengthSectors=# sides
	// Must return the number of sectors on the disk in LengthSectors if no error
	virtual bool FormatDisk(FileInfo *pFile) { return unsupported(pFile); }

	// ProtectFile: nDrive=Drive#, csName=Filename
	// No return beyond error code required
	virtual bool ProtectFile(FileInfo *pFile) { return unsupported(pFile); }

	// UnProtectFile: nDrive=Drive#, csName=Filename
	// No return beyond error code required
	virtual bool UnProtectFile(FileInfo *pFile) { return unsupported(pFile); }

	// RenameFile: nDrive=Drive#, csName=old filename
	// No return beyond error code required
	virtual bool RenameFile(FileInfo *pFile, const char * /*pNewFile*/) { return unsupported(pFile); }

	// public base functions
	bool GetWriteProtect() { return bWriteProtect; }

	// functions for internal use
protected:
    // quick helper for 'not supported'
    bool unsupported(FileInfo *pFile) { 
        debug_write("Operation not supported on this disk type."); pFile->LastError=ERR_ILLEGALOPERATION; return false; 
    }

	// writes a TI floating point value
	char *WriteAsFloat(char *pData, int nVal);
	// remap a buffer from DSK1
	void AutomapDSK(unsigned char *pBuf, int nSize, int nDsk, bool bVariable);

	CString m_csPath;
	int m_nMaxFiles;					// TODO: need to make this meaningful
	FileInfo m_sFiles[MAX_FILES];		// up to 9 files allowed per drive
	static bool bPowerUpRun;			// to help ensure powerup is only run once (one controller!)

	// global disk options
	bool bAutomapDSK1;					// whether DSK1 is being mapped to another number on load
	bool bWriteProtect;					// whether the disk is write protected
};

// externs
extern BaseDisk *pDriveType[MAX_DRIVES];
extern CRITICAL_SECTION csDriveType;

