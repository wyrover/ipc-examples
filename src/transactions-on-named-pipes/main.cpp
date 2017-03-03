#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#define BUFSIZE 512

int _tmain(int argc, TCHAR *argv[])
{
    HANDLE hPipe;
    LPTSTR lpszWrite = TEXT("Default message from client");
    TCHAR chReadBuf[BUFSIZE];
    BOOL fSuccess;
    DWORD cbRead, dwMode;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

    if (argc > 1) {
        lpszWrite = argv[1];
    }

    // Try to open a named pipe; wait for it, if necessary.
    while (1) {
        hPipe = CreateFile(
                    lpszPipename,   // pipe name
                    GENERIC_READ |  // read and write access
                    GENERIC_WRITE,
                    0,              // no sharing
                    NULL,           // default security attributes
                    OPEN_EXISTING,  // opens existing pipe
                    0,              // default attributes
                    NULL);          // no template file

        // Break if the pipe handle is valid.
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs.
        if (GetLastError() != ERROR_PIPE_BUSY) {
            printf("Could not open pipe\n");
            return 0;
        }

        // All pipe instances are busy, so wait for 20 seconds.
        if (! WaitNamedPipe(lpszPipename, 20000)) {
            printf("Could not open pipe\n");
            return 0;
        }
    }

    // The pipe connected; change to message-read mode.
    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
                   hPipe,    // pipe handle
                   &dwMode,  // new pipe mode
                   NULL,     // don't set maximum bytes
                   NULL);    // don't set maximum time

    if (!fSuccess) {
        printf("SetNamedPipeHandleState failed.\n");
        return 0;
    }

    // Send a message to the pipe server and read the response.
    fSuccess = TransactNamedPipe(
                   hPipe,                  // pipe handle
                   lpszWrite,              // message to server
                   (lstrlen(lpszWrite) + 1) * sizeof(TCHAR), // message length
                   chReadBuf,              // buffer to receive reply
                   BUFSIZE * sizeof(TCHAR), // size of read buffer
                   &cbRead,                // bytes read
                   NULL);                  // not overlapped

    if (!fSuccess && (GetLastError() != ERROR_MORE_DATA)) {
        printf("TransactNamedPipe failed.\n");
        return 0;
    }

    while (1) {
        _tprintf(TEXT("%s\n"), chReadBuf);

        // Break if TransactNamedPipe or ReadFile is successful
        if (fSuccess)
            break;

        // Read from the pipe if there is more data in the message.
        fSuccess = ReadFile(
                       hPipe,      // pipe handle
                       chReadBuf,  // buffer to receive reply
                       BUFSIZE * sizeof(TCHAR), // size of buffer
                       &cbRead,  // number of bytes read
                       NULL);    // not overlapped

        // Exit if an error other than ERROR_MORE_DATA occurs.
        if (!fSuccess && (GetLastError() != ERROR_MORE_DATA))
            break;
        else _tprintf(TEXT("%s\n"), chReadBuf);
    }

    _getch();
    CloseHandle(hPipe);
    return 0;
}