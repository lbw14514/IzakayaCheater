#include "ipc_server.h"
#include "log.h"
#include "main_thread.h"
#include <windows.h>

static DWORD WINAPI CoreThread(LPVOID)
{
    CoreLog("core loaded pid=%lu", GetCurrentProcessId());

    std::string error;
    if (!MainThread_EnsureInstalled(&error)) {
        CoreLog("main thread install deferred: %s", error.c_str());
    }

    Ipc_Start();
    return 0;
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(NULL, 0, CoreThread, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
