/*---------------------------------------------------------*\
| RGBController_KFA2EX.h                                    |
|                                                           |
| KFA2 EX Gamer I2C lighting controller interface           |
|                                                           |
| valer23                                     13 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#pragma once

#include "RGBController.h"
#include "i2c_smbus.h"
#include <mutex>

class RGBController_KFA2EX : public RGBController
{
public:
    RGBController_KFA2EX(i2c_smbus_interface* bus, unsigned char address, const std::string& name);
    ~RGBController_KFA2EX();
    static bool Probe(i2c_smbus_interface* bus, unsigned char address);
    void SetupZones();
    void DeviceUpdateLEDs() override;
    void DeviceUpdateZoneLEDs(int zone) override;
    void DeviceUpdateSingleLED(int led) override;
    void DeviceUpdateMode() override;

private:
    i2c_smbus_interface* bus;
    unsigned char address;
    std::mutex io_mutex;
    static int Read(i2c_smbus_interface* bus, unsigned char address, unsigned char reg,
                    unsigned char* data, unsigned int length);
    void Write(unsigned char reg, unsigned char* data, unsigned int length);
    void WriteByte(unsigned char reg, unsigned char value);
    void SetSync(unsigned char value);
    void ApplyColour();
};
