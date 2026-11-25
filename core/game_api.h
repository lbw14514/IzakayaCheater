#pragma once
#include <string>

bool Game_EnsureResolved(std::string* error);
bool Game_IsResolved();
bool Game_IsMoneyReady();
bool Game_IsSpeedReady();
bool Game_GameAssemblyLoaded();
bool Game_GetMoney(long long* out, std::string* error);
bool Game_SetMoney(long long value, std::string* error);
bool Game_GetTimeScale(float* out, std::string* error);
bool Game_SetTimeScale(float value, std::string* error);
bool Game_Obtain(const std::string& category, std::string* error);
