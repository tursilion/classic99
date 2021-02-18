// This doesn't look like it's going to work. The GPL interpreter makes too many assumptions
// about memory layout and way, way too many calls to assembly XML functions to be 100%
// abstracted (without abstracting all the XML calls too)... I don't think it's high on my list
// anymore... but it was an interesting exercise

#if 0

// I was hoping this would work for the RXB accelerator project, but it seems now
// the only hope there is a full emulation on a card, with the TI acting solely as
// dumb terminal. I have a long, long way to go before I can do that...

#include <stdio.h>

// We are assuming in here that we are the only GPL interpreter that matters,
// even if we end up feeding back to a separate host from time to time.
// as such, we hold our own data

unsigned short grmAddress = 0;  // current GROM address
unsigned char scratchPad[256];  // this is overkill, really

// hardware access functions
void gplWriteSound(unsigned char byte) {
}
void gplWriteVDPReg(unsigned char reg, unsigned char byte) {
}
void gplSetVDPAddress(unsigned short adr) {
}
void gplWriteVDPData(unsigned short adr, unsigned char byte) {
}
void gplReadVDPData(unsigned short adr, unsigned char byte) {
}
unsigned char gplReadRawVDP() {
}
void gplSetGROMAddress(unsigned short adr) {
}
unsigned char gplReadGROM(unsigned short adr) {
}
unsigned char gplReadRawGROM() {
}

typedef void (*GPLFctn)(void);
// to avoid eating up valuable RAM, a const array...
const GPLFctn gplOpcode[256] = {     // GPL is just an 8-bit instruction space
    gplRTN,
    gplRTNC,
    gplRAND,
    gplSCAN,
    gplBACK,
    gplB,
    gplCALL,
    gplALL,
    gplFMT,
    gplH,
    gplGT,
    gplEXIT,
    gplCARRY,
    gplOVF,
    gplPARSE,
    gplXML,

    gplCONT,        // 0x10
    gplEXEC,
    gplRTNB,
    gplRTGR,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,

    gplMOVE,        // 0x20
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,

    gplMOVE,        // 0x30
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,
    gplMOVE,

    gplBR,          // 0x40
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,

    gplBR,          // 0x50
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,
    gplBR,

    gplBS,          // 0x60
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,

    gplBS,          // 0x70
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,
    gplBS,

    gplABS,         // 0x80
    gplDABS,
    gplNEG,
    gplDNEG,
    gplINV,
    gplDINV,
    gplCLR,
    gplDCLR,
    gplFETCH,
    gplFETCH,       // incompletely decoded
    gplCASE,
    gplDCASE,
    gplPUSH,
    gplPUSH,        // incompletely decoded
    gplCZ,
    gplDCZ,

    gplINC,         // 0x90
    gplDINC,
    gplDEC,
    gplDDEC,
    gplINCT,
    gplDINCT,
    gplDECT,
    gplDDECT,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL,

    gplADD,         // 0xa0
    gplDADD,
    gplADD,
    gplDADD,
    gplSUB,
    gplDSUB,
    gplSUB,
    gplDSUB,
    gplMUL,
    gplDMUL,
    gplMUL,
    gplDMUL,
    gplDIV,
    gplDDIV,
    gplDIV,
    gplDDIV,

    gplAND,         // 0xb0
    gplDAND,
    gplAND,
    gplDAND,
    gplOR,
    gplDOR,
    gplOR,
    gplDOR,
    gplXOR,
    gplDXOR,
    gplXOR,
    gplDXOR,
    gplST,
    gplDST,
    gplST,
    gplDST,

    gplEX,          // 0xc0
    gplDEX,
    gplEX,
    gplDEX,
    gplCH,
    gplDCH,
    gplCH,
    gplDCH,
    gplCHE,
    gplDCHE,
    gplCHE,
    gplDCHE,
    gplCGT,
    gplDCGT,
    gplCGT,
    gplDCGT,

    gplCGE,         // 0xd0
    gplDCGE,
    gplCGE,
    gplDCGE,
    gplCEQ,
    gplDCEQ,
    gplCEQ,
    gplDCEQ,
    gplCLOG,
    gplDCLOG,
    gplCLOG,
    gplDCLOG,
    gplSRA,
    gplDSRA,
    gplSRA,
    gplDSRA,

    gplSLL,         // 0xe0
    gplDSLL,
    gplSLL,
    gplDSLL,
    gplSRL,
    gplDSRL,
    gplSRL,
    gplDSRL,
    gplSRC,
    gplDSRC,
    gplSRC,
    gplDSRC,
    gplCOINC,       // incomplete decode
    gplCOINC,
    gplCOINC,
    gplCOINC,

    gplXGPL,        // 0xf0
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplIO,
    gplDIO,         // wait, IS there a DIO?
    gplIO,
    gplDIO,
    gplSWGR,
    gplDSWGR,       // IS there a DSWGR?
    gplSWGR,
    gplDSWGR,
    gplXGPL,
    gplXGPL,
    gplXGPL,
    gplXGPL
};

