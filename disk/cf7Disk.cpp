//
// (C) 2019 Mike Brent aka Tursi aka HarmlessLion.com
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

// there's very little disk class here - just a little at the bottom
// of the file. A true CF7Disk (if it was ever needed) would implement
// a lot more, but would probably just be a TICC with a different fileformat.
// Anyway, we implement just enough to make Classic99 think that a TICC is
// installed so it leaves the disk system alone, but we can still use
// the other TI devices.

// Quick emulation of the CF7 device, which maps a compact flash card
// into RAM so that the registers are mapped at every odd address.
// Reads are mapped to 0x5Exx, and writes to 0x5Fxx, when the CRU
// is mapped in. It normally overrides the disk controller at >1100,
// but it appears to operate all right at >1000, so that's where
// Classic99 loads its DSR, just to prevent conflicting with the
// Classic99 disk hooks... at least for now. We can probably change
// it to be more correct later...

// CF7 has some timing(?) issues that cause it to fail with some
// cards, particularly faster/newer cards. I've not looked into that.

// Compact flash registers:
//
//    TI.Adr	Read		Write
//    01	    Even Data	Even Data
//    03	    Error		Features
//    05	    SectorCnt	SectorCnt	(counts down to 0 after an access)
//    07	    Sector.No	Sector.No
//    09	    Cyl.Low	Cyl.Low
//    0B	    Cyl.High	Cyl.High
//    0D	    Card/Head	Card/Head
//    0F	    Status		Command
//    11	    Even Data	Even Data	(Duplicate, but not available on CF7?)
//    13	    Odd Data	Odd Data	(Duplicate, but not available on CF7?)
//    15	    n/a
//    17	    n/a
//    19	    n/a
//    1B	    Error		Features	(Duplicate, IS available on CF7)
//    1D	    Alt.Status	Device Ctl
//    1F	    Drive.Adr	reserved

// there are a crapton of actual commands you can give to a CF device. Rather than
// emulate all of them, or even most of them, I'm only going to do the ones I
// actually need to run the BIOS, which is mostly read sector and write sector in LBA mode...
//
// TODO: not even an attempt at emulating timing...

#include <atlstr.h>
#include <errno.h>
#include "tiemul.h"
#include "diskclass.h"
#include "cf7Disk.h"

extern CString csCf7Bios;
extern CString csCf7Disk;
extern int nCf7DiskSize;

// where we store a read sector
// and what position we are at in reading it
unsigned char sector[512];
int secpos = 512;
int writecnt = 512;

// various variables to track
int features = 0;
int dataReg = 0;
int errorReg = 0;       // TOOD: error emulation is minimal
int secCount = 0;
int secNum = 0;
int cylLow = 0;
int cylHigh = 0;
int cardHead = 0;
int status = 0;

// modes we track
bool is8Bit = false;    // whether 8 bit or 16 bit mode - defaults to 16
bool isReset = false;

// Status register
#define SBUSY 0x80
#define SRDY  0x40
#define SWFAULT 0x20
#define SDSC  0x10
#define SDREQ 0x08
#define SECC  0x04
// 0x02 not defined
#define SERR  0x01

// card/head flags
#define FLG_ALWAYS 0xA0
#define FLG_LBA 0x40
#define FLG_DRV 0x10

// device control bits
#define DC_RESET       0x04

// commands we know
#define CMD_SENSE       0x03        // puts the last error code in the error register (TODO)
#define CMD_READ        0x20        // reads a sector
#define CMD_WRITE       0x30        // writes a sector
#define CMD_IDENTIFY    0xec        // request identification
#define CMD_FEATURES    0xef        // sets features
    #define CMD_FEATURES_8BITON 0x01
    #define CMD_FEATURES_8BITOFF 0x81

