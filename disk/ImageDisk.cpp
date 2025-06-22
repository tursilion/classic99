// TODO: check that all mallocs() in this file are freed()
// TODO: check creation of save files don't overflow their buffers

// I hate disk images. :)

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

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <atlstr.h>
#include <time.h>
#include <errno.h>
#include "..\console\tiemul.h"
#include "diskclass.h"
#include "imagedisk.h"

// largest disk size
#define MAX_SECTORS 1600

//********************************************************
// ImageDisk
//********************************************************

// TODO: does not support writing files one sector at a time - though probably not too hard to add

// constructor
ImageDisk::ImageDisk() {
	bUseV9T9DSSD = false;
}

ImageDisk::~ImageDisk() {
}

// powerup routine
void ImageDisk::Startup() {
	BaseDisk::Startup();
}

// handle options
void ImageDisk::SetOption(int nOption, int nValue) {
	switch (nOption) {
		case OPT_IMAGE_USEV9T9DSSD:	
			bUseV9T9DSSD = nValue?true:false;
			break;

		default:
			BaseDisk::SetOption(nOption, nValue);
			break;
	}
}

bool ImageDisk::GetOption(int nOption, int &nValue) {
	switch (nOption) {
		case OPT_IMAGE_USEV9T9DSSD:	
			nValue = bUseV9T9DSSD;
			break;

		default:
			return BaseDisk::GetOption(nOption, nValue);
	}

	return true;
}

// return a local path name on the PC disk
CString ImageDisk::BuildFilename(FileInfo *pFile) {
	if (NULL == pFile) {
		return "";
	}

	// return just the disk image path
	return pDriveType[pFile->nDrive]->GetPath();
}

// verify that a disk image is formatted in a way we can manage
bool ImageDisk::VerifyFormat(FILE *fp, bool &bIsPC99, int &Gap1, int &PreIDGap, int &PreDatGap, int &SLength, int &SekTrack, int &TrkLen) {
     unsigned char buf[256];

    if (NULL == fp) {
        debug_write("File not open to be verified.");
        return false;
    }

	// PC99 detection routines adapted from code by Paolo Bagnaresi! Thanks for your research, Paolo!
	// Need to add flags to disable PC99 detection (support weird disks)
	if (fseek(fp, 0, SEEK_SET)) {
        debug_write("Disk seek failed, code %d", errno);
		return false;
	}
	if (256 != fread(buf, 1, 256, fp)) {		// read the first block of the file
        debug_write("Can't read first sector of disk, corrupt file? Errno %d\n", errno);
		return false;
	}

	bIsPC99 = (memcmp("DSK", &buf[13], 3) != 0);	// if we find DSK, then it's not PC99 for sure, if not, it probably is (or a weird disk format)
    if (!bIsPC99) {
        if (!detected) debug_write("Found DSK marker, treating as sector-based image.");
        detected = true;
        return true;
    } else {
		// data to find the sector - kind of bad that we do this every time we access
		// a sector. TODO?
        if (!detected) debug_write("No DSK marker, assuming PC99 track-based image...");
        detected = true;

		// find the beginning of the sector by looking for 0xfe
		int idx=0;
		bool bDoubleDensity;

		while ((idx<256)&&(buf[idx]!=0xfe)) idx++;
		if ((idx >= 256) || (buf[idx] != 0xfe)) {
			debug_write("Can't find PC99 start of sector indicator - corrupt dsk?");
			return false;
		}
		if (buf[idx+4] != 0x01) {
			debug_write("Can't find PC99 start of sector indicator - corrupt dsk!");
			return false;
		}
		if (memcmp("\xa1\xa1\xa1", &buf[idx-3], 3) == 0) {
			bDoubleDensity = true;
			if (idx != 53) {
				debug_write("Unknown track data size on PC99 disk. Corrupt dsk?");
				return false;
			}
			Gap1 = 40;
			PreIDGap = 14;
			PreDatGap = 58;
			SLength = 340;
			SekTrack = 18;
			TrkLen = 6872;
		} else {
			if (memcmp("\0\0\0", &buf[idx-3], 3) != 0) {
				debug_write("Can't identify PC99 density marker. Corrupt dsk?");
				return false;
			}
			bDoubleDensity = false;
			if (idx != 22) {
				debug_write("Unknown track data size on PC99 disk. Corrupt dsk?");
				return false;
			}
			Gap1 = 16;
			PreIDGap = 7;
			PreDatGap = 31;
			SLength = 334;
			SekTrack = 9;
			TrkLen = 3253;
		}

        return true;
    }

    // shouldn't get here...
    debug_write("Detection failed - disk type unknown.");
    return false;
}


// Read a sector from an open file - true on success, false
// if an error occurs. buf must be at least 256 bytes!
bool ImageDisk::GetSectorFromDisk(FILE *fp, int nSector, unsigned char *buf) {
	if (NULL == fp) return false;
	if (NULL == buf) return false;

    bool bIsPC99;
    int Gap1, PreIDGap, PreDatGap, SLength, SekTrack, TrkLen;

    if (!VerifyFormat(fp, bIsPC99, Gap1, PreIDGap, PreDatGap, SLength, SekTrack, TrkLen)) {
        return false;
    }

	if (bIsPC99) {
		// now find the sector in the file - convert to side/track/sector
		int Trk = nSector / SekTrack;		// which track is it on?
		int DskSide = 0;
		if (Trk > 39) {
			Trk = 79-Trk;
			DskSide = 1;
		}
		nSector %= SekTrack;
		int nOffset = (TrkLen*40)*DskSide + Trk*TrkLen;		// assumes 40 tracks
		
		// read in the entire track, since we don't know the sector order - caching would be useful here
		// we probably should just cache the whole disk image in both cases?? If we cache the disk image,
		// then we can just read sectors in both cases. But writing becomes a tricky operation, especially
		// if we do add Omniflop support someday. Probably should just live with it?
		unsigned char *pTrk = (unsigned char*)malloc(TrkLen);
		if (fseek(fp, nOffset, SEEK_SET)) {
			debug_write("Seek failed for PC99 disk, code %d", errno);
			free(pTrk);
			return false;
		}
		if (TrkLen != fread(pTrk, 1, TrkLen, fp)) {
			debug_write("Read failed for PC99 disk, code %d", errno);
			free(pTrk);
			return false;
		}
		for (int tst=0; tst<SekTrack; tst++) {
			int p = Gap1 + PreIDGap + (SLength * tst);
			if ((pTrk[p] == Trk) && (pTrk[p+1] == DskSide) && (pTrk[p+2] == nSector) && (pTrk[p+3] == 1)) {
				memcpy(buf, &pTrk[Gap1 + PreDatGap + (SLength * tst)], 256);
				free(pTrk);
				return true;
			}
		}
		debug_write("PC99 disk can't find side %d, track %d, sector %d. Corrupt dsk?", DskSide, Trk, nSector);
		return false;
	} else {
		// V9T9 disk
		// Note: V9T9 reversed the order of side 2 of DSSD disks, so if the sector
		// range is from 360-719, we have to reverse it to get the right offset
		// (only if the option is set!) OR IS THIS A RUMOR?? I've never seen one!
		// TODO: this option removed from dialog 10/21/2021, remove fully eventually
		if ((bUseV9T9DSSD) && (nSector >= 360) && (nSector <= 719)) {
			debug_write("Note: using swapped V9T9 order for sector %d", nSector);
			nSector = (719-nSector) + 360;
		}

		if (fseek(fp, nSector*256, SEEK_SET)) {
            debug_write("Sector seek to %d for read failed, errno %d", nSector, errno);
			return false;
		}

		if (256 != fread(buf, 1, 256, fp)) {
            debug_write("Read sector %d failed, errno %d", nSector, errno);
			return false;
		}

		return true;
	}
}

