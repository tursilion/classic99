//
// (C) 2017 Mike Brent aka Tursi aka HarmlessLion.com
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

// F18A support functions - for the moment it's just the SPI flash

// TODO: I did this bitwise, but that was unnecessary, the F18A can only
// speak to it one byte at a time. This emulation could be simplified by
// removing the bit assumptions.

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#include <stdio.h>
#include <windows.h>
#include <vector>
#include "tiemul.h"
#include "F18A.h"

// This exists mostly to be able to test the F18A updater.

// This is based loosely on the datasheet,
// the flash chip is the M25P80 SPI Flash

// There's pretty much no error handing - we just assume commands are valid
// There's no timing here - commands are assumed to complete immediately

// assumed to be a multiple of 1024
#define FLASH_SIZE (64*(64*1024))
#define FLASH_FILENAME "F18ASpiFlash.bin"

unsigned char *pFlash = NULL;
bool bEnable = false;
unsigned int reg32_in;
unsigned int status_reg = 0;
unsigned int pending_out;
int bitcount;
int address = 0;
int activecmd = 0;
int bytesleft;
unsigned char pagedata[256];
int pageaddress;

// instruction opcodes
#define CMD_WRSR 0x01
#define CMD_PP 0x02
#define CMD_READ 0x03
#define CMD_WRDI 0x04
#define CMD_RDSR 0x05
#define CMD_WREN 0x06
#define CMD_FAST_READ 0x0b
#define CMD_RDID 0x9f
#define CMD_RES 0xab
#define CMD_DP 0xb9
#define CMD_BE 0xc7
#define CMD_SE 0xd8

#define CMD_READ_MODE2 0xff
#define CMD_PP_MODE2 0xfe
#define CMD_PP_MODE3 0xfd

// status register
#define STATUS_SRWD 0x80
#define STATUS_BP2  0x10
#define STATUS_BP1  0x08
#define STATUS_BP0  0x04
#define STATUS_WEL  0x02
#define STATUS_WIP  0x01

void spi_backup_flash(int adr, int size) {
	// open in modify mode
	FILE *fp = fopen(FLASH_FILENAME, "r+b");
	if (NULL == fp) {
		// but if we couldn't, just create from scratch
		debug_write("Creating new file for F18A SPI Flash backup: %s", FLASH_FILENAME);
		fp = fopen(FLASH_FILENAME, "wb");
		if (NULL == fp) {
			debug_write("Failed to create disk backup");
			return;
		}
	}

	fseek(pFlash, adr, SEEK_SET);
	if (size/1024*1024 == size) {
		// a little faster if the math works
		fwrite(pFlash, 1024, size/1024, fp);
	} else {
		// just write bytes then
		fwrite(pFlash, 1, size, fp);
	}
	fclose(fp);
}

void spi_reset() {
	status_reg=0;
	pending_out = 0xffffffff;
	reg32_in = 0;
	bytesleft = 0;
}