/*
The destination operands and source operands have five different forms:

MSB 0 1 2 3 4 5 6 7 LSB
1   0 Address       CPU RAM is directly addressed >8300 through >837F
2   1 0 V I Address V --> 0=CPU RAM, 1=VDP RAM
	Address         I --> 0=Direct, 1=Indirect
3   1 1 V I Address Same as number 2, but indexed
	Address
	Index
4   1 0 V I 1 1 1 1 Extended area at 0 through 65535
	Address         Address with offset >8300, i.e.
	Address         >DD00 corresponds to address >6000.
5   1 1 V I 1 1 1 1 Like number 4, only indexed
	Address
	Address
	Index
*/

void gplRTN() {
    /*
    Op-Code: >00 Description: RTN
    Format type:3
    Description: Takes the highest value of the substacks and sets the
    program counter ( new GROM address ). Reset condition-bit into
    status byte.
    GPL-Statusbyte: Condition-bit reset
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/

}

void gplRTNC() {
    /*
    Op-Code: >01 Description: RTNC
    Format type:3
    Description: Same as RTN but the condition-bit is not influenced.
    GPL-Statusbyte: Not influenced
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/

}

void gplRAND() {
    /*
	Op-Code: >02 Description: RAND IMM
	Format type:2 Result: Random number at >8378
	Description: Creates a random number at >8378. The IMM shows the
	maximum. The minimum is always 0.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
	
}

void gplSCAN() {
    /*
	Op-Code: >03 Description: SCAN
	Format type:3 Result: Key value at >8375, Joystick
	values at >8376,>8377.
	Description: Scans the keyboard (Modus at >8374) and sets the
	corresponding values at >8375 ( Keys ) and >8376/>8377
	( Joystick ).
	GPL-Statusbyte: Condition-bit is set by pushing a new key.
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplBACK() {
    /*
	Op-Code: >04 Description: BACK IMM
	Format type:2 Result: VDP-Register 7 = IMM
	Description: Sets the background color of the screen on the value
	of IMM.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
	
}

void gplB() {
    /*
	Op-Code: >05 Description: B IMM (2 Bytes)
	Format type:2
	Description: Jump to the absolute address of the IMM-value.
	Program counter takes the value of IMM.
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
	
}

void gplCALL() {
    /*
	Op-Code: >06 Description: CALL IMM (2 Bytes)
	Format type:2
	Description: Jump to a subroutine. The program counter takes the
	value of IMM, the old value of the program counter is stored on
	the substack.
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
	
}

void gplALL() {
    /*
	Op-Code: >07 Description: ALL IMM
	Format type:2
	Description: Screen is filled with the IMM value.
	GPL-Statusbyte: Not changed
    */

	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
	
}

void gplFMT() {
    /*
    Op-Code: >08 Description: FMT several operands
	Description: Special output command for the screen. The FMT
	Interpreter is independent of the GPL Interpreter. ( See ROMListing >04DE through >05A1 )
	GPL-Statusbyte: Not influenced
    */
}

void gplH() {
    /*
	Op-Code: >09 Description: H
	Format type:3 Result:Condition-Bit = H-Bit
	Description: Checks the High-Bit in GPL-Statusbyte and sets the
	Condition-Bit accordingly.
	GPL-Statusbyte: Condition-Bit is set on value of High-Bit.
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplGT() {
    /*
	Op-Code: >0A Description: GT
	Format type:3 Result:Condition-Bit = GT-Bit
	Description: Checks the Greater-Bit in the GPL-Status Byte and
	sets the Condition-Bit accordingly.
	GPL-Statusbyte: Condition-Bit is set on value of Greater-Bit.
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplEXIT() {
    /*
	Op-Code: >0B Description: EXIT
	Format type:3
	Description: Software reset, returns to Master Titel Screen or
	Power-up routine.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplCARRY() {
    /*
	Op-Code: >0C Description: CARRY
	Format type:3 Result:Condition-Bit = Carry-Bit
	Description: Tests the Carry-Bit in the GPL-Statusbyte and sets
	the Condition-Bit accordingly.
	GPL-Statusbyte: Condition-Bit is set on value of Carry-Bit.
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplOVF() {
    /*
	Op-Code: >0D Description: OVF
	Format-type:3 Result:Condition-Bit = OVF-Bit
	Description: Tests the Overflow-Bit in the GPL-Statusbyte and sets
	the Condition-Bit accordingly.
	GPL-Statusbyte: Condition-Bit is set on value of Overflow-Bit.
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplPARSE() {
    /*
	Op-Code: >0E Description: PARSE IMM
	Format type:2
	Description: Extension in Basic-Interpreter until a caracter
	appears in Basic which is smaller than the IMM value or decimal
	point. Is mostly used in GPL subprograms for Basic to store
	contents of variable on FAC.
    */

	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
}

