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
enum { DEVICE_TYPE_GPU, DEVICE_TYPE_COOLER, DEVICE_TYPE_MOTHERBOARD, ZONE_TYPE_SINGLE, ZONE_TYPE_LINEAR };
enum { MODE_COLORS_PER_LED, MODE_COLORS_NONE, MODE_COLORS_MODE_SPECIFIC };
enum { MODE_FLAG_HAS_PER_LED_COLOR=1, MODE_FLAG_HAS_BRIGHTNESS=2, MODE_FLAG_HAS_DIRECTION_LR=4, MODE_FLAG_HAS_MODE_SPECIFIC_COLOR=8, MODE_FLAG_HAS_SPEED=16 };
enum { ZONE_FLAG_MANUALLY_CONFIGURABLE_SIZE=1, ZONE_FLAG_MANUALLY_CONFIGURABLE_NAME=2, ZONE_FLAG_MANUALLY_CONFIGURABLE_TYPE=4, ZONE_FLAG_MANUALLY_CONFIGURABLE_MATRIX_MAP=8, ZONE_FLAG_MANUALLY_CONFIGURABLE_SEGMENTS=16, ZONE_FLAG_MANUALLY_CONFIGURED_NAME=32, ZONE_FLAG_MANUALLY_CONFIGURED_SIZE=64, ZONE_FLAG_MANUALLY_CONFIGURED_TYPE=128, ZONE_FLAG_MANUALLY_CONFIGURED_MATRIX_MAP=256 };
struct mode { std::string name; int value=0,flags=0,color_mode=0,speed=0,brightness=4,direction=0,speed_min=0,speed_max=0,brightness_min=0,brightness_max=0,colors_min=0,colors_max=0; std::vector<RGBColor> colors; };
struct zone { std::string name; int type=0,leds_min=0,leds_max=0,leds_count=0,flags=0; RGBColor* colors=nullptr; struct {unsigned int width=0,height=0; std::vector<unsigned int> map;} matrix_map; };
struct led { std::string name; unsigned int value=0; };
// Only the presentation/worker layer is stubbed. Tests compile the actual
// native driver and NVAPI structure header, then observe its real calls.
class RGBController {
public:
    virtual ~RGBController() = default;
    virtual void DeviceUpdateLEDs()=0;
    virtual void DeviceUpdateZoneLEDs(int)=0;
    virtual void DeviceUpdateSingleLED(int)=0;
    virtual void DeviceUpdateMode()=0;
    inline static unsigned int shutdown_calls=0;
    void Shutdown() {++shutdown_calls;}
    void SetupColors() { colors.resize(leds.size()); size_t offset=0; for(auto& z: zones) { z.colors=colors.empty()?nullptr:colors.data()+offset; offset+=z.leds_count; } }
    std::string name,vendor,description,location,version,serial;
    int type,active_mode=0;
    std::vector<mode> modes;
    std::vector<zone> zones;
    std::vector<led> leds;
    std::vector<RGBColor> colors;
};
