// Quick and dirty SD card emulation

#include "SDCard.h"
#include <Windows.h>

extern void debug_write(char*, ...);

// stats
int SDReads = 0;
int SDWrites = 0;

char szCardFilename[2048];
bool isHighSpeed = false;
bool isChipEnabled = false;

// state data
enum STATE {
    SYNCING,
    RXCMD,
    PROCESSING,
    RESPONSE,
    READ,
    WRITE,
    READMANY,
    WRITEMANY
};
enum STATE myState = SYNCING;
int stateTimeout = 10;
unsigned char replyBuf[532];
int replyPos = 0;
int replySize = 0;
unsigned char inputCmd[6];   // always six bytes
int inputPos = 0;
int blockSize = 512;
bool isAppCmd = false;
unsigned __int64 fileOffset = 0;
unsigned char writeBuf[532];
unsigned int writePos = 0;
bool isWriting = false;

// status bit
// 0 P A E C I R X
// - 0 = always zero
// - P = parameter error
// - A = address error
// - E = erase sequence error
// - C = Command CRC error
// - I = Illegal command
// - R = erase Reset
// - X = Idle state
#define STATUS_PARAM_ERROR 0x40
#define STATUS_ADDRESS_ERROR 0x20
#define STATUS_ERASESEQ_ERROR 0x10
#define STATUS_CRC_ERROR 0x08
#define STATUS_ILLEGAL_COMMAND 0x04
#define STATUS_ERASE_RESET 0x02
#define STATUS_IDLE_STATE 0x01      // card is NOT ready to process anything! only return during SYNCING

const char *getStateName(STATE st) {
    switch (st) {
        case SYNCING:    return "SYNCING";
        case RXCMD:      return "RXCMD";
        case PROCESSING: return "PROCESSING";
        case RESPONSE:   return "RESPONSE";
        case READ:       return "READ";
        case WRITE:      return "WRITE";
        case READMANY:   return "READMANY";
        case WRITEMANY:  return "WRITEMANY";
    }
    return "??unknown??";
}

void changeState(STATE newstate) {
    if (myState != newstate) {
        debug_write("SD Changing state from %s to %s", getStateName(myState), getStateName(newstate));
        myState = newstate;
    }
}

// handles a single byte reply
void handleReply(unsigned char val, int processTime) {
    if (myState == SYNCING) val |= STATUS_IDLE_STATE;
    replyBuf[0] = val;
    replyPos = 0;
    replySize = 1;
    stateTimeout = processTime;
}

