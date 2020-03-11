// TODO: add a block of startup code that sets the workspace pointer,
// because I just spent far too long troubleshooting it. It can
// just copy a trampoline to scratchpad with the workspace and a
// jump.. it's only 4 bytes. ;) (Some programs never set it!)

// TODO2: TI BASIC carts only store the boot code in the second page,
// and then assume an inverted order when they jump from trampoline
// back to the code. Since we now default to non-inverted, we need
// to patch the code that calls the trampoline to set the other
// return bank (in R3, I believe).

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
//*****************************************************
//* Classic99 - TI Emulator for Win32				  *
//* by M.Brent                                        *
//* Win32 WindowProc                                  *
//*****************************************************

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#include <stdio.h>
#include <windows.h>
#include <malloc.h>
#include <ddraw.h>
#include <commctrl.h>
#include <commdlg.h>
#include <atlstr.h>
#include <stdexcept>

#include "..\resource.h"
#include "tiemul.h"
#include "cpu9900.h"
#include "ams.h"
#include "..\addons\makecart.h"
#include "..\disk\diskclass.h"
#include "..\disk\FiadDisk.h"

// debugger
extern bool gDisableDebugKeys;
extern Byte CPUMemInited[65536];
extern Byte VDPMemInited[128*1024];
extern CPU9900 * volatile pCurrentCPU;
extern int max_cpf, cfg_cpf;
extern volatile signed long cycles_left;

// direct access to write CPU RAM
void WriteMemoryByte(Word address, Byte value, bool allowWrite);
void EnableDlgItem(HWND hwnd, int id, BOOL bEnable);

// Functions related to saving out cartridges and programs
// These replace my MakeCart utility with a far more powerful loader - the entire TI itself!
// Each cart type has its own restrictions in order to work.
void DoMakeDlg(HWND hwnd) {
	// Create a dialog to manage the file creation
	DialogBox(NULL, MAKEINTRESOURCE(IDD_CARTDLG), hwnd, CartDlgProc);
}

// returns a vanity padding byte, and lots of 0xff bytes (better for eproms)
// formatted to be compatible with GetSafeCpuByte()
Byte GetPaddingByte(int /*ad*/, int /*bank*/) {
	static int nPos = -1;
	static const char *str = "\xffMade with Classic99\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";

	nPos++;
	if (str[nPos] == '\0') nPos=0;
	return str[nPos];
}

// returns a VDP byte, to look like GetSafeCpuByte
Byte GetSafeVDP(int ad, int /*bank*/) {
	return VDP[ad];
}

#if 0
//
// This works, reasonably well, but I ended up not needing it
// after remembering the DSR buffer space. Keeping the code
// around for now.
//
// perform a simple LZ77 compression. Blocks are always 4 bytes
// long. When you look at a byte, there are three possibilities:
// Byte is 0x00: next four bytes are a literal copy
// High bit is clear: this byte and the next three are a literal copy
// High bit is set: byte&0x7f bytes backwards starts the 4-byte reference
// Back references can overlap with data, but the maximum back reference
// is 127 bytes. So keep a 127 byte ring buffer in scratchpad of the last
// 127 bytes decoded for easy reference, make sure to update scratchpad
// and VDP for every byte, otherwise you will get out of sync.
// Returns NULL if compression is impossible (expansion happens instead, or at least
// exceeds the slightly smaller buffer needed). pIn must be 16k!
// In that case, the encoder should set a flag and store uncompressed.
// Note that for TI BASIC programs, it will not be possible to save the
// VDP screen in that case, so the user needs to be told.
//
// TODO: this needs to return the output size so the caller knows
// what to do. Maybe remove the failure from here (unless > 16k) and let the caller decide?
unsigned char *SimpleLZ77(unsigned char *pIn) {
	unsigned char *pRet = (unsigned char*)malloc(16384-768);
	int nPosIn = 0;
	int nPosOut = 0;
	bool bAllowOverlap = true;
	int nByte;
	int Cnt[256];

	for (int i=0; i<256; i++) Cnt[i]=0;

	while ((nPosIn < 16384) && (nPosOut < 16384-768)) {
		// check for a negative match
		nByte=0;
		for (int i=1; i<256; i++) {
			if (nPosIn-i < 0) break;	// beginning of input buffer
			if ((!bAllowOverlap)&&(i<4)) continue;	// don't allow overlap of data (used to compare results)
			if (0 == memcmp(&pIn[nPosIn-i], &pIn[nPosIn], 4)) {
				// matched!	
				nByte = i;
				break;
			}
		}
		// we can skip the 0x00 byte if the first byte of the literal
		// block has its high bit clear. when decoding, a literal block
		// either starts with high bit set, or a byte 0x00
		if (nByte == 0) {
			// literal string to copy in
			if (nPosOut >= 16384-768-5) {
				break;
			}
			if ((pIn[nPosIn]&0x80)!=0) {
				pRet[nPosOut++]=0x00;
				Cnt[128]++;
			} else {
				Cnt[0]++;
			}
			memcpy(&pRet[nPosOut], &pIn[nPosIn], 4);
			nPosOut+=4;
		} else {
			pRet[nPosOut++] = nByte | 0x80;
			Cnt[nByte]++;
		}
		nPosIn+=4;
	}
	if (nPosIn < 16384) {
		debug_write("Failed compression - buffer overflow at %d bytes in", nPosIn);
		free(pRet);
		return NULL;
	}

	for (int i=0; i<129; i++) {
		debug_write("Count of %03d = %d", i, Cnt[i]);
	}

	debug_write("Compressed file to %d bytes with overlap(%d)", nPosOut, bAllowOverlap);
	return pRet;
}
#endif


// Saves CPU RAM (and optionally VDP RAM) in E/A#5 PROGRAM image format (TIFILES only)
// Takes opt.Boot as the load/start point, and saves all RAM specified as chained
// files. If the opt.Boot is past the beginning of memory (ie: memory starts at
// >A000 but opt.Boot is at >A100), then the area before the PC is loaded separately.
// It should be warned that this must be breakpointed at the true entry to the program,
// since no registers or workspace information will be preserved. If VDP is saved,
// it should be warned that VDP is not auto-loaded and that the program will need to
// load it manually, it will be saved as a single block of PROGRAM image data with
// no header on the assumption the user knows what to do with it. Add a reminder
// to save low memory if the E/A routines are needed.
// The user should be able to select high mem (>A000->FFFF), low mem (>2000->3FFF), and
// VDP (>0000->3FFF), as well as being able to override each range.
// This provides a more-or-less unlimited replacement for the SAVE program.
class MakeEA5Class {
public:
	MakeEA5Class(HWND in_hwnd, struct options in_opt) {
		hwnd = in_hwnd;
		opt = in_opt;
	}

	// this function takes raw PC pointers and saves it out with a simplified
	// TIFILES program header. Pass 0 as nFirst to write without a 6-byte header.
	bool SaveOneEA5(unsigned char *p1, unsigned char *p2, int nFirst, int nLeft, char *pszFileName) {
		FILE *fp;
		Byte h[128];							// header 
		memset(h, 0, 128);						// initialize with zeros

		unsigned int nLength = (p2-p1)+1;
		unsigned int nLengthSectors = (nLength+255)/256;

		// this is mostly a sanity check, but we allow more than 8k files for VDP saves
		if (nLength > 0x4000) {
			debug_write("Save size larger than 16k, failing (normally 8k is max)");
			return false;
		}

		if (nFirst != 0) {
			// *p1 leaves 6 bytes for the header
			if (nLeft) {
				p1[0] = 0xff;			// more files flag (FFFF = yes, 0000 = no)
				p1[1] = 0xff;
			} else {
				p1[0] = 0x00;			// more files flag (FFFF = yes, 0000 = no)
				p1[1] = 0x00;
			}
			p1[2] = (nLength-6)/256;	// length
			p1[3] = (nLength-6)%256;	
			p1[4] = nFirst / 256;		// load address
			p1[5] = nFirst % 256;
		}

		fp=fopen(pszFileName, "wb");
		if (NULL == fp) {
			// couldn't open the file
			debug_write("Can't open %s for writing, errno %d", pszFileName, errno);
			return false;
		} else {
			debug_write("Writing E/A#5 file %s", pszFileName);
		}

		h[0] = 7;
		h[1] = 'T';
		h[2] = 'I';
		h[3] = 'F';
		h[4] = 'I';
		h[5] = 'L';
		h[6] = 'E';
		h[7] = 'S';
		h[8] = nLengthSectors>>8;			// length in sectors HB
		h[9] = nLengthSectors&0xff;			// LB 
		h[10] = TIFILES_PROGRAM;			// File type 
		h[11] = 0;							// records/sector
		h[12] = nLength & 0xff;				// # of bytes in last sector
		h[13] = 0;							// record length 
		h[14] = 0;							// # of records(FIX)/sectors(VAR) LB 
		h[15] = 0;							// HB

		fwrite(h, 1, 128, fp);

		if (fwrite(p1, 1, nLength, fp) == nLength) {
			// pad it up to a full sector multiple
			int nPad = nLength % 0x100;
			if (nPad > 0) {
				nPad = 256 - nPad;
				for (int idx=0; idx < nPad; idx++) {
					fputc(GetPaddingByte(0,0), fp);
				}
			}
		} else {
			debug_write("Failed to write full length of file. errno %d", errno);
			fclose(fp);
			return false;
		}

		fclose(fp);
		return true;
	}

	// loops for CPU data
	bool DoOneLoop(int nStart, int nEnd, bool bMore) {
		int nCnt, nFirst, nLeft;
		unsigned char buf[8192];		// for CPU memory copies, since it could come from AMS or otherwise (someday?)

		nCnt = 6;
		nFirst = nStart;
		nLeft = nEnd - nStart + 1;
		if (bMore) {
			nLeft++;	// so it never runs out
		}
		for (int ad = nStart; ad <= nEnd; ad++) {
			buf[nCnt++] = GetSafeCpuByte(ad, 0);
			nLeft--;
			if (nCnt >= 0x2000) {
				// time to write the block
				if (!SaveOneEA5(&buf[0], &buf[nCnt-1], nFirst, nLeft, opt.FileName)) {
					return false;
				}
				nCnt = 6;
				nFirst = ad+1;
				opt.FileName[strlen(opt.FileName)-1]++;
			}
		}
		// time to write the block
		if (!SaveOneEA5(&buf[0], &buf[nCnt-1], nFirst, nLeft, opt.FileName)) {
			return false;
		}
		opt.FileName[strlen(opt.FileName)-1]++;
		return true;
	}

	void Go() {
		//	IDC_CHKHIGHRAM		check
        //  IDC_CHKCARTMEM      check
		//	IDC_CHKLOWRAM		check
		//	IDC_CHKVDPRAM		check (save only)
		//	IDC_BOOT			text (boot address)
		//	IDC_CHKEA			check (e/a utilities)
		//	IDC_CHKCHARSET		check (load charset) (save through VDP only)
		//	IDC_CHKCHARA1		check (use CHARA1) (save through VDP only)
		
		// special notes: User must write their own code to load VDP code - we save it just for convenience.
		// this does make a bit of a chicken-and-an-egg situations since the code must already be
		// written, but in most cases the inconvenience should be minor and the need infrequent

		// Now from the beginning of that block to the PC, if any
		if (opt.Boot != 0) {
			if (opt.Boot < 0x4000) {
				// boot in LOW ram
				// Our first block to save is from the PC to the end 
				if (!DoOneLoop(opt.Boot, opt.EndLow, (opt.StartLow != opt.Boot) || (opt.EndHigh != 0))) {
					goto error;
				}
				// Next, if needed, from the start of the block to the PC
				if (opt.Boot != opt.StartLow) {
					if (!DoOneLoop(opt.StartLow, opt.Boot-1, (opt.EndHigh != 0)||(opt.EndMid != 0))) {
						goto error;
					}
				}
                // then the cart mem, if any
   				if (opt.EndMid) {
					if (!DoOneLoop(opt.StartMid, opt.EndMid, (opt.EndHigh != 0))) {
						goto error;
					}
				}

				// now the high block, if any
				if (opt.EndHigh) {
					// there are never 'more' even if we do VDP, since the VDP can't be auto-loaded
					if (!DoOneLoop(opt.StartHigh, opt.EndHigh, false)) {
						goto error;
					}
				}
            } else if (opt.Boot < 0xA000) {
                // boot in CART mem
				// Our first block to save is from the PC to the end 
				if (!DoOneLoop(opt.Boot, opt.EndMid, (opt.StartMid != opt.Boot) || (opt.EndHigh != 0) || (opt.EndLow != 0))) {
					goto error;
				}
				// Next, if needed, from the start of the block to the PC
				if (opt.Boot != opt.StartMid) {
					if (!DoOneLoop(opt.StartMid, opt.Boot-1, (opt.EndHigh != 0)||(opt.EndLow != 0))) {
						goto error;
					}
				}

                // then the low mem, if any
   				if (opt.EndLow) {
					if (!DoOneLoop(opt.StartLow, opt.EndLow, (opt.EndHigh != 0))) {
						goto error;
					}
				}

				// now the high block, if any
				if (opt.EndHigh) {
					// there are never 'more' even if we do VDP, since the VDP can't be auto-loaded
					if (!DoOneLoop(opt.StartHigh, opt.EndHigh, false)) {
						goto error;
					}
				}
			} else {
				// boot in HIGH ram
				// Our first block to save is from the PC to the end 
				if (!DoOneLoop(opt.Boot, opt.EndHigh, (opt.StartHigh != opt.Boot) || (opt.EndLow != 0))) {
					goto error;
				}
				// Next, if needed, from the start of the block to the PC
				if (opt.Boot != opt.StartHigh) {
					if (!DoOneLoop(opt.StartHigh, opt.Boot-1, (opt.EndLow != 0))) {
						goto error;
					}
				}
				// now the low block, if any
				if (opt.EndLow) {
					// there are never 'more' even if we do VDP, since the VDP can't be auto-loaded
					if (!DoOneLoop(opt.StartLow, opt.EndLow, false)) {
						goto error;
					}
				}
			}
		}

		// now if there is VDP, save it and warn the user
		if (opt.EndVDP) {
			// we can't check StartVDP since that can legally be 0
			// No loop here, save it as one big chunk. I doubt it will
			// work for most loaders if you try to save all 16k... but
			// that is your problem. :)
			if (!SaveOneEA5(&VDP[opt.StartVDP], &VDP[opt.EndVDP], 0, 0, opt.FileName)) {
				goto error;
			}
			MessageBox(hwnd, "VDP file data saved, however, your application must load it manually. See manual for details.", "Notice", MB_OK | MB_ICONINFORMATION);
		}

		return;

	error:
		MessageBox(hwnd, "An error occurred writing E/A#5 files, see debug for details.", "Error occurred.", MB_OK | MB_ICONERROR);
	}

private:
	HWND hwnd;
	struct options opt;
};


