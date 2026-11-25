#include "ipc_server.h"
#include "game_api.h"
#include "log.h"
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

static std::atomic<bool> g_running(false);
static std::thread g_thread;

static bool ReadMessage(HANDLE pipe, std::string& out)
{
    char buf[4096] = {0};
    DWORD read = 0;
    if (!ReadFile(pipe, buf, sizeof(buf) - 1, &read, NULL)) return false;
    if (read == 0) return false;
    out.assign(buf, read);
    while (!out.empty() && (out[out.size() - 1] == '\n' || out[out.size() - 1] == '\r' || out[out.size() - 1] == '\0')) {
        out.erase(out.size() - 1);
    }
    return true;
}

static bool WriteMessage(HANDLE pipe, const std::string& text)
{
    std::string data = text + "\n";
    DWORD written = 0;
    return WriteFile(pipe, data.data(), (DWORD)data.size(), &written, NULL) != FALSE;
}

static std::string Ok(const std::string& text)
{
    return "OK " + text;
}

static std::string Err(const std::string& text)
{
    return "ERR " + text;
}

static std::string HandleStatus()
{
    bool game = Game_GameAssemblyLoaded();
    bool resolved = Game_IsResolved();
    bool moneyReady = Game_IsMoneyReady();
    bool speedReady = Game_IsSpeedReady();
    bool frozen = Game_IsFrozen();
    long long money = 0;
    float speed = 1.0f;
    std::string ignored;
    if (moneyReady) Game_GetMoney(&money, &ignored);
    if (speedReady) Game_GetTimeScale(&speed, &ignored);
    char buf[320] = {0};
    _snprintf(buf, sizeof(buf) - 1,
              "game=%d resolved=%d moneyReady=%d speedReady=%d frozen=%d money=%lld timeScale=%.4f",
              game ? 1 : 0, resolved ? 1 : 0, moneyReady ? 1 : 0, speedReady ? 1 : 0,
              frozen ? 1 : 0, money, speed);
    return Ok(buf);
}

static std::string HandleCommand(const std::string& command)
{
    if (command == "PING") return Ok("PONG " + std::to_string(GetCurrentProcessId()));
    if (command == "SELFCHECK") {
        bool game = Game_GameAssemblyLoaded();
        std::string error;
        bool resolved = game ? Game_EnsureResolved(&error) : false;
        char buf[320] = {0};
        _snprintf(buf, sizeof(buf) - 1,
                  "game=%d il2cpp=%d moneyReady=%d speedReady=%d obtain=unchecked%s%s",
                  game ? 1 : 0, resolved ? 1 : 0,
                  Game_IsMoneyReady() ? 1 : 0, Game_IsSpeedReady() ? 1 : 0,
                  error.empty() ? "" : " error=", error.c_str());
        return Ok(buf);
    }
    if (command == "INIT") {
        std::string error;
        if (!Game_EnsureResolved(&error)) return Err(error);
        return Ok("resolved");
    }
    if (command == "STATUS") return HandleStatus();

    size_t space = command.find(' ');
    std::string head = space == std::string::npos ? command : command.substr(0, space);
    std::string rest = space == std::string::npos ? "" : command.substr(space + 1);

    if (head == "MONEY") {
        std::string error;
        if (rest == "GET") {
            long long value = 0;
            if (!Game_EnsureResolved(&error)) return Err(error);
            if (!Game_GetMoney(&value, &error)) return Err(error);
            return Ok(std::to_string(value));
        }
        if (rest.compare(0, 4, "SET ") == 0) {
            long long value = strtoll(rest.c_str() + 4, NULL, 10);
            if (!Game_EnsureResolved(&error)) return Err(error);
            if (!Game_SetMoney(value, &error)) return Err(error);
            return Ok("done");
        }
        return Err("未知金钱命令");
    }

    if (head == "SPEED") {
        std::string error;
        if (rest == "GET") {
            float value = 1.0f;
            if (!Game_EnsureResolved(&error)) return Err(error);
            if (!Game_GetTimeScale(&value, &error)) return Err(error);
            char buf[64] = {0};
            _snprintf(buf, sizeof(buf) - 1, "%.4f", value);
            return Ok(buf);
        }
        if (rest.compare(0, 4, "SET ") == 0) {
            double value = strtod(rest.c_str() + 4, NULL);
            if (!Game_EnsureResolved(&error)) return Err(error);
            if (!Game_SetTimeScale((float)value, &error)) return Err(error);
            return Ok("done");
        }
        return Err("未知速度命令");
    }

    if (head == "FREEZE") {
        std::string error;
        if (!Game_EnsureResolved(&error)) return Err(error);
        bool enabled = (rest == "1" || rest == "true");
        if (!Game_FreezeTime(enabled, &error)) return Err(error);
        return Ok(enabled ? "frozen" : "restored");
    }

    if (head == "OBTAIN") {
        std::string error;
        if (!Game_Obtain(rest, &error)) return Err(error);
        return Ok("done");
    }

    if (command == "SHUTDOWN") {
        g_running = false;
        return Ok("bye");
    }

    return Err("未知命令");
}

static void IpcThread()
{
    std::wstring name = L"\\\\.\\pipe\\IzakayaCheaterCore." + std::to_wstring(GetCurrentProcessId());
    CoreLog("pipe server start: %ls", name.c_str());

    while (g_running) {
        HANDLE pipe = CreateNamedPipeW(name.c_str(),
                                      PIPE_ACCESS_DUPLEX,
                                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                      1, 8192, 8192, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) {
            CoreLog("CreateNamedPipe failed: %lu", GetLastError());
            Sleep(500);
            continue;
        }

        BOOL connected = ConnectNamedPipe(pipe, NULL);
        if (!connected) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(pipe);
                if (!g_running) break;
                Sleep(100);
                continue;
            }
        }

        for (;;) {
            std::string message;
            if (!ReadMessage(pipe, message)) break;
            CoreLog("command: %s", message.c_str());
            std::string response = HandleCommand(message);
            if (!WriteMessage(pipe, response)) break;
            if (!g_running) break;
        }

        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }

    CoreLog("pipe server stop");
}

void Ipc_Start()
{
    if (g_running) return;
    g_running = true;
    g_thread = std::thread(IpcThread);
    g_thread.detach();
}

void Ipc_Stop()
{
    g_running = false;
}

bool Ipc_IsRunning()
{
    return g_running;
}
