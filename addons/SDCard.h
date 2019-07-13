#pragma once
// Quick and dirty SPI SD card emulation

// API
void SDInitLibrary();                     // basic initialization
void SDReset();                           // reset the card to default, but don't eject it if inserted

bool SDInsertCard(char *pFilename);       // registers a card as inserted, returns true if file could be opened
void SDEjectCard();                       // frees the data
bool SDIsInserted();                      // returns true if the card is inserted

void SDSetCE(bool enable);                // sets or clears the chip enable
void SDSetHighSpeed(bool enable);         // not really an SD feature, but tell the code so it can check for initialization sequence errors

void SDWrite(unsigned char data);         // writes a byte and caches the received byte
unsigned char SDRead();                   // reads the last byte read from the card

// Theory of OP: https://openlabpro.com/guide/interfacing-microcontrollers-with-sd-card/
//
// Every command to the card is 6 bytes long.
// Card communication is bi-directional, every byte written also retrieves a byte
// However, we normally just write a 0xff to read data after a command
// The CRC byte can be 0xff for 'don't care'.
// Chip select must go to active before the command, and held for the entire command.
// The Phoenix hardware deals with the bit level control, so we only need to manage bytes.
// 
// After insertion, the console should send 10 0xff dummy bytes. It MUST send at least one. Warn if we don't.
// The SD card needs this to lock onto the clock.
// 
// For each command:
// - send one 0xff dummy byte
// - send command: one byte command, 4 bytes data, one byte CRC (or 0xff)
// - send dummy bytes and read response until reply (0x80 is zeroed)
// - timeout if no response
// - send one dummy byte to complete internal operations
// - If the card is 'busy', then the host must continue clocking until it's not!
// - most significant bit first
//
// Regarding the timeout - the card should ALWAYS respond. 250ms is considered the maximum timeout (for write/erase).
// If you don't get a reply, assume the card has crashed.
// 
// Response bits:
// 0 P A E C I R X
// - 0 = always zero
// - P = parameter error
// - A = address error
// - E = erase sequence error
// - C = Command CRC error
// - I = Illegal command
// - R = erase Reset
// - X = Idle state
//
// Non-error responses are either 0x01 (card has gone to idle) or 0x00 (card is waiting for commanded event)
//
// Sequence: (command, data, CRC if known (CRC is disabled in SPI by default, except for CMD0))
// CMD0:    0x40, 0x00000000, 0x95     (Software reset. Expect 0x01. CRC MUST be valid, as this cmd is sent in SD card mode, not SPI)
// CMD58:   Read OCR (fill in data)    (SD only, gives valid voltages and busy bit - wait till not busy)
// CMD8:    0x48, 0x000001AA, 0x87     (V2 only, check voltage. 0x01 means v2 card, 0x05 means v1 or MMC)
// CMD55:   0x77, 0x00000000, 0xff     (prepares for ACMD - not sure this sequence. if fails, go straight to CMD1)
// ACMD41:  0x69, 0x00000000, 0xff     (SD only, init. not sure the sequence..??? Wait for 0x00. If it fails, send CMD1 instead)
// CMD1:    0x41, 0x00000000, 0xff     (SD or MMC, init. Wait for 0x00. Init can take several hundred milliseconds)
// CMD16:   0x50, 0x00000200, 0xff     (set block size to 512 bytes. Wait for 0x00 (optional, spec notes 512 byte is the only valid size))

// Read/Write (probably don't need to write...?)
// CMD17:   0x51, <block adr>, 0xff    (read one block, wait for 0x00, then wait for 0xfe which is start of data. then read data).
// CMD18:   0x52, <block adr>, 0xff    (same as above, but keeps reading until you send CMD12 (0x4c).)
// CMD12:   0x4C, 0x00000000 , 0xff    (stop reading)
// CMD24:   0x58, <block adr>, 0xff    (write one block, wait for 0x00. Send 0xfe and then your data, wait for idle?) 
// CMD25:   0x59, <block adr>, 0xff    (write multiple blocks, same process. 0xfd at the END of each block. CMD13 (0x4d) mandatory to save. Remember dummy bytes between.)
// CMD13:   0x4D, 0x00000000 , 0xff    (stop writing, commit, send status)

// Other interesting commands:
// ACMD13:  0x4D, 0x00000000 , 0xff <-- read status (only 16 bits available in SPI mode)

// *** OCR ***
// This is important to know whether we can use the SD card, although I think it's expected all will be in line
// One bit is reserved for each valid voltage range, plus one bit for busy (used to detect end of powerup)
//
// bits 0-3 reserved
// bit 4    1.6-1.7v
// bit 5    1.7-1.8v
// bit 6    1.8-1.9v
// bit 7    1.9-2.0v
// bit 8    2.0-2.1v
// bit 9    2.1-2.2v
// bit 10   2.2-2.3v
// bit 11   2.3-2.4v
// bit 12   2.4-2.5v
// bit 13   2.5-2.6v
// bit 14   2.6-2.7v
// bit 15   2.7-2.8v
// bit 16   2.8-2.9v
// bit 17   2.9-3.0v
// bit 18   3.0-3.1v
// bit 19   3.1-3.2v
// bit 20   3.2-3.3v
// bit 21   3.3-3.4v
// bit 22   3.4-3.5v
// bit 23   3.5-3.6v
// bit 23-30 reserved
// bit 31   busy
//
// Bits are active low. Do not access the memory array if the voltage range is incorrect (bit 20/21, I guess?)

// *** CID ***
// Card IDentification register
//
// Return 16 bytes:
// 1 byte - Manufacturer ID (3 = Sandisk)
// 2 bytes- Card OEM ASCII ("SD" = Sandisk)
// 5 bytes- Product name (ie: "SD128")
// 1 byte - Revision (BCD) (ie: 0x30 for 3.0)
// 4 bytes- serial number (unsigned 32-bit int)
// 2 bytes- Manufacture data code (0yym in bcd)
// 1 byte - CRC7 checksum with MSb set to '1' (see SanDisk document for formula)


// Check the spec I downloaded to see when we're allowed to raise the speed and verify the init sequence