// Write a sector to an open file - true on success, false
// Note that the file must be open for read AND write!
// if an error occurs. buf must be at least 256 bytes!
// The image on disk is updated immediately and irrevocably!
bool ImageDisk::PutSectorToDisk(FILE *fp, int nSector, unsigned char *wrbuf) {
	unsigned char buf[256];		// work buffer
	if (NULL == fp) return false;
	if (NULL == buf) return false;

    bool bIsPC99;
    int Gap1, PreIDGap, PreDatGap, SLength, SekTrack, TrkLen;

    if (!VerifyFormat(fp, bIsPC99, Gap1, PreIDGap, PreDatGap, SLength, SekTrack, TrkLen)) {
        return false;
    }

	if (bIsPC99) {
		// now find the sector in the file - convert to side/track/sector
		int Trk = nSector / SekTrack;		// which track is it on?
		int DskSide = 0;
		if (Trk > 39) {
			Trk = 79-Trk;
			DskSide = 1;
		}
		nSector %= SekTrack;
		int nOffset = (TrkLen*40)*DskSide + Trk*TrkLen;		// assumes 40 tracks
		
		// read in the entire track, since we don't know the sector order - caching would be useful here
		// we probably should just cache the whole disk image in both cases?? If we cache the disk image,
		// then we can just read sectors in both cases. But writing becomes a tricky operation, especially
		// if we do add Omniflop support someday. Probably should just live with it?
		// Note that by cache, I mean like files are cached, by just reading the sectors into a buffer
		// so that we know where each one is exactly. But this is probably a bad idea -- it makes my life
		// easier, but there is high risk of the user changes the disk image of overwriting an entire disk
		// just because a file was changed. Mind you, for the sake of file updates, maybe it makes sense
		// to cache the disk while updating the bitmap just to reduce writes? Hell, I dunno. Too many
		// tradeoffs to balance.
		unsigned char *pTrk = (unsigned char*)malloc(TrkLen);
		if (fseek(fp, nOffset, SEEK_SET)) {
			debug_write("WSeek failed for PC99 disk. Code %d", errno);
			free(pTrk);
			return false;
		}
		if (TrkLen != fread(pTrk, 1, TrkLen, fp)) {
			debug_write("WRead failed for PC99 disk, code %d", errno);
			free(pTrk);
			return false;
		}
		for (int tst=0; tst<SekTrack; tst++) {
			int p = Gap1 + PreIDGap + (SLength * tst);
			if ((pTrk[p] == Trk) && (pTrk[p+1] == DskSide) && (pTrk[p+2] == nSector) && (pTrk[p+3] == 1)) {
				// Just write back the one sector, no need to mess with the whole track
				if (fseek(fp, nOffset + Gap1 + PreDatGap + (SLength * tst), SEEK_SET)) {
					debug_write("WSeek failed for PC99 disk");
					free(pTrk);
					return false;
				}
				if (256 != fwrite(wrbuf, 1, 256, fp)) {
					debug_write("WFailed to write entire sector - corruption may have occurred.");
					free(pTrk);
					return false;
				}

				free(pTrk);
				return true;
			}
		}
		debug_write("WPC99 disk can't find side %d, track %d, sector %d. Corrupt dsk?", DskSide, Trk, nSector);
		return false;
	} else {
		// V9T9 disk
		// Note: V9T9 reversed the order of side 2 of DSSD disks, so if the sector
		// range is from 360-719, we have to reverse it to get the right offset
		// (only if the option is set!) OR IS THIS A RUMOR?? I've never seen one!
		// TODO: this option removed from dialog 10/21/2021, remove fully eventually
		if ((bUseV9T9DSSD) && (nSector >= 360) && (nSector <= 719)) {
			debug_write("WNote: using swapped V9T9 order for sector %d", nSector);
			nSector = (719-nSector) + 360;
		}

		if (fseek(fp, nSector*256, SEEK_SET)) {
            debug_write("Seek to sector %d failed for write, errno %d", nSector, errno);
			return false;
		}

		if (256 != fwrite(wrbuf, 1, 256, fp)) {
            debug_write("Write sector %d failed, errno %d", nSector, errno);
			return false;
		}

		return true;
	}
}

