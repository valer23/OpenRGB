#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
using RGBColor = uint32_t;
inline RGBColor ToRGBColor(unsigned char r,unsigned char g,unsigned char b) { return r|(g<<8)|(b<<16); }
inline unsigned char RGBGetRValue(RGBColor c) { return c&255; }
inline unsigned char RGBGetGValue(RGBColor c) { return (c>>8)&255; }
inline unsigned char RGBGetBValue(RGBColor c) { return (c>>16)&255; }
enum { DEVICE_TYPE_GPU, MODE_FLAG_HAS_PER_LED_COLOR, MODE_COLORS_PER_LED, MODE_COLORS_NONE, ZONE_TYPE_SINGLE };
struct mode { std::string name; int value,flags,color_mode,speed=0,brightness=4,direction=0; std::vector<RGBColor> colors; };
struct zone { std::string name; int type,leds_min,leds_max,leds_count; };
struct led { std::string name; };
// Only the presentation/worker layer is stubbed. Tests compile the actual
// native driver and NVAPI structure header, then observe its real calls.
class RGBController {
public:
    virtual ~RGBController() = default;
    virtual void DeviceUpdateLEDs()=0;
    virtual void DeviceUpdateZoneLEDs(int)=0;
    virtual void DeviceUpdateSingleLED(int)=0;
    virtual void DeviceUpdateMode()=0;
    void Shutdown() {}
    void SetupColors() { colors.resize(leds.size()); }
    std::string name,vendor,description,location;
    int type,active_mode=0;
    std::vector<mode> modes;
    std::vector<zone> zones;
    std::vector<led> leds;
    std::vector<RGBColor> colors;
};
