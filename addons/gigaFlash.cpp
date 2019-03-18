// included helper code to simulate a 128MB gigacart flash chip
// with the first 128KB locked out
// includes the CPLD emulation too

unsigned int gigaLatch = 0;     // consists of 14 bits of latch
unsigned int gigaMask = 0;      // the control bits from the data byte
#define GIGAMASK_RESET 0x10
#define GIGAMASK_MSB 0x08
#define GIGAMASK_LSB 0x04

unsigned char flashSpace[128*1024*1024];    // huge!
unsigned char writeBuffer[512];

// this information from the datasheet
unsigned char cfiSpace[] = {
    // AutoSelect/ID data
    0x00,0x01,      // mfg id
    0x22,0x7e,      // device id
    0,0,            // sector protection state
    0xff,0xaf,      // indicator bits (factory locked, user not locked, WP# protects lowest)
    0,0,            // RFU reserved
    0,0,
    0,0,
    0,0,
    0,0,
    0,0,
    0,0,
    0,0,
    0,3,            // lower software bits - status and DQ polling support
    0,0,            // upper bits reserved
    0x22,0x28,      // 1Gb device id
    0x22,0x01,      // device id

    // CFI query ID at 0x20
    0,0x51,         // string QRY
    0,0x52,
    0,0x59,
    0,2,            // primary OEM command set
    0,0,
    0,0x40,         // primary extended table
    0,0,
    0,0,            // alternate OEM set (none)
    0,0,
    0,0,            // alternate OEM extended (none)
    0,0,

    // CFI System interface at 0x36
    0,0x27,         // vcc min
    0,0x36,         // vcc max
    0,0,            // vpp min (if present)
    0,0,            // vpp max (if present)
    0,8,            // single word timeout 2^n uS
    0,9,            // multibyte timeout 2^n uS
    0,10,           // timeout block erase 2^n ms
    0,0x14,         // timeout full chip erase 2^n ms
    0,2,            // max timeout single word 2^n times above
    0,1,            // max timeout buffer 2^n times above
    0,2,            // max timeout block 2^n times above
    0,2,            // max timeout full chip erase 2^n times above
    0,0x1b,         // size 2^n bytes
    0,2,            // bus type (8/16)
    0,0,
    0,9,            // max bytes multibyte (2^n)
    0,0,
    0,1,            // erase block regions (uniform)
    0,0xff,         // erase block 1 info
    0,3,
    0,0,
    0,2,
    0,0,            // erase block 2 info
    0,0,
    0,0,
    0,0,
    0,0,            // erase block 3 info
    0,0,
    0,0,
    0,0,
    0,0,            // erase block 4 info
    0,0,
    0,0,
    0,0,
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved

    // Primary vendor-specific extended data at 0x80
    0,0x50,         // string PRI
    0,0x52,
    0,0x49,
    0,0x31,         // major version ASCII (1)
    0,0x35,         // minor version ASCII (5)
    0,0x24,         // process technology (address sensitive unlock, 0.045um)
    0,2,            // erase suspend r/w
    0,1,            // sector protect size
    0,0,            // temporary unprotect (no)
    0,8,            // sector protect scheme (advanced)
    0,0,            // simultaneous operation count
    0,0,            // burst mode type
    0,3,            // page mode type (16 words)
    0,0xb5,         // acceleration supply min (11.5v)
    0,0xc5,         // acceleration supply max (12.5v)
    0,4,            // WP# protection (uniform, bottom)
    0,1,            // program suspend (yes)

    // v1.5 and up data
    0,1,            // unlock bypass
    0,9,            // customer OTP size (2^n bytes)
    0,0x8f,         // feature bits
    0,5,            // page size 2^n bytes
    0,6,            // erase suspend timeout 2^n us
    0,6,            // program suspend timeout 2^n us
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0xff,0xff,      // reserved
    0,6,            // embedded hardware reset timeout 2^n us (reset pin)
    0,9,            // non-embedded hardware reset timeout 2^n us (power on reset)
};

