#pragma once
#include <nlohmann/json.hpp>
using json=nlohmann::json;
inline json mouse_test_settings={{"enabled",true},{"timeout_seconds",30}};
struct SettingsManager {json GetSettings(const std::string&) {return mouse_test_settings;}};