// Saves CPU RAM (and optionally VDP RAM) in E/A#3 UNCOMPRESSED image format (TIFILES only)
// Normally not autostarting, but an autostart entry is added if opt.Boot is non-zero.
// The user should be able to select high mem (>A000->FFFF) and low mem (>2000->3FFF)
// This mostly exists for MiniMemory which doesn't have PROGRAM image load.
// We can lean on the FIAD object for the file creation, since record-based files
// can be a bit of a pain...
class MakeEA3Class {
public:
	MakeEA3Class(HWND in_hwnd, struct options in_opt) {
		hwnd = in_hwnd;
		opt = in_opt;
	}

    ~MakeEA3Class() {
        if (NULL != fi.pData) {
            free(fi.pData);
            fi.pData = NULL;
        }
    }

    // sz must be at least 81 chars and have room left to finish the string!
    void finishLine(char *sz) {
        char buf[256];
        int chk = 0;

        // create checksum (2s complement, includes checksum tag!)
        strcat(sz, "7");
        char *p = sz;
        while (*p) {
            chk += *(p++);
        }
        chk = 0x10000 - chk;
        sprintf(buf, "%04X", chk);
        strcat(sz, buf);

        // end the record
        strcat(sz, "F");

        // pad out to 76 chars
        while (strlen(sz) < 76) strcat(sz, " ");

        // add the line number - we should be 80 chars!
        sprintf(buf, "%04d", nLine++);
        strcat(sz, buf);

        // copy the string to VDP
        memcpy(&VDP[fi.DataBuffer], sz, 80);

        // and give it to the disk driver, which updates our buffers
        dsk.Write(&fi);
    }

	// this function takes raw PC pointers and saves it out with a simplified
	// TIFILES program header. (p1->p2 inclusive). No check for ref/def overwrite.
    // Lines are DF80 uncompressed mode (for XB)
    // Used tags:
    // 00000________    - module ID (8 spaces)
    // 9xxxx            - Absolute load address
    // Bxxxx            - Absolute data
    // 7xxxx            - record checksum
    // F                - end of record
    // The line is then padded to 80 chars ending with a line number in decimal
    // ie: 00000Clasic99A0000B0000A0002A0004B0000B0001B0000A000AA000CA000E7F38BF       0001
    // At the end:
    // 1xxxx            - autostart address, IF opt.Boot is set
    // :Classic99 object dump - mark end of file
	bool SaveOneEA3(unsigned char *p1, unsigned char *p2, int adr) {
        // we're going to (very carefully!) hack this through the disk system
        // build strings up to char 60 (we can step over it cause we have till 80)
        static int lastAdr = 0;     // last address we processed

        char outsz[81];             // buffer to build the line into
        unsigned char *pWork = p1;  // working pointer

        if (bFirst) {
            strcpy(outsz,"00000        ");      // initial tag for line 1 only
            lastAdr = 0;            // reset, new save
            nLine = 1;              // reset, new save
            bFirst = false;
        }

        while (pWork <= p2) {
            if (strlen(outsz) < 60) {
                char buf[32];

                if (adr != lastAdr) {
                    sprintf(buf, "9%04X", adr);
                    strcat(outsz, buf);
                }

                // in case the address was added
                if (strlen(outsz) < 60) {
                    int word = (*(pWork++))<<8;
                    word |= *(pWork++);
                    adr += 2;
                    lastAdr=adr;
                    sprintf(buf, "B%04X", word);
                    strcat(outsz, buf);
                }
            } else {
                finishLine(outsz);
                outsz[0]='\0';
            }
        }
       
        return true;
	}

	// loops for CPU data
	bool DoOneLoop(int nStart, int nEnd) {
		int nCnt = 0;
		unsigned char buf[32768];		// for CPU memory copies, since it could come from AMS or otherwise (someday?)

		for (int ad = nStart; ad <= nEnd; ad++) {
			buf[nCnt++] = GetSafeCpuByte(ad, 0);
		}
		// time to write the block
		if (!SaveOneEA3(&buf[0], &buf[nCnt-1], nStart)) {
			return false;
		}
		return true;
	}

    // don't exit early - we have to restore VRAM!
	void Go() {
		//	IDC_CHKHIGHRAM		check
        //  IDC_CHKCARTMEM      check
		//	IDC_CHKLOWRAM		check
		//	IDC_BOOT			text (boot address)
		//	IDC_CHKEA			check (e/a utilities)
		//	IDC_CHKCHARSET		check (load charset) (save through VDP only)
		//	IDC_CHKCHARA1		check (use CHARA1) (save through VDP only)
        char backup[256];

        try {
            // backup the VRAM we're using to interact with the disk system
            memcpy(backup, &VDP[fi.DataBuffer], 255);
		
            // set up just enough disk interface to work
            fi.bDirty = true;
            fi.bFree = false;
            fi.bOpen = false;
            fi.CharCount = 0;
            fi.csName = opt.FileName;
            fi.csOptions = "";
            fi.FileType = 0;    // display/fixed
            fi.ImageType = IMAGE_TIFILES;
            fi.LastError = 0;
            fi.nCurrentRecord = 0;
            fi.nDrive = -1;     // this will crash if we call a function that needs it!!
            fi.nIndex = -1;
            fi.nLocalData = 0;
            fi.RecordLength = 80;
            fi.RecordNumber = 0;
            fi.RecordsPerSector = 256/80;   // should be 4
            fi.ScreenOffset = 0;
            fi.Status = 0;
            // data that may change
            fi.BytesInLastSector = 0;
            fi.DataBuffer = 0;      // VDP 0
		    fi.nDataSize = (100) * (fi.RecordLength + 2);
		    fi.pData = (unsigned char*)malloc(fi.nDataSize);
            fi.LengthSectors = 0;
            fi.NumberRecords = 0;

		    // Now from the beginning of that block to the PC, if any
            bFirst = true;
			// now the low block, if any
			if (opt.EndLow) {
				if (!DoOneLoop(opt.StartLow, opt.EndLow)) {
    				throw std::invalid_argument("Save block failed");
				}
			}
            // now the cart block, if any
			if (opt.EndMid) {
				if (!DoOneLoop(opt.StartMid, opt.EndMid)) {
    				throw std::invalid_argument("Save block failed");
				}
			}
			// now the high block, if any
			if (opt.EndHigh) {
				if (!DoOneLoop(opt.StartHigh, opt.EndHigh)) {
    				throw std::invalid_argument("Save block failed");
				}
			}

            // now the autostart tag, if defined
            if (opt.Boot) {
                char buf[81];
                sprintf(buf, "1%04X", opt.Boot);
                finishLine(buf);
            }

            // and lastly, the end of file tag
            // this will add a checksum, but that's ok
            {
                char buf[81];
                strcpy(buf, ":Classic99 File Dump - ");
                finishLine(buf);
            }

            // now the file is stored in our file object, we just need to ask
            // fiadDisk to write it out
            dsk.Flush(&fi);

        }
        catch(std::invalid_argument &) {
    		MessageBox(hwnd, "An error occurred writing E/A#3 files, see debug for details.", "Error occurred.", MB_OK | MB_ICONERROR);
        }

        // then restore VRAM
        memcpy(&VDP[fi.DataBuffer], backup, 255);

		return;
	}

private:
	HWND hwnd;
	struct options opt;
    FiadDisk dsk;               // temporary disk object for write
    FileInfo fi;
    bool bFirst;
    int nLine;
};


// Same as for EA#5, except that this version copies the data and writes the startup
// and copy code to load the data from a 379 style bank-switched cartridge. It should
// put the copy loop in the last bank (after all the data) with a startup function to
// switch banks and jump to the right address at the beginning of every bank. The 
// existing code is mostly okay, but make sure that it is updated to set the keyboard
// mode as well as all registers, and add code to copy in the VDP data as well, if
// it is present. Also has the option to load a character set before starting (all the
// options that were available in the old GROM cart).
// 379 carts MUST be a power of two in size to function - this is a bit wasteful if
// making more than one on a multicart. We'll sort it out eventually.
class Make379Copy {
public:
	Make379Copy(HWND in_hwnd, struct options in_opt) {
		hwnd = in_hwnd;
		opt = in_opt;
	}

	// Basic cartridge header - unfortunately if the
	// code blocks change these numbers must change - they
	// must match exactly the number of bytes used!
	// One trick you can use to verify you have the right count - put down one
	// byte too few and make sure you get 'too many initializers' warnings.
	// Also, of course, the result should always be even!
	static const unsigned char hdr[38];
	static const unsigned char tramphdr[28];
	static const unsigned char branchcmd[4];
	static const unsigned char callchrset[4];
	static const unsigned char loadchrset[178];
	static const unsigned char workspace[4];
	static const unsigned char chkkeyboard[20];
	static const unsigned char cpyROMtoRAM[20];
	static const unsigned char cpyROMtoRAMsp[46];
	static const unsigned char cpyROMtoVDP[36];
	static const unsigned char cpyROMtoVDPsp[48];
	static const unsigned char cpyVDPRegs[32];

	// copy hdr and tramphdr into a bank and fill in the filename
	// must only be called at the beginning of a new bank,
	// and never early or late or it will break the fixup system
	int LoadHdr(unsigned char *pWhere) {
		nCurrentBank++;
		memcpy(pWhere, hdr, sizeof(hdr));
		memcpy(pWhere+sizeof(hdr), tramphdr, sizeof(tramphdr));
		// and fill in the filename
		memcpy(pWhere+0x11, opt.Name, strlen(opt.Name));
		*(pWhere+0x10) = (unsigned char)strlen(opt.Name);
		return sizeof(hdr)+sizeof(tramphdr);
	}

	// copies a block of data from the CPU to the cartridge, and sets up the code
	// updates nOutSize and nLastSize, as well as the OutCart and LastBank arrays
	void CopyROMBlock(int nFirstTI, int nLastTI, Byte (*GetByte)(int,int), const unsigned char *pCode, int nCodeSize) {
		int nSize = nLastTI - nFirstTI + 1;
		int nCnt = 0;				// how many bytes did we copy?
		int nStartDest = nFirstTI;	// where did this block start?
		int nStartSrc = nOutSize&0x1fff;
		for (int p = nFirstTI; p <= nLastTI; p++) {
			OutCart[nOutSize++] = GetByte(p, 0);
			nCnt++;
			if (((nOutSize > 0) && ((nOutSize&0x1fff)==0)) || (p == nLastTI)) {
				// we have reached a bank boundary
				// write out the code needed to copy this bank
				// we assume that the last bank never overflows, since it's separate (and it shouldn't be able to)
				// Note the code copied must follow a particular order for these fixups to work!!
				if (nCodeSize > 0) {
					memcpy(&LastBank[nLastSize], pCode, nCodeSize);
					
					// patch it
					LastBank[nLastSize+2] = ((nStartSrc+0x6000)>>8)&0xff;
					LastBank[nLastSize+3] = (nStartSrc+0x6000)&0xff;
					LastBank[nLastSize+6] = (nStartDest>>8)&0xff;
					LastBank[nLastSize+7] = nStartDest&0xff;
					LastBank[nLastSize+10] = (nCnt>>8)&0xff;
					LastBank[nLastSize+11] = nCnt&0xff;

					// we can't fix the bank index until we know how many
					// banks there will be. This is because the last bank is
					// number zero, and is one of many reasons that this choice
					// of order is a royal pain in the ass.
					if (nBankFixups > 15) {
						MessageBox(hwnd, "Failed to manage banking fixups - build will fail.", "Internal Error", MB_OK | MB_ICONERROR);
						nBankFixups = 0; // just to suppress further errors
					} else {
						nBankFixAddr[nBankFixups] = nLastSize+14;
						nBankFixBank[nBankFixups] = nCurrentBank;
						nBankFixups++;
					}

					nLastSize += nCodeSize;
				}
				
				if ((nOutSize&0x1fff) == 0) {
					// load next header, if appropriate
					nOutSize += LoadHdr(&OutCart[nOutSize]);
				}
				
				// update counters
				nCnt = 0;
				nStartDest = p+1;
				nStartSrc = nOutSize&0x1fff;

				// and now we can continue
			}
		}
	}

