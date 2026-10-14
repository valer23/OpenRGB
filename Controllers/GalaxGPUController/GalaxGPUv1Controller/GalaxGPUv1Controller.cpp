/*---------------------------------------------------------*\
| GalaxGPUv1Controller.cpp                                  |
|                                                           |
|   Driver for Galax/KFA2 GPU                               |
|                                                           |
|   Niels Westphal (crashniels)                 12 Jul 2020 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include <chrono>
#include <thread>
#include <stdexcept>
#include "GalaxGPUv1Controller.h"

GalaxGPUv1Controller::GalaxGPUv1Controller(i2c_smbus_interface* bus, galax_gpu_dev_id dev, std::string dev_name, bool ex_gamer)
{
    this->bus   = bus;
    this->dev   = dev;
    this->name  = dev_name;
    this->ex_gamer = ex_gamer;
}

GalaxGPUv1Controller::~GalaxGPUv1Controller()
{

}

int GalaxGPUv1Controller::ReadEX(i2c_smbus_interface* bus, galax_gpu_dev_id dev, unsigned char reg,
                                unsigned char* data, unsigned int length)
{
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        if(bus->i2c_smbus_read_i2c_block_data(dev, reg, length, data) == (int)length)
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

bool GalaxGPUv1Controller::TestForEXGamer(i2c_smbus_interface* bus, galax_gpu_dev_id dev)
{
    unsigned char id[2] = {};
    return ReadEX(bus, dev, 0x00, id, 2) == 0 && id[0] == 0x11 && id[1] == 0x00;
}

bool GalaxGPUv1Controller::IsEXGamer()
{
    return ex_gamer;
}

void GalaxGPUv1Controller::GetLEDColors(unsigned char& red, unsigned char& green, unsigned char& blue)
{
    if(ex_gamer)
    {
        unsigned char rgb[3] = {};
        if(ReadEX(bus, dev, GALAX_V1_RED_REGISTER, rgb, 3) != 0)
        {
            throw std::runtime_error("Unable to read KFA2 EX colour");
        }
        red = rgb[0];
        green = rgb[1];
        blue = rgb[2];
    }
    else
    {
        red = GetLEDRed();
        green = GetLEDGreen();
        blue = GetLEDBlue();
    }
}

int GalaxGPUv1Controller::GetEXSync()
{
    unsigned char sync = 0;
    return ReadEX(bus, dev, GALAX_V1_EX_SYNC_REGISTER, &sync, 1) == 0 ? sync : -1;
}

void GalaxGPUv1Controller::WriteEX(unsigned char reg, unsigned char* data, unsigned int length)
{
    int status = -1;
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        status = bus->i2c_smbus_write_i2c_block_data(dev, reg, length, data);
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
        throw std::runtime_error("KFA2 EX write failed at register " + std::to_string(reg));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
}

void GalaxGPUv1Controller::WriteEXByte(unsigned char reg, unsigned char value)
{
    WriteEX(reg, &value, 1);
}

void GalaxGPUv1Controller::SetEXSync(unsigned char value)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        int status = bus->i2c_smbus_write_byte_data(dev, GALAX_V1_EX_SYNC_REGISTER, value);
        if(status == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if(GetEXSync() == value)
            {
                return;
            }
            throw std::runtime_error("KFA2 EX sync readback failed");
        }
        if(status != -1)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200 * (attempt + 1)));
    }
    throw std::runtime_error("KFA2 EX sync write failed");
}

std::string GalaxGPUv1Controller::GetDeviceLocation()
{
    std::string return_string(bus->info.device_name);
    char addr[5];
    snprintf(addr, 5, "0x%02X", dev);
    return_string.append(", address ");
    return_string.append(addr);
    return("I2C: " + return_string);
}

std::string GalaxGPUv1Controller::GetDeviceName()
{
    return(name);
}

unsigned char GalaxGPUv1Controller::GetLEDRed()
{
    return(GalaxGPURegisterRead(GALAX_V1_RED_REGISTER));
}

unsigned char GalaxGPUv1Controller::GetLEDGreen()
{
    return(GalaxGPURegisterRead(GALAX_V1_GREEN_REGISTER));
}

unsigned char GalaxGPUv1Controller::GetLEDBlue()
{
    return(GalaxGPURegisterRead(GALAX_V1_BLUE_REGISTER));
}

void GalaxGPUv1Controller::SetLEDColorsDirect(unsigned char red, unsigned char green, unsigned char blue)                  // Direct Mode is just Static Mode without applying color changes
{
    if(ex_gamer)
    {
        unsigned char rgb[3] = {red, green, blue};
        WriteEX(GALAX_V1_RED_REGISTER, rgb, 3);
        WriteEXByte(GALAX_V1_MODE_REGISTER_1, GALAX_V1_MODE_STATIC_VALUE_1);
        WriteEXByte(GALAX_V1_MODE_REGISTER_2, GALAX_V1_EX_STATIC_VALUE_2);
        WriteEXByte(GALAX_V1_EX_APPLY_REGISTER, GALAX_V1_EX_APPLY_VALUE);
        return;
    }
    GalaxGPURegisterWrite(GALAX_V1_RED_REGISTER, red);
    GalaxGPURegisterWrite(GALAX_V1_GREEN_REGISTER, green);
    GalaxGPURegisterWrite(GALAX_V1_BLUE_REGISTER, blue);
}

void GalaxGPUv1Controller::SetLEDColorsEffect(unsigned char red, unsigned char green, unsigned char blue)
{
    GalaxGPURegisterWrite(GALAX_V1_RED_REGISTER, red);
    GalaxGPURegisterWrite(GALAX_V1_GREEN_REGISTER, green);
    GalaxGPURegisterWrite(GALAX_V1_BLUE_REGISTER, blue);
}

void GalaxGPUv1Controller::SetMode(unsigned char mode)
{
    if(ex_gamer)
    {
        if(mode == 1 || mode == GALAX_V1_EX_SYNC_MODE)
        {
            SetEXSync(mode == GALAX_V1_EX_SYNC_MODE ? 1 : 0);
        }
        return;
    }
    switch(mode)
    {
    case 1:
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_1, GALAX_V1_MODE_STATIC_VALUE_1);
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_2, GALAX_V1_MODE_STATIC_VALUE_2);
        break;

    case 2:
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_1, GALAX_V1_MODE_BREATHING_VALUE_1);
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_2, GALAX_V1_MODE_BREATHING_VALUE_2);
        break;

    case 3:
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_1, GALAX_V1_MODE_RAINBOW_VALUE_1);
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_2, GALAX_V1_MODE_RAINBOW_VALUE_2);
        break;

    case 4:
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_1, GALAX_V1_MODE_CYCLE_BREATHING_VALUE_1);
        GalaxGPURegisterWrite(GALAX_V1_MODE_REGISTER_2, GALAX_V1_MODE_CYCLE_BREATHING_VALUE_2);
        break;

    default:
        break;
    }
}

unsigned char GalaxGPUv1Controller::GalaxGPURegisterRead(unsigned char reg)
{
    return(bus->i2c_smbus_read_byte_data(dev, reg));
}

void GalaxGPUv1Controller::GalaxGPURegisterWrite(unsigned char reg, unsigned char val)
{
    bus->i2c_smbus_write_byte_data(dev, reg, val);
}
