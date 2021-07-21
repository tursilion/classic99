// This is just a SIMULATION of TIPI, you can not rely on programming
// idiosyncrases to be correct. It IS possible to communicate with an
// emulation of TIPI - there is a QEMU package, but at the moment
// that is not what this file is emulating. This is just an interface
// to make the API work.

// TIPI's file system maps the following device names:
//
// TIPI - the root folder of the file system
// DSK0 - an alias for TIPI
// DSK1 - user mapped to any subfolder of TIPI
// DSK2 - ""
// DSK3 - ""
// DSK4 - ""
// URI1 - User defined alias for "PI.*" (for URIs)
// URI2 - ""
// URI3 - ""
// URI4 - ""
// DSK  - simulation of DSK search
//
// The filesystem supports subdirectories as file type 6
// It supports the extended file system information as
// documented in the proposal I started with JediMatt's 
// comments.
//

// Hardware
// >5FF9	RC - Raspberry PI Control Byte
// >5FFB	RD - Raspberry PI Data Byte
// >5FFD	TC - TI-99/4A Control Byte
// >5FFF	TD - TI-99/4A Data Byte
//
// See https://github.com/jedimatt42/tipi/wiki/TIPI-Protocol
// At this level, use emulation
// https://github.com/jedimatt42/tipi/wiki/emulation-installation

// High level interface
// PI.PIO - output to PDF: https://github.com/jedimatt42/tipi/wiki/PI.PIO
// PI.CLOCK - read a timestamp (like existing): https://github.com/jedimatt42/tipi/wiki/PI.CLOCK
// PI.TCP - TCP interface - https://github.com/jedimatt42/tipi/wiki/PI.TCP
// PI.UDP - UDP interface - https://github.com/jedimatt42/tipi/wiki/PI.UDP
// PI.HTTP - URL file access - https://github.com/jedimatt42/tipi/wiki/PI.HTTP
// PI.CONFIG - Access configuration file - https://github.com/jedimatt42/tipi/wiki/PI.CONFIG
// PI.STATUS - Access status data - https://github.com/jedimatt42/tipi/wiki/PI.STATUS
// PI.VARS - Variable access to myti99.com - https://github.com/jedimatt42/tipi/wiki/PI.VARS
// PI.SHUTDOWN - shutdown PI - https://github.com/jedimatt42/tipi/wiki/PI.SHUTDOWN
// PI.REBOOT - reboot PI - https://github.com/jedimatt42/tipi/wiki/PI.REBOOT
// PI.UPGRADE - upgrade PI - https://github.com/jedimatt42/tipi/wiki/PI.UPGRADE

// Extensions
// These normally run through the messaging interface:
//
//  AORG >4010
//	DATA	recvmsg		; MOV @>4010,R3   BL *R3   to invoke recvmsg.
//	DATA	sendmsg		; MOV @>4012,R3   BL *R3   to invoke sendmsg. 
//	DATA	vrecvmsg	; MOV @>4014,R3   BL *R3   to invoke recvmsg (VDP?).
//	DATA	vsendmsg	; MOV @>4016,R3   BL *R3   to invoke sendmsg (VDP?). 
//	DATA	>0000 
//
// https://github.com/jedimatt42/tipi/wiki/RawExtensions
//
// 0x20 - read mouse - https://github.com/jedimatt42/tipi/wiki/Extension-Mouse
// 0x21 - network variables - https://github.com/jedimatt42/tipi/wiki/Extension-NetVar
// 0x22 - TCP - https://github.com/jedimatt42/tipi/wiki/Extension-TCP
// 0x23 - UDP - https://github.com/jedimatt42/tipi/wiki/Extension-UDP

// This TIPI Sim will install at CRU >1200
//
// Status:
// DSK0 - handled by Classic99 DSR - not implemented here
// DSK1 - handled by Classic99 DSR - not implemented here
// DSK2 - handled by Classic99 DSR - not implemented here
// DSK3 - handled by Classic99 DSR - not implemented here
// DSK4 - handled by Classic99 DSR - not implemented here
// DSK  - handled by Classic99 DSR - not implemented here
// DSR - not implemented
// >5FF9 RC - Raspberry PI Control Byte - not implemented
// >5FFB RD - Raspberry PI Data Byte - not implemented
// >5FFD TC - TI-99/4A Control Byte - not implemented
// >5FFF TD - TI-99/4A Data Byte - not implemented
// PI.PIO - output to PDF - not implemented
// 
// 4800 CALL TIPI
// 4810 TIPI - alias for DSK0
// 4820 URI1 - User defined alias for "PI.*" (for URIs)
// 4822 URI2 - ""
// 4824 URI3 - ""
// 4826 URI4 - ""
// 4830 PI.CLOCK - alias for CLOCK
// 4830 PI.TCP - TCP interface
// 4830 PI.UDP - UDP interface
// 4830 PI.HTTP - URL file access
// 4830 PI.CONFIG - Access configuration file
// 4830 PI.STATUS - Access status data
// 4830 PI.VARS - Variable access to myti99.com - pending?
// 4830 PI.SHUTDOWN - shutdown PI (NOP here)
// 4830 PI.REBOOT - reboot PI (NOP here)
// 4830 PI.UPGRADE - upgrade PI (NOP here)
// 
// 4840 DATA recvmsg
// 4850	DATA sendmsg
// 4860	DATA vrecvmsg
// 4870	DATA vsendmsg

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <io.h>
#include <atlstr.h>
#include <time.h>
#include <map>
#include <string>
#include <vector>
#include "tiemul.h"
#include "diskclass.h"
#include "cpu9900.h"
#include "tipiDisk.h"

extern CPU9900 * volatile pCurrentCPU;
extern void do_dsrlnk(char *forceDevice);
extern const char* getOpcode(int opcode);
extern void GetFilenameFromVDP(int nName, int nMax, FileInfo *pFile);
extern void setfileerror(FileInfo *pFile);
extern BaseDisk *pDriveType[MAX_DRIVES];
extern HWND myWnd;
extern RECT gWindowRect;
extern int WindowActive;
extern int nCurrentDSR;

CString TipiURI[3];
CString TipiAuto;
CString TipiDirSort;
CString TipiTz;
CString TipiSSID;
CString TipiPSK;
CString TipiName;
TipiWebDisk tipiDsk;
bool initDone = false;
// TODO: netvars are not persistent and have only
// one namespace - should be adequate for now though
std::map<std::string,std::string> ti_vars;
bool isNetvars = false;     // next message is expected to be netvars - see that section for why

// network interface
unsigned char *rxMessageBuf = NULL; // data storage - size may vary
int rxMessageLen = 0;               // current message size (not necessarily the full buffer size)
SOCKET sock[256] = { 0 };           // up to 256 sockets are allowed (here, I share them)

bool callTipi();
bool dsrTipi();
bool dsrUri(int x);
bool dsrPI();
bool dsrPiPio();
bool dsrPiClock();
bool dsrPiTcp();
bool dsrPiUdp();
bool dsrPiHttp();
bool dsrPiConfig();
bool dsrPiStatus();
bool dsrPiVars();
bool dsrPiHardwareNop();
bool directRecvMsg();
bool directSendMsg();
bool directVRecvMsg();
bool directVSendMsg();
bool handleSendMsg(unsigned char *buf, int len);
bool handleMouse(unsigned char *buf, int len);
bool handleNetvars(unsigned char *buf, int len);
bool handleTcp(unsigned char *buf, int len);
bool handleUdp(unsigned char *buf, int len);

bool tipiDsrLnk(bool (*bufferCode)(FileInfo *pFile));
bool bufferWebFile(FileInfo *pFile);
bool bufferConfig(FileInfo *pFile);
bool bufferStatus(FileInfo *pFile);

#define MAX_RAM_FILE 128*1024

// on the day this changes (for instance, we support
// direct access to RAW files, etc), add a headersize
// variable to FileInfo and make sure the detection code
// updates it, then use that instead.
#define HEADERSIZE 128

static const unsigned char LoadPAB[] = {
    0x05, 0x00, // LOAD
    0x10, 0x00, // VDP address >1000
    0x00, 0x00, // record length, char count
    0x20, 0x06, // max size 0x2006 (8k+EA5 header)
    0x00, 0x00  // BIAS, name length
};