	void Go() {
		// supported options: 
		//	IDC_CHKHIGHRAM		check
		//	IDC_CHKLOWRAM		check
		//	IDC_CHKVDPRAM		check
		//	IDC_BOOT			text (boot address)
		//	IDC_NAME			text (title)
		//	IDC_CHKEA			check (e/a utilities) (through low RAM only)
		//	IDC_CHKCHARSET		check (load charset)
		//	IDC_CHKCHARA1		check (use CHARA1) (through VDP RAM only)
		//	IDC_KEYBOARD		check (init keyboard)
        //  IDC_INVERTBANKS     check (invert bank order)
        const char *pExt;

		nOutSize=0;
		nLastSize=0;
		nCurrentBank=-1;				// counts UP, not down, so inverse of the TI (we assume building inverted and deal with it on output)
		nBankFixups=0;

		// fill the output data with EPROM friendly 0xff bytes
		memset(OutCart, 0xff, sizeof(OutCart));	
		memset(LastBank, 0xff, sizeof(LastBank));

		// make sure the filename ends with .BIN
        // New extensions: 9.BIN for 379/inverted, 8.BIN for non-inverted
        if (opt.bInvert) {
            pExt = "_9.BIN";
        } else {
            pExt = "_8.BIN";
        }
		if (_stricmp(&opt.FileName[strlen(opt.FileName)-5], &pExt[1])) {
			opt.FileName[250]='\0';
			strcat(opt.FileName, pExt);
			FILE *fp=fopen(opt.FileName, "r");
			if (NULL != fp) {
				fclose(fp);
				sprintf((char*)OutCart, "Output file %s already exists - overwrite?", opt.FileName);
				if (IDNO == MessageBox(hwnd, (char*)OutCart, "Overwrite file?", MB_YESNO | MB_ICONQUESTION)) {
					return;
				}
			}
		}

		// start by setting up the default copy for the copy block (ignoring headers and such)
		memcpy(&LastBank[nLastSize], workspace, sizeof(workspace));
		nLastSize+=sizeof(workspace);

		if ((opt.bCharSet)&&(!opt.bCharA1)) {
			// if we want to load the charset without copying VDP, we have to do that first
			// the actual charset code we'll copy in when merging, since it must go at a fixed
			// address, and this is all floating code
			memcpy(&LastBank[nLastSize], callchrset, sizeof(callchrset));
			nLastSize += sizeof(callchrset);
		}

		if (opt.bKeyboard) {
			// user wants to init the keyboard? we can do that!
			memcpy(&LastBank[nLastSize], chkkeyboard, sizeof(chkkeyboard));
			nLastSize += sizeof(chkkeyboard);
		}

		// now we can start copying data

		// The first thing we always need is the header
		nOutSize+=LoadHdr(&OutCart[nOutSize]);
		// Now, we just need to copy blocks of data, and write copy code for each block
		// Put the VDP stuff first so it can take effect during the RAM copy
		if (opt.bVDPRegs) {
			// Copy in the VDP registers
			memcpy(&LastBank[nLastSize], cpyVDPRegs, sizeof(cpyVDPRegs));
			nLastSize += sizeof(cpyVDPRegs);
		}
		if (opt.EndVDP != 0) {
			// copy the scratchpad functions in
			memcpy(&LastBank[nLastSize], cpyROMtoVDPsp, sizeof(cpyROMtoVDPsp));
			nLastSize += sizeof(cpyROMtoVDPsp);
			CopyROMBlock(opt.StartVDP, opt.EndVDP, GetSafeVDP, cpyROMtoVDP, sizeof(cpyROMtoVDP));
			// VDP is allowed to copy bytes, so it may misalign - realign if so :)
			if (nOutSize & 0x01) nOutSize++;
		}
		// now the RAM
		if (opt.EndHigh != 0) {
			// we need to copy in the scratchpad subroutine first!
			memcpy(&LastBank[nLastSize], cpyROMtoRAMsp, sizeof(cpyROMtoRAMsp));
			nLastSize += sizeof(cpyROMtoRAMsp);
			CopyROMBlock(opt.StartHigh, opt.EndHigh, GetSafeCpuByte, cpyROMtoRAM, sizeof(cpyROMtoRAM));
		}
		if (opt.EndLow != 0) {
			// copy the scratchpad function in only if the high copies didn't already
			if (opt.EndHigh == 0) {
				memcpy(&LastBank[nLastSize], cpyROMtoRAMsp, sizeof(cpyROMtoRAMsp));
				nLastSize += sizeof(cpyROMtoRAMsp);
			}
			CopyROMBlock(opt.StartLow, opt.EndLow, GetSafeCpuByte, cpyROMtoRAM, sizeof(cpyROMtoRAM));
		}

		// set the appropriate (current) workspace
		{
			Word WP = pCurrentCPU->GetWP();
			memcpy(&LastBank[nLastSize], workspace, sizeof(workspace));
			LastBank[nLastSize+2] = (WP>>8) & 0xff;
			LastBank[nLastSize+3] = WP & 0xff;
		}

		// finally, add a branch to the entry point
		memcpy(&LastBank[nLastSize], branchcmd, sizeof(branchcmd));
		LastBank[nLastSize+2] = (opt.Boot>>8) & 0xff;
		LastBank[nLastSize+3] = opt.Boot & 0xff;
		nLastSize += sizeof(branchcmd);

		// okay, all the data is processed, and all we have left to do is stitch together the last bank. 
		// If there is not enough room in the last bank, then we'll create one more new on.
		int nNeededBytes = nLastSize;
		if ((opt.bCharSet)&&(!opt.bCharA1)) {
			nNeededBytes += sizeof(loadchrset);
		}

		if ((nNeededBytes+(nOutSize&0x1fff)) > 0x2000) {
			// we need a new bank, so just pad this one out
			// the zero code length means no data is copyed to LastBank
			// This should add the new header for us, too
			CopyROMBlock(nOutSize, 0x1fff, GetPaddingByte, NULL, 0);
		}

		// now, similar to the above, make sure we are truly on the last bank. Chips come in 8,16,32,64, and 128k,
		// and sadly we can't full anything in the middle. (But we disregard 128k since we can't fill that, but we
		// CAN get to 64k if too much is copied.)
		if ((nOutSize >= 0x4000) && (nOutSize < 0x8000-0x2000)) {
			// if more than 16k but not on the last bank of 32k, we have to pad it out 
			CopyROMBlock(nOutSize, 0x8000-0x2000-1, GetPaddingByte, NULL, 0);
		} else if ((nOutSize >= 0x8000) && (nOutSize < 0x10000-0x2000)) {
			// same, but more than 32k and not on the last page of 64k
			CopyROMBlock(nOutSize, 0x10000-0x2000-1, GetPaddingByte, NULL, 0);
		}

		// there should now be enough space!
		// First, patch the boot to jump to our new start address (rather than running the trampoline)
		memcpy(&OutCart[(nOutSize&0xe000)+0x26], branchcmd, sizeof(branchcmd));
		OutCart[(nOutSize&0xe000)+0x28] = ((nOutSize>>8)&0x1f)+0x60;
		OutCart[(nOutSize&0xe000)+0x29] = nOutSize&0xff;
		// overwrites old code - no change in size

		// Then, if requested, copy the VDP registers over some more of that unused space
		if (opt.bVDPRegs) {
			for (int i = 0; i < 8; i++) {
				OutCart[(nOutSize&0xe000)+0x2a+i] = VDPREG[i];
			}
		}

		// next, patch the banking commands for the correct bank indexes
		for (int i=0; i<nBankFixups; i++) {
			LastBank[nBankFixAddr[i]] = 0x60;		// assumed
			LastBank[nBankFixAddr[i]+1] = ((nCurrentBank - nBankFixBank[i])*2) & 0xff;
		}

		// next, copy in the boot commands
		memcpy(&OutCart[nOutSize], LastBank, nLastSize);
		// update offset
		nOutSize+=nLastSize;

		// finally, if needed, copy the character set code in at a fixed offset
		if ((opt.bCharSet)&&(!opt.bCharA1)) {
			nOutSize=(nOutSize&0xe000)+0x1f4e;
			memcpy(&OutCart[nOutSize], loadchrset, sizeof(loadchrset));
			nOutSize+=sizeof(loadchrset);	// memory should be full now
		}

		// pad the data to the end of the block
		while ((nOutSize&0x1fff) != 0x0000) {
			OutCart[nOutSize++] = GetPaddingByte(0,0);
		}

		// That's it. Believe it or not, we're done. Just need to save it now.
		FILE *fp = fopen(opt.FileName, "wb");
		if (NULL == fp) {
			MessageBox(hwnd, "Failed to open output file.", "Disk error", MB_OK | MB_ICONERROR);
			return;
		}
        if (opt.bInvert) {
    		fwrite(OutCart, 1, nOutSize, fp);
        } else {
            // write the banks out in the inverse (which is really non-inverted) order
            for (int idx=nOutSize-0x2000; idx>=0; idx-=0x2000) {
                fwrite(&OutCart[idx], 1, 0x2000, fp);
            }
        }
		fclose(fp);
	}

private:
	HWND hwnd;
	struct options opt;

	unsigned char OutCart[64*1024];	// maximum size is 64k (although, should never be more than about 50k or so, but we have to pad the image)
	int nOutSize;
	unsigned char LastBank[8*1024];	// last bank contains the actual copy code
	int nLastSize;
	int nCurrentBank;				// counts UP, not down, so inverse of the TI
	int nBankFixups;
	int nBankFixAddr[16];			// about twice what we should need!
	int nBankFixBank[16];
};

const unsigned char Make379Copy::hdr[] = {
	// >6000
	0xaa, 0x01,		// header AA01
	0x01, 0x40,		// one program, unused byte used for the VDP write-mode bit ;)
	0x00, 0x00,		// powerup list (not available in cart)
	0x60, 0x0c,		// program list (right after this header!)
	0x00, 0x00,		// dsr list 
	0x00, 0x00,		// subprogram list
	
	// >600C
	0x00, 0x00,		// next program (none)
	0x60, 0x26,		// start address (right after the name)
	0x14, 0x20,		// length and name (patched below, up to 20 chars) (>6010)
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x20,
	0x20, 0x00		// end of name, and padding to make it even
};

// code to switch to the last bank. The last bank is
// always going to be bank "0" so there's no condition on the code.
// TODO: this is overcomplicated - if the header always starts with the
// bank switch then we don't need a trampoline.
const unsigned char Make379Copy::tramphdr[] = {
	// >6026		Normal Header				Last Header
	0x02, 0x00,		// LI R0,TRAMP				B @>xxxx
	0x60, 0x3A,
	0x02, 0x01,		// LI R1,>8320				VDPR0,VDPR1		>602A
	0x83, 0x20,		//							VDPR2,VDPR3
	0xCC, 0x70,		// MOV *R0+,*R1+			VDPR4,VDPR5
	0xCC, 0x70,		// MOV *R0+,*R1+			VDPR6,VDPR7
	0xCC, 0x70,		// MOV *R0+,*R1+			
	0xCC, 0x70,		// MOV *R0+,*R1+
	0x04, 0x60,		// B @>8320
	0x83, 0x20,
	0xc8, 0x00,		// TRAMP: MOV R0,@>6000
	0x60, 0x00,
	0x04, 0x60,		// B @>6026
	0x60, 0x26

	// >6042
};

// just a branch command for getting to the right place
const unsigned char Make379Copy::branchcmd[] = {
	0x04,0x60,		// B @>0000 (to be filled in later)
	0x00,0x00
};

// call out for the charset (hardcoded destination)
const unsigned char Make379Copy::callchrset[] = {
	0x06,0xa0,		// BL @>7F4E
	0x7f,0x4e
};

// code to load the character sets from GROM
// this is not targetted to be relocatable, so goes
// right at >7F4E (end of bank)
const unsigned char Make379Copy::loadchrset[] = {
	// 7F4E = GOGO
	0xC2, 0x4B,	// MOV R11,R9   * Save our return spot
	//            * +++ 99/4 support begin +++
	//            * load R3 with 6 for 99/4, or 7 for 99/4A
	0x04, 0xC0,	// CLR R0
	0x06, 0xA0,	// BL @GPLSET
	0x7f, 0x9a,	// 
	0x06, 0xA0,	// BL @GETGPL   * read GROM >0000
	0x7f, 0xa6,	// 
	0x02, 0x03,	// LI R3,7
	0x00, 0x07,	// 
	0x02, 0x80,	// CI R0,>AA01  * 99/4 is AA01, all versions of 99/4A seem to be AA02 (even 2.2!)
	0xAA, 0x01,	// 
	0x16, 0x0A,	// JNE IS4A     * note we also assume unknown is 99/4A just to be safe
	0x06, 0x03,	// DEC R3
	//            * make a copy of the capitals for the 99/4 to 'support' lowercase
	//            * this will be partially overwritten by the main set, but it works!
	0x02, 0x00,	// LI R0,>0018  * GPL vector address
	0x00, 0x18,	// 
	0x02, 0x01,	// LI R1,>4A00  * dest in VDP - must OR with >4000 for write
	0x4A, 0x00,	// 
	0x02, 0x02,	// LI R2,>0040  * how many chars
	0x00, 0x40,	// 
	0x06, 0xA0,	// BL @GPLVDP   * this function goes somewhere later in your ROM
	0x7f, 0xb4,	// 
	0x10, 0x08,	// JMP MNSET
	//            IS4A
	//            * 'lowercase' letters
	0x02, 0x00,	// LI R0,>004A  * GPL vector address (not available for 99/4)
	0x00, 0x4A,	// 
	0x02, 0x01,	// LI R1,>4B00  * dest in VDP - must OR with >4000 for write
	0x4B, 0x00,	// 
	0x02, 0x02,	// LI R2,>001F  * how many chars
	0x00, 0x1F,	// 
	0x06, 0xA0,	// BL @GPLVDP   * this function goes somewhere later in your ROM
	0x7f, 0xb4,	// 
	//            MNSET
	0x02, 0x00,	// LI R0,>0018  * GPL vector address
	0x00, 0x18,	// 
	0x02, 0x01,	// LI R1,>4900  * dest in VDP - must OR with >4000 for write
	0x49, 0x00,	// 
	0x02, 0x02,	// LI R2,>0040  * how many chars
	0x00, 0x40,	// 
	0x06, 0xA0,	// BL @GPLVDP   * this function goes somewhere later in your ROM
	0x7f, 0xb4,	// 
	0x04, 0x59,	// B *R9        * RETURN TO CALLER
	//            * Set GROM address
	//            GPLSET
	0xD8, 0x00,	// MOVB R0,@>9C02
	0x9C, 0x02,	// 
	0x06, 0xC0,	// SWPB R0
	0xD8, 0x00,	// MOVB R0,@>9C02
	0x9C, 0x02,	// 
	0x04, 0x5B,	// B *R11
	//            * Get a word from GPL
	//            GETGPL
	0xD0, 0x20,	// MOVB @>9800,R0
	0x98, 0x00,	// 
	0x06, 0xC0,	// SWPB R0
	0xD0, 0x20,	// MOVB @>9800,R0
	0x98, 0x00,	// 
	0x06, 0xC0,	// SWPB R0
	0x04, 0x5B,	// B *R11
	//            * Copy R2 characters from a GPL copy function vectored at
	//            * R0 to VDP R1. GPL vector must be a B or BR and
	//            * the first actual instruction must be a DEST with an
	//            * immediate operand. Set R3 to 6 for 99/4 (6 byte characters)
	//            * or 7 for a 99/4A (7 byte characters)
	//            GPLVDP
	0xC2, 0x8B,	// MOV R11,R10    * save return address
	0x06, 0xA0,	// BL @GPLSET     * set GROM address
	0x7f, 0x9a,	// 
	0x06, 0xA0,	// BL @GETGPL     * Get branch instruction (not verified!)
	0x7f, 0xa6,	// 
	0x02, 0x40,	// ANDI R0,>1FFF  * mask out instruction part
	0x1F, 0xFF,	// 
	0x02, 0x20,	// AI R0,3        * skip instruction and destination
	0x00, 0x03,	// 
	0x06, 0xA0,	// BL @GPLSET     * set new GROM address
	0x7f, 0x9a,	// 
	0x06, 0xA0,	// BL @GETGPL     * get actual address of the table
	0x7f, 0xa6,	// 
	0x06, 0xA0,	// BL @GPLSET     * and set that GROM address - GROM is now ready!
	0x7f, 0x9a,	// 
	0x06, 0xC1,	// SWPB R1        * assume VDP is already prepared for write to save space
	0xD8, 0x01,	// MOVB R1,@>8C02
	0x8C, 0x02,	// 
	0x06, 0xC1,	// SWPB R1
	0xD8, 0x01,	// MOVB R1,@>8C02 * VDP is now ready!
	0x8C, 0x02,	// 
	0x04, 0xC0,	// CLR R0
	0xD8, 0x00,	// MOVB R0,@>8C00 * pad the top of the char with a space
	0x8C, 0x00,	// 
	0xC0, 0x03,	// MOV R3,R0      * then copy 7 (or 6) bytes
	0x02, 0x83,	// CI R3,6        * check for 99/4
	0x00, 0x06,	// 
	0x16, 0x02,	// JNE LP9
	0xD8, 0x00,	// MOVB R0,@>8C00 * extra blank line for 99/4
	0x8C, 0x00,	// 
	0xD8, 0x20,	// MOVB @>9800,@>8C00  * copy a byte (both sides autoincrement)
	0x98, 0x00,	// 
	0x8C, 0x00,	// 
	0x06, 0x00,	// DEC R0
	0x16, 0xFB,	// JNE LP9
	0x06, 0x02,	// DEC R2         * next character
	0x16, 0xF1,	// JNE LP8
	0x04, 0x5A	// B *R10

	// >8000
};