void gplXML() {
    /*
	Op-Code: >0F Description: XML IMM
	Format type:2
	Description: Execution of a assembler routine according to the
	table values of XMR. ( See explanation to ROM )
	GPL-Statusbyte: Depends on assembler routine
    */


	/*
	Format type 2:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	Immediate operand (byte or word)
	*/
}

void gplCONT() {
    /*
	Op-Code: >10 Description: CONT
	Format type:3
	Description: Leads back to the Basic-Interpreter, is used at the
	end of the Basic routines ( not the subprogram ) which are located
	in GROM.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplEXEC() {
    /*
	Op-Code: >11 Description: EXEC
	Format type:3
	Description: Starts with execution of a Basic program.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplRTNB() {
    /*
	Op-Code: >12 Description: RTNB
	Format type:3
	Description: Leads back to Basic interpreter, return-address on
	substack.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplRTGR() {
    /*
	Op-Code: >13 Description: RTGR
	Format type:3
	Description: Takes the old GRMRD and the old program counter from
	the substack and resets GROM.
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 3:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 0 X X X X X
	*/
}

void gplXGPL() {
    /*
	Op-Code: >14->1F Description: XGPL
	>98->9F
	>F0->F4
	>FC->FF
	Description: For GPL etensions. Contains in the interpreter up to
	now, is starting of a DSR on CRU address >1B00 and then the jump B
	>4020 or b >401C ( Code 1F ).
	GPL-Statusbyte:
    */
}

void gplMOVE() {
    /*
	Op-Code: >20->3F Description: MOVE S1, from S2 to D
	Format type:6
	Description: Moves certain numbers of bytes (S1) from address S2
	to destination address (D). The VDP register as well as GROM
	address can also be used as destination address.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 6:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 0 1 G R V C N
	Number
	Destination operand
	source operand

	G --> 0=Destination operand is GROM(only possible with GRAMS)
	1=Destination operand is no GROM
	R --> 0=Destination operand is not a VDP register
	1=Destination operand is VDP register
	V --> 0=Source operand is not the VDP RAM or CPU RAM
	1=Source operand is the VDP RAM or CPU RAM
	C --> 0=Source is not GROM addressed over CPU RAM
	1=Source is GROM indicated or addressed over
	CPU RAM
	N --> 0=Number is not direct operand
	1=Number is direct operand
	*/

}

void gplBR() {
    /*
	Op-Code: >40->5F Description: BR IMM
	Format type:4
	Description: Jumps to a certain address (only possible within a
	GROM) when the condition bit has not been set.
	GPL-Statusbyte: Condition bit reset
    */

	/*
	Format type 4:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 1 X Address
	Address (1 byte)
	*/
}

void gplBS() {
    /*
	Op-Code: >60->7F Description: BS IMM
	Format type:4
	Description: Jumps to a certain address (only possible in a GROM)
	when condition bit is set.
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 4:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 0 1 X Address
	Address (1 byte)
	*/
}