void spi_try_command() {
	int tmp = 0;

	// when the enable is released, the command begins (if valid)
	if ((bitcount&0x07)!=0) {
		debug_write("Improper bit count %d on SPI release", bitcount);
		return;
	}

	if ((activecmd)&&(bytesleft > 0)) {
		--bytesleft;
		if (bytesleft == 0) {
			switch (activecmd) {
			case CMD_WRSR:
				// TODO: SRWD is not supported (is it only possible to set it with hardware WP?)
				status_reg &= ~(STATUS_BP2|STATUS_BP1|STATUS_BP0);
				status_reg |= (reg32_in&0xff) & (STATUS_BP2|STATUS_BP1|STATUS_BP0);
				reg32_in=0;
				activecmd=0;
				break;

			case CMD_READ:
				bytesleft = 1;
				activecmd = CMD_READ_MODE2;
				address = reg32_in&0x0fffff;
				//debug_write("SPI Set read address to %p", address);
				pending_out = pFlash[address++]<<24;
				address &= 0x0fffff;
				break;

			case CMD_FAST_READ:
				bytesleft = 1;
				activecmd = CMD_READ_MODE2;
				address = reg32_in&0x0fffff;
				//debug_write("SPI Set read address to %p", address);
				pending_out = 0xffffffff;		// dummy read
				break;

			case CMD_READ_MODE2:
				bytesleft = 1;
				pending_out = pFlash[address++]<<24;
				address &= 0x0fffff;
				break;

			case CMD_PP:
				tmp = status_reg & (STATUS_BP2|STATUS_BP1|STATUS_BP0);
				switch (tmp) {
				case 0:
					// all sectors unlocked
					break;

				case STATUS_BP0:
					if ((address/0x10000) < 0x0f) {
						tmp = 0;
					}
					break;

				case STATUS_BP1:
					if ((address/0x10000) < 0x0e) {
						tmp = 0;
					}
					break;

				case STATUS_BP0|STATUS_BP1:
					if ((address/0x10000) < 0x0c) {
						tmp = 0;
					}
					break;

				case STATUS_BP2:
					if ((address/0x10000) < 0x08) {
						tmp = 0;
					}
					break;

				case STATUS_BP0|STATUS_BP2:
				case STATUS_BP1|STATUS_BP2:
				case STATUS_BP0|STATUS_BP1|STATUS_BP2:
					// all sectors locked
					break;
				}
				if (tmp == 0) {
					bytesleft = 1;
					activecmd = CMD_PP_MODE2;
					address = reg32_in&0x0fff00;
					pageaddress = reg32_in&0xff;
				} else {
					debug_write("Attempt to write to protected sector");
					reg32_in=0;
					activecmd = 0;
				}
				break;

			case CMD_PP_MODE2:
				pagedata[pageaddress++] = reg32_in&0xff;
				pageaddress &= 0xff;
				bytesleft = 1;
				break;

			case CMD_PP_MODE3:
				// flush the data out - remember we can only set 1's to 0's
				//debug_write("Sector write - %p", address);

				for (int idx=0; idx<256; idx++) {
					pFlash[address+idx] &= pagedata[idx];
				}
				spi_backup_flash(address, 256);

				reg32_in=0;
				activecmd = 0;
				status_reg &= ~STATUS_WEL;
				break;

			case CMD_SE:
				// TODO: this shouldn't happen unless chip select is released
				address = reg32_in&0x0f0000;
				//debug_write("Sector erase of SPI flash - %p", address);
				memset(&pFlash[address], 0xff, 65536);
				spi_backup_flash(address, 256);
				reg32_in=0;
				activecmd = 0;
				status_reg &= ~STATUS_WEL;
				break;
			}
		}
		return;
	}

	activecmd = reg32_in&0xff;
	// we have a command - do something!
	switch (activecmd) {	//  								address bytes	dummy_bytes	data_bytes
	case CMD_WREN		:	// write enable							0				0			0
		status_reg |= STATUS_WEL;
		reg32_in=0;
		activecmd = 0;
		break;

	case CMD_WRDI		:	// write disable						0				0			0
		status_reg &= ~STATUS_WEL;
		reg32_in=0;
		activecmd = 0;
		break;

	case CMD_RDID		:	// read identification (T9HX only)		0				0			1-20
		// manufacturer id (1 byte)
		// device id (2 bytes)
		// length of unique id (1 byte)
		// unique id (16 bytes - I don't provide this, it's zero by default)
		pending_out = 0x20201410;
		reg32_in=0;
		activecmd = 0;
		break;

	case CMD_RDSR		:	// read status register					0				0			1 or more
		pending_out = status_reg<<24;
		break;

	case CMD_WRSR		:	// write status register				0				0			1
		if ((status_reg & STATUS_WEL) == 0) {
			reg32_in=0;
			activecmd = 0;
		} else {
			bytesleft = 1;
		}
		status_reg &= ~STATUS_WEL;
		break;

	case CMD_READ		:	// read data bytes						3				0			1 or more
		// fast_read seems to be a hardware thing, not a logical thing
		bytesleft = 3;
		break;

	case CMD_FAST_READ	:	// read data bytes faster				3				1			1 or more
		// fast_read seems to be a hardware thing, not a logical thing
		bytesleft = 3;
		break;

	case CMD_PP			:	// page program							3				0			1-256
		if ((status_reg & STATUS_WEL) == 0) {
			reg32_in=0;
			activecmd = 0;
		} else {
			bytesleft = 3;
		}
		break;

	case CMD_SE			:	// sector erase							3				0			0
		if ((status_reg & STATUS_WEL) == 0) {
			reg32_in=0;
			activecmd = 0;
		} else {
			bytesleft = 3;
		}
		status_reg &= ~STATUS_WEL;
		break;

	case CMD_BE			:	// bulk erase							0				0			0
		// TODO: this is supposed to wait for select to be released
		debug_write("Bulk erase of SPI flash");
		memset(pFlash, 0xff, FLASH_SIZE);
		spi_backup_flash(0, FLASH_SIZE);
		status_reg &= ~STATUS_WEL;
		reg32_in=0;
		activecmd = 0;
		break;

	case CMD_DP			:	// deep power-down						0				0			0
		debug_write("Ignoring deep power down");
		reg32_in=0;
		activecmd = 0;
		break;

	case CMD_RES		:	// release/read signature				0				3/0			1 or more/0
		debug_write("Treating release as signature read");
		pending_out = 0x13000000;
		reg32_in=0;
		activecmd = 0;
		break;

	default:
		reg32_in=0;
		activecmd = 0;
		break;

	}
}