// set up our workspace
const unsigned char Make379Copy::workspace[] = {
	0x02, 0xe0,	// LWPI >8300
	0x83, 0x00
};

// scan the keyboard once to set up KSCAN correctly
const unsigned char Make379Copy::chkkeyboard[] = {
	0x02, 0x00,	// LI R0,>0500
	0x05, 0x00,
	0xD8, 0x00,	// MOVB R0,@>8374
	0x83, 0x74,
	0x02, 0xe0,	// LWPI >83E0
	0x83, 0xe0,
	0x06, 0xA0,	// BL @>000E
	0x00, 0x0E,
	0x02, 0xE0,	// LWPI >8300
	0x83, 0x00
};

// copy loop from ROM to RAM
const unsigned char Make379Copy::cpyROMtoRAM[] = {
	0x02,0x00,	// LI R0,>6000 (src)
	0x60,0x00,
	0x02,0x01,	// LI R1,>2000 (dst)
	0x20,0x00,
	0x02,0x02,	// LI R2,>1234 (count in bytes)
	0x12,0x34,
	0x02,0x03,	// LI R3,>600E (bank to copy from)
	0x60,0x0e,
	0x06,0xa0,	// BL @>8320 (scratchpad must already be set up)
	0x83,0x20
};

// prepare scratchpad for ROM->RAM
const unsigned char Make379Copy::cpyROMtoRAMsp[] = {
	0x02, 0x01,	// LI R1,>8320
	0x83, 0x20,	// 
	0x02, 0x00,	// LI R0,>045B             * B *R11 (gets return address for us)
	0x04, 0x5B,	// 
	0xC4, 0x40,	// MOV R0,*R1
	0x06, 0x91,	// BL *R1
	0xC0, 0x0B,	// MOV R11,R0
	0x02, 0x20,	// AI R0,18
	0x00, 0x12,	// 

	0x02, 0x02,	// LI R2,8
	0x00, 0x08,	//
	0xCC, 0x70,	// MOV *R0+,*R1+
	0x06, 0x02,	// DEC R2
	0x16, 0xFD,	// JNE CLP
	0x10, 0x08,	// JMP POSTLP

	0xC4, 0xC3,	// MOV R3,*R3        * set bank
	0xCC, 0x70,	// MOV *R0+,*R1+ * copy loop
	0x06, 0x42,	// DECT R2
	0x16, 0xFD,	// JNE LP
	0x02, 0x00,	// LI R0,>6000   * restore bank
	0x60, 0x00,	// 
	0xC4, 0x00,	// MOV R0,*R0 
	0x04, 0x5B	// B *R11        * return
};

// copy loop from ROM to VDP
const unsigned char Make379Copy::cpyROMtoVDP[] = {
	0x02, 0x00,	// LI R0,>6000  * Src in ROM
	0x60, 0x00,	// 
	0x02, 0x01,	// LI R1,>4800  * dst in VDP, pre-swapped with write bit set
	0x48, 0x00,	// 
	0x02, 0x02,	// LI R2,>1234  * byte count (BYTE!)
	0x12, 0x34,	// 
	0x02, 0x03,	// LI R3,>600E  * bank to copy from
	0x60, 0x0E,	// 
	0xf0, 0x60,	// SOCB @>6003,R1
	0x60, 0x03, //
	0x06, 0xc1, // SWPB R1
	0xD8, 0x01,	// MOVB R1,@>8C02
	0x8C, 0x02,	// 
	0x06, 0xC1,	// SWPB R1
	0xD8, 0x01,	// MOVB R1,@>8C02
	0x8C, 0x02,	// 
	0x06, 0xA0,	// BL @>8320    * do the copy
	0x83, 0x20	// 
};

// scratchpad setup for ROM->VDP
const unsigned char Make379Copy::cpyROMtoVDPsp[] = {
	0x02, 0x01,	// LI R1,>8320
	0x83, 0x20,	// 
	0x02, 0x00,	// LI R0,>045B             * B *R11 (gets return address for us)
	0x04, 0x5B,	// 
	0xC4, 0x40,	// MOV R0,*R1
	0x06, 0x91,	// BL *R1
	0xC0, 0x0B,	// MOV R11,R0
	0x02, 0x20,	// AI R0,18
	0x00, 0x12,	// 
	0x02, 0x02,	// LI R2,9
	0x00, 0x09,	// 
	0xCC, 0x70,	// MOV *R0+,*R1+
	0x06, 0x02,	// DEC R2
	0x16, 0xFD,	// JNE CLP1
	0x10, 0x09,	// JMP POSTLP1

	0xC4, 0xC3,	// MOV R3,*R3      * Set bank
	0xD8, 0x30,	// MOVB *R0+,@>8C00
	0x8C, 0x00,	// 
	0x06, 0x02,	// DEC R2
	0x16, 0xFC,	// JNE LP1
	0x02, 0x00,	// LI R0,>6000     * restore bank
	0x60, 0x00,	// 
	0xC4, 0x00,	// MOV R0,*R0
	0x04, 0x5B	// B *R11
};

// Load VDP registers - stored at hard coded address in last bank
const unsigned char Make379Copy::cpyVDPRegs[] = {
	0x02, 0x00,	// LI R0,>0080             * VDP Register 0 write, preswapped
	0x00, 0x80,	// 
	0x02, 0x01,	// LI R1,>602A             * location of VDP registers
	0x60, 0x2A,	// 
	0x02, 0x02,	// LI R2,8                 * register count
	0x00, 0x08,	// 
	0xD0, 0x31,	// LP      MOVB *R1+,R0    * get value
	0xD8, 0x00,	// MOVB R0,@>8C02
	0x8C, 0x02,	// 
	0x06, 0xC0,	// SWPB R0
	0xD8, 0x00,	// MOVB R0,@>8C02
	0x8C, 0x02,	// 
	0x06, 0xC0,	// SWPB R0
	0x05, 0x80,	// INC R0
	0x06, 0x02,	// DEC R2
	0x16, 0xF6,	// JNE LP
};

// The same as CopyCart, but this one stores its data in GROM. Straight up replacement
// of my old MakeCart program, should support all listed options.
class MakeGromCopy {
public:
	static const unsigned char hdr[54];
	static const unsigned char callchrset[43];
	static const unsigned char chkkeyboard[4];
	static const unsigned char cpyROMtoRAM[96];
	static const unsigned char cpyVDPRegs[6];
	static const unsigned char Finish[7];

	MakeGromCopy(HWND in_hwnd, struct options in_opt) {
		hwnd = in_hwnd;
		opt = in_opt;
	}

	void WriteCmd(int nFirst, int nPos, int nFirstData, bool bVDP) {
		if (nFirst == nPos) return;		// no data to worry about yet, just a boundary crossing
		nCntDown--;		// to prevent the error from repeating
		if (nCntDown < 0) {
			if (nCntDown == -1) {
				MessageBox(hwnd, "Too many copies in GROM - cartridge will fail.", "Error", MB_OK | MB_ICONERROR);
			}
			return;
		}
		memcpy(&grom[nProgramPos], cpyROMtoRAM, 8);		// just one copy command
		
		// patch count
		grom[nProgramPos+1] = ((nPos-nFirst)>>8)&0xff;
		grom[nProgramPos+2] = (nPos-nFirst)&0xff;
		
		// patch destination type
		if (bVDP) {
			grom[nProgramPos+3] = 0xaf;
		} else {
			grom[nProgramPos+3] = 0x8f;
		}
		
		// patch destination address
		if (bVDP) {
			grom[nProgramPos+4] = (nFirst>>8)&0xff;
			grom[nProgramPos+5] = nFirst & 0xff;
		} else {
			grom[nProgramPos+4] = ((nFirst-0x8300)>>8)&0xff;
			grom[nProgramPos+5] = (nFirst-0x8300) & 0xff;
		}

		// patch source address
		grom[nProgramPos+6] = (nFirstData>>8) & 0xff;
		grom[nProgramPos+7] = nFirstData&0xff;

		nProgramPos+=8;
	}

	void CopyGromMemory(int nFirst, int nEnd, bool bVDP) {
		int nPos = nFirst;
		int nFirstData = nDataPos;
		while (nPos <= nEnd) {
			if (((opt.bGROM8K)&&((nDataPos&0x1fff)==0)) || ((!opt.bGROM8K)&&((nDataPos&0x1fff)==0x1800))) {
				// crossed a GROM boundary, so write out a copy command
				WriteCmd(nFirst, nPos, nFirstData, bVDP);
				// jump ahead if needed
				if (!opt.bGROM8K) {
					nDataPos+=0x0800;
				}
				nFirst = nPos;
				nFirstData = nDataPos;
				if (nDataPos >= 0x10000) {
					MessageBox(hwnd, "Not enough space in GROM - cartridge will fail.", "Error", MB_OK | MB_ICONERROR);
					nDataPos = 0;		// to prevent the error from repeating
				}
			}

			if (bVDP) {
				grom[nDataPos++] = GetSafeVDP(nPos++, 0);
			} else {
				grom[nDataPos++] = GetSafeCpuByte(nPos++, 0);
			}
		}

		// finished this block, so also save it off
		WriteCmd(nFirst, nPos, nFirstData, bVDP);
		nFirst = nPos;
		nFirstData = nDataPos;
	}

	void Go() {
		// Options supported:
		// StartHigh, EndHigh;
		// StartLow, EndLow;
		// StartVDP, EndVDP;
		// Boot;
		// Name[256];
		// FileName[256];
		// bEA (memory copy only)
		// bCharSet
		// bCharA1 (memory copy only)
		// bKeyboard, 
		// bGROM8K;
		// bVDPRegs;
		// FirstGROM;

		// make sure the filename ends with G.BIN
		if (_stricmp(&opt.FileName[strlen(opt.FileName)-5], "G.BIN")) {
			opt.FileName[250]='\0';
			strcat(opt.FileName, "G.BIN");
			FILE *fp=fopen(opt.FileName, "r");
			if (NULL != fp) {
				char buf[256];
				fclose(fp);
				sprintf(buf, "Output file %s already exists - overwrite?", opt.FileName);
				if (IDNO == MessageBox(hwnd, buf, "Overwrite file?", MB_YESNO | MB_ICONQUESTION)) {
					return;
				}
			}
		}

		// prepare the output data
		memset(grom, 0xff, sizeof(grom));
		nProgramPos = opt.FirstGROM * 0x2000;
		// hard code the start of data to after the whole potential header code
		// a little wasteful, but very easy to work with, and still smaller than the
		// fully loaded CPU code loader.
		nDataPos = nProgramPos + sizeof(hdr) + sizeof(callchrset) + sizeof(chkkeyboard) + sizeof(cpyROMtoRAM) + sizeof(cpyVDPRegs) + sizeof(Finish);

		// Now load up the program
		memcpy(&grom[nProgramPos], hdr, sizeof(hdr));
		// patch the header
		grom[nProgramPos+6] = opt.FirstGROM * 0x20;
		grom[nProgramPos+14] = opt.FirstGROM * 0x20;
		memcpy(&grom[nProgramPos+17], opt.Name, strlen(opt.Name));
		grom[nProgramPos+16] = (unsigned char)strlen(opt.Name);
		// save off the VDP registers whether we are going to load them later or not
		for (int i=0; i<8; i++) {
			grom[nProgramPos+0x26+i] = VDPREG[i];
		}
		nProgramPos+=sizeof(hdr);

		// do we need to load up the character set?
		if ((opt.bCharSet) && (!opt.bCharA1)) {
			memcpy(&grom[nProgramPos], callchrset, sizeof(callchrset));
			grom[nProgramPos+9] = 0x60 | ((((nProgramPos+20)&0x1fff)>>8)&0xff);
			grom[nProgramPos+10] = (((nProgramPos+20)&0x1fff)&0xff);
			grom[nProgramPos+18] = 0x40 | ((((nProgramPos+27)&0x1fff)>>8)&0xff);
			grom[nProgramPos+19] = (((nProgramPos+27)&0x1fff)&0xff);
			grom[nProgramPos+41] = opt.FirstGROM*0x20;
			nProgramPos += sizeof(callchrset);
		}

		// how about the keyboard
		if (opt.bKeyboard) {
			memcpy(&grom[nProgramPos], chkkeyboard, sizeof(chkkeyboard));
			nProgramPos += sizeof(chkkeyboard);
		}

		// VDP registers?
		if (opt.bVDPRegs) {
			memcpy(&grom[nProgramPos], cpyVDPRegs, sizeof(cpyVDPRegs));
			grom[nProgramPos+4] = opt.FirstGROM*0x20;
			nProgramPos += sizeof(cpyVDPRegs);
		}

		// start copying the data (we can do up to 12 copies)
		nCntDown = 12;
		if (opt.EndVDP != 0) {
			CopyGromMemory(opt.StartVDP, opt.EndVDP, true);
		}
		if (opt.EndHigh != 0) {
			CopyGromMemory(opt.StartHigh, opt.EndHigh, false);
		}
		if (opt.EndLow != 0) {
			CopyGromMemory(opt.StartLow, opt.EndLow, false);
		}

		// copy the final startup code
		memcpy(&grom[nProgramPos], Finish, sizeof(Finish));
		grom[nProgramPos+2] = (opt.Boot>>8)&0xff;
		grom[nProgramPos+3] = opt.Boot&0xff;
		nProgramPos+=sizeof(Finish);

		// pad up to the beginning of data
		while ((nProgramPos&0x1fff) < 0x00c3) {
			grom[nProgramPos++] = GetPaddingByte(0,0);
		}

		// pad the data to the end of the GROM
		if (opt.bGROM8K) {
			while ((nDataPos&0x1fff) != 0x0000) {
				grom[nDataPos++] = GetPaddingByte(0,0);
			}
		} else {
			while ((nDataPos&0x1fff) != 0x1800) {
				grom[nDataPos++] = GetPaddingByte(0,0);
			}
		}

		// now just save the whole bugger
		FILE *fp = fopen(opt.FileName, "wb");
		if (NULL == fp) {
			debug_write("Error opening output file '%s', code %d", opt.FileName, errno);
			MessageBox(hwnd, "Failed to open output file, unable to save.", "Error", MB_OK | MB_ICONERROR);
			return;
		}
		fwrite(&grom[opt.FirstGROM*0x2000], 1, nDataPos - opt.FirstGROM*0x2000, fp);
		fclose(fp);
	}

private:
	HWND hwnd;
	struct options opt;

