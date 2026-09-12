#include "utils.h"
#include <tlhelp32.h>

const uintptr_t MOD_BASE_OFFSET = 0x2E7E9C0;
const std::vector<unsigned int> MONEY_OFFSETS = { 0xB8, 0x10 };
const char PROC_NAME[] = "Touhou Mystia Izakaya.exe";
const char MODULE_NAME[] = "GameAssembly.dll";

DWORD GetProcessID(const char* procName){
	DWORD procId = 0;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 procEntry = { sizeof(procEntry) };
		if (Process32First(hSnap, &procEntry)) {
			do
			{
				if (!_stricmp(procEntry.szExeFile, procName)) {
					procId = procEntry.th32ProcessID;
					break;
				}

			} while (Process32Next(hSnap, &procEntry));
		}
	}
	CloseHandle(hSnap);
	return procId;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const char* modName) {
	uintptr_t modBase = 0;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 modEntry = { sizeof(modEntry) };
		if (Module32First(hSnap, &modEntry)) {
			do
			{
				if (!_stricmp(modEntry.szModule, modName)) {
					modBase = (uintptr_t)modEntry.modBaseAddr;
					break;
				}

			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBase;
}

uintptr_t GetDMAAddress(HANDLE hProc, uintptr_t ptr, const std::vector<unsigned int>& offsets) {
	uintptr_t dma = ptr;
	for (size_t i = 0; i < offsets.size(); i++) {
		if (!ReadProcessMemory(hProc, (BYTE*)dma, &dma, sizeof(dma), nullptr)) {
			return 0;
		}
		dma += offsets[i];
	}
	return dma;
}

IzakayaResult GetIzakayaProcess()
{
    IzakayaResult result;
    DWORD procId = GetProcessID(PROC_NAME);
    if (procId == 0) {
        result.code = IzakayaCode::PROCESS_NOT_FOUND;
        return result;
    }
    uintptr_t modBase = GetModuleBaseAddress(procId, MODULE_NAME);
    if (modBase == 0) {
        result.code = IzakayaCode::BASE_ADDRESS_NOT_FOUND;
        return result;
    }
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
    if (hProc == NULL || hProc == INVALID_HANDLE_VALUE) {
        result.code = IzakayaCode::CANNOT_ATTACH_TO_PROCESS;
        return result;
    }
    result.code = IzakayaCode::SUCCESS;
    result.modBase = modBase;
    result.hProc = hProc;
    return result;
}

IzakayaCode ReadMoney(uintptr_t modBase, HANDLE hProc, DWORD* outValue)
{
    *outValue = 0;
    uintptr_t moneyPtr = GetDMAAddress(hProc, modBase + MOD_BASE_OFFSET, MONEY_OFFSETS);
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(hProc, (BYTE*)moneyPtr, outValue, sizeof(*outValue), &bytesRead)
            || bytesRead != sizeof(*outValue)) {
        return IzakayaCode::MEMORY_READ_FAILED;
    }
    return IzakayaCode::SUCCESS;
}

IzakayaCode ChangeMoney(uintptr_t modBase, HANDLE hProc, DWORD value)
{
    uintptr_t moneyPtr = GetDMAAddress(hProc, modBase + MOD_BASE_OFFSET, MONEY_OFFSETS);
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProc, (BYTE*)moneyPtr, &value, sizeof(value), &bytesWritten)
            || bytesWritten != sizeof(value)) {
        return IzakayaCode::MEMORY_WRITE_FAILED;
    }
    return IzakayaCode::SUCCESS;
}
