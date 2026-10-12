#pragma once
#include <string>
#include <vector>
#define MODE_FLAG_HAS_PER_LED_COLOR 32
class RGBControllerInterface
{
public:
    std::string name;
    std::vector<std::string> modes;
    std::vector<unsigned int> flags;
    unsigned int GetModeCount() { return (unsigned int)modes.size(); }
    std::string GetModeName(unsigned int index) { return modes[index]; }
    std::string GetName() { return name; }
    unsigned int GetModeFlags(unsigned int index) { return flags[index]; }
};
