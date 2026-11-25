#pragma once
#include <string>

bool Features_Init(std::string* error);
bool Features_IsKnown(const std::string& key);
bool Features_Ready(const std::string& key);
bool Features_Set(const std::string& key, bool enabled, float value, std::string* error);
bool Features_SetBaseTimeScale(float value, std::string* error);
void Features_DisableAll();
float Features_UnscaleTimeScale(float current);
std::string Features_Readiness();
void Features_OnObtainBegin();
void Features_OnObtainEnd();
