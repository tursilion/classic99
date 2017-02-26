// This code contributed by Retroclouds
//
// I hereby grant you the permission to use/change the source code in the 
// attached zip file without any restrictions in your classic99 emulator.
//
// retroclouds (Filip Van Vooren)

#include <Windows.h>
#include <stdio.h>
#include "..\resource.h"

extern bool AddBreakpoint(char *buf1);
extern void debug_write(char *s, ...);
extern const char *FormatBreakpoint(int idx);
extern void ReloadDumpFiles();
extern int nBreakPoints;

int LoadBreakpoints(HWND *myhwnd) {
	OPENFILENAME ofn;                          // Structure for filename dialog
	char buf[256], buf2[256];
	char ReadBuffer[32768] = {0};

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize    = sizeof(OPENFILENAME);
	ofn.hwndOwner      = NULL;
	ofn.lpstrFilter    = "Breakpoint file\0*.brk\0\0"; 
	strcpy(buf, "");
	ofn.lpstrFile      = buf;
	ofn.nMaxFile       = 256;
	strcpy(buf2, "");
	ofn.lpstrFileTitle = buf2;
	ofn.nMaxFileTitle  = 256;
	ofn.Flags          = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn)) {
		// MessageBox( NULL, ofn.lpstrFile, "File Name", MB_OK);
		debug_write("Reading breakpoint file ....");
		HANDLE fh;
		DWORD BytesRead = 0;

		// Create File Handle
		fh = CreateFile(ofn.lpstrFile,         // file to open
						GENERIC_READ,          // open for reading
						FILE_SHARE_READ,       // share for reading
						NULL,                  // default security
						OPEN_EXISTING,         // existing file only
						FILE_ATTRIBUTE_NORMAL, // normal file
						NULL);                 // no attr. template
		if (fh == INVALID_HANDLE_VALUE) {
			MessageBox ( NULL, "Could not create file handle", "Load Breakpoints", MB_OK);
			return false;
		}

		// Read breakpoints file
		if (!(ReadFile(fh, ReadBuffer, 32767, &BytesRead, NULL) && CloseHandle(fh))) {
			MessageBox ( NULL, "Could not read file", "Load Breakpoints", MB_OK);
			return false;
		}
		// MessageBox( NULL, ReadBuffer, "Breakpoints File Content", MB_OK);		
	} else {
		return false;
	}
	
	// Remove all current breakpoints
	nBreakPoints = 0;
	SendDlgItemMessage(*myhwnd, IDC_COMBO1, CB_RESETCONTENT, NULL, NULL);

	// Add new breakpoints
	char *line;
	line = strtok(ReadBuffer,"\n");
	while(line != NULL) {		
		if (AddBreakpoint(line)) {
			char zbuf[128];
			strcpy(zbuf, FormatBreakpoint(nBreakPoints-1));
			SendDlgItemMessage(*myhwnd, IDC_COMBO1, CB_ADDSTRING, NULL, (LPARAM)zbuf);		   
		}
		line = strtok(NULL, "\n");
	}
	ReloadDumpFiles();
	return true;
}

int SaveBreakpoints(HWND *myhwnd) {
	if (nBreakPoints == 0) {
		MessageBox ( NULL, "No breakpoints to save!", "Save Breakpoints", MB_OK);
		return false;
	}

	OPENFILENAME ofn;                          // Structure for filename dialog
	char buf[256], buf2[256];
	char WriteBuffer[32768] = {0};

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize    = sizeof(OPENFILENAME);
	ofn.hwndOwner      = NULL;
	ofn.lpstrFilter    = "Breakpoint file\0*.brk\0\0"; 
	strcpy(buf, "");
	ofn.lpstrFile      = buf;
	ofn.nMaxFile       = 256;
	strcpy(buf2, "");
	ofn.lpstrFileTitle = buf2;
	ofn.nMaxFileTitle  = 256;
	ofn.lpstrDefExt    = "brk\0";
	ofn.Flags          = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn)) {
		// MessageBox( NULL, ofn.lpstrFile, "File Name", MB_OK);
		HANDLE fh;
		DWORD BytesWritten = 0;

		// Create File Handle
		fh = CreateFile(ofn.lpstrFile,         // file to open
						GENERIC_WRITE,         // open for writing
						FILE_SHARE_WRITE,      // share for writing
						NULL,                  // default security
						CREATE_ALWAYS,         // Overwrite if necessary
						FILE_ATTRIBUTE_NORMAL, // normal file
						NULL);        

		if (fh == INVALID_HANDLE_VALUE) {
			MessageBox ( NULL, "Could not create file handle", "Save Breakpoints", MB_OK);
			return false;
		}

		// Build breakpoint strings
		int n=SendDlgItemMessage(*myhwnd, IDC_COMBO1, CB_GETCOUNT, 0, 0);
		for (int l=0; l<n; l++) {
			char buf[32] = {0};
			SendDlgItemMessage(*myhwnd, IDC_COMBO1, CB_GETLBTEXT, (WPARAM)l, (LPARAM)buf);		
			// MessageBox(NULL, buf,"DEBUG",MB_OK);
			strcat(WriteBuffer,buf);			
			strcat(WriteBuffer,"\r\n");
		}
		
		WriteFile(fh,WriteBuffer,nBreakPoints*32,&BytesWritten,NULL);
		CloseHandle(fh);
	}
	return true;
}
