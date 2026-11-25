#include "log.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>

std::wstring CoreDataDir()
{
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    std::wstring dir;
    if (n > 0 && n < MAX_PATH) {
        dir.assign(buf, n);
    } else {
        wchar_t tmp[MAX_PATH] = {0};
        DWORD t = GetTempPathW(MAX_PATH, tmp);
        if (t > 0 && t < MAX_PATH) dir.assign(tmp, t);
    }
    dir += L"\\IzakayaCheater";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir;
}

void CoreLog(const char* fmt, ...)
{
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);

    char text[2048] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text) - 1, fmt, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::wstring path = CoreDataDir() + L"\\core.log";
    FILE* f = _wfopen(path.c_str(), L"ab");
    if (!f) return;
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, text);
    fclose(f);

    char dbg[2200] = {0};
    _snprintf(dbg, sizeof(dbg) - 1, "[IzakayaCheatCore] %s\n", text);
    OutputDebugStringA(dbg);
}