	unsigned char grom[0x10000];		// all 8 GROMs (most of this space will not be used, 2k alignment)
	int nDataPos;
	int nProgramPos;
	int nCntDown;
};

const unsigned char MakeGromCopy::hdr[] = {
	// >6000
	//* CARTRIDGE HEADER
	0xAA, 0x01,				//	DATA >AA01	* HEADER
	0x01,					//	BYTE >01	* NUMBER OF PROGRAMS
	0x00,					//	BYTE >00	* UNUSED
	0x00, 0x00,				//	DATA >0000	* POINTER TO POWERUP LIST
	0x60, 0x0C,				//	DATA >600C	* POINTER TO PROGRAM LIST  <--- GROM Bank patch
	0x00, 0x00,				//	DATA >0000	* POINTER TO DSR LIST
	0x00, 0x00,				//	DATA >0000	* POINTER TO SUBPROGRAM LIST

	//* PROGRAM LIST
	0x00, 0x00,				//	DATA >0000	* LINK TO NEXT ITEM
	0x60, 0x36,				//	DATA >6036	* ADDRESS   <--- GROM Bank patch
	0x14,					//	BYTE >14	* NAME LENGTH
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,		// TEXT '            '
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,		// * PADDING - MAX 20 BYTES
	0x20, 0x20, 0x20, 0x20, 0x00,						// plus 1 to make it even

	// Since we can afford the space more on GROM, reserve space for the VDP registers
	// >6026
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

	// and the pattern definition for the cursor!
	// >602E
	0x00, 0x7C, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c

	// >6036
};

// call out for the charset (hardcoded destination)
const unsigned char MakeGromCopy::callchrset[] = {
	// This code detects the 99/4 and skips the lower case char set if detected
	// Instead it loads a copy of the uppercase set over the lowercase letters
	0x31,0x00,0x01,0x59,0x00,0x01,			// MOVE >0001 TO @>8359 FROM GROM@>0001  * 99/4 is version 01, 99/4A is 02
	0xd6,0x59,0x01,							// CEQ @>8359,>01
 
	0x60,0xff,								// BS @>60xx		* to label 1		<-- PATCH ADDRESS (both bytes, within GROM, mask with >1fff)
	0xbf,0x4a,0x0b,0x00,					// DST >0B00,@>834A
	0x06,0x00,0x4a,							// CALL @>004A      * 99/4A - load lowercase letters
	0x40,0xff,								// BR @>60xx        * to label 2		<-- PATCH ADDRESS (both bytes, within GROM, mask with >1fff)
	0xbf,0x4a,0x0a,0x00,					// DST >0A00,@>834A	* LABEL 1 - 99/4 makes a copy for upper/lower
	0x06,0x00,0x18,							// CALL @>0018
	0x86,0x74,								// CLR @>8374		* LABEL 2 - common - load main set
	0xbf,0x4a,0x09,0x00,					// DST >0900,@>834A
	0x06,0x00,0x18,							// CALL @>0018
	0x31,0x00,0x07,0xa8,					// MOVE >0007 to VDP@>08F0 FROM GROM@>602E <-- PATCH ADDRESS (just the bank)
	0xf0,0x60,0x2e
};

// scan the keyboard once to set up KSCAN correctly
const unsigned char MakeGromCopy::chkkeyboard[] = {
	0xBE,0x74,0x05,							// ST >05,@>8374
	0x03									// SCAN
};

// copy loop from ROM to RAM (including VRAM), then we just patch it up
const unsigned char MakeGromCopy::cpyROMtoRAM[] = {
	/* Reserve space for the maximum number of moves - dest offset is address minus >8300 for CPU only */
	// VDP
	0x31, 0x18, 0x00, 0xAF, 0x7D, 0x00, 0xA0, 0x00,	//	MOVE >1800 TO VDP@>0000 FROM GROM@>A000
	0x31, 0x18, 0x00, 0xAF, 0x95, 0x00, 0xC0, 0x00,	//	MOVE >1800 TO VDP@>1800 FROM GROM@>C000
	0x31, 0x18, 0x00, 0xAF, 0xAD, 0x00, 0xE0, 0x00,	//	MOVE >1000 TO VDP@>3000 FROM GROM@>E000

	// LOW RAM
	0x31, 0x18, 0x00, 0x8F, 0x9D, 0x00, 0x60, 0x00,	//	MOVE >1800 TO @>2000 FROM GROM@>6000
	0x31, 0x18, 0x00, 0x8F, 0xB5, 0x00, 0x80, 0x00,	//	MOVE >0800 TO @>3800 FROM GROM@>8000
	// HIGH RAM
	0x31, 0x10, 0x00, 0x8F, 0x1D, 0x00, 0x88, 0x00,	//	MOVE >1000 TO @>A000 FROM GROM@>8800
	0x31, 0x18, 0x00, 0x8F, 0x2D, 0x00, 0xA0, 0x00,	//	MOVE >1800 TO @>B000 FROM GROM@>A000
	0x31, 0x18, 0x00, 0x8F, 0x45, 0x00, 0xC0, 0x00,	//	MOVE >1800 TO @>C800 FROM GROM@>C000
	0x31, 0x18, 0x00, 0x8F, 0x5D, 0x00, 0xE0, 0x00,	//	MOVE >1800 TO @>E000 FROM GROM@>E000

	// I dunno, some padding ;) GPL is small at least!
	0x31, 0x10, 0x00, 0x8F, 0x1D, 0x00, 0x88, 0x00,	//	MOVE >1000 TO @>A000 FROM GROM@>8800
	0x31, 0x18, 0x00, 0x8F, 0x2D, 0x00, 0xA0, 0x00,	//	MOVE >1800 TO @>B000 FROM GROM@>A000
	0x31, 0x18, 0x00, 0x8F, 0x45, 0x00, 0xC0, 0x00	//	MOVE >1800 TO @>C800 FROM GROM@>C000

	// 12 moves total
};

// Load VDP registers - stored at hard coded address 
const unsigned char MakeGromCopy::cpyVDPRegs[] = {
	0x39, 0x00, 0x08, 0x00, 0x60, 0x26	// MOVE >0008 TO REG>00 FROM GROM@>6026  <-- GROM BANK PATCH
};

const unsigned char MakeGromCopy::Finish[] = {
	0xBF, 0x00, 0xA0, 0x00,		// DST >A000, @>8300	* Load address for XML  <--- boot address patch
	0x0F, 0xF0,					// XML >F0				* EXecute cart
	0x0B						// EXIT					* if it returns
};

// Derivative of my MakeBasicCart program for Owen, this one stores VDP memory into
// a 379 cartridge for the sake of resuming a TI BASIC program. To make this one more
// flexible, it should (after warning the user) set a breakpoint and resume execution
// until it reaches >0070 (the main GPL interpreter loop). Then we know we can safely
// resume without restoring the status register or needing a register to jump to
// an arbitrary PC. The header.bin code will need to be updated with this new
// assumption. This always saves all 16k of VRAM and prepends the loader, both
// banks should include startup code to jump to the right bank and start copying.
class MakeBasicCopy {
public:
	MakeBasicCopy(HWND in_hwnd, struct options in_opt) {
		hwnd = in_hwnd;
		opt = in_opt;
	}

	void Go() {
		// Options supported:
		// Name
		// Disable F4

		FILE *fp;
		// remember that the second bank is the usual power-up bank
		unsigned char buf1[8192], buf2[8192];
        const char *pExt;
		int ncnt, nStart;
		char *pData;
		HRSRC hRsrc;
		HGLOBAL hGlob;

		// make sure the filename ends with .BIN
        // New extensions: 9.BIN for 379/inverted, 8.BIN for non-inverted
        if (opt.bInvert) {
            pExt = "_9.BIN";
        } else {
            pExt = "_8.BIN";
        }

		if (_stricmp(&opt.FileName[strlen(opt.FileName)-5], &pExt[1])) {
			opt.FileName[250]='\0';
			strcat(opt.FileName, pExt);
			FILE *fp=fopen(opt.FileName, "r");
			if (NULL != fp) {
				fclose(fp);
				char buf[256];
				sprintf(buf, "Output file %s already exists - overwrite?", opt.FileName);
				if (IDNO == MessageBox(hwnd, buf, "Overwrite file?", MB_YESNO | MB_ICONQUESTION)) {
					return;
				}
			}
		}

		// zero out the carts (use 0xff for eprom's sake)
		memset(buf1, 0xff, 8192);
		memset(buf2, 0xff, 8192);

		// first load the header.bin
		hRsrc=FindResource(NULL, MAKEINTRESOURCE(IDR_BASICHDR), "ROMS");
		if (hRsrc) {
			int nRealLen=SizeofResource(NULL, hRsrc);
			if (nRealLen > 496) {
				MessageBox(hwnd, "Header resource too large - can not create cartridge.", "Internal error.", MB_OK | MB_ICONERROR);
				return;
			}
			hGlob=LoadResource(NULL, hRsrc);
			if (NULL != hGlob) {
				pData=(char*)LockResource(hGlob);
			}
			memcpy(buf2, pData, nRealLen);
			ncnt = nRealLen;
			// Don't need to release the locked resource
		} else {
			MessageBox(hwnd, "Failed to locate BASIC header resource - can not create cartridge.", "Internal error.", MB_OK | MB_ICONERROR);
			return;
		}

		// patch the header to load the user's name
		nStart=buf2[6]*256 + buf2[7];			// get entry address
		buf2[6]=(ncnt+0x6000)/256;
		buf2[7]=(ncnt+0x6000)%256;				// patch to the end of the header
		buf2[ncnt++]=0;					
		buf2[ncnt++]=0;							// no next entry
		buf2[ncnt++]=nStart/256;
		buf2[ncnt++]=nStart%256;				// start address
		buf2[ncnt++]=(unsigned char)strlen(opt.Name);			// length of the name
		strcpy((char*)&buf2[ncnt], opt.Name);	// copy the text over
		_strupr((char*)&buf2[ncnt]);			// make sure it's uppercase

		// before we start to copy data, we need to get the CPU to the right spot
		// execute up to 100,000 instructions to get back to >0070
		int nCntDown = 100000;
		while (pCurrentCPU->GetPC() != 0x0070) {
			cycles_left = 0;						// don't allow the main thread to run instructions
			pCurrentCPU->ExecuteOpcode(false);
			nCntDown--;
			if (nCntDown < 1) break;
		}
		if (nCntDown < 1) {
			MessageBox(hwnd, "Unable to cycle PC to start of GPL interpreter - can not save TI BASIC program. Is BASIC running?", "Internal Error", MB_OK | MB_ICONERROR);
			return;
		}

		// if we do NOT want to disable F4, we have to disable that code in the header
		// ie: this lets the user break, list, save, modify, etc.
		if (!opt.bDisableF4) {
			// so clear it - the flag is right after the standard header
			buf2[12] = 0;
			buf2[13] = 0;											// zero the value (so >0000 is written to the int hook)
		}

		// patch in the GROM address - this matters to some instructions
		buf2[14] = ((GROMBase[0].GRMADD-1)>>8)&0xff;
		buf2[15] = (GROMBase[0].GRMADD-1)&0xff;

		// patch in the size of the copy bank 2 (we'll use this again later)
		int nBank2Size = (opt.EndVDP - opt.StartVDP) - 0x1d00 + 1;		// 0x1D00 is the size of the first bank
		if (nBank2Size < 0) {
			// unlikely, but possible I guess
			nBank2Size = 1;		// copy 1 byte just to make it work
		}
		buf2[16] = (nBank2Size>>8) & 0xff;
		buf2[17] = nBank2Size & 0xff;

		// and patch the start address for the VDP load
		// Note the reversed byte order and setting of the write bit
		// This should only be 0x0000 or 0x0300
		buf2[18] = opt.StartVDP&0xff;
		buf2[19] = ((opt.StartVDP>>8)&0xff) | 0x40;

		// copy the VDP registers over the header
		// these are written in little-endian order with the register command, and VDPREG1 is last
		buf2[20] = VDPREG[0];
		buf2[22] = VDPREG[2];
		buf2[24] = VDPREG[3];
		buf2[26] = VDPREG[4];
		buf2[28] = VDPREG[5];
		buf2[30] = VDPREG[6];
		buf2[32] = VDPREG[7];
		buf2[34] = VDPREG[1];

		// load in the scratchpad dump - >8300->83FF at >6200->62FF
		for (int i=0; i<0x100; i++) {
			buf2[0x200+i] = GetSafeCpuByte(0x8300+i, 0);
		}

		// load the first part of VDP RAM
		for (int i=0; i<0x1d00; i++) {
			buf2[0x300+i] = GetSafeVDP(i+opt.StartVDP, 0);
		}
		// and read the rest of it into buffer 1
		for (int i=0; i<nBank2Size; i++) {
			buf1[i] = GetSafeVDP(i+opt.StartVDP+0x1D00, 0);
		}

		// Now write the 379 file out (just concatenates the two buffers)
		fp=fopen(opt.FileName, "wb");
		if (NULL == fp) {
			debug_write("Failed to open %s for writing, code %d", opt.FileName, errno);
			MessageBox(hwnd, "Can't open output file - unable to save cartridge.", "Disk Error", MB_OK | MB_ICONERROR);
			return;
		}
        // default now is to write non-inverted files
        if (opt.bInvert) {
    		fwrite(buf1, 1, 8192, fp);
	    	fwrite(buf2, 1, 8192, fp);
        } else {
            MessageBox(hwnd, "Warning: the cartridge will write correctly, but Classic99 can't boot it because it must start in the first bank.", "Notice", MB_OK | MB_ICONINFORMATION);
    		fwrite(buf2, 1, 8192, fp);
	    	fwrite(buf1, 1, 8192, fp);
        }
		fclose(fp);
	}

private:
	HWND hwnd;
	struct options opt;
};


