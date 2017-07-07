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

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <atlstr.h>
#include <time.h>
#include "tiemul.h"
#include "diskclass.h"
#include "fiaddisk.h"

// on the day this changes (for instance, we support
// direct access to RAW files, etc), add a headersize
// variable to FileInfo and make sure the detection code
// updates it, then use that instead.
#define HEADERSIZE 128

//********************************************************
// FiadDisk
//********************************************************

// constructor
FiadDisk::FiadDisk() {
	bWriteV9T9 = false;
	bReadTIFiles = true;
	bReadV9T9 = true;
	bWriteDV80AsText = false;
	bWriteAllDVAsText = false;
	bWriteDF80AsText = false;
	bWriteAllDFAsText = false;
	bReadTxtAsDV = true;
	bAllowTxtWithoutExtension = false;
	bAllowNoHeaderAsDF128 = true;
	bReadImgAsTIAP = false;
	bEnableLongFilenames = false;
	bAllowMore127Files = false;

	nCachedDrive=-1;
	pCachedFiles=NULL;
	tCachedTime=(time_t)0;
	nCachedCount=0;
}

FiadDisk::~FiadDisk() {
	if (NULL != pCachedFiles) {
		// this should placement delete through the array...
		// really we should array new the whole thing.
		free(pCachedFiles);
		pCachedFiles=NULL;
	}
}

// powerup routine
void FiadDisk::Startup() {
	BaseDisk::Startup();
}

// path - make sure we end with a backslash!
const char *FiadDisk::GetPath() { 
	if (m_csPath.Right(1) != '\\') {
		m_csPath+='\\';
	}
	return m_csPath; 
}

// handle options
void FiadDisk::SetOption(int nOption, int nValue) {
	switch (nOption) {
		case OPT_FIAD_WRITEV9T9:	
			bWriteV9T9 = nValue?true:false;
			break;

		case OPT_FIAD_READTIFILES:
			bReadTIFiles = nValue?true:false;
			break;

		case OPT_FIAD_READV9T9:
			bReadV9T9 = nValue?true:false;
			break;

		case OPT_FIAD_WRITEDV80ASTEXT:
			bWriteDV80AsText = nValue?true:false;
			break;

		case OPT_FIAD_WRITEALLDVASTEXT:
			bWriteAllDVAsText = nValue?true:false;
			break;

		case OPT_FIAD_WRITEDF80ASTEXT:
			bWriteDF80AsText = nValue?true:false;
			break;

		case OPT_FIAD_WRITEALLDFASTEXT:
			bWriteAllDFAsText = nValue?true:false;
			break;

		case OPT_FIAD_READTXTASDV:
			bReadTxtAsDV = nValue?true:false;
			break;

		case OPT_FIAD_ALLOWNOHEADERASDF128:
			bAllowNoHeaderAsDF128 = nValue?true:false;
			break;

		case OPT_FIAD_READTXTWITHOUTEXT:
			bAllowTxtWithoutExtension = nValue?true:false;
			break;

		case OPT_FIAD_READIMGASTIAP:
			bReadImgAsTIAP = nValue?true:false;
			break;

		case OPT_FIAD_ENABLELONGFILENAMES:
			bEnableLongFilenames = nValue?true:false;
			break;

		case OPT_FIAD_ALLOWMORE127FILES:
			bAllowMore127Files = nValue?true:false;
			break;

		default:
			BaseDisk::SetOption(nOption, nValue);
			break;
	}
}

bool FiadDisk::GetOption(int nOption, int &nValue) {
	switch (nOption) {
		case OPT_FIAD_WRITEV9T9:	
			nValue = bWriteV9T9;
			break;

		case OPT_FIAD_READTIFILES:
			nValue = bReadTIFiles;
			break;

		case OPT_FIAD_READV9T9:
			nValue = bReadV9T9;
			break;

		case OPT_FIAD_WRITEDV80ASTEXT:
			nValue = bWriteDV80AsText;
			break;

		case OPT_FIAD_WRITEALLDVASTEXT:
			nValue = bWriteAllDVAsText;
			break;

		case OPT_FIAD_WRITEDF80ASTEXT:
			nValue = bWriteDF80AsText;
			break;

		case OPT_FIAD_WRITEALLDFASTEXT:
			nValue = bWriteAllDFAsText;
			break;

		case OPT_FIAD_READTXTASDV:
			nValue = bReadTxtAsDV;
			break;

		case OPT_FIAD_ALLOWNOHEADERASDF128:
			nValue = bAllowNoHeaderAsDF128;
			break;

		case OPT_FIAD_READTXTWITHOUTEXT:
			nValue = bAllowTxtWithoutExtension;
			break;

		case OPT_FIAD_READIMGASTIAP:
			nValue = bReadImgAsTIAP;
			break;

		case OPT_FIAD_ENABLELONGFILENAMES:
			nValue = bEnableLongFilenames;
			break;

		case OPT_FIAD_ALLOWMORE127FILES:
			nValue = bAllowMore127Files;
			break;

		default:
			return BaseDisk::GetOption(nOption, nValue);
	}

	return true;
}

// Wrapper function to more easily handle filename munging for FIAD files
FILE *FiadDisk::fopen(const char *szFile, char *szMode) {
	FILE *pTmp; 
	char szLocalName[MAX_PATH];
	char *p;

	strncpy_s(szLocalName, MAX_PATH, szFile, _TRUNCATE);
	p=szLocalName;
	while (*p) {
		// replace some characters okay on the TI with ~ here
		switch (*p) {
			case '?':
			case '/':
//			case '\\':		// we allow backslash as a path limiter, so we can't support it
			case '>':
			case '<':
//			case ':':		// need to allow colon so we can have drive specifications
			case '\"':
			case '*':
			case '|':
				debug_write("Replacing PC illegal character '%c' in filename with '~'", *p);
				*p='~';
				break;
		}
		p++;
	}
	errno_t err=fopen_s(&pTmp, szLocalName, szMode); 
	if (err != 0) pTmp=NULL; 

	// trick part B - if the user asked for _C or _P, try .TIAC or .TIAP
	if (NULL == pTmp) {
		int x=strlen(szLocalName)-2;
		if ((x<MAX_PATH-4)&&(szLocalName[x]=='_')) {
			bool bTry=false;

			if (toupper(szLocalName[x+1])=='P') {
				strcpy(&szLocalName[x], ".TIAP");
				bTry=true;
			} else if (toupper(szLocalName[x+1])=='C') {
				strcpy(&szLocalName[x], ".TIAC");
				bTry=true;
			} else if (toupper(szLocalName[x+1])=='M') {
				strcpy(&szLocalName[x], ".TIAM");
				bTry=true;
			}
			if (bTry) {
				errno_t err=fopen_s(&pTmp, szLocalName, szMode); 
				if (err != 0) pTmp=NULL; 
			}
		}
	}

	// if the above doesn't work, V9T9 used a really hacky method of
	// making some characters high ASCII. Try it, but for READ only.
	// Note that some of these chars are now legal under Windows,
	// but they were not under DOS. We don't need to repeat the TI Artist
	// name trick cause those files can't have DOS extensions.
	if ((NULL == pTmp) && (szMode[0]=='r')) {
		strncpy_s(szLocalName, MAX_PATH, szFile, _TRUNCATE);
		p=szLocalName;
		while (*p) {
			switch (*p) {
				case '\\':
				case '/':
				case ':':
				case '*':
				case '?':
				case '\"':
				case '<':
				case '>':
				case '|':
					*p|=0x80;
					break;
			}
			p++;
		}
		errno_t err=fopen_s(&pTmp, szLocalName, szMode); 
		if (err != 0) pTmp=NULL; 
		
		if (NULL != pTmp) {
			debug_write("Opening file with old V9T9 modifiers.. suggest resaving.");
		}
	}

	if ((NULL != pTmp) && strcmp(szLocalName, szFile)) {
		static CString csLast="";
		// We still get called a few times per open request,
		// so to reduce spam a little, don't report if it's the
		// same name as last time.
		if (csLast != szLocalName) {
			csLast=szLocalName;
			debug_write("Open with translated name '%s'", szLocalName);
		}
	}

	return pTmp; 
}