// based on the SanDisk manual information
// must be less than 512 bytes!
const unsigned char identity[] = {
    0x84,0x8a,  // disk information bits
    0,99,       // number cylinders
    0,0,        // reserved
    0,1,        // number heads
    0,0,        // unformatted bytes per track
    0,0,        // unformatted bytes per sector
    0,9,        // sectors per track
    0,0,        // sectors per card, big endian
    0,0,
    0,0,        // reserved
    32,32,      // serial number in ASCII, right justified
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    0x39,0x39,
    0x34,65,
    0,0,        // buffer type
    0,0,        // buffer size
    0,4,        // ECC size
    'R','e',    // firmware revision
    'v','.',
    '1','.',
    '0',' ',
    'C','l',    // model number in ASCII, left justified
    'a','s',
    's','i',
    'c','9',
    '9',' ',
    'C','F',
    ' ','s',
    'i','m',
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    32,32,
    0,1,        // max sectors per read
    0,0,        // double word
    2,0,        // supports LBA, no DMA
    0,0,        // reserved
    2,0,        // PIO timing mode
    0,0,        // DMA timing mode
    0,3,        // field validity
    0,0,        // current cylinders
    0,0,        // current heads
    0,0,        // current sectors per track
    0,0,        // current LBA capacity (little endian word order)
    0,0,
    1,0,        // multiple sector valid
    0,0,        // LBA addressable (little endian word order)
    0,0,
    0,0,        // single word DMA
    0,0,        // DMA modes supported (0x07 in Sandisk)
    0,3,        // PIO modes supported (not really)
    0,0x78,     // min IDE DMA time ns
    0,0x78,     // rec IDE DMA time ns
    0,0x78,     // min PIO no flow control
    0,0x78,     // min PIO with IORDY
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,0,        // reserved
    0,1,        // major ATA version
    0,0,        // minor ATA version
    0,0,        // command sets supported
    0x40,4,     // command sets supported
    0x40,0,     // command sets supported
    0,0,        // command sets supported
    0,4,        // command sets supported
    0x40,0,     // command sets supported
    0,0,        // Ultra DMA
    0,0,        // secure erase time
    0,0,        // enhanced secure erase time
    0,0,        // current power management value
    // there's more, but mostly reserved...
};

// open the CF7 disk file and move the file pointer to the right place
// note we always assume LBA
FILE *prepareDisk(const char *pMode) {
    // check the flags in drive/head
    if ((cardHead & FLG_ALWAYS) != FLG_ALWAYS) {
        debug_write("CF7 %s: Mandatory bits 0xA0 are not set in drive/head register", pMode);
        return NULL;
    }
    if (cardHead & FLG_DRV) {
        debug_write("CF7 %s: Drive bit is set in drive/head, only drive 0 supported.", pMode);
        return NULL;
    }
    if ((cardHead & FLG_LBA) == 0) {
        debug_write("CF7 %s: LBA bit is not set in drive/head - Classic99 will use LBA addressing only and so should you ;)", pMode);
        return NULL;
    }

    FILE *fp = fopen(csCf7Disk, "rb+");
    if ((NULL == fp) && (GetLastError() != 2)) {    // 2 is FILE NOT FOUND
        debug_write("CF7 %s: Failed to open CF7 file '%s' with code %d", pMode, csCf7Disk.GetString(), errno);
        return NULL;
    }
    if (NULL == fp) {
        // we need to create the missing file
        debug_write("CF7 %s: Creating CF7 emulation file of %d bytes at '%s'", pMode, nCf7DiskSize, csCf7Disk.GetString());
        fp = fopen(csCf7Disk, "wb+");
        if (NULL == fp) {
            debug_write("CF7 %s: Failed to create new file for CF7 with code %d", pMode, errno);
            return fp;
        }
    }

    int pos = secNum | (cylLow << 8) | (cylHigh << 16) | ((cardHead&0x0f)<<24);

    if (pos*512 >= nCf7DiskSize) {
        debug_write("CF7 %s: request for sector >%08X which is past disk end of >%08X", pMode, pos, nCf7DiskSize/512);
        fclose(fp);
        return NULL;
    }

    debug_write("CF7 %s: access sector >%08X", pMode, pos);   // TODO: this one will get spammy
    fseek(fp, pos*512, SEEK_SET);

    return fp;
}

// read a sector from the ondisk image
void readSector() {
    FILE *fp = prepareDisk("read");
    if (NULL == fp) {
        return;
    }

    // if we don't get it all, it'll just be zeroed
    memset(sector, 0, sizeof(sector));
    fread(sector, 1, 512, fp);
    fclose(fp);
}

// load identity data into the buffer
void readIdentity() {
    memset(sector, 0, sizeof(sector));
    memcpy(sector, identity, sizeof(identity));
    int sectors = nCf7DiskSize/512;
    // fill in some configurable data
    sector[14]=(sectors>>24)&0xff;
    sector[15]=(sectors>>16)&0xff;
    sector[16]=(sectors>>8)&0xff;
    sector[17]=(sectors)&0xff;
    // byte order is weird on purpose
    sector[116]=(sectors>>24)&0xff;
    sector[117]=(sectors>>16)&0xff;
    sector[114]=(sectors>>8)&0xff;
    sector[115]=(sectors)&0xff;
    // byte order is weird on purpose
    sector[122]=(sectors>>24)&0xff;
    sector[123]=(sectors>>16)&0xff;
    sector[120]=(sectors>>8)&0xff;
    sector[121]=(sectors)&0xff;
    
    // now byte flip the buffer
    for (int idx=0; idx<512; idx+=2) {
        unsigned char x = sector[idx];
        sector[idx]=sector[idx+1];
        sector[idx+1]=x;
    }
}

