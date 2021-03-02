// The Governor
// Oct 16, 2005 - Greg Hoglund
// www.rootkit.com

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include "GovernorDLL.h"
#include "detours.h"
#include <psapi.h>

//////////////////////////////////////////////////////////////////////////
// These are functions used by the warden to spy on other processes
//////////////////////////////////////////////////////////////////////////


DETOUR_TRAMPOLINE(BOOL __stdcall Real_GetWindowTextA( HWND hWnd, LPSTR lpString, int nMaxCount), GetWindowTextA);
BOOL __stdcall Mine_GetWindowTextA( HWND hWnd, LPSTR lpString, int nMaxCount)
{
	int len = Real_GetWindowTextA( hWnd, lpString, nMaxCount);
	if( len != 0)
	{
		// WoW found some window text, let's report what it is...
		char _t[255];
		_snprintf(_t, 252, "GetWindowTextA called from WoW, returned %d bytes, %s ", len, lpString);
		SendText(g_hPipe, _t);
	}

	return len;
}

DETOUR_TRAMPOLINE(BOOL __stdcall Real_GetWindowTextW( HWND hWnd, LPWSTR lpString, int nMaxCount), GetWindowTextW);
BOOL __stdcall Mine_GetWindowTextW( HWND hWnd, LPWSTR lpString, int nMaxCount)
{
	int len = Real_GetWindowTextW( hWnd, lpString, nMaxCount);
	if( len != 0)
	{
		// WoW found some window text, let's report what it is...	
		
		char _t[255];
		_snprintf(_t, 252, "GetWindowTextW called from WoW, returned %d bytes", len);
		SendText(g_hPipe, _t);
	}

	return len;
}

DETOUR_TRAMPOLINE(int __stdcall Real_WSARecv( 
  SOCKET s,
  LPWSABUF lpBuffers,
  DWORD dwBufferCount,
  LPDWORD lpNumberOfBytesRecvd,
  LPDWORD lpFlags,
  LPWSAOVERLAPPED lpOverlapped,
  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine), WSARecv);

int __stdcall Mine_WSARecv( 
  SOCKET s,
  LPWSABUF lpBuffers,
  DWORD dwBufferCount,
  LPDWORD lpNumberOfBytesRecvd,
  LPDWORD lpFlags,
  LPWSAOVERLAPPED lpOverlapped,
  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	int res = Real_WSARecv(		s, 
								lpBuffers, 
								dwBufferCount,
								lpNumberOfBytesRecvd,
								lpFlags,
								lpOverlapped,
								lpCompletionRoutine );
	char _t[255];
	_snprintf(_t, 252, "WSARecv returned %d, %d bytes received", res, *lpNumberOfBytesRecvd);
	SendText(g_hPipe, _t);

	return res;
}

DETOUR_TRAMPOLINE(DWORD __stdcall Real_CharUpperBuffA( LPTSTR lpString, DWORD cchLength), CharUpperBuffA);
DWORD __stdcall Mine_CharUpperBuffA( LPTSTR lpString, DWORD cchLength)
{
	DWORD len = Real_CharUpperBuffA( lpString, cchLength );
	if( len != 0)
	{
		// WoW is processing some text, lets report it
		
		char _t[255];
		_snprintf(_t, 252, "CharUpperBuffA called from WoW, string %s", lpString);
		SendText(g_hPipe, _t);
	}

	return len;
}

DETOUR_TRAMPOLINE(HANDLE __stdcall Real_OpenProcess( DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId ), OpenProcess);
HANDLE __stdcall Mine_OpenProcess( DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId )
{
	HANDLE h = Real_OpenProcess( dwDesiredAccess, bInheritHandle, dwProcessId );
	if( h != 0)
	{
		// WoW is opening a process, lets report it
		
		char _t[255];
		_snprintf(_t, 252, "OpenProcess called from WoW, pid %d, returned handle 0x%08X", dwProcessId, (DWORD)h);
		SendText(g_hPipe, _t);
	}

	return h;
}

DETOUR_TRAMPOLINE(BOOL __stdcall Real_ReadProcessMemory( 
				HANDLE hProcess,
				LPCVOID lpBaseAddress,
				LPVOID lpBuffer,
				SIZE_T nSize,
				SIZE_T* lpNumberOfBytesRead ), ReadProcessMemory );

BOOL __stdcall Mine_ReadProcessMemory( HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesRead )
{
	BOOL ret = Real_ReadProcessMemory( hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead );
	if( ret && ((DWORD)hProcess != -1) )
	{
		// WoW is reading a process, lets report it
		char szProcessName[MAX_PATH] = "unknown";

		
		GetProcessImageFileName(hProcess, szProcessName, MAX_PATH);
		

		char _t[255];
		_snprintf(_t, 252, "ReadProcessMemory called from WoW, handle 0x%08X, process %s, read from address 0x%08X, %d bytes", 
			hProcess, 
			szProcessName,
			lpBaseAddress,
			nSize);
		SendText(g_hPipe, _t);
	}

	return ret;
}

void PatchFunctions()
{
	DetourFunctionWithTrampoline( (PBYTE)Real_GetWindowTextA, (PBYTE)Mine_GetWindowTextA);
	DetourFunctionWithTrampoline( (PBYTE)Real_GetWindowTextW, (PBYTE)Mine_GetWindowTextW);
	DetourFunctionWithTrampoline( (PBYTE)Real_CharUpperBuffA, (PBYTE)Mine_CharUpperBuffA);
	DetourFunctionWithTrampoline( (PBYTE)Real_OpenProcess, (PBYTE)Mine_OpenProcess);
	DetourFunctionWithTrampoline( (PBYTE)Real_ReadProcessMemory, (PBYTE)Mine_ReadProcessMemory);

	//DetourFunctionWithTrampoline( (PBYTE)Real_WSARecv, (PBYTE)Mine_WSARecv);
}

		