// create an output file
bool FiadDisk::CreateOutputFile(FileInfo *pFile) {
	FILE *fp;
	CString csFileName = BuildFilename(pFile);

	// first a little sanity checking -- we never overwrite an existing file
	// unless the mode is 'output', so check existance against the mode
	fp=fopen(csFileName, "rb");
	if (NULL != fp) {
		fclose(fp);
		// file exists, are we allowed to overwrite?
		if ((pFile->Status & FLAG_MODEMASK) != FLAG_OUTPUT) {
			// no, we are not
			debug_write("Can't overwrite existing file with open mode 0x%02X", (pFile->Status & FLAG_MODEMASK));
			return false;
		}
	}

	// see if we can open the file at all
	fp=fopen(csFileName, "wb");
	if (NULL == fp) {
		// No, we failed
		debug_write("Failed to open output file %s, err %d", (LPCSTR)csFileName, errno);
		switch (errno) {
			case EACCES:
				pFile->LastError = ERR_WRITEPROTECT;
				break;
			case ENOSPC:
				pFile->LastError = ERR_BUFFERFULL;
				break;
			default:
				pFile->LastError = ERR_FILEERROR;
				break;
		}
		return false;
	}
	// we don't need the file open here any more
	fclose(fp);
	
	// check if the user requested a default record length, and fill it in if so
	if (pFile->RecordLength == 0) {
		pFile->RecordLength = 80;
	}

	// calculate necessary data and fill in header fields
	pFile->RecordsPerSector = 256  / (pFile->RecordLength + ((pFile->Status & FLAG_VARIABLE) ? 1 : 0) );
	pFile->LengthSectors = 1;
	pFile->FileType = 0;
	if (pFile->Status & FLAG_VARIABLE) pFile->FileType|=TIFILES_VARIABLE;
	if (pFile->Status & FLAG_INTERNAL) pFile->FileType|=TIFILES_INTERNAL;
	pFile->BytesInLastSector = 0;
	pFile->NumberRecords = 0;

	// Allocate a buffer for it (size of 10 records by default)
	if (NULL != pFile->pData) {
		free(pFile->pData);
	}
	pFile->pData = (unsigned char*)malloc(10 * (pFile->RecordLength+2));
	pFile->bDirty = true;	// never written out!

	// Since we opened it on the disk, we should flush it just to ensure there is valid data in it
	if (!Flush(pFile)) {
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	return true;
}

// Open an existing file, check the header against the parameters
bool FiadDisk::TryOpenFile(FileInfo *pFile) {
	char *pMode;
	CString csFileName = BuildFilename(pFile);
	int nMode = pFile->Status & FLAG_MODEMASK;	// should be UPDATE, APPEND or INPUT
	FILE *fp=NULL;
	FileInfo lclInfo;

	switch (nMode) {
		case FLAG_UPDATE:
			pMode="update";
			fp=fopen(csFileName, "r+b");
			break;

		case FLAG_APPEND:
			pMode="append";
			fp=fopen(csFileName, "ab");
			break;

		case FLAG_INPUT:
			pMode="input";
			fp=fopen(csFileName, "r");
			break;

		default:
			debug_write("Unknown mode - can't open.");
			pMode="unknown";
	}

	if (NULL == fp) {
		debug_write("Can't open %s for %s, errno %d.", (LPCSTR)csFileName, pMode, errno);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// we got the file, but we need to read the header and determine the
	// filetype. For that we close and let the universal function handle it.
	fclose(fp);
	
	// there should be no data to copy as we are just opening the file
	lclInfo.CopyFileInfo(pFile, false);
	DetectImageType(&lclInfo, csFileName);

	if (lclInfo.ImageType == IMAGE_UNKNOWN) {
		debug_write("%s is an unknown file type - can not open.", (LPCSTR)lclInfo.csName);
		pFile->LastError = ERR_BADATTRIBUTE;
		return false;
	}

	// Verify the parameters as a last step before we OK it all :)
	if ((pFile->FileType&TIFILES_MASK) != (lclInfo.FileType&TIFILES_MASK)) {
		// note: don't put function calls into varargs!
		const char *str1,*str2;
		str1=GetAttributes(lclInfo.FileType);
		str2=GetAttributes(pFile->FileType);
		debug_write("Incorrect file type: %d/%s%d (real) vs %d/%s%d (requested)", lclInfo.FileType&TIFILES_MASK, str1, lclInfo.RecordLength, pFile->FileType&TIFILES_MASK, str2, pFile->RecordLength);
		pFile->LastError = ERR_BADATTRIBUTE;
		return false;
	}

	if (0 == (lclInfo.FileType & TIFILES_PROGRAM)) {
		// check record length (we already verified if PROGRAM was wanted above)

		if (pFile->RecordLength == 0) {
			pFile->RecordLength = lclInfo.RecordLength;
		}

		if (pFile->RecordLength != lclInfo.RecordLength) {
			debug_write("Record Length mismatch: %d (real) vs %d (requested)", lclInfo.RecordLength, pFile->RecordLength);
			pFile->LastError = ERR_BADATTRIBUTE;
			return false;
		}
	}

	// seems okay? Copy the data over from the PAB
	pFile->CopyFileInfo(&lclInfo, false);
	return true;
}

// Determine the type of image (must exist) into ImageType
// Full pathname may be optionally passed in to save having to build it again
// but pFile->csName must be the ondisk name
// This also translates the TIFILES FileType back to a PAB status type
void FiadDisk::DetectImageType(FileInfo *pFile, CString csFileName) {
	unsigned char buf[512];		// buffer to read first block into
	int nReadLen;

	// note that if the detection is overridden using a filename
	// parameter, you get what you deserve. ;)

	if (csFileName.IsEmpty()) {
		CString csFileName = BuildFilename(pFile);
	} else {
		// or the alternate, possible from SBR calls
		if (pFile->csName.IsEmpty()) {
			int idx=csFileName.ReverseFind('\\');
			if (-1 == idx) idx=0;
			pFile->csName=csFileName.Mid(idx+1);
		}
	}

	// there are four options currently supported here:
	// TIFILES, V9T9, Windows Text, and Windows image
	// Configuration switches may disable any of these!
	pFile->ImageType = IMAGE_UNKNOWN;

	// file must exist and be readable! (and if it needs a header, has to have it)
	FILE *fp=fopen((LPCSTR)csFileName, "rb");
	if (NULL == fp) {
		debug_write("Can't read file %s, errno %d", (LPCSTR)csFileName, errno);
		return;
	}

	// we have it open - read the first block then close it
	memset(buf, 0, sizeof(buf));
	nReadLen=fread(buf, 1, 512, fp);			// it's legal to read less!
	fclose(fp);
	
	// check for TIFILES, if supported
	// the first characters would be a length and then "TIFILES" if it's a TIFILES file
	if ((0==_strnicmp((char*)buf, "\x07TIFILES", 8)) || (pFile->csOptions.Find('T')!=-1)) {
		if (bReadTIFiles) {
			debug_write("Detected %s as a TIFILES file", (LPCSTR)csFileName);
			pFile->ImageType = IMAGE_TIFILES;
			// fill in the information 
			pFile->LengthSectors=(buf[8]<<8)|buf[9];
			pFile->FileType=buf[10];
			pFile->RecordsPerSector=buf[11];
			pFile->BytesInLastSector=buf[12];
			pFile->RecordLength=buf[13];
			pFile->NumberRecords=(buf[15]<<8)|buf[14];		// NOTE: swapped on disk!
			// translate FileType to Status
			pFile->Status = 0;
			if (pFile->FileType & TIFILES_VARIABLE) pFile->Status|=FLAG_VARIABLE;
			if (pFile->FileType & TIFILES_INTERNAL) pFile->Status|=FLAG_INTERNAL;
			// do some fixup of the NumberRecords field
			// If it's variable or fixed, we should easily determine if the value is byteswapped
			if (pFile->FileType & TIFILES_VARIABLE) {
				// must match the number of sectors
				if (pFile->NumberRecords != pFile->LengthSectors) {
					// check for byte swap (better debugging)
					if (((buf[14]<<8)|(buf[15])) == pFile->LengthSectors) {
						debug_write("Var File had Number Records byte-swapped - will fix - recommend re-saving file");
						pFile->NumberRecords = pFile->LengthSectors;
					} else {
						debug_write("Warning: Number Records doesn't match sector length on variable file.");
					}
				}
			} else {
				// is an actual record count, but we can use LengthSectors and RecordsPerSector to
				// see if it was byte swapped. We don't bother with using BytesInLastSector to get the actual count
				if (pFile->NumberRecords > (pFile->LengthSectors+1)*pFile->RecordsPerSector) {
					if (((buf[14]<<8)|(buf[15])) <= (pFile->LengthSectors+1)*pFile->RecordsPerSector) {
						debug_write("Fix File had Number Records byte-swapped - will fix - recommend re-saving file");
						pFile->NumberRecords = ((buf[14]<<8)|(buf[15]));
					}
				}
			}
			return;
		} else {
			debug_write("%s looks like TIFILES, but TIFILES reading is disabled.", (LPCSTR)csFileName);
		}
	}

	// Check for V9T9 format, if supported
	// the first characters should match the filename if this
	// is really a v9t9 file. To improve handling of filename munging,
	// we'll only check alphabetic characters.
	int idx=0;
	// a little parsing helps with stripping subdirectories
	CString csTmpName = pFile->csName;
	idx = csTmpName.ReverseFind('\\');
	if (idx != -1) {
		csTmpName = csTmpName.Mid(idx+1);
	}
	idx=0;
	if (csTmpName.GetLength() <= 10) {
		// maximum length of a V9T9 filename is 10 chars
		while ((idx<10) && (*(buf+idx)!=' ')) {
			if (idx >= csTmpName.GetLength()) {
				// disk is longer than CString
				idx=99;
				break;
			}
			if ((isalnum(*(buf+idx))) || (isalnum(csTmpName[idx]))) {
				if (toupper(*(buf+idx))!=toupper(csTmpName[idx])) {
					idx=99;
					break;
				}
			}
			idx++;
		}
		// verify length on exit
		if (csTmpName.GetLength() > idx) {
			// CString is longer than disk
			idx=99;
		}
	} else {
		idx=99;		// failure flag
	}
	if ((idx != 99) || (pFile->csOptions.Find('V')!=-1)) {
		if (bReadV9T9) {
			debug_write("Detected %s as a V9T9 file", (LPCSTR)csFileName);
			pFile->ImageType = IMAGE_V9T9;
			// fill in the information 
			pFile->LengthSectors=(buf[14]<<8)|buf[15];
			pFile->FileType=buf[12];
			pFile->RecordsPerSector=buf[13];
			pFile->BytesInLastSector=buf[16];
			pFile->RecordLength=buf[17];
			pFile->NumberRecords=(buf[19]<<8)|buf[18];		// Note: byte-swapped on disk!
			// translate FileType to Status
			pFile->Status = 0;
			if (pFile->FileType & TIFILES_VARIABLE) pFile->Status|=FLAG_VARIABLE;
			if (pFile->FileType & TIFILES_INTERNAL) pFile->Status|=FLAG_INTERNAL;
			// do some fixup of the NumberRecords field (less likely here)
			// If it's variable or fixed, we should easily determine if the value is byteswapped
			if (pFile->FileType & TIFILES_VARIABLE) {
				// must match the number of sectors
				if (pFile->NumberRecords != pFile->LengthSectors) {
					// check for byte swap (better debugging)
					if (((buf[18]<<8)|(buf[19])) == pFile->LengthSectors) {
						debug_write("Var File had Number Records byte-swapped - will fix - recommend re-saving file");
						pFile->NumberRecords = pFile->LengthSectors;
					} else {
						debug_write("Warning: Number Records doesn't match sector length on variable file.");
					}
				}
			} else {
				// is an actual record count, but we can use LengthSectors and RecordsPerSector to
				// see if it was byte swapped. We don't bother with using BytesInLastSector to get the actual count
				if (pFile->NumberRecords > (pFile->LengthSectors+1)*pFile->RecordsPerSector) {
					if (((buf[18]<<8)|(buf[19])) <= (pFile->LengthSectors+1)*pFile->RecordsPerSector) {
						debug_write("Fix File had Number Records byte-swapped - will fix - recommend re-saving file");
						pFile->NumberRecords = ((buf[18]<<8)|(buf[19]));
					}
				}
			}
			return;
		} else {
			debug_write("%s looks like V9T9, but V9T9 reading is disabled.", (LPCSTR)csFileName);
		}
	}

	// check for Windows text - unlike the old Classic99, we'll just work with
	// a list of extensions, anything in that list is assumed to be valid. We
	// only analyze the file to determine record length now
	// TODO: For now, the extensions are hard-coded, since they don't work
	// well with the numeric configuration system:
	// TXT - text (duh)
	// OBJ - Object file (assumed from Win994a Asm)
	// COB - Compressed object file (assumed from Win994a Asm)
	// One goal is that in the future Classic99 will be able to extract parts of files and thus do more
	// Perhaps for that we can implement file system filters - they take in the
	// filename and the FileInfo object for the open, and extract appropriate data
	if ((csFileName.Right(4).CompareNoCase(".TXT") == 0) || 
	(csFileName.Right(4).CompareNoCase(".OBJ") == 0) || 
	(csFileName.Right(4).CompareNoCase(".COB") == 0) ||
	((bAllowTxtWithoutExtension)&&(-1 == csFileName.Find('.'))) ||
	(pFile->csOptions.Find('W')!=-1)  ) {
		if (bReadTxtAsDV) {
			debug_write("Detected %s as a Host TEXT file", (LPCSTR)csFileName);
			pFile->ImageType = IMAGE_TEXT;
			// in an open call, STATUS will contain what is wanted,
			// but in others it may not. In that case it may often appear as fixed
			// We can check record length - if it's 0, return DV80 (normal TI text),
			// assuming DV is allowed, else DF80. Otherwise, just allow whatever
			// the user was requesting
			if (pFile->RecordLength == 0) {
				// assume we should fill it in
				pFile->RecordLength = 80;		// even if it's not really, this is ok
			}

			// allow most user fields, but make sure it's at least display
			pFile->FileType &= ~TIFILES_INTERNAL;
			pFile->Status &= ~FLAG_INTERNAL;

			if (!(pFile->Status & FLAG_VARIABLE)) {
				// add variable flag if variable is allowed at all (default)
				// exception: OBJ and COB will be fixed.
				// need a way to configure this
				if ((csFileName.Right(4).CompareNoCase(".OBJ") == 0) || 
					(csFileName.Right(4).CompareNoCase(".COB") == 0)) {
						// do no such thing
				} else {
					pFile->FileType |= TIFILES_VARIABLE;
					pFile->Status |= FLAG_VARIABLE;
				}
			}

			// calculate some of the other fields
			{
				FILE *fp;
				char buf[128];

				fp=fopen(csFileName, "r");		// text mode!
				if (NULL != fp) {
					pFile->LengthSectors=_filelength(_fileno(fp))/256+1;
					pFile->NumberRecords = pFile->LengthSectors;
					pFile->RecordsPerSector = 256  / (pFile->RecordLength + ((pFile->Status & FLAG_VARIABLE) ? 1 : 0) );
					pFile->BytesInLastSector = _filelength(_fileno(fp))%256;	// this is somewhat imaginary
					// need to count the number of records (line endings)
					pFile->NumberRecords = 1;	// counts last line
					while (!feof(fp)) {
						memset(buf, 0, 128);
						fread(buf, 1, 128, fp);
						for (int idx=0; idx<128; idx++) {
							if (buf[idx] == 0x0a) {
								pFile->NumberRecords++;
							}
						}
					}
				}
				fclose(fp);
			}
			return;
		} else {
			debug_write("%s looks like text, but text is disabled.", (LPCSTR)csFileName);
		}
	}

	// check for image filetype. Any file with NO extension is ok. 
	// These would always be DF128. 
	if ((csFileName.Find('.')==-1) && (bAllowNoHeaderAsDF128)) {
		debug_write("Detected %s as a PC (headerless) file - read as DF128.", (LPCSTR)csFileName);
		pFile->ImageType = IMAGE_IMG;
		// in an open call, STATUS will contain what is wanted,
		// but in others it may not. In that case it may often appear as fixed
		// We can check record length - if it's 0, return DF128
		if (pFile->RecordLength == 0) {
			// assume we should fill it in
			pFile->RecordLength = 128;
		}

		pFile->FileType &= ~(TIFILES_INTERNAL | TIFILES_VARIABLE);		// display/fixed format
		pFile->Status &= ~(FLAG_INTERNAL | FLAG_VARIABLE);

		// fill in some of the other fields
		{
			FILE *fp;
			fp=fopen(csFileName, "rb");
			if (NULL != fp) {
				pFile->LengthSectors=_filelength(_fileno(fp))/256+1;
				pFile->RecordsPerSector = 2;
				pFile->BytesInLastSector = (_filelength(_fileno(fp))%256+127)/128;	// 0, 128 or 256
				// can then calculate the number of records
				pFile->NumberRecords = pFile->LengthSectors*2-(pFile->BytesInLastSector==128?1:0);
			}
			fclose(fp);
		}
		return;
	} /* else {
		debug_write("%s looks like headerless image file, but that type is disabled.", (LPCSTR)csFileName);
	} */

	// no other match, just return default
	debug_write("%s could not be identified.", (LPCSTR)csFileName);
}


// Read the file into the disk buffer
// This function's job is to read the file into individual records
// into the memory buffer so it can be worked on generically. This
// function is not used for PROGRAM image files, but everything else
// is fair game. It reads the entire file, and it stores the records
// at maximum size (even for variable length files). Since TI files
// should max out at about 360k (largest floppy size), this should
// be fine (even though hard drives do exist, there are very few
// files for them, and thanks to MESS changing all the time the
// format seems not to be well defined.) Note that if you DO open
// a large file, though, Classic99 will try to allocate enough RAM
// to hold it. Buyer beware, so to speak. ;)
bool FiadDisk::BufferFile(FileInfo *pFile) {
	// this checks for PROGRAM images as well as Classic99 sequence bugs
	if (0 == pFile->RecordLength) {
		debug_write("Attempting to buffer file with 0 record length, can't do it.", (LPCSTR)pFile->csName);
		return false;
	}

	// all right, then, let's give it a shot.
	// So really, all we do here is branch out to the correct function
	switch (pFile->ImageType) {
		case IMAGE_UNKNOWN:
			debug_write("Attempting to buffer unknown file type %s, can't do it.", (LPCSTR)pFile->csName);
			break;

		case IMAGE_TIFILES:
		case IMAGE_V9T9:
			// these two can be handled the same way
			return BufferFiadFile(pFile);

		case IMAGE_TEXT:
			// Windows file
			return BufferTextFile(pFile);

		case IMAGE_IMG:
			return BufferImgFile(pFile);

		default:
			debug_write("Failed to buffer undetermined file type %d for %s", pFile->ImageType, (LPCSTR)pFile->csName);
	}
	return false;
}

// Buffer a TIFILES or V9T9 style file -- after the
// header, these filetypes are the same so both are
// handled here. 
bool FiadDisk::BufferFiadFile(FileInfo *pFile) {
	CString csFileName;
	int idx, nSector;
	unsigned char *pData;
	char tmpbuf[256];

	// Fixed records are obvious. Variable length records are prefixed
	// with a byte that indicates how many bytes are in this record. It's
	// all padded to 256 byte blocks. If it won't fit, the space in the
	// rest of the sector is wasted (and 0xff marks it)
	// Even better - the NumberRecords field in a variable file is a lie,
	// it's really a sector count. So we need to read them differently,
	// more like a text file.
	csFileName=BuildFilename(pFile);

	FILE *fp=fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Failed to open %s", (LPCSTR)csFileName);
		return false;
	}

	// get a new buffer as well sized as we can figure
	if (NULL != pFile->pData) {
		free(pFile->pData);
		pFile->pData=NULL;
	}

	// Datasize = (number of records+10) * (record size + 2)
	// the +10 gives it a little room to grow
	// the +2 gives room for a length word (16bit) at the beginning of each
	// record, necessary because it may contain binary data with zeros
	if (pFile->Status & FLAG_VARIABLE) {
		// like with the text files, we'll just assume a generic buffer size of 
		// about 100 records and grow it if we need to :)
		pFile->nDataSize = (100) * (pFile->RecordLength + 2);
		pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
	} else {
		// for fixed length fields we know how much memory we need
		pFile->nDataSize = (pFile->NumberRecords+10) * (pFile->RecordLength + 2);
		pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
	}

	idx=0;							// count up the records read
	nSector=256;					// bytes left in this sector
	fseek(fp, HEADERSIZE, SEEK_SET);// skip the header
	pData = pFile->pData;

	// we need to let the embedded code decide the terminating rule
	for (;;) {
		if (feof(fp)) {
			debug_write("Premature EOF - truncating read.");
			pFile->NumberRecords = idx;
			break;
		}

		if (pFile->Status & FLAG_VARIABLE) {
			// read a variable record
			int nLen=fgetc(fp);
			if (EOF == nLen) {
				debug_write("Corrupt file - truncating read.");
				pFile->NumberRecords = idx;
				break;
			}

			nSector--;
			if (nLen==0xff) {
				// end of sector indicator, no record read, skip rest of sector
				fread(tmpbuf, 1, nSector, fp);
				nSector=256;
				pFile->NumberRecords--;
				// are we done?
				if (pFile->NumberRecords == 0) {
					// yes we are, get the true count
					pFile->NumberRecords = idx;
					break;
				}
			} else {
				// check for buffer resize
				if ((pFile->pData+pFile->nDataSize) - pData < (pFile->RecordLength+2)*10) {
					int nOffset = pData - pFile->pData;		// in case the buffer moves
					// time to grow the buffer - add another 100 lines
					pFile->nDataSize += (100) * (pFile->RecordLength + 2);
					pFile->pData  = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
					pData = pFile->pData + nOffset;
				}
				
				// clear buffer
				memset(pData, 0, pFile->RecordLength+2);

				// check again
				if (nSector < nLen) {
					debug_write("Corrupted file - truncating read.");
					pFile->NumberRecords = idx;
					break;
				}

				// we got some data, read it in and count off the record
				// verify it (don't get screwed up by a bad file)
				if (nLen > pFile->RecordLength) {
					debug_write("Potentially corrupt file - skipping end of record.");
					
					// store length data
					*(unsigned short*)pData = pFile->RecordLength;
					pData+=2;

					fread(pData, 1, pFile->RecordLength, fp);
					nSector-=nLen;
					// skip the excess and trim down nLen
					fread(tmpbuf, 1, nLen - pFile->RecordLength, fp);
					nLen = pFile->RecordLength;
				} else {
					// record is okay (normal case)
					
					// write length data
					*(unsigned short*)pData = nLen;
					pData+=2;

					fread(pData, 1, nLen, fp);
					nSector-=nLen;
				}
				// count off a valid record and update the pointer
				idx++;
				pData+=pFile->RecordLength;
			}
		} else {
			// are we done?
			if (idx >= pFile->NumberRecords) {
				break;
			}

			// clear buffer
			memset(pData, 0, pFile->RecordLength+2);

			// read a fixed record
			if (nSector < pFile->RecordLength) {
				// not enough room for another record, skip to the next sector
				fread(tmpbuf, 1, nSector, fp);
				nSector=256;
			} else {
				// a little simpler, we just need to read the data
				*(unsigned short*)pData = pFile->RecordLength;
				pData+=2;

				fread(pData, 1, pFile->RecordLength, fp);
				nSector -= pFile->RecordLength;
				idx++;
				pData += pFile->RecordLength;
			}
		}
	}

	fclose(fp);
	debug_write("%s read %d records", (LPCSTR)csFileName, pFile->NumberRecords);
	return true;
}

// Buffer a Windows text file - records are delimited by
// end of line characters (any two). Multibyte and Unicode
// are not currently supported.
bool FiadDisk::BufferTextFile(FileInfo *pFile) {
	CString csFileName;
	unsigned char *pData;
	char *pTmp;

	csFileName=BuildFilename(pFile);
	pFile->NumberRecords=0;		// we don't know how many records there are, so we just read

	FILE *fp=fopen(csFileName, "r");
	if (NULL == fp) {
		debug_write("Failed to open %s", (LPCSTR)csFileName);
		return false;
	}

	// get a new buffer as well sized as we can figure
	if (NULL != pFile->pData) {
		free(pFile->pData);
		pFile->pData=NULL;
	}

	// Datasize = (100) * (record size + 2) + 1
	// we assume 100 records to start and resize as needed
	// the +2 gives room for a length word (16bit) at the beginning of each
	// record, necessary because it may contain binary data with zeros
	// The final +1 is so we can read a terminating NUL safely at the end
	// NULs in the middle are overwritten by the next record
	pFile->nDataSize = (100) * (pFile->RecordLength + 2) + 1;
	pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
	pData = pFile->pData;

	while (!feof(fp)) {
		// We assume all the prerequisites are met, and just read what we
		// are told. Records longer than requested are truncated.

		// check for buffer resize
		if ((pFile->pData+pFile->nDataSize) - pData < (pFile->RecordLength+2)*10) {
			int nOffset = pData - pFile->pData;		// in case the buffer moves
			// time to grow the buffer - add another 100 lines - the +1 is already in there
			// so we don't need it here.
			pFile->nDataSize += (100) * (pFile->RecordLength + 2);
			pFile->pData  = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
			pData = pFile->pData + nOffset;
		}

		// clear the buffer in case the read is short
		memset(pData+2, ' ', pFile->RecordLength);

		// The final +1 is so we can read a terminating NUL safely at the end
		// NULs in the middle are overwritten by the next record
		if (NULL == fgets((char*)(pData+2), pFile->RecordLength+1, fp)) {
			// no data was read! Ensure no duplicates
			*(pData+2)='\0';
		}
		// remove and pad EOL
		bool bEOLFound = false;
		char *pEnd = strchr((char*)(pData+2), '\0');
		while ((pTmp = strchr((char*)(pData+2), '\r')) != NULL) {
			*pTmp='\0';
			bEOLFound=true;
		}
		while ((pTmp = strchr((char*)(pData+2), '\n')) != NULL) {
			*pTmp='\0';
			bEOLFound=true;
		}
		*(unsigned short*)pData = (unsigned short)strlen((char*)(pData+2));
		if ((pFile->FileType & TIFILES_VARIABLE) == 0) {
			// fix up fixed width file
			// we don't want that NUL other
			pTmp=strchr((char*)(pData+2), '\0');
			if (NULL != pTmp) {
				// TI pads with spaces - especially need this for cases of fixed length
				*pTmp=' ';
				++pTmp;
				while (pTmp <= pEnd) {
					if (*pTmp == '\0') {
						*pTmp = ' ';
					} else {
						break;
					}
					++pTmp;
				}
			}
			*(unsigned short*)pData = (unsigned short)pFile->RecordLength;
		}
		pData+=pFile->RecordLength+2;
		pFile->NumberRecords++;
		if (!bEOLFound) {
			// we didn't read the whole line, so skim until we do
			// in text translation mode, the end of line should be just
			// 0x0a (\n)
			while (!feof(fp)) {
				if (fgetc(fp) == '\n') {
					break;
				}
			}
		}
	}

	fclose(fp);
	debug_write("%s read %d records", (LPCSTR)csFileName, pFile->NumberRecords);
	return true;
}

// Buffer a headerless file as DF128. Data is copied raw
// into the buffer 128 bytes at a time.
bool FiadDisk::BufferImgFile(FileInfo *pFile) {
	CString csFileName;
	unsigned char *pData;

	csFileName=BuildFilename(pFile);
	pFile->NumberRecords=0;		// we could calculate the number of records, but that's okay

	FILE *fp=fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Failed to open %s", (LPCSTR)csFileName);
		return false;
	}

	// get a new buffer as well sized as we can figure
	if (NULL != pFile->pData) {
		free(pFile->pData);
		pFile->pData=NULL;
	}

	// Datasize = (100) * (record size + 2) + 1
	// we assume 100 records to start and resize as needed
	// the +2 gives room for a length word (16bit) at the beginning of each
	// record, necessary because it may contain binary data with zeros
	// The final +1 is so we can read a terminating NUL safely at the end
	// NULs in the middle are overwritten by the next record
	pFile->nDataSize = (100) * (pFile->RecordLength + 2) + 1;
	pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
	pData = pFile->pData;

	while (!feof(fp)) {
		// We assume all the prerequisites are met, and just read what we
		// are told. 

		// check for buffer resize
		if ((pFile->pData+pFile->nDataSize) - pData < (pFile->RecordLength+2)*10) {
			int nOffset = pData - pFile->pData;		// in case the buffer moves
			// time to grow the buffer - add another 100 lines - the +1 is already in there
			// so we don't need it here.
			pFile->nDataSize += (100) * (pFile->RecordLength + 2);
			pFile->pData  = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
			pData = pFile->pData + nOffset;
		}

		// clear the buffer in case the read is short (TI pads with spaces)
		memset(pData+2, ' ', pFile->RecordLength);

		// read the next block of data
		fread((char*)(pData+2), 1, pFile->RecordLength, fp);
		*(unsigned short*)pData = (unsigned short)(128);
		pData+=pFile->RecordLength+2;
		pFile->NumberRecords++;
	}

	fclose(fp);
	debug_write("%s read %d records", (LPCSTR)csFileName, pFile->NumberRecords);
	return true;
}

