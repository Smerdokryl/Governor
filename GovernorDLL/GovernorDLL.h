// The Governor
// Oct 16, 2005 - Greg Hoglund
// www.rootkit.com

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the GOVERNORDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// GOVERNORDLL_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef GOVERNORDLL_EXPORTS
#define GOVERNORDLL_API __declspec(dllexport)
#else
#define GOVERNORDLL_API __declspec(dllimport)
#endif

extern HANDLE g_hPipe;
void SendText (HANDLE hPipe, char *szText);
HANDLE StartPipe(char *szPipeName);
void ShutdownPipe(HANDLE hPipe);