// write a sector to the ondisk image
void writeSector() {
    FILE *fp = prepareDisk("write");
    if (NULL == fp) {
        return;
    }

    // here we want to know if we failed to write...
    if (512 != fwrite(sector, 1, 512, fp)) {
        debug_write("CF7: Failed to write sector: code %d", errno);
    }
    fclose(fp);
}

Byte read_cf7(Word adr) {
    // we should already be filtered so that we don't need to return ROM data, so we only worry about 0x5Exx
    if ((adr < 0x5e00) || (adr > 0x5eff)) {
        return 0;
    }

    // repeats every 32 bytes
    adr&=0x1f;

    if (isReset) {
        debug_write("Read from >04X while CF7 is reset", adr);
        return 0;
    }

    switch (adr) {
        default:
            return 0;       // even bytes and some registers return 0

        case 0x01:  // even data
            if (status & SBUSY) {
                return 0;
            }
            if (secpos < 512) {
                Byte ret = sector[secpos];
                if (is8Bit) {
                    ++secpos;
                } else {
                    secpos+=2;
                }
                if (secpos >= 512) {
                    if (secCount > 0) {
                        --secCount;
                        if (secCount > 0) {
                            status |= SBUSY;
                            readSector();
                            secpos = 0;
                        }
                    }
                }
                return ret;
            } else {
                return 0;
            }

        case 0x1b:  // dup error
        case 0x03:  // Error
            return errorReg&0xff;

        case 0x05:  // Sector Count
            return secCount&0xff;

        case 0x07:  // Sector No
            return secNum&0xff;

        case 0x09:  // Cyl.Low
            return cylLow&0xff;

        case 0x0B:  // Cyl.High
            return cylHigh&0xff;

        case 0x0D:  // Card/Head
            return cardHead&0xff;

        case 0x0f:  // Status
        {
            Byte ret = status;

            if ((status&SBUSY)==0) ret |= SRDY|SDSC;
            if ((secpos<512)||(writecnt<512)) ret |= SDREQ;
            // TODO: error bits?

            // force people to read status between operations with a single frame of busy
            if (status & SBUSY) {
                status &= ~SBUSY;
            }

            return ret;
        }
    }
}

void write_cf7(Word adr, Byte c) {
    // we should already be filtered so that we don't need to return ROM data, so we only worry about 0x5Exx
    if ((adr < 0x5f00) || (adr > 0x5fff)) {
        return;
    }

    // repeats every 32 bytes
    adr&=0x1f;

    switch (adr) {
        default:    return;

        case 0x01:  // even data
            if (status & SBUSY) return;
            if (writecnt < 512) {
                sector[writecnt] = c;
                if (is8Bit) {
                    ++writecnt;
                } else {
                    writecnt+=2;
                }
                if (writecnt >= 512) {
                    writeSector();
                    status |= SBUSY;
                    if (secCount > 0) {
                        --secCount;
                        if (secCount > 0) {
                            memset(sector, 0, sizeof(sector));
                            writecnt = 0;
                        }
                    }
                }
            }
            return;

        case 0x1b:  // features (dup)
        case 0x03:  // features
            features = c;
            break;

        case 0x05:  // Sector Count
            secCount = c;
            break;

        case 0x07:  // Sector No
            secNum = c;
            break;

        case 0x09:  // Cyl.Low
            cylLow = c;
            break;

        case 0x0B:  // Cyl.High
            cylHigh = c;
            break;

        case 0x0D:  // Card/Head
            cardHead = c;
            break;

        case 0x0f:  // command
            switch (c) {
                // what command are we going for?
                case CMD_READ:
                    status |= SBUSY;
                    readSector();
                    secpos = 0;
                    break;

                case CMD_WRITE:
                    status |= SBUSY;
                    memset(sector, 0, sizeof(sector));
                    writecnt = 0;
                    break;

                case CMD_IDENTIFY:
                    status |= SBUSY;
                    readIdentity();
                    secpos = 0;
                    break;

                case CMD_FEATURES:
                    switch (features) {
                        case CMD_FEATURES_8BITON:
                            is8Bit = true;
                            break;
                        case CMD_FEATURES_8BITOFF:
                            is8Bit = false;
                            break;
                        default:
                            debug_write("CF7: Unrecognized feature command: >%02X", features);
                    }
                    break;

                default:
                    debug_write("CF7: Unknown command >%02X", c);
            }
            break;

        case 0x1d:  // device control
            if (c & DC_RESET) {
                isReset = true;
            } else {
                isReset = false;
                secpos = 512;
                writecnt = 512;
                features = 0;
                dataReg = 0;
                errorReg = 0;
                secCount = 0;
                secNum = 0;
                cylLow = 0;
                cylHigh = 0;
                cardHead = 0;
                status = 0;
            }
            break;
    }
}

//********************************************************
// ClipboardDisk
//********************************************************

// constructor
Cf7Disk::Cf7Disk() {
}

Cf7Disk::~Cf7Disk() {
}
