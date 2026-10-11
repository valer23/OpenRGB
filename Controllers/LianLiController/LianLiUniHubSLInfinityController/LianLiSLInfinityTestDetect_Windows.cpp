/* SPDX-License-Identifier: GPL-2.0-or-later */
// Isolated local test detector. Normal builds use LianLiControllerDetect.cpp.
#ifdef SLINF_LOCAL_TEST
#include "DetectionManager.h"
#include "LianLiUniHubSLInfinityController.h"
#include "RGBController_LianLiUniHubSLInfinity.h"

class LocalSLInfinityTestController : public RGBController_LianLiUniHubSLInfinity
{
public:
    explicit LocalSLInfinityTestController(LianLiUniHubSLInfinityController* hardware)
        : RGBController_LianLiUniHubSLInfinity(hardware)
    {
        const unsigned int fan_counts[4] = {3, 2, 1, 0}; // User-confirmed local layout.
        for(unsigned int channel = 0; channel < 8; channel++)
        {
            zones[channel].leds_count = 16 * fan_counts[channel / 2];
            zones[channel].flags |= ZONE_FLAG_MANUALLY_CONFIGURED_SIZE | ZONE_FLAG_MANUALLY_CONFIGURED_NAME;
            zones[channel].name = "Port " + std::to_string(channel / 2 + 1) + (channel % 2 ? " sides" : " centres");
        }
        SetupZones();
        // Keep the verified Static mode and vendor-defined independent effects.
        mode breathing = modes[2];
        mode spectrum = modes[3];
        mode rainbow = modes[4];
        breathing.colors.assign(6, ToRGBColor(128, 0, 255));
        modes.resize(1);
        modes[0].name = "Static";
        modes[0].brightness = modes[0].brightness_max;
        modes.push_back(breathing);
        modes.push_back(spectrum);
        modes.push_back(rainbow);
        active_mode = 0;
        for(auto& color : colors) color = ToRGBColor(128, 0, 255);
        // Presentation only: no lighting or cooling writes during detection.
    }
};

DetectedControllers DetectLocalSLInfinity(hid_device_info* info, const std::string& name)
{
    DetectedControllers found;
    hid_device* dev = hid_open_path(info->path);
    if(!dev) return found;
    auto* hardware = new LianLiUniHubSLInfinityController(dev, info->path, name);
    found.push_back(new LocalSLInfinityTestController(hardware));
    return found;
}
REGISTER_HID_DETECTOR_IPU("Lian Li SL Infinity (local static test)", DetectLocalSLInfinity, 0x0CF2, 0xA102, 0x01, 0xFF72, 0xA1);
#endif