// maybe the simplest one - just dumps the 8k of cart rom space as a binary file
class MakeCartROM {
public:
	MakeCartROM(HWND in_hwnd, struct options in_opt) {
		hwnd = in_hwnd;
		opt = in_opt;
	}

	void Go() {
		//  data range is stored in HighStart to HighEnd
		//  (but is expected to be >6000 to >7FFF). No other
		//  options are valid.
		char buf[8192];

		FILE *fp=fopen(opt.FileName, "wb");
		if (NULL == fp) {
			// couldn't open the file
			debug_write("Can't open %s for writing, errno %d", opt.FileName, errno);
			MessageBox(hwnd, "An error occurred writing cartridge file, see debug for details.", "Error occurred.", MB_OK | MB_ICONERROR);
		} else {
			debug_write("Writing cartridge ROM file (bank 0 only) %s", opt.FileName);
		}
		// we have to read it into a buffer... ehhh. oh well.
		for (Word idx = opt.StartHigh; idx<=opt.EndHigh; idx++) {
			int pos = idx - opt.StartHigh;
			if (pos >= 8192) break;
			buf[pos] = ReadMemoryByte(idx, 0);
		}
		fwrite(buf, 1, 8192, fp);
		fclose(fp);
	}

private:
	HWND hwnd;
	struct options opt;
};


void SetDefaultEnables(HWND hwnd) {
	SendDlgItemMessage(hwnd, IDC_CHKHIGHRAM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKHIGHRAM, TRUE);
	EnableDlgItem(hwnd, IDC_HIGHSTART, FALSE);
	EnableDlgItem(hwnd, IDC_HIGHEND, FALSE);
	SendDlgItemMessage(hwnd, IDC_CHKCARTMEM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKCARTMEM, FALSE);
	EnableDlgItem(hwnd, IDC_MIDSTART, FALSE);
	EnableDlgItem(hwnd, IDC_MIDEND, FALSE);
	SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKLOWRAM, TRUE);
	EnableDlgItem(hwnd, IDC_LOWSTART, FALSE);
	EnableDlgItem(hwnd, IDC_LOWEND, FALSE);
	SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKVDPRAM, TRUE);
	EnableDlgItem(hwnd, IDC_VDPSTART, FALSE);
	EnableDlgItem(hwnd, IDC_VDPEND, FALSE);
	EnableDlgItem(hwnd, IDC_CHKCHARA1, FALSE);
	EnableDlgItem(hwnd, IDC_KEYBOARD, TRUE);
	EnableDlgItem(hwnd, IDC_GROM8K, FALSE);
	EnableDlgItem(hwnd, IDC_DISABLEF4, FALSE);
	EnableDlgItem(hwnd, IDC_LSTFIRSTGROM, FALSE);
	EnableDlgItem(hwnd, IDC_BTNREADDEFS, FALSE);
	EnableDlgItem(hwnd, IDC_VDPREGS, TRUE);

	EnableDlgItem(hwnd, IDC_NAME, TRUE);
	EnableDlgItem(hwnd, IDC_BOOT, TRUE);
	EnableDlgItem(hwnd, IDC_CHKEA, TRUE);
	EnableDlgItem(hwnd, IDC_CHKCHARSET, TRUE);
	EnableDlgItem(hwnd, IDC_MODIFIEDHIGH, TRUE);
	EnableDlgItem(hwnd, IDC_MODIFIEDLOW, TRUE);
	EnableDlgItem(hwnd, IDC_MODIFIEDVRAM, TRUE);
    EnableDlgItem(hwnd, IDC_INVERTBANKS, TRUE);

	SendDlgItemMessage(hwnd, IDC_CHKCHARSET, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(hwnd, IDC_CHKCHARA1, BM_SETCHECK, BST_UNCHECKED, 0);
}

void DisableAll(HWND hwnd) {
	SendDlgItemMessage(hwnd, IDC_CHKHIGHRAM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKHIGHRAM, FALSE);
	EnableDlgItem(hwnd, IDC_HIGHSTART, FALSE);
	EnableDlgItem(hwnd, IDC_HIGHEND, FALSE);
	SendDlgItemMessage(hwnd, IDC_CHKCARTMEM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKCARTMEM, FALSE);
	EnableDlgItem(hwnd, IDC_MIDSTART, FALSE);
	EnableDlgItem(hwnd, IDC_MIDEND, FALSE);
	SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKLOWRAM, FALSE);
	EnableDlgItem(hwnd, IDC_LOWSTART, FALSE);
	EnableDlgItem(hwnd, IDC_LOWEND, FALSE);
	SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_SETCHECK, BST_UNCHECKED, 0);
	EnableDlgItem(hwnd, IDC_CHKVDPRAM, FALSE);
	EnableDlgItem(hwnd, IDC_VDPSTART, FALSE);
	EnableDlgItem(hwnd, IDC_VDPEND, FALSE);
	EnableDlgItem(hwnd, IDC_CHKCHARA1, FALSE);
	EnableDlgItem(hwnd, IDC_KEYBOARD, FALSE);
	EnableDlgItem(hwnd, IDC_GROM8K, FALSE);
	EnableDlgItem(hwnd, IDC_DISABLEF4, FALSE);
	EnableDlgItem(hwnd, IDC_LSTFIRSTGROM, FALSE);
	EnableDlgItem(hwnd, IDC_BTNREADDEFS, FALSE);
	EnableDlgItem(hwnd, IDC_VDPREGS, FALSE);

	EnableDlgItem(hwnd, IDC_NAME, FALSE);
	EnableDlgItem(hwnd, IDC_BOOT, FALSE);
	EnableDlgItem(hwnd, IDC_CHKEA, FALSE);
	EnableDlgItem(hwnd, IDC_CHKCHARSET, FALSE);
	EnableDlgItem(hwnd, IDC_MODIFIEDHIGH, FALSE);
	EnableDlgItem(hwnd, IDC_MODIFIEDLOW, FALSE);
	EnableDlgItem(hwnd, IDC_MODIFIEDVRAM, FALSE);
    EnableDlgItem(hwnd, IDC_INVERTBANKS, FALSE);

	SendDlgItemMessage(hwnd, IDC_CHKCHARSET, BM_SETCHECK, BST_UNCHECKED, 0);
	SendDlgItemMessage(hwnd, IDC_CHKCHARA1, BM_SETCHECK, BST_UNCHECKED, 0);
}

bool LooksLikeC99(int ad) {
	// this pattern sets up the GPL copy loop in all versions of C99PFI,
	// though it is located at various points. Later versions may not
	// require this hack.
	unsigned char pattern[] = {
		0x02,0x00,0x70,0xe8,0x02,0x01,0x20,0xfa,0x02,0x02,0x02,0xc2
	};

	if ((GetSafeCpuByte(ad, 0) == 0xc8) &&		// mov R11,@>xxxx
		(GetSafeCpuByte(ad+1, 0) == 0x0b) &&
		(GetSafeCpuByte(ad+4, 0) == 0x02) &&	// lwpi >xxxx
		(GetSafeCpuByte(ad+5, 0) == 0xe0)) {
			// search a little further for a pattern match
			for (int i=8; i<100; i++) {
				unsigned char buf[12];
				for (int j=0; j<12; j++) {
					buf[j]=GetSafeCpuByte(ad+i+j, 0);
				}
				if (0 == memcmp(buf, pattern, 12)) {
					return true;
				}
			}
	}
	return false;
}