void processAppCmd() {
    // received command was application specific
    switch (inputCmd[0]) {
        case 0x4D:  // SD_STATUS                - send SD status
            // mostly zeroed, but the whole 512 byte block
            // This is kind of useless...
            memset(&replyBuf[8], 0, 512);

            replyBuf[0] = 0;    // busy
            replyBuf[1] = 0;
            replyBuf[2] = 0;
            replyBuf[3] = 0;
            replyBuf[4] = 0;
            replyBuf[5] = 0;
            replyBuf[6] = 0;
            replyBuf[7] = 0xfe; // sync

            replyPos = 0;
            replySize = 520;
            stateTimeout = 0;
            break;

        case 0x56:  // SEND_NUM_WR_BLOCKS       - returns number of non-errored blocks
            debug_write("SEND_NUM_WR_BLOCKS unimplemented.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x57:  // SET_WR_BLK_ERASE_COUNT   - number of blocks to pre-erase before a multiple block write
            debug_write("SET_WR_BLK_ERASE_COUNT unimplemented.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x69:  // SD_SEND_OP_COND             - activate the card's initialization process
            // this is the preferred init setup for SD cards
            // SPI, not SD, is just 8 bits
            handleReply(0, 0);
            break;

        case 0x6A:  // SET_CLR_CARD_DETECT      - lsb connects or disconnects the internal pullup resistor on pin 1
            debug_write("SET_CLR_CARD_DETECT unimplemented.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x73:  // SEND_SCR                 - send SD Configuration Register
            replyBuf[0] = 0;    // SCR version 1, Sd spec v1.01
            replyBuf[1] = 0x25; // security spec 2, bus widths 1 and 4 supported
            replyBuf[2] = 0;    // reserved
            replyBuf[3] = 0;
            replyBuf[4] = 0;    // reserved for manufacturer usage
            replyBuf[5] = 0;
            replyBuf[6] = 0;
            replyBuf[7] = 0;
            replyPos = 0;
            replySize = 8;
            stateTimeout = 0;
            break;

        default:
            debug_write("Unknown app command: ACMD%d (0x%02X)", inputCmd[0]&0x3f, inputCmd[0]);
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;
    }
}

void processCmd() {
    // command is 6 bytes in inputCmd
    inputPos = 0;       // reset input

    if ((myState == SYNCING)&&(inputCmd[0] != 0x40)) {
        debug_write("Card in sync, but received command CMD%d (0x%02X) before CMD0!", inputCmd[0]&0x3f, inputCmd[0]);
    }

    if (isAppCmd) {
        processAppCmd();
        isAppCmd = false;
        return;
    } 

    switch(inputCmd[0]) {
        case 0x40:  // GO_IDLE_CMD  (reset - must be first to activate SPI mode)
            SDReset();
            handleReply(STATUS_IDLE_STATE, 2);
            break;

        case 0x41:  // SEND_OP_COND (start initialization) (CMD58 is read_ocr (0x7A))
//          debug_write("Using MMC initialization SEND_OP_COND, use CMD58 instead."); // this okay, just deprecated
            handleReply(0, 0);
            break;

        case 0x49:  // SEND_CSD
            // this is heavily bit packed, and does not always align on a byte boundary
            // these hard coded values are not tested to be correct
            // TODO: I'm pretty sure it's misaligned
            replyBuf[0]  = 0;       // CSD structure
            replyBuf[1]  = 0x26;    // data read access time binary
            replyBuf[2]  = 0x0f;    // data read access time MLC
            replyBuf[3]  = 0;       // data read access time 2 CLKs
            replyBuf[4]  = 0x32;    // max data transfer rate
            replyBuf[5]  = 0x01;    // card command classes (all)
            replyBuf[6]  = 0xf5;
            replyBuf[7]  = 0x98;    // max read len 512 bytes, partial reads allowed, no misalignment
            replyBuf[8]  = 0x0f;    // device size 128=3843? (todo: shouldn't be fixed, only 6 bits here)
            replyBuf[9]  = 0x03;
            replyBuf[10] = 0xfb;    // current settings (3 bits each)
            replyBuf[11] = 0xe9;
            replyBuf[12] = 0x3f;    // erase sector size plus 1 bit
            replyBuf[13] = 0xfe;    // write protect data
            replyBuf[14] = 0x45;    // write speed factor
            replyBuf[15] = 0x20;    // no partial writes
            replyBuf[16] = 0x20;    // not original
            replyBuf[17] = 0xff;    // CRC7 plus 1 lsb
            replyPos = 0;
            replySize = 17;
            stateTimeout = 0;
            break;

        case 0x4A:  // SEND_CID
            replyBuf[0]  = 0x03;    // manufacturer ID (SanDisk)
            replyBuf[1]  = 0x53;    // 'S' application ID
            replyBuf[2]  = 0x44;    // 'D'
            replyBuf[3]  = 0x43;    // 'C' product name
            replyBuf[4]  = 0x4C;    // 'L'
            replyBuf[5]  = 0x41;    // 'A'
            replyBuf[6]  = 0x39;    // '9'
            replyBuf[7]  = 0x39;    // '9'
            replyBuf[8]  = 0x10;    // Revision 1.0
            replyBuf[9]  = 0x39;    // '9' serial number
            replyBuf[10] = 0x39;    // '9'
            replyBuf[11] = 0x34;    // '4'
            replyBuf[12] = 0x41;    // 'A'
            replyBuf[13] = 0x01;    // manufacture date (2019, May)
            replyBuf[14] = 0x95;
            replyBuf[15] = 0xff;    // CRC
            replyPos = 0;
            replySize = 16;
            stateTimeout = 0;
            break;

        case 0x4C:  // STOP_TX      (stop transmission during multiple block read)
            if (myState != READMANY) {
                debug_write("STOP_TX received while in state %s.", getStateName(myState));
            }
            myState = RXCMD;
            handleReply(0, 0);
            break;

        case 0x4D:  // SEND_STATUS
            // TODO: we should handle all the various states
            // Byte 1:
            // 7 - always 0
            // 6 - parameter error
            // 5 - address error
            // 4 - erase sequence error
            // 3 - com CRC error
            // 2 - illegal command
            // 1 - erase reset
            // 0 - idle state
            // Byte 2:
            // 7 - out of range
            // 6 - erase param
            // 5 - WP violation
            // 4 - card ECC failed
            // 3 - Card Controller error
            // 2 - error
            // 1 - WP erase skip, lock/unlock cmd failed
            // 0 - card is locked
            if (stateTimeout == 0) {
                replyBuf[0] = 1;
            } else {
                replyBuf[0] = 0;
            }
            replyBuf[1] = 0;
            replyPos = 0;
            replySize = 2;
            stateTimeout = 0;
            break;

        case 0x50:  // SET_BLOCKLEN (for reads only, any write not 512 bytes is illegal)
            blockSize = (inputCmd[1]<<24)+(inputCmd[2]<<16)+(inputCmd[3]<<8)+inputCmd[4];
            if (blockSize != 512) {
                debug_write("Warning: blockSize set to non-standard %d bytes", blockSize);
            } else {
                debug_write("BlockSize set to %d bytes", blockSize);
            }
            handleReply(0, 0);
            break;

        case 0x51:  // READ_BLOCK
            fileOffset = (inputCmd[1]<<24)+(inputCmd[2]<<16)+(inputCmd[3]<<8)+inputCmd[4];
            //fileOffset *= 512;  // TODO: this is a block address only on V2 cards, we asked for byte addresses
            {
                HANDLE hFile = CreateFile(szCardFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (INVALID_HANDLE_VALUE == hFile) {
                    debug_write("Failed to open SD card file.");
                    handleReply(STATUS_CRC_ERROR, 0);
                    break;
                }
                long DistanceLow = fileOffset&0xffffffff;
                long DistanceHigh = (fileOffset>>32)&0xffffffff;
                if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, DistanceLow, &DistanceHigh, FILE_BEGIN)) {
                    debug_write("Failed to seek SD card file to block %llu", fileOffset);
                    CloseHandle(hFile);
                    handleReply(STATUS_PARAM_ERROR, 0);
                    break;
                }
                if (!ReadFile(hFile, &replyBuf[8], blockSize, NULL, NULL)) {
                    debug_write("Failed to read from SD card file");
                    CloseHandle(hFile);
                    handleReply(STATUS_PARAM_ERROR, 0);
                    break;
                }
                CloseHandle(hFile);
                debug_write("Read from file address %llu (sector %llu)", fileOffset, fileOffset/512);

                // the response is immediate, but there's a random delay until the sync byte
                // we fake that here with this reply stream

                replyBuf[0] = 0xff;    // working
                replyBuf[1] = 0;       // ready
                replyBuf[2] = 0xff;    // working...
                replyBuf[3] = 0xff;
                replyBuf[4] = 0xff;
                replyBuf[5] = 0xff;
                replyBuf[6] = 0xff;
                replyBuf[7] = 0xfe; // sync

                replyPos = 0;
                replySize = 520;
                stateTimeout = 3;
            }
            break;

        case 0x52:  // READ_MULTIPLE
            debug_write("READ_MULTIPLE is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x58:  // WRITE_BLOCK
            debug_write("READ_MULTIPLE is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
#if 0
            // this is at least PARTIALLY right, needs more testing. My Phoenix library might
            // be broken for FAT16 writes...
            fileOffset = (inputCmd[1]<<24)+(inputCmd[2]<<16)+(inputCmd[3]<<8)+inputCmd[4];
            //fileOffset *= 512;  // TODO: this is a block address only on V2 cards, we asked for byte addresses
            if (fileOffset % 512) {
                debug_write("Attempt to write to non-sector aligned address %u", fileOffset);
                handleReply(STATUS_CRC_ERROR, 0);
                break;
            }
            writePos = -1;
            isWriting = true;
            debug_write("Starting WRITE_BLOCK to %u", fileOffset);
            handleReply(0, 0);
#endif
            break;

        case 0x59:  // WRITE_MULTIPLE
            debug_write("WRITE_MULTIPLE is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x5B:  // PROGRAM_CSD
            debug_write("PROGRAM_CSD is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x5C:  // SET_WRITE_PROTECT
            debug_write("SET_WRITE_PROTECT is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x5D:  // CLR_WRITE_PROTECT
            debug_write("CLR_WRITE_PROTECT is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x5E:  // SEND_WRITE_PROTECT
            debug_write("SEND_WRITE_PROTECT is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x60:  // ERASE_WR_BLK_START_ADDR  - first write block to be erased
        case 0x61:  // ERASE_WR_BLK_END_ADDR    - last write block to be erased
        case 0x66:  // ERASE                    - erase the selected blocks
            debug_write("ERASE BLOCK commands are not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x77:  // APP_CMD                  - next command is application specific
            debug_write("App command pending...");
            isAppCmd = true;
            break;

        case 0x78:  // GEN_CMD                  - general purpose block access (lsb 1=read, 0=write)
            debug_write("GEN_CMD is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        case 0x7a:  // READ_OCR
            // standard response, plus the 4-byte OCR
            handleReply(0, 0);
            replyBuf[1] = 0;
            replyBuf[2] = 0xff;  // supports 2.7 to 3.6v, per Matt
            replyBuf[3] = 0x80;  // last bit of that range
            replyBuf[4] = 0;     // not busy
            replyPos = 0;
            replySize = 5;
            stateTimeout = 0;
            break;

        case 0x7b:  // CRC_ON_OFF               - least significant bit (1 = on, 0 = off)
            debug_write("CRC_ON_OFF is not implemented yet.");
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;

        default:
            debug_write("Unknown command: CMD%d (0x%02X)", inputCmd[0]&0x3f, inputCmd[0]);
            handleReply(STATUS_ILLEGAL_COMMAND, 0);
            break;
    }
}

// basic initialization - only need to call it once
void SDInitLibrary() {
    szCardFilename[0]='\0';
    SDReset();
    isHighSpeed = false;
    isChipEnabled = false;

    // at startup we need to sync
    changeState( SYNCING );
    stateTimeout = 10;
}

void SDReset() {
    replyPos = 0;
    replySize = 0;
    inputPos = 0;
    writePos = 0;
    isWriting = false;
    isAppCmd = false;
    blockSize = 512;
    changeState(RXCMD);

    debug_write("SD Card reset.");
}

// registers a card as inserted, pass the filename
// returns true if successfully opened
bool SDInsertCard(char *pData) {
    char tmp[2048];

    if (NULL == pData) {
        SDEjectCard();
        return false;
    }
    strncpy(tmp, pData, sizeof(tmp));
    tmp[sizeof(tmp)-1]='\0';

    SDReset();
    changeState( SYNCING );
    stateTimeout = 10;

    // Try to open the file and return success or failed
    HANDLE hFile = CreateFile(tmp, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile) {
        debug_write("Failed to open SD card '%s'", tmp);
        SDEjectCard();
        return false;
    } else {
        CloseHandle(hFile);
        // fill in the real filename - first byte last to make it a little safer. Still not the right way. ;)
        strcpy(&szCardFilename[1], &tmp[1]);
        szCardFilename[0] = tmp[0];

        debug_write("SD Card '%s' opened...", tmp);
        return true;
    }
}

// frees the data
void SDEjectCard() {
    debug_write("SD Card closed.");
    szCardFilename[0]='\0';
    SDReset();
}

// returns true if the card is inserted
bool SDIsInserted() {
    return (szCardFilename[0] != '\0');
}

// sets or clears the chip enable
void SDSetCE(bool enable) {
    if (enable != isChipEnabled) {
        isChipEnabled = enable;
        debug_write("Setting SD chip enable to %s", enable?"true":"false");
        // clearing chip enable does not stop any operations
        // TODO: does it stop receipt of a command? I think so...
        if (!enable) {
            if (isWriting) {
                debug_write("Write was at %d bytes", writePos);
            } else {
                if (inputPos > 0) {
                    debug_write("Input command was at %d bytes", inputPos);
                }
            }
            inputPos = 0;
            isWriting = false;
            // going to NOT stop readout of a buffer though
        }

        if (enable == false) {
            debug_write("Reads: %d  Writes : %d", SDReads, SDWrites);
        }
    }
}

// not really an SD feature, but tell the code so it can check for initialization sequence errors
void SDSetHighSpeed(bool enable) {
    // TODO: verify that the necessary initialization was done before switching to high speed,
    // emit debug if it's not.
    if (enable != isHighSpeed) {
        isHighSpeed = enable;
        debug_write("Setting SD high speed to %s", enable?"true":"false");
        // no need to re-condition the line, stay in current state
    }
}

// writes a byte and caches the received byte
void SDWrite(unsigned char data) {
    ++SDWrites;

    // make sure we're active
    if (!isChipEnabled) {
        debug_write("Writing 0x%02X to SD while chip enable is not active.", data);
        return;
    }

    if ((inputPos == 0)&&(data == 0xff)) {
        // ignore non-commands
        return;
    }

    // are we writing?
    if (isWriting) {
        if (writePos == -1) {
            // waiting for start block
            if (data == 0xfe) {
                debug_write("Got start block for write data");
                writePos = 0;
            }
        } else {
            // receiving - we expect 514 bytes (512 bytes + 2 bytes CRC)
            writeBuf[writePos++] = data;
            if (writePos == 514) {
                debug_write("Received all data, attempting to write...");
                {
                    bool ok = true;

                    HANDLE hFile = CreateFile(szCardFilename, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (INVALID_HANDLE_VALUE == hFile) {
                        debug_write("Write failed to open SD card file.");
                        ok = false;
                    } else {
                        long DistanceLow = fileOffset&0xffffffff;
                        long DistanceHigh = (fileOffset>>32)&0xffffffff;
                        if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, DistanceLow, &DistanceHigh, FILE_BEGIN)) {
                            debug_write("Write failed to seek SD card file to block %llu", fileOffset);
                            CloseHandle(hFile);
                            ok = false;
                        } else {
                            if (!WriteFile(hFile, &writeBuf[0], blockSize, NULL, NULL)) {
                                debug_write("Failed to write to SD card file");
                                CloseHandle(hFile);
                                ok = false;
                            } else {
                                CloseHandle(hFile);
                                debug_write("Write to file address %llu (sector %llu)", fileOffset, fileOffset/512);
                            }
                        }
                    }

                    if (ok) {
                        replyBuf[0] = 0x05; // data accepted
                    } else {
                        replyBuf[0] = 0x0d; // write error
                    }
                    replyPos = 0;
                    replySize = 1;
                    stateTimeout = 0;
                }
                isWriting = false;
            }
        }
    } else {
        if ((inputPos == 0) && (stateTimeout > 0)) {
            debug_write("Receiving command %sCMD%d (0x%02X) while card is still busy", isAppCmd?"A":"", data&0x3f, data);
        } else if (inputPos == 0) {
            debug_write("Receiving command %sCMD%d (0x%02X)", isAppCmd?"A":"", data&0x3f, data);
        }

        // write the byte
        inputCmd[inputPos++] = data;
        if (inputPos >= 6) {
            if (replyPos < replySize) {
                debug_write("New command received when old command at pos %d", replyPos);
            }
            processCmd();
        }
    }
}

// reads from the card
unsigned char SDRead() {
    ++SDReads;

    if (!isChipEnabled) {
        // this is normal for finishing off a command
        //debug_write("Reading from SD while chip enable is not active.");
        if (stateTimeout > 0) {
            // emulate processing delay
            --stateTimeout;
            return 0x00;    // card busy
        }
        return 0xff;
    }

    if (replyPos >= replySize) {
        if (stateTimeout > 0) {
            // emulate processing delay
            --stateTimeout;
            return 0x00;    // card busy
        }
        // all good
        return 0xff;
    }

    if (replyPos >= sizeof(replyBuf)) {
        debug_write("Emulation error reading SD past end of buffer.");
        return 0xff;
    }

    return replyBuf[replyPos++];
}
