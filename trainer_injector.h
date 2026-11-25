#pragma once
#include <windows.h>
#include <string>

bool Injector_FindGame(DWORD* pid);
bool Injector_Inject(DWORD pid, const std::wstring& dllPath, std::string* error);