void gplABS() {
    /*
	Op-Code: >80->81 Description: ABS D
	DABS D
	Format type:5 Result: D = ABS(D)
	Description: Replaces D by the absolut value of D
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDABS() {
    /*
	Op-Code: >80->81 Description: ABS D
	DABS D
	Format type:5 Result: D = ABS(D)
	Description: Replaces D by the absolut value of D
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplNEG() {
    /*
	Op-Code: >82->83 Description: NEG D
	DNEG D
	Format type:5 Result: D = -D
	Description: Replaces destination by the two´s compliment of
	destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDNEG() {
    /*
	Op-Code: >82->83 Description: NEG D
	DNEG D
	Format type:5 Result: D = -D
	Description: Replaces destination by the two´s compliment of
	destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplINV() {
    /*
	Op-Code: >84->85 Description: INV D
	_DINV D
	Format type:5 Result: D = D
	Description: Inverts each bit in the destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDINV() {
    /*
	Op-Code: >84->85 Description: INV D
	_DINV D
	Format type:5 Result: D = D
	Description: Inverts each bit in the destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplCLR() {
    /*
	Op-Code: >86->87 Description: CLR D
	DCLR D
	Format type:5 Result: D = 0
	Description: Sets destination on 0
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDCLR() {
    /*
	Op-Code: >86->87 Description: CLR D
	DCLR D
	Format type:5 Result: D = 0
	Description: Sets destination on 0
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplFETCH() {
    /*
	Op-Code: >88-(>89) Description: FETCH D
	incompletely decoded
	Format type:5
	Description: Fetches a byte on which the return address on the
	subroutine-stack shows. Puts this byte in the destination and
	increases the return address on the substack by 1.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplCASE() {
    /*
	Op-Code: >8A->8B Description: CASE D
	DCASE D
	Format type:5
	Description: Adds twice the value of D to the Program Counter
	(GROM address)
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDCASE() {
    /*
	Op-Code: >8A->8B Description: CASE D
	DCASE D
	Format type:5
	Description: Adds twice the value of D to the Program Counter
	(GROM address)
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplPUSH() {
    /*
	Op-Code: >8C-(>8D) Description: PUSH D
	incompletely decoded
	Format type:5
	Description: Puts bits from D on the GPL data stack and increases
	data stack pointer one point. A pop can be realized with ST
	*>837C,D. ( specialty in the interpreter )
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplCZ() {
    /*
	Op-Code: >8E->8F Description: CZ D
	DCZ D
	Format type:5 Result: Condition-bit = 1 if D = 0
	Description: Compairs D with 0 and sets the condition bit, if D
	equals 0.
	GPL-Statusbyte: Condition-Bit set if S = 0
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDCZ() {
    /*
	Op-Code: >8E->8F Description: CZ D
	DCZ D
	Format type:5 Result: Condition-bit = 1 if D = 0
	Description: Compairs D with 0 and sets the condition bit, if D
	equals 0.
	GPL-Statusbyte: Condition-Bit set if S = 0
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplINC() {
    /*
	Op-Code: >90->91 Description: INC D
	DINC D
	Format type:5 Result: D = D + 1
	Description: Increases D by one
	GPL-Statusbyte: GPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDINC() {
    /*
	Op-Code: >90->91 Description: INC D
	DINC D
	Format type:5 Result: D = D + 1
	Description: Increases D by one
	GPL-Statusbyte: GPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDEC() {
    /*
	Op-Code: >92->93 Description: DEC D
	DDEC D
	Format type:5 Result: D = D - 1
	Description: One point is substraction from D
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDDEC() {
    /*
	Op-Code: >92->93 Description: DEC D
	DDEC D
	Format type:5 Result: D = D - 1
	Description: One point is substraction from D
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplINCT() {
    /*
	Op-Code: >94->95 Description: INCT D
	DINCT D
	Format type:5 Result: D = D + 2
	Description: D increased by 2
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDINCT() {
    /*
	Op-Code: >94->95 Description: INCT D
	DINCT D
	Format type:5 Result: D = D + 2
	Description: D increased by 2
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDECT() {
    /*
	Op-Code: >96->97 Description: DECT D
	DDECT D
	Format type:5 Result: D = D - 2
	Description: D decreased by 2
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplDDECT() {
    /*
	Op-Code: >96->97 Description: DECT D
	DDECT D
	Format type:5 Result: D = D - 2
	Description: D decreased by 2
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 5:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 0 0 X X X X W
	Destination operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	*/
}
void gplADD() {
    /*
	Op-Code: >A0->A3 Description: ADD S,D
	DADD S,D
	Format type:1 Result: D = S + D
	Description: Adds source to destination and stores the result in
	the dfestination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDADD() {
    /*
	Op-Code: >A0->A3 Description: ADD S,D
	DADD S,D
	Format type:1 Result: D = S + D
	Description: Adds source to destination and stores the result in
	the dfestination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplSUB() {
    /*
	Op-Code: >A4->A7 Description: SUB S,D
	DSUB S,D
	Format type:1 Result: D = D - S
	Description: Substracts source from destination and stores the
	result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */
	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDSUB() {
    /*
	Op-Code: >A4->A7 Description: SUB S,D
	DSUB S,D
	Format type:1 Result: D = D - S
	Description: Substracts source from destination and stores the
	result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplMUL() {
    /*
	Op-Code: >A8->AB Description: MUL S,D
	DMUL S,S
	Format type:1 Result: D(D,D+1) = S * D
	or at Word: D(D,D+2) = S * D
	Description: Source and destination are multiplied by each other.
	During a byte operation, the result is stored in the word of the
	destination; during a word operation, the result is stored in 2
	words of the destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDMUL() {
    /*
	Op-Code: >A8->AB Description: MUL S,D
	DMUL S,S
	Format type:1 Result: D(D,D+1) = S * D
	or at Word: D(D,D+2) = S * D
	Description: Source and destination are multiplied by each other.
	During a byte operation, the result is stored in the word of the
	destination; during a word operation, the result is stored in 2
	words of the destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDIV() {
    /*
	Op-Code: >AC->AF Description: DIV S,D
	DDIV S,D
	Format type:1 Result: D=D(D,D+1)/S ; D+1=remainder
	or at word always +2
	Description: Replaces the destination by the quocient of the
	destination by the source and the destination +1 (at word
	destination +2) by the remains of the destination devided by
	source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDDIV() {
    /*
	Op-Code: >AC->AF Description: DIV S,D
	DDIV S,D
	Format type:1 Result: D=D(D,D+1)/S ; D+1=remainder
	or at word always +2
	Description: Replaces the destination by the quocient of the
	destination by the source and the destination +1 (at word
	destination +2) by the remains of the destination devided by
	source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplAND() {
    /*
    	Op-Code: >B0->B3 Description: AND S,D
	DAND S,D
	Format type:1 Result: D = S AND D
	Description: Executes a AND operation by bits and stores the
	result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDAND() {
    /*
    	Op-Code: >B0->B3 Description: AND S,D
	DAND S,D
	Format type:1 Result: D = S AND D
	Description: Executes a AND operation by bits and stores the
	result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplOR() {
    /*
	Op-Code: >B4->B7 Description: OR S,D
	DOR S,D
	Format type:1 Result: D = S OR D
	Description: Executes an OR operation by bits and stores the
	result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDOR() {
    /*
	Op-Code: >B4->B7 Description: OR S,D
	DOR S,D
	Format type:1 Result: D = S OR D
	Description: Executes an OR operation by bits and stores the
	result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplXOR() {
    /*
	Op-Code: >B8->BB Destination: XOR S,D
	DXOR S,D
	Format type:1 Result: D = S EXOR D
	Description: Executes an exclusive OR operation by bits and stores
	the result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDXOR() {
    /*
	Op-Code: >B8->BB Destination: XOR S,D
	DXOR S,D
	Format type:1 Result: D = S EXOR D
	Description: Executes an exclusive OR operation by bits and stores
	the result in the destination.
	GPL-Statusbyte: CPU-Statusbyte equals GPL-Statusbyte
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplST() {
    /*
	Op-Code: >BC->BF Description: ST S,D
	DST S,D
	Format type:1 Result: D = S
	Description: Replaces the destination by the source operand.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDST() {
    /*
	Op-Code: >BC->BF Description: ST S,D
	DST S,D
	Format type:1 Result: D = S
	Description: Replaces the destination by the source operand.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplEX() {
    /*
	Op-Code: >C0->C3 Description: EX S,D
	Format type:1 Result: D = S, S = D
	Description: Exchanges source and destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDEX() {
    /*
	Op-Code: >C0->C3 Description: EX S,D
	Format type:1 Result: D = S, S = D
	Description: Exchanges source and destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCH() {
    /*
	Op-Code: >C4->C7 Description: CH S,D
	DCH S,D
	Format type:1
	Description: Compares source and destination and sets the
	condition-Bit when the destination is logically greater than the
	source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDCH() {
    /*
	Op-Code: >C4->C7 Description: CH S,D
	DCH S,D
	Format type:1
	Description: Compares source and destination and sets the
	condition-Bit when the destination is logically greater than the
	source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCHE() {
    /*
	Op-Code: >C8->CB Description: CHE S,D
	DCHE S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit when the destination is logically greater than or
	equal to the source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDCHE() {
    /*
	Op-Code: >C8->CB Description: CHE S,D
	DCHE S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit when the destination is logically greater than or
	equal to the source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCGT() {
    /*
	Op-Code: >CC->CF Destination: CGT S,D
	DCGT S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit if the destination is arithmetically greater than
	the source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDCGT() {
    /*
	Op-Code: >CC->CF Destination: CGT S,D
	DCGT S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit if the destination is arithmetically greater than
	the source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCGE() {
    /*
	Op-Code: >D0->D3 Destination: CGE S,D
	DCGE S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit if the destination is greater than or equal to the
	source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDCGE() {
    /*
	Op-Code: >D0->D3 Destination: CGE S,D
	DCGE S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit if the destination is greater than or equal to the
	source.
	GPL-Statusbyte: Condition-Bit set according to comparison.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCEQ() {
    /*
	Op-Code: >D4->D7 Description: CEQ S,D
	DCEQ S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit if the destination and the source are equal.
	GPL-Statusbyte: Condition-Bit set if source and destination are
	equal.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDCEQ() {
    /*
	Op-Code: >D4->D7 Description: CEQ S,D
	DCEQ S,D
	Format type:1
	Description: Compares source and destination and sets the
	Condition-Bit if the destination and the source are equal.
	GPL-Statusbyte: Condition-Bit set if source and destination are
	equal.
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCLOG() {
    /*
	Op-Code: >D8->DB Description: CLOG S,D
	DCLOG S,D
	Format type:1
	Description: Executes an AND operation by bits between destination
	and source and sets the Condition-Bit if the result is 0.
	GPL-Statusbyte: Condition-Bit set if S AND D = 0
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDCLOG() {
    /*
	Op-Code: >D8->DB Description: CLOG S,D
	DCLOG S,D
	Format type:1
	Description: Executes an AND operation by bits between destination
	and source and sets the Condition-Bit if the result is 0.
	GPL-Statusbyte: Condition-Bit set if S AND D = 0
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplSRA() {
    /*
	Op-Code: >DC->DF Description: SRA S,D
	DSRA S,D
	Format type:1
	Description: The contents of the destination is moved to the right
	according to the number of bits of the source. The empty bit
	digits are filled with the MSB of the destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDSRA() {
    /*
	Op-Code: >DC->DF Description: SRA S,D
	DSRA S,D
	Format type:1
	Description: The contents of the destination is moved to the right
	according to the number of bits of the source. The empty bit
	digits are filled with the MSB of the destination.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplSLL() {
    /*
	Op-Code: >E0->E3 Description: SLL S,D
	DSLL S,D
	Format type:1
	Description: The contents of the destination is moved to the left
	according to the number of bits of the source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDSLL() {
    /*
	Op-Code: >E0->E3 Description: SLL S,D
	DSLL S,D
	Format type:1
	Description: The contents of the destination is moved to the left
	according to the number of bits of the source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplSRL() {
    /*
	Op-Code: >E4->E7 Description: SRL S,D
	DSRL S,D
	Format type:1
	Description: The contents of the destination is moved to the left
	according to the number of bits of the source.
	GPL-Statusbyte: Not influenced
    	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDSRL() {
    /*
	Op-Code: >E4->E7 Description: SRL S,D
	DSRL S,D
	Format type:1
	Description: The contents of the destination is moved to the left
	according to the number of bits of the source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplSRC() {
    /*
	Op-Code: >E8->EB Description: SRC S,D
	DSRC S,D
	Format type:1
	Description: The contents of the destination will be cyclically
	moved to the right according to the number of bits in the source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDSRC() {
    /*
	Op-Code: >E8->EB Description: SRC S,D
	DSRC S,D
	Format type:1
	Description: The contents of the destination will be cyclically
	moved to the right according to the number of bits in the source.
	GPL-Statusbyte: Not influenced
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplCOINC() {
    /*
	Op-Code: >ED Description: COINC S,D
	incompletely decoded from >EC to >EF
	Format type:1
	Description: Condition-Bit is set if points of 2 objects on the
	screen overlap. COINC requires special tables in GROM.
	GPL-Statusbyte: Condition-Bit set in case of overlapping
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplIO() {
    /*
	Op-Code: >F4->F7 Description: I/O S,D
	Format type:1
	Description: I/O is a special command. The destination is the
	address of a list whose format depends on the output or input
	function. The source choses the function. Today the following
	values are permitted:
	0 = Sound in GROM
	1 = Sound in VDP
	2 = CRU input
	3 = CRU output
	4 = Write cassette
	5 = Read cassette
	6 = Verify cassette
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDIO() {
    /*
	Op-Code: >F4->F7 Description: I/O S,D
	Format type:1
	Description: I/O is a special command. The destination is the
	address of a list whose format depends on the output or input
	function. The source choses the function. Today the following
	values are permitted:
	0 = Sound in GROM
	1 = Sound in VDP
	2 = CRU input
	3 = CRU output
	4 = Write cassette
	5 = Read cassette
	6 = Verify cassette
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplSWGR() {
    /*
	Op-Code: >F8->FB Description: SWGR S,D
	Format type:1
	Description: Switches the GROM-read address (CPU). The source is
	the new GRMRD and destination is the program counter (GROM). Old
	PC and GRMRD are put on substack.
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}
void gplDSWGR() {
    /*
	Op-Code: >F8->FB Description: SWGR S,D
	Format type:1
	Description: Switches the GROM-read address (CPU). The source is
	the new GRMRD and destination is the program counter (GROM). Old
	PC and GRMRD are put on substack.
	GPL-Statusbyte: Condition-Bit reset
    */

	/*
	Format type 1:
	MSB 0 1 2 3 4 5 6 7 LSB
	- - - - - - - -
	1 Byte 1 X X X X X U W
	Destination operand
	Source operand

	W --> 0=Byte operation
	1=Word operation (2 bytes)
	U --> 0=No direct operand
	1=Direct operand (IMM)

	*/	
}

// Interpreter functions...

#if 0
void buildGPL() {
    for (int idx=0; idx<256; ++idx) {
        gplOpcode[idx] = NULL;  // should be no gaps...
    }

    gplOpcode[0x00] = gplRTN;
    gplOpcode[0x01] = gplRTNC;
    gplOpcode[0x02] = gplRAND;
    gplOpcode[0x03] = gplSCAN;
    gplOpcode[0x04] = gplBACK;
    gplOpcode[0x05] = gplB;
    gplOpcode[0x06] = gplCALL;
    gplOpcode[0x07] = gplALL;
    gplOpcode[0x08] = gplFMT;
    gplOpcode[0x09] = gplH;
    gplOpcode[0x0a] = gplGT;
    gplOpcode[0x0b] = gplEXIT;
    gplOpcode[0x0c] = gplCARRY;
    gplOpcode[0x0d] = gplOVF;
    gplOpcode[0x0e] = gplPARSE;
    gplOpcode[0x0f] = gplXML;

    gplOpcode[0x10] = gplCONT;        // 0x10
    gplOpcode[0x11] = gplEXEC;
    gplOpcode[0x12] = gplRTNB;
    gplOpcode[0x13] = gplRTGR;

    for (int idx=0x14; idx<=0x1f; ++idx) gplOpcode[idx] = gplXGPL;

    for (int idx=0x20; idx<=0x3f; ++idx) gplOpcode[idx] = gplMOVE;

    for (int idx=0x40; idx<=0x5f; ++idx) gplOpcode[idx] = gplBR;
    for (int idx=0x60; idx<=0x7f; ++idx) gplOpcode[idx] = gplBS;

    gplOpcode[0x80] = gplABS;
    gplOpcode[0x81] = gplDABS;
    gplOpcode[0x82] = gplNEG;
    gplOpcode[0x83] = gplDNEG;
    gplOpcode[0x84] = gplINV;
    gplOpcode[0x85] = gplDINV;
    gplOpcode[0x86] = gplCLR;
    gplOpcode[0x87] = gplDCLR;
    gplOpcode[0x88] = gplFETCH;
    gplOpcode[0x89] = gplFETCH; // incompletely decoded
    gplOpcode[0x8a] = gplCASE;
    gplOpcode[0x8b] = gplDCASE;
    gplOpcode[0x8c] = gplPUSH;
    gplOpcode[0x8d] = gplPUSH;  // incompletely decoded
    gplOpcode[0x8e] = gplCZ;
    gplOpcode[0x8f] = gplDCZ;
    gplOpcode[0x90] = gplINC;
    gplOpcode[0x91] = gplDINC;
    gplOpcode[0x92] = gplDEC;
    gplOpcode[0x93] = gplDDEC;
    gplOpcode[0x94] = gplINCT;
    gplOpcode[0x95] = gplDINCT;
    gplOpcode[0x96] = gplDECT;
    gplOpcode[0x97] = gplDDECT;

    for (int idx=0x98; idx<=0x9f; ++idx) gplOpcode[idx] = gplXGPL;

    gplOpcode[0xa0] = gplADD;
    gplOpcode[0xa1] = gplDADD;
    gplOpcode[0xa2] = gplADD;
    gplOpcode[0xa3] = gplDADD;
    gplOpcode[0xa4] = gplSUB;
    gplOpcode[0xa5] = gplDSUB;
    gplOpcode[0xa6] = gplSUB;
    gplOpcode[0xa7] = gplDSUB;
    gplOpcode[0xa8] = gplMUL;
    gplOpcode[0xa9] = gplDMUL;
    gplOpcode[0xaa] = gplMUL;
    gplOpcode[0xab] = gplDMUL;
    gplOpcode[0xac] = gplDIV;
    gplOpcode[0xad] = gplDDIV;
    gplOpcode[0xae] = gplDIV;
    gplOpcode[0xaf] = gplDDIV;
    gplOpcode[0xb0] = gplAND;
    gplOpcode[0xb1] = gplDAND;
    gplOpcode[0xb2] = gplAND;
    gplOpcode[0xb3] = gplDAND;
    gplOpcode[0xb4] = gplOR;
    gplOpcode[0xb5] = gplDOR;
    gplOpcode[0xb6] = gplOR;
    gplOpcode[0xb7] = gplDOR;
    gplOpcode[0xb8] = gplXOR;
    gplOpcode[0xb9] = gplDXOR;
    gplOpcode[0xba] = gplXOR;
    gplOpcode[0xbb] = gplDXOR;
    gplOpcode[0xbc] = gplST;
    gplOpcode[0xbd] = gplDST;
    gplOpcode[0xbe] = gplST;
    gplOpcode[0xbf] = gplDST;
    gplOpcode[0xc0] = gplEX;
    gplOpcode[0xc1] = gplDEX;
    gplOpcode[0xc2] = gplEX;
    gplOpcode[0xc3] = gplDEX;
    gplOpcode[0xc4] = gplCH;
    gplOpcode[0xc5] = gplDCH;
    gplOpcode[0xc6] = gplCH;
    gplOpcode[0xc7] = gplDCH;
    gplOpcode[0xc8] = gplCHE;
    gplOpcode[0xc9] = gplDCHE;
    gplOpcode[0xca] = gplCHE;
    gplOpcode[0xcb] = gplDCHE;
    gplOpcode[0xcc] = gplCGT;
    gplOpcode[0xcd] = gplDCGT;
    gplOpcode[0xce] = gplCGT;
    gplOpcode[0xcf] = gplDCGT;
    gplOpcode[0xd0] = gplCGE;
    gplOpcode[0xd1] = gplDCGE;
    gplOpcode[0xd2] = gplCGE;
    gplOpcode[0xd3] = gplDCGE;
    gplOpcode[0xd4] = gplCEQ;
    gplOpcode[0xd5] = gplDCEQ;
    gplOpcode[0xd6] = gplCEQ;
    gplOpcode[0xd7] = gplDCEQ;
    gplOpcode[0xd8] = gplCLOG;
    gplOpcode[0xd9] = gplDCLOG;
    gplOpcode[0xda] = gplCLOG;
    gplOpcode[0xdb] = gplDCLOG;
    gplOpcode[0xdc] = gplSRA;
    gplOpcode[0xdd] = gplDSRA;
    gplOpcode[0xde] = gplSRA;
    gplOpcode[0xdf] = gplDSRA;
    gplOpcode[0xe0] = gplSLL;
    gplOpcode[0xe1] = gplDSLL;
    gplOpcode[0xe2] = gplSLL;
    gplOpcode[0xe3] = gplDSLL;
    gplOpcode[0xe4] = gplSRL;
    gplOpcode[0xe5] = gplDSRL;
    gplOpcode[0xe6] = gplSRL;
    gplOpcode[0xe7] = gplDSRL;
    gplOpcode[0xe8] = gplSRC;
    gplOpcode[0xe9] = gplDSRC;
    gplOpcode[0xea] = gplSRC;
    gplOpcode[0xeb] = gplDSRC;

    for (int idx=0xec; idx<=0xef; ++idx) gplOpcode[idx] = gplCOINC; // incompletely decoded

    for (int idx=0xf0; idx<=0xf3; ++idx) gplOpcode[idx] = gplXGPL;

    gplOpcode[0xf4] = gplIO;
    gplOpcode[0xf5] = gplDIO;   // wait, IS there a DIO?
    gplOpcode[0xf6] = gplIO;
    gplOpcode[0xf7] = gplDIO;

    gplOpcode[0xf8] = gplSWGR;
    gplOpcode[0xf9] = gplDSWGR; // IS there a DSWGR?
    gplOpcode[0xfa] = gplSWGR;
    gplOpcode[0xfb] = gplDSWGR;

    for (int idx=0xfc; idx<=0xff; ++idx) gplOpcode[idx] = gplXGPL;
}
#endif

void gplInit() {
    // in case we need to initialize the hardware as well, like for Coleco...
    memset(scratchPad, 0, sizeof(scratchPad));


// start GPL from scratch, at the requested address
void gplStart(int adr) {
    grmAddress = adr;

    // the console sort of does these IN GPL, but we are not
    // expecting to run the console GROM... so no need for hardware setup
    scratchPad[0x72] = 0xfe;  // data stack at >83fe
    scratchPad[0x73] = 0x7e;  // subroutine stack at >837e

    // run until we get a reason to quit
    gplRun();
}

// continue GPL from whatever the address is now
void gplRun() {
    unsigned char opcode = 

}

#endif
