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
static std::atomic<bool> g_client(false);
static std::atomic<ULONGLONG> g_disconnected(0);
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
    char buf[2048] = {0};
    std::string features = Game_FeatureReadiness();
    std::string states = Game_FeatureStates();
    std::string values = Game_FeatureValues();
    _snprintf(buf, sizeof(buf) - 1,
              "game=%d resolved=%d moneyReady=%d speedReady=%d frozen=%d money=%lld timeScale=%.4f features=%s states=%s values=%s",
              game ? 1 : 0, resolved ? 1 : 0, moneyReady ? 1 : 0, speedReady ? 1 : 0,
              frozen ? 1 : 0, money, speed, features.c_str(), states.c_str(), values.c_str());
    return Ok(buf);
}

static std::string HandleCommand(const std::string& command)
{
    if (command == "PING") return Ok("PONG " + std::to_string(GetCurrentProcessId()));
    if (command == "SELFCHECK") {
        bool game = Game_GameAssemblyLoaded();
        std::string error;
        bool resolved = game ? Game_EnsureResolved(&error) : false;
        std::string detail = "obtain=0";
        if (game && resolved) {
            const char* cats[] = {
                "badges", "clothes", "records", "fishing", "partners", "decorations",
                "ingredients", "foods", "beverages", "cookers", "recipes", "izakayas"
            };
            detail = "obtain=";
            for (size_t i = 0; i < sizeof(cats) / sizeof(cats[0]); i++) {
                std::string itemError;
                bool ok = Game_CheckObtain(cats[i], &itemError);
                detail += cats[i];
                detail += ok ? "=1" : "=0";
                detail += " ";
            }
        }
        char buf[2048] = {0};
        std::string features = Game_FeatureReadiness();
        _snprintf(buf, sizeof(buf) - 1,
                  "game=%d il2cpp=%d moneyReady=%d speedReady=%d %s features=%s%s%s",
                  game ? 1 : 0, resolved ? 1 : 0,
                  Game_IsMoneyReady() ? 1 : 0, Game_IsSpeedReady() ? 1 : 0,
                  detail.c_str(), features.c_str(), error.empty() ? "" : " error=", error.c_str());
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

    if (head == "FEATURE") {
        char key[64] = {0};
        int enabled = 0;
        float value = 0.0f;
        int parsed = sscanf(rest.c_str(), "%63s %d %f", key, &enabled, &value);
        if (parsed < 2) return Err("参数不足");
        std::string error;
        if (!Game_SetFeature(key, enabled != 0, value, &error)) return Err(error);
        return Ok(enabled ? "on" : "off");
    }

    if (head == "SPECIAL") {
        char key[64] = {0};
        int value = 0;
        int value2 = 0;
        int consumed = 0;
        int parsed = sscanf(rest.c_str(), "%63s %d %d%n", key, &value, &value2, &consumed);
        if (parsed < 1) return Err("参数不足");
        std::string text;
        if (consumed > 0 && (size_t)consumed < rest.size()) {
            text = rest.substr(consumed);
            while (!text.empty() && text[0] == ' ') text.erase(0, 1);
        }
        std::string error;
        if (!Game_Special(key, value, value2, text, &error)) return Err(error);
        return Ok("special");
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

        g_client = true;
        g_disconnected = 0;

        for (;;) {
            std::string message;
            if (!ReadMessage(pipe, message)) break;
            CoreLog("command: %s", message.c_str());
            std::string response = HandleCommand(message);
            if (!WriteMessage(pipe, response)) break;
            if (!g_running) break;
        }

        g_client = false;
        g_disconnected = GetTickCount64();
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }

    g_client = false;
    g_disconnected = GetTickCount64();
    CoreLog("pipe server stop");
}

void Ipc_Start()
{
    if (g_running) return;
    g_running = true;
    g_disconnected = GetTickCount64();
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

bool Ipc_ClientGone(int timeoutMs)
{
    if (g_client.load()) return false;
    ULONGLONG disconnected = g_disconnected.load();
    if (disconnected == 0) return false;
    return GetTickCount64() - disconnected > (ULONGLONG)timeoutMs;
}
