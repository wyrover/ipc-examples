#include <SDKDDKVer.h>
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <time.h>
#include <errno.h>
#include <vector>

typedef long long uint64_t;

using namespace std;


inline size_t strindex(const char *s, int ch)
{
    const char *p = std::strchr(s, ch);
    return p ? p - s : -1;
}
inline size_t strindex(const wchar_t *s, int ch)
{
    const wchar_t *p = std::wcschr(s, ch);
    return p ? p - s : -1;
}
template <typename T>
void squeeze(T *str, const T *charset)
{
    T *q = str;

    for (T *p = str; *p; ++p)
        if (strindex(charset, *p) == -1)
            *q++ = *p;

    *q = 0;
}


std::wstring GetFullPathNameX(const std::wstring &path)
{
    DWORD length = GetFullPathNameW(path.c_str(), 0, 0, 0);
    std::vector<wchar_t> vec(length);
    length = GetFullPathNameW(path.c_str(), static_cast<DWORD>(vec.size()),
                              &vec[0], 0);
    return std::wstring(&vec[0], &vec[length]);
}

std::wstring prefixed_path(const wchar_t *path)
{
    std::wstring fullpath = GetFullPathNameX(path);
    return fullpath;
}


std::wstring format(const wchar_t *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int rc = _vscwprintf(fmt, args);
    va_end(args);
    std::vector<wchar_t> buffer(rc + 1);
    va_start(args, fmt);
    rc = _vsnwprintf(&buffer[0], buffer.size(), fmt, args);
    va_end(args);
    return std::wstring(&buffer[0], &buffer[rc]);
}


void print_error(const std::wstring &msg, DWORD code)
{
    LPWSTR pszMsg;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM,
                   0,
                   code,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&pszMsg,
                   0,
                   0);
    std::wstring ss;

    if (pszMsg) {
        squeeze(pszMsg, L"\r\n");
        ss = format(L"%s: %s", msg.c_str(), pszMsg);
        LocalFree(pszMsg);
    } else if (code < 0xfe00)
        ss = format(L"%d: %s", code, msg.c_str());
    else
        ss = format(L"%08x: %s", code, msg.c_str());

    //printf("%s", strutil::w2us(ss));
    //throw std::runtime_error(strutil::w2us(ss));
}


FILE *tmpfile(const wchar_t *prefix)
{
    FILE* fp = NULL;
    std::wstring sprefix =
        format(L"%s.%d.", prefix, GetCurrentProcessId());
    wchar_t *tmpname = _wtempnam(0, sprefix.c_str());
    HANDLE fh = CreateFileW(prefixed_path(tmpname).c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            0, 0, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_TEMPORARY |
                            FILE_FLAG_DELETE_ON_CLOSE,
                            0);

    if (fh == INVALID_HANDLE_VALUE) {
        goto Exit0;
    }

    int fd = _open_osfhandle(reinterpret_cast<intptr_t>(fh),
                             _O_BINARY | _O_RDWR);

    if (fd == -1) {
        CloseHandle(fh);
        //util::throw_crt_error("win32::tmpfile: open_osfhandle()");
        goto Exit0;
    }

    fp = _fdopen(fd, "w+");

    if (!fp) {
        _close(fd);
        //util::throw_crt_error("win32::tmpfile: _fdopen()");
        goto Exit0;
    }

Exit0:

    if (tmpname)
        free(tmpname);

    return fp;
}

char *load_with_mmap(const wchar_t *path, uint64_t *size)
{
    char *view = NULL;
    std::wstring fullpath = prefixed_path(path);
    HANDLE hFile = CreateFileW(fullpath.c_str(), GENERIC_READ,
                               0, 0, OPEN_EXISTING, 0, 0);

    if (hFile == INVALID_HANDLE_VALUE) {
        goto Exit0;
        //throw_error(fullpath, GetLastError());
    }

    DWORD sizeHi, sizeLo;
    sizeLo = GetFileSize(hFile, &sizeHi);
    *size = (static_cast<uint64_t>(sizeHi) << 32) | sizeLo;
    HANDLE hMap = CreateFileMappingW(hFile, 0, PAGE_READONLY, 0, 0, 0);
    DWORD err = GetLastError();
    CloseHandle(hFile);

    if (hMap <= 0) {
        goto Exit0;
        //throw_error("CreateFileMapping", err);
    }

    view =
        reinterpret_cast<char*>(MapViewOfFile(hMap, FILE_MAP_READ,
                                0, 0, 0));
    CloseHandle(hMap);
Exit0:
    return view;
}


int create_named_pipe(const wchar_t *path)
{
    int retval = -1;
    HANDLE fh = CreateNamedPipeW(path,
                                 PIPE_ACCESS_OUTBOUND,
                                 PIPE_TYPE_BYTE | PIPE_WAIT,
                                 1, 0, 0, 0, 0);

    if (fh == INVALID_HANDLE_VALUE)
        goto Exit0;

    ConnectNamedPipe(fh, 0);
    retval = _open_osfhandle(reinterpret_cast<intptr_t>(fh),
                             _O_WRONLY | _O_BINARY);
Exit0:
    return retval;
}

void OSFileToCRTFile()
{
    HANDLE hFile = CreateFile(L"e:\\test.dat", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    char szText [] = "Hello world!\n";
    DWORD dwWritten;
    WriteFile(hFile, szText, strlen(szText), & dwWritten, NULL);
    FILE * pFile = NULL;
    int nHandle = _open_osfhandle((long) hFile, _O_TEXT | _O_APPEND);

    if (nHandle != -1)
        pFile = _fdopen(nHandle, "wt");

    if (pFile) {
        int n = fputs("write by FILE *!", pFile);
        fflush(pFile);  // write to file immediately
    }

    CloseHandle(hFile);
}


int FileMapping(const wchar_t *name, size_t size)
{
    HANDLE handle;
    handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)(size >> 32), (DWORD)(size), name);
    int fd = _open_osfhandle((intptr_t)handle, 0);
    return fd;
}


int main()
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG | _CRTDBG_LEAK_CHECK_DF);
#endif
    //int fno = FileMapping(L"test", 1024);
    HANDLE handle;
    handle = CreateFileMapping(INVALID_HANDLE_VALUE,
                               NULL,
                               PAGE_READWRITE,
                               0,
                               1024 * 1024,
                               L"ShareMemoryTest");

    if (NULL == handle) {
        cout << "CreateFileMapping fail:" << GetLastError() << endl;
        return 0;
    }

    int fno = _open_osfhandle((intptr_t)handle, 0);

    if (fno == -1)
        return 0;

    FILE* stream = _fdopen(fno, "w");
    fprintf(stream, "%s", "hello world!");
    LPVOID lpShareMemory = MapViewOfFile(handle,
                                         FILE_MAP_ALL_ACCESS,
                                         0,
                                         0,      //memory start address
                                         0);     //all memory space

    if (NULL == lpShareMemory) {
        cout << "MapViewOfFile" << GetLastError() << endl;
    }

    std::cout << lpShareMemory << std::endl;
    ::_close(fno);
    _CrtDumpMemoryLeaks();
    return 0;
}