// reset is handled outside of this state
enum {
    FLASH_READ,     // array is set to read data mode
    FLASH_ERROR,    // error mode, returning status word
    FLASH_UNLOCK1,  // step 1 unlock
    FLASH_UNLOCKED, // fully unlocked
    FLASH_CFI,      // reading CFI
    FLASH_STATUS,   // read status register
    FLASH_PROGRAM,  // programming
    FLASH_ERASE1,   // intermediate for erase
    FLASH_ERASE1U1, // unlock 1 for erase
    FLASH_ERASE1U2, // unlock 2 for erase
    FLASH_ERASE,    // erasing
    FLASH_FILLSTART,// waiting for the page word count
    FLASH_FILLPAGE, // filling in the page write buffer
    FLASH_WORDPROG  // waiting for a word program
};
int state = 0;
// these are used for the data polling
int lastProgramAddress = 0;
int lastProgramByte = 0;
int toggleByte = 0;         // data polling return status
int eraseSector = 0;        // -1 for the entire chip
int currentEraseSector = 0;
int programSector = 0;      // for commands
int programPage = 0;        // 256 byte offset
int programCount = 0;
int programBuffer = 0;
int totalProgramCount = 0;
int previousState = FLASH_READ;
int statusReg = 0x80;

char *getStateName(int state) {
    switch (state) {
    case FLASH_READ:     return "FLASH_READ";
    case FLASH_ERROR:    return "FLASH_ERROR";
    case FLASH_UNLOCK1:  return "FLASH_UNLOCK1";
    case FLASH_UNLOCKED: return "FLASH_UNLOCKED";
    case FLASH_CFI:      return "FLASH_CFI";
    case FLASH_STATUS:   return "FLASH_STATUS";
    case FLASH_ERASE1:   return "FLASH_ERASE1";
    case FLASH_ERASE1U1: return "FLASH_ERASE1U1";
    case FLASH_ERASE1U2: return "FLASH_ERASE1U2";
    case FLASH_FILLSTART:return "FLASH_FILLSTART";
    case FLASH_FILLPAGE: return "FLASH_FILLPAGE";
    case FLASH_WORDPROG: return "FLASH_WORDPROG";
    case FLASH_PROGRAM:  return "FLASH_PROGRAM";
    case FLASH_ERASE:    return "FLASH_ERASE";
    default: return "????";
    }
}


// do whatever pending tasks we need to do
// this way we can let the TI run in the background
// this is called for each memory access
void runStateMachine() {
    switch (state) {
    // these states do nothing
    case FLASH_READ:     // array is set to read data mode
    case FLASH_ERROR:    // error mode: returning status word
    case FLASH_UNLOCK1:  // step 1 unlock
    case FLASH_UNLOCKED: // fully unlocked
    case FLASH_CFI:      // reading CFI
    case FLASH_STATUS:   // read status register
    case FLASH_ERASE1:   // intermediate for erase
    case FLASH_ERASE1U1: // unlock 1 for erase
    case FLASH_ERASE1U2: // unlock 2 for erase
    case FLASH_FILLSTART:// waiting for the page word count
    case FLASH_FILLPAGE: // filling in the page write buffer
    case FLASH_WORDPROG: // waiting for a word program
        return;

    // this state programs data into the array
    case FLASH_PROGRAM:  // programming
        if (programPage > sizeof(flashSpace)) {
            debug_write("Program beyond size of flash space - code error?");
            toggleByte |= 0x20;
            statusReg |= 0x10;
            state = FLASH_ERROR;
            return;
        }
        if (programBuffer > sizeof(writeBuffer)) {
            debug_write("Program beyond size of write buffer - code error?");
            toggleByte |= 0x20;
            statusReg |= 0x10;
            state = FLASH_ERROR;
            return;
        }
        // hard coded 128k protected boot page
        if (programPage < 128*1024) {
            debug_write("Attempt to program protected boot page");
            toggleByte |= 0x20;
            statusReg |= 0x12;
            state = FLASH_ERROR;
            return;
        }
        flashSpace[programPage++] = writeBuffer[programBuffer++];
        totalProgramCount--;
        if (totalProgramCount < 0) {
            debug_write("Program operation complete.");
            toggleByte = 0;
            statusReg = 0x80;
            state = FLASH_READ;
            return;
        }
        return;

    // this state returns the array to zeros
    // funny that I do a sector at a time, so this is faster than the writes...
    case FLASH_ERASE:    // erasing
        if (eraseSector == -1) {
            // whole flash
            if (currentEraseSector == -1) {
                currentEraseSector = 0;
            } else {
                if (currentEraseSector > 0) {
                    // first sector is write protected, not an error
                    memset(&flashSpace[currentEraseSector*128*1024], 0xff, 128*1024);
                }
                ++currentEraseSector;
                if (currentEraseSector*128*1024 >= sizeof(flashSpace)) {
                    debug_write("Erase chip complete.");
                    toggleByte = 0;
                    statusReg = 0x80;
                    state = FLASH_READ;
                }
            }
        } else {
            // just one sector
            if (currentEraseSector == -1) {
                currentEraseSector = eraseSector;
            } else {
                if (eraseSector == 0) {
                    // sector 0 is protected, this is an error
                    debug_write("Attempting to sector erase locked sector 0");
                    toggleByte |= 0x20;     // fail
                    toggleByte &= 0xf7;     // clear erase
                    statusReg |= 0x21;
                    state = FLASH_ERROR;
                    return;
                }
                if (eraseSector+128*1024-1 > sizeof(flashSpace)) {
                    debug_write("Attempt to erase sector larger than flash, error.");
                    toggleByte |= 0x20;     // fail
                    toggleByte &= 0xf7;     // clear erase
                    statusReg |= 0x20;
                    state = FLASH_ERROR;
                    return;
                }
                memset(&flashSpace[eraseSector], 0xff, 128*1024);
                debug_write("Erase sector complete.");
                toggleByte = 0;
                statusReg = 0x80;
                state = FLASH_READ;
            }
        }
        return;
    }

    debug_write("Unknown state machine state %d", state);
}

