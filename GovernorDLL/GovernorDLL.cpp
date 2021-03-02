// The Governor
// Oct 16, 2005 - Greg Hoglund
// www.rootkit.com

#include "stdafx.h"
#include "GovernorDLL.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <process.h>

HANDLE g_hPipe = 0;
CRITICAL_SECTION g_pipe_protector;

void PatchFunctions();

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&g_pipe_protector);
		g_hPipe = StartPipe("\\\\.\\pipe\\wow_hooker");
		SendText(g_hPipe, "GovernorDLL Loaded.");
		PatchFunctions();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		SendText(g_hPipe, "GovernorDLL Unloaded.");
		ShutdownPipe(g_hPipe);
		break;
	}
    return TRUE;
}

//
// Send text down the pipe
//
void
SendText (HANDLE hPipe, char *szText)
{
	if(!hPipe) return;

    char *c;
	DWORD dwWritten;
	DWORD len = strlen(szText);
	DWORD lenh = 4;

	EnterCriticalSection(&g_pipe_protector);

	// send length first
	c = (char *)&len;
	while(lenh)
	{
		//char _g[255];
		//_snprintf(_g, 252, "sending header %d", lenh);
		//OutputDebugString(_g);

		if (!WriteFile (hPipe, c, lenh, &dwWritten, NULL))
		{
			LeaveCriticalSection(&g_pipe_protector);
			ShutdownPipe(hPipe);
			return;
		}
		lenh -= dwWritten;
		c += dwWritten;
	}

	// then string
	c = szText;
	while(len)
	{
		//char _g[255];
		//_snprintf(_g, 252, "sending string %d", len);
		//OutputDebugString(_g);


		if (!WriteFile (hPipe, c, len, &dwWritten, NULL))
		{
			LeaveCriticalSection(&g_pipe_protector);
			ShutdownPipe(hPipe);
			return;
		}
		len -= dwWritten;
		c += dwWritten;
	}

	LeaveCriticalSection(&g_pipe_protector);
}

HANDLE StartPipe(char *szPipeName)
{
	HANDLE hPipe;
	TCHAR szBuffer[300];
	
	//
    // Open the output pipe
    //
    hPipe = CreateFile (szPipeName, GENERIC_WRITE, 0, NULL, 
                        OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
    if (hPipe == INVALID_HANDLE_VALUE)
    {
        _snprintf (szBuffer, sizeof (szBuffer),
                   "Failed to open output pipe(%s): %d\n",
                   szPipeName, GetLastError ());
		OutputDebugString(szBuffer);
		return NULL;
	}
	
	return hPipe;
}

void ShutdownPipe(HANDLE hPipe)
{
	//cleanup
	if (hPipe)
    {
        FlushFileBuffers (hPipe);
        CloseHandle (hPipe);
    }
	// make sure it stops being used
	g_hPipe = 0;
}
