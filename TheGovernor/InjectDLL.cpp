 
/***************************************************************************
 * InjectDLL
 *
 * purpose: DLL Injection
 *
 * Some parts of this code shamelessly ripped from 'net
 *
 * (c) 2003-2005 HBGary, Inc.
 *
 * This program may be used in any form or for any purpose as long as the
 * above copyright is included.
 *
 * Oct 5, 2003 - initial version, Greg Hoglund
 * Oct 16, 2005 - repurposed for TheGovernor, Greg Hoglund
 ***************************************************************************/


#include "stdafx.h"
#include <windows.h>
#include <winnt.h>
#include <stdio.h>
#include <stdlib.h>
#include <psapi.h>
#include "InjectDLL.h"

#define DUMP_PIPE_SIZE 1024
#define DLL_NAME "GovernorDLL.DLL"

void ReceiveOutput(HANDLE hEvent);
BOOL InjectDll(		HANDLE hProcess, 
					HANDLE hThread, 
					VOID* hModuleBase, 
					char *DllName);

VOID*		gRemoteSectionPtr = NULL;
CONTEXT		gRemoteThreadOriginalContext;
char		gOriginalCodePage[4096];
DWORD		gSizeOfRemoteCodePage=0;

bool g_codepage_breakpoint_has_fired = FALSE;
bool g_original_breakpoint_has_fired = FALSE;

////////////////////////////////////////////////////
// These function pointers are used in the
// debugger code.
////////////////////////////////////////////////////
typedef 
void * 
(__stdcall *FOPENTHREAD)
(
  DWORD dwDesiredAccess,  // access right
  BOOL bInheritHandle,    // handle inheritance option
  DWORD dwThreadId        // thread identifier
);
FOPENTHREAD fOpenThread;

typedef
BOOL(__stdcall *DEBUGSETPROCESSKILLONEXIT) 
(
  BOOL KillOnExit
);
DEBUGSETPROCESSKILLONEXIT fDebugSetProcessKillOnExit;

DWORD RestoreOriginalCodePage( HANDLE hProcess, HANDLE hThread, DWORD *outSize )
{
	BOOL res;
	DWORD written;
	CONTEXT Context;

	if(outSize) *outSize = gSizeOfRemoteCodePage;

	Context.ContextFlags = CONTEXT_FULL;
	GetThreadContext( hThread, &Context);

	logprintf("Restoring original code to remote section ptr 0x%08X, len %d\n", 
		(DWORD)gRemoteSectionPtr,
		gSizeOfRemoteCodePage);

	res = WriteProcessMemory(	hProcess, 
								gRemoteSectionPtr, 
								gOriginalCodePage, 
								gSizeOfRemoteCodePage, 
								&written );

	if(!res)
	{
		logprintf("WriteProcessMemory error %08X\n", GetLastError());
		return -1;
	}

	if(written!=gSizeOfRemoteCodePage)
	{
		return written+1;
	}

	res=SetThreadContext(	hThread, 
							(CONST CONTEXT*)&gRemoteThreadOriginalContext);
	if(!res)
	{
		logprintf("SetThreadContext error %08X\n", GetLastError());
		return -1;
	}

	return 0;
}

