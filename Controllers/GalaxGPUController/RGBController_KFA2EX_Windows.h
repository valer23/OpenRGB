/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "RGBController.h"
#include "nvapi.h"
#include <mutex>

class RGBController_KFA2EX : public RGBController
{
public:
    explicit RGBController_KFA2EX(NV_PHYSICAL_GPU_HANDLE gpu);
    ~RGBController_KFA2EX();
    static bool Probe(NV_PHYSICAL_GPU_HANDLE gpu);
    void SetupZones();
    void DeviceUpdateLEDs() override;
    void DeviceUpdateZoneLEDs(int zone) override;
    void DeviceUpdateSingleLED(int led) override;
    void DeviceUpdateMode() override;

private:
    NV_PHYSICAL_GPU_HANDLE gpu;
    std::mutex io_mutex;
    static NV_STATUS Transfer(NV_PHYSICAL_GPU_HANDLE gpu, unsigned char reg,
                              unsigned char* data, unsigned int length, bool write);
    void Write(unsigned char reg, unsigned char* data, unsigned int length);
    void WriteByte(unsigned char reg, unsigned char value);
    void SetSync(unsigned char value);
    void ApplyColour();
};
