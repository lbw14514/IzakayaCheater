#pragma once
#include <string>

bool Game_EnsureResolved(std::string* error);
bool Game_IsResolved();
bool Game_IsMoneyReady();
bool Game_IsSpeedReady();
bool Game_FreezeTime(bool enabled, std::string* error);
bool Game_IsFrozen();
bool Game_HoldFreeze();
bool Game_GameAssemblyLoaded();
bool Game_GetMoney(long long* out, std::string* error);
bool Game_SetMoney(long long value, std::string* error);
bool Game_GetTimeScale(float* out, std::string* error);
bool Game_SetTimeScale(float value, std::string* error);
bool Game_Obtain(const std::string& category, std::string* error);
bool Game_CheckObtain(const std::string& category, std::string* error);
bool Game_SetFeature(const std::string& key, bool enabled, float value, std::string* error);
std::string Game_FeatureReadiness();
std::string Game_FeatureStates();
void Game_DisableFeatures();