// retrieve the disk name from a FIAD folder - in this case
// we just match on the last part of the path (folder name)
CString FiadDisk::GetDiskName() {
	CString csDiskName=GetPath();
	csDiskName.TrimRight('\\');
	int p = csDiskName.ReverseFind('\\');
	if (p != -1) {
		csDiskName = csDiskName.Mid(p+1);
	}

	return csDiskName;
}

// Write the disk buffer out to the file, with appropriate modes and headers
bool FiadDisk::Flush(FileInfo *pFile) {
	// first make sure this is an open file!
	if (!pFile->bDirty) {
		// nothing to flush, return success anyway
		return true;
	}

	// figure out what we are writing and call the appropriate code
	// It doesn't matter what the file was read as, we can write it as
	// whatever is configured.

	// The only real test here is whether it's going to be Windows or not,
	// and that's only valid for display type files.
	if ((pFile->Status & FLAG_INTERNAL) == 0) {
		// Check for Windows output override
		if (pFile->csOptions.Find('W') != -1) {
			return FlushWindowsText(pFile);
		} else {
			// display type
			if (pFile->Status & FLAG_VARIABLE) {
				// DV
				if ((bWriteAllDVAsText) || ((bWriteDV80AsText) && (pFile->RecordLength==80))) {
					return FlushWindowsText(pFile);
				}
			} else {
				// DF
				if ((bWriteAllDFAsText) || ((bWriteDF80AsText) && (pFile->RecordLength==80))) {
					return FlushWindowsText(pFile);
				}
			}
		}
	}

	return FlushFiad(pFile);
}