DWORD InjectDLLByPID(DWORD dwPid)
{
	HANDLE hTargetProc;
    HANDLE hReceiveThread;
    HANDLE hEvent;
    DWORD dwIgnored;
    DWORD dwErr;
	DEBUG_EVENT	dbg_evt;
	VOID * BaseOfImage;
	HANDLE hThread;
	BOOL bDoneDebugging = FALSE;
	CHAR szDllName[MAX_PATH];

	//////////////////////////////////////////////
	// Load the function pointers
	// fDebugSetProcessKillOnExit only works on
	// XP and above.
	//////////////////////////////////////////////
	fDebugSetProcessKillOnExit = (DEBUGSETPROCESSKILLONEXIT) GetProcAddress( GetModuleHandle("kernel32.dll"), "DebugSetProcessKillOnExit" );
	if(!fDebugSetProcessKillOnExit)
	{
		logprintf("[!] failed to get fDebugSetProcessKillOnExit function!\n");
	}

	fOpenThread = (FOPENTHREAD) GetProcAddress( GetModuleHandle("kernel32.dll"), "OpenThread" );
	if(!fOpenThread)
	{
		logprintf("[!] failed to get openthread function!\n");
		return 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	// debug privs are required for our operations
	///////////////////////////////////////////////////////////////////////////////
    //if (EnableDebugPriv () != 0)
    //{
    //  logprintf ("Failed enabling Debug privilege.  Proceeding anyway\n");
    //}

    hTargetProc = OpenProcess (PROCESS_ALL_ACCESS, FALSE, dwPid);
    if (hTargetProc == 0)
    {
        dwErr = GetLastError ();
        logprintf("Failed to open target process: %d.  Exiting.\n", dwErr);
        return(0);
    }

    ///////////////////////////////////////////////////////
    // Prepare to communicate over the pipe
    ///////////////////////////////////////////////////////
    hEvent = CreateEvent (NULL, 0, 0, NULL);
    hReceiveThread = CreateThread (NULL, 0,
                                   (LPTHREAD_START_ROUTINE) ReceiveOutput,
                                   hEvent, 0, &dwIgnored);
    if (!hReceiveThread)
    {
        dwErr = GetLastError ();
        logprintf("Failed to create receiving thread: %d.  Exiting\n", dwErr);
        return(0);
    }

    ///////////////////////////////////////////////////////////
    // Wait for the child thread to create the pipe
    ///////////////////////////////////////////////////////////
    if (WaitForSingleObject (hEvent, 10000) != ERROR_SUCCESS)
    {
        dwErr = GetLastError ();
        logprintf("Failed starting listen on pipe: %d.  Exiting\n", dwErr);
        return(0);
    }

	///////////////////////////////////////////////////////////
	// Allright, -- time to start debugging
	///////////////////////////////////////////////////////////
	if(!DebugActiveProcess(dwPid))
	{
		logprintf("[!] DebugActiveProcess failed !\n");
		return 0;
	}

	// don't kill the process on thread exit, XP and above
	if(fDebugSetProcessKillOnExit) fDebugSetProcessKillOnExit(FALSE);

	///////////////////////////////////////////////////////
	// init path to injectable DLL
	///////////////////////////////////////////////////////
	GetModuleFileName (NULL, szDllName, sizeof (szDllName));
    strcpy (strrchr (szDllName, '\\') + 1, DLL_NAME);
	logprintf("Injection DLL: %s \n", szDllName);

	///////////////////////////////////////////////////////
	// main debugging loop
	///////////////////////////////////////////////////////
	while(1)
	{
		if(WaitForDebugEvent(&dbg_evt, 500))
		{
			if(dbg_evt.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT)
			{
				BaseOfImage = dbg_evt.u.CreateProcessInfo.lpBaseOfImage;
			}

			if(dbg_evt.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
			{
				//logprintf("got debug event\n");

				if(EXCEPTION_BREAKPOINT == dbg_evt.u.Exception.ExceptionRecord.ExceptionCode) 
				{
					if(g_original_breakpoint_has_fired == FALSE)
					{
						////////////////////////////////////////////////
						// the first breakpoint fires in it's own
						// thread, created by the debugger
						////////////////////////////////////////////////
						g_original_breakpoint_has_fired = TRUE;

						hThread = fOpenThread(
										THREAD_ALL_ACCESS, 
										FALSE, 
										dbg_evt.dwThreadId
										);

						/////////////////////////////////////////////////
						// Inject code into the remote process and use
						// the current thread to execute it.  The code
						// will force-load a DLL of our choice.
						/////////////////////////////////////////////////
						if(!InjectDll(	hTargetProc, 
										hThread, 
										BaseOfImage, 
										szDllName ))
						{
							return 0;
						}
					}
					else if( g_codepage_breakpoint_has_fired == FALSE )
					{
						//////////////////////////////////////////////////
						// the breakpoint in our injected code has fired
						// this means the 'injected' DLL has been loaded
						// and it's now time to fix the remote exe
						// back to normal and let the DLL do the rest
						// of the work.
						//////////////////////////////////////////////////
						CONTEXT ctx;

						ctx.ContextFlags = CONTEXT_FULL;
						if(!GetThreadContext(hThread, &ctx))
						{
							logprintf("[!] GetThreadContext failed ! \n");
							return 0;
						}

						logprintf("Hit codepage breakpoint\n");
						logprintf("EAX: 0x%08X\n", ctx.Eax);
						logprintf("EIP: 0x%08X\n", ctx.Eip);
						
						g_codepage_breakpoint_has_fired = TRUE;
						RestoreOriginalCodePage( hTargetProc, hThread, 0);
						
						// we are done using the debugging loop
						bDoneDebugging = TRUE;
					}
				}
			}
		
			if(!ContinueDebugEvent(dwPid, dbg_evt.dwThreadId, DBG_CONTINUE))
			{
				return 0;
			}
		}
		else
		{
			if(bDoneDebugging) 
			{
				if(!DebugActiveProcessStop(dwPid))
				{
					logprintf("Error, could not detach from process\n");
				}
				break;
			}
		}
	}

	////////////////////////////////////////////////////////////////
	// DLL is successfully injected, now wait for our child thread
    // to finish processing on the pipe
	////////////////////////////////////////////////////////////////
    //logprintf("waiting for named pipe processing...\n");
	//WaitForSingleObject (hReceiveThread, INFINITE);

    return 1;
}

///////////////////////////////////////////////////////////
// Try to enable the debug privilege
///////////////////////////////////////////////////////////
int EnableDebugPriv (void)
{
    HANDLE hToken = 0;
    DWORD dwErr = 0;
    TOKEN_PRIVILEGES newPrivs;

    if (!OpenProcessToken (GetCurrentProcess (),
                           TOKEN_ADJUST_PRIVILEGES,
                           &hToken))
    {
        dwErr = GetLastError ();
        logprintf("Unable to open process token: %d\n", dwErr);
        goto exit;
    }

    if (!LookupPrivilegeValue (NULL, SE_DEBUG_NAME,
                               &newPrivs.Privileges[0].Luid))
    {
        dwErr = GetLastError ();
        logprintf("Unable to lookup privilege: %d\n", dwErr);
        goto exit;
    }

    newPrivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    newPrivs.PrivilegeCount = 1;
    
    if (!AdjustTokenPrivileges (hToken, FALSE, &newPrivs, 0, NULL, NULL))
    {
        dwErr = GetLastError ();
        logprintf("Unable to adjust token privileges: %d\n", dwErr);
        goto exit;
    }

 exit:
    if (hToken)
        CloseHandle (hToken);

    return dwErr;
}

//////////////////////////////////////////////////////////////////////////////
// Section scanning and injection code (adapted from CrankHank)
//////////////////////////////////////////////////////////////////////////////
BOOL InjectDll(		HANDLE hProcess, 
					HANDLE hThread, 
					VOID* hModuleBase, 
					char *DllName)
{
	IMAGE_DOS_HEADER		DOShdr;
	IMAGE_NT_HEADERS		*pNThdr, NThdr;
	IMAGE_SECTION_HEADER	SecHdr, *pSecHdr;
	IMAGE_DATA_DIRECTORY	DataDir, *pDataDir; //@@@@@@@@
	DWORD dwD, dwD2, read, written;
	CONTEXT Context;
	BOOL res;
	char *DLLName;
	DWORD *EAX, *EBX;

	//////////////////////////////////////////////////
	// injected instructions, the addresses are
	// stamped into place below.
	//////////////////////////////////////////////////
	char InjectedCodePage[4096] = 
	
	{	0xB8, 0x00, 0x00, 0x00, 0x00,	// mov EAX,  0h | Pointer to LoadLibraryA() (DWORD)
		0xBB, 0x00, 0x00, 0x00, 0x00,	// mov EBX,  0h | DLLName to inject (DWORD)
		0x53,					// push EBX
		0xFF, 0xD0,				// call EAX
		0x5b,					// pop EBX
		0xcc					// INT 3h
	};
	int length_of_injection_code=15;
	
	////////////////////////////////////////////////////////////
	// We assume that these functions are located at the same
	// address both here and in the remote process.
	////////////////////////////////////////////////////////////
	FARPROC LoadLibProc = GetProcAddress(GetModuleHandle("KERNEL32.dll"), "LoadLibraryA");
	FARPROC LastErrProc = GetProcAddress(GetModuleHandle("KERNEL32.dll"), "GetLastError");
	if(!LoadLibProc || !LastErrProc) return FALSE;
	
	///////////////////////////////////////////////////////////
	// pointers to be used for 'stamping' values into the
	// injection code (see below)
	///////////////////////////////////////////////////////////
	DLLName = (char*)((DWORD)InjectedCodePage + length_of_injection_code);
	EAX = (DWORD*)( InjectedCodePage +  1);
	EBX = (DWORD*) ( InjectedCodePage +  6);

	gSizeOfRemoteCodePage = strlen(DllName) + length_of_injection_code + 1;

	////////////////////////////////////////////////////////////
	// we are going to 'borrow' this thread, so save the
	// original thread context so we can put it back when we
	// are done.
	////////////////////////////////////////////////////////////
	Context.ContextFlags = CONTEXT_FULL;//CONTROL;
	gRemoteThreadOriginalContext.ContextFlags = CONTEXT_FULL;//CONTROL;
	if(!GetThreadContext( hThread, &gRemoteThreadOriginalContext))
	{
		dwD = GetLastError();
		logprintf("[!]Could not get thread context, error %08X\n", dwD);
		return FALSE;
	}
	
	////////////////////////////////////////////////////////////
	// Now we do all that ugly pointer arithmetic scanning the
	// PE header of the loaded module.
	//
	// Check to see if we have valid Headers:
	// Get DOS hdr...
	////////////////////////////////////////////////////////////
	res = ReadProcessMemory(hProcess, hModuleBase, &DOShdr, sizeof(DOShdr), &read);
	if( (!res) || (read!=sizeof(DOShdr)) ) 
	{
		logprintf("Could not get DOS header\n");
		return FALSE;
	}

	if( DOShdr.e_magic != IMAGE_DOS_SIGNATURE ) //Check for `MZ
	{
		logprintf("Could not find MZ for DOS header\n");
		return FALSE;
	}

	//Get NT header
	res = ReadProcessMemory(	hProcess, 
								(VOID*)((DWORD)hModuleBase + 
								(DWORD)DOShdr.e_lfanew), 
								&NThdr, 
								sizeof(NThdr), 
								&read);
	
	if( (!res) || (read!=sizeof(NThdr)) )
	{
		logprintf("Could not get NT header\n");
		return FALSE;
	}

	if( NThdr.Signature != IMAGE_NT_SIGNATURE ) //Check for `PE\0\0
	{
		logprintf("Could not find PE in NT Header\n");
		return 0;
	}

	//////////////////////////////////////////////////////////////////
	// We are here if we have a valid EXE header!
	// Look for a usable writable code page:
	//////////////////////////////////////////////////////////////////

	if( (dwD=NThdr.FileHeader.NumberOfSections) < 1 ) 
	{
		logprintf("No sections to scan!\n");
		return FALSE;//Section table: (after optional header)
	}

	// nasty ptr arithmetic (yucky PE)
	pSecHdr = (IMAGE_SECTION_HEADER*)
		( 
		((DWORD)hModuleBase + (DWORD)DOShdr.e_lfanew) + 
		(DWORD)sizeof(NThdr.FileHeader) + 
		(DWORD)NThdr.FileHeader.SizeOfOptionalHeader + 4
		);

	res=FALSE;
	dwD2 = (DWORD)GetModuleHandle(0);
	
	for( dwD2=0 ; dwD2<dwD ; dwD2++ )
	{
		//iterate sections to look for a writable part of memory and NOT .idata
		if( !ReadProcessMemory( hProcess, pSecHdr, &SecHdr, sizeof(SecHdr), &read) )
		{
			logprintf("ReadProcessMemory failed, error %08X\n", GetLastError());
			return FALSE;
		}
		
		if(read != sizeof(SecHdr)) 
		{
			logprintf("section size mismatch!\n");
			return FALSE;
		}
		
		logprintf("looking at target section, %s\n", SecHdr.Name);
		
		if( 
			(SecHdr.Characteristics & IMAGE_SCN_MEM_WRITE) //writable section
			&&
			( strcmpi((const char*)SecHdr.Name, ".idata")!=NULL ) //not .idata (import data)
			)
		{
			logprintf("FOUND useable code page: %s\n", SecHdr.Name );

			res = TRUE;
			break;//OK!!
		}

		pSecHdr++;
	}

	if(!res)
	{
		logprintf("couldn't find usable code page!\n");
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////
	// we are here if we found a section to write code into
	//////////////////////////////////////////////////////////////////////
	gRemoteSectionPtr = (VOID*)(SecHdr.VirtualAddress + (DWORD)hModuleBase);
	logprintf("Using section pointer 0x%08X\n", gRemoteSectionPtr);

	///////////////////////////////////////////////////////////
	// stamp the correct values into our injectable code
	///////////////////////////////////////////////////////////
	strcpy( DLLName, DllName );
	*EAX = (DWORD)LoadLibProc;
	*EBX = length_of_injection_code + (DWORD)gRemoteSectionPtr;

	///////////////////////////////////////////////////////////
	// save the original code before we overwrite it
	///////////////////////////////////////////////////////////
	if(!ReadProcessMemory( 
			hProcess, 
			gRemoteSectionPtr,
			gOriginalCodePage, 
			gSizeOfRemoteCodePage, 
			&read) )
	{
		logprintf("ReadProcessMemory failed, error %08X\n", GetLastError());
		return FALSE;
	}

	if(read != gSizeOfRemoteCodePage)
	{
		logprintf("Could not write the correct number of bytes\n");
		return FALSE;
	}

	logprintf("writing new code to remote address 0x%08X\n", gRemoteSectionPtr);

	/////////////////////////////////////////////////////////////
	// Now write our injectable code into the remote section.
	/////////////////////////////////////////////////////////////
	res = WriteProcessMemory( 
		hProcess, 
		gRemoteSectionPtr,
		InjectedCodePage, 
		gSizeOfRemoteCodePage, 
		&written);
	
	if( (written!=0) && (written!=gSizeOfRemoteCodePage) )
	{
		logprintf("Error writing injection code.  Remote process MAY CRASH\n");

		// try to save face and put the old code back...
		WriteProcessMemory( hProcess, gRemoteSectionPtr, gOriginalCodePage, gSizeOfRemoteCodePage, &written);			
		return FALSE; 
	}

	if((!res) || (written!=gSizeOfRemoteCodePage))
	{
		logprintf("Error injecting code\n");
		return FALSE;
	}

	////////////////////////////////////////////////////////
	// Ok, injected successfully, 
	// You MUST call function RestoreOriginalCodePage() 
	// function upon the following breakpoint!
	////////////////////////////////////////////////////////
	logprintf("setting thread EIP to 0x%08X\n", gRemoteSectionPtr);

	Context = gRemoteThreadOriginalContext;
	Context.Eip = (DWORD)gRemoteSectionPtr;
	res = SetThreadContext(hThread, &Context);
	if(!res) 
	{
		logprintf("Error, could not set remote thread EIP, process MAY CRASH\n");
		return FALSE;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// Pipe read/write method to remote process stolen from Todd Sabin, pwdump
//////////////////////////////////////////////////////////////////////////////
void ReceiveOutput (HANDLE hEvent)
{
    HANDLE hPipe;
    CHAR szPipeName[MAX_PATH];

	//logprintf("--[ creating named pipe ]--\n");
    /////////////////////////////////////////////////////
    // Create the pipe
    // TODO, allow for multiple pipe names
	/////////////////////////////////////////////////////
    _snprintf (szPipeName, sizeof (szPipeName),
               "\\\\.\\pipe\\wow_hooker" );
    hPipe = CreateNamedPipe (szPipeName,
                             PIPE_ACCESS_INBOUND
                             | FILE_FLAG_WRITE_THROUGH,
                             PIPE_TYPE_BYTE | PIPE_WAIT,
                             1, DUMP_PIPE_SIZE, DUMP_PIPE_SIZE,
                             10000, NULL);
    if (!hPipe)
    {
        //logprintf ( "Failed to create the pipe: %d\n",
        //         GetLastError ());
        return;
    }

    //
    // Tell the main thread we're ready to go
    //
    SetEvent (hEvent);

    if (ConnectNamedPipe (hPipe, NULL)
        || (GetLastError () == ERROR_PIPE_CONNECTED))
    {
        BYTE buff[DUMP_PIPE_SIZE+1];
        DWORD dwRead;
        DWORD dwErr = ERROR_SUCCESS;

        do
        {
			char *c;
			DWORD header_len;
			DWORD lenh = 4;
			
			// read header first
			c = (char *)&header_len;
			while(lenh)
			{
				char _g[255];
				_snprintf(_g, 252, "reading header %d", lenh);
				OutputDebugString(_g);

				if(!ReadFile (hPipe, c, lenh, &dwRead, NULL))
				{
					//error!
					dwErr = GetLastError ();
					break;
				}
				lenh -= dwRead;
				c += dwRead;
			}

			// now read the rest
			if(header_len < DUMP_PIPE_SIZE)
			{
				DWORD len = header_len;
				c = (char *) &buff[0];
				while(len)
				{
					char _g[255];
					_snprintf(_g, 252, "reading string %d", len);
					OutputDebugString(_g);

					if(!ReadFile (hPipe, c, len, &dwRead, NULL))
					{
						//error!
						dwErr = GetLastError ();
						break;
					}
					len -= dwRead;
					c += dwRead;
				}
				buff[header_len] = '\0';
				logprintf ("%s", buff);
			}
			else
			{
				logprintf("overly long message size %d", header_len);
				break;
			}
        } while (dwErr != ERROR_BROKEN_PIPE);
    }
    else
    {
        logprintf( "Failed to connect the pipe: %d\n",
                 GetLastError ());
        CloseHandle (hPipe);
        return;
    }
	logprintf("--[ disconnecting named pipe ]--\n");
    DisconnectNamedPipe (hPipe);
    CloseHandle (hPipe);
    return;
}

#define MAX_PROCS 1024
DWORD GetPIDForProcess(char *processName)
{
	DWORD aPidArray[MAX_PROCS];
	DWORD cbNeeded;
	DWORD procCount = 0;

	if(TRUE == EnumProcesses(aPidArray, (MAX_PROCS * sizeof(DWORD)), &cbNeeded))
	{
		procCount = cbNeeded/sizeof(DWORD);
		for(DWORD i=0;i<procCount;i++)
		{
			char szProcessName[MAX_PATH] = "";
			DWORD Pid = aPidArray[i];

			HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
										PROCESS_VM_READ,
										FALSE, Pid);
			if (NULL != hProcess )
			{
				HMODULE hMod;
				DWORD cbNeeded;

				if (EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
					&cbNeeded) )
				{
					GetModuleBaseName(	hProcess, 
										hMod, 
										szProcessName, 
										sizeof(szProcessName) );

					strupr(szProcessName);
					if(strcmp(szProcessName, processName) == 0)
					{
						CloseHandle(hProcess);
						return Pid;
					}
				}
				else 
				{
					CloseHandle(hProcess);
				}
			}
		}
	}
	return 0;
}