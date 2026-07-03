#pragma once
#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <string>

static const uintptr_t modBaseOffset = 0x3F81830;

static const std::vector<unsigned int> moneyOffsets = { 0xB8, 0x38 };

DWORD GetProcessID(const char* procName);
uintptr_t GetModuleBaseAddress(DWORD procId, const char* modName);
uintptr_t GetDMAAddress(HANDLE hProc, uintptr_t ptr, std::vector<unsigned int> offsets);

enum IzakayaCode
{
    SUCCESS = 0,
    PROCESS_NOT_FOUND = -1,
    BASE_ADDRESS_NOT_FOUND = -2,
    CANNOT_ATTACH_TO_PROCESS = -3
};

struct IzakayaResult
{
    IzakayaCode code;
    uintptr_t modBase;
    HANDLE hProc;
};

IzakayaResult GetIzakayaProcess();
DWORD ReadMoney(uintptr_t modBase, HANDLE hProc);
void ChangeMoney(uintptr_t modBase, HANDLE hProc, DWORD value);

