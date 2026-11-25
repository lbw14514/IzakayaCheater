#include "trainer_injector.h"
#include <tlhelp32.h>
#include <cwchar>

bool Injector_FindGame(DWORD* pid)
{
    if (!pid) return false;
    *pid = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"Touhou Mystia Izakaya.exe") == 0 ||
                _wcsicmp(entry.szExeFile, L"Izakaya.exe") == 0) {
                *pid = entry.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

bool Injector_Inject(DWORD pid, const std::wstring& dllPath, std::string* error)
{
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        if (error) *error = "OpenProcess 失败 错误码 " + std::to_string(GetLastError());
        return false;
    }

    SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        if (error) *error = "VirtualAllocEx 失败 错误码 " + std::to_string(GetLastError());
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, remote, dllPath.c_str(), bytes, NULL)) {
        if (error) *error = "WriteProcessMemory 失败 错误码 " + std::to_string(GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC loadLibrary = kernel ? GetProcAddress(kernel, "LoadLibraryW") : NULL;
    if (!loadLibrary) {
        if (error) *error = "找不到 LoadLibraryW";
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, NULL, 0,
                                       (LPTHREAD_START_ROUTINE)loadLibrary,
                                       remote, 0, NULL);
    if (!thread) {
        if (error) *error = "CreateRemoteThread 失败 错误码 " + std::to_string(GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    DWORD wait = WaitForSingleObject(thread, 10000);
    bool ok = false;
    if (wait == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        if (GetExitCodeThread(thread, &exitCode) && exitCode != 0) {
            ok = true;
        } else if (error) {
            *error = "LoadLibraryW 返回 0 目标进程未能加载核心 DLL";
        }
        CloseHandle(thread);
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    } else {
        if (error) {
            *error = (wait == WAIT_FAILED)
                ? "等待注入线程失败 错误码 " + std::to_string(GetLastError())
                : "注入线程超时 远程内存已保留以避免目标进程崩溃";
        }
        CloseHandle(thread);
    }

    CloseHandle(process);
    return ok;
}