void write6000(Word adr, Byte dat) {
    unsigned int oldMask = gigaMask;
    runStateMachine();
    // ignore the latch bits for odd writes
    if ((adr&1)==0) {
        // this write goes to the CPLD for updating the latch etc
        gigaLatch = ((adr&0x1ffe)>>1) | ((dat&0x03) << 12);
        gigaMask = dat&0x1c;
        if ((gigaMask&GIGAMASK_RESET) && (0 == (oldMask&GIGAMASK_RESET))) {
            debug_write("Gigacart Reset released");
            state = FLASH_READ;
        }
        if ((oldMask&GIGAMASK_RESET) && (0 == (gigaMask&GIGAMASK_RESET))) {
            debug_write("Gigacart Reset asserted");
        }
        debug_write("Gigacart >%X=%X : latch now >%X, mask now >%X, PC >%X", adr, dat, gigaLatch, gigaMask, pCurrentCPU->GetPC());
    }
}

Byte progStatus(Word adr) {
    // return the program status and toggle the toggle bits
    Byte output = (~lastProgramByte)&0x80;
    toggleByte^=0x40;   // always toggles
    if (state == FLASH_ERASE) {
        if (eraseSector == -1) {
            toggleByte^=0x04;
        } else {
            unsigned int fullAdr = adr+(gigaLatch<<13);
            unsigned int eraseAdr = eraseSector*(128*1024); // 128k
            if ((fullAdr >= eraseAdr)&&(fullAdr < (eraseAdr+(128*1024)))) {
                toggleByte ^= 0x04;
            }
        }
    }
    return output | (toggleByte&0x7f);
}

