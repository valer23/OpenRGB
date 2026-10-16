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
#include "GalaxGPUv1Controller.h"

GalaxGPUv1Controller::GalaxGPUv1Controller(i2c_smbus_interface* bus, galax_gpu_dev_id dev, std::string dev_name, bool sg)
{
    this->bus   = bus;
    this->dev   = dev;
    this->name  = dev_name;
    this->sg    = sg;
}

GalaxGPUv1Controller::~GalaxGPUv1Controller()
{

}

int GalaxGPUv1Controller::ReadSG(i2c_smbus_interface* bus, galax_gpu_dev_id dev, unsigned char reg,
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

bool GalaxGPUv1Controller::TestForSG(i2c_smbus_interface* bus, galax_gpu_dev_id dev)
{
    unsigned char id[2] = {};
    return ReadSG(bus, dev, 0x00, id, 2) == 0 && id[0] == 0x11 && id[1] == 0x00;
}

bool GalaxGPUv1Controller::IsSG()
{
    return sg;
}

bool GalaxGPUv1Controller::GetLEDColors(unsigned char& red, unsigned char& green, unsigned char& blue)
{
    if(sg)
    {
        unsigned char rgb[3] = {};
        if(ReadSG(bus, dev, GALAX_V1_RED_REGISTER, rgb, 3) != 0)
        {
            return(false);
        }
        red     = rgb[0];
        green   = rgb[1];
        blue    = rgb[2];
    }
    else
    {
        red     = GetLEDRed();
        green   = GetLEDGreen();
        blue    = GetLEDBlue();
    }
    return(true);
}

int GalaxGPUv1Controller::GetSGSync()
{
    unsigned char sync = 0;
    sync_mode = ReadSG(bus, dev, GALAX_V1_SG_SYNC_REGISTER, &sync, 1) == 0 ? sync : -1;
    return(sync_mode);
}

bool GalaxGPUv1Controller::WriteSG(unsigned char reg, unsigned char* data, unsigned int length)
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
        return(false);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    return(true);
}

bool GalaxGPUv1Controller::WriteSGByte(unsigned char reg, unsigned char value)
{
    return(WriteSG(reg, &value, 1));
}

bool GalaxGPUv1Controller::SetSGSync(unsigned char value)
{
    if(sync_mode == value)
    {
        return(true);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    for(unsigned int attempt = 0; attempt < 3; attempt++)
    {
        int status = bus->i2c_smbus_write_byte_data(dev, GALAX_V1_SG_SYNC_REGISTER, value);
        if(status == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if(GetSGSync() == value)
            {
                return(true);
            }
        }
        if(status != 0 && status != -1)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200 * (attempt + 1)));
    }
    sync_mode = -1;
    return(false);
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

bool GalaxGPUv1Controller::SetMode(unsigned char mode, unsigned char red, unsigned char green, unsigned char blue)
{
    if(sg)
    {
        if(mode == GALAX_V1_SG_SYNC_MODE)
        {
            return(SetSGSync(1));
        }
        if(mode != 1 || !SetSGSync(0))
        {
            return(false);
        }
        unsigned char rgb[3] = { red, green, blue };
        return(WriteSG(GALAX_V1_RED_REGISTER, rgb, 3)
            && WriteSGByte(GALAX_V1_MODE_REGISTER_1, GALAX_V1_MODE_STATIC_VALUE_1)
            && WriteSGByte(GALAX_V1_MODE_REGISTER_2, GALAX_V1_SG_STATIC_VALUE_2)
            && WriteSGByte(GALAX_V1_SG_APPLY_REGISTER, GALAX_V1_SG_APPLY_VALUE));
    }

    unsigned char mode_reg_1;
    unsigned char mode_reg_2;
    switch(mode)
    {
    case 1:
        mode_reg_1 = GALAX_V1_MODE_STATIC_VALUE_1;
        mode_reg_2 = GALAX_V1_MODE_STATIC_VALUE_2;
        break;

    case 2:
        mode_reg_1 = GALAX_V1_MODE_BREATHING_VALUE_1;
        mode_reg_2 = GALAX_V1_MODE_BREATHING_VALUE_2;
        break;

    case 3:
        mode_reg_1 = GALAX_V1_MODE_RAINBOW_VALUE_1;
        mode_reg_2 = GALAX_V1_MODE_RAINBOW_VALUE_2;
        break;

    case 4:
        mode_reg_1 = GALAX_V1_MODE_CYCLE_BREATHING_VALUE_1;
        mode_reg_2 = GALAX_V1_MODE_CYCLE_BREATHING_VALUE_2;
        break;

    default:
        return(false);
    }

    /*-----------------------------------------------------*\
    | Apply the mode after each colour update.              |
    \*-----------------------------------------------------*/
    return(bus->i2c_smbus_write_byte_data(dev, GALAX_V1_RED_REGISTER, red) == 0
        && bus->i2c_smbus_write_byte_data(dev, GALAX_V1_GREEN_REGISTER, green) == 0
        && bus->i2c_smbus_write_byte_data(dev, GALAX_V1_BLUE_REGISTER, blue) == 0
        && bus->i2c_smbus_write_byte_data(dev, GALAX_V1_MODE_REGISTER_1, mode_reg_1) == 0
        && bus->i2c_smbus_write_byte_data(dev, GALAX_V1_MODE_REGISTER_2, mode_reg_2) == 0);
}

unsigned char GalaxGPUv1Controller::GalaxGPURegisterRead(unsigned char reg)
{
    return(bus->i2c_smbus_read_byte_data(dev, reg));
}

void GalaxGPUv1Controller::GalaxGPURegisterWrite(unsigned char reg, unsigned char val)
{
    bus->i2c_smbus_write_byte_data(dev, reg, val);
}