// Write out a display file as normal Windows text (\r\n ending)
bool FiadDisk::FlushWindowsText(FileInfo *pFile) {
	CString csFileName = BuildFilename(pFile);
	FILE *fp = fopen(csFileName, "wb");		// note we still use binary mode, just in case
	if (NULL == fp) {
		debug_write("Unable to write file %s", (LPCSTR)csFileName);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// TODO: this should check the valid text filename list when that's implemented
	if (csFileName.Right(4).CompareNoCase(".TXT") != 0) {
		debug_write("Warning: writing Windows text without .txt extension - Classic99 may not be able to re-read");
	}

	// The assumption is made that these are valid text strings,
	// but we will still write them out as binary
	unsigned char *pData = pFile->pData;
	if (NULL == pData) {
		debug_write("Warning: no data to flush.");
	} else {
		for (int idx=0; idx<pFile->NumberRecords; idx++) {
			if (pData-pFile->pData >= pFile->nDataSize) {
				break;
			}
			int nLen = *(unsigned short*)pData;
			pData+=2;

			fwrite(pData, nLen, 1, fp);
			pData+=pFile->RecordLength;

			fprintf(fp, "\r\n");
		}
	}

	fclose(fp);
	debug_write("Flushed %s (%d records) as Windows text", (LPCSTR)csFileName, pFile->NumberRecords);
	pFile->bDirty = false;

	return true;
}

// write out the file as a FIAD, including appropriate header
bool FiadDisk::FlushFiad(FileInfo *pFile) {
	// make sure there's anything to flush here
	if (!pFile->bDirty) {
		// nothing to flush, not open! Return success anyway
		return true;
	}

	CString csFileName = BuildFilename(pFile);
	FILE *fp = fopen(csFileName, "wb");
	if (NULL == fp) {
		debug_write("Unable to write file %s", (LPCSTR)csFileName);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// we can't write the header till we are done writing the file,
	// sadly. This could screw up software that tries to read the
	// file information before the file is closed, but tough. We
	// don't need to support that.
	unsigned char *pData = pFile->pData;
	if (NULL == pData) {
		// not really a big deal when the file is first created
		debug_write("Warning: no data to flush.");
	} else {
		// while going through here, we need to update the header records
		// for LengthSectors, and BytesInLastSector
		int nSector;

		// seek past the (currently empty) header
		fseek(fp, HEADERSIZE, SEEK_SET);

		pFile->LengthSectors = 1;		// at least one so far

		nSector=256;
		for (int idx=0; idx<pFile->NumberRecords; idx++) {
			if (pData-pFile->pData >= pFile->nDataSize) {
				break;
			}
			int nLen = *(unsigned short*)pData;
			pData+=2;

			if (pFile->Status & FLAG_VARIABLE) {
				// write a variable record
				// first, check it if will fit
				if (nSector < nLen+2) {		// 1 byte for record length, one for end of sector marker if needed
					// pad the sector out
					fputc(0xff, fp);
					nSector--;
					while (nSector > 0) {
						fputc(0, fp);
						nSector--;
					}
					nSector=256;
					pFile->LengthSectors++;
				}
				
				// write the length byte
				fputc(nLen&0xff, fp);
				nSector--;
				// write the data
				fwrite(pData, 1, nLen, fp);
				nSector-=nLen;
			} else {
				// write a fixed length record
				// first, check if it will fit
				if (nSector < pFile->RecordLength) {
					// pad the sector out
					while (nSector > 0) {
						fputc(0, fp);
						nSector--;
					}
					nSector=256;
					pFile->LengthSectors++;
				}

				// write the data
				if (nLen != pFile->RecordLength) {
					debug_write("Internal inconsistency - fixed record doesn't match record length");
					// this may crash? But it shouldn't happen, don't crash anyway
					nLen=pFile->RecordLength;
				}
				fwrite(pData, 1, nLen, fp);
				nSector-=nLen;
			}
			pData+=pFile->RecordLength;
		}

		// is this right? I guess it can't be on a new sector with 0 bytes...
		// this goes BEFORE the >FF byte is written on variable files
		pFile->BytesInLastSector = 256-nSector;

		if (pFile->Status & FLAG_VARIABLE) {
			// done writing - if this is variable, then we need to close the sector
			fputc(0xff, fp);
			nSector--;
		}
		
		// don't forget to pad the file to a full sector!
		while (nSector > 0) {
			fputc(0, fp);
			nSector--;
		}
	}

	// write the header before we are done
	WriteFileHeader(pFile, fp);

	fclose(fp);
	debug_write("Flushed %s (%d records) as %s FIAD", (LPCSTR)csFileName, pFile->NumberRecords, pFile->ImageType == IMAGE_V9T9?"V9T9":"TIFILES");
	pFile->bDirty = false;

	return true;
}

///////////////////////////////////////////////////////////////////////
// Writes a header - note: moves file pointer - overwrites first 128
// bytes. Must only be called on files with WRITE access.
// NOTE: For reference, the V9T9 source code includes source to
// XMDM2TI and TI2XMDM, where we could get some verification.
///////////////////////////////////////////////////////////////////////
void FiadDisk::WriteFileHeader(FileInfo *pFile, FILE *fp) {
	Byte h[128];							// header 
	int idx;

	// check the output type - this has no effect on the override
	if (IMAGE_UNKNOWN == pFile->ImageType) {
		if (bWriteV9T9) {
			pFile->ImageType = IMAGE_V9T9;
		} else {
			pFile->ImageType = IMAGE_TIFILES;
		}
	}
	// check override
	if (pFile->csOptions.Find('V') != -1) {
		pFile->ImageType = IMAGE_V9T9;
	} else if (pFile->csOptions.Find('T') != -1) {
		pFile->ImageType = IMAGE_TIFILES;
	}

	fseek(fp, 0, SEEK_SET);
	
	// some fixups -- I'm not 100% sure I trust all this, but Fred did
	// more work on it than I have. Till I have my real TI set up again
	// I will have to trust it.
	struct s_FILEINFO tmpInfo = *pFile;

	// Tim had concerns about software in the real world that copies around some of the
	// extended bits, like the Myarc 'modify' bit, and thus confused other disk controllers.
	// So, since we are writing, strip out any bits that make no sense to us. This will
	// also strip the protection bit, which was never very useful anyway.
	tmpInfo.FileType &= TIFILES_MASK;

	if (tmpInfo.FileType & TIFILES_PROGRAM) {
		// Zero out some headers for PROGRAM images -- these I did confirm with V9T9's tools and other examples
		tmpInfo.RecordsPerSector=0;
		tmpInfo.RecordLength=0;
		tmpInfo.NumberRecords=0;
	} else {
		if (tmpInfo.FileType & TIFILES_VARIABLE) {
			// on variable files, the 'bytes in last sector' field does NOT include the final 0xff
			// Also, the NumberRecords field should have the number of SECTORS, not records (does
			// not include the header sector).
			tmpInfo.NumberRecords = tmpInfo.LengthSectors;
		} else {
			// Fred says EOF offset should be zero on fixed files, so do that
			tmpInfo.BytesInLastSector=0;
		}
	}

	memset(h, 0, 128);							// initialize with zeros

	if (IMAGE_V9T9 == pFile->ImageType) {
		/* create v9t9 header */
		// does not support long filenames
		// does not support extended characters very well
		// not recommended
		memset(h, ' ', 10);
		for (idx=0; idx<pFile->csName.GetLength(); idx++) {
			if (idx>=10) break;
			h[idx]=pFile->csName[idx];
		}

		h[12] = tmpInfo.FileType;				// File type
		h[13] = tmpInfo.RecordsPerSector;		// records/sector 
		h[14] = tmpInfo.LengthSectors>>8;		// length in sectors HB 
		h[15] = tmpInfo.LengthSectors&0xff;		// LB 
		h[16] = tmpInfo.BytesInLastSector;		// # of bytes in last sector 
		h[17] = tmpInfo.RecordLength;			// record length 
		h[18] = tmpInfo.NumberRecords&0xff;		// # of records(FIX)/sectors(VAR) LB! 
		h[19] = tmpInfo.NumberRecords>>8;		// HB 
		fwrite(h, 1, 128, fp);
	} else if (pFile->ImageType == IMAGE_TIFILES) {
		/* create TIFILES header - more up to date version of the header? */
		h[0] = 7;
		h[1] = 'T';
		h[2] = 'I';
		h[3] = 'F';
		h[4] = 'I';
		h[5] = 'L';
		h[6] = 'E';
		h[7] = 'S';
		h[8] = tmpInfo.LengthSectors>>8;			// length in sectors HB
		h[9] = tmpInfo.LengthSectors&0xff;			// LB 
		h[10] = tmpInfo.FileType;					// File type 
		h[11] = tmpInfo.RecordsPerSector;			// records/sector
		h[12] = tmpInfo.BytesInLastSector;			// # of bytes in last sector
		h[13] = tmpInfo.RecordLength;				// record length 
		h[14] = tmpInfo.NumberRecords&0xff;			// # of records(FIX)/sectors(VAR) LB 
		h[15] = tmpInfo.NumberRecords>>8;			// HB

#if 0
		// There are a number of TIFILES Extensions that are being
		// gently pushed as "good ideas" (actually they are being
		// documented as part of the standard and anything that
		// does not include them is documented as a 'variant', which
		// I /really/ find offensive, but that said...)
		//
		// 0x10 - 0x19 - TI filename (10 character limit)
		//		I refuse to support embedded filenames because 
		//		(a) they represent yet another copy of something that can get out of sync
		//		(b) in 99% of cases they are redundant
		//		(c) they represent tying the new DSR to the TI disk controller, something I'd like to get away from
		//		(d) as with most of the other cases, this one is limited to 10 characters (TI CC legacy)
		//		(e) they unnecessarily complicate renaming the file outside of the emulation environment
		//
		// 0x1A	- MXT - only meaningful in file transfers, means more files are coming
		// 
		// 0x1C-0x1D - 0xFFFF if more fields are set, else 0x0000
		// 
		// 0x1E-0x21 - Creation Time
		// 0x22-0x25 - Update time
		//			Timestamps are 16 bit words formatted as hhhh.hmmm.mmms.ssss, yyyy.yyyM.MMMd.dddd. Resolution is 2 seconds, year of 70 means 1970.
		//			I am not completely adverse to the timestamp fields, although they are an extension from
		//			later systems like the Geneve which actually had a clock. However, the TI Disk Controller
		//			didn't support them and as a result, very little software ever reads it. I'm slightly biased
		//			against adding them because software doesn't expect them to be there.
		//
		// There, now they are documented. ;)
#endif

		fwrite(h, 1, 128, fp);
	}
}

#define SHORT_FILENAME_RECORDS	38
#define LONG_FILENAME_RECORDS 254

// Open a file with a particular mode, creating it if necessary
FileInfo *FiadDisk::Open(FileInfo *pFile) {
	FileInfo *pNewFile=NULL;

	if (pFile->bOpen) {
		// trying to open a file that is already open! Can't allow that!
		pFile->LastError = ERR_FILEERROR;
		return NULL;
	}

	// See if we can get a new file handle from the driver
	pNewFile = pDriveType[pFile->nDrive]->AllocateFileInfo();
	if (NULL == pNewFile) {
		// no files free
		pFile->LastError = ERR_BUFFERFULL;
		return NULL;
	}

	if (pFile->Status & FLAG_VARIABLE) {
		// variable length file - check maximum length
		if (pFile->RecordLength > 254) {
			pFile->LastError = ERR_BADATTRIBUTE;
			goto error;
		}
	}

	// So far so good -- let's try to actually find the file
	// We need to find the file, verify it is correct to open it,
	// then load all its data into a buffer.

	// TODO: Some directory enhancements
	// 1) support subdirectories for directory listing (using only \ as a separator)
	//	  There are a lot of problems adapting '.' when it comes to telling extensions
	//	  apart from paths. It's solvable on a read, but on a write you are just guessing!
	//
	// 2) The long filename support is a bust - TI Artist just crashes, for instance.
	//	  So change it - normally a directory is Internal/Fixed 38 (often opened as 0).
	//	  For long filenames, use Internal/Variable 0, and return IV254 records with
	//	  filenames up to 127 characters. (Done)
	//
	// 3) As part of the above, code a mapping for long filenames to short. We can't use
	//	  the windows scheme, as the resulting filename is still 12 characters long and
	//	  this won't work on a Linux filesystem. I recommend using the '~' as a tag at
	//	  a fixed position, position 4. So the short filename is 3 characters from the
	//	  beginning, the '~', then 6 characters from the end. This is because extensions
	//	  are generally 4 characters (.EXT), so allows us to keep 2 characters from the
	//	  real filename. Of course, if there is no extension then we get more of the end,
	//	  that should still be okay. In the event of collision, place a digit (or more) after the
	//	  tilde, up to 6 digits should be more then enough for this emulation!!
	//	  Example: ANATASIA5089_P becomes ANA~5089_P
	//	  The tilde should be looked for only when the filename is 10 characters. Also,
	//	  be aware that files saved with the character actually take on that filename!
	//	  Alternate idea: a mapping file on the disk of long->short names?

	// Check for directory (empty filename)
	// TODO: subdirectories using '.' someday...
	if (pFile->csName == ".") {
		CString csTmp;
		char *pData, *pStart;

		// read directory!
		if ((pFile->Status & FLAG_MODEMASK) != FLAG_INPUT) {
			debug_write("Can't open directory for anything but input");
			pFile->LastError = ERR_ILLEGALOPERATION;
			goto error;
		}
		// check for 0, map to actual size (38 normally!)
		if (pFile->RecordLength == 0) {
			// If opened for variable length, then allow long records (if configured)
			if ((bEnableLongFilenames) && (pFile->Status & FLAG_VARIABLE)) {
				pFile->RecordLength = LONG_FILENAME_RECORDS;
			} else {
				pFile->RecordLength = SHORT_FILENAME_RECORDS;
			}
		} else {
			// only legal /explicit/ value is 38 (you may not specify 254)
			if (pFile->RecordLength != SHORT_FILENAME_RECORDS) {
				debug_write("Record length must be 0 or 38 (requested %d)", pFile->RecordLength);
				pFile->LastError = ERR_BADATTRIBUTE;
				goto error;
			}
		}

		// final checks, validate the open type against the record length
		// at this point, we must have either IV/254 (long filenames) or IF/38 (short)
		// we can use the record length after this point to figure which one we have
		if (pFile->RecordLength == SHORT_FILENAME_RECORDS) {
			// it MUST be IF/38 then
			if ((pFile->Status & FLAG_TYPEMASK) != FLAG_INTERNAL) {
				debug_write("Regular directory must be IF38");
				pFile->LastError = ERR_BADATTRIBUTE;
				goto error;
			}
		} else if (pFile->RecordLength == LONG_FILENAME_RECORDS) {
			// it MUST be IV/254 then
			if ((pFile->Status & FLAG_TYPEMASK) != (FLAG_INTERNAL | FLAG_VARIABLE)) {
				debug_write("Long filename directory must be IV254");
				pFile->LastError = ERR_BADATTRIBUTE;
				goto error;
			}
		} else {
			debug_write("Unacceptable record length %d for directory - pass 0 for default.", pFile->RecordLength);
			pFile->LastError = ERR_BADATTRIBUTE;
			goto error;
		}

		// Well, it's good then, buffer and format the directory records
		FileInfo *Filenames = NULL;
		pFile->NumberRecords = GetDirectory(pFile, Filenames);
		if (Filenames == NULL) {
			// disk error of some sort
			debug_write("Could not get directory.");
			pFile->LastError = ERR_DEVICEERROR;
			goto error;
		}

		// get a new buffer as well sized as we can figure
		if (NULL != pFile->pData) {
			free(pFile->pData);
			pFile->pData=NULL;
		}

		// sadly we can't make /this/ directory support longer names either,
		// because most applications that use it either hardcode the 38 or
		// they assume the filename is always 10 characters long. We can
		// return the short filename though! :)

		// for fixed length fields we know how much memory we need
		// we add two records - one for record 0, one for the terminator
		pFile->nDataSize = (pFile->NumberRecords+2) * (pFile->RecordLength + 2);
		pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
		pData = (char*)pFile->pData;

		// Record 0 contains:
		// - Diskname (an ascii string of upto 10 chars).
		// - The number zero.
		// - The number of sectors on disk (minus 2 for the VIB and FDIR)
		// - The number of free sectors on disk.
		memset(pData, 0, pFile->RecordLength+2);
		csTmp = pDriveType[pFile->nDrive]->GetPath();

		// DiskName - first 10 bytes - we provide the local path (last 10 chars of it)
		pStart=pData;
		csTmp = csTmp.Right(10);
		*(unsigned short*)pData = (unsigned short)pFile->RecordLength;						// record length (2 bytes)
		pData+=2;
		*(pData++) = csTmp.GetLength();														// string length (strange, not very fixed length. My bug? TODO: probably this should always be 10 chars long)
		memcpy(pData, (LPCSTR)csTmp, csTmp.GetLength());									// string
		pData+=csTmp.GetLength();
		pData=WriteAsFloat(pData,0);	// always 0											// 8 bytes
		pData=WriteAsFloat(pData,1438);	// we lie and say 1440 sectors total (minus 2)		// 8 bytes
		pData=WriteAsFloat(pData,1311);	// we lie and say 1311 sectors free (1440-2-127)	// 8 bytes

		// now the rest of the entries. 
		// - Filename (an ascii string of upto 10 chars) 
		// - Filetype: 1=D/F, 2=D/V, 3=I/F, 4=I/V, 5=Prog, 0=end of directory.
		//   If the file is protected, this number is negative (-1=D/F, etc).
		// - File size in sectors (including the FDR itself).
		// - File record length (0 for programs).
		for (int idx=0; idx<pFile->NumberRecords; idx++) {
			pData = pStart + pFile->RecordLength+2;
			pStart = pData;

			memset(pData, 0, pFile->RecordLength+2);
			csTmp = Filenames[idx].csName;
			if (pFile->RecordLength == SHORT_FILENAME_RECORDS) {
				csTmp = csTmp.Left(10);
#if 0
				// don't pad the string - even though it's supposedly fixed 38 bytes,
				// the disk controller doesn't pad the fields (so it can be less!)
				if (csTmp.GetLength() < 10) {	// fixed records
					csTmp += "          ";
					csTmp = csTmp.Left(10);
				}
#endif
			} else {
				csTmp = csTmp.Left(127);		// variable. arbitrary size, but a good max. SCSI limits to 40 for the full path!
			}
			*(unsigned short*)pData = (unsigned short)pFile->RecordLength;					// record length (my structure)
			pData+=2;
			*(pData++) = csTmp.GetLength();													// string length
			memcpy(pData, (LPCSTR)csTmp, csTmp.GetLength());								// filename
			pData+=csTmp.GetLength();
			int nType=0;
			if (Filenames[idx].FileType & TIFILES_PROGRAM) {
				nType = 5;
			} else {
				nType = 1;	// DF
				if (Filenames[idx].FileType & TIFILES_INTERNAL) {
					nType+=2;
				}
				if (Filenames[idx].FileType & TIFILES_VARIABLE) {
					nType+=1;
				}
			}
			pData=WriteAsFloat(pData, nType);												// file type
			pData=WriteAsFloat(pData, Filenames[idx].LengthSectors+1);						// file length (1 sector added for directory per standard)
			pData=WriteAsFloat(pData, Filenames[idx].RecordLength);							// record length
		}
		
		// free the array
		free(Filenames);
		Filenames=NULL;

		// and last, the terminator
		pData = pStart + pFile->RecordLength+2;
		pStart = pData;

		memset(pData, 0, pFile->RecordLength+2);
		csTmp = "";
		*(unsigned short*)pData = (unsigned short)38;
		pData+=2;
		*(pData++) = csTmp.GetLength();
		memcpy(pData, (LPCSTR)csTmp, csTmp.GetLength());
		pData+=csTmp.GetLength();
		pData=WriteAsFloat(pData, 0);
		pData=WriteAsFloat(pData, 0);
		pData=WriteAsFloat(pData, 0);

		// now add two to the number of records (header and terminator)
		pFile->NumberRecords+=2;

		// that should do it!
	} else {
		// let's see what we are doing here...
		switch (pFile->Status & FLAG_MODEMASK) {
			case FLAG_OUTPUT:
				if (!CreateOutputFile(pFile)) {
					goto error;
				}
				break;

			// we can open these both the same way
			case FLAG_UPDATE:
			case FLAG_APPEND:
				// First, try to open the file
				if (!TryOpenFile(pFile)) {
					// No? Try to create it then?
					if (pFile->LastError == ERR_FILEERROR) {
						if (!CreateOutputFile(pFile)) {
							// Still no? Error out then (save error already set)
							goto error;
						}
					} else {
						goto error;
					}
				}

				// So we should have a file now - read it in
				if (!BufferFile(pFile)) {
					pFile->LastError = ERR_FILEERROR;
					goto error;
				}
				break;

			default:	// should only be FLAG_INPUT
				if (!TryOpenFile(pFile)) {
					// Error out then, must exist for input (save old error)
					goto error;
				}

				// So we should have a file now - read it in
				if (!BufferFile(pFile)) {
					pFile->LastError = ERR_FILEERROR;
					goto error;
				}
				break;
		}
	}

	// Finally, transfer the object over to the DriveType object
	pNewFile->CopyFileInfo(pFile, false);
	return pNewFile;

error:
	// release the allocated fileinfo, we didn't succeed to open
	// Use the base class as we have nothing to flush
	Close(pNewFile);
	return NULL;
}

// Load a PROGRAM image file - this happens immediately and doesn't
// need to use a buffer (nor does it close)
bool FiadDisk::Load(FileInfo *pFile) {
	FILE *fp;
	int read_bytes;
	CString csFileName = BuildFilename(pFile);

	// sanity check -- make sure we don't request more data
	// than there is RAM. A real TI would probably wrap the 
	// address counter, but for simplicity we don't. It's
	// likely a bug anyway! If we want to do emulator proof
	// code, though... ;)
	if (pFile->DataBuffer + pFile->RecordNumber > 0x4000) {
		debug_write("Attempt to load bytes past end of VDP, truncating");
		pFile->RecordNumber = 0x4000 - pFile->DataBuffer;
	}

	// We may need to fill in some of the FileInfo object here
	pFile->Status = FLAG_INPUT;
	pFile->FileType = TIFILES_PROGRAM;
	
	if (!TryOpenFile(pFile)) {
		// couldn't open the file - keep original error code
		return false;
	}
	int nDetectedLength = pFile->LengthSectors*256 + pFile->BytesInLastSector;
	if (pFile->BytesInLastSector != 0) {
		nDetectedLength -= 256;
	}

	// the TI disk controller will fail the load if the file is larger than the buffer!
	// (It won't load a partial file). It's okay if the buffer is larger than the file.
	if (nDetectedLength > pFile->RecordNumber) {
		debug_write("Requested file is larger than available buffer, failing.");
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// XB first tries to load as a PROGRAM image file with the
	// maximum available VDP RAM. If that fails with code 0x60,
	// it tries again as the proper DIS/FIX254
	// I am leaving this comment here, but hacks might not be
	// needed anymore now that the headers are properly tested.

	// It's good, so we try to open it now
	fp=fopen(csFileName, "rb");
	fseek(fp, HEADERSIZE, SEEK_SET);	// we know it has a fixed size header since that's all we support
	read_bytes = fread(&VDP[pFile->DataBuffer], 1, min(pFile->RecordNumber, nDetectedLength), fp);
	debug_write("loading 0x%X bytes", read_bytes);	// do we need to give this value to the user?
	fclose(fp);										// all done
		
	// handle DSK1 automapping (AutomapDSK checks whether it's enabled)
	AutomapDSK(&VDP[pFile->DataBuffer], min(pFile->RecordNumber, nDetectedLength), pFile->nDrive, false);
		
	// update heatmap
	for (int idx=0; idx<read_bytes; idx++) {
		UpdateHeatVDP(pFile->DataBuffer+idx);
	}

	return true;
}

// Save a PROGRAM image file
bool FiadDisk::Save(FileInfo *pFile) {
	CString csFileName = BuildFilename(pFile);
	FILE *fp;

	// sanity check -- make sure we don't save more data
	// than there is RAM. A real TI would probably wrap the 
	// address counter, but for simplicity we don't. It's
	// likely a bug anyway! If we want to do emulator proof
	// code, though... ;)
	if (pFile->DataBuffer + pFile->RecordNumber > 0x4000) {
		debug_write("Attempt to save bytes past end of VDP, truncating");
		pFile->RecordNumber = 0x4000 - pFile->DataBuffer;
	}

	// there is no try on an output file -- do or do not!
	debug_write("saving 0x%X bytes file %s", pFile->RecordNumber, csFileName);

	fp=fopen(csFileName, "wb");
	if (NULL == fp) {
		// couldn't open the file
		debug_write("Can't open for writing, errno %d", errno);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// We need to fill in some of the FileInfo object here
	pFile->Status = FLAG_OUTPUT;
	pFile->FileType = TIFILES_PROGRAM;
	pFile->LengthSectors = (pFile->RecordNumber+255)/256;
	pFile->RecordsPerSector = 0;
	pFile->BytesInLastSector = pFile->RecordNumber & 0xff;
	pFile->RecordLength = 0;
	pFile->NumberRecords = 0;

	// Write the appropriate header
	WriteFileHeader(pFile, fp);

	if (fwrite(VDP+pFile->DataBuffer, 1, pFile->RecordNumber, fp) == (unsigned int)pFile->RecordNumber) {
		// pad it up to a full sector multiple
		int nPad = pFile->RecordNumber % 0x100;
		if (nPad > 0) {
			nPad = 256 - nPad;
			for (int idx=0; idx < nPad; idx++) {
				fputc(0, fp);
			}
		}
	} else {
		debug_write("Failed to write full length of file. errno %d", errno);
		pFile->LastError = ERR_FILEERROR;
	}

	fclose(fp);

	// update heatmap
	for (int idx=0; idx<pFile->RecordNumber; idx++) {
		UpdateHeatVDP(pFile->DataBuffer+idx);
	}

	return (pFile->LastError == ERR_NOERROR);
}

// SBRLNK opcodes (files is handled by shared handler)

// Read the volume information block (sector 0)
bool FiadDisk::ReadVIB(FileInfo *pFile) {
	unsigned char *p;
	CString csPath = pDriveType[pFile->nDrive]->GetPath();

	// DiskName - first 10 bytes - we provide the local path (last 10 chars of it)
	while (csPath.GetLength() < 10) {
		csPath += ' ';
	}
	csPath = csPath.Right(10);
	memcpy(&VDP[pFile->DataBuffer], (LPCSTR)csPath, 10);

	// next block
	p = &VDP[pFile->DataBuffer+0x0a];	// point to next part
	*p++ = 0x05;						// number of sectors (we write 0x5A0 - 1440 - same as a DSDD)
	*p++ = 0xa0;
	*p++ = 0x12;						// Sectors per track, we write 0x12 (DD)
	*p++ = 'D';							// Magic tag, "DSK"
	*p++ = 'S';
	*p++ = 'K';
	*p++ = ' ';							// "P" for protected, would be space otherwise
	*p++ = 0x28;						// Tracks per side, we write 0x28 for DD
	*p++ = 0x02;						// Number of sides (2 for DS)
	*p++ = 0x02;						// Density (2 for DD)
	
	// write out the reserved bytes as zero
	memset(p, 0x00, 36);
	p+=36;

	// write out the disk bitmap as fully empty, except the first 129 sectors
	memset(p, 0xff, 16);	// 128
	p+=16;
	*p++ = 0x80;			// 129
	memset(p, 0x00, 163);	// rest of the bitmap
	p+=163;

	// write out the last reserved bytes as 0xff
	memset(p, 0xff, 20);

	// should total 256 bytes exactly - check if you alter this code.
	return true;
}

// Get a list of the files in the current directory (up to 127, unless bMoreThan127Files is set)
// returns the count of files, and an allocated array at Filenames
// The array is a list of FileInfo objects
// Caller must free Filenames!
// To reduce thrashing, we track the last used directory for 15 seconds :)
int FiadDisk::GetDirectory(FileInfo *pFile, FileInfo *&Filenames) {
	int n;
	HANDLE fsrc;
	WIN32_FIND_DATA myDat;
	CString csSearchPath;

	// check cache first
	if ((nCachedDrive == pFile->nDrive) &&
		(pCachedFiles != NULL) &&
		(time(NULL) < tCachedTime+15)) {
			// just replay the cache
			// It's okay to copy since these don't include any data
			// But just to protect from later changes, we'll NULL the data pointers
			Filenames = (FileInfo*)malloc(_msize(pCachedFiles));
			memcpy(Filenames, pCachedFiles, _msize(pCachedFiles));
			return nCachedCount;
	} else {
		// free any old cache
		if (NULL != pCachedFiles) {
			free(pCachedFiles);
			pCachedFiles=NULL;
		}
	}

	// I dislike realloc, but that's the best way to solve the dynamic size issue,
	// besides counting twice (and that's not guaranteed not to change!)
	Filenames = (FileInfo*)malloc(sizeof(FileInfo));	// get one
	if (NULL == Filenames) {
		debug_write("Couldn't malloc memory for directory.");
		return 0;
	}
	new(&Filenames[0]) FileInfo;

	n=0;
	csSearchPath.Format("%s*", (LPCSTR)pDriveType[pFile->nDrive]->GetPath());
	fsrc=FindFirstFile(csSearchPath, &myDat);
	if (INVALID_HANDLE_VALUE == fsrc) {
		n=0;
	} else {
		do {
			// Make uppercase - do we still do this? (TODO: maybe another option?)
			_strupr(myDat.cFileName);
			// get the path for the sake of collecting data
			csSearchPath.Format("%s%s", (LPCSTR)pDriveType[pFile->nDrive]->GetPath(), myDat.cFileName);
			// cache the fileinfo header too.
			// This way we can automatically handle new filetypes without adding them here
			Filenames[n].csName = myDat.cFileName;
			DetectImageType(&Filenames[n], csSearchPath);

			if (IMAGE_UNKNOWN != Filenames[n].ImageType) {
				// it's a format we recognize, keep it
				Filenames[n].csName = myDat.cFileName;
				
				// if it's got a TIAP or TIAC extension, fake it to
				// _P or _C so TI apps recognize it. We fake it back
				// again on load anyway. (Also my own _M)
				if (Filenames[n].csName.Right(5).MakeUpper() == ".TIAP") {
					Filenames[n].csName.Replace(".TIAP", "_P");
				} else if (Filenames[n].csName.Right(5).MakeUpper() == ".TIAC") {
					Filenames[n].csName.Replace(".TIAC", "_C");
				} else if (Filenames[n].csName.Right(5).MakeUpper() == ".TIAM") {
					Filenames[n].csName.Replace(".TIAM", "_M");
				}

				n++;
				if ((n>126) && (!bAllowMore127Files)) {
					break;
				}

				// else, allocate the next entry
				Filenames = (FileInfo*)realloc(Filenames, sizeof(FileInfo)*(n+1));	// get one more
				if (NULL == Filenames) {
					debug_write("Couldn't realloc memory for directory after %d entries.", n);
					return 0;
				}
				new(&Filenames[n]) FileInfo;
			}
		} while (FindNextFile(fsrc, &myDat));
		FindClose(fsrc);
	}

	// copy into the cache
	pCachedFiles=(FileInfo*)malloc(_msize(Filenames));
	memcpy(pCachedFiles, Filenames, _msize(Filenames));
	nCachedDrive=pFile->nDrive;
	time(&tCachedTime);
	nCachedCount = n;

	return n;
}

// Read File Descriptor Record - a bit of a misnomer,
// since it's already read into Filenames, we just need to
// parse and return it to the user. pFile is the request,
// Filenames is the file to be parsed. Technically this
// should be the same as the V9T9 or TIFILES header, but
// there seem to be variations in places.
bool FiadDisk::ReadFDR(FileInfo *pFile, FileInfo *Filenames) {
	unsigned char *p;
	CString csTmp;

	p=&VDP[pFile->DataBuffer];

	// first 10 chars are the filename
	csTmp = Filenames->csName;
	while (csTmp.GetLength() < 10) {
		csTmp+=' ';
	}
	csTmp = csTmp.Left(10);			// take the first 10 chars
	memcpy(p, (LPCSTR)csTmp, 10);
	p+=10;

	*(p++)=0;								// reserved
	*(p++)=0;
	*(p++)=Filenames->FileType;				// TIFILES file type
	*(p++)=Filenames->RecordsPerSector;
	*(p++)=Filenames->LengthSectors/256;
	*(p++)=Filenames->LengthSectors%256;
	*(p++)=Filenames->BytesInLastSector;
	*(p++)=Filenames->RecordLength;
	*(p++)=Filenames->NumberRecords%256;	// NOTE: Little endian!
	*(p++)=Filenames->NumberRecords/256;
	// 0x14-0x1b are reserved (leave as 0)
	// 0x1c-0xff are the cluster list (not used here, left as 0)

	return true;
}

// ReadSector: nDrive=Drive#, DataBuffer=VDP address, RecordNumber=Sector to read
// Must return the sector number in RecordNumber if no error.
bool FiadDisk::ReadSector(FileInfo *pFile) {
	bool nRet = true;

	// sanity test
	if (pFile->DataBuffer + 256 > 0x4000) {
		debug_write("Attempt to read sector past end of VDP memory, aborting.");
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// Zero buffer first
	memset(&VDP[pFile->DataBuffer], 0, 256);

	// which sector?
	if (pFile->RecordNumber == 0) {
		return ReadVIB(pFile);
	} else {
		/* other sector */
		FileInfo *Filenames = NULL;
		int nCnt = GetDirectory(pFile, Filenames);
		if (Filenames == NULL) {
			// disk error of some sort
			debug_write("Could not get directory.");
			pFile->LastError = ERR_DEVICEERROR;
			nRet = false;
		} else {
			if (pFile->RecordNumber == 1) {
				// Sector 1 is the FDR index - pointers to all files on the disk
				for (int idx = 0; idx < nCnt; idx++) {
					// We store our FDRs in the first 129 "sectors", so we only need one byte each
					VDP[pFile->DataBuffer + (2*idx) + 1] = idx + 2;
				}
			} else {
				// another sector - if it's not an FDR sector then it's invalid
				if ((nCnt > 0) && (pFile->RecordNumber >= 2) && (pFile->RecordNumber < nCnt+2)) {
					nRet = ReadFDR(pFile, &Filenames[pFile->RecordNumber-2]);
				} else {
					debug_write("Attempt to read not simulated sector %d", pFile->RecordNumber);
					pFile->LastError = ERR_DEVICEERROR;
					nRet = false;
				}
			}
		}

		if (NULL != Filenames) {
			free(Filenames);
		}
	}
	return nRet;
}

// Read a file by sectors (file type irrelevant)
// LengthSectors - number of sectors to read
// csName - filename to read
// DataBuffer - address of data in VDP
// RecordNumber - first sector to read
// If 0 sectors spectified, the following are returned by ReadFileSectors, 
// They are the same as in a PAB.
// FileType, RecordsPerSector, BytesInLastSector, RecordLength, NumberRecords
// On return, LengthSectors must contain the actual number of sectors read,
bool FiadDisk::ReadFileSectors(FileInfo *pFile) {
	FILE *fp;
	FileInfo lclFile;
	CString csFilename = BuildFilename(pFile);

	if (pFile->LengthSectors == 0) {
		DetectImageType(&lclFile, csFilename);
		if (IMAGE_UNKNOWN == lclFile.ImageType) {
			pFile->LastError = ERR_FILEERROR;
			return false;
		}
		pFile->LengthSectors = lclFile.LengthSectors;
		pFile->FileType = lclFile.FileType;
		pFile->RecordsPerSector = lclFile.RecordsPerSector;
		pFile->BytesInLastSector = lclFile.BytesInLastSector;
		pFile->RecordLength = lclFile.RecordLength;
		pFile->NumberRecords = lclFile.NumberRecords;

		debug_write("Information request on file %s (Type >%02x, %d Sectors, Records Per Sector %d, EOF Offset %d, Record Length %d, Number Records %d)", 
			pFile->csName, pFile->FileType, pFile->LengthSectors, pFile->RecordsPerSector, pFile->BytesInLastSector, pFile->RecordLength, pFile->NumberRecords);

		return true;
	}

	// sanity test
	if (pFile->LengthSectors*256 + pFile->DataBuffer > 0x4000) {
		debug_write("Attempt to sector read file %s past end of VDP, truncating.", pFile->csName);
		pFile->LengthSectors = (0x4000 - pFile->DataBuffer) / 256;
		if (pFile->LengthSectors < 1) {
			debug_write("Not enough VDP RAM for even one sector, aborting.");
			pFile->LastError = ERR_BUFFERFULL;
			return false;
		}
	}

	debug_write("Reading drive %d file %s sector %d-%d to VDP %04x", pFile->nDrive, pFile->csName, pFile->RecordNumber, pFile->RecordNumber+pFile->LengthSectors-1, pFile->DataBuffer);

	// Read the requested sectors from the file
	DetectImageType(&lclFile, csFilename);
	if (IMAGE_UNKNOWN == lclFile.ImageType) {
		debug_write("Can't get file type of %s", csFilename);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}
	if ((IMAGE_TIFILES != lclFile.ImageType) && (IMAGE_V9T9 != lclFile.ImageType)) {
		debug_write("Only TIFILES or V9T9 supported for sector read on %s", csFilename);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	fp=fopen(csFilename, "rb");
	fseek(fp, pFile->RecordNumber*256+HEADERSIZE, SEEK_SET);
	int readcnt = fread(&VDP[pFile->DataBuffer], 1, pFile->LengthSectors*256, fp);
	pFile->LengthSectors = (readcnt+255)/256;
	fclose(fp);

	if (pFile->LengthSectors == 0) {
		debug_write("Failed to read any data from the file!");
		pFile->RecordNumber = 0;			// record number is zeroed on read past EOF (is error thrown?)
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	return true;
}

// Write a file by sectors (file type irrelevant)
// LengthSectors - number of sectors to write
// csName - filename to write
// DataBuffer - address of data in VDP
// RecordNumber - first sector to write
// If 0 sectors spectified, the following are used to create the file.  
// They are the same as in a PAB. (If not 0, file must exist).
// FileType, RecordsPerSector, BytesInLastSector, RecordLength, NumberRecords
// On return, LengthSectors must contain the actual number of sectors written,
// leave as 0 if 0 was originally specified.
bool FiadDisk::WriteFileSectors(FileInfo *pFile) {
	FILE *fp;
	FileInfo lclFile;
	CString csFilename = BuildFilename(pFile);

	if (pFile->LengthSectors == 0) {
		// Create File
		fp=fopen(csFilename, "wb");
		if (NULL == fp) {
			debug_write("Can't create %s, errno %d", csFilename, errno);
			pFile->LastError = ERR_DEVICEERROR;
			return false;
		}
		if (pFile->RecordNumber != 0) {
			// write out this many sectors
			char buf[256];
			memset(buf, 0, sizeof(buf));

			fseek(fp, (pFile->RecordNumber-1)*256+HEADERSIZE, SEEK_SET);
			fwrite(buf, 256, 1, fp);
		}

		WriteFileHeader(pFile, fp);
		fclose(fp);

		debug_write("Low-level created file %s (Type >%02x, Records Per Sector %d, EOF Offset %d, Record Length %d, Number Records %d, Record # %d)", 
			pFile->csName, pFile->FileType, pFile->RecordsPerSector, pFile->BytesInLastSector, pFile->RecordLength, pFile->NumberRecords, pFile->RecordNumber);
		
		return true;
	}

	// sanity test
	if (pFile->LengthSectors*256 + pFile->DataBuffer > 0x4000) {
		debug_write("Attempt to sector write file %s past end of VDP, truncating.", pFile->csName);
		pFile->LengthSectors = (0x4000 - pFile->DataBuffer) / 256;
		if (pFile->LengthSectors < 1) {
			debug_write("Not enough VDP RAM for even one sector, aborting.");
			pFile->LastError = ERR_BUFFERFULL;
			return false;
		}
	}

	debug_write("Writing drive %d file %s sector %d-%d from VDP %04x", pFile->nDrive, pFile->csName, pFile->RecordNumber, pFile->RecordNumber+pFile->LengthSectors-1, pFile->DataBuffer);

	// verify the file exists and get its current data
	DetectImageType(&lclFile, csFilename);
	if (IMAGE_UNKNOWN == lclFile.ImageType) {
		debug_write("Can't get file type of %s", csFilename);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}
	if ((IMAGE_TIFILES != lclFile.ImageType) && (IMAGE_V9T9 != lclFile.ImageType)) {
		debug_write("Only TIFILES or V9T9 supported for sector write on %s", csFilename);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// Write the requested sectors to the file (r+ is read and write of existing file)
	fp=fopen(csFilename, "r+b");
	if (NULL == fp) {
		debug_write("Can't write to %s, errno %d", csFilename, errno);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// This seek is good even past EOF
	fseek(fp, pFile->RecordNumber*256+HEADERSIZE, SEEK_SET);

	// Write the sectors
	pFile->LengthSectors = fwrite(&VDP[pFile->DataBuffer], 256, pFile->LengthSectors, fp);

	if (pFile->LengthSectors == 0) {
		debug_write("Failed to write any data to the file!");
		pFile->LastError = ERR_FILEERROR;
		fclose(fp);
		return false;
	}

	// Use this to fix up the header (file size) with actual information
	// just length in sectors, the rest should be correct
	fseek(fp, 0, SEEK_END);
	lclFile.LengthSectors = (ftell(fp) - HEADERSIZE + 255) / 256;
	WriteFileHeader(&lclFile, fp);

	// all done
	fclose(fp);
	return true;
}

// return two letters indicating DISPLAY or INTERNAL,
// and VARIABLE or FIXED. Static buffer, not thread safe
const char* FiadDisk::GetAttributes(int nType) {
	// this is a hacky way to allow up to 3 calls on a single line ;) not thread-safe though!
	static char szBuf[3][3];
	static int cnt=0;

	if (++cnt == 3) cnt=0;

	szBuf[cnt][0]=(nType&TIFILES_INTERNAL) ? 'I' : 'D';
	szBuf[cnt][1]=(nType&TIFILES_VARIABLE) ? 'V' : 'F';
	szBuf[cnt][2]='\0';

	return szBuf[cnt];
}
