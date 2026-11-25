#include "trainer_client.h"
#include "trainer_injector.h"
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <mutex>

namespace
{
HANDLE g_pipe = INVALID_HANDLE_VALUE;
DWORD g_pid = 0;
std::mutex g_lock;

std::wstring ExeDir()
{
    wchar_t path[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (n == 0) return L".";
    std::wstring p(path, n);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring(L".") : p.substr(0, pos);
}

void ClosePipeLocked()
{
    if (g_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
    g_pid = 0;
}

HANDLE ConnectPipe(DWORD pid, DWORD timeoutMs)
{
    std::wstring name = L"\\\\.\\pipe\\IzakayaCheaterCore." + std::to_wstring(pid);
    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                  OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE) {
            DWORD mode = PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
            return pipe;
        }
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_BUSY) WaitNamedPipeW(name.c_str(), 100);
        if (GetTickCount64() >= deadline) return INVALID_HANDLE_VALUE;
        Sleep(100);
    }
}

bool WriteLine(HANDLE pipe, const std::string& command)
{
    std::string data = command + "\n";
    DWORD written = 0;
    return WriteFile(pipe, data.data(), (DWORD)data.size(), &written, NULL) != FALSE;
}

bool ReadLine(HANDLE pipe, std::string& reply, DWORD timeoutMs)
{
    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) return false;
        if (available > 0) {
            char buf[4096] = {0};
            DWORD read = 0;
            if (!ReadFile(pipe, buf, sizeof(buf) - 1, &read, NULL)) return false;
            reply.assign(buf, read);
            while (!reply.empty() && (reply[reply.size() - 1] == '\n' || reply[reply.size() - 1] == '\r')) {
                reply.erase(reply.size() - 1);
            }
            return true;
        }
        if (GetTickCount64() >= deadline) return false;
        Sleep(5);
    }
}

void ParseFeatureStates(const std::string& reply, std::string* out)
{
    out->clear();
    size_t pos = reply.find(" states=");
    if (pos == std::string::npos) return;
    pos += 8;
    size_t end = reply.find(" values=", pos);
    if (end == std::string::npos) end = reply.size();
    *out = reply.substr(pos, end - pos);
    while (!out->empty() && out->back() == ' ') out->erase(out->size() - 1);
}

void ParseFeatureValues(const std::string& reply, std::string* out)
{
    out->clear();
    size_t pos = reply.find(" values=");
    if (pos == std::string::npos) return;
    pos += 8;
    size_t end = reply.find('\n', pos);
    if (end == std::string::npos) end = reply.size();
    *out = reply.substr(pos, end - pos);
    while (!out->empty() && (out->back() == ' ' || out->back() == '\r')) out->erase(out->size() - 1);
}

bool CommandLocked(const std::string& command, std::string* reply, std::string* error)
{
    if (g_pipe == INVALID_HANDLE_VALUE) {
        if (error) *error = "未附加游戏";
        return false;
    }
    if (!WriteLine(g_pipe, command)) {
        if (error) *error = "写入管道失败 错误码 " + std::to_string(GetLastError());
        ClosePipeLocked();
        return false;
    }
    std::string line;
    if (!ReadLine(g_pipe, line, 60000)) {
        if (error) *error = "读取管道超时";
        ClosePipeLocked();
        return false;
    }
    if (reply) *reply = line;
    return true;
}

bool AttachLocked(std::string* error)
{
    if (g_pipe != INVALID_HANDLE_VALUE) {
        std::string reply;
        if (!CommandLocked("INIT", &reply, error)) return false;
        if (reply.compare(0, 2, "OK") != 0) {
            if (error) *error = reply.compare(0, 3, "ERR") == 0 ? reply.substr(4) : reply;
            return false;
        }
        return true;
    }

    DWORD pid = 0;
    if (!Injector_FindGame(&pid)) {
        if (error) *error = "未找到游戏进程";
        return false;
    }

    HANDLE pipe = ConnectPipe(pid, 300);
    if (pipe == INVALID_HANDLE_VALUE) {
        std::wstring dll = ExeDir() + L"\\IzakayaCheatCore.dll";
        if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
            if (error) *error = "找不到 IzakayaCheatCore.dll 请重新构建或重新安装";
            return false;
        }
        if (!Injector_Inject(pid, dll, error)) return false;
        pipe = ConnectPipe(pid, 5000);
        if (pipe == INVALID_HANDLE_VALUE) {
            if (error) *error = "注入后无法连接管道";
            return false;
        }
    }

    g_pipe = pipe;
    g_pid = pid;

    std::string reply;
    if (!CommandLocked("INIT", &reply, error)) return false;
    if (reply.compare(0, 2, "OK") != 0) {
        if (error) *error = reply.compare(0, 3, "ERR") == 0 ? reply.substr(4) : reply;
        return false;
    }
    return true;
}
}

