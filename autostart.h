#pragma once
#include <string>

bool AutoStart_Enabled();
bool AutoStart_Set(bool enabled, std::string* error);
bool AutoStart_StartWatcher();
