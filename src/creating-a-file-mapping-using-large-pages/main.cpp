#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#define BUF_SIZE 65536

TCHAR szName[] = TEXT("LARGEPAGE");
typedef int (*GETLARGEPAGEMINIMUM)(void);

void DisplayError(TCHAR* pszAPI, DWORD dwError)
{
    LPVOID lpvMessageBuffer;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, dwError,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpvMessageBuffer, 0, NULL);
    //... now display this string
    _tprintf(TEXT("ERROR: API        = %s\n"), pszAPI);
    _tprintf(TEXT("       error code = %d\n"), dwError);
    _tprintf(TEXT("       message    = %s\n"), lpvMessageBuffer);
    // Free the buffer allocated by the system
    LocalFree(lpvMessageBuffer);
    ExitProcess(GetLastError());
}

void Privilege(TCHAR* pszPrivilege, BOOL bEnable)
{
    HANDLE           hToken;
    TOKEN_PRIVILEGES tp;
    BOOL             status;
    DWORD            error;

    // open process token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        DisplayError(TEXT("OpenProcessToken"), GetLastError());

    // get the luid
    if (!LookupPrivilegeValue(NULL, pszPrivilege, &tp.Privileges[0].Luid))
        DisplayError(TEXT("LookupPrivilegeValue"), GetLastError());

    tp.PrivilegeCount = 1;

    // enable or disable privilege
    if (bEnable)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // enable or disable privilege
    status = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
    // It is possible for AdjustTokenPrivileges to return TRUE and still not succeed.
    // So always check for the last error value.
    error = GetLastError();

    if (!status || (error != ERROR_SUCCESS))
        DisplayError(TEXT("AdjustTokenPrivileges"), GetLastError());

    // close the handle
    if (!CloseHandle(hToken))
        DisplayError(TEXT("CloseHandle"), GetLastError());
}

void _tmain(void)
{
    HANDLE hMapFile;
    LPCTSTR pBuf;
    DWORD size;
    GETLARGEPAGEMINIMUM pGetLargePageMinimum;
    HINSTANCE  hDll;
    // call succeeds only on Windows Server 2003 SP1 or later
    hDll = LoadLibrary(TEXT("kernel32.dll"));

    if (hDll == NULL)
        DisplayError(TEXT("LoadLibrary"), GetLastError());

    pGetLargePageMinimum = (GETLARGEPAGEMINIMUM)GetProcAddress(hDll,
                           "GetLargePageMinimum");

    if (pGetLargePageMinimum == NULL)
        DisplayError(TEXT("GetProcAddress"), GetLastError());

    size = (*pGetLargePageMinimum)();
    FreeLibrary(hDll);
    _tprintf(TEXT("Page Size: %u\n"), size);
    Privilege(TEXT("SeLockMemoryPrivilege"), TRUE);
    hMapFile = CreateFileMapping(
                   INVALID_HANDLE_VALUE,    // use paging file
                   NULL,                    // default security
                   PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES,
                   0,                       // max. object size
                   size,                    // buffer size
                   szName);                 // name of mapping object

    if (hMapFile == NULL)
        DisplayError(TEXT("CreateFileMapping"), GetLastError());
    else
        _tprintf(TEXT("File mapping object successfulyl created.\n"));

    Privilege(TEXT("SeLockMemoryPrivilege"), FALSE);
    pBuf = (LPTSTR) MapViewOfFile(hMapFile,   // handle to map object
                                  FILE_MAP_ALL_ACCESS, // read/write permission
                                  0,
                                  0,
                                  BUF_SIZE);

    if (pBuf == NULL)
        DisplayError(TEXT("MapViewOfFile"), GetLastError());
    else
        _tprintf(TEXT("View of file successfully mapped.\n"));

    // do nothing, clean up an exit
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
}