Byte read6000(Word adr, bool rmw) {
    static int oldState = -1;
    if (oldState != state) {
        debug_write("Now in state %s", getStateName(state));
        oldState = state;
    }

    // rmw indicates what it's a read-before-write cycle
    // most of the time, that shouldn't matter, mostly it's for the debugger
    runStateMachine();

    // reads only go through to the flash chip when Gigamask allows it (is 00)
    if (gigaMask&(GIGAMASK_LSB|GIGAMASK_MSB)) {
        // bus is not driven, so the TI floats to 0
        return 0;
    }

    // can't do anything if it's in reset, either (active low)
    if ((gigaMask&GIGAMASK_RESET)==0) {
        // we do drive, and the CPLD pulls high
        debug_write("Gigacart read from >%X from PC >%X while flash is in reset", adr, pCurrentCPU->GetPC());
        return 0xff;
    }

    int offset = adr&0x1fff;    // offset in TI cartridge space
    int sectorOffset = (offset+(gigaLatch<<13))%(128*1024); // offset in erase sector

    // otherwise, it would appear to be real
    switch(state) {
        case FLASH_READ:
            return flashSpace[offset+(gigaLatch<<13)];

        case FLASH_ERROR:   // fall through
        case FLASH_PROGRAM: // fall through
        case FLASH_ERASE:
            return progStatus(offset);

        case FLASH_ERASE1U1:
        case FLASH_ERASE1U2:
        case FLASH_ERASE1:
            debug_write("Partial erase sequence broken by read to >%X (rmw:%d) from PC >%X", adr, rmw, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return flashSpace[offset+(gigaLatch<<13)];

        case FLASH_UNLOCK1:
            debug_write("Partial unlock sequence broken by read to >%X (rmw:%d) from PC >%X", adr, rmw, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return flashSpace[offset+(gigaLatch<<13)];

        case FLASH_UNLOCKED:
            debug_write("Full unlock broken by read to >%X (rmw:%d) from PC >%X", adr, rmw, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return flashSpace[offset+(gigaLatch<<13)];

        case FLASH_CFI:
            // this address is based on the sector address
            if (((sectorOffset&0xfffe)==4)&&(((offset+(gigaLatch<<13))/(128*1024) == 0))) {
                // sector 0 protected on this hardware
                return 1;
            } else if (sectorOffset < 0xf4) {
                return cfiSpace[sectorOffset|1];  // in 8-bit mode, we always get the LSB
            } else {
                debug_write("Reading invalid CFI data at >%X (latch >%X) from PC %X", adr, gigaLatch, pCurrentCPU->GetPC());
                return 0xff;
            }
            break;

        case FLASH_STATUS:
            debug_write("Read status register at >%X (rmw:%d) from PC >%X", adr, rmw, pCurrentCPU->GetPC());
            // one read only!
            state = previousState;
            if (adr&1) {
                // LSB contains useful data
                return statusReg&0xff;
            } else {
                // MSB doesn't
                debug_write("Status register MSB is not useful.");
                return 0xff;
            }
            break;

        case FLASH_FILLSTART:   // fall through
        case FLASH_FILLPAGE:
            // apparently reads don't abort anything, but I don't intend them...
            debug_write("Read to >%X (rmw:%d) from PC >%X during write buffer fill operation", adr, rmw, pCurrentCPU->GetPC());
            return progStatus(offset);

        case FLASH_WORDPROG:
            // apparently reads don't abort anything, but I don't intend them...
            debug_write("Read to >%X (rmw:%d) from PC >%X during word program operation", adr, rmw, pCurrentCPU->GetPC());
            return progStatus(offset);
    }

    debug_write("Unknown state in read6000 %d\n", state);
    return 0xff;
}

Byte readE000(Word adr, bool rmw) {
    runStateMachine();
    // this is normally not legal and we should never deliberately do it
    if (!rmw) {
        debug_write("Gigacart read from PC >%X to write port address >%X!", pCurrentCPU->GetPC(), adr);
    }

    // I believe we always float the lines, so 0
    // Whatever RAM is under it will be read instead
    return 0;
}

void writeE000(Word adr, Byte dat) {
    runStateMachine();
    // writing to the flash write port passes through only if gigaMask says so
    // undesired writes are a normal part of operation, that's why we have a mask
    if (adr&1) {
        // LSB address
        if ((gigaMask&GIGAMASK_LSB)==0) {
            return;
        }
    } else {
        // MSB address
        if ((gigaMask&GIGAMASK_MSB)==0) {
            return;
        }
    }

    // can't do anything if in reset
    if ((gigaMask&GIGAMASK_RESET) == 0) {
        debug_write("Gigacart write to >%X from PC >%X while flash is in reset", adr, pCurrentCPU->GetPC());
        return;
    }

    int offset = adr&0x1fff;                        // offset in TI cartridge space
    int fullAddress = (offset+(gigaLatch<<13));     // the address from the flash's point of view
    int sectorOffset = (fullAddress)%(128*1024);    // offset in erase sector
    int sectorAddress = fullAddress-sectorOffset;   // base address of the sector

    debug_write("Flash write >%X = >%X", fullAddress, dat);

    // so now we can process the state machine - limiting to the commands I actually use
    switch (state) {
        case FLASH_READ:     // array is set to read data mode
            // most writes will be ignored, but there shouldn't be any!!
            if ((dat == 0xaa)&&(fullAddress == 0xaaa)) {
                debug_write("Flash unlock step 1 from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_UNLOCK1;
                return;
            }
            if ((dat == 0x98)&&(fullAddress == 0x0aa)) {
                debug_write("CFI entry from PC >%X", pCurrentCPU->GetPC());
                previousState = state;
                state = FLASH_CFI;
                return;
            }
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register from PC >%X", pCurrentCPU->GetPC());
                previousState = state;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status from PC >%X", pCurrentCPU->GetPC());
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown READ MODE: >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            return;

        case FLASH_ERROR:    // error mode: returning toggle word
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register (from error) from PC >%X", pCurrentCPU->GetPC());
                previousState = FLASH_READ;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status (from error) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            if ((dat == 0xaa)&&(fullAddress == 0xaaa)) {
                // this is really only for the write-to-buffer-abort reset, but I'm
                // going to just allow it anyway
                debug_write("Unlock step 1 (from error) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_UNLOCK1;
                return;
            }

            debug_write("Unknown ERROR MODE: >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            return;

        case FLASH_UNLOCK1:  // step 1 unlock
            if ((dat == 0x55)&&(fullAddress == 0x555)) {
                debug_write("Flash unlock step 2 at PC >%X", pCurrentCPU->GetPC());
                state = FLASH_UNLOCKED;
                return;
            }
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                previousState = FLASH_READ;
                state = FLASH_STATUS;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x3b;
                return;
            }
            debug_write("Unknown UNLOCK1 MODE (abort unlock): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return;

        case FLASH_UNLOCKED: // fully unlocked
            if ((dat == 0x25)&&(sectorOffset == 0)) {
                // initiate a buffer write
                programSector = sectorAddress;
                programPage = -1;   // not defined till the first write
                programCount = 0;
                debug_write("Begin buffer write from PC >%X to sector >%X", pCurrentCPU->GetPC(), programSector);
                state = FLASH_FILLSTART;
                return;
            }
            if ((dat == 0xa0)&&(fullAddress == 0xaaa)) {
                debug_write("Word program from PC >%X to address >%X", pCurrentCPU->GetPC(), fullAddress);
                state = FLASH_WORDPROG;
                return;
            }
            if ((dat == 0x80)&&(fullAddress == 0xaaa)) {
                debug_write("Erase step 1 from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_ERASE1;
                return;
            }
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                previousState = FLASH_READ;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown UNLOCKED MODE (abort unlock): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return;

        case FLASH_ERASE1:   // erase step 1
            if ((dat == 0xaa)&&(fullAddress == 0xaaa)) {
                debug_write("Flash erase unlock step 1 from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_ERASE1U1;
                return;
            }
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                previousState = FLASH_READ;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown ERASE1 MODE (abort unlock): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return;

        case FLASH_ERASE1U1:
            if ((dat == 0x55)&&(fullAddress == 0x555)) {
                debug_write("Flash erase unlock step 2 from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_ERASE1U2;
                return;
            }
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                previousState = FLASH_READ;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status (abort unlock) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown ERASE1U1 MODE (abort unlock): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return;

        case FLASH_ERASE1U2:
            if ((dat == 0x30)&&(sectorOffset == 0)) {
                debug_write("Begin sector erase of >%X from PC >%X", sectorAddress, pCurrentCPU->GetPC());
                eraseSector = sectorAddress;
                currentEraseSector = -1;
                toggleByte |= 0x08; // erasing
                lastProgramByte = 0xff;
                state = FLASH_ERASE;
                return;
            }
            if ((dat == 0x10)&&(fullAddress == 0xaaa)) {
                debug_write("Begin chip erase from PC >%X", pCurrentCPU->GetPC());
                eraseSector = -1;
                currentEraseSector = -1;
                toggleByte |= 0x08; // erasing
                lastProgramByte = 0xff;
                state = FLASH_ERASE;
                return;
            }
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register (abort erase) from PC >%X", pCurrentCPU->GetPC());
                previousState = FLASH_READ;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status (abort erase) from PC >%X", pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown ERASE1U2 MODE (abort erase): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            state = FLASH_READ;
            return;

        case FLASH_ERASE:    // erasing
            // suspend not supported by me here...
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register from PC >%X", pCurrentCPU->GetPC());
                previousState = state;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status from PC >%X", pCurrentCPU->GetPC());
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown ERASE MODE (ignored): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            return;

        case FLASH_CFI:      // reading CFI
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register from PC >%X", pCurrentCPU->GetPC());
                previousState = state;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status from PC >%X", pCurrentCPU->GetPC());
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown CFI MODE (ignored): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            return;

        case FLASH_STATUS:   // read status register
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown STATUS MODE (ignored): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            return;

        case FLASH_PROGRAM:  // programming
            // suspend not supported by me here...
            if (dat == 0xf0) {
                // no address needed here
                debug_write("Flash soft reset to >%X from PC >%X.", fullAddress, pCurrentCPU->GetPC());
                state = FLASH_READ;
                statusReg &= ~0x33;
                toggleByte = 0;
                return;
            }
            if ((dat == 0x70)&&(fullAddress == 0xaaa)) {
                debug_write("Flash enter status register from PC >%X", pCurrentCPU->GetPC());
                previousState = state;
                state = FLASH_STATUS;
                return;
            }
            if ((dat == 0x71)&&(fullAddress == 0xaaa)) {
                debug_write("Flash clear status from PC >%X", pCurrentCPU->GetPC());
                statusReg &= ~0x3b;
                toggleByte = 0;
                return;
            }
            debug_write("Unknown PROGRAM MODE (ignored): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            return;

        case FLASH_FILLSTART:// waiting for page word count
            // docs say there is /no/ way out save an invalid write
            if ((sectorOffset == 0) && (sectorAddress == programSector)) {
                debug_write("Got word count >%X from PC >%X", dat, pCurrentCPU->GetPC());
                programCount = dat;
                totalProgramCount = dat;
                state = FLASH_FILLPAGE;
                return;
            }
            debug_write("Unknown FILLSTART MODE (abort): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            toggleByte |= 0x02;
            statusReg |= 0x08;
            state = FLASH_ERROR;
            return;

        case FLASH_FILLPAGE: // filling in the page write buffer
            // writes need to be constrained to the 256 bytes (cause 8-bit) line
            if (programPage == -1) {
                if (sectorAddress == programSector) {
                    programPage = fullAddress & 0xffffff00;     // might still be 512, even in byte, but whatever
                } else {
                    debug_write("First page write is not within the program sector from PC >%X (abort)", pCurrentCPU->GetPC());
                    toggleByte |= 0x02;
                    statusReg |= 0x08;
                    state = FLASH_ERROR;
                    return;
                }
            }
            if ((fullAddress == sectorAddress)&&(dat == 0x29)&&(programCount < 0)) {
                debug_write("Beginning buffer program to >%X from PC >%X of %d bytes", programPage, pCurrentCPU->GetPC(), totalProgramCount+1);
                programBuffer = 0;
                state = FLASH_PROGRAM;
                return;
            }
            if ((fullAddress >= programPage) && (fullAddress < programPage+256)) {
                if (programCount < 0) {
                    debug_write("Writing too many bytes to buffer from PC >%X (abort)", pCurrentCPU->GetPC());
                    toggleByte |= 0x02;
                    statusReg |= 0x08;
                    state = FLASH_ERROR;
                    return;
                }
                writeBuffer[fullAddress-programPage] = dat;
                programCount--;
                if (programCount == -1) {
                    debug_write("Last program count to buffer at >%X (value >%X) from PC >%X", fullAddress, dat, pCurrentCPU->GetPC());
                    lastProgramAddress = fullAddress;
                    lastProgramByte = dat;
                }
                return;
            }
            debug_write("Unknown FILLPAGE MODE (abort): >%X to >%X from PC >%X", dat, fullAddress, pCurrentCPU->GetPC());
            toggleByte |= 0x02;
            statusReg |= 0x08;
            state = FLASH_ERROR;
            return;

        case FLASH_WORDPROG: // waiting for a word program
            // not a lot is illegal here, unless of course it's a protected sector
            debug_write("Beginning byte program to >%X (value >%X) from PC >%X", fullAddress, dat, pCurrentCPU->GetPC());
            lastProgramAddress = fullAddress;
            lastProgramByte = dat;
            writeBuffer[0] = dat;
            totalProgramCount = 0;  // 1 byte
            programPage = fullAddress;
            programBuffer = 0;
            state = FLASH_PROGRAM;
            return;
    }
}
