#pragma once
#include <cstddef>
#include <string>

bool Json_Validate(const char* text, size_t len, std::string* error);
