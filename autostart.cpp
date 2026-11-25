#include "autostart.h"
#include <windows.h>
#include <string>

static const char* kTaskName = "IzakayaCheaterAutoStart";
static const char* kMarkerKey = "Software\\IzakayaCheater";
static const char* kMarkerValue = "AutoStart";

static std::string ExePath()
{
    char buffer[MAX_PATH] = {0};
    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return std::string();
    return std::string(buffer, length);
}

static bool SetMarker(bool enabled)
{
    HKEY key = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kMarkerKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
        return false;
    DWORD value = enabled ? 1 : 0;
    LONG result = RegSetValueExA(key, kMarkerValue, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

static int RunHidden(const std::string& command)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
    std::string mutableCommand = command;
    if (!CreateProcessA(NULL, (LPSTR)mutableCommand.c_str(), NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

bool AutoStart_Enabled()
{
    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kMarkerKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    LONG result = RegQueryValueExA(key, kMarkerValue, NULL, &type, (LPBYTE)&value, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_DWORD && value != 0;
}

bool AutoStart_Set(bool enabled, std::string* error)
{
    std::string exe = ExePath();
    if (exe.empty())
    {
        if (error) *error = "无法获取程序路径";
        return false;
    }

    if (enabled)
    {
        std::string command = "schtasks.exe /Create /TN \"" + std::string(kTaskName) +
                              "\" /SC ONLOGON /RL HIGHEST /F /TR \"\\\"" + exe + "\\\" --watch\"";
        int code = RunHidden(command);
        if (code != 0)
        {
            if (error) *error = "创建登录任务失败 " + std::to_string(code);
            return false;
        }
        if (!SetMarker(true))
        {
            if (error) *error = "写入启动标记失败";
            return false;
        }
        return true;
    }

    RunHidden("schtasks.exe /Delete /TN \"" + std::string(kTaskName) + "\" /F");
    if (!SetMarker(false))
    {
        if (error) *error = "清除启动标记失败";
        return false;
    }
    return true;
}

bool AutoStart_StartWatcher()
{
    std::string exe = ExePath();
    if (exe.empty()) return false;
    std::string command = "\"" + exe + "\" --watch";
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
    BOOL ok = CreateProcessA(exe.c_str(), (LPSTR)command.c_str(), NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (ok)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok != FALSE;
}
