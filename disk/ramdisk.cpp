//
// (C) 2025 Mike Brent aka Tursi aka HarmlessLion.com
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
// of the file. This is based on CF7Disk.
// Anyway, we implement just enough to make Classic99 think that a TICC is
// installed so it leaves the disk system alone, but we can still use
// the other TI devices.

// Quick emulation of the Horizon 3000 RAMDISK, which maps RAM into
// DSR space for access as a disk drive.
// https://www.unige.ch/medecine/nouspikel/ti99/horizon.htm
// Note for the larger disks (8MB, 16MB) you need the cfg842f as of Jan 2025.
// The actual "RAM" is file backed instead.
// The file consists of 8k of DSR space, followed by the size specified in config.
// RAMdisks normally run at >1000, but are designed to work anywhere.
// That plays nicely with the Classic99 DSR.

// DSR: >4000 - >57FF  (6k)
// RAM: >5800 - >5FFF  (2k)
// CRU: 0 - enable
//      1-14 - page select (16,384 pages at 2k each for 32MB)
//      15 - RAMBO
// Not clear if the software will let me just directly map memory...
// The RAMBO usage is documented in the Horizon manual. It changes the meaning of the other CRU bits.

// RAMBO will page memory in from >6000 - >7FFF. It looks like it's expected to work with a GRAMKracker
// so that it can turn cartridge ROM off while it's active. Thierry wrote a modification for the GRAM-KARTE.
// So to that end maybe we still skip it... there's no schematic and we'll have to look at source
// code (apparently available) or ask Gary how it's supposed to work, but it won't be compatible with
// Classic99's concept and I think AMS is the future.
// This doesn't change my idea to make a compatible hardware using SD though...

#include <atlstr.h>
#include <errno.h>
#include "..\console\tiemul.h"
#include "diskclass.h"
#include "RamDisk.h"

extern CString csRamDisk;
extern int nRamDiskSize;

// warning: caches are flushed back to disk only when the RAMdisk is turned off by CRU bit 0!
static int RamDiskCRU[16] = { 0 };
static unsigned char blockcache[2048];
static int cacheadr = -1;
static int cachedirty = 0;
static unsigned char dsrcache[8192];
static int dsrloaded = 0;
static int dsrdirty = 0;

// calculate the current page number from CRU
static int getPage() {
    // position is CRU bits 1-14
    int pos = 0;
    for (int i = 14; i > 0; --i) {
        pos <<= 1;
        if (RamDiskCRU[i]) {
            pos |= 1;
        }
    }

    return pos;
}

// open the ramdisk file and move the file pointer to the right place
static FILE *prepareDisk(const char *pMode, int page=-1) {
    FILE *fp = fopen(csRamDisk, "rb+");
    if ((NULL == fp) && (GetLastError() != 2)) {    // 2 is FILE NOT FOUND
        debug_write("RAMDISK %s: Failed to open RamDisk file '%s' with code %d", pMode, csRamDisk.GetString(), errno);
        return NULL;
    }
    if (NULL == fp) {
        // we need to create the missing file
        debug_write("RAMDISK %s: Creating RamDisk emulation file of %d bytes at '%s'", pMode, nRamDiskSize, csRamDisk.GetString());
        fp = fopen(csRamDisk, "wb+");
        if (NULL == fp) {
            debug_write("RAMDISK %s: Failed to create new file for RamDisk with code %d", pMode, errno);
            return fp;
        }
    }

    int pos;
    if (page == -1) {
        pos = getPage();
    } else {
        pos = page;
    }
    if (pos*2048 >= nRamDiskSize) {
        debug_write("RAMDISK %s: request for page >%08X which is past disk end of >%08X", pMode, pos, nRamDiskSize/2048);
        fclose(fp);
        return NULL;
    }

    //debug_write("RAMDISK %s: access page >%08X", pMode, pos);
    fseek(fp, pos*2048+8192, SEEK_SET);     // 2k sectors, plus 8k bios at start

    return fp;
}

static void flushcaches() {
    if (cachedirty) {
        FILE* fp = prepareDisk("write", cacheadr);
        if (NULL == fp) {
            debug_write("WARNING: RAMDISK failed to flush disk data cache");
        } else {
            fwrite(blockcache, 1024, 2, fp);
            fclose(fp);
            cachedirty = 0;
            //debug_write("flush disk cache");
        }
    }
    if (dsrdirty) {
        FILE* fp = prepareDisk("writedsr", 0);
        if (NULL == fp) {
            debug_write("WARNING: RAMDISK failed to flush DSR cache");
        } else {
            fseek(fp, 0, SEEK_SET);
            fwrite(dsrcache, 1024, 8, fp);
            fclose(fp);
            dsrdirty = 0;
            //debug_write("flush dsr cache");
        }
    }
}

static void updatecache() {
    int pos = getPage();
    if (pos != cacheadr) {
        flushcaches();
        memset(blockcache, 0, sizeof(blockcache));
        FILE* fp = prepareDisk("read");
        if (NULL == fp) {
            return;
        }
        fread(blockcache, 1024, 2, fp);
        fclose(fp);
        cacheadr = pos;
    }
}

int ReadRamdiskCRU(int ad) {
    // there's no readback of CRU bits
    //debug_write("RAMDISK read CRU bit %d (1)", ad);
    return 1;
}
void WriteRamdiskCRU(int ad, int bt) {
    //debug_write("RAMDISK write CRU bit %d (%d)", ad, bt);
    ad &= 0xf;  // 16 bits available
    RamDiskCRU[ad] = bt;

    if ((ad == 0) && (bt == 0)) {
        flushcaches();
    }

}

// make sure DSR is loaded from image
void preload_dsr() {
    if (!dsrloaded) {
        FILE* fp = prepareDisk("dsr");
        if (NULL != fp) {
            fseek(fp, 0, SEEK_SET);
            fread(dsrcache, 1024, 8, fp);
            fclose(fp);
            dsrloaded = 1;
        }
    }
}

Byte read_ramdisk(Word adr) {
    if ((adr >= 0x5800) && (adr < 0x6000)) {
        // paged area
        updatecache();
        //debug_write("RAMDISK read address >%X (%d)", adr, blockcache[adr - 0x5800]);
        return blockcache[adr - 0x5800];
    } else {
        // DSR area
        preload_dsr();
        return dsrcache[adr - 0x4000];
    }
}

void write_ramdisk(Word adr, Byte c) {
    if ((adr >= 0x5800) && (adr < 0x6000)) {
        // paged area
        //debug_write("RAMDISK write address >%X (%d)", adr, c);
        updatecache();
        blockcache[adr - 0x5800] = c;
        cachedirty = 1;
    } else {
        // DSR area
        preload_dsr();
        dsrcache[adr - 0x4000] = c;
        dsrdirty = 1;
    }
}

//********************************************************
// RamDisk
//********************************************************

// constructor
RamDisk::RamDisk() {
}

RamDisk::~RamDisk() {
}
