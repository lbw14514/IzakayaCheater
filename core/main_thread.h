#pragma once
#include <windows.h>
#include <functional>
#include <string>

bool MainThread_EnsureInstalled(std::string* error);
bool MainThread_Run(const std::function<void()>& fn, int timeoutMs, std::string* error);
HWND MainThread_Window();
void MainThread_Uninstall();
