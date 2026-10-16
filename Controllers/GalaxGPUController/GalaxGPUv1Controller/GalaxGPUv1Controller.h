/*---------------------------------------------------------*\
| GalaxGPUv1Controller.h                                    |
|                                                           |
|   Driver for Galax/KFA2 GPU                               |
|                                                           |
|   Niels Westphal (crashniels)                 12 Jul 2020 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include "i2c_smbus.h"

typedef unsigned char galax_gpu_dev_id;

enum
{
    /* RGB Registers */
    GALAX_V1_RED_REGISTER                      = 0x02, /* Red Register */
    GALAX_V1_GREEN_REGISTER                    = 0x03, /* Green Register */
    GALAX_V1_BLUE_REGISTER                     = 0x04, /* Blue Register */
    GALAX_V1_SG_SYNC_REGISTER                  = 0x07,
    GALAX_V1_SG_APPLY_REGISTER                 = 0x08,
    /* MODE Registers */
    GALAX_V1_MODE_REGISTER_1                   = 0x05, /* Mode Register 1 */
    GALAX_V1_MODE_REGISTER_2                   = 0x06, /* Mode Register 2 */
};

enum
{
    /* Static Mode Values */
    GALAX_V1_MODE_STATIC_VALUE_1               = 0x00,
    GALAX_V1_MODE_STATIC_VALUE_2               = 0x01,
    /* Breathing Mode Values */
    GALAX_V1_MODE_BREATHING_VALUE_1            = 0x04,
    GALAX_V1_MODE_BREATHING_VALUE_2            = 0x00,
    /* Rainbow Mode Values */
    GALAX_V1_MODE_RAINBOW_VALUE_1              = 0x84,
    GALAX_V1_MODE_RAINBOW_VALUE_2              = 0x02,
    /* Cycle Breathing Mode Values */
    GALAX_V1_MODE_CYCLE_BREATHING_VALUE_1      = 0x84,
    GALAX_V1_MODE_CYCLE_BREATHING_VALUE_2      = 0x40,
};

#define GALAX_V1_SG_STATIC_VALUE_2  0x20
#define GALAX_V1_SG_APPLY_VALUE     0x5A
#define GALAX_V1_SG_SYNC_MODE       5

class GalaxGPUv1Controller
{
public:
    GalaxGPUv1Controller(i2c_smbus_interface* bus, galax_gpu_dev_id, std::string dev_name, bool sg = false);
    ~GalaxGPUv1Controller();

    static bool   TestForSG(i2c_smbus_interface* bus, galax_gpu_dev_id dev);
    bool          IsSG();
    bool          GetLEDColors(unsigned char& red, unsigned char& green, unsigned char& blue);
    int           GetSGSync();
    std::string   GetDeviceLocation();
    std::string   GetDeviceName();
    unsigned char GetLEDRed();
    unsigned char GetLEDGreen();
    unsigned char GetLEDBlue();
    bool          SetMode(unsigned char mode, unsigned char red, unsigned char green, unsigned char blue);

    unsigned char GalaxGPURegisterRead(unsigned char reg);
    void          GalaxGPURegisterWrite(unsigned char reg, unsigned char val);

private:
    i2c_smbus_interface *   bus;
    galax_gpu_dev_id        dev;
    std::string             name;
    bool                    sg;
    int                     sync_mode = -1;
    static int ReadSG(i2c_smbus_interface* bus, galax_gpu_dev_id dev, unsigned char reg,
                      unsigned char* data, unsigned int length);
    bool WriteSG(unsigned char reg, unsigned char* data, unsigned int length);
    bool WriteSGByte(unsigned char reg, unsigned char value);
    bool SetSGSync(unsigned char value);
};
