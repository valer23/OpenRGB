#pragma once
#include "SettingsManager.h"
struct ResourceManager {
 static ResourceManager* get() {static ResourceManager r;return &r;}
 SettingsManager* GetSettingsManager() {static SettingsManager s;return &s;}
};