// try to locate the FDR for a file on an open disk image
// return true if found, the sector index is filled into pFile->nLocalData and the data stored in fdr (must be 256 bytes!)
// false if not found, fdr buffer is overwritten
bool ImageDisk::FindFileFDR(FILE *fp, FileInfo *pFile, unsigned char *fdr) {
	unsigned char sector1[256];	// work buffer
	if (!GetSectorFromDisk(fp, 1, sector1)) {
		fclose(fp);
		debug_write("Can't read sector 1 from %s, errno %d", (LPCSTR)BuildFilename(pFile), errno);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// loop until no more files, a disk error, or we find it
	// because the TI disk controller performed a binary search starting in the middle,
	// some disks performed basic copy protection by blanking the first entry, thus they
	// would have an empty catalog but still find files. I'll just search the whole
	// damn sector for now until someone finds a case with duplicate files on the
	// disk that relies on the disk controller's precise search order. Or till I do
	// it myself. ;)
	CString csSearch = pFile->csName + "          ";
	csSearch.Truncate(10);
	for (int i=0; i<128; i++) {
		int nSect = sector1[i*2]*256 + sector1[i*2+1];
		if (nSect == 0) {
			// check the next one, maybe it's protected
			continue;
		}

		if (!GetSectorFromDisk(fp, nSect, fdr)) {
			fclose(fp);
			debug_write("Can't read sector %d (file %d) from %s, errno %d", nSect, i, (LPCSTR)BuildFilename(pFile), errno);
			pFile->LastError = ERR_DEVICEERROR;
			// We COULD continue anyway, but a real TI controller would barf on this too.
			// but what if it was a copy protected disk that relied on finding the
			// valid file before the bad sector?
			return false;
		}

		// got a supposed FDR, try to match the filename
		if (memcmp(fdr, (LPCSTR)csSearch, csSearch.GetLength()) == 0) {
			// it's a match!
			pFile->nLocalData = nSect;		// save off the FDR location
			return true;
		}
	}

	// file not found
	fclose(fp);
	debug_write("Can't find file %s on %s.", pFile->csName, (LPCSTR)BuildFilename(pFile));
	pFile->LastError = ERR_FILEERROR;
	return false;
}

// copy file information from an FDR buffer (must be 256 bytes)
// into the FileInfo structure
void ImageDisk::CopyFDRToFileInfo(unsigned char *buf, FileInfo *pFile) {
	pFile->ImageType = IMAGE_SECTORDSK;
	// fill in the information 
	pFile->LengthSectors=(buf[0x0e]<<8)|buf[0x0f];
	pFile->FileType=buf[0x0c];
	pFile->RecordsPerSector=buf[0x0d];
	pFile->BytesInLastSector=buf[0x10];
	pFile->RecordLength=buf[0x11];
	pFile->NumberRecords=(buf[0x13]<<8)|buf[0x12];		// NOTE: swapped on disk!
	// translate FileType to Status
	pFile->Status = 0;
	if (pFile->FileType & TIFILES_VARIABLE) pFile->Status|=FLAG_VARIABLE;
	if (pFile->FileType & TIFILES_INTERNAL) pFile->Status|=FLAG_INTERNAL;
}

// Open an existing file, check the header against the parameters
bool ImageDisk::TryOpenFile(FileInfo *pFile) {
	char *pMode;
	unsigned char fdr[256];	// work buffer

	CString csFileName = BuildFilename(pFile);
	int nMode = pFile->Status & FLAG_MODEMASK;	// should be UPDATE, APPEND or INPUT
	FILE *fp=NULL;
	FileInfo lclInfo;

	// output doesn't come through this function, that's CreateOutputFile
	switch (nMode) {
		case FLAG_UPDATE:
			pMode="update";
			fp=fopen(csFileName, "r+b");
			break;

		case FLAG_APPEND:
			pMode="append";
			fp=fopen(csFileName, "r+b");
			break;

		case FLAG_INPUT:
			pMode="input";
			fp=fopen(csFileName, "rb");
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

	// we got the disk, now we need to find the file.
	if (!FindFileFDR(fp, pFile, fdr)) {
    	fclose(fp);
		return false;
	}
	fclose(fp);

	// now we have a file, and an FDR, so fill in the data
	// there should be no data to copy as we are just opening the file
	lclInfo.CopyFileInfo(pFile, false);
	CopyFDRToFileInfo(fdr, &lclInfo);

	if (lclInfo.ImageType == IMAGE_UNKNOWN) {
		debug_write("%s is an unknown file type - can not open.", (LPCSTR)lclInfo.csName);
		pFile->LastError = ERR_BADATTRIBUTE;
		return false;
	}

	// only verify on open (we reuse this function for sector reads)
	if ((pFile->OpCode == OP_OPEN) || (pFile->OpCode == OP_LOAD)) {
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
	}

	// seems okay? Copy the data over from the PAB
	pFile->CopyFileInfo(&lclInfo, false);
	return true;
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
bool ImageDisk::BufferFile(FileInfo *pFile) {
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

		case IMAGE_SECTORDSK:
			return BufferSectorFile(pFile);

		default:
			debug_write("Failed to buffer undetermined file type %d for %s", pFile->ImageType, (LPCSTR)pFile->csName);
	}
	return false;
}

// parse the cluster list in an FDR (256 byte sector)
// Returns a malloc'd list of ints, ending with 0 (user frees)
int *ImageDisk::ParseClusterList(unsigned char *fdr) {
	int *pList = (int*)malloc(sizeof(int)*MAX_SECTORS+1);
	int nListPos = 0;
	int nOff = 0x1c;
	int nFilePos = 0;

    debug_write("Cluster List:");

	while (nOff < 0x100) {
		int um, sn, of, num, ofs;

		um = fdr[nOff]&0xff;
		sn = fdr[nOff+1]&0xff;
		of = fdr[nOff+2]&0xff;
		nOff+=3;

		num = ((sn&0x0f)<<8) | um;
		ofs = (of<<4) | ((sn&0xf0)>>4);

		if (num == 0) {
			break;
		}

        debug_write("  Sector start: 0x%x, file sectors %d-%d", num, nFilePos, ofs);

		for (int i=nFilePos; i<=ofs; i++) {
			pList[nListPos++] = num++;
			if (nListPos >= MAX_SECTORS) {
				debug_write("Cluster list exceeds maximum number of sectors, aborting.");
				free(pList);
				return NULL;
			}
		}

		nFilePos = ofs+1;
	}

	pList[nListPos] = 0;

    if (0) {
        // todo debug
        char str[1024];
        str[0]=0;
        for (int i=0; i<nListPos; ++i) {
            snprintf(str, sizeof(str), "%s%x,",str,pList[i]);
            str[sizeof(str)-4]='.';
            str[sizeof(str)-3]='.';
            str[sizeof(str)-2]='.';
            str[sizeof(str)-1]='\0';
        }
        debug_write(str);
    }
	return pList;
}

// Buffer a sector disk style file - pFile->nLocalData
// contains the index of the FDR record. This will
// read somewhat similar to how Fiad files read, actually.
bool ImageDisk::BufferSectorFile(FileInfo *pFile) {
	int idx, nSector;
	unsigned char *pData;
	unsigned char tmpbuf[256];
	int *pSectorList = NULL;
	int nSectorPos = 0;

	// Fixed records are obvious. Variable length records are prefixed
	// with a byte that indicates how many bytes are in this record. It's
	// all padded to 256 byte blocks. If it won't fit, the space in the
	// rest of the sector is wasted (and 0xff marks it)
	// Even better - the NumberRecords field in a variable file is a lie,
	// it's really a sector count. So we need to read them differently,
	// more like a text file.
	CString csFileName=BuildFilename(pFile);
	FILE *fp=fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Failed to open %s", (LPCSTR)csFileName);
		return false;
	}

	// read in the FDR so we can get the cluster list
	if (!GetSectorFromDisk(fp, pFile->nLocalData, tmpbuf)) {
		fclose(fp);
		debug_write("Failed to retrieve FDR at sector %d for %s", pFile->nLocalData, csFileName);
		return false;
	}
	// Get the sector list by parsing the Cluster list (0 terminated)
	// Then we don't need the FDR anymore.
	pSectorList = ParseClusterList(tmpbuf);
	if (NULL == pSectorList) {
		fclose(fp);
		debug_write("Failed to parse cluster list for %s", csFileName);
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
	nSector = 0;					// position in this sector
	nSectorPos = 0;					// position in the sector list
	pData = pFile->pData;

	if (pSectorList[0] == 0) {
		// empty file - we're done
		fclose(fp);
		free(pSectorList);
		debug_write("%s::%s was empty.", (LPCSTR)csFileName, (LPCSTR)pFile->csName);
		pFile->NumberRecords = 0;
		return true;
	}

	if (!GetSectorFromDisk(fp, pSectorList[nSectorPos++], tmpbuf)) {
		fclose(fp);
		debug_write("Failed reading first sector %d in file %s", pSectorList[nSectorPos-1], pFile->csName);
		free(pSectorList);
		return false;
	}

	// we need to let the embedded code decide the terminating rule
	for (;;) {
		if (pFile->Status & FLAG_VARIABLE) {
			// read a variable record
			int nLen=tmpbuf[nSector++];
			if (nLen==0xff) {
				// end of sector indicator, no record read, skip rest of sector
				nSector=0;
				pFile->NumberRecords--;
				// are we done?
				if (pFile->NumberRecords == 0) {
					// yes we are, get the true count
					pFile->NumberRecords = idx;
					break;
				}
				// otherwise, read in the next sector in the list
				if (pSectorList[nSectorPos] == 0) {
					debug_write("Read past EOF - truncating read at index %d.", idx);
					pFile->NumberRecords = idx;
					break;
				}
				if (!GetSectorFromDisk(fp, pSectorList[nSectorPos++], tmpbuf)) {
					debug_write("Failed reading sector %d in file %s - truncating", pSectorList[nSectorPos-1], pFile->csName);
					pFile->NumberRecords = idx;
					break;
				}
			} else {
				// check for buffer resize
				if ((pFile->pData+pFile->nDataSize) - pData < (pFile->RecordLength+2)*10) {
					int nOffset = pData - pFile->pData;		// in case the buffer moves
					// time to grow the buffer - add another 100 lines
					pFile->nDataSize += (100) * (pFile->RecordLength + 2);
					unsigned char *pTmp  = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
                    if (NULL == pTmp) {
                        debug_write("BufferSector couldn't realloc memory for buffer, failing.");
                        pFile->LastError = ERR_FILEERROR;
                        return false;
                    }
                    pFile->pData = pTmp;
					pData = pFile->pData + nOffset;
				}
				
				// clear buffer
				memset(pData, 0, pFile->RecordLength+2);

				// check again
				if (256-nSector < nLen) {
					debug_write("Corrupted file - truncating read at record %d.", idx);
					pFile->NumberRecords = idx;
					break;
				}

				// we got some data, read it in and count off the record
				// verify it (don't get screwed up by a bad file)
				if (nLen > pFile->RecordLength) {
					debug_write("Potentially corrupt file - skipping end of record %d.", idx);
					
					// store length data
					*(unsigned short*)pData = pFile->RecordLength;
					pData+=2;

					memcpy(pData, &tmpbuf[nSector], pFile->RecordLength);
					nSector+=nLen;
					// trim down nLen
					nLen = pFile->RecordLength;
				} else {
					// record is okay (normal case)
					
					// write length data
					*(unsigned short*)pData = nLen;
					pData+=2;

					memcpy(pData, &tmpbuf[nSector], nLen);
					nSector+=nLen;
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
			if (256-nSector < pFile->RecordLength) {
				// not enough room for another record, skip to the next sector
				if (pSectorList[nSectorPos] == 0) {
					debug_write("Read past EOF - truncating read.");
					pFile->NumberRecords = idx;
					break;
				}
				if (!GetSectorFromDisk(fp, pSectorList[nSectorPos++], tmpbuf)) {
					debug_write("Failed reading sector %d in file %s - truncating", pSectorList[nSectorPos-1], pFile->csName);
					pFile->NumberRecords = idx;
					break;
				}
				nSector=0;
			} else {
				// a little simpler, we just need to read the data
				*(unsigned short*)pData = pFile->RecordLength;
				pData+=2;

				memcpy(pData, &tmpbuf[nSector], pFile->RecordLength);
				nSector += pFile->RecordLength;
				idx++;
				pData += pFile->RecordLength;
			}
		}
	}

	fclose(fp);
	free(pSectorList);
	debug_write("%s::%s read %d records", (LPCSTR)csFileName, (LPCSTR)pFile->csName, pFile->NumberRecords);
	return true;
}

// retrieve the disk name from a disk image - relatively
// straight forward, it's the first 10 characters of the disk in sector 0
CString ImageDisk::GetDiskName() {
	unsigned char buf[256];
	CString csDiskName;

	CString csFileName=GetPath();
	FILE *fp=fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Failed to open %s", (LPCSTR)csFileName);
		return BAD_DISK_NAME;
	}

	// read in sector 0
	if (!GetSectorFromDisk(fp, 0, buf)) {
		fclose(fp);
		debug_write("Failed to retrieve sector 0 for %s", csFileName);
		return BAD_DISK_NAME;
	}

    // close the disk image ;)
    fclose(fp);

	// and parse out the diskname
	for (int idx=0; idx<10; idx++) {
		if (buf[idx] == ' ') break;
		csDiskName+=buf[idx];
	}

	return csDiskName;
}

// Open a file with a particular mode, creating it if necessary
FileInfo *ImageDisk::Open(FileInfo *pFile) {
	FileInfo *pNewFile=NULL;
	CString csTmp;

	if (pFile->bOpen) {
		// trying to open a file that is already open! Can't allow that!
		pFile->LastError = ERR_FILEERROR;
		return NULL;
	}

    // allow debug for the first detection
    detected = false;

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

	// Check for directory (empty filename)
	if (pFile->csName == ".") {
		char *pData, *pStart;

		// read directory!
		if ((pFile->Status & FLAG_MODEMASK) != FLAG_INPUT) {
			debug_write("Can't open directory for anything but input");
			pFile->LastError = ERR_ILLEGALOPERATION;
			goto error;
		}
		// check for 0, map to actual size (38)
		if (pFile->RecordLength == 0) {
			pFile->RecordLength = 38;
		}
		// internal fixed 38
		if (((pFile->Status & FLAG_TYPEMASK) != FLAG_INTERNAL) || (pFile->RecordLength != 38)) {
			debug_write("Must open directory as IF38");
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
		// we add one record for the terminator
		pFile->nDataSize = (pFile->NumberRecords+1) * (pFile->RecordLength + 2);
		pFile->pData = (unsigned char*)malloc(pFile->nDataSize);
		pData = (char*)pFile->pData;

		// Record 0 contains:
		// - Diskname (an ascii string of upto 10 chars).
		// - The number zero.
		// - The number of sectors on disk (minus 2 for the VIB and FDIR) 
		// - The number of free sectors on disk.
		memset(pData, 0, pFile->RecordLength+2);

		// DiskName - first 10 bytes - we provide the local path (last 10 chars of it)
		pStart=pData;
		*(unsigned short*)pData = (unsigned short)38;
		pData+=2;
		*(pData++) = Filenames[0].csName.GetLength();
		memcpy(pData, (LPCSTR)Filenames[0].csName, Filenames[0].csName.GetLength());
		pData+=Filenames[0].csName.GetLength();
		pData=WriteAsFloat(pData,0);	// always 0
		pData=WriteAsFloat(pData,Filenames[0].LengthSectors);		// number of sectors on the disk (-2 was already done)
		pData=WriteAsFloat(pData,Filenames[0].BytesInLastSector);	// number of free sectors on the disk

		// now the rest of the entries. 
		// - Filename (an ascii string of upto 10 chars) 
		// - Filetype: 1=D/F, 2=D/V, 3=I/F, 4=I/V, 5=Prog, 0=end of directory.
		//   If the file is protected, this number is negative (-1=D/F, etc).
		// - File size in sectors (including the FDR itself).
		// - File record length (0 for programs).
		for (int idx=1; idx<pFile->NumberRecords; idx++) {
			pData = pStart + pFile->RecordLength+2;
			pStart = pData;

			memset(pData, 0, pFile->RecordLength+2);
			csTmp = Filenames[idx].csName;
			csTmp = csTmp.Left(10);
			*(unsigned short*)pData = (unsigned short)38;
			pData+=2;
			*(pData++) = csTmp.GetLength();
			memcpy(pData, (LPCSTR)csTmp, csTmp.GetLength());
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
			pData=WriteAsFloat(pData, nType);
			pData=WriteAsFloat(pData, Filenames[idx].LengthSectors);
			pData=WriteAsFloat(pData, Filenames[idx].RecordLength);
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
                        // don't try to buffer the new file, it's empty (see OUTPUT)
                        break;
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
					// in this special case, I want to return pFile
					// so that STATUS can get information even if
					// it had mismatched attributes, like PROGRAM
					pNewFile->CopyFileInfo(pFile, false);
					Close(pNewFile);
					return pNewFile;
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
bool ImageDisk::Load(FileInfo *pFile) {
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

	// XB first tries to load as a PROGRAM image file with the
	// maximum available VDP RAM. If that fails with code 0x60,
	// it tries again as the proper DIS/FIX254
	// I am leaving this comment here, but hacks might not be
	// needed anymore now that the headers are properly tested.

	// It's good, so we try to open it now
	fp=fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Late failure opening %s", (LPCSTR)csFileName);	
		return false;
	}
	
	// we need to get the disk sector list
	unsigned char fdr[256];
	if (!GetSectorFromDisk(fp, pFile->nLocalData, fdr)) {
		fclose(fp);
		debug_write("Late failure reading FDR at sector %d on %s", pFile->nLocalData, (LPCSTR)csFileName);	
		return false;
	}
	int *pSectors = ParseClusterList(fdr);
	if (NULL == pSectors) {
		fclose(fp);
		debug_write("Late failure parsing cluster list in FDR at sector %d on %s", pFile->nLocalData, (LPCSTR)csFileName);	
		return false;
	}

	// now we run through the sectors and load the data up to the maximum requested,
	// or the end of the file, whichever comes first. Only the last sector needs to
	// honor the BytesInLastSector field.
	read_bytes = 0;
	int VDPOffset = pFile->DataBuffer;
	int nBytesLeft = pFile->RecordNumber;
	for (int i=0; i < pFile->LengthSectors; i++) {
		unsigned char tmpbuf[256];
		int nToRead;

		int pos = pSectors[i];
		if (pos == 0) {
			break;
		}
		if (!GetSectorFromDisk(fp, pos, tmpbuf)) {
			debug_write("Error reading sector %d for %s - truncating.", pos, (LPCSTR)csFileName);
			break;
		}
		if (i == pFile->LengthSectors-1) {
			// last sector
			nToRead = pFile->BytesInLastSector;
			if (nToRead == 0) {
				nToRead = 256;
			}
		} else {
			nToRead = 256;
		}
		if (nToRead > nBytesLeft) {
			nToRead = nBytesLeft;
		}
		memcpy(&VDP[VDPOffset], tmpbuf, nToRead);

		// update heatmap
		for (int idx=0; idx<nToRead; idx++) {
			UpdateHeatVDP(VDPOffset+idx);
		}

		read_bytes+=nToRead;
		nBytesLeft-=nToRead;
		VDPOffset+=nToRead;

		if (nBytesLeft < 1) {
			break;
		}
	}

	free(pSectors);
	debug_write("loaded 0x%X bytes", read_bytes);	// do we need to give this value to the user?
	fclose(fp);										// all done

	// handle DSK1 automapping (AutomapDSK checks whether it's enabled)
	AutomapDSK(&VDP[pFile->DataBuffer], pFile->RecordNumber, pFile->nDrive, false);

	return true;
}

// SBRLNK opcodes (files is handled by shared handler)

// Get a list of the files in the current directory (up to 127)
// returns the count of files, and an allocated array at Filenames
// The array is a list of FileInfo objects. Note that unlike the
// FIAD version, this one creates record 0 as well from the disk
// header. (Thus returns up to 128 records)
// Caller must free Filenames!
int ImageDisk::GetDirectory(FileInfo *pFile, FileInfo *&Filenames) {
	int n;
	unsigned char sector1[256];
	CString csFilename = BuildFilename(pFile);
	
	FILE *fp = fopen(csFilename, "rb");
	if (NULL == fp) {
		debug_write("Could not open disk %s", csFilename);
		return 0;
	}

	if (!GetSectorFromDisk(fp, 0, sector1)) {
		debug_write("Could not read sector 0 from %s", csFilename);
		fclose(fp);
		return 0;
	}

	Filenames = (FileInfo*)malloc(sizeof(FileInfo) * 128);
	if (NULL == Filenames) {
		debug_write("Couldn't malloc memory for directory.");
		fclose(fp);
		return 0;
	}
	for (int idx=0; idx<128; idx++) {
		new(&Filenames[idx]) FileInfo;
	}

	// Record 0 contains:
	// - Diskname (an ascii string of upto 10 chars).
	// - The number zero.
	// - The number of sectors on disk.
	// - The number of free sectors on disk.
	// A little hacky, but this will work - 0x14 is unused
	sector1[0x14]='\0';
	Filenames[0].csName = sector1;
	Filenames[0].csName.Truncate(10);
	Filenames[0].LengthSectors = sector1[0x0a]*256+sector1[0x0b]-2;	// sectors total, minus 2 (confirmed TICC does this)
	Filenames[0].BytesInLastSector = Filenames[0].LengthSectors+2;	// sectors free (after we subtract the bitmap below - bitmap includes the two reserved, so add it here)
	for (int i=0; i<(Filenames[0].LengthSectors+7)/8; i++) {
		// calculate free sectors from the sector bitmap
		unsigned char x=sector1[0x38+i];
		for (unsigned char j=0x80; j>0; j>>=1) {
			if (x&j) Filenames[0].BytesInLastSector--;
		}
	}

	if (!GetSectorFromDisk(fp, 1, sector1)) {
		debug_write("Could not read sector 1 from %s", csFilename);
		fclose(fp);
		return 1;
	}

	n=1;	// start at 1 because 0 is the diskname
	for (;;) {
		unsigned char tmpbuf[256];
		int nSect = sector1[(n-1)*2]*256 + sector1[(n-1)*2+1];		// read FDR entry
		if (nSect == 0) {
			// list finished
			break;
		}
		if (GetSectorFromDisk(fp, nSect, tmpbuf)) {
			// got an FDR - get the filename from it
			tmpbuf[10] = '\0';	// supposed to be anyway, this lets us treat the filename as a string
			int t = 9;
			while ((t>=0) && (tmpbuf[t] == ' ')) {
				tmpbuf[t]='\0';
				--t;
			}
			Filenames[n].csName = tmpbuf;

			// and get the fileinfo data too
			CopyFDRToFileInfo(tmpbuf, &Filenames[n]);
			Filenames[n].LengthSectors++;			// TICC does this too - includes directory sector

			if (IMAGE_UNKNOWN != Filenames[n].ImageType) {
				n++;
				if (n>126) {
					break;
				}
			}
		}
	}

	fclose(fp);
	return n;
}

// ReadSector: nDrive=Drive#, DataBuffer=VDP address, RecordNumber=Sector to read
// Must return the sector number in RecordNumber if no error.
bool ImageDisk::ReadSector(FileInfo *pFile) {
	bool nRet = true;

	// sanity test
	if (pFile->DataBuffer + 256 > 0x4000) {
		debug_write("Attempt to read sector past end of VDP memory, aborting.");
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// Zero buffer first
	memset(&VDP[pFile->DataBuffer], 0, 256);

	// any sector is okay now!
	CString csPath = BuildFilename(pFile);
	FILE *fp = fopen(csPath, "rb");
	if (NULL == fp) {
		debug_write("Can't open %s for sector read.", (LPCSTR)csPath);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}
	if (!GetSectorFromDisk(fp, pFile->RecordNumber, &VDP[pFile->DataBuffer])) {
		debug_write("Can't read sector %d on %s.", pFile->RecordNumber, (LPCSTR)csPath);
		fclose(fp);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// it wants the entire sector, so we wrote it to the appropriate spot
	fclose(fp);
	return true;
}

// ReadSector: nDrive=Drive#, DataBuffer=VDP address, RecordNumber=Sector to read
// Must return the sector number in RecordNumber if no error.
bool ImageDisk::WriteSector(FileInfo *pFile) {
	bool nRet = true;

	// sanity test
	if (pFile->DataBuffer + 256 > 0x4000) {
		debug_write("Attempt to write sector from buffer past end of VDP memory, aborting.");
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// any sector is okay now!
	CString csPath = BuildFilename(pFile);
	FILE *fp = fopen(csPath, "rb+");		// must be able to read and write to update the image
	if (NULL == fp) {
		debug_write("Can't open %s for sector write.", (LPCSTR)csPath);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}
	if (!PutSectorToDisk(fp, pFile->RecordNumber, &VDP[pFile->DataBuffer])) {
		debug_write("Can't write sector %d on %s.", pFile->RecordNumber, (LPCSTR)csPath);
		fclose(fp);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// wrote the sector as requested
	fclose(fp);
	return true;
}

// return two letters indicating DISPLAY or INTERNAL,
// and VARIABLE or FIXED. Static buffer, not thread safe
const char* ImageDisk::GetAttributes(int nType) {
	// this is a hacky way to allow up to 3 calls on a single line ;) not thread-safe though!
	static char szBuf[3][4];
	static int cnt=0;

	if (++cnt == 3) cnt=0;

    if (nType&TIFILES_PROGRAM) {
        szBuf[cnt][0]='P';
        szBuf[cnt][1]='R';
        szBuf[cnt][2]='G';
	    szBuf[cnt][3]='\0';
    } else {
	    szBuf[cnt][0]=(nType&TIFILES_INTERNAL) ? 'I' : 'D';
	    szBuf[cnt][1]=(nType&TIFILES_VARIABLE) ? 'V' : 'F';
	    szBuf[cnt][2]='\0';
    }

	return szBuf[cnt];
}

// Read a file by sectors (file type irrelevant)
// LengthSectors - number of sectors to read
// csName - filename to read
// DataBuffer - address of data in VDP
// RecordNumber - first sector to read
// If 0 sectors spectified, the following are returned by ReadFileSectors, 
// They are the same as in a PAB.
// FileType, RecordsPerSector, BytesInLastSector, RecordLength, NumberRecords
// On return, LengthSectors must contain the actual number of sectors read
// This is just a wrapper for ReadFileSectorsToAddress
bool ImageDisk::ReadFileSectors(FileInfo *pFile) {
	if (pFile->LengthSectors == 0) {
		FileInfo lclFile;
		lclFile.CopyFileInfo(pFile, true);  // not that this is necessarily correct yet
		if (!TryOpenFile(&lclFile)) {
			pFile->LastError = ERR_FILEERROR;
			return false;
		}

		pFile->LengthSectors = lclFile.LengthSectors;
		pFile->FileType = lclFile.FileType;
		pFile->RecordsPerSector = lclFile.RecordsPerSector;
		pFile->BytesInLastSector = lclFile.BytesInLastSector;
		pFile->RecordLength = lclFile.RecordLength;
		pFile->NumberRecords = lclFile.NumberRecords;

		debug_write("Information request on file %s (Type >%02x, %d sectors, Records Per Sector %d, EOF Offset %d, Record Length %d, Number Records %d)", 
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

    // now go do the real work
    return ReadFileSectorsToAddress(pFile, &VDP[pFile->DataBuffer]);
}

// read to an arbitrary address - used by both read and write but does the real work
bool ImageDisk::ReadFileSectorsToAddress(FileInfo *pFile, unsigned char *pAdr) {
	unsigned char tmpbuf[256];

	// we just want to read by sector, so we can reuse some of the buffering code
	CString csFileName=BuildFilename(pFile);
	FILE *fp=fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Failed to open %s for %s", (LPCSTR)csFileName, csFileName);
		return false;
	}

	// read in the FDR so we can get the cluster list
	if (!FindFileFDR(fp, pFile, tmpbuf)) {
		fclose(fp);
		debug_write("Failed to retrieve FDR at sector %d for %s", pFile->nLocalData, csFileName);
		return false;
	}

	// Get the sector list by parsing the Cluster list (0 terminated)
	// Then we don't need the FDR anymore.
	int *pSectorList = ParseClusterList(tmpbuf);
	if (NULL == pSectorList) {
		fclose(fp);
		debug_write("Failed to parse cluster list for %s", csFileName);
		return false;
	}

	// now we just read the desired sectors. We do need to make sure we don't run off the end
	int nLastSector=0;
	while (pSectorList[nLastSector] != 0) nLastSector++;

	for (int idx=pFile->RecordNumber; idx < pFile->RecordNumber + pFile->LengthSectors; idx++) {
		if (idx >= nLastSector) {
			debug_write("Reading out of range... aborting.");
			break;
		}
		GetSectorFromDisk(fp, pSectorList[idx], pAdr);
		pAdr+=256;
	}
	free(pSectorList);
	fclose(fp);

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
// TODO: we delete and rewrite the file, so it may move on a disk, which is not
// how the TI disk controller does it.
bool ImageDisk::WriteFileSectors(FileInfo *pFile) {
	FILE *fp;
    CString csFilename = BuildFilename(pFile);

	if (pFile->LengthSectors == 0) {
		// Create a new, empty file
        // first, delete the old one if it exists
        Delete(pFile);

        // now open the disk image so we can work on it
        fp = fopen(csFilename, "r+b");
	    if (NULL == fp) {
		    // couldn't open the file
		    debug_write("Can't open %s for writing, errno %d", (LPCSTR)csFilename, errno);
		    pFile->LastError = ERR_FILEERROR;
		    return false;
	    }

		if (pFile->RecordNumber != 0) {
			// write out this many sectors
            int bytes = pFile->RecordNumber * 256;
            unsigned char *buf = (unsigned char*)malloc(bytes);
            if (NULL == buf) {
                debug_write("Failed to allocate memory to create new file %s with %d sectors", pFile->csName, pFile->RecordNumber);
                pFile->LastError = ERR_DEVICEERROR;
                fclose(fp);
                return false;
            }
			memset(buf, 0, bytes);
            
            // fill in the status flags - they were not set by the caller and WriteOutFile needs them

	        if (pFile->FileType & TIFILES_VARIABLE) pFile->Status |= FLAG_VARIABLE;
	        if (pFile->FileType & TIFILES_INTERNAL) pFile->Status |= FLAG_INTERNAL; 
            pFile->LengthSectors = pFile->RecordNumber;

            if (!WriteOutFile(pFile, fp, buf, bytes)) {
                // error occurred - WriteOutFile already complained
                free(buf);
                fclose(fp);
                return false;
            }

            free(buf);
		}

		fclose(fp);

		debug_write("Low-level created drive %d file %s (Type >%02x, Records Per Sector %d, EOF Offset %d, Record Length %d, Number Records %d, Record # %d)", 
			pFile->nDrive, pFile->csName, pFile->FileType, pFile->RecordsPerSector, pFile->BytesInLastSector, pFile->RecordLength, pFile->NumberRecords, pFile->RecordNumber);
		
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
    FileInfo lclInfo;
    lclInfo.CopyFileInfo(pFile, true);
    lclInfo.Status &= ~FLAG_MODEMASK;
    lclInfo.Status |= FLAG_UPDATE;  // lie so we can get the information

    debug_write("Attempting to identify existing file drive %d file %s...", pFile->nDrive, pFile->csName);

    if (!TryOpenFile(&lclInfo)) {
        // TODO: what does the TI controller do here?
        debug_write("Drive %d file %s can't find file to update...", pFile->nDrive, pFile->csName);
        pFile->LastError = ERR_FILEERROR;
        return false;
    }

    // use ReadFileSectors to get the file in... we should have a rough idea of how big it is
    // I think LengthSectors is always right...?
    // lclInfo has the current file size, pFile has the new count to write
    // calculate a buffer size
    int size = lclInfo.LengthSectors + (pFile->RecordNumber - lclInfo.LengthSectors) + pFile->LengthSectors;
    if (size < lclInfo.LengthSectors) size = lclInfo.LengthSectors; // covers the case of the file getting smaller
    debug_write("Allocating work buffer of %d sectors (file length %d, record %d, adding %d)", size, lclInfo.LengthSectors, pFile->RecordNumber, pFile->LengthSectors);
    size *= 256;
    unsigned char *workbuf = (unsigned char*)malloc(size);
    if (NULL == workbuf) {
        debug_write("Failed to allocate memory to buffer %d sectors for drive %d file %s", size/256, pFile->nDrive, pFile->csName);
        pFile->LastError = ERR_DEVICEERROR;
        return false;
    }

    debug_write("Attempting to buffer existing file drive %d file %s...", pFile->nDrive, pFile->csName);

    // now we should be able to pull the file into memory...
    lclInfo.RecordNumber = 0;
    if (!ReadFileSectorsToAddress(&lclInfo, workbuf)) {
        debug_write("Can't cache drive %d file %s for sector write.", pFile->nDrive, pFile->csName);
        pFile->LastError = ERR_DEVICEERROR;
        free(workbuf);
        return false;
    }

    // okay, now nuke the file - this way we can re-allocate the sectors with existing code
    // this is the part that's not right for the TI controller
    // TODO: presumably we can make a version of WriteOutFile that just goes through the
    // motions for the first part of the file, then starts adding sectors when it hits
    // the end of what is already there... but not today
    debug_write("Attempting to remove existing file drive %d file %s...", pFile->nDrive, pFile->csName);
    Delete(pFile);

    // pFile->RecordNumber is the first sector to write, pFile->LengthSectors is the number of sectors to write
    int offset = pFile->RecordNumber*256;

    // we should be able to just do this if we did everything above correctly...
    memcpy(&workbuf[offset], &VDP[pFile->DataBuffer], pFile->LengthSectors*256);

    // update the file information
    lclInfo.LengthSectors = size / 256;

    debug_write("Attempting to re-write existing file drive %d file %s...", pFile->nDrive, pFile->csName);

    // now open the disk image so we can work on it
    fp = fopen(csFilename, "r+b");
	if (NULL == fp) {
		// couldn't open the file
		debug_write("Can't open %s for writing, errno %d", (LPCSTR)csFilename, errno);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

    // and write it back out
    if (!WriteOutFile(&lclInfo, fp, workbuf, lclInfo.LengthSectors*256)) {
        // error already emitted
        fclose(fp);
        free(workbuf);
        return false;
    }

	// all done
	fclose(fp);
    free(workbuf);
	return true;
}

// rename should be reasonably safe
// it needs to resort the index!
bool ImageDisk::RenameFile(FileInfo *pFile, const char *szNewFile) {
	unsigned char fdr[256];	// work buffer

    if (strlen(szNewFile) > 10) {
        debug_write("New filename '%s' may not be longer than 10 characters on image disk.", szNewFile);
        pFile->LastError = ERR_BADATTRIBUTE;
        return false;
    }

    if (pFile->bOpen) {
		// trying to open a file that is already open! Can't allow that!
        debug_write("Can't rename an open file! Returning error.");
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	CString csFileName = BuildFilename(pFile);
    FILE *fp=fopen(csFileName, "r+b");
	if (NULL == fp) {
		debug_write("Can't open %s for rename, errno %d.", (LPCSTR)csFileName, errno);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// we got the disk, now we need to find the file.
	if (!FindFileFDR(fp, pFile, fdr)) {
        fclose(fp);
		return false;
	}

    // now in theory we just replace the filename
    for (unsigned int idx=0; idx<10; ++idx) {
        if (idx < strlen(szNewFile)) {
            fdr[idx] = szNewFile[idx];
        } else {
            fdr[idx] = ' ';
        }
    }

    // and write it back out
    if (!PutSectorToDisk(fp, pFile->nLocalData, fdr)) {
        pFile->LastError = ERR_DEVICEERROR;
        fclose(fp);
        return false;
    }

    // update the directory sort in sector 1
    sortDirectory(fp, 0);

	fclose(fp);

	return true;
}

// remove a bit from the disk bitmap
bool ImageDisk::freeSectorFromBitmap(FILE *fp, int nSec) {
	unsigned char sector[256];

	if (!GetSectorFromDisk(fp, 0, sector)) {
		debug_write("Can't read sector 0.");
		return false;
	}

	// bitmap runs from 0x38 to 0xEB. Least significant bit is first.
	int offset = nSec/8 + 0x38;	
	int bit = 1<<(nSec%8);
	// TI Disk Controller stops at 0xEB, but CF card goes to the end
	if (offset > 0xff) {
		debug_write("Bitmap offset for sector %d is larger than legal for TI disk image", nSec);
		return false;
	}

	// now fix the bit
	sector[offset] &= ~bit;

	// write it back and we're done
	if (!PutSectorToDisk(fp, 0, sector)) {
		debug_write("Can't write sector 0.");
		return false;
	}

    debug_write("Freeing sector 0x%03X", nSec);

	return true;
}

// find a free sector, lock it and return it
// lastFileSector indicates the search type:
// < 0 - first file sector, value is file size in sectors. Per
// the TI controller, we will start our search for free sectors at sector 34.
// DO NOT INCLUDE SIZE OF FDR.
// >= 0 - start at the suggested sector and find the next free one
// For FDR, pass 0 to start at beginning of disk. (Matches TI controller).
int ImageDisk::findFreeSector(FILE *fp, int lastFileSector) {
	unsigned char sector[256];
	int offset, bit;
	int nSec, idx;

	if (!GetSectorFromDisk(fp, 0, sector)) {
		debug_write("Can't read sector 0.");
		return -1;
	}

	// bitmap runs from 0x38 to 0xEB. Least significant bit is first.
	// For the CF card, we can run all the way to 0xFF. We use the disk
	// header to tell us what's valid.
	//         sectors/track  tracks/side    number sides
	int nMax = sector[0x0c] * sector[0x11] * sector[0x12];
    if (nMax != sector[0xA]*256 + sector[0xb]) {
        debug_write("Error: corrupt disk sector 0 - disk size %d doesn't match calculated %d\n", sector[0xA]*256 + sector[0xb], nMax);
        return -1;
    }
	if (nMax > 1600) {
		// 1600 sectors is the size of the 400k CF card, which is the biggest I know
		// Bigger than that needs a different bitmask size
		debug_write("Too many sectors, disk format not recognized.");
		return -1;
	}

	if (lastFileSector < 0) {
#if 0
		// I tried searching backwards and allocating file data from the end
		// of the disk -- and this worked pretty well! But, some dumb copy
		// protection methods such as Bubble Plane and my own SSA use the last
		// sector on the disk for hidden information, and so this scheme blows
		// that away if the disk is ever written to (affects Bubble Plane for 
		// high score tables). So, we'll do like the TI controller and just
		// start at sector 34. Boo. ;)
		int lastFree = 0;
		lastFileSector = -lastFileSector;
		for (nSec = nMax-1; nSec > 0; nSec--) {
			offset = nSec/8 + 0x38;
			bit = 1<<(nSec%8);

			if ((sector[offset]&bit)==0) {
				if (lastFree == 0) {
					lastFree = nSec;
				}
				if ((lastFree-nSec+1) >= lastFileSector) {
					// we have a match! Start here.
					debug_write("Found room on disk starting at sector %d", nSec);
					lastFileSector = nSec;
					break;
				}
			} else {
				// nope, not here
				lastFree = 0;
			}
		}
		// if the search failed, search forward. The file may still fit
		// with fragmentation. The real controller checks the file size
		// in advance, but this will work...
		if (nSec == 0) {
			debug_write("No block large enough for file, going to try fragmentation.");
			lastFileSector = 0;
		}
#else
		lastFileSector = 34;
#endif
	}

    // Note that 0 and 1 are special, and not always tagged as used, so skip them
    if (lastFileSector < 2) lastFileSector = 2;

	// now we search forward... we loop around as needed.
	nSec = lastFileSector;
	for (idx = 0; idx < nMax-2; idx++) {    // omit the two protected sectors
		offset = nSec/8 + 0x38;
		bit = 1<<(nSec%8);

		if ((sector[offset]&bit)==0) break;

		// next sector with wraparound
		++nSec;
		if (nSec >= nMax) nSec = 2;     // dont' wrap around to protected sectors
	}
	if (idx >= nMax-2) {
		debug_write("Disk is full.");
		return -1;
	}

	// now fix the bit
	sector[offset] |= bit;

	// write it back and we're done
	if (!PutSectorToDisk(fp, 0, sector)) {
		debug_write("Can't write sector 0.");
		return -1;
	}

    debug_write("Allocating sector 0x%03X", nSec);

	return nSec;
}

// delete a file (I mostly use this cause I need it to make saves easier)
bool ImageDisk::Delete(FileInfo *pFile) { 
	// So far so good -- let's try to actually find the file
	unsigned char fdr[256];	// work buffer
	unsigned char sector[256];

    if (pFile->bOpen) {
		// trying to open a file that is already open! Can't allow that!
        debug_write("Can't delete an open file! Returning error.");
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

    CString csFileName = BuildFilename(pFile);
	FILE *fp=NULL;
	int *pSectorList = NULL, *pTmp;
	FileInfo lclInfo;

	// output doesn't come through this function, that's CreateOutputFile
	fp=fopen(csFileName, "r+b");
	if (NULL == fp) {
		debug_write("Can't open %s for delete, errno %d.", (LPCSTR)csFileName, errno);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// we got the disk, now we need to find the file.
	if (!FindFileFDR(fp, pFile, fdr)) {
		debug_write("File %s not found for delete on %s", (LPCSTR)pFile->csName, (LPCSTR)csFileName);
		fclose(fp);
		// TODO: what's the right error code?
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// Get the sector list by parsing the Cluster list (0 terminated)
	// Then we don't need the FDR anymore.
	pSectorList = ParseClusterList(fdr);
	if (NULL == pSectorList) {
		debug_write("Failed to parse cluster list for %s", (LPCSTR)csFileName);
		fclose(fp);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}
	memset(sector, 0xe5, 256);
	pTmp = pSectorList;
	while (*pTmp) {
		if (!freeSectorFromBitmap(fp, *pTmp)) {
			debug_write("Can't update bitmap on %s.", (LPCSTR)csFileName);
			fclose(fp);
			pFile->LastError = ERR_DEVICEERROR;
			free(pSectorList);
			return false;
		}

#if 0
		// Note: real controller doesn't wipe the sectors, but this helps debugging
        // don't do this, better if old data is there to recover
		if (!PutSectorToDisk(fp, *pTmp, sector)) {
			debug_write("Can't write sector %d on %s.", *pTmp, (LPCSTR)csFileName);
		}
#endif
		++pTmp;
	}
	// done with the sector list
	free(pSectorList);
	pSectorList = NULL;

	// now wipe the FDR
	if (!PutSectorToDisk(fp, pFile->nLocalData, sector)) {
		debug_write("Can't write sector %d on %s.", *pTmp, (LPCSTR)csFileName);
		// keep going
	}
	
	// and finally, remove the index - this part matters
	if (!GetSectorFromDisk(fp, 1, sector)) {
		debug_write("Can't read sector 1 on %s.", (LPCSTR)csFileName);
		fclose(fp);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// sift through the list and find the index
	int idx = 0;
	while ((idx<256)&&(sector[idx]*256+sector[idx+1] != 0)) {
		if (sector[idx]*256+sector[idx+1] == pFile->nLocalData) break;
		idx+=2;
	}
	if ((idx>=256)||(sector[idx]*256+sector[idx+1] == 0)) {
		debug_write("Couldn't find FDR in FDR list for %s", (LPCSTR)csFileName);
		fclose(fp);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}
	memmove(&sector[idx], &sector[idx+2], 256-idx-2);
	sector[254]=0;
	sector[255]=0;
	// and write it back
	if (!PutSectorToDisk(fp, 1, sector)) {
		debug_write("Can't write sector 1 on %s.", (LPCSTR)csFileName);
		fclose(fp);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}
    // and free it from the bitmap
    if (!freeSectorFromBitmap(fp, pFile->nLocalData)) {
		debug_write("Can't update bitmap on %s!", (LPCSTR)csFileName);
		fclose(fp);
		pFile->LastError = ERR_DEVICEERROR;
		free(pSectorList);
		return false;
	}

    // just so any write fixes broken disks
    sortDirectory(fp, 0);

	// that's it baby! It's history
	fclose(fp);
	pFile->LastError = ERR_NOERROR;

	return true;
}

// create an output file
// TODO: doesn't check for disk space or anything fun
bool ImageDisk::CreateOutputFile(FileInfo *pFile) {
	FILE *fp;
	unsigned char fdr[256];
	CString csFileName = BuildFilename(pFile);

	// does the disk image exist? We can't write to a non-existing image
	fp = fopen(csFileName, "rb");
	if (NULL == fp) {
		debug_write("Can't open disk image %s", (LPCSTR)csFileName);
		return false;
	}

	// first a little sanity checking -- we never overwrite an existing file
	// unless the mode is 'output', so check existance against the mode
	if (FindFileFDR(fp, pFile, fdr)) {
		// file exists, are we allowed to overwrite?
		if ((pFile->Status & FLAG_MODEMASK) != FLAG_OUTPUT) {
			// no, we are not
			debug_write("Can't overwrite existing file with open mode 0x%02X", (pFile->Status & FLAG_MODEMASK));
            fclose(fp);
			return false;
		}
        // flush will remove any existing file if needed
	}

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
        fclose(fp);
		return false;
	}

    fclose(fp);
	return true;
}

// Save a PROGRAM image file
bool ImageDisk::Save(FileInfo *pFile) {
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
	debug_write("saving 0x%X bytes file %s on %s", pFile->RecordNumber, pFile->csName, csFileName);

	fp=fopen(csFileName, "r+b");
	if (NULL == fp) {
		// couldn't open the file
		debug_write("Can't open %s for writing, errno %d", (LPCSTR)csFileName, errno);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// delete the old file, if it exists
	if (Delete(pFile)) {
		debug_write("Deleted old file %s on %s", pFile->csName, csFileName);
	}

	// We need to fill in some of the FileInfo object here
	pFile->Status = FLAG_OUTPUT;
	pFile->FileType = TIFILES_PROGRAM;
	pFile->LengthSectors = (pFile->RecordNumber+255)/256;
	pFile->RecordsPerSector = 0;
	pFile->BytesInLastSector = pFile->RecordNumber & 0xff;
	pFile->RecordLength = 0;
	pFile->NumberRecords = 0;

	// create an actual block of memory, formatted for the disk
	// For PROGRAM images, that's easy
	int bufsize = ((pFile->RecordNumber+255)/256)*256;
	unsigned char *pBuffer = (unsigned char*)malloc(bufsize);
	memset(pBuffer, 0, bufsize);
	memcpy(pBuffer, VDP+pFile->DataBuffer, pFile->RecordNumber);

	// update heatmap
	for (int idx=0; idx<pFile->RecordNumber; idx++) {
		UpdateHeatVDP(pFile->DataBuffer+idx);
	}

	bool ret = WriteOutFile(pFile, fp, pBuffer, pFile->RecordNumber);
	free(pBuffer);
	fclose(fp);

	if (ret) {
		pFile->LastError = ERR_NOERROR;
	}

	return ret;
}

// Write the disk buffer out to the file, with appropriate modes and headers
bool ImageDisk::Flush(FileInfo *pFile) {
	// first make sure this is an open file!
	if (!pFile->bDirty) {
		// nothing to flush, return success anyway
		return true;
	}

    // allow debug for the first detection
    detected = false;

    // get the disk name
	CString csFileName = BuildFilename(pFile);

    // delete the old file, if it exists
	if (Delete(pFile)) {
		debug_write("Deleted old file %s on %s", pFile->csName, csFileName);
	}

    // start to work on it
	FILE *fp = fopen(csFileName, "r+b");
	if (NULL == fp) {
		debug_write("Unable to write file %s", (LPCSTR)csFileName);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

	// create a buffer for the output file
	// this is bigger than I should need
	unsigned char *pBuffer = (unsigned char *)malloc(1024*1024);
	memset(pBuffer, 0, 1024*1024);
	unsigned char *pOut = pBuffer;

	unsigned char *pData = pFile->pData;
	if (NULL == pData) {
		// not really a big deal when the file is first created
		debug_write("Warning: no data to flush.");
	} else {
		// while going through here, we need to update the header records
		// for LengthSectors, and BytesInLastSector
		int nSector;

		// we're going to write to RAM for now
        if (pFile->NumberRecords == 0) {
            // empty file case - this is correct as the stored values does NOT include the FDR
            pFile->LengthSectors = 0;
        } else {
    		pFile->LengthSectors = 1;		// at least one so far
        }

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
					*(pOut++)=0xff;
					nSector--;
					while (nSector > 0) {
						*(pOut++)=0;
						nSector--;
					}
					nSector=256;
					pFile->LengthSectors++;
				}
				
				// write the length byte
				*(pOut++)=nLen&0xff;
				nSector--;
				// write the data
				memcpy(pOut, pData, nLen);
				pOut+=nLen;
				nSector-=nLen;
			} else {
				// write a fixed length record
				// first, check if it will fit
				if (nSector < pFile->RecordLength) {
					// pad the sector out
					while (nSector > 0) {
						*(pOut++)=0;
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
				memcpy(pOut, pData, nLen);
				pOut+=nLen;
				nSector-=nLen;
			}
			pData+=pFile->RecordLength;
		}

		// is this right? I guess it can't be on a new sector with 0 bytes...
		// this goes BEFORE the >FF byte is written on variable files
		pFile->BytesInLastSector = 0x100-nSector;

        if (pFile->NumberRecords > 0) {
		    if (pFile->Status & FLAG_VARIABLE) {
			    // done writing - if this is variable, then we need to close the sector
			    *(pOut++)=0xff;
			    nSector--;
                pFile->BytesInLastSector++;
		    }
		
		    // don't forget to pad the file to a full sector!
		    while (nSector > 0) {
			    *(pOut++)=0;
			    nSector--;
		    }
        }
	}

	bool ret = WriteOutFile(pFile, fp, pBuffer, pOut - pBuffer);
	free(pBuffer);
	fclose(fp);

	if (ret) {
		pFile->LastError = ERR_NOERROR;
		debug_write("Flushed %s (%d records) to ImageDisk", (LPCSTR)csFileName, pFile->NumberRecords);
		pFile->bDirty = false;
	}

	return ret;
}

// write a single cluster
void ImageDisk::WriteCluster(unsigned char *buf, int clusterOff, int startnum, int ofs) {
    buf[clusterOff++] = startnum&0xff;
	buf[clusterOff++] = ((startnum>>8)&0x0f) | ((ofs&0x0f)<<4);
	buf[clusterOff]   = (ofs>>4)&0xff;
}

// this function actually writes the data back to the disk and updates the headers
bool ImageDisk::WriteOutFile(FileInfo *pFile, FILE *fp, unsigned char *pBuffer, int cnt) {
	unsigned char buf[256];
	int sectorList[1600];
	int sectorCnt = 0;

	// find a slot for the FDR - deliberately start at 0 for the FDR
	int fdr = findFreeSector(fp, 0);
	if (fdr == -1) {
		pFile->LastError = ERR_BUFFERFULL;
		return false;
	}

	// build the FDR so we can write it at the end
	// (needs the cluster map installed)
	memset(buf, 0, 256);

	if (pFile->csName.GetLength() > 10) {
		pFile->csName.Truncate(10);
	}

	memset(buf, ' ', 10);
	memcpy(buf, (LPCSTR)pFile->csName, pFile->csName.GetLength());
	
	buf[0x0c] = pFile->FileType;
	buf[0x0d] = pFile->RecordsPerSector;
	buf[0x0e] = pFile->LengthSectors>>8;
	buf[0x0f] = pFile->LengthSectors&0xff;
	if ((pFile->Status & FLAG_VARIABLE) || (pFile->FileType & TIFILES_PROGRAM)) {
		buf[0x10] = pFile->BytesInLastSector&0xff;
	} else {
        buf[0x10] = 0;  // 0 for fixed
    }
	buf[0x11] = pFile->RecordLength;
    if ((pFile->Status & FLAG_VARIABLE) && ((pFile->FileType & TIFILES_PROGRAM) == 0)) {
        // variable file stores number of sectors again
    	buf[0x12] = pFile->LengthSectors&0xff;	// little endian
	    buf[0x13] = pFile->LengthSectors>>8;
    } else {
        // fixed file stores number records
    	buf[0x12] = pFile->NumberRecords&0xff;	// little endian
	    buf[0x13] = pFile->NumberRecords>>8;
    }

	// all right, it's set up, now write out the file itself
	int lastFileSector = -(pFile->LengthSectors);
	while (cnt > 0) {
		unsigned char tmp[256];
		memcpy(tmp, pBuffer, min(cnt, 256));
		lastFileSector = findFreeSector(fp, lastFileSector);
		if (-1 == lastFileSector) {
			FreePartialFile(fp, fdr, sectorList, sectorCnt);
			pFile->LastError = ERR_BUFFERFULL;
			return false;
		}
		if (!PutSectorToDisk(fp, lastFileSector, tmp)) {
			debug_write("Failed to write data.");
			FreePartialFile(fp, fdr, sectorList, sectorCnt);
			pFile->LastError = ERR_DEVICEERROR;
			return false;
		}
		pBuffer+=256;
		sectorList[sectorCnt++] = lastFileSector;
		if (sectorCnt >= 1599) {
			debug_write("Too many sectors in this file, aborting");
			FreePartialFile(fp, fdr, sectorList, sectorCnt);
			pFile->LastError = ERR_BUFFERFULL;
			return false;
		}
		cnt-=256;
	}

    // now fill in the sector information into the FDR

	// now store the sector list into the FDR
    // todo: probably temp debug delete me too...
    debug_write("Output Sector List:");
    {
        char outbuf[1024];
        outbuf[0]='\0';
        for (int idx=0; idx<sectorCnt; ++idx) {
            snprintf(outbuf, sizeof(outbuf), "%s,%03X", outbuf, sectorList[idx]);
            outbuf[sizeof(outbuf)-4] = '.';
            outbuf[sizeof(outbuf)-3] = '.';
            outbuf[sizeof(outbuf)-2] = '.';
            outbuf[sizeof(outbuf)-1] = '\0';
        }
        debug_write(outbuf);
    }

    if (sectorCnt > 0) {
	    // Each entry has two 12-bit values:
	    // NUM - disk sector number to start at
	    // OFS - ending offset in sectors of the FILE (0-based!)
	    // So a two sector file in two clusters at >10 and >14
	    // Would have NUMs of >10 and >14, and an OFS of 0 and 1.
        // if it wasn't fragmented, it would be NUMS >10 and OFS >01
	    int num = sectorList[0];    // current NUMS
        int startnum = num;         // beginning of cluster
	    int clusterOff = 0x1c;      // position in the FDR for output
        // what if we build the cluster list dynamically?
        // set up the first one
        WriteCluster(buf, clusterOff, startnum, 0);
	    for (int ofs=0; ofs<sectorCnt; ++ofs,++num) {
            if (sectorList[ofs] != num) {
                // new cluster
                clusterOff+=3;
                startnum = sectorList[ofs];

                if (clusterOff >= 0xfd) {
		            debug_write("Cluster list full - defragment disk image!");
		            FreePartialFile(fp, fdr, sectorList, sectorCnt);
		            pFile->LastError = ERR_BUFFERFULL;
		            return false;
	            }
            }

            WriteCluster(buf, clusterOff, startnum, ofs);
        }
        // DEBUG - todo delete me
        int *tmp = ParseClusterList(buf);
        delete tmp;
    }

	// great, so now write it out
	if (!PutSectorToDisk(fp, fdr, buf)) {
		debug_write("Can't write sector %d on %s.", fdr, pFile->csName);
		FreePartialFile(fp, fdr, sectorList, sectorCnt);
		pFile->LastError = ERR_DEVICEERROR;
		return false;
	}

    // resort the directory and add the new entry
    sortDirectory(fp, fdr);

	// I think we're good then!
	pFile->LastError = ERR_NOERROR;
	debug_write("File written successfully.");
	return true;
}

// frees the sector bitmask for the sectors allocated for a failed file, best effort
void ImageDisk::FreePartialFile(FILE *fp, int fdr, int *sectorList, int sectorCnt) {
	// it would be more efficient to do it with one read and one write, but that's okay
	// we do a little sanity checking - no file can start less than sector 1, 0 and 1
	// are reserved for other things.
    
	debug_write("Freeing %d sectors from partially allocated file.", sectorCnt+1);
	if (fdr > 1) {
		freeSectorFromBitmap(fp, fdr);
	}
	if (sectorCnt > 1600) {
		debug_write("Debug check: freeing with a sectorCnt > 1600. Should not happen.");
		sectorCnt = 1600;
	}
	for (int idx=0; idx<sectorCnt; idx++) {
		if (sectorList[idx] > 1) {
			freeSectorFromBitmap(fp, sectorList[idx]);
		}
	}
}

// helper for the qsort below
static int nameCmp(const void *p1, const void *p2)
{
   return strcmp((char*)p1,(char*)p2);
}

// because there are so many broken disk directories out there, some of which
// we may have contributed to, resort the entire directory on a write. This
// should fix the order even if it was broken before we started.
// if newFDR is not 0, add it to the list as a new entry (saves extra writes)
bool ImageDisk::sortDirectory(FILE *fp, int newFDR) {
    unsigned char tmpbuf[256];
    unsigned char tmpbuf2[256];
    unsigned char tmpbuf3[256];
    struct NAMELIST {
        char name[11];
        int fdr;
    } nameList[128];
    int nameCnt = 0;

	// read in the directory index
	if (!GetSectorFromDisk(fp, 1, tmpbuf)) {
		debug_write("Warning: Failed to retrieve directory index from disk image for sorting.");
		return false;
	}

    for (int idx=0; idx<255; idx+=2) {
        int x = tmpbuf[idx]*256 + tmpbuf[idx+1];    // get entry

        // check for new entry at end of list
        if (x == 0) {
            if (newFDR != 0) {
                x = newFDR;
                newFDR = 0; // so we don't detect it again
                idx -= 2;   // so it doesn't move for the next loop
            }
        }
        if (x == 0) break;                          // all done

        nameList[nameCnt].fdr = x;                  // save index
        if (!GetSectorFromDisk(fp, x, tmpbuf2)) {
            debug_write("Warning: Failed to read sector %d for sorting.", x);
            return false;
        }
        // make the filename nul terminate - we don't care about the rest
        tmpbuf2[10] = '\0';
        for (int i2=0; i2<10; i2++) {
            if (tmpbuf2[i2] == ' ') {
                tmpbuf2[i2] = '\0';
                break;
            }
        }
        strcpy(nameList[nameCnt].name, (char*)tmpbuf2);
        ++nameCnt;
    }

    // sort the list
    qsort(nameList, nameCnt, sizeof(nameList[0]), nameCmp);

    // rebuild the directory index
    memset(tmpbuf3, 0, 256);
    int pos = 0;
    for (int idx=0; idx<nameCnt; ++idx) {
        tmpbuf3[pos] = nameList[idx].fdr / 256;
        tmpbuf3[pos+1] = nameList[idx].fdr % 256;
        pos += 2;
    }

    // debug
    if (memcmp(tmpbuf, tmpbuf3, 256)) {
        debug_write("Directory sorted.");
    }

    // and write it back out
	if (!PutSectorToDisk(fp, 1, tmpbuf3)) {
		debug_write("Warning: Failed to retrieve directory index from disk image for sorting.");
		return false;
	}

    // all done
    return true;
}
