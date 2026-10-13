/*---------------------------------------------------------*\
| RGBController_KFA2EX.cpp                                  |
|                                                           |
| KFA2 EX Gamer I2C lighting controller                     |
|                                                           |
| valer23                                     13 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "RGBController_KFA2EX.h"
#include "LogManager.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

/**------------------------------------------------------------------*\
    @name KFA2 GeForce RTX 3080 Ti EX Gamer
    @category GPU
    @type I2C
    @save :o:
    @direct :white_check_mark:
    @effects :o:
    @detectors DetectKFA2EXControllers
    @comment Static colour streaming uses the shared I2C/SMBus interface.
             Hardware effects and persistent saving are unsupported.
             Motherboard Sync selects the captured flag; external colour
             following has not been verified. PCI 10DE:2208 / 10DE:1535.
\*-------------------------------------------------------------------*/

int RGBController_KFA2EX::Read(i2c_smbus_interface* bus, unsigned char address,
                             unsigned char reg, unsigned char* data, unsigned int length)
{
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        int count = bus->i2c_smbus_read_i2c_block_data(address, reg, length, data);
        if(count == (int)length)
        {
            return 0;
        }
        if(attempt < 2)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
    return -1;
}

bool RGBController_KFA2EX::Probe(i2c_smbus_interface* bus, unsigned char address)
{
    unsigned char id[2] = {};
    return Read(bus, address, 0x00, id, 2) == 0 && id[0] == 0x11 && id[1] == 0x00;
}

RGBController_KFA2EX::RGBController_KFA2EX(i2c_smbus_interface* bus, unsigned char address, const std::string& device_name)
    : bus(bus), address(address)
{
    try
    {
        name        = device_name;
        vendor      = "KFA2";
        type        = DEVICE_TYPE_GPU;
        description = "KFA2 EX Gamer I2C lighting controller";
        char address_string[8];
        snprintf(address_string, sizeof(address_string), "0x%02X", address);
        location = "I2C: " + std::string(bus->info.device_name) + ", address " + address_string;

        mode static_mode;
        static_mode.name       = "Static";
        static_mode.value      = 0;
        static_mode.flags      = MODE_FLAG_HAS_PER_LED_COLOR;
        static_mode.color_mode = MODE_COLORS_PER_LED;
        modes.push_back(static_mode);

        mode sync_mode;
        sync_mode.name       = "Motherboard Sync";
        sync_mode.value      = 1;
        sync_mode.flags      = 0;
        sync_mode.color_mode = MODE_COLORS_NONE;
        modes.push_back(sync_mode);

        SetupZones();
        unsigned char sync = 0;
        int sync_status = Read(bus, address, 0x07, &sync, 1);
        if(sync_status != 0 || sync > 1)
        {
            // PCI identity, controller signature and RGB registers already
            // identify this device. An optional mode read must not hide it.
            LOG_WARNING("[KFA2 EX] Sync state unavailable (status %d, value 0x%02X); presenting Static until a mode is selected", sync_status, sync);
            active_mode = 0;
        }
        else
        {
            active_mode = sync;
        }
    }
    catch(...)
    {
        Shutdown();
        throw;
    }
}

RGBController_KFA2EX::~RGBController_KFA2EX()
{
    Shutdown();
}

void RGBController_KFA2EX::SetupZones()
{
    zone gpu_zone;
    gpu_zone.name       = "GPU";
    gpu_zone.type       = ZONE_TYPE_SINGLE;
    gpu_zone.leds_min   = 1;
    gpu_zone.leds_max   = 1;
    gpu_zone.leds_count = 1;
    zones.push_back(gpu_zone);
    led gpu_led;
    gpu_led.name = "GPU";
    leds.push_back(gpu_led);
    SetupColors();
    unsigned char rgb[3] = {};
    if(Read(bus, address, 0x02, rgb, 3) != 0)
    {
        throw std::runtime_error("Unable to read KFA2 colour");
    }
    colors[0] = ToRGBColor(rgb[0], rgb[1], rgb[2]);
}

void RGBController_KFA2EX::Write(unsigned char reg, unsigned char* data, unsigned int length)
{
    int status = -1;
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        status = bus->i2c_smbus_write_i2c_block_data(address, reg, length, data);
        if(status != -1)
        {
            break;
        }
        if(attempt < 2)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
    if(status != 0)
    {
        throw std::runtime_error("KFA2 lighting write failed at register " + std::to_string(reg));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
}

void RGBController_KFA2EX::WriteByte(unsigned char reg, unsigned char value)
{
    Write(reg, &value, 1);
}

void RGBController_KFA2EX::SetSync(unsigned char value)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        int status = bus->i2c_smbus_write_byte_data(address, 0x07, value);
        if(status == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            unsigned char actual = 0;
            if(Read(bus, address, 0x07, &actual, 1) == 0 && actual == value)
            {
                return;
            }
            throw std::runtime_error("KFA2 motherboard sync readback failed");
        }
        if(status != -1)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200 * (attempt + 1)));
    }
    throw std::runtime_error("KFA2 motherboard sync write failed");
}

void RGBController_KFA2EX::ApplyColour()
{
    unsigned char rgb[3] = { (unsigned char)RGBGetRValue(colors[0]),
                             (unsigned char)RGBGetGValue(colors[0]),
                             (unsigned char)RGBGetBValue(colors[0]) };
    Write(0x02, rgb, 3);
    WriteByte(0x05, 0x00);
    WriteByte(0x06, 0x20);
    WriteByte(0x08, 0x5A);
}

void RGBController_KFA2EX::DeviceUpdateLEDs()
{
    std::lock_guard<std::mutex> lock(io_mutex);
    if(active_mode != 0)
    {
        return;
    }
    try
    {
        ApplyColour();
    }
    catch(const std::exception& error)
    {
        LOG_ERROR("[KFA2 EX] %s", error.what());
    }
}

void RGBController_KFA2EX::DeviceUpdateZoneLEDs(int zone)
{
    if(zone == 0)
    {
        DeviceUpdateLEDs();
    }
}
void RGBController_KFA2EX::DeviceUpdateSingleLED(int led)
{
    if(led == 0)
    {
        DeviceUpdateLEDs();
    }
}

void RGBController_KFA2EX::DeviceUpdateMode()
{
    std::lock_guard<std::mutex> lock(io_mutex);
    try
    {
        SetSync(active_mode == 1 ? 1 : 0);
        if(active_mode == 0)
        {
            ApplyColour();
        }
    }
    catch(const std::exception& error)
    {
        LOG_ERROR("[KFA2 EX] %s", error.what());
    }
}
