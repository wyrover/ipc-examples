#include <SDKDDKVer.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <crtdbg.h>

#define BUF_SIZE 256
TCHAR szName[] = TEXT("Global\\MyFileMappingObject");

int main()
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG | _CRTDBG_LEAK_CHECK_DF);
#endif
    HANDLE hMapFile;
    LPCTSTR pBuf;
    hMapFile = OpenFileMapping(
                   FILE_MAP_ALL_ACCESS,   // read/write access
                   FALSE,                 // do not inherit the name
                   szName);               // name of mapping object

    if (hMapFile == NULL) {
        _tprintf(TEXT("Could not open file mapping object (%d).\n"),
                 GetLastError());
        return 1;
    }

    pBuf = (LPTSTR) MapViewOfFile(hMapFile, // handle to map object
                                  FILE_MAP_ALL_ACCESS,  // read/write permission
                                  0,
                                  0,
                                  BUF_SIZE);

    if (pBuf == NULL) {
        _tprintf(TEXT("Could not map view of file (%d).\n"),
                 GetLastError());
        CloseHandle(hMapFile);
        return 1;
    }

    MessageBox(NULL, pBuf, TEXT("Process2"), MB_OK);
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    _CrtDumpMemoryLeaks();
    system("pause");
    return 0;
}

