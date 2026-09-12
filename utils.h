#pragma once
#include <windows.h>
#include <vector>
#include <string>

// Memory layout of the game (GameAssembly.dll); tied to SUPPORTED_VERSION in config.h.
extern const uintptr_t MOD_BASE_OFFSET;
extern const std::vector<unsigned int> MONEY_OFFSETS;

// Game process and mono module names.
extern const char PROC_NAME[];
extern const char MODULE_NAME[];

DWORD GetProcessID(const char* procName);
uintptr_t GetModuleBaseAddress(DWORD procId, const char* modName);
uintptr_t GetDMAAddress(HANDLE hProc, uintptr_t ptr, const std::vector<unsigned int>& offsets);

enum IzakayaCode
{
    SUCCESS = 0,
    PROCESS_NOT_FOUND = -1,
    BASE_ADDRESS_NOT_FOUND = -2,
    CANNOT_ATTACH_TO_PROCESS = -3,
    MEMORY_READ_FAILED = -4,
    MEMORY_WRITE_FAILED = -5
};

struct IzakayaResult
{
    IzakayaCode code;
    uintptr_t modBase;
    HANDLE hProc;
};

IzakayaResult GetIzakayaProcess();
IzakayaCode ReadMoney(uintptr_t modBase, HANDLE hProc, DWORD* outValue);
IzakayaCode ChangeMoney(uintptr_t modBase, HANDLE hProc, DWORD value);
