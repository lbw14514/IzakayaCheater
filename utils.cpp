#include "utils.h"
#include <tlhelp32.h>


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
uintptr_t GetDMAAddress(HANDLE hProc, uintptr_t ptr, std::vector <unsigned int> offsets) {
	uintptr_t dma = 0;
	dma = ptr;
	for (int i = 0; i < offsets.size(); i++) {
		ReadProcessMemory(hProc, (BYTE*)dma, &dma, sizeof(dma), nullptr);
		dma += offsets[i];
	}
	return dma;
}



IzakayaResult GetIzakayaProcess()
{
    IzakayaResult result;
    DWORD procId = GetProcessID("Touhou Mystia Izakaya.exe");
    if (procId == 0) {
        result.code = IzakayaCode::PROCESS_NOT_FOUND;
        return result;
    }
    uintptr_t modBase = GetModuleBaseAddress(procId, "GameAssembly.dll");
    if (modBase == 0) {
        result.code = IzakayaCode::BASE_ADDRESS_NOT_FOUND;
        return result;
    }
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
    if (hProc == INVALID_HANDLE_VALUE) {
        result.code = IzakayaCode::CANNOT_ATTACH_TO_PROCESS;
        return result;
    }
    result.code = IzakayaCode::SUCCESS;
    result.modBase = modBase;
    result.hProc = hProc;
    return result;
}
DWORD ReadMoney(uintptr_t modBase, HANDLE hProc)
{
    DWORD money = 0;
    uintptr_t moneyPtr = GetDMAAddress(hProc, modBase + modBaseOffset, moneyOffsets);
    ReadProcessMemory(hProc, (BYTE*)moneyPtr,  &money, sizeof(money), nullptr);
	return money;
}
void ChangeMoney(uintptr_t modBase, HANDLE hProc, DWORD value)
{
    uintptr_t moneyPtr = GetDMAAddress(hProc, modBase + modBaseOffset, moneyOffsets);
    WriteProcessMemory(hProc, (BYTE*)moneyPtr, &value, sizeof(value), nullptr);
}