// called from the GPU CKON and CKOFF
void spi_flash_enable(bool enable) {
	bEnable = enable;

	if ((bEnable) && (pFlash == NULL)) {
		pFlash = (unsigned char*)malloc(FLASH_SIZE);
		if (NULL != pFlash) {
			// make it look uninitialized
			memset(pFlash, 0xcd, FLASH_SIZE);
			// check if there's an old one to load from disk
			FILE *fp = fopen(FLASH_FILENAME, "rb");
			if (NULL == fp) {
				debug_write("Creating 1MB flash emulation for F18A SPI.");
				spi_backup_flash(0, FLASH_SIZE);
			} else {
				debug_write("Reading old 1MB flash for F18A SPI.");
				fread(pFlash, 1024, FLASH_SIZE/1024, fp);
				fclose(fp);
			}
		} else {
			debug_write("Unable to allocate RAM for F18A SPI flash emulation.");
		}
	}

	if (bEnable) {
		// start new command
		//debug_write("SPI enable");
		bitcount = 0;
		bytesleft = 0;
	} else {
		//debug_write("SPI disable");
		switch (activecmd) {
		case CMD_PP_MODE2:
			activecmd = CMD_PP_MODE3;
			break;
		}
		spi_try_command();
		reg32_in=0;
		activecmd = 0;
	}
}

void clock() {
	reg32_in <<= 1;
	pending_out <<= 1;
}

void spi_write_bit(unsigned int bit) {
	clock();
	if (bit) reg32_in |= 1;
	++bitcount;

	if (bitcount == 8) {
		spi_try_command();
		bitcount = 0;
	}
}

// called from the GPU LDCR
void spi_write_data(unsigned int data, int bits) {
	if (!bEnable) return;

	if (bits < 9) {
		data <<= 24;
	} else {
		data <<= 16;
	}
	for (int idx = 0; idx<bits; idx++) {
		spi_write_bit(data&0x80000000);
		data <<= 1;
	}
}

unsigned int spi_read_bit() {
	unsigned int out = pending_out&0x80000000;
	clock();
	++bitcount;

	if (bitcount == 8) {
		spi_try_command();
		bitcount = 0;
	}
	return out;
}

// called from the GPU STCR
int spi_read_data(int bits) {
	unsigned int data = 0;
	unsigned int mask = 0x80000000;
	for (int idx=0; idx<bits; idx++) {
		data |= (spi_read_bit() ? mask : 0);
		mask >>= 1;
	}
	if (bits < 9) {
		data >>= 24;
	} else {
		data >>= 16;
	}

	return data;
}