bool TrainerClient_Attach(std::string* error)
{
    std::lock_guard<std::mutex> guard(g_lock);
    return AttachLocked(error);
}

void TrainerClient_Detach()
{
    std::lock_guard<std::mutex> guard(g_lock);
    ClosePipeLocked();
}

bool TrainerClient_Command(const std::string& command, std::string* reply, std::string* error)
{
    std::lock_guard<std::mutex> guard(g_lock);
    return CommandLocked(command, reply, error);
}

bool TrainerClient_Status(TrainerStatus* out)
{
    if (!out) return false;
    out->featureStates.clear();
    out->featureValues.clear();
    std::lock_guard<std::mutex> guard(g_lock);

    DWORD pid = 0;
    bool running = Injector_FindGame(&pid);
    out->gameRunning = running;
    if (!running) {
        ClosePipeLocked();
        out->attached = false;
        out->resolved = false;
        out->moneyReady = false;
        out->speedReady = false;
        out->frozen = false;
        out->money = 0;
        out->timeScale = 1.0f;
        out->error.clear();
        return true;
    }

    if (g_pipe == INVALID_HANDLE_VALUE) {
        out->attached = false;
        out->resolved = false;
        out->moneyReady = false;
        out->speedReady = false;
        out->frozen = false;
        out->money = 0;
        out->timeScale = 1.0f;
        out->error.clear();
        return true;
    }

    std::string reply;
    std::string error;
    if (!CommandLocked("STATUS", &reply, &error)) {
        ClosePipeLocked();
        out->attached = false;
        out->resolved = false;
        out->moneyReady = false;
        out->speedReady = false;
        out->frozen = false;
        out->money = 0;
        out->timeScale = 1.0f;
        out->error = error;
        return true;
    }

    out->attached = true;
    if (reply.compare(0, 3, "ERR") == 0) {
        out->resolved = false;
        out->moneyReady = false;
        out->speedReady = false;
        out->frozen = false;
        out->error = reply.size() > 4 ? reply.substr(4) : reply;
        return true;
    }

    int game = 0;
    int resolved = 0;
    int moneyReady = 0;
    int speedReady = 0;
    int frozen = 0;
    long long money = 0;
    double speed = 1.0;
    int fields = sscanf(reply.c_str(),
                        "OK game=%d resolved=%d moneyReady=%d speedReady=%d frozen=%d money=%lld timeScale=%lf",
                        &game, &resolved, &moneyReady, &speedReady, &frozen, &money, &speed);
    if (fields == 7) {
        out->resolved = resolved != 0;
        out->moneyReady = moneyReady != 0;
        out->speedReady = speedReady != 0;
        out->frozen = frozen != 0;
        out->money = money;
        out->timeScale = (float)speed;
        out->error.clear();
    } else if (sscanf(reply.c_str(),
                      "OK game=%d resolved=%d moneyReady=%d speedReady=%d money=%lld timeScale=%lf",
                      &game, &resolved, &moneyReady, &speedReady, &money, &speed) == 6) {
        out->resolved = resolved != 0;
        out->moneyReady = moneyReady != 0;
        out->speedReady = speedReady != 0;
        out->frozen = false;
        out->money = money;
        out->timeScale = (float)speed;
        out->error.clear();
    } else if (sscanf(reply.c_str(), "OK game=%d resolved=%d money=%lld timeScale=%lf",
                      &game, &resolved, &money, &speed) == 4) {
        out->resolved = resolved != 0;
        out->moneyReady = resolved != 0;
        out->speedReady = resolved != 0;
        out->frozen = false;
        out->money = money;
        out->timeScale = (float)speed;
        out->error.clear();
    } else {
        out->resolved = false;
        out->moneyReady = false;
        out->speedReady = false;
        out->frozen = false;
        out->error = "无法解析状态: " + reply;
    }
    if (out->error.empty()) {
        ParseFeatureStates(reply, &out->featureStates);
        ParseFeatureValues(reply, &out->featureValues);
    }
    return true;
}
