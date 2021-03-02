// The Governor
// Oct 16, 2005 - Greg Hoglund
// www.rootkit.com

#pragma once

DWORD GetPIDForProcess(char *processName);
DWORD InjectDLLByPID(DWORD dwPid);
int EnableDebugPriv(void);