// get a file from the web and store in RAM
// caller is responsible for freeing data
// NULL on failure
unsigned char *getWebFile(CString &filename, int &outSize) {
    // adapted from https://stackoverflow.com/questions/23038973/c-winhttp-get-response-header-and-body
    DWORD dwSize;
    DWORD dwDownloaded;
    DWORD headerSize = 0;
    BOOL  bResults = FALSE;
    HINTERNET hSession;
    HINTERNET hConnect;
    HINTERNET hRequest;
    wchar_t host[MAX_PATH];
    wchar_t resource[MAX_PATH];
    char tmpStr[MAX_PATH];
    bool secure = false;
    unsigned char *buf = NULL;
    int outPos = 0;
    outSize = 0;

    // Parse out pi.http vs urix
    //
    // it's a URI request - if PI make sure it's http
    // TODO: not sure if multiple web files are allowed to
    // be open! This code assumes only one...
    CString url;
    CString tst = filename.Left(3);

    if (tst.CompareNoCase("PI.") == 0) {
        if (filename.Mid(3, 4).CompareNoCase("http") != 0) {
            debug_write("Can't load from '%s'!", filename);
            return NULL;
        }
        url = filename.Mid(3);
    } else {
        // URI1, URI2, URI3, URI4
        int idx = filename[3]-'1';
        if ((idx<0)||(idx>2)||(TipiURI[idx].IsEmpty())) {
            debug_write("Can't load from '%s'?", filename);
            return NULL;
        }
        // assuming, not checking for the '.'
        url = TipiURI[idx] + filename.Mid(5);
    }

    debug_write("Load URL is '%s'", url.GetString());

    // split up the path and make it wide
    strncpy(tmpStr, url.GetString(), MAX_PATH);
    tmpStr[MAX_PATH-1]='\0';

    char *p = strchr(tmpStr, ':');
    if (NULL == p) {
        // okay, assume no http part
        secure = false;
        p = tmpStr;
    } else if (p == tmpStr) {
        // what is this nonsense?
        secure = false;
        ++p;
    } else if ((*(p-1) == 'S')||(*(p-1) == 's')) {
        // https:
        secure = true;
        ++p;
    } else {
        // probably http:, but not checking
        // we shouldn't be called with other methods...
        secure = false;
        ++p;
    }
    while (*p == '/') ++p;
    char *p2 = strchr(p, '/');
    if (NULL != p2) {
        *p2 = '\0';
        ++p2;
        _snwprintf(host, MAX_PATH, L"%S", p);
        _snwprintf(resource, MAX_PATH, L"%S", p2);
    } else {
        _snwprintf(host, MAX_PATH, L"%S", p);
        _snwprintf(resource, MAX_PATH, L"");
    }

    // CALL TIPI("PI.http://harmlesslion.com/tipi/PIANO1")

    hSession = WinHttpOpen( L"Classic99TipiSim/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
    if (NULL == hSession) {
        debug_write("Failed to create web session, code %d", GetLastError());
        return NULL;
    }

    hConnect = WinHttpConnect( hSession, host, secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0 );
    if (NULL == hConnect) {
        debug_write("Web connect request failed, code %d", GetLastError());
        WinHttpCloseHandle(hSession);
        return NULL;
    }
        
    hRequest = WinHttpOpenRequest( hConnect, L"GET", resource, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0 );
    if (NULL == hRequest) {
        debug_write("Web open request failed, code %d", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    bResults = WinHttpSendRequest( hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0 );
    if (!bResults) {
        debug_write("Web Send Request failed, code %d", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    bResults = WinHttpReceiveResponse( hRequest, NULL );
    if (!bResults) {
        debug_write("Web Receive failed, code %d", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    /* store headers...
    bResults = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, WINHTTP_NO_OUTPUT_BUFFER, &headerSize, WINHTTP_NO_HEADER_INDEX);
    if ((!bResults) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
    {
        responseHeader.resize(headerSize / sizeof(wchar_t));
        if (responseHeader.empty())
        {
            bResults = TRUE;
        }
        else
        {
            bResults = WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, NULL, &responseHeader[0], &headerSize, WINHTTP_NO_HEADER_INDEX);
            if( !bResults ) headerSize = 0;
            responseHeader.resize(headerSize / sizeof(wchar_t));
        }
    }
    */

    // TODO: this needs optimizing...
    do
    {
        // Check for available data.
        dwSize = 0;
        bResults = WinHttpQueryDataAvailable( hRequest, &dwSize );
        if (!bResults) {
            debug_write("Failed to query web data available, code %d", GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            free(buf);
            return NULL;
        }

        if (dwSize == 0) {
            // all done
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return buf;
        }

        // grab what is so-far available
        do
        {
            // Allocate space for the buffer.
            if (outPos + (signed)dwSize > outSize) {
                buf = (unsigned char*)realloc(buf, outPos+dwSize);
                outSize = outPos+dwSize;
                if (outSize >= MAX_RAM_FILE) {
                    debug_write("Web file exceeds max size (%dk) (Classic99 limit) - failing", MAX_RAM_FILE/1024);
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    free(buf);
                    return NULL;
                }
            }
            // Read the data.
            bResults = WinHttpReadData( hRequest, &buf[outPos], dwSize, &dwDownloaded );
            if (!bResults) {
                debug_write("Failed reading web data, code %d", GetLastError());
                // is this not an error?
                dwDownloaded = 0;
            }

            outPos += dwDownloaded;
            dwSize -= dwDownloaded;

            if (dwDownloaded == 0) {
                break;
            }
        }
        while (dwSize > 0);
    }
    while (true);
}

// TIPI DSR jump - return true to add two to R11
// (DSR success is indicated this way)
bool HandleTIPI() {
    if (false == initDone) {
        initDone = true;

        for (int idx=0; idx<256; ++idx) {
            sock[idx] = INVALID_SOCKET;
        }
    }

    // figure out which entry point was requested
    switch (pCurrentCPU->GetPC()) {
    case 0x4800:    // CALL TIPI
        return callTipi();

    case 0x4810:    // TIPI
        return dsrTipi();

    case 0x4820:    // URI1
        return dsrUri(1);

    case 0x4822:    // URI2
        return dsrUri(2);

    case 0x4824:    // URI3
        return dsrUri(3);

    case 0x4826:    // URI4
        return dsrUri(4);

    case 0x4830:    // PI
        return dsrPI();

    case 0x4840:    // recvmsg
        return directRecvMsg();

    case 0x4850:    // sendmsg
        return directSendMsg();

    case 0x4860:    // vrecvmsg
        return directVRecvMsg();

    case 0x4870:    // vsendmsg
        return directVSendMsg();

    default:
        debug_write("Warning: Unemulated TIPI entry point: >%04X", pCurrentCPU->GetPC());
        return false;
    }
}

// CALL TIPI from TI BASIC
// This is an EA#5 loader which defaults to "TIPI.TIPICFG"
bool callTipi() {
    // verify assumptions:
    // workspace is >83E0
    if (pCurrentCPU->GetWP() != 0x83E0) {
        debug_write("Warning: CALL TIPI with incorrect workspace of >%04X instead of >83E0",pCurrentCPU->GetWP());
    }
    // R1 = number of times called, normally 1 (?)
    // R9 = address of the subprogram
    if (GetSafeCpuWord(pCurrentCPU->GetWP()+18, 0) != 0x4800) {
        debug_write("Warning: CALL TIPI should have address of program in R9, got >%04X", GetSafeCpuWord(pCurrentCPU->GetWP()+18, 0));
    }
    // R11 - return address (to keep scanning)
    // R12 - CRU base
    if (GetSafeCpuWord(pCurrentCPU->GetWP()+24, 0) != 0x1200) {
        debug_write("Warning: CALL TIPI should have CRU address in R12, got >%04X", GetSafeCpuWord(pCurrentCPU->GetWP()+24, 0));
    }
    // >83D0-D1: CRU base address of the card
    if (GetSafeCpuWord(0x83d0, 0) != 0x1200) {
        debug_write("Warning: CALL TIPI should have CRU address in 0x83D0, got >%04X", GetSafeCpuWord(0x83d0, 0));
    }
    // >83D2-D3: link to next subprogram in the header

    // Assuming TI BASIC in RAM

    // pull the filename out of the command - if there is not
    // one, load the default. This is intended to be called from TI BASIC
    // TODO: does CALL TIPI support variables? We are assuming no.
    CString filename = "";

    // useful tokens:
    // B7 - open parenthesis
    // C7 - quoted string (length byte, then data)
    // B6 - close parenthesis
    // 00 - end of line
    // Token pointer at >832C - points to the "TIPI" name: C8 04 T I P I
    int adr = GetSafeCpuWord(0x832c, 0);
    if (adr > 0x3ffb) {
        debug_write("VDP address for CALL TIPI too large - >%04X", adr);
        return false;
    }

    // the rest is in VDP - we can read directly
    if (VDP[adr] != 4) {   // length of TIPI
        // just fail
        debug_write("BASIC syntax parse error..");
        return false;
    }
    // not going to check the string...
    adr+=5;
    // either end of command, or open parenthesis
    if (VDP[adr] != 0) {
        if (VDP[adr] != 0xb7) {
            debug_write("BASIC syntax parse error...");
            return false;
        }

        ++adr;
        // if it's close parenthesis, then ignore
        if (VDP[adr] != 0xb6) {
            if (VDP[adr] != 0xc7) {
                debug_write("Classic99 only supports quoted strings for CALL TIPI");
                return false;
            }

            ++adr;
            int len = VDP[adr++];
            for (int idx=0; idx<len; ++idx) {
                filename += (char)VDP[adr++];
            }

            // now it MUST be close
            if (VDP[adr++] != 0xb6) {
                debug_write("Missing close parenthesis on CALL TIPI...");
                return false;
            }
        }

        // we should be pointing at the 0, but BASIC can check that, if we return
    }
    // write back the address
    wrword(0x832c, adr);
    // and copy the NUL token to >8342
    wcpubyte(0x8342, 0);

    if (filename.IsEmpty()) filename = "DSK0.TIPICFG";
    debug_write("Handling CALL TIPI(\"%s\")", filename.GetString());

    // if the filename starts with TIPI, replace it with DSK0
    if (filename.Left(5).CompareNoCase("TIPI.") == 0) {
        filename = "DSK0." + filename.Mid(5);
    }
    // only legal results now are starting with DSK, URI, or PI.
    CString tst = filename.Left(3);
    if ((tst.CompareNoCase("DSK") != 0) && (tst.CompareNoCase("URI") != 0) && (tst.CompareNoCase("PI.") != 0)) {
        debug_write("Can't load from '%s'", filename);
        return false;
    }

    int bootAddress = 0;

    // before we go any further, just make sure the DSR is turned off. On the
    // real thing, the DSR is turned off before launching the program, but
    // because of the address hacks (for reboot, etc), the console doesn't
    // always get a chance to do it, so just fake it here
    nCurrentDSR = -1;

    // load the file - TODO: does TIPI load through VDP or straight to CPU?
    // if file fails to load, reset
    // It's a bit wasteful to loop here.. but this is concept...
nextfile:
    if (tst.CompareNoCase("DSK") == 0) {
        // need to route through the disk system
        // TODO: we don't need this fakery if the real CALL TIPI
        // uses a normal PAB, which I can check, but I'm just
        // gonna fake it the safe way for now.
        unsigned char tmpVDP[16384];
        unsigned char tmpCPU[256];
        
        // backup VDP
        memcpy(tmpVDP, VDP, 16384);
        for (int idx=0; idx<256; ++idx) {
            tmpCPU[idx] = GetSafeCpuByte(idx+0x8300, 0);
        }

        // create a PAB to load up to 8k to >1000
        memcpy(&VDP[0x3100], LoadPAB, sizeof(LoadPAB));
        VDP[0x3109] = filename.GetLength() & 0xff;
        for (int idx=0; idx<filename.GetLength(); ++idx) {
            VDP[0x310a+idx] = filename[idx];
        }

        // We are bypassing the real DSRLNK call, so we need to set
        // up the pointers slightly differently
        wrword(0x8354, 4);          // all supported filenames here are 4 bytes
        wrword(0x8356, 0x310a+4);   // one byte after the filename
        do_dsrlnk(NULL);            // ask the disk system to do it for us
        // The TI disk controller will reject these calls, it will
        // not work to try and wrap through to that. Not sure if the
        // real TIPI can... but certainly not when it's at >1200.

        // check for success, if it failed we just reboot
    	if (0 != (VDP[0x3100+1]&0xe0)) {
            debug_write("CALL TIPI load failed, reboot.");
            // the Classic99 code will "return" to the caller's
            // R11, so we need to load the address we want there,
            // then we can still safely change WP here... 
            // (though it should already be right)
            wrword(pCurrentCPU->GetWP()+22, GetSafeCpuWord(2,0));
            pCurrentCPU->SetWP(GetSafeCpuWord(0,0));
            return false;
        }

        // copy the data from VDP to CPU
        int next = VDP[0x1000]*256 + VDP[0x1001];   // next file flag
        int size = VDP[0x1002]*256 + VDP[0x1003];
        int dest = VDP[0x1004]*256 + VDP[0x1005];
        for (int idx=0; idx<size; ++idx) {
            if ((dest+idx > 0xffff)||(0x1006+idx > 0x3fff)) {
                debug_write("Load address overflow - aborting load.");
                break;
            }
            wcpubyte(dest+idx, VDP[0x1006+idx]);
        }

        // restore VDP
        for (int idx=0; idx<256; ++idx) {
            wcpubyte(idx+0x8300, tmpCPU[idx]);
        }
        memcpy(VDP, tmpVDP, 16384);

        // set start
        if (0 == bootAddress) {
            bootAddress = dest;
        }

        // increment filename
        int flen = filename.GetLength();
        filename = filename.Left(flen-1) + (char)(filename[flen-1] + 1);

        // loop if flagged
        if (next) goto nextfile;

        // start the application - need to load it
        // into R11 to trick the Classic99 code
        wrword(pCurrentCPU->GetWP()+22, bootAddress);
    } else {
        int outSize = 0;
        unsigned char *buf = getWebFile(filename, outSize);
        if (NULL == buf) {
            debug_write("CALL TIPI load failed, reboot.");
            // the Classic99 code will "return" to the caller's
            // R11, so we need to load the address we want there,
            // then we can still safely change WP here... 
            // (though it should already be right)
            wrword(pCurrentCPU->GetWP()+22, GetSafeCpuWord(2,0));
            pCurrentCPU->SetWP(GetSafeCpuWord(0,0));
            return false;
        }
        if (outSize < 128+6) {
            // if it's not a TIFILES file with PROGRAM header, then skip it
            debug_write("CALL TIPI load got bad file, reboot.");
            // the Classic99 code will "return" to the caller's
            // R11, so we need to load the address we want there,
            // then we can still safely change WP here... 
            // (though it should already be right)
            wrword(pCurrentCPU->GetWP()+22, GetSafeCpuWord(2,0));
            pCurrentCPU->SetWP(GetSafeCpuWord(0,0));
            return false;
        }
        // might as well check it's a PROGRAM image too, won't bother with the rest
        if (buf[10] != TIFILES_PROGRAM) {
            debug_write("CALL TIPI load is non-PROGRAM TIFILES image, reboot.");
            // the Classic99 code will "return" to the caller's
            // R11, so we need to load the address we want there,
            // then we can still safely change WP here... 
            // (though it should already be right)
            wrword(pCurrentCPU->GetWP()+22, GetSafeCpuWord(2,0));
            pCurrentCPU->SetWP(GetSafeCpuWord(0,0));
            return false;
        }

        // copy the data from buffer to CPU (after the 128 byte TIFILES header)
        int next = buf[128]*256 + buf[129];   // next file flag
        int size = buf[130]*256 + buf[131];
        int dest = buf[132]*256 + buf[133];
        for (int idx=0; idx<size; ++idx) {
            if ((dest+idx > 0xffff)||(idx+128+6 > outSize)) {
                // this may actually happen a lot - a lot of EA#5 headers
                // include the 6 byte header in the length, even though
                // the header is not copied
                debug_write("Load address overflow - aborting load.");
                break;
            }
            wcpubyte(dest+idx, buf[idx+128+6]);
        }

        // set start
        if (0 == bootAddress) {
            bootAddress = dest;
        }

        // increment filename
        int flen = filename.GetLength();
        filename = filename.Left(flen-1) + (char)(filename[flen-1] + 1);

        // loop if flagged
        if (next) goto nextfile;

        // start the application - need to load it
        // into R11 to trick the Classic99 code
        wrword(pCurrentCPU->GetWP()+22, bootAddress);
    }

    // always return false, we have ceded control
    return false;
}

// This is TIPI as a device name, and should just wrap to DSK0
bool dsrTipi() {
    debug_write("Remapping TIPI to DSK0...");
    do_dsrlnk("DSK0");
    return true;
}
bool dsrPiClock() {
    debug_write("Remapping PI.CLOCK to CLOCK...");
    do_dsrlnk("CLOCK");
    return true;
}

// This has a number of suboptions that we need to parse out
bool dsrPI() {
    // we need to figure out from the DSR what is being asked of us
    // we have a device name like PI.HTTP, etc...
    // 0x8354 (word) contains the DSR name length, but that's just 'PI'
    // 0x8356 (word) points to the period - in this case it MUST be a period
    // So at 0x8356 - at 0x8354 - 1 has the total name length byte, but for now
    // we can just substring match
    int PAB = romword(0x8356, ACCESS_FREE);
    ++PAB;
    if (PAB > 0x3ffd) {
        debug_write("Invalid PAB address, failing");
        return false;
    }
    // shortest one is 3 characters, so we'll just take 3
         if (0 == _strnicmp((char*)&VDP[PAB], "TCP", 3)) return dsrPiTcp();
    else if (0 == _strnicmp((char*)&VDP[PAB], "UDP", 3)) return dsrPiUdp();
    else if (0 == _strnicmp((char*)&VDP[PAB], "HTT", 3)) return dsrPiHttp();
    else if (0 == _strnicmp((char*)&VDP[PAB], "CON", 3)) return dsrPiConfig();
    else if (0 == _strnicmp((char*)&VDP[PAB], "STA", 3)) return dsrPiStatus();
    else if (0 == _strnicmp((char*)&VDP[PAB], "VAR", 3)) return dsrPiVars();
    else if (0 == _strnicmp((char*)&VDP[PAB], "SHU", 3)) return dsrPiHardwareNop();
    else if (0 == _strnicmp((char*)&VDP[PAB], "REB", 3)) return dsrPiHardwareNop();
    else if (0 == _strnicmp((char*)&VDP[PAB], "UPG", 3)) return dsrPiHardwareNop();

    debug_write("Unknown PI action, skipping");
    return false;
}
bool dsrPiHttp() {
    // load the web file into a TipiWebDisk
    return tipiDsrLnk(bufferWebFile);
}
// This spawns up an alias for PI.HTTP://whateverpath
bool dsrUri(int x) {
    // load the web file into a TipiWebDisk
    return tipiDsrLnk(bufferWebFile);
}
bool dsrPiConfig() {
    // load the web file into a TipiWebDisk
    return tipiDsrLnk(bufferConfig);
}
bool dsrPiStatus() {
    // load the web file into a TipiWebDisk
    return tipiDsrLnk(bufferStatus);
}

// similar to the disk dsrlnk, but for TIPI web services
// bufferCode is a function that buffers data into initData
bool tipiDsrLnk(bool (*bufferCode)(FileInfo *pFile)) {
	// performs web file i/o using the PAB passed as per a normal dsrlnk call.
	// address of length byte passed in CPU >8356
	int PAB;
	FileInfo tmpFile, *pWorkFile;

	// get the base address of PAB in VDP RAM
	PAB = romword(0x8356, ACCESS_FREE);		// this points to the character AFTER the device name (the '.' or end of string)
	// since there must be 10 bytes before the name, we can do a quick early out here...
	if (PAB < 13) {		// 10 bytes, plus three for shortest device name "DSK"
		debug_write("Bad PAB address >%04X", PAB);		// not really what the TI would do, but it's wrong anyway.
		// we can't set the error, because we'd underrun! (real TI would wrap around the address)
		return false;
	}

	// verify mandatory setup makes some form of sense - for debugging DSR issues
	{
		static int nLastOp = -1;
		static int nLastPAB = -1;	// reduce debug by reducing repetition

		int nLen = romword(0x8354, ACCESS_FREE);
		if (nLen > 7) {
			debug_write("Warning: bad DSR name length %d in >8354", nLen);
			// this is supposed to tell us where to write the error!?
			return false;
		}

		int nOpcode=VDP[PAB-nLen-10];
		if ((nOpcode < 0) || (nOpcode > 9)) {
			debug_write("Warning: bad Opcode in PAB at VDP >%04X (>%02X)", PAB-nLen-10,nOpcode);
			TriggerBreakPoint();
			// since we don't have a structure yet, set it manually
			VDP[PAB-nLen-10+1] &= 0x1f;					// no errors
			VDP[PAB-nLen-10+1] |= ERR_BADATTRIBUTE<<5;	// file error
			return true;
		}

		int nNameLen = VDP[PAB-nLen-1];
		if (PAB-nLen+nNameLen > 0x3fff) {
			debug_write("PAB name too long (%d) for VDP >%04X", nNameLen, PAB);		// not really what the TI would do, but it's wrong anyway.
			// since we don't have a structure yet, set it manually
			VDP[PAB-nLen-10+1] &= 0x1f;					// no errors
			VDP[PAB-nLen-10+1] |= ERR_BADATTRIBUTE<<5;	// file error
			return true;
		}

		if ((nLastOp!=nOpcode)||(nLastPAB != PAB)||(nOpcode == 0)) {
			char buf[32];
			if (nNameLen > 32) {
				nNameLen=31;
			}
			memset(buf, 0, sizeof(buf));
			memcpy(buf, &VDP[PAB-nLen], nNameLen);
			debug_write("TIPIsim opcode >%d (%s) on PAB >%04X, filename %s", nOpcode, getOpcode(nOpcode), PAB, buf);
			nLastOp = nOpcode;
			nLastPAB = PAB;
		}
	}

    // we are called here only for PI, or URIx
    // PI has several options as well, but we have a buffering tool already set for that

	// You can make assumptions when the device name is always the same length, but
	// we have a few, so we need to work out which one we had, then subtract an additional 10 bytes.
    // Force device doesn't apply here, cause we're playing to find the start of the PAB
    // TODO: why can't we just use the length byte in 0x8354??
    PAB -= 2;                   // either PI or Ix
	if (0 == _strnicmp("uri", (const char*)&VDP[PAB]-2, 3)) PAB-=2;		// must be URI
	PAB-=10;					// back up to the beginning of the PAB
	PAB&=0x3FFF;				// mask to VDP memory range

	// The DSR limited the names that can be passed in here, so it's okay to
	// make some assumptions.!
	// TODO: It's TI code that decides if we get called, so it's actually NOT okay,
	// we need to actually verify the name. I had a DSRLNK that passed a block of
	// zero bytes and ended up accessing the CLIP device instead of failing.

	// save the drive index (always zero - tipiDsk)
	tmpFile.nDrive = 0;

	// copy the PAB data into the tmpFile struct
	tmpFile.PABAddress = PAB;
	tmpFile.OpCode = VDP[PAB++];							// 0
	PAB&=0x3FFF;
	tmpFile.Status = VDP[PAB++];							// 1
	PAB&=0x3FFF;
	tmpFile.DataBuffer = (VDP[PAB]<<8) | VDP[PAB+1];		// 2,3
	PAB+=2;
	PAB&=0x3FFF;
	tmpFile.RecordLength = VDP[PAB++];						// 4
	PAB&=0x3FFF;
	tmpFile.CharCount = VDP[PAB++];							// 5
	PAB&=0x3FFF;
	tmpFile.RecordNumber = (VDP[PAB]<<8) | VDP[PAB+1];		// 6,7	It's not relative vs sequential, it's variable (not used) vs fixed (used)
	PAB+=2;
	PAB&=0x3FFF;
	tmpFile.ScreenOffset = VDP[PAB++];						// 8
	PAB&=0x3FFF;
	int nLen = VDP[PAB++];  								// 9
    // don't skip anything, so we have the full path (PI.HTTP://xxx, URI1.xxx, PI.CONFIG, PI.STATUS)
    GetFilenameFromVDP(PAB, nLen, &tmpFile);

	// somewhat annoying, but we need to try and keep the FileType (TIFILES) and Status (PAB)
	// in sync, so map Status over
	tmpFile.FileType=0;
	if (tmpFile.Status & FLAG_VARIABLE) tmpFile.FileType |= TIFILES_VARIABLE;
	if (tmpFile.Status & FLAG_INTERNAL) tmpFile.FileType |= TIFILES_INTERNAL;

	// split name into options and name (shouldn't be any options)
	tmpFile.SplitOptionsFromName();

    // and 0x8354 is supposed to point to the PAB, again, TI specific...
    // TODO: probably not TIPI?
    wrword(0x8354, PAB);

	// See if we can find an existing FileInfo for this request, and use it instead
	pWorkFile=tipiDsk.FindFileInfo(tmpFile.csName);
	if (NULL == pWorkFile) {
		// none was found, just use what we have, hopefully it's an existing file
		pWorkFile=&tmpFile;
	} else {
		// XB is bad about not closing its files, so if this is an open request,
		// then just assume we should close the old file and do it again. Usually this
		// happens only if an error occurs while loading a program. TODO: how did TI deal with this?
        // Maybe this doesn't happen any more with the better emulation?
		if (tmpFile.OpCode == OP_OPEN) {
			// discard any changes to the old file, it was never closed
			pWorkFile->bDirty = false;
			tipiDsk.Close(pWorkFile);
			debug_write("Recycling unclosed file buffer %d", pWorkFile->nIndex);
			// and then use tmpFile as if it were new
			pWorkFile = &tmpFile;
		} else {
			// it was found, so it's open. Verify the header/mode parameters match what we have
			// then go ahead and swap it over
			// We don't care on a CLOSE request
			if (tmpFile.OpCode != OP_CLOSE) {
				if ((pWorkFile->Status&FLAG_TYPEMASK) != (tmpFile.Status&FLAG_TYPEMASK)) {
					debug_write("File mode/status does not match open file for %s (0x%02X vs 0x%02X)", (LPCSTR)tmpFile.csName, pWorkFile->Status&0x1f, tmpFile.Status&0x1f);
					pWorkFile->LastError = ERR_BADATTRIBUTE;
					setfileerror(pWorkFile);
					return true;
				}
				if (pWorkFile->RecordLength != tmpFile.RecordLength) {
					debug_write("File record length does not match open file for %s (%d vs %d)", (LPCSTR)tmpFile.csName, pWorkFile->RecordLength, tmpFile.RecordLength);
					pWorkFile->LastError = ERR_BADATTRIBUTE;
					setfileerror(pWorkFile);
					return true;
				}
			}
			
			// okay, that's the important stuff, copy the request data we got into the new one
			pWorkFile->CopyFileInfo(&tmpFile, true);
		}
	}

	// make sure the last error is cleared
	pWorkFile->LastError = ERR_NOERROR;

#define HELPFULDEBUG(x) debug_write(x " %s on drive type %s", (LPCSTR)pWorkFile->csName, tipiDsk.GetDiskTypeAsString());
#define HELPFULDEBUG1(x,y) debug_write(x " %s on drive type %s", y, (LPCSTR)pWorkFile->csName, tipiDsk.GetDiskTypeAsString());
	switch (pWorkFile->OpCode) {
		case OP_OPEN:
			{
				FileInfo *pNewFile=NULL;

				HELPFULDEBUG("Opening");

                // check for disk write protection on output or append (update will come if a write is attempted)
				if (((pWorkFile->Status & FLAG_MODEMASK) == FLAG_OUTPUT) ||
					((pWorkFile->Status & FLAG_MODEMASK) == FLAG_APPEND)) {
    				if (tipiDsk.GetWriteProtect()) {
						debug_write("Attempt to write to write-protected disk.");
						pWorkFile->LastError = ERR_WRITEPROTECT;
						setfileerror(pWorkFile);
						break;
					} else if (bufferCode != bufferConfig) {
                        // that's one way to check ;) Allow writes only to PI.CONFIG
                        debug_write("Virtual file is read-only");
						pWorkFile->LastError = ERR_WRITEPROTECT;
						setfileerror(pWorkFile);
						break;
                    }
				} else {
                    // before the open, buffer the file
                    if (!bufferCode(pWorkFile)) {
                        setfileerror(pWorkFile);
                        break;
                    }
                }

				// a little more info just for opens
				debug_write("PAB requested file type is %c%c%d", (pWorkFile->Status&FLAG_INTERNAL)?'I':'D', (pWorkFile->Status&FLAG_VARIABLE)?'V':'F', pWorkFile->RecordLength);

                // now the open
                pNewFile = tipiDsk.Open(pWorkFile);
				if ((NULL == pNewFile) /*|| (!pNewFile->bOpen)*/) {
					// the bOpen check is to make STATUS able to use Open to check PROGRAM files
					pNewFile = NULL;
					setfileerror(pWorkFile);
				} else {
					// if the user didn't specify a RecordLength in the PAB, we should do that now
                    // TODO: why am I missing pWorkFile, tmpFile, and pNewFile in here...?
					if ((VDP[pWorkFile->PABAddress+4] == 0) && (pNewFile->RecordLength != 0)) {
						VDP[pWorkFile->PABAddress+4]=pNewFile->RecordLength;
					}

					// ALWAYS performs a rewind operation , which has this effect (should be zeroes)
					tipiDsk.Restore(pWorkFile);		// todo: should we check for error?
					VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
					VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;

					// The pointer to the new object is needed since that's what the derived
					// class updated/created. (This fixes Owen's Wycove Forth issue)
					pNewFile->nCurrentRecord = 0;
					pNewFile->bOpen = true;
					pNewFile->bDirty = false;	// can't be dirty yet!
				}
			}
			break;

		case OP_CLOSE:
			// we should check for open here, but safer to just always try to close
			HELPFULDEBUG("Closing");
			if (!tipiDsk.Close(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_READ:
			if ((!pWorkFile->bOpen) ||
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_OUTPUT) || 
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_APPEND)) {
				debug_write("Can't read from %s as file is not opened for read.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			if (!tipiDsk.Read(pWorkFile)) {
				HELPFULDEBUG1("Failed reading max %d bytes", pWorkFile->RecordLength);
				setfileerror(pWorkFile);
			} else {
				// always copy the char count back (TODO: always?)
				VDP[tmpFile.PABAddress+5] = pWorkFile->CharCount;
				// Fixed files always get the record number updated
				if (0 == (tmpFile.Status & FLAG_VARIABLE)) {
					VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
					VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;
				}
			}
			break;

		case OP_WRITE:
			if ((!pWorkFile->bOpen) || ((pWorkFile->Status & FLAG_MODEMASK) == FLAG_INPUT)) {
				debug_write("Can't write to %s as file is not opened for write.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			// check for disk write protection
			if (tipiDsk.GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}

			if (!tipiDsk.Write(pWorkFile)) {
				HELPFULDEBUG1("Failed writing %d bytes", pWorkFile->CharCount);
				setfileerror(pWorkFile);
			} else {
				// mark it dirty
				pWorkFile->bDirty = true;
				// Fixed files always get the record number updated
				if (0 == (tmpFile.Status & FLAG_VARIABLE)) {
					VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
					VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;
				}
			}
			break;

		case OP_RESTORE:
			if ((!pWorkFile->bOpen) ||
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_OUTPUT) || 
				((pWorkFile->Status & FLAG_MODEMASK) == FLAG_APPEND)) {
				debug_write("Can't restore %s as file is not opened for read.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			// the basics of this operation is copied in OPEN, so if you fix something,
			// also look there.
			HELPFULDEBUG1("Restoring from record %d", pWorkFile->RecordNumber);
			if (!tipiDsk.Restore(pWorkFile)) {
				setfileerror(pWorkFile);
			} else {
				// update the PAB, all file types (verified via TICC emulation)
				VDP[tmpFile.PABAddress+6] = pWorkFile->RecordNumber/256;
				VDP[tmpFile.PABAddress+7] = pWorkFile->RecordNumber%256;
			}
			break;

		case OP_LOAD:
			HELPFULDEBUG1("Loading to VDP >%04X", pWorkFile->DataBuffer);
            // before the load, buffer the file
            if (!bufferCode(pWorkFile)) {
                setfileerror(pWorkFile);
                break;
            }
			if (!tipiDsk.Load(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_SAVE:
			HELPFULDEBUG1("Saving from VDP >%04X", pWorkFile->DataBuffer);
			
			// check for disk write protection
			if (tipiDsk.GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}

			if (!tipiDsk.Save(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_DELETE:
			HELPFULDEBUG("Deleting");
			// check for disk write protection
			if (tipiDsk.GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}
			if (!tipiDsk.Delete(pWorkFile)) {
				setfileerror(pWorkFile);
			}
			break;

		case OP_SCRATCH:
			if (!pWorkFile->bOpen) {
				debug_write("Can't scratch in %s as file is not opened.", pWorkFile->csName);
				pWorkFile->LastError = ERR_ILLEGALOPERATION;
				setfileerror(pWorkFile);
				break;
			}
			HELPFULDEBUG1("Scratching record %d", pWorkFile->RecordNumber);

			// check for disk write protection
			if (tipiDsk.GetWriteProtect()) {
				debug_write("Attempt to write to write-protected disk.");
				pWorkFile->LastError = ERR_WRITEPROTECT;
				setfileerror(pWorkFile);
				break;
			}

			if (!tipiDsk.Scratch(pWorkFile)) {
				setfileerror(pWorkFile);
			} else {
				// mark it dirty
				pWorkFile->bDirty = true;
			}
			break;

		case OP_STATUS:
			pWorkFile->ScreenOffset = 0;
			if (!tipiDsk.GetStatus(pWorkFile)) {
				setfileerror(pWorkFile);
			} else {
				// write the result back to the PAB
				VDP[tmpFile.PABAddress+8] = pWorkFile->ScreenOffset;
			}
			HELPFULDEBUG1("Status returns >%02X on", VDP[tmpFile.PABAddress+8]);
			break;

		default:
			HELPFULDEBUG1("Unknown DSRLNK opcode %d", pWorkFile->OpCode);
			pWorkFile->LastError = ERR_BADATTRIBUTE;
			setfileerror(pWorkFile);		// Bad open attribute
			break;
	}
#undef HELPFULDEBUG
#undef HELPFULDEBUG1

    return true;
}

bool bufferWebFile(FileInfo *pFile) {
    // download a web file into pFile's initdata
    // pName can be PI.HTTP://stuff, or URIx.stuff
    int outSize = 0;
    unsigned char *buf = getWebFile(pFile->csName, outSize);
    if (NULL == buf) {
        debug_write("bufferWebFile failed...");
        return false;
    }

    pFile->initData = buf;
    pFile->initDataSize = outSize;
    return true;
}

bool bufferConfig(FileInfo *pFile) {
    // make a fake DV80 configuration file (did docs say DV254??)
    // again, the docs aren't really matching the data...
    // to make life simple, we'll just store one record per sector
    // We can get away with that because of how variable works ;)
    const int RECORDS = 15;  // needs to match what is written below!
    unsigned char *buf = (unsigned char*)malloc(256*RECORDS+128);
    memset(buf, 0xff, 256*RECORDS+128);
    pFile->initData = buf;
    pFile->initDataSize = 256*RECORDS+128;

    // build the TIFILES header - DV80 with 12 records, fully padded
	buf[0] = 7;
	buf[1] = 'T';
	buf[2] = 'I';
	buf[3] = 'F';
	buf[4] = 'I';
	buf[5] = 'L';
	buf[6] = 'E';
	buf[7] = 'S';
	buf[8] = 0;			// length in sectors HB
	buf[9] = RECORDS;	// LB 
	buf[10] = TIFILES_VARIABLE;			// File type 
	buf[11] = 1;		// records/sector
	buf[12] = 255;  	// # of bytes in last sector
	buf[13] = 80;		// record length 
	buf[14] = RECORDS;	// # of records(FIX)/sectors(VAR) LB 
	buf[15] = 0;		// HB

    // write out the records (length, data)
    // padding is already in place
    int off = 128;

    // TODO: automap DSK1 to last load location
    snprintf((char*)&buf[off+1], 8, "AUTO=%s", TipiAuto.GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "DIR_SORT=%s", TipiDirSort.GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // because these tend to be long, and tipicfg doesn't trim, we do
    snprintf((char*)&buf[off+1], 39, "DSK1_DIR=(handled by Classic99)");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 39, "DSK2_DIR=(handled by Classic99)");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 39, "DSK3_DIR=(handled by Classic99)");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 39, "DSK4_DIR=(handled by Classic99)");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // TODO: mouse acceleration
    snprintf((char*)&buf[off+1], 29, "MOUSE_SCALE=50");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // for directory listings?
    snprintf((char*)&buf[off+1], 29, "SECTOR_COUNT=1440");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "TIPI_NAME=%s", TipiName.GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "TZ=%s", TipiTz.GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "URI1=%s", TipiURI[0].GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "URI2=%s", TipiURI[1].GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "URI3=%s", TipiURI[2].GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // In the real file, this is plain text...
    snprintf((char*)&buf[off+1], 80, "WIFI_PSK=******");    // don't show password
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 80, "WIFI_SSID=%s", TipiSSID.GetString());
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    //off+=256;     // not on the last one

    // patch header
	buf[12] = buf[off]+1;  	// # of bytes in last sector

    return true;
}
bool bufferStatus(FileInfo *pFile) {
    // make a fake DV80 configuration file
    // to make life simple, we'll just store one record per sector
    // We can get away with that because of how variable works ;)
    const int RECORDS = 6;  // needs to match what is written below!
    unsigned char *buf = (unsigned char*)malloc(256*RECORDS+128);
    memset(buf, 0xff, 256*RECORDS+128);
    pFile->initData = buf;
    pFile->initDataSize = 256*RECORDS+128;

    // build the TIFILES header - DV80 with 6 records, fully padded to each sector
	buf[0] = 7;
	buf[1] = 'T';
	buf[2] = 'I';
	buf[3] = 'F';
	buf[4] = 'I';
	buf[5] = 'L';
	buf[6] = 'E';
	buf[7] = 'S';
	buf[8] = 0;			// length in sectors HB
	buf[9] = RECORDS;   // LB 
	buf[10] = TIFILES_VARIABLE;			// File type 
	buf[11] = 3;		// (up to) records/sector
	buf[12] = 255;  	// # of bytes in last sector
	buf[13] = 80;		// record length 
	buf[14] = RECORDS;	// # of records(FIX)/sectors(VAR) LB 
	buf[15] = 0;		// HB

    // write out the records (length, data)
    // padding is already in place
    int off = 128;

    // docs for these records are incorrect
    // based on the current version 2.19
    // TODO: get windows MAC address
    snprintf((char*)&buf[off+1], 255, "MAC_WLAN0=00:00:00:00:00:00");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // TODO: get windows IP address
    snprintf((char*)&buf[off+1], 255, "IP_WLAN0=127.0.0.1");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 255, "VERSION=2.19");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    snprintf((char*)&buf[off+1], 255, "RELDATE=2021-02-26");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // TODO: what is this UUID?
    snprintf((char*)&buf[off+1], 255, "UUID=00000000-0000-0000-0000-000000000000");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    off+=256;

    // TODO: I assume it fetched latest from the server... doesn't help us anyway
    snprintf((char*)&buf[off+1], 255, "LATEST=2.19");
    buf[off] = strlen((char*)&buf[off+1])&0xff; 
    buf[off+1+buf[off]] = 0xff;  // fix terminator
    //off+=256; // not on last record

    // patch header
	buf[12] = buf[off]+1;  	// # of bytes in last sector

    return true;
}

bool dsrPiPio() {
    debug_write("PI.PIO NOT IMPLEMENTED");
    return false;

}
bool dsrPiTcp() {
    debug_write("PI.TCP NOT IMPLEMENTED");
    return false;

}
bool dsrPiUdp() {
    debug_write("PI.UDP NOT IMPLEMENTED");
    return false;

}
bool dsrPiVars() {
    debug_write("PI.VARS NOT IMPLEMENTED");
    return false;

}
bool dsrPiHardwareNop() {
    debug_write("Ignoring action, hardware not supported.");
    return true;
}

// These functions directly interface to several functions
// In each of these R0 is the length of the message buffer,
// and R1 is the pointer to it. Workspace is expected to be >83E0
// For receive, R0 is updated with the new length of data.
// I don't think the "V" versions are used as often, but
// they appear to access VDP instead of CPU RAM. Generally,
// a message is sent, and then return data is received. This
// suggests only one message should be active at a time.
// TODO: There is a note in the docs that says messages MUST be received,
// so maybe there is a queue or a block?

bool handleSendMsg(unsigned char *buf, int len) {
    if (isNetvars) {
        // meant for netvars
        isNetvars = false;
        return handleNetvars(buf, len);
    }

    // otherwise, first byte of the buffer tells us what to do
    switch (buf[0]) {
    case 0x20:  // read mouse
        return handleMouse(buf, len);
    case 0x21:  // netvars
        isNetvars = true;   // NEXT message will have the data
        return true;
    case 0x22:  // TCP
        return handleTcp(buf, len);
    case 0x23:  // UDP
        return handleUdp(buf, len);
    default:
        // TODO: what does TIPI do with an unknown extension?
        return false;
    }
}

bool directSendMsg() {
    unsigned char buf[65537];   // max size plus terminator
    int wp = pCurrentCPU->GetWP();
    if (wp != 0x83e0) {
        debug_write("Warning: Call TIPI functions with GPLWS (>83E0), SendMsg WP is >%04X", wp);
    }
    int len = romword(wp);
    int off = romword(wp+2);
    
    // because CPU memory is fragmented, use the read functions to fetch the data
    // we don't need to sanity test, this is safe
    for (int idx=0; idx<len; ++idx) {
        buf[idx] = rcpubyte(off+idx);
    }
    buf[len] = '\0';

    // now figure out what to do with it
    if (!handleSendMsg(buf, len)) {
        debug_write("handleSendMsg failed");
    }

    // always return false, direct calls don't skip like DSRs
    return false;
}
bool directVSendMsg() {
    // same, but from VDP
    unsigned char buf[65537];   // max size plus terminator
    int wp = pCurrentCPU->GetWP();
    if (wp != 0x83e0) {
        debug_write("Warning: Call TIPI functions with GPLWS (>83E0), VSendMsg WP is >%04X", wp);
    }
    int len = romword(wp);
    int off = romword(wp+2);
    if (off+len > 0x3fff) {
        debug_write("Illegal VDP access in VSendMsg, wrapping (address >%04X, len >%04X)", off, len);
        off &= 0x3fff;
        len = 0x4000-off;
    }
    // since we need to terminate the buffer, we still need to copy
    memcpy(buf, &VDP[off], len);
    buf[len] = '\0';

    if (!handleSendMsg(buf, len)) {
        debug_write("handleSendMsg (VDP) failed");
    }

    // always return false, direct calls don't skip like DSRs
    return false;
}

bool directRecvMsg() {
    int wp = pCurrentCPU->GetWP();
    if (wp != 0x83e0) {
        debug_write("Warning: Call TIPI functions with GPLWS (>83E0), RecvMsg WP is >%04X", wp);
    }
    int len = romword(wp);
    int off = romword(wp+2);

    // TODO temp: TIPI currently ignores R0, so
    // we set it to 0 here to force the write to always happen
    len = 0;

    // anyway, here we make sure we don't send more than the message
    if ((len == 0) || (len > rxMessageLen)) {
        len = rxMessageLen;
        wrword(wp, len);
    }

    if (len > 0) {
        for (int idx=0; idx<len; ++idx) {
            // safe function, don't need to check here
            wcpubyte(off+idx, rxMessageBuf[idx]);
        }
    }

    // always return false
    return false;
}
bool directVRecvMsg() {
    int wp = pCurrentCPU->GetWP();
    if (wp != 0x83e0) {
        debug_write("Warning: Call TIPI functions with GPLWS (>83E0), VRecvMsg WP is >%04X", wp);
    }
    int len = romword(wp);
    int off = romword(wp+2);

    // TODO temp: TIPI currently ignores R0, so
    // we set it to 0 here to force the write to always happen
    len = 0;

    if ((len == 0) || (len > rxMessageLen)) {
        len = rxMessageLen;
        wrword(wp, len);
    }

    if (len > 0) {
        // TODO part two - need to check the range again since we faked it above
        if (off+len > 0x3fff) {
            debug_write("Illegal VDP access in VSendMsg, truncating (address >%04X, len >%04X)", off, len);
            off &= 0x3fff;
            len = 0x4000-off;
        }
        // copy should be safe now
        memcpy(&VDP[off], rxMessageBuf, len);
    }

    // always return false
    return false;
}

void resizeBuffer(int size) {
    // todo: we could track message length and buffer size separately
    // and reduce how often we need to realloc this memory...
    if (size > rxMessageLen) {
        rxMessageBuf = (unsigned char*)realloc(rxMessageBuf, size);
    }
    rxMessageLen = size;
}

bool handleMouse(unsigned char *buf, int len) {
    if (len != 1) {
        debug_write("Warning: TIPI mouse message with length %d - 1 expected.", len);
    }
    // just load up the mouse data in the return buffer
    // three bytes: x delta, y delta, buttons
    // button bits are : 1 - left, 2 - right, 4 - middle
    // This means we need the last mouse position...
    static POINT lastMouse = { -1, -1 };

    // update the buffer
    resizeBuffer(3);

    // ... so how will we handle mouse coordinates, anyway?
    // I guess we try to scale to the window size and return
    // in 265x192 pixels
    // todo: this only works when Pause when Window Inactive is set...
    if (!WindowActive) {
        // no move if not active
        memset(rxMessageBuf, 0, 3);
    } else {
        if (GetWindowRect(myWnd, &gWindowRect)) {
            // TODO: this assumption may work poorly in text or 80 column mode...
            // but because it's delta, it may be okay. Note that since the moves
            // are relative, the TI cursor, not the Windows cursor, must be used.
            // We have no way to know where the application cursor is.
            // TODO: Also, double-clicks will still trigger the paste function...
            double scaleX = 256.0/(gWindowRect.right-gWindowRect.left)+.5;
            double scaleY = 192.0/(gWindowRect.bottom-gWindowRect.top)+.5;
            POINT pt;
            GetCursorPos(&pt); // screen coordinates!
            double x = (pt.x-lastMouse.x) * scaleX;
            double y = (pt.y-lastMouse.y) * scaleY;
            lastMouse = pt;

            // so, buttons only if you're pointing at the window
            // that's awkward, but I don't know how else to do it...
            // (short of capture, that is...)
            int btn = 0;
            if (WindowFromPoint(pt) == myWnd) {
                if (GetAsyncKeyState(VK_LBUTTON)&0x8000) {
                    btn |= 0x01;
                }
                if (GetAsyncKeyState(VK_MBUTTON)&0x8000) {
                    btn |= 0x04;
                }
                if (GetAsyncKeyState(VK_RBUTTON)&0x8000) {
                    btn |= 0x02;
                }
            }

            rxMessageBuf[0] = (unsigned char)((int)round(x));
            rxMessageBuf[1] = (unsigned char)((int)round(y));
            rxMessageBuf[2] = btn;
        }
    }
    return true;
}

// remove trailing spaces only
void rstrip(std::string &str) {
    size_t end = str.find_last_not_of(" \n\r\t\f\v");
    if (end != std::string::npos) {
        str = str.substr(0, end+1);
    }
}

bool netvarError(char *err) {
    debug_write(err);
    resizeBuffer(7);
    memcpy(rxMessageBuf, "0\x1e\x45RROR", 7);   // \x45 is 'E', to prevent the hex byte from running on
    return false;
}

SOCKET connectTCP(const char *hostname, const char *port) {
	debug_write("Accessing '%s:%s'", hostname, port);
	// TODO: I think port is mandatory?
	if (('\0' == hostname[0]) || ('\0' == port[0])) {
		debug_write("Bad syntax on TCP open, failing");
		return INVALID_SOCKET;
	} else {
		// try to convert it to data
		SOCKET sock = INVALID_SOCKET;
		struct addrinfo hints;
		struct addrinfo *result, *rp;   // we'll play like the example..
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;  // allows v4 and v6?
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = IPPROTO_TCP;
		int s = getaddrinfo(hostname, port, &hints, &result);
		if (s) {
			debug_write("Failed to look up address in TCP open, code %d", s);
            return INVALID_SOCKET;
		} else {
			/* getaddrinfo() returns a list of address structures.
				Try each address until we successfully connect(2).
				If socket(2) (or connect(2)) fails, we (close the socket
				and) try the next address. */
			int idx = 0;
			for (rp = result; rp != NULL; rp=rp->ai_next) {
				debug_write("Trying result %d", ++idx);
				sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
				if (sock == INVALID_SOCKET) continue;

				if (connect(sock, rp->ai_addr, rp->ai_addrlen) != SOCKET_ERROR) {
					break;  // got it
				}

				closesocket(sock);
				sock = INVALID_SOCKET;
			}

			// clean up, either way
			freeaddrinfo(result);

			if (rp == NULL) {
				debug_write("Failed to configure socket for %s:%s after %d tries", hostname, port, idx);
				return INVALID_SOCKET;
			}
		}
		return sock;
	}
}

bool sendData(SOCKET sock, const char *ptr, int tosend) {
	bool ret = true;
	
	while (tosend > 0) {
		int s = send(sock, ptr, tosend, 0);
		if (s != SOCKET_ERROR) {
			tosend -= s;
		} else {
			debug_write("Socket send failed WSA code 0x%x", WSAGetLastError());
			ret = false;
			break;
		}
	}
	
	return ret;
}

int recvData(SOCKET sock, char *rxMessageBuf, int rxSize) {
	int rxMessageLen = 0;
	fd_set reads = {0};
	TIMEVAL nilTime = {0,0};
	FD_SET(sock, &reads);
	int sel = select(0, &reads, NULL, NULL, &nilTime);
	if ((SOCKET_ERROR == sel)||(0 == sel)) {
		// broken or no data
		rxMessageLen = 0;
	} else {
		int s = recv(sock, (char*)rxMessageBuf, rxSize, 0);
		if (s == SOCKET_ERROR) {
			debug_write("Socket recv failed WSA code 0x%x", WSAGetLastError());
			rxMessageLen = 0;
		} else {
			rxMessageLen = s;   // guaranteed to be smaller or equal
		}
	}
	return rxMessageLen;
}

void dumpmsg(char *msg) {
    char buf[1024];
    strncpy(buf, msg, 1024);
    buf[1023]='\0';

    char *p;
    while ((p=strchr(buf, 0x1e))) {
        *p='/';
    }
    debug_write(buf);
}

bool handleNetvars(unsigned char *buf, int len) {
    // 0x21 namespace 0x1e net-context 0x1e operation 0x1e queue-flag 0x1e [results_var] [ 0x1e key [ 0x1e value ] ]{1:6}
    // https://github.com/jedimatt42/tipi/wiki/Extension-NetVar
    // https://github.com/jedimatt42/tipi/blob/master/services/TipiVariable.py

    // A double-weird and not intuitive part about netvars: the message type 0x21
    // is ALWAYS only the single byte. The service /itself/ then waits for a second
    // message from the TI which contains the specific data.
    // Why not do it all in one message like the other ones??

    /*
NetVars - depends on the Action byte. Despite the name, these may be local! 

/home/tipi/.tipivars/caller_guid is a local file for each caller's stored data (wouldn't there only be one?)
/home/tipi/.tipivars/GLOBAL is a local file for local stored data

The 0x1D byte is used as a separator in the lines in these files.

Input message from TI:

0x21 caller_guid 0x1e net-context 0x1e operation 0x1e queue-flag 0x1e [results_var] [ 0x1e key [ 0x1e value ] ]{1:6}
     |                |                |              |               |                    |          > data to write
     |                |                |              |               |                    > up to 6 keys to write or one to read?
     |                |                |              |               > name of variable to save a response in
     |                |                |              > allow values to queue (local only)
     |                |                > R (read) RS (read short) W (write, up to 6) U (UDP??) T (TCP??)
     |                > namespace for U and T, depends on myti99.com. not used for local
     > filename, basically. caller_guid in code. Can be "GLOBAL" as well.

String is split on the 0x1e bytes. 

return is: "0\x1eERROR" or "1\x1edata"

The save mechanism appears to be a little buggy.. both GLOBAL and a requested GUID file are loaded on
every request, but all new data is added to the requested file, which is then written back out. If
the software is accessing the GLOBAL data, then it's read twice. Maybe not a big deal, a little wasteful.

W/U/T: All writes
	- clear response variable
	- all six key/value pairs are stored in the local caller data (with 0x1D as separator and \n at the end of the line)
	
U: labeled as incomplete (doesn't appear to actually send anything)
	- creates a UDP socket
	- extracts the REMOTE_HOST and REMOTE_PORT from the local global variables
	- closes the socket
	- re-saves the caller_guid file
	
T: TCP send
	- verifies that REMOTE_HOST and REMOTE_PORT are set in the local caller variables
	- clears the response variable (again, W/U/T already did this)
	- extracts a SESSION_ID from the local caller variables, else uses ""
	- creates a string with 0x1e separators of caller_guid, session_id, context, action, and all six key/value pairs, if keys are set
	- opens a TCP connection to REMOTE_HOST:REMOTE_PORT
	- sends the string terminated by \n
	- then receives a response up to 1024 bytes
	- sends error string if it contains (File ") or (Traceback)
	- close the socket
	- save the data in the response variable
	- re-saves the caller_guid file
	- returns a success response with the returned data

R/RS: Read (read simple just omits the header bytes for BASIC)
	- checks if the key exists in the local caller variables (if not, returns error)
	- if queued (meaning a series of replies separated by 0x1e), pops the first one and resaves the file
	- otherwise, if key includes ".RESP", then the key is cleared after reading
	- re-saves caller file regardless
	- returns the result
	
Returns empty string for anything else.

While not explicitly documented, it appears that strings have leading and trailing whitespace stripped.
I don't know what counts as whitespace in this case, so I'm just going to strip anything under exclamation point.
I'm going to strip both reads and writes so we are okay with old variables, if any.
*/
// Based generously on TipiVariable.pu by ElectricLab, because it's late and I'm sleepy ;)

    // we can assume a nul terminator here, but not for parsing, embeded NUL is okay!
    dumpmsg((char*)buf);

    std::vector<std::string> ti_message;
    unsigned char *p = buf;
    while (p-buf < len) {
        std::string x;
        while ((p-buf < len)&&(*p!=0x1e)) {
            x+=*p;
            ++p;
        }
        ti_message.push_back(x);
        ++p;
    }

    std::string caller_guid = ti_message.size() >= 1 ? ti_message[0] : "";
    std::string context = ti_message.size() >= 2 ? ti_message[1] : "";  // ti99.com context
    std::string action = ti_message.size() >= 3 ? ti_message[2] : "";   // R,RS,W,U,T
    std::string queue = ti_message.size() >= 4 ? ti_message[3] : "";    // Allow values to queue for these variables. (only locally for now)
    std::string results_var = ti_message.size() >= 5 ? ti_message[4] : "";  // optional results_var

    std::string var_key1 = ti_message.size() >= 6 ? ti_message[5] : "";
    std::string var_val1 = ti_message.size() >= 7 ? ti_message[6] : "";
    std::string var_key2 = ti_message.size() >= 8 ? ti_message[7] : "";
    std::string var_val2 = ti_message.size() >= 9 ? ti_message[8] : "";
    std::string var_key3 = ti_message.size() >= 10 ? ti_message[9] : "";
    std::string var_val3 = ti_message.size() >= 11 ? ti_message[10] : "";
    std::string var_key4 = ti_message.size() >= 12 ? ti_message[11] : "";
    std::string var_val4 = ti_message.size() >= 13 ? ti_message[12] : "";
    std::string var_key5 = ti_message.size() >= 14 ? ti_message[13] : "";
    std::string var_val5 = ti_message.size() >= 15 ? ti_message[14] : "";
    std::string var_key6 = ti_message.size() >= 16 ? ti_message[15] : "";
    std::string var_val6 = ti_message.size() >= 17 ? ti_message[16] : "";

    std::string response = results_var.length() ? results_var : "";

    if (caller_guid != "GLOBAL") {
        debug_write("Warning: TIPIsim Netvars using non-global namespace is ignored");
    }

    // TODO: not persistent, so nothing to load here

    // All three write cases pass through this block ("W" /only/ does so)
    if (action == "W" || action == "U" || action == "T") {
        ti_vars[response] = "";     // Blank out our old response
        rstrip(var_val1);
        ti_vars[var_key1] = var_val1;
        rstrip(var_val2);
        ti_vars[var_key2] = var_val2;
        rstrip(var_val3);
        ti_vars[var_key3] = var_val3;
        rstrip(var_val4);
        ti_vars[var_key4] = var_val4;
        rstrip(var_val5);
        ti_vars[var_key5] = var_val5;
        rstrip(var_val6);
        ti_vars[var_key6] = var_val6;

        // TODO: original saved here
    }

    if (action == "U") {
        // UDP is non-functional... probably meant to function like TCP
        // but we don't need to do anything here...
    } else if (action == "T") {
        // TCP transmission
        if ((ti_vars["REMOTE_HOST"].length() == 0) || 
            (ti_vars["REMOTE_PORT"].length() == 0)) {
            return netvarError("Remote host not set");
        }

        ti_vars[response] = "";

        // Listener on port 9918 expects:
        // prog_guid
        // session_id
        // app_id
        // context (Optional)
        // action
        // var
        // val

        std::string message;
        std::string session_id = ti_vars["SESSION_ID"];
        message = caller_guid + '\x1e' + session_id + '\x1e' + context + '\x1e' + action;

        if (var_key1.length()) message += '\x1e' + var_key1 + '\x1e' + var_val1;
        if (var_key2.length()) message += '\x1e' + var_key2 + '\x1e' + var_val2;
        if (var_key3.length()) message += '\x1e' + var_key3 + '\x1e' + var_val3;
        if (var_key4.length()) message += '\x1e' + var_key4 + '\x1e' + var_val4;
        if (var_key5.length()) message += '\x1e' + var_key5 + '\x1e' + var_val5;
        if (var_key6.length()) message += '\x1e' + var_key6 + '\x1e' + var_val6;

        SOCKET sock = connectTCP(ti_vars["REMOTE_HOST"].c_str(), ti_vars["REMOTE_PORT"].c_str());
        if (INVALID_SOCKET == sock) {
            ti_vars[response] = "ERROR";
            // TODO: original saved file here
            return netvarError("TIPIsim NetVars server error connecting socket");
        }

        // send data blindly
        message += '\n';
        sendData(sock, message.c_str(), message.length());

        // TODO: I can't help but notice there's no delay here.. is this supposed to block?
        // Maybe we need to loop. I'm gonna pause...
        // now try to get the data back from the server - up to 1k
        // there are two characters to insert at the beginning of the file
        resizeBuffer(1024+2);
        int pos = 2;
        for (int idx=0; idx<10; ++idx) {
            Sleep(50);  // allow up to 500ms

            int ret = recvData(sock, (char*)rxMessageBuf+pos, 1026-pos);
            if (ret > 0) {
                pos += ret;
            } else if (pos > 2) {
                // we already got some, and it stopped, call it done
                break;
            }
        }
        rxMessageLen = pos;
        if (rxMessageLen > 1023+2) rxMessageLen = 1023+2;
        rxMessageBuf[rxMessageLen] = '\0';

        dumpmsg((char*)rxMessageBuf);

        if ((strstr((char*)rxMessageBuf, "File \"")) || (strstr((char*)rxMessageBuf, "Traceback"))) {
            // BAD! Usually means compilation error on far end, esp when running via inetd.
            return netvarError("TIPIsim NetVars error in response string");
        }

        shutdown(sock, SD_BOTH);
        closesocket(sock);

        ti_vars[response] = (char*)(rxMessageBuf+2);

        // TODO: original code saved file here
        rxMessageBuf[0] = '1';
        rxMessageBuf[1] = 0x1e;

        // and done
        return true;
    } else if ((action == "R") || (action == "RS")) {
        // Read! 'RS' is "READ SIMPLE" which will just return the value, not preceeded by return code. Better for BASIC!
        if (ti_vars.find(var_key1) != ti_vars.end()) {
            debug_write(">> '%s'", ti_vars[var_key1].c_str());
            // Need to check to see if variable is queued, uses ASCII 31 (Unit Separator) to delimit.
            // If so, we want to pop off the first one and return it.
            if (ti_vars[var_key1].find(0x1e) != std::string::npos) {
                size_t pos = ti_vars[var_key1].find(0x1e);
                std::string first_item;
                if (pos > 0) {
                    first_item = ti_vars[var_key1].substr(0, pos);
                }
                ti_vars[var_key1] = ti_vars[var_key1].substr(pos+1, std::string::npos);
                // todo: original should save file here

                if (action == "R") {
                    resizeBuffer(2+first_item.length());
                    rxMessageBuf[0] = '1';
                    rxMessageBuf[1] = 0x1e;
                    memcpy(&rxMessageBuf[2], first_item.c_str(), first_item.length());
                } else {
                    resizeBuffer(first_item.length());
                    memcpy(&rxMessageBuf[0], first_item.c_str(), first_item.length());
                }
                return true;
            } else {
                response = ti_vars[var_key1];
                
                if (var_key1.find(".RESP") != std::string::npos) {
                    ti_vars[var_key1] = "";
                }

                // TODO: original saved file here

                if (action == "R") {
                    resizeBuffer(2+response.length());
                    rxMessageBuf[0] = '1';
                    rxMessageBuf[1] = 0x1e;
                    memcpy(&rxMessageBuf[2], response.c_str(), response.length());
                } else {
                    resizeBuffer(response.length());
                    memcpy(&rxMessageBuf[0], response.c_str(), response.length());
                }
            }
        } else {
            // no data
            if (action == "R") {
                return netvarError("Empty data - no key available for read");
            } else {
                resizeBuffer(5);
                memcpy(&rxMessageBuf[0], "ERROR", 5);
            }
        }
    }

    return true;
}

bool handleTcp(unsigned char *buf, int len) {
    // https://github.com/jedimatt42/tipi/wiki/Extension-TCP
    if (len < 3) {
        debug_write("Illegal TCP access, missing data, len %d", len);
        return false;
    }

    int index = buf[1]; // handle byte - caller specifies
    int cmd = buf[2];   // what to do

    switch (cmd) {
    case 0x01:  // open
        {
            // data expected is a string: hostname:port
            bool ret = true;

            // probably paranoid...
            shutdown(sock[index], SD_BOTH);
            closesocket(sock[index]);
            sock[index] = INVALID_SOCKET;

            // parse the remote string
            CString hostname;
            CString port;
            int idx = 3;
            while (idx<len) {
                if ((buf[idx]==':') || (buf[idx]=='\0')) break;
                hostname += buf[idx++];
            }
            if (buf[idx] == ':') {
                ++idx;
                while (idx<len) {
                    if (buf[idx]=='\0') break;
                    port += buf[idx++];
                }
            }
            
            sock[index] = connectTCP(hostname.GetString(), port.GetString());
            if (INVALID_SOCKET == sock[index]) {
                ret = false;
            }

            resizeBuffer(1);
            if (ret) {
                rxMessageBuf[0] = 255;
            } else {
                rxMessageBuf[0] = 0;
            }
        }
        break;

    case 0x06:  // unbind
        // I think the real TIPI has server and client sockets in different spaces, thus the different
        // function. For me, it should work fine to just close it...
        // fall through...
    case 0x02:  // close
        shutdown(sock[index], SD_BOTH);
        closesocket(sock[index]);
        sock[index] = INVALID_SOCKET;
        resizeBuffer(1);
        rxMessageBuf[0] = 255;
        break;

    case 0x03:  // write
        {
            int tosend = len-3;
            unsigned char *ptr = buf+3;
            bool ret = true;
            resizeBuffer(1);

            ret = sendData(sock[index], (const char*)ptr, tosend);

            if (ret) {
                rxMessageBuf[0] = 255;
            } else {
                rxMessageBuf[0] = 0;
            }
        }
        break;

    case 0x04:  // read
        if (len < 5) {
            debug_write("UDP read command string too short");
            rxMessageLen = 0;
        } else {
            int rxSize = buf[3]*256 + buf[4];
            resizeBuffer(rxSize);
            rxMessageLen = recvData(sock[index], (char*)rxMessageBuf, rxSize);
        }
        break;

    case 0x05:  // bind (instead of open)
        // this is very similar to open, but creates a listen socket
        {
            // data expected is a string: interface:port (interface can be '*' for all)
            bool ret = true;

            // probably paranoid...
            shutdown(sock[index], SD_BOTH);
            closesocket(sock[index]);
            sock[index] = INVALID_SOCKET;

            // parse the local string
            CString hostname;
            CString port;
            int idx = 3;
            while (idx<len) {
                if ((buf[idx]==':') || (buf[idx]=='\0')) break;
                hostname += buf[idx++];
            }
            if (buf[idx] == ':') {
                ++idx;
                while (idx<len) {
                    if (buf[idx]=='\0') break;
                    port += buf[idx++];
                }
            }
            debug_write("Binding to %s:%s", hostname.GetString(), port.GetString());
            if ((hostname.IsEmpty()) || (port.IsEmpty())) {
                debug_write("Bad syntax on TCP bind, failing");
                ret = false;
            } else {
                // try to convert it to data
                struct addrinfo hints;
                struct addrinfo *result;   // we'll play like the example..
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;  // TODO: unspec??
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_flags = AI_PASSIVE;
                hints.ai_protocol = IPPROTO_TCP;

                int s = getaddrinfo(hostname=="*" ? NULL : hostname.GetString(), port.GetString(), &hints, &result);
                if (s) {
                    debug_write("Failed to look up local address in TCP bind, code %d", s);
                    ret = false;
                } else {
                    // in this case, we expect only one response
                    sock[index] = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
                    if (sock[index] == INVALID_SOCKET) {
                        debug_write("Failed to configure listen socket for %s:%s", hostname, port);
                        ret = false;
                    } else {
                        if (bind(sock[index], result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
                            debug_write("Failed to bind listen socket for %s:%s", hostname, port);
                            closesocket(sock[index]);
                            sock[index] = INVALID_SOCKET;
                            ret = false;
                        } else {
                            if (listen(sock[index], SOMAXCONN) == SOCKET_ERROR) {
                                debug_write("Failed to listen on listen socket for %s:%s", hostname, port);
                                closesocket(sock[index]);
                                sock[index] = INVALID_SOCKET;
                                ret = false;
                            }
                        }
                    }

                    // clean up, either way
                    freeaddrinfo(result);
                }
            }
            resizeBuffer(1);
            if (ret) {
                rxMessageBuf[0] = 255;
            } else {
                rxMessageBuf[0] = 0;
            }
        }
        break;

    //case 0x06:  // unbind (see close above)

    case 0x07:  // accept
        // try to accept on the server socket and return a new handle
        // 0 for failure, 255 if not a server socket
        {
            unsigned char ret = 255;    // assume bad socket...

            int outsock = 0;
            for (int idx=1; idx<255; ++idx) {
                // can't use 0 or 255
                if (sock[idx] == INVALID_SOCKET) {
                    outsock = idx;
                }
            }
            if (outsock == 0) {
                // pretty unlikely...
                debug_write("No sockets available for accept...");
                ret = 0;
            } else {
                // check if there is a connection to accept
                fd_set reads = {0};
                TIMEVAL nilTime = {0,0};
                FD_SET(sock[index], &reads);
                int sel = select(0, &reads, NULL, NULL, &nilTime);
                if (SOCKET_ERROR == sel) {
                    ret = 255;
                } else if (0 == sel) {
                    // nobody's waiting
                    ret = 0;
                } else {
                    // accept, don't care from who
                    sock[outsock] = accept(sock[index], NULL, NULL);
                    if (INVALID_SOCKET == sock[outsock]) {
                        int err = WSAGetLastError();
                        if (err == WSAENOTSOCK) {
                            ret = 255;
                        } else {
                            ret = 0;
                        }
                        debug_write("Accept failed, WSAE error 0x%x", err);
                    } else {
                        // return valid accepted socket
                        ret = outsock;
                    }
                }
            }
            resizeBuffer(1);
            rxMessageBuf[0] = ret;
        }
        break;

    default:
        debug_write("Unknown UDP command >%02X", cmd);
        return false;
    }

    return true;
}

bool handleUdp(unsigned char *buf, int len) {
    if (len < 3) {
        debug_write("Illegal UDP access, missing data, len %d", len);
        return false;
    }

    int index = buf[1]; // handle byte - caller specifies
    int cmd = buf[2];   // what to do

    switch (cmd) {
    case 0x01:  // open
        {
            // data expected is a string: hostname:port
            // TODO: can we open an unconnected socket?
            bool ret = true;

            // probably paranoid...
            shutdown(sock[index], SD_BOTH);
            closesocket(sock[index]);
            sock[index] = INVALID_SOCKET;

            // parse the remote string
            CString hostname;
            CString port;
            int idx = 3;
            while (idx<len) {
                if ((buf[idx]==':') || (buf[idx]=='\0')) break;
                hostname += buf[idx++];
            }
            if (buf[idx] == ':') {
                ++idx;
                while (idx<len) {
                    if (buf[idx]=='\0') break;
                    port += buf[idx++];
                }
            }
            debug_write("Accessing %s:%s", hostname.GetString(), port.GetString());
            // TODO: I think port is mandatory?
            if ((hostname.IsEmpty()) || (port.IsEmpty())) {
                debug_write("Bad syntax on UDP open, failing");
                ret = false;
            } else {
                // try to convert it to data
                struct addrinfo hints;
                struct addrinfo *result, *rp;   // we'll play like the example..
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_UNSPEC;  // allows v4 and v6?
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_flags = 0;
                hints.ai_protocol = IPPROTO_UDP;
                int s = getaddrinfo(hostname.GetString(), port.GetString(), &hints, &result);
                if (s) {
                    debug_write("Failed to look up address in UDP open, code %d", s);
                    ret = false;
                } else {
                    /* getaddrinfo() returns a list of address structures.
                        Try each address until we successfully connect(2).
                        If socket(2) (or connect(2)) fails, we (close the socket
                        and) try the next address. */
                    int idx = 0;
                    for (rp = result; rp != NULL; rp=rp->ai_next) {
                        debug_write("Trying result %d", ++idx);
                        sock[index] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                        if (sock[index] == INVALID_SOCKET) continue;

                        if (connect(sock[index], rp->ai_addr, rp->ai_addrlen) != SOCKET_ERROR) {
                            break;  // got it
                        }

                        closesocket(sock[index]);
                        sock[index] = INVALID_SOCKET;
                    }

                    // clean up, either way
                    freeaddrinfo(result);

                    if (rp == NULL) {
                        debug_write("Failed to configure socket for %s:%s after %d tries", hostname, port, idx);
                        ret = false;
                    }
                }
            }
            resizeBuffer(1);
            if (ret) {
                rxMessageBuf[0] = 255;
            } else {
                rxMessageBuf[0] = 0;
            }
        }
        break;

    case 0x02:  // close
        shutdown(sock[index], SD_BOTH);
        closesocket(sock[index]);
        sock[index] = INVALID_SOCKET;
        resizeBuffer(1);
        rxMessageBuf[0] = 255;
        break;

    case 0x03:  // write
        {
            int tosend = len-3;
            unsigned char *ptr = buf+3;
            bool ret = true;
            resizeBuffer(1);

            while (tosend > 0) {
                int s = send(sock[index], (const char*)ptr, tosend, 0);
                if (s != SOCKET_ERROR) {
                    tosend -= s;
                } else {
                    debug_write("Socket[%d] send failed WSA code 0x%x", index, WSAGetLastError());
                    ret = false;
                    break;
                }
            }
            if (ret) {
                rxMessageBuf[0] = 255;
            } else {
                rxMessageBuf[0] = 0;
            }
        }
        break;

    case 0x04:  // read
        if (len < 5) {
            debug_write("UDP read command string too short");
            rxMessageLen = 0;
        } else {
            int rxSize = buf[3]*256 + buf[4];
            resizeBuffer(rxSize);
            int s = recv(sock[index], (char*)rxMessageBuf, rxSize, 0);
            if (s == SOCKET_ERROR) {
                debug_write("Socket[%d] recv failed WSA code 0x%x", index, WSAGetLastError());
                rxMessageLen = 0;
            } else {
                rxMessageLen = s;   // guaranteed to be smaller or equal
            }
        }
        break;

    default:
        debug_write("Unknown UDP command >%02X", cmd);
        return false;
    }

    return true;
}

// Web disk class
// TODO: if we changed the FiadDisk object to work with streams instead of files directly,
// then we could feed it file streams and buffer streams, and share all that parsing code
// more easily...

// TODO: as implemented so far, writes are all disallowed, but I need to be able to write
// to PI.CONFIG for the config tool to work...

// constructor
TipiWebDisk::TipiWebDisk() : FiadDisk() {
}

TipiWebDisk::~TipiWebDisk() {
}

// create an output file
bool TipiWebDisk::CreateOutputFile(FileInfo *pFile) {
    // we should be called only for PI.CONFIG, so we rely on that.
    // create a new memory-only file

    // we need to open PI.CONFIG in append mode, but nothing is being destroyed...
    // save this in case other files are added...
#if 0
    // first a little sanity checking -- we never overwrite an existing file
	// unless the mode is 'output', so check existance against the mode
	if ((pFile->Status & FLAG_MODEMASK) != FLAG_OUTPUT) {
		// no, we are not
		debug_write("Can't overwrite PI.CONFIG with open mode 0x%02X", (pFile->Status & FLAG_MODEMASK));
		return false;
	}
#endif

	// check if the user requested a default record length, and fill it in if so
	if (pFile->RecordLength == 0) {
		pFile->RecordLength = 80;
	}
    if (pFile->RecordLength != 80) {
        debug_write("PI.CONFIG must be opened as D/V80");
        return false;
    }

	// now make sure the type is display/variable
	if (pFile->Status&FLAG_INTERNAL) {
		debug_write("PI.CONFIG supports display files only");
		return false;
	}
	if (0==(pFile->Status&FLAG_VARIABLE)) {
		debug_write("PI.CONFIG supports variable records only");
		return false;
	}

	// calculate necessary data and fill in header fields
	// Most of this is probably meaningless here, but that's okay, we'll keep it
	pFile->RecordsPerSector = 256  / (pFile->RecordLength + ((pFile->Status & FLAG_VARIABLE) ? 1 : 0) );
	pFile->LengthSectors = 1;
	pFile->FileType = 0;
	if (pFile->Status & FLAG_VARIABLE) pFile->FileType|=TIFILES_VARIABLE;
	if (pFile->Status & FLAG_INTERNAL) pFile->FileType|=TIFILES_INTERNAL;
	pFile->BytesInLastSector = 0;
	pFile->NumberRecords = 0;

	// Allocate a buffer for it (size of 15 records by default)
	if (NULL != pFile->pData) {
		free(pFile->pData);
	}
	pFile->pData = (unsigned char*)malloc(15 * (pFile->RecordLength+2));
	pFile->bDirty = true;

	return true;
}

// Open an existing file from RAM (initData), check the header against the parameters
bool TipiWebDisk::TryOpenFile(FileInfo *pFile) {
	char *pMode;
	int nMode = pFile->Status & FLAG_MODEMASK;	// should be UPDATE, APPEND or INPUT
	FileInfo lclInfo;

	switch (nMode) {
		case FLAG_UPDATE:
			pMode="update";
			break;

		case FLAG_INPUT:
			pMode="input";
			break;

		default:
			debug_write("Illegal or Unknown mode - can't open.");
		    pFile->LastError = ERR_FILEERROR;
		    return false;
	}

	if (NULL == pFile->initData) {
		debug_write("No file data for %s, failing.", pMode);
		pFile->LastError = ERR_FILEERROR;
		return false;
	}

	// there should be no data to copy as we are just opening the file
	lclInfo.CopyFileInfo(pFile, false);
	DetectImageType(&lclInfo);

	if (lclInfo.ImageType == IMAGE_UNKNOWN) {
		debug_write("%s is an unknown file type - can not open.", (LPCSTR)lclInfo.csName);
		pFile->LastError = ERR_BADATTRIBUTE;
		return false;
	}

    // TODO: TIPI allows headerless files, but as what type? It's not the open mode...
#if 0
    // special case - a headerless file normally detects as DF128, but
    // I'm going to allow software to open them as IF128 too.
    // pFile is the request, lclInfo is the disk file
    if (lclInfo.ImageType == IMAGE_IMG) {
        if (((pFile->FileType & TIFILES_INTERNAL) == TIFILES_INTERNAL) && ((lclInfo.FileType & TIFILES_INTERNAL) == 0)) {
            // it's a headerless file, which should be DF128, but the app wants IF128. Since there's
            // no difference save the flag, we're going to allow it. ;)
            debug_write("Changing headerless filetype to IF128 to match open request.");
            lclInfo.FileType|=TIFILES_INTERNAL;
            lclInfo.Status|=FLAG_INTERNAL;
        }
    }
#else
    if (lclInfo.ImageType == IMAGE_IMG) {
		debug_write("%s is missing TIFILES header - can not open.", (LPCSTR)lclInfo.csName);
		pFile->LastError = ERR_BADATTRIBUTE;
		return false;
	}
#endif

	// Verify the parameters as a last step before we OK it all, but only on open
	if ((pFile->OpCode == OP_OPEN) || (pFile->OpCode == OP_LOAD)) {
		if ((pFile->FileType&TIFILES_MASK) != (lclInfo.FileType&TIFILES_MASK)) {
			// note: don't put function calls into varargs!
			const char *str1,*str2;
			str1=GetAttributes(lclInfo.FileType);
			str2=GetAttributes(pFile->FileType);
			debug_write("Incorrect file type: %d/%s%d (real) vs %d/%s%d (requested)", lclInfo.FileType&TIFILES_MASK, str1, lclInfo.RecordLength, pFile->FileType&TIFILES_MASK, str2, pFile->RecordLength);
			pFile->LastError = ERR_BADATTRIBUTE;
			return false;
		}
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

	// seems okay? Copy the data over from the PAB
	pFile->CopyFileInfo(&lclInfo, false);
	return true;
}

// check a string for a meaningful PI.CONFIG value
// return 1 if updated or 0 if not
int processConfig(char *key) {
    char *dat = strchr(key, '=');
    if (NULL == dat) {
        return 0;
    }
    *dat = '\0';
    ++dat;

    if (0 == strcmp(key, "AUTO")) {
        TipiAuto = dat;
        return 1;
    }
    if (0 == strcmp(key, "DIR_SORT")) {
        TipiDirSort = dat;
        return 1;
    }
    if (0 == strcmp(key, "AUTO")) {
        TipiAuto = dat;
        return 1;
    }
    // Skipping DSK1_DIR, DSK2_DIR, DSK3_DIR, DSK4_DIR
    // Should do MOUSE_SCALE, SECTOR_COUNT
    if (0 == strcmp(key, "TIPI_NAME")) {
        TipiName = dat;
        return 1;
    }
    if (0 == strcmp(key, "TZ")) {
        TipiTz = dat;
        return 1;
    }
    if (0 == strcmp(key, "AUTO")) {
        TipiAuto = dat;
        return 1;
    }
    if (0 == strcmp(key, "URI1")) {
        TipiURI[0] = dat;
        return 1;
    }
    if (0 == strcmp(key, "URI2")) {
        TipiURI[1] = dat;
        return 1;
    }
    if (0 == strcmp(key, "URI3")) {
        TipiURI[2] = dat;
        return 1;
    }
    if (0 == strcmp(key, "WIFI_PSK")) {
        TipiPSK = dat;
        return 1;
    }
    if (0 == strcmp(key, "WIFI_SSID")) {
        TipiSSID = dat;
        return 1;
    }

    return 0;
}

// Write the disk buffer out to the file, with appropriate modes and headers
// Again, we are /assuming/ that ONLY PI.CONFIG can get to this code...
// Unlike the real TIPI (?) we don't allow adding new records, and in fact
// only a very few records can be updated...
bool TipiWebDisk::Flush(FileInfo *pFile) {
	CString csOut;

	// first make sure this is an open file!
	if (!pFile->bDirty) {
		// nothing to flush, return success anyway
		return true;
	}

	// The assumption is made that these are valid text strings
	unsigned char *pData = pFile->pData;
	char tmp[256];
    int updates = 0;
	if (NULL == pData) {
		debug_write("Warning: no data to flush.");
	} else {
		// parse out the strings, looking for interesting ones
		for (int idx=0; idx<pFile->NumberRecords; idx++) {
			if (pData-pFile->pData >= pFile->nDataSize) {
				break;
			}
			int nLen = *(unsigned short*)pData;
			pData+=2;

			memset(tmp, 0, sizeof(tmp));
			memcpy(tmp, pData, nLen);

            updates += processConfig(tmp);

			pData+=pFile->RecordLength;
		}
	}

	debug_write("Flushed %d records to PI.CONFIG, %d updates", pFile->NumberRecords, updates);
	pFile->bDirty = false;

	return true;
}

// Determine the type of image (in initData) into ImageType
// This also translates the TIFILES FileType back to a PAB status type
void TipiWebDisk::DetectImageType(FileInfo *pFile) {
	unsigned char buf[512];		// buffer to read first block into

    // TIPI only supports TIFILES and so will we
    // otherwise we're just going to confuse ourselves...
    // Anything else, including V9T9 and Text, will be unknown
    pFile->ImageType = IMAGE_UNKNOWN;

	// data must exist
	if ((NULL == pFile->initData)||(pFile->initDataSize < 128)) {
		debug_write("Can't read file data");
		return;
	}

	// we have it open - read the first block then close it
    // TODO: don't really need this copy...
    memcpy(buf, pFile->initData, 128);
	
	// check for TIFILES, if supported
	// the first characters would be a length and then "TIFILES" if it's a TIFILES file
	if ((0==_strnicmp((char*)buf, "\x07TIFILES", 8)) || (pFile->csOptions.Find('T')!=-1)) {
		debug_write("Detected data as a TIFILES file");
		pFile->ImageType = IMAGE_TIFILES;
		// fill in the information 
		pFile->LengthSectors=(buf[8]<<8)|buf[9];
		pFile->FileType=buf[10];
		pFile->RecordsPerSector=buf[11];
		pFile->BytesInLastSector=buf[12];
		pFile->RecordLength=buf[13];
		pFile->NumberRecords=(buf[15]<<8)|buf[14];		// NOTE: swapped on disk!
		// translate FileType to Status
		pFile->Status = 0;
		if (pFile->FileType & TIFILES_VARIABLE) pFile->Status|=FLAG_VARIABLE;
		if (pFile->FileType & TIFILES_INTERNAL) pFile->Status|=FLAG_INTERNAL;
		// do some fixup of the NumberRecords field
		// If it's variable or fixed, we should easily determine if the value is byteswapped
		if (pFile->FileType & TIFILES_VARIABLE) {
			// must match the number of sectors
			if (pFile->NumberRecords != pFile->LengthSectors) {
				// check for byte swap (better debugging)
                // TODO: option to breakpoint on these warnings - sometimes they scroll off too quickly
                // Or maybe warnings should have their own output window? That would be handy!
				if (((buf[14]<<8)|(buf[15])) == pFile->LengthSectors) {
					debug_write("Warning: Var File had Number Records byte-swapped - will fix - recommend re-saving file");
					pFile->NumberRecords = pFile->LengthSectors;
				} else {
					debug_write("Warning: Number Records doesn't match sector length on variable file.");
				}
			}
		} else {
			// is an actual record count, but we can use LengthSectors and RecordsPerSector to
			// see if it was byte swapped. We don't bother with using BytesInLastSector to get the actual count
			if (pFile->NumberRecords > (pFile->LengthSectors+1)*pFile->RecordsPerSector) {
				if (((buf[14]<<8)|(buf[15])) <= (pFile->LengthSectors+1)*pFile->RecordsPerSector) {
					debug_write("Warning: Fix File had Number Records byte-swapped - will fix - recommend re-saving file");
					pFile->NumberRecords = ((buf[14]<<8)|(buf[15]));
				}
			}
		}
		return;
	}

	// no other match, just return default
	debug_write("Data could not be identified.");
}


// Read the data from initData into the disk buffer
// This function's job is to read the data into individual records
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
// Success or failure, initData is freed.
bool TipiWebDisk::BufferFile(FileInfo *pFile) {
	// this checks for PROGRAM images as well as Classic99 sequence bugs
	if (0 == pFile->RecordLength) {
		debug_write("Attempting to buffer file with 0 record length, can't do it.", (LPCSTR)pFile->csName);
        if (NULL != pFile->initData) free(pFile->initData);
        pFile->initData = NULL;
        pFile->initDataSize = 0;
		return false;
	}

	// all right, then, let's give it a shot.
	// So really, all we do here is branch out to the correct function
	switch (pFile->ImageType) {
		case IMAGE_UNKNOWN:
			debug_write("Attempting to buffer unknown file type %s, can't do it.", (LPCSTR)pFile->csName);
			break;

		case IMAGE_TIFILES:
			// these two can be handled the same way
            { 
                bool ret = BufferFiadFile(pFile);
                if (NULL != pFile->initData) free(pFile->initData);
                pFile->initData = NULL;
                pFile->initDataSize = 0;
                return ret;
            }

		default:
			debug_write("Failed to buffer undetermined file type %d for %s", pFile->ImageType, (LPCSTR)pFile->csName);
	}

    if (NULL != pFile->initData) free(pFile->initData);
    pFile->initData = NULL;
    pFile->initDataSize = 0;
	return false;
}

// Buffer a TIFILES or V9T9 style file -- after the
// header, these filetypes are the same so both are
// handled here. Of course, this should only get TIFILES here...
bool TipiWebDisk::BufferFiadFile(FileInfo *pFile) {
	int idx, nSector;
	unsigned char *pData;
    unsigned char *pOffset;

    if (NULL == pFile->initData) {
        debug_write("Nothing to buffer, failing.");
        return false;
    }

	// Fixed records are obvious. Variable length records are prefixed
	// with a byte that indicates how many bytes are in this record. It's
	// all padded to 256 byte blocks. If it won't fit, the space in the
	// rest of the sector is wasted (and 0xff marks it)
	// Even better - the NumberRecords field in a variable file is a lie,
	// it's really a sector count. So we need to read them differently,
	// more like a text file.

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
	nSector=256;					// bytes left in this sector
    pOffset = pFile->initData + HEADERSIZE; // skip the header
	pData = pFile->pData;

	// we need to let the embedded code decide the terminating rule
	for (;;) {
		if (pOffset >= pFile->initData+pFile->initDataSize) {
			debug_write("Premature EOF - truncating read at record %d.", idx);
			pFile->NumberRecords = idx;
			break;
		}

		if (pFile->Status & FLAG_VARIABLE) {
			// read a variable record
			int nLen=*(pOffset++);
			if (pOffset >= pFile->initData+pFile->initDataSize) {
				debug_write("Corrupt file - truncating read at record %d.", idx);
				pFile->NumberRecords = idx;
				break;
			}

			nSector--;
			if (nLen==0xff) {
				// end of sector indicator, no record read, skip rest of sector
                pOffset+=nSector;
				nSector=256;
				pFile->NumberRecords--;
				// are we done?
				if (pFile->NumberRecords == 0) {
					// yes we are, get the true count
					pFile->NumberRecords = idx;
					break;
				}
			} else {
				// check for buffer resize
				if ((pFile->pData+pFile->nDataSize) - pData < (pFile->RecordLength+2)*10) {
					int nOffset = pData - pFile->pData;		// in case the buffer moves
					// time to grow the buffer - add another 100 lines
					pFile->nDataSize += (100) * (pFile->RecordLength + 2);
                    unsigned char *pTmp = (unsigned char*)realloc(pFile->pData, pFile->nDataSize);
                    if (NULL == pTmp) {
                        debug_write("BufferFIAD failed to allocate memory for data, failing.");
                        pFile->LastError = ERR_FILEERROR;
                        return false;
                    }
		            pFile->pData = pTmp;

					pData = pFile->pData + nOffset;
				}
				
				// clear buffer
				memset(pData, 0, pFile->RecordLength+2);

				// check again
				if ((nSector < nLen) || (pOffset+nLen >= pFile->initData+pFile->initDataSize)) {
					debug_write("Corrupted file - truncating read.");
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

                    memcpy(pData, pOffset, pFile->RecordLength);
                    pOffset+=pFile->RecordLength;
                    nSector-=nLen;
					// skip the excess and trim down nLen
                    pOffset+=nLen - pFile->RecordLength;
					nLen = pFile->RecordLength;
				} else {
					// record is okay (normal case)
					
					// write length data
					*(unsigned short*)pData = nLen;
					pData+=2;

                    memcpy(pData, pOffset, nLen);
                    pOffset+=nLen;
					nSector-=nLen;
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
			if (nSector < pFile->RecordLength) {
				// not enough room for another record, skip to the next sector
                pOffset += nSector;
				nSector=256;
			} else {
				// a little simpler, we just need to read the data
				*(unsigned short*)pData = pFile->RecordLength;
				pData+=2;

			    if (pOffset+pFile->RecordLength >= pFile->initData+pFile->initDataSize) {
				    debug_write("Corrupt file - truncating read at record %d.", idx);
				    pFile->NumberRecords = idx;
				    break;
			    }

                memcpy(pData, pOffset, pFile->RecordLength);
                pOffset+=pFile->RecordLength;
				nSector -= pFile->RecordLength;
				idx++;
				pData += pFile->RecordLength;
			}
		}
	}

	debug_write("Memory read %d records", pFile->NumberRecords);
	return true;
}

// Open a file with a particular mode, creating it if necessary
FileInfo *TipiWebDisk::Open(FileInfo *pFile) {
	FileInfo *pNewFile=NULL;

	if (pFile->bOpen) {
		// trying to open a file that is already open! Can't allow that!
		pFile->LastError = ERR_FILEERROR;
		return NULL;
	}

	// See if we can get a new file handle from the driver
	pNewFile = AllocateFileInfo();
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

    // We should have been given the file. In addition,
    // we don't support directories.
    {
		// let's see what we are doing here...
		switch (pFile->Status & FLAG_MODEMASK) {
			case FLAG_APPEND:   // TIPICFG opens in Append mode...
			case FLAG_OUTPUT:
                // we checked in the main function (the only place that could check, so this
                // must be PI.CONFIG, which we have to allow). Always create an empty output
                // block, which we then read into the file as an array.
			    if (!CreateOutputFile(pFile)) {
				    goto error;
			    }
			    break;

			default:	// should only be FLAG_INPUT or FLAG_UPDATE
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
// need to use a buffer (nor does it close). Success or failure it
// will free initData
bool TipiWebDisk::Load(FileInfo *pFile) {
	int read_bytes;

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
        if (NULL != pFile->initData) free(pFile->initData);
        pFile->initData = NULL;
        pFile->initDataSize = 0;
		return false;
	}
	int nDetectedLength = pFile->LengthSectors*256 + pFile->BytesInLastSector;
	if (pFile->BytesInLastSector != 0) {
		nDetectedLength -= 256;
	}

	// the TI disk controller will fail the load if the file is larger than the buffer!
	// (It won't load a partial file). It's okay if the buffer is larger than the file.
	if (nDetectedLength > pFile->RecordNumber) {
		debug_write("Requested file is larger than available buffer, failing.");
		pFile->LastError = ERR_FILEERROR;
        if (NULL != pFile->initData) free(pFile->initData);
        pFile->initData = NULL;
        pFile->initDataSize = 0;
		return false;
	}

	// XB first tries to load as a PROGRAM image file with the
	// maximum available VDP RAM. If that fails with code 0x60,
	// it tries again as the proper DIS/FIX254
	// I am leaving this comment here, but hacks might not be
	// needed anymore now that the headers are properly tested.

    // we know it has a fixed size header since that's all we support
    read_bytes = min(pFile->RecordNumber, nDetectedLength);
    if (read_bytes > pFile->initDataSize-HEADERSIZE) read_bytes = pFile->initDataSize-HEADERSIZE;
    memcpy(&VDP[pFile->DataBuffer], pFile->initData+HEADERSIZE, read_bytes);
	debug_write("loading 0x%X bytes", read_bytes);	// do we need to give this value to the user?
		
    if (NULL != pFile->initData) free(pFile->initData);
    pFile->initData = NULL;
    pFile->initDataSize = 0;

	// update heatmap
	for (int idx=0; idx<read_bytes; idx++) {
		UpdateHeatVDP(pFile->DataBuffer+idx);
	}

	return true;
}

// Save a PROGRAM image file
bool TipiWebDisk::Save(FileInfo *pFile) {
    debug_write("Save to web not supported");
	pFile->LastError = ERR_ILLEGALOPERATION;
    return false;
}

