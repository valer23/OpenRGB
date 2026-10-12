/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_KFA2EX_Windows.h"
#include "LogManager.h"
#include <chrono>
#include <stdexcept>
#include <thread>

NV_STATUS RGBController_KFA2EX::Transfer(NV_PHYSICAL_GPU_HANDLE gpu, unsigned char reg,
                                        unsigned char* data, unsigned int length, bool write)
{
    NV_I2C_INFO_V3 info;
    info.display_mask    = 1;
    info.is_ddc_port     = 2; // Exact vendor value; do not normalise to bool.
    info.i2c_dev_address = 0x46;
    info.i2c_reg_address = &reg;
    info.reg_addr_size   = 1;
    info.data            = data;
    info.size            = length;
    info.i2c_speed       = 0;
    info.i2c_speed_khz   = static_cast<NV_I2C_SPEED>(0);
    info.port_id         = 1;
    info.is_port_id_set  = 1;
    return write ? NvAPI_I2CWrite(gpu, &info) : NvAPI_I2CRead(gpu, &info);
}

bool RGBController_KFA2EX::Probe(NV_PHYSICAL_GPU_HANDLE gpu)
{
    unsigned char id[2] = {};
    return Transfer(gpu, 0x00, id, 2, false) == 0 && id[0] == 0x11 && id[1] == 0x00;
}

RGBController_KFA2EX::RGBController_KFA2EX(NV_PHYSICAL_GPU_HANDLE gpu_handle) : gpu(gpu_handle)
{
    name        = "KFA2 GeForce RTX 3080 Ti EX Gamer";
    vendor      = "KFA2";
    type        = DEVICE_TYPE_GPU;
    description = "Captured KFA2 EX lighting protocol (Windows NVAPI)";
    NV_U32 bus_id = 0;
    NV_U32 slot_id = 0;
    if(NvAPI_GPU_GetBusId(gpu, &bus_id) != 0 || NvAPI_GPU_GetBusSlotId(gpu, &slot_id) != 0)
    {
        throw std::runtime_error("Unable to read KFA2 PCI location");
    }
    location = "NVAPI: PCI bus " + std::to_string(bus_id) + ", slot " + std::to_string(slot_id)
             + ", I2C address 0x23, port 1";

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
    if(Transfer(gpu, 0x07, &sync, 1, false) != 0 || sync > 1)
    {
        throw std::runtime_error("Unable to read KFA2 motherboard sync state");
    }
    active_mode = sync;
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
    if(Transfer(gpu, 0x02, rgb, 3, false) != 0)
    {
        throw std::runtime_error("Unable to read KFA2 colour");
    }
    colors[0] = ToRGBColor(rgb[0], rgb[1], rgb[2]);
}

void RGBController_KFA2EX::Write(unsigned char reg, unsigned char* data, unsigned int length)
{
    if(Transfer(gpu, reg, data, length, true) != 0)
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
        NV_STATUS status = Transfer(gpu, 0x07, &value, 1, true);
        if(status == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            unsigned char actual = 0;
            if(Transfer(gpu, 0x07, &actual, 1, false) == 0 && actual == value)
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
    unsigned char rgb[3] = { RGBGetRValue(colors[0]), RGBGetGValue(colors[0]), RGBGetBValue(colors[0]) };
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

void RGBController_KFA2EX::DeviceUpdateZoneLEDs(int /*zone*/) { DeviceUpdateLEDs(); }
void RGBController_KFA2EX::DeviceUpdateSingleLED(int /*led*/) { DeviceUpdateLEDs(); }

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
