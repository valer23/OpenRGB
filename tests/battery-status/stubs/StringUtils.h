#pragma once
#include <string>
struct StringUtils { static std::string wstring_to_string(const std::wstring w) {return std::string(w.begin(),w.end());} };
