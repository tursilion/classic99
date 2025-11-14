//
// (C) 2014 Mike Brent aka Tursi aka HarmlessLion.com
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
// Provides a UDP/IP accessible I/O interface to the system
// All memory can be read or written. How the external tool
// uses it is up to that tool. (This is mostly used by
// me for testing external code that needs the hardware
// handy ;) ). Must be manually enabled in Classic99.ini 
// (EnableDebugger, DebuggerPort and EnableDebugSharedMem)
//
// Functions on port 0x9900
// Packet is a simple type/address/data format:
// For consistency, type is always 16-bit, address is always 32-bit,
// and data is always 16-bit. However, the number of /valid/ bits
// varies. A packet can pack multiple requests in one buffer and
// will get the data back in the same order.
// Type word is an index:
// LSB (target)
// 01	CPU memory	16-bit	(AMS should be a different space, this is CPU visible memory)
// 02	WP			16-bit
// 03	PC			16-bit
// 04	Status		16-bit
// 05	CPU Reg		8-bit	(simplify register update)
// 06	Cart Bank	8-bit
// 07	Cart memory	24-bit	(supports banked carts > 64k)
// 08	CRU Timer	16-bit
// 09	CRU Bit		16-bit
// 10	DSR Page	8-bit
// 11	VDP memory	24-bit	(just in case)
// 12	VDP reg		8-bit
// 13	VDP address	24-bit
// 14	VDP status	8-bit
// 15	VDP scanlin 8-bit
// 16	GROM memory	24-bit	(includes bases)
// 17	GROM addres 24-bit	(includes bases)
// 18	Sound		16-bit voices (address 0-3)
//					 8-bit volumes (address 4-7)
// 19	Debug timer 32-bit address is two 16-bit (start/end)
// Lots to add - Ubergrom, SID, etc...
//
// WmmBeSxx tttttttt
// MSb is a read/write flag (0=read, 1=write)
// Next two bits indicates mode:
// system (00) 
// breakpoint (01)			(defining this way means we have all the above options available)
// delete breakpoint (10)
// ?? (11)
// tttttttt is the type byte
// breakpoints can use an address of 0xffff to indicate 'available'
// next bit indicates byte (0) or word (1) data (todo: only safe byte access today)
// next bit is set if an error occurred with the request
// next bit indicates whether to do a 'safe' access (1) or allow side effects (0)
//
// Might rethink all this. Shared memory is a ton faster, and then we'd only need
// some way to access CRU and sound to own the machine.

#include <WinSock2.h>
#include <Windows.h>
#include "..\console\tiemul.h"

static SOCKET sock;
extern Byte *VDP;
extern Byte staticCPU[];    // 64k memory map, holds ROMs and scratchpad, all else is in AMS
HANDLE hMapVDP = NULL;
HANDLE hMapCPU = NULL;
bool bEnableDebugger = false;                               // if true, open up the debugger port and shared memory if also active
int debuggerPort = 0x9900;                                  // UDP port to listen to for debugger
bool bEnableDebugSharedMem = false;                         // if true, share memory locally (first emulator instance only, VDP only today)

void initDbgHook() {
	sock = NULL;

    if (!bEnableDebugger) {
        return;
    }

    sockaddr_in RecvAddr;
    int Port = debuggerPort;

	// create socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (SOCKET_ERROR == sock) {
		debug_write("Failed to create dbgHook socket, code %d", WSAGetLastError());
		sock = NULL;
		return;
	}

	// make non-blocking
	unsigned long unblock = 1;
	unsigned long out=0, outlen=0;
	if (SOCKET_ERROR == WSAIoctl(sock, FIONBIO, &unblock, 4, &out, sizeof(out), &outlen, NULL, NULL)) {
		debug_write("Failed to set dbgHook socket to non-blocking mode, code %d", WSAGetLastError());
		closesocket(sock);
		sock = NULL;
		return;
	}

	// bind port
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (SOCKET_ERROR == bind(sock, (SOCKADDR *) &RecvAddr, sizeof(RecvAddr))) {
		debug_write("Failed to bind to port 0x9900 for dbgHook, code %d", WSAGetLastError());
		closesocket(sock);
		sock = NULL;
		return;
	}

	debug_write("dbgHook listening on port 0x9900");
}