BOOL CALLBACK CartDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char buf[256];

    switch (uMsg) 
    { 
        case WM_COMMAND: 
			if (HIWORD(wParam) == BN_CLICKED) {
				// button/check box clicked
				switch (LOWORD(wParam)) 
				{ 
					case IDC_MODIFIEDHIGH:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKHIGHRAM, BM_GETCHECK, 0, 0)) {
							int nFirst=0xa000, nLast=0xa000;
							for (int i = 0xa000; i <= 0xffff; i++) {
								if (CPUMemInited[i]) {
									nFirst = i&0xFFFE;		// make it even
									break;
								}
							}
							for (int i = 0xffff; i >= 0xa000; i--) {
								if (CPUMemInited[i]) {
									nLast = i|0x01;		// make it odd (because it's inclusive)
									break;
								}
							}
							sprintf(buf, "%04X", nFirst);
							SendDlgItemMessage(hwnd, IDC_HIGHSTART, WM_SETTEXT, 0, (LPARAM)buf);
							sprintf(buf, "%04X", nLast);
							SendDlgItemMessage(hwnd, IDC_HIGHEND, WM_SETTEXT, 0, (LPARAM)buf);
						}
						break;

                    case IDC_MODIFIEDLOW:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_GETCHECK, 0, 0)) {
							int nFirst=0x2000, nLast=0x2000;
							for (int i = 0x2000; i <= 0x3fff; i++) {
								if (CPUMemInited[i]) {
									nFirst = i&0xFFFE;		// make it even
									break;
								}
							}
							for (int i = 0x3fff; i >= 0x2000; i--) {
								if (CPUMemInited[i]) {
									nLast = i|0x01;		// make it odd (because it's inclusive)
									break;
								}
							}
							sprintf(buf, "%04X", nFirst);
							SendDlgItemMessage(hwnd, IDC_LOWSTART, WM_SETTEXT, 0, (LPARAM)buf);
							sprintf(buf, "%04X", nLast);
							SendDlgItemMessage(hwnd, IDC_LOWEND, WM_SETTEXT, 0, (LPARAM)buf);
						}
						break;

					case IDC_MODIFIEDVRAM:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_GETCHECK, 0, 0)) {
							int nFirst=0x0000, nLast=0x0000;
							for (int i = 0x0000; i <= 0x3fff; i++) {
								if (VDPMemInited[i]) {
									nFirst = i;
									break;
								}
							}
							for (int i = 0x3fff; i >= 0x0000; i--) {
								if (VDPMemInited[i]) {
									nLast = i;
									break;
								}
							}
							sprintf(buf, "%04X", nFirst);
							SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_SETTEXT, 0, (LPARAM)buf);
							sprintf(buf, "%04X", nLast);
							SendDlgItemMessage(hwnd, IDC_VDPEND, WM_SETTEXT, 0, (LPARAM)buf);
						}
						break;

					case IDC_BUILD:
						{
							// first, sanity check the address ranges
							int x;
							struct options opt;
							
							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKHIGHRAM, BM_GETCHECK, 0, 0)) {
								SendDlgItemMessage(hwnd, IDC_HIGHSTART, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0xa000)||(x>0xffff)) {
									MessageBox(hwnd, "High RAM addresses must be in the range A000-FFFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								if (x&0x01) {
									MessageBox(hwnd, "High RAM start addresses must be even", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.StartHigh = x;

								SendDlgItemMessage(hwnd, IDC_HIGHEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0xa000)||(x>0xffff)) {
									MessageBox(hwnd, "High RAM addresses must be in the range A000-FFFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								if (!(x&0x01)) {
									MessageBox(hwnd, "High RAM end addresses must be odd", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.EndHigh = x;

								if (opt.EndHigh < opt.StartHigh) {
									MessageBox(hwnd, "High RAM addresses must be specified as low to high", "Bad Order", MB_OK | MB_ICONSTOP);
									break;
								}
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKCARTMEM, BM_GETCHECK, 0, 0)) {
								SendDlgItemMessage(hwnd, IDC_MIDSTART, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0x4000)||(x>0x7fff)) {
									MessageBox(hwnd, "Cart mem addresses must be in the range 4000-7FFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								if (x&0x01) {
									MessageBox(hwnd, "Cart mem start addresses must be even", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.StartMid = x;

								SendDlgItemMessage(hwnd, IDC_MIDEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0x4000)||(x>0x7fff)) {
									MessageBox(hwnd, "Cart mem addresses must be in the range 4000-7FFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								if (!(x&0x01)) {
									MessageBox(hwnd, "Cart mem end addresses must be odd", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.EndMid = x;

								if (opt.EndMid < opt.StartMid) {
									MessageBox(hwnd, "Cart mem addresses must be specified as low to high", "Bad Order", MB_OK | MB_ICONSTOP);
									break;
								}
							}

                            if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_GETCHECK, 0, 0)) {
								SendDlgItemMessage(hwnd, IDC_LOWSTART, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0x2000)||(x>0x3fff)) {
									MessageBox(hwnd, "Low RAM addresses must be in the range 2000-3FFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								if (x&0x01) {
									MessageBox(hwnd, "Low RAM start addresses must be even", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.StartLow = x;

								SendDlgItemMessage(hwnd, IDC_LOWEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0x2000)||(x>0x3fff)) {
									MessageBox(hwnd, "Low RAM addresses must be in the range 2000-3FFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								if (!(x&0x01)) {
									MessageBox(hwnd, "Low RAM end addresses must be odd", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.EndLow = x;

								if (opt.EndLow < opt.StartLow) {
									MessageBox(hwnd, "Low RAM addresses must be specified as low to high", "Bad Order", MB_OK | MB_ICONSTOP);
									break;
								}
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_GETCHECK, 0, 0)) {
								SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0x0000)||(x>0x3fff)) {
									MessageBox(hwnd, "VDP RAM addresses must be in the range 0000-3FFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.StartVDP = x;

								SendDlgItemMessage(hwnd, IDC_VDPEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if ((x<0x0000)||(x>0x3fff)) {
									MessageBox(hwnd, "VDP RAM addresses must be in the range 0000-3FFF", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
								opt.EndVDP = x;

								if (opt.EndVDP < opt.StartVDP) {
									MessageBox(hwnd, "VDP RAM addresses must be specified as low to high", "Bad Order", MB_OK | MB_ICONSTOP);
									break;
								}
							}

							// get save type
							int nOption = SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_GETCURSEL, 0, 0);

							// check cartridge
							if (nOption == 4) {
								opt.StartHigh = 0x6000;
								opt.EndHigh = 0x7FFF;
							}

							if ((opt.EndHigh == 0) && (opt.EndMid ==0) && (opt.EndLow == 0) && (opt.EndVDP == 0)) {
								MessageBox(hwnd, "No memory ranges selected for saving.", "Inconsistency", MB_OK | MB_ICONSTOP);
								break;
							}

							SendDlgItemMessage(hwnd, IDC_BOOT, WM_GETTEXT, 256, (LPARAM)buf);
							x = 0;
							sscanf(buf, "%x", &x);
							opt.Boot = 0;
							if ((opt.StartHigh!=0)&&(x>=opt.StartHigh)&&(x<=opt.EndHigh)) {
								opt.Boot = x;
							}
							if ((opt.StartMid!=0)&&(x>=opt.StartMid)&&(x<=opt.EndMid)) {
								opt.Boot = x;
							}
							if ((opt.StartLow!=0)&&(x>=opt.StartLow)&&(x<=opt.EndLow)) {
								opt.Boot = x;
							}
							if (opt.Boot == 0) {
								if ((0 == nOption) && (opt.EndVDP != 0)) {
									// allow saving E/A format VDP memory without a program
									if (IDNO == MessageBox(hwnd, "Only VDP memory selected for saving, no program. Continue?", "Inconsistency", MB_YESNO | MB_ICONQUESTION)) {
										break;
									}
								} else if (nOption == 3) {
									// TI BASIC has a fixed boot address
									opt.Boot = x;
								} else if ((x==0) && ((nOption == 4)||(nOption == 5))) {
									// cartridge ROMs have their own header, user is responsible for this being right
                                    // EA#3 doesn't have to specify a boot address
                                } else {
									MessageBox(hwnd, "Boot address must be in one of the saved memory ranges.", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
							}
                            if (nOption == 5) {
                                // EA#3: nothing here either - boot address is legal but may be disabled
							}
							buf[0]='\0';
							// only E/A and cart rom don't need a cartridge name
							if ((0 != nOption) && (4 != nOption) && (5 != nOption)) {
								SendDlgItemMessage(hwnd, IDC_NAME, WM_GETTEXT, 256, (LPARAM)buf);
								buf[255]='\0';
								if ((strlen(buf) < 1) || (strlen(buf) > 20)) {
									MessageBox(hwnd, "Cartridge Name must be between 1 and 20 characters", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
								}
							}
							strcpy(opt.Name, buf);
							_strupr(opt.Name);

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKEA, BM_GETCHECK, 0, 0)) {
								if ((opt.StartLow > 0x2000) || (opt.EndLow < 0x2705)) {
									if (IDNO == MessageBox(hwnd, "Warning: You have asked for the E/A routines, but are not saving some or all of the range >2000 - >2705. Continue anyway?", "Inconsistency", MB_YESNO | MB_ICONQUESTION)) {
										break;
									}
								}
								opt.bEA = true;
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKCHARSET, BM_GETCHECK, 0, 0)) {
								if ((0 == nOption) ||
									(BST_CHECKED == SendDlgItemMessage(hwnd, IDC_CHKCHARA1, BM_GETCHECK, 0, 0))) {
									// this is a VDP memory copy
									int PDT=((VDPREG[4]&0x07)<<11) + 0xF0;	// start at char 30

									if ((opt.StartVDP > PDT) || (opt.EndVDP < PDT+0x310-1)) {
										if (IDNO == MessageBox(hwnd, "Warning: You have asked for a character set copy, but are not saving some or all of the relevant PDT. Continue anyway?", "Inconsistency", MB_YESNO | MB_ICONQUESTION)) {
											break;
										}
									}
									opt.bCharA1 = true;
								}
								opt.bCharSet = true;
							}
							
							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_KEYBOARD, BM_GETCHECK, 0, 0)) {
								opt.bKeyboard = true;
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_GROM8K, BM_GETCHECK, 0, 0)) {
								opt.bGROM8K = true;
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_DISABLEF4, BM_GETCHECK, 0, 0)) {
								opt.bDisableF4 = true;
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_VDPREGS, BM_GETCHECK, 0, 0)) {
								opt.bVDPRegs = true;
							}

							if (BST_CHECKED == SendDlgItemMessage(hwnd, IDC_INVERTBANKS, BM_GETCHECK, 0, 0)) {
								opt.bInvert = true;
							}

							opt.FirstGROM = SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_GETCURSEL, 0, 0);

							// special check for GROM 0
							if (2 == nOption) {
								// warn the user if anything is weird here
								if (opt.FirstGROM == 0) {
									if (IDNO == MessageBox(hwnd, "Warning: GROM 0 is the operating system. Replacing this will almost never work. Continue anyway?", "Warning", MB_YESNO | MB_ICONQUESTION)) {
										break;
									}
								}
							}

							// check whether this looks like a c99 program, and if so, check for E/A utilities
							// double check that the user knows what he is doing.
							if ((LooksLikeC99(opt.Boot)) && (!opt.bEA)) {
								if (IDNO == MessageBox(hwnd, "This looks like a c99 program startup, and the Editor/Assembler functions are not loaded. c99 usually needs these to run - continue anyway?", "Are you sure?", MB_YESNO | MB_ICONQUESTION)) {
									break;
								}
							}

							// sanity test looks okay, get a save filename
							OPENFILENAME ofn;

							memset(&ofn, 0, sizeof(OPENFILENAME));
							ofn.lStructSize=sizeof(OPENFILENAME);
							ofn.hwndOwner=hwnd;
							ofn.lpstrFilter="Save Files\0*.*\0\0";
							ofn.lpstrFile = opt.FileName;
							ofn.nMaxFile=256;
							ofn.Flags=OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;

							char szTmpDir[MAX_PATH];
							GetCurrentDirectory(MAX_PATH, szTmpDir);
							BOOL bRet = GetSaveFileName(&ofn);
							SetCurrentDirectory(szTmpDir);

							if (!bRet) {
								break;
							}
							
							// call the appropriate build function
							switch (nOption) {
								case 0:		// E/A #5
									{
										MakeEA5Class cEA5(hwnd, opt);;
										cEA5.Go();
									}
									break;

								case 1:		// 379 copy
									{
										Make379Copy c379(hwnd, opt);
										c379.Go();
									}
									break;

								case 2:		// GROM copy
									{
										MakeGromCopy cGrom(hwnd, opt);
										cGrom.Go();
									}
									break;

								case 3:		// TI BASIC 
									{
										MakeBasicCopy cBasic(hwnd, opt);
										cBasic.Go();
									}
									break;

								case 4:		// cartridge ROM dump
									{
										MakeCartROM cROM(hwnd, opt);
										cROM.Go();
									}
									break;

								case 5:		// E/A #3
									{
										MakeEA3Class cEA3(hwnd, opt);;
										cEA3.Go();
									}
									break;

								default:
									MessageBox(hwnd, "Memory save type not selected.", "Out of Range", MB_OK | MB_ICONSTOP);
									break;
							}

							// and done!
							return TRUE; 
						}
						break;

					case IDC_BTNREADDEFS:
						{
							bool bFirst = false, bLast = false, bLoad = false;

							// look for an E/A REF/DEF table, and pull out SLOAD,SFIRST,SLAST
							int nPos = GetSafeCpuWord(0x202a, 0);
							if ((nPos < 0x3000) || (nPos > 0x3FFF)) {
								MessageBox(hwnd, "No valid REF/DEF table found.", "Cannot read labels", MB_OK | MB_ICONEXCLAMATION);
							} else {
								// turn off options since we are overriding the load data
								SetDefaultEnables(hwnd);
								EnableDlgItem(hwnd, IDC_BTNREADDEFS, TRUE);

								while (nPos <= 0x3ff8) {
									unsigned char buf2[8];
									buf2[0] = GetSafeCpuByte(nPos, 0);
									buf2[1] = GetSafeCpuByte(nPos+1, 0);
									buf2[2] = GetSafeCpuByte(nPos+2, 0);
									buf2[3] = GetSafeCpuByte(nPos+3, 0);
									buf2[4] = GetSafeCpuByte(nPos+4, 0);
									buf2[5] = GetSafeCpuByte(nPos+5, 0);
									buf2[6] = GetSafeCpuByte(nPos+6, 0);
									buf2[7] = GetSafeCpuByte(nPos+7, 0);
									nPos+=8;
									if (memcmp(buf2, "SFIRST", 6) == 0) {
										bFirst = true;
										sprintf(buf, "%04X", buf2[6]*256 + buf2[7]);
										SendDlgItemMessage(hwnd, IDC_BOOT, WM_SETTEXT, 0, (LPARAM)buf);
									} else if (memcmp(buf2, "SLOAD ", 6) == 0) {
										bLoad = true;
										sprintf(buf, "%04X", buf2[6]*256 + buf2[7]);
										if (buf2[6]*256 + buf2[7] >= 0xa000) {
											// high RAM
											SendDlgItemMessage(hwnd, IDC_CHKHIGHRAM, BM_SETCHECK, BST_CHECKED, 0);
											EnableDlgItem(hwnd, IDC_HIGHSTART, TRUE);
											EnableDlgItem(hwnd, IDC_HIGHEND, TRUE);
											SendDlgItemMessage(hwnd, IDC_HIGHSTART, WM_SETTEXT, 0, (LPARAM)buf);
										} else {
											// low RAM
											SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_SETCHECK, BST_CHECKED, 0);
											EnableDlgItem(hwnd, IDC_LOWSTART, TRUE);
											EnableDlgItem(hwnd, IDC_LOWEND, TRUE);
											SendDlgItemMessage(hwnd, IDC_LOWSTART, WM_SETTEXT, 0, (LPARAM)buf);
										}
									} else if (memcmp(buf2, "SLAST ", 6) == 0) {
										bLast = true;
										sprintf(buf, "%04X", buf2[6]*256 + buf2[7] - 1);
										if (buf2[6]*256 + buf2[7] - 1 >= 0xa000) {
											// high RAM
											SendDlgItemMessage(hwnd, IDC_CHKHIGHRAM, BM_SETCHECK, BST_CHECKED, 0);
											EnableDlgItem(hwnd, IDC_HIGHSTART, TRUE);
											EnableDlgItem(hwnd, IDC_HIGHEND, TRUE);
											SendDlgItemMessage(hwnd, IDC_HIGHEND, WM_SETTEXT, 0, (LPARAM)buf);
										} else {
											// low RAM
											SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_SETCHECK, BST_CHECKED, 0);
											EnableDlgItem(hwnd, IDC_LOWSTART, TRUE);
											EnableDlgItem(hwnd, IDC_LOWEND, TRUE);
											SendDlgItemMessage(hwnd, IDC_LOWEND, WM_SETTEXT, 0, (LPARAM)buf);
										}
									}
								}

								if (!bLoad || !bFirst || !bLast) {
									sprintf(buf, "Failed to locate DEFs: %s%s%s", !bLoad?"SLOAD ":"", !bFirst?"SFIRST ":"", !bLast?"SLAST ":"");
									MessageBox(hwnd, buf, "Unresolved references", MB_OK | MB_ICONASTERISK);
								}
							}
						}
						break;

					case IDC_CHKHIGHRAM:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							EnableDlgItem(hwnd, IDC_HIGHSTART, TRUE);
							EnableDlgItem(hwnd, IDC_HIGHEND, TRUE);
						} else {
							EnableDlgItem(hwnd, IDC_HIGHSTART, FALSE);
							EnableDlgItem(hwnd, IDC_HIGHEND, FALSE);
						}
						break;

					case IDC_CHKCARTMEM:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							EnableDlgItem(hwnd, IDC_MIDSTART, TRUE);
							EnableDlgItem(hwnd, IDC_MIDEND, TRUE);
						} else {
							EnableDlgItem(hwnd, IDC_MIDSTART, FALSE);
							EnableDlgItem(hwnd, IDC_MIDEND, FALSE);
						}
						break;

                    case IDC_CHKLOWRAM:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							EnableDlgItem(hwnd, IDC_LOWSTART, TRUE);
							EnableDlgItem(hwnd, IDC_LOWEND, TRUE);
						} else {
							EnableDlgItem(hwnd, IDC_LOWSTART, FALSE);
							EnableDlgItem(hwnd, IDC_LOWEND, FALSE);
						}
						break;

					case IDC_CHKVDPRAM:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							EnableDlgItem(hwnd, IDC_VDPSTART, TRUE);
							EnableDlgItem(hwnd, IDC_VDPEND, TRUE);
						} else {
							EnableDlgItem(hwnd, IDC_VDPSTART, FALSE);
							EnableDlgItem(hwnd, IDC_VDPEND, FALSE);
						}
						break;

                    case IDC_INVERTBANKS:
                        // no changes needed
                        break;

					case IDC_CHKEA:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							if (IDYES == MessageBox(hwnd, "This will overwrite low RAM from >2000 to >2705 - continue?", "Change RAM?", MB_YESNO | MB_ICONQUESTION)) {
								char *pData;
								HRSRC hRsrc;
								HGLOBAL hGlob;
								
								hRsrc=FindResource(NULL, MAKEINTRESOURCE(IDR_EAUTILS), "ROMS");
								if (hRsrc) {
									int nRealLen=SizeofResource(NULL, hRsrc);
									if (nRealLen > 0x706) nRealLen = 0x706;
									hGlob=LoadResource(NULL, hRsrc);
									if (NULL != hGlob) {
										pData=(char*)LockResource(hGlob);
									}
									for (int i=0; i<nRealLen; i++) {
										WriteMemoryByte(0x2000+i, *(pData+i), false);
									}
									// Don't need to release the locked resource
								} else {
									MessageBox(hwnd, "Failed to locate E/A Utilities resource - can not load data.", "Internal error.", MB_OK | MB_ICONERROR);
								}

								// make sure that the low memory is enabled at the appropriate range
								if (BST_CHECKED != SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_GETCHECK, 0, 0)) {
									SendDlgItemMessage(hwnd, IDC_CHKLOWRAM, BM_SETCHECK, TRUE, 0);
									EnableDlgItem(hwnd, IDC_LOWSTART, TRUE);
									EnableDlgItem(hwnd, IDC_LOWEND, TRUE);
								}
								SendDlgItemMessage(hwnd, IDC_LOWSTART, WM_GETTEXT, 256, (LPARAM)buf);
								int x = 0;
								sscanf(buf, "%x", &x);
								if (x != 0x2000) {
									SendDlgItemMessage(hwnd, IDC_LOWSTART, WM_SETTEXT, 0, (LPARAM)"2000");
								}
								SendDlgItemMessage(hwnd, IDC_LOWEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if (x < 0x2705) {
									SendDlgItemMessage(hwnd, IDC_LOWEND, WM_SETTEXT, 0, (LPARAM)"2705");
								}

								SendDlgItemMessage(hwnd, IDC_BOOT, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);

								if (LooksLikeC99(x)) {
									if (IDYES == MessageBox(hwnd, "This looks like a c99 program startup. Some c99 programs will not start without a true E/A cartridge, but will still run if the E/A copy is skipped. You should only say yes to this if the program does not work when you say no. Try to skip the E/A copy loop?", "Patch c99?", MB_YESNO | MB_ICONQUESTION)) {
										for (int i=0; i<100; i+=2) {
											// find the initial branch to the main code
											if ((GetSafeCpuByte(x+i,0) == 0x04) && (GetSafeCpuByte(x+i+1,0) == 0x60)) {
												// this should be it
												WriteMemoryByte(x, 0x04, false);
												WriteMemoryByte(x+1, 0x60, false);
												WriteMemoryByte(x+2, ((x+i)>>8)&0xff, false);
												WriteMemoryByte(x+3, (x+i)&0xff, false);
												MessageBox(hwnd, "Program successfully patched.", "Success", MB_OK);
												break;
											} else if (i >= 98) {
												MessageBox(hwnd, "Failed to locate program start point - unable to patch.", "Could not patch", MB_OK);
											}
										}
									}
								}
							}
						}
						break;

					case IDC_CHKCHARSET:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							EnableDlgItem(hwnd, IDC_CHKCHARA1, TRUE);
							// for Editor/Assembler mode only, we need to set up the VDP pointers same as for CHARA1
							if (0 == SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_GETCURSEL, 0, 0)) {
								int PDT=((VDPREG[4]&0x07)<<11) + 0xF0;	// start at char 30

								if (BST_CHECKED != SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_GETCHECK, 0, 0)) {
									SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_SETCHECK, TRUE, 0);
									EnableDlgItem(hwnd, IDC_VDPSTART, TRUE);
									EnableDlgItem(hwnd, IDC_VDPEND, TRUE);
								}
								SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_GETTEXT, 256, (LPARAM)buf);
								int x = 0;
								sscanf(buf, "%x", &x);
								if ((x > PDT) || (x == 0)) {
									sprintf(buf, "%04X", PDT);
									SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_SETTEXT, 0, (LPARAM)buf);
								}
								SendDlgItemMessage(hwnd, IDC_VDPEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if (x < PDT+0x0310) {
									if (PDT+0x0310 > 0x3FFF) {
										sprintf(buf, "3FFF");
									} else {
										sprintf(buf, "%04X", PDT+0x0310-1);
									}
									SendDlgItemMessage(hwnd, IDC_VDPEND, WM_SETTEXT, 0, (LPARAM)buf);
								}
							}
						} else {
							EnableDlgItem(hwnd, IDC_CHKCHARA1, FALSE);
						}
						break;

					case IDC_CHKCHARA1:
						if (BST_CHECKED == SendDlgItemMessage(hwnd, LOWORD(wParam), BM_GETCHECK, 0, 0)) {
							if (IDYES == MessageBox(hwnd, "This will overwrite the character set in VRAM - continue?", "Change VDP?", MB_YESNO | MB_ICONQUESTION)) {
								char *pData;
								HRSRC hRsrc;
								HGLOBAL hGlob;
								int PDT=((VDPREG[4]&0x07)<<11) + 0xF0;	// start at char 30
								
								// For BASIC mode, we need to offset the start point by >60 characters
								if (3 == SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_GETCURSEL, 0, 0)) {
									PDT+=0x60*8;
								}
		
								hRsrc=FindResource(NULL, MAKEINTRESOURCE(IDR_CHARA1), "ROMS");
								if (hRsrc) {
									int nRealLen=SizeofResource(NULL, hRsrc);
									if (nRealLen > 0x310) nRealLen = 0x310;
									hGlob=LoadResource(NULL, hRsrc);
									if (NULL != hGlob) {
										pData=(char*)LockResource(hGlob);
									}
									// get the address of the pattern table
									if (PDT >= 0x4000-0x310) nRealLen = 0x4000-PDT;
									memcpy(&VDP[PDT], pData, nRealLen);
									// Don't need to release the locked resource
								} else {
									MessageBox(hwnd, "Failed to locate CharA1 resource - can not load data.", "Internal error.", MB_OK | MB_ICONERROR);
								}

								// now make sure that the VDP area is active in the copy
								if (BST_CHECKED != SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_GETCHECK, 0, 0)) {
									SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_SETCHECK, TRUE, 0);
									EnableDlgItem(hwnd, IDC_VDPSTART, TRUE);
									EnableDlgItem(hwnd, IDC_VDPEND, TRUE);
								}
								SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_GETTEXT, 256, (LPARAM)buf);
								int x = 0;
								sscanf(buf, "%x", &x);
								if ((x > PDT) || (x == 0)) {
									sprintf(buf, "%04X", PDT);
									SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_SETTEXT, 0, (LPARAM)buf);
								}
								SendDlgItemMessage(hwnd, IDC_VDPEND, WM_GETTEXT, 256, (LPARAM)buf);
								x = 0;
								sscanf(buf, "%x", &x);
								if (x < PDT+0x310) {
									if (PDT+0x310 >= 0x4000) {
										sprintf(buf, "3FFF");
									} else {
										sprintf(buf, "%04X", PDT+0x310-1);
									}
									SendDlgItemMessage(hwnd, IDC_VDPEND, WM_SETTEXT, 0, (LPARAM)buf);
								}
							} else {
								SendDlgItemMessage(hwnd, IDC_CHKCHARA1, BM_SETCHECK, FALSE, 0);
							}
						}
						break;

					case IDCANCEL: 
						gDisableDebugKeys = false;
						EndDialog(hwnd, wParam); 
						return TRUE; 
				} 
			} else if (HIWORD(wParam) == CBN_SELCHANGE) {
				if (LOWORD(wParam) == IDC_LSTSAVETYPE) {

					int nIdx = SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_GETCURSEL, 0, 0);
					switch (nIdx) {
						case 0:		// enable/disable for E/A#5
							SetDefaultEnables(hwnd);

                            EnableDlgItem(hwnd, IDC_CHKCARTMEM, TRUE);
                            EnableDlgItem(hwnd, IDC_VDPREGS, FALSE);
							EnableDlgItem(hwnd, IDC_NAME, FALSE);
							EnableDlgItem(hwnd, IDC_KEYBOARD, FALSE);
							EnableDlgItem(hwnd, IDC_GROM8K, FALSE);
							EnableDlgItem(hwnd, IDC_DISABLEF4, FALSE);
							EnableDlgItem(hwnd, IDC_LSTFIRSTGROM, FALSE);
							EnableDlgItem(hwnd, IDC_BTNREADDEFS, TRUE);
							EnableDlgItem(hwnd, IDC_INVERTBANKS, FALSE);
							break;

						case 1:		// enable/disable for 379
							SetDefaultEnables(hwnd);

                            EnableDlgItem(hwnd, IDC_VDPREGS, TRUE);
							EnableDlgItem(hwnd, IDC_NAME, TRUE);
							EnableDlgItem(hwnd, IDC_KEYBOARD, TRUE);
							EnableDlgItem(hwnd, IDC_GROM8K, FALSE);
							EnableDlgItem(hwnd, IDC_DISABLEF4, FALSE);
							EnableDlgItem(hwnd, IDC_LSTFIRSTGROM, FALSE);
							SendDlgItemMessage(hwnd, IDC_KEYBOARD, BM_SETCHECK, BST_CHECKED, 0);
							EnableDlgItem(hwnd, IDC_BTNREADDEFS, FALSE);
							EnableDlgItem(hwnd, IDC_INVERTBANKS, TRUE);
							break;

						case 2:		// enable/disable for GROM
							SetDefaultEnables(hwnd);

                            EnableDlgItem(hwnd, IDC_VDPREGS, TRUE);
							EnableDlgItem(hwnd, IDC_NAME, TRUE);
							EnableDlgItem(hwnd, IDC_KEYBOARD, TRUE);
							EnableDlgItem(hwnd, IDC_GROM8K, TRUE);
							EnableDlgItem(hwnd, IDC_DISABLEF4, FALSE);
							EnableDlgItem(hwnd, IDC_LSTFIRSTGROM, TRUE);
							SendDlgItemMessage(hwnd, IDC_KEYBOARD, BM_SETCHECK, BST_CHECKED, 0);
							EnableDlgItem(hwnd, IDC_BTNREADDEFS, FALSE);
							EnableDlgItem(hwnd, IDC_INVERTBANKS, FALSE);
							break;

						case 3:		// enable/disable for BASIC 379
							{
								DisableAll(hwnd);
								EnableDlgItem(hwnd, IDC_NAME, TRUE);
								EnableDlgItem(hwnd, IDC_CHKCHARA1, TRUE);
								EnableDlgItem(hwnd, IDC_DISABLEF4, TRUE);
								SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, IDC_VDPREGS, BST_CHECKED, 0);
								SendDlgItemMessage(hwnd, IDC_BOOT, WM_SETTEXT, 0, (LPARAM)"0070");
								SendDlgItemMessage(hwnd, IDC_CHKVDPRAM, BM_SETCHECK, BST_CHECKED, 0);
    							EnableDlgItem(hwnd, IDC_INVERTBANKS, TRUE);
								int nTop = GetSafeCpuByte(0x8370, 0) * 256 + GetSafeCpuByte(0x8371, 0);
								if (nTop != 0x37d7) {
									char buf[128];
									if (nTop > 0x3fff-768) {
										MessageBox(hwnd, "Warning: TI BASIC programs not using CALL FILES(3) may crash if disk is accessed. In addition, this program is too large to save the initial screen, so you should breakpoint before the first information is displayed.", "Warning", MB_OK | MB_ICONINFORMATION);
										sprintf(buf, "%04X", nTop);
										SendDlgItemMessage(hwnd, IDC_VDPEND, WM_SETTEXT, 0, (LPARAM)buf);
										SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_SETTEXT, 0, (LPARAM)"0300");
									} else {
										MessageBox(hwnd, "Warning: TI BASIC programs not using CALL FILES(3) may crash if disk is accessed.", "Warning", MB_OK | MB_ICONINFORMATION);
										sprintf(buf, "%04X", nTop);
										SendDlgItemMessage(hwnd, IDC_VDPEND, WM_SETTEXT, 0, (LPARAM)buf);
										SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_SETTEXT, 0, (LPARAM)"0000");
									}
								} else {
									SendDlgItemMessage(hwnd, IDC_VDPEND, WM_SETTEXT, 0, (LPARAM)"37D7");
									SendDlgItemMessage(hwnd, IDC_VDPSTART, WM_SETTEXT, 0, (LPARAM)"0000");
								}
							}
							break;

						case 4:		// raw cart rom dump (nothing set up automatically for you)
							SetDefaultEnables(hwnd);

							EnableDlgItem(hwnd, IDC_CHKHIGHRAM, FALSE);
							EnableDlgItem(hwnd, IDC_CHKLOWRAM, FALSE);
							EnableDlgItem(hwnd, IDC_CHKVDPRAM, FALSE);
							EnableDlgItem(hwnd, IDC_BTNREADDEFS, FALSE);

							EnableDlgItem(hwnd, IDC_BOOT, FALSE);
							EnableDlgItem(hwnd, IDC_CHKEA, FALSE);
							EnableDlgItem(hwnd, IDC_CHKCHARSET, FALSE);
							EnableDlgItem(hwnd, IDC_MODIFIEDHIGH, FALSE);
							EnableDlgItem(hwnd, IDC_MODIFIEDLOW, FALSE);
							EnableDlgItem(hwnd, IDC_MODIFIEDVRAM, FALSE);
							break;

						case 5:		// enable/disable for E/A#3
							SetDefaultEnables(hwnd);

                            // disable autoboot by default
                            SendDlgItemMessage(hwnd, IDC_BOOT, WM_SETTEXT, 0, (LPARAM)"0");

                            EnableDlgItem(hwnd, IDC_CHKCARTMEM, TRUE);
	                        EnableDlgItem(hwnd, IDC_CHKEA, FALSE);
	                        EnableDlgItem(hwnd, IDC_CHKCHARSET, FALSE);
							EnableDlgItem(hwnd, IDC_CHKVDPRAM, FALSE);
							EnableDlgItem(hwnd, IDC_VDPREGS, FALSE);
							EnableDlgItem(hwnd, IDC_NAME, FALSE);
							EnableDlgItem(hwnd, IDC_KEYBOARD, FALSE);
							EnableDlgItem(hwnd, IDC_GROM8K, FALSE);
							EnableDlgItem(hwnd, IDC_DISABLEF4, FALSE);
							EnableDlgItem(hwnd, IDC_LSTFIRSTGROM, FALSE);
							EnableDlgItem(hwnd, IDC_BTNREADDEFS, TRUE);
							EnableDlgItem(hwnd, IDC_INVERTBANKS, FALSE);
							break;
					}
				}
			}
			break;

		case WM_INITDIALOG:
			SetDefaultEnables(hwnd);
			gDisableDebugKeys = true;		// don't allow global debug keys in emu (override scroll lock)

			sprintf(buf, "%04X", pCurrentCPU->GetPC());
			SendDlgItemMessage(hwnd, IDC_BOOT, WM_SETTEXT, 0, (LPARAM)buf);
			SendDlgItemMessage(hwnd, IDC_CHKCHARSET, BM_SETCHECK, BST_UNCHECKED, 0);

			SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_ADDSTRING, 0, (LPARAM)"E/A #5");
			SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_ADDSTRING, 0, (LPARAM)"ROM Bank-Switched Copy");
			SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_ADDSTRING, 0, (LPARAM)"GROM Copy");
			SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_ADDSTRING, 0, (LPARAM)"TI BASIC Restore");
			SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_ADDSTRING, 0, (LPARAM)"ROM Cart dump");
            SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_ADDSTRING, 0, (LPARAM)"E/A #3");

			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"0 (Console)");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"1 (TI BASIC)");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"2 (TI BASIC)");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"3 (Default)");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"4");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"5");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"6");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_ADDSTRING, 0, (LPARAM)"7");
			SendDlgItemMessage(hwnd, IDC_LSTFIRSTGROM, CB_SETCURSEL, 3, 0);			// GROM 3
			SendDlgItemMessage(hwnd, IDC_LSTSAVETYPE, CB_SETCURSEL, 1, 0);			// set ROM copy cart
			return TRUE;
    } 
    return FALSE; 
} 