void handleBreakpoint(unsigned char *buf, unsigned int p) {
	// todo later
}

void handleMemory(unsigned char *buf, unsigned int p) {
	unsigned short type;
	unsigned int address;
	unsigned short data;
	bool iswrite = (buf[p]&0x80)!=0;
	bool isword = (buf[p]&0x10)!=0;
	bool issafe = (buf[p]&0x04)!=0;
	type=buf[p]*256 + buf[p+1];
	address=(buf[p+2]<<24) | (buf[p+3]<<16) | (buf[p+4]<<8) | buf[p+5];
	data=buf[p+6]*256 + buf[p+7];

	// process by memory type
	switch (type & 0xff) {
		// lots to do, for right now I just need sound, via raw memory writes
		case 01:
			// 01	CPU memory	16-bit	(does NOT supports AMS)
			if (iswrite) {
				if (isword) {
					if (issafe) {
						// not supported, set error bit
						buf[p]|=0x08;
					} else {
						wrword(address&0xffff, data);
					}
				} else {
					// write byte
					if (issafe) {
						// not supported, set error bit
						buf[p]|=0x08;
					} else {
						wcpubyte(address&0xffff, data&0xff);
					}
				}
			} else {
				// reading
				if (isword) {
					if (issafe) {
						data = GetSafeCpuWord(address&0xffff, 0);	// todo: always bank 0? should be current bank
					} else {
						data = romword(address&0xffff, ACCESS_RMW);	// rmw set to not trigger breakpoints
					}
				} else {
					// read byte
					if (issafe) {
						data = GetSafeCpuByte(address&0xffff, 0);	    // todo: always bank 0? should be current bank
					} else {
						data = rcpubyte(address&0xffff, ACCESS_RMW);	// rmw set to not trigger breakpoints
					}
				}
				buf[p+6]=data>>8;
				buf[p+7]=data&0xff;
			}
			break;

		default:
			debug_write("Unknown debug type 0x%02X", type&0xff);
			break;
	}

}

void processDbgPackets() {
	unsigned char buf[2048];
	struct sockaddr recvaddr;
	int recvsize = sizeof(sockaddr);
	int ret;

	if (NULL == sock) {
		return;
	}

	for (int cntdown = 0; cntdown<25; cntdown++) {	// maximum packets processed to prevent DOS
		ret = recvfrom(sock, (char*)buf, sizeof(buf), 0, &recvaddr, &recvsize);
		// walk through the data in buf. All entries should be 
		if (SOCKET_ERROR == ret) {
            int err = WSAGetLastError();
            if ((err != WSAEWOULDBLOCK) && (err != WSAECONNRESET)) {
				debug_write("Dbghook socket got code %d, closing", WSAGetLastError());
				closesocket(sock);
                initDbgHook();
			}
			break;
		}

		for (int p = 0; p < ret; p += 8) {
			// each request is 8 bytes long. We replace the buffer and send it back as confirmation
			// all data is big endian (network order)

			// check breakpoint decision bits
			if (buf[p]&0x60) {
				// non-zero here means it's breakpoint related
				handleBreakpoint(buf, p);
			} else {
				// good old fashioned memory
				handleMemory(buf, p);
			}
		}

		// now send back the answer
		if (SOCKET_ERROR == sendto(sock, (char*)buf, ret, 0, &recvaddr, sizeof(recvaddr))) {
			debug_write("Error writing dbghook reply: %d", WSAGetLastError());
			// probably should do more...
		}
	}
}


