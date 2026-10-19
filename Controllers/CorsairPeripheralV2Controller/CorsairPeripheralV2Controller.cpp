/*---------------------------------------------------------*\
| CorsairPeripheralV2Controller.cpp                         |
|                                                           |
|   Driver for Corsair V2 peripherals                       |
|                                                           |
|   Chris M (Dr_No)                             07 Aug 2022 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <chrono>
#include <cwchar>
#include <thread>

#include "CorsairPeripheralV2Controller.h"
#include "StringUtils.h"
#ifdef _WIN32
#include <hidapi_winapi.h>
#endif
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;

static unsigned long long CorsairMouseTime()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

CorsairPeripheralV2Controller::CorsairPeripheralV2Controller(hid_device* dev_handle, const char* path, std::string name)
{
    dev                 = dev_handle;
    location            = path;
    device_name         = name;
    device_index        = CORSAIR_V2_DEVICE_NOT_FOUND;

    /*---------------------------------------------------------*\
    | Get PID                                                   |
    |   If the PID is in the know wireless receivers list       |
    |   switch the write_cmd to talk to the device and retry    |
    \*---------------------------------------------------------*/
    unsigned int pid    = GetAddressRetry(0x12);
    mouse_receiver_pid  = pid;

    switch(pid)
    {
        case CORSAIR_SLIPSTREAM_WIRELESS_PID1:
        case CORSAIR_SLIPSTREAM_WIRELESS_V2_PID1:
        case CORSAIR_SLIPSTREAM_WIRELESS_PID2:
        case CORSAIR_SLIPSTREAM_M75_PID:
        case CORSAIR_SLIPSTREAM_IRONCLAW_SE_PID:
            write_cmd   = CORSAIR_V2_WRITE_WIRELESS_ID;
            pid         = GetAddressRetry(0x12);
            break;

        case CORSAIR_K57_RGB_WIRED_PID:
            write_cmd   = 0x80;
            light_ctrl  = CORSAIR_V2_LIGHT_CTRL1;
            skip_reads  = true;
            break;
    }

    /*---------------------------------------------------------*\
    | If the hid_pid passed in from the detector does not match |
    |   the pid reported by the device then it is likey         |
    |   behind a wireless receiver.                             |
    \*---------------------------------------------------------*/
    LOG_DEBUG("[%s] Setting write CMD to %02X for %s mode for PID %04X", device_name.c_str(),
              write_cmd, (write_cmd == CORSAIR_V2_WRITE_WIRELESS_ID) ? "wireless" : "wired", pid);

    /*---------------------------------------------------------*\
    | Get VID                                                   |
    |   NB: this can be achieved with GetAddress(0x11) but we   |
    |   also need to set the packet length capabilities for     |
    |   the device being set up.                                |
    \*---------------------------------------------------------*/
    uint8_t buffer[CORSAIR_V2_PACKET_SIZE];
    buffer[1] = write_cmd;
    buffer[2] = CORSAIR_V2_CMD_GET;
    buffer[3] = 0x11;
    hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE);
    hid_read_timeout(dev, buffer, CORSAIR_V2_PACKET_SIZE, CORSAIR_V2_TIMEOUT);

    /*---------------------------------------------------------*\
    | NB: If the device is not found in the device list         |
    |   then wireless mode may not work reliably                |
    \*---------------------------------------------------------*/
    bool not_found          = true;

    for(uint16_t i = 0; i < CORSAIR_V2_DEVICE_COUNT; i++)
    {
        LOG_DEBUG("[%s] Checking PID %04X against index %d with %04X - %smatch", device_name.c_str(),
                  pid, i, corsair_v2_device_list[i]->pid, corsair_v2_device_list[i]->pid == pid ? "" : "no ");
        if(corsair_v2_device_list[i]->pid == pid)
        {
            /*---------------------------------------------------------*\
            | Set device ID                                             |
            \*---------------------------------------------------------*/
            not_found       = false;
            device_index    = i;
            break;
        }
    }

    /*---------------------------------------------------------*\
    | Unknown PID                                               |
    |   Leave device_index at CORSAIR_V2_DEVICE_NOT_FOUND so    |
    |   GetDeviceData() returns nullptr and the detector drops  |
    |   the device, rather than building an RGBController on    |
    |   top of an out of range index.                           |
    \*---------------------------------------------------------*/
    if(not_found)
    {
        LOG_ERROR("[%s] device capabilities not found. Please creata a new device request.",
                  device_name.c_str());
        return;
    }

    /*---------------------------------------------------------*\
    | Check lighting control endpoints                          |
    |   If lighting control endpoint 2 is unavailable           |
    |   then use endpoint 1.                                    |
    |   NB: the K100 answers both endpoints, but its software   |
    |   direct lighting only renders through endpoint 2, so     |
    |   it must stay on CORSAIR_V2_LIGHT_CTRL2.                 |
    \*---------------------------------------------------------*/
    if(light_ctrl == CORSAIR_V2_LIGHT_CTRL2)
    {
        switch(pid)
        {
            case CORSAIR_K100_OPTICAL_V1_PID:
            case CORSAIR_K100_OPTICAL_V2_PID:
            case CORSAIR_K100_MXRED_PID:
                LOG_DEBUG("[%s] Lighting Endpoint pinned to %02X for K100", device_name.c_str(), light_ctrl);
                break;

            default:
                uint8_t result  = StartTransaction(0);
                if(result > 0)
                {
                    light_ctrl = CORSAIR_V2_LIGHT_CTRL1;
                    StartTransaction(0);
                }
                StopTransaction(0);
                LOG_DEBUG("[%s] Lighting Endpoint set to %02X", device_name.c_str(), light_ctrl);
                break;
        }
    }

}

CorsairPeripheralV2Controller::~CorsairPeripheralV2Controller()
{
    /*---------------------------------------------------------*\
    | Hand rendering back to the device's onboard firmware      |
    |   before releasing it.  Some devices (e.g. K70 Core RGB)  |
    |   stop generating onboard input events (e.g. the volume   |
    |   knob) while left in software render mode, and the mode  |
    |   persists until the next USB enumeration if it is never  |
    |   reverted.                                               |
    \*---------------------------------------------------------*/
    if(!mouse_idle_enabled || mouse_connected)
    {
        SetRenderMode(CORSAIR_V2_MODE_HW);
    }
    if(mouse_events)
    {
        hid_close(mouse_events);
    }

    hid_close(dev);
}

const corsair_v2_device* CorsairPeripheralV2Controller::GetDeviceData()
{
    if(!IsDeviceSupported())
    {
        return nullptr;
    }

    return corsair_v2_device_list[device_index];
}

bool CorsairPeripheralV2Controller::IsDeviceSupported()
{
    return device_index < CORSAIR_V2_DEVICE_COUNT;
}

std::string CorsairPeripheralV2Controller::GetDeviceLocation()
{
    return("HID: " + location);
}

std::string CorsairPeripheralV2Controller::GetErrorString(uint8_t err)
{
    switch(err)
    {
        case 1:
            return "Invalid Value";
        case 3:
            return "Failed";
        case 5:
            return "Unsupported";
        default:
            return "Protocol Error (Unknown)";
    }
}

std::string CorsairPeripheralV2Controller::GetFirmwareString()
{
    return "";
}

std::string CorsairPeripheralV2Controller::GetName()
{
    return device_name;
}

std::string CorsairPeripheralV2Controller::GetSerialString()
{
    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(serial_string));
}

void CorsairPeripheralV2Controller::SetRenderMode(corsair_v2_device_mode mode)
{
    SetRenderModeChecked(mode);
}

bool CorsairPeripheralV2Controller::SetRenderModeChecked(corsair_v2_device_mode mode)
{
    uint8_t buffer[CORSAIR_V2_WRITE_SIZE];

    memset(buffer, 0, CORSAIR_V2_WRITE_SIZE);

    /*---------------------------------------------------------*\
    | Set Mode                                                  |
    \*---------------------------------------------------------*/
    buffer[1]   = write_cmd;
    buffer[2]   = CORSAIR_V2_CMD_SET;
    buffer[3]   = CORSAIR_V2_VALUE_MODE;
    buffer[5]   = mode;

    if(hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE) != CORSAIR_V2_WRITE_SIZE)
    {
        return false;
    }
    if(skip_reads)
    {
        return true;
    }
    int count = hid_read_timeout(dev, buffer, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT);
    return count >= 3 && buffer[2] == 0;
}

void CorsairPeripheralV2Controller::LightingControl(uint8_t opt1)
{
    LightingControlChecked(opt1);
}

bool CorsairPeripheralV2Controller::LightingControlChecked(uint8_t opt1)
{
    uint8_t buffer[CORSAIR_V2_WRITE_SIZE];

    memset(buffer, 0, CORSAIR_V2_WRITE_SIZE);

    /*---------------------------------------------------------*\
    | The Corsair command is the same for each initialisation   |
    |   packet and the registers and options differ for         |
    |   each peripheral supported by the protocol               |
    \*---------------------------------------------------------*/
    buffer[1]   = write_cmd;
    buffer[2]   = CORSAIR_V2_CMD_GET;
    buffer[3]   = opt1;
    buffer[5]   = 0x00;

    if(hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE) != CORSAIR_V2_WRITE_SIZE)
    {
        return false;
    }
    if(skip_reads)
    {
        return true;
    }
    int count = hid_read_timeout(dev, buffer, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT);
    return count >= 3 && buffer[2] == 0;
}

unsigned int CorsairPeripheralV2Controller::GetKeyboardLayout()
{
    return GetAddress(0x41);
}

/*---------------------------------------------------------*\
| GetAddress returns NA when the device answers with a      |
|   protocol error, and a receiver whose device has not     |
|   woken answers zero.  Straight after enumeration both    |
|   are normal, so poll before treating either as final.    |
\*---------------------------------------------------------*/
unsigned int CorsairPeripheralV2Controller::GetAddressRetry(uint8_t address)
{
    unsigned int value = NA;

    for(unsigned int attempt = 0; attempt < CORSAIR_V2_PID_RETRIES; attempt++)
    {
        value = GetAddress(address);

        if(value != NA && value != 0)
        {
            return value;
        }

        if(attempt + 1 < CORSAIR_V2_PID_RETRIES)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(CORSAIR_V2_PID_RETRY_DELAY));
        }
    }

    return value;
}

unsigned int CorsairPeripheralV2Controller::GetAddress(uint8_t address)
{
    uint8_t buffer[CORSAIR_V2_WRITE_SIZE];
    uint8_t read[CORSAIR_V2_WRITE_SIZE];

    memset(buffer, 0, CORSAIR_V2_WRITE_SIZE);
    memset(read, 0, CORSAIR_V2_WRITE_SIZE);

    buffer[1]   = write_cmd;
    buffer[2]   = CORSAIR_V2_CMD_GET;
    buffer[3]   = address;

    hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE);
    hid_read_timeout(dev, read, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT);

    unsigned int temp = (unsigned int)(read[6] << 24 | read[5] << 16 | read[4] << 8 | read[3]);
    LOG_DEBUG("[%s] GetAddress %02X - %02X %02X - %02X %02X %02X %02X   %02X %02X %02X %02X", device_name.c_str(),
              address, read[0], read[1], read[2], read[3], read[4], read[5], read[6], read[7], read[8], read[9]);

    uint8_t result = read[2];
    if(result > 0)
    {
        LOG_DEBUG("[%s] An error occurred! Get Address %02X failed - %d %s", device_name.c_str(),
                  address, result, GetErrorString(result).c_str());
        return -1;
    }
    return temp;
}

unsigned char CorsairPeripheralV2Controller::StartTransaction(uint8_t opt1)
{
    uint8_t buffer[CORSAIR_V2_WRITE_SIZE];

    memset(buffer, 0, CORSAIR_V2_WRITE_SIZE);

    buffer[1]   = write_cmd;
    buffer[2]   = CORSAIR_V2_CMD_START_TX;
    buffer[3]   = opt1;
    buffer[4]   = light_ctrl;

    hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE);

    if(!skip_reads)
    {
        hid_read_timeout(dev, buffer, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT);
    }

    return buffer[2];
}

void CorsairPeripheralV2Controller::StopTransaction(uint8_t opt1)
{
    uint8_t buffer[CORSAIR_V2_WRITE_SIZE];

    memset(buffer, 0, CORSAIR_V2_WRITE_SIZE);

    buffer[1]   = write_cmd;
    buffer[2]   = CORSAIR_V2_CMD_STOP_TX;
    buffer[3]   = 0x01;
    buffer[4]   = opt1;

    hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE);

    if(!skip_reads)
    {
        hid_read_timeout(dev, buffer, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT);
    }
}

void CorsairPeripheralV2Controller::ClearPacketBuffer()
{
    if(skip_reads)
    {
        return;
    }

    int result              = 0;
    uint8_t buffer[CORSAIR_V2_PACKET_SIZE];

    do
    {
        result = hid_read_timeout(dev, buffer, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT_SHORT);
    }
    while(result > 0);
}

void CorsairPeripheralV2Controller::SetLEDs(uint8_t *data, uint16_t data_size)
{
    std::lock_guard<std::mutex> mouse_guard(mouse_lighting_mutex);
    if(mouse_idle_enabled)
    {
        unsigned long long now = CorsairMouseTime();
        ReadMouseEvents(now);
        if(!mouse_events)
        {
            return;
        }
        if(mouse_connection_sync)
        {
            if(now < mouse_next_query_ms)
            {
                return;
            }
            mouse_next_query_ms = now + CORSAIR_V2_MOUSE_QUERY_PERIOD;
            if(!ReadMouseConnection())
            {
                return;
            }
            mouse_connection_sync = false;
            mouse_next_query_ms = 0;
        }
        if(!mouse_connected)
        {
            return;
        }
        if(now >= mouse_next_query_ms)
        {
            mouse_next_query_ms = now + CORSAIR_V2_MOUSE_QUERY_PERIOD;
            mouse_inactivity_valid = ReadMouseInactivity();
            ReadMouseEvents(now);
            if(!mouse_events || mouse_connection_sync || !mouse_connected)
            {
                return;
            }
        }
        if(!mouse_idle_policy.AllowFrame(MouseIsIdle(), now,
            [this](bool software) { return ChangeMouseOwnership(software); }))
        {
            return;
        }
        mouse_last_frame_ms = now;
    }
    const uint8_t offset1   = 8;
    const uint8_t offset2   = 4;
    uint16_t remaining      = data_size;

    uint8_t buffer[CORSAIR_V2_PACKET_SIZE];
    uint8_t response[CORSAIR_V2_PACKET_SIZE];
    memset(buffer, 0, CORSAIR_V2_PACKET_SIZE);
    memset(response, 0, CORSAIR_V2_PACKET_SIZE);

    ClearPacketBuffer();
    StartTransaction(0);
    /*---------------------------------------------------------*\
    | Set the data header in packet 1 with the data length      |
    |   signaling how many packets to expect to the device      |
    \*---------------------------------------------------------*/
    buffer[1]               = write_cmd;
    buffer[2]               = CORSAIR_V2_CMD_BLK_W1;
    buffer[4]               = data_size & 0xFF;
    buffer[5]               = data_size >> 8;

    /*---------------------------------------------------------*\
    | Check if the data needs more than 1 packet                |
    \*---------------------------------------------------------*/
    uint16_t copy_bytes     = CORSAIR_V2_WRITE_SIZE - offset1;
    if(remaining < copy_bytes)
    {
        copy_bytes          = remaining;
    }

    memcpy(&buffer[offset1], &data[0], copy_bytes);

    hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE);

    if(!skip_reads)
    {
        /*-----------------------------------------------------*\
        | Some devices (e.g. K100) drop packets that arrive     |
        | back to back.  Wait for the transaction response      |
        | before sending the next packet.                       |
        \*-----------------------------------------------------*/
        if(hid_read_timeout(dev, response, CORSAIR_V2_WRITE_SIZE, CORSAIR_V2_TIMEOUT_SHORT) <= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(CORSAIR_V2_WRITE_PACE));
        }
    }

    remaining              -= copy_bytes;
    buffer[2]               = CORSAIR_V2_CMD_BLK_WN;
    copy_bytes              = CORSAIR_V2_WRITE_SIZE - offset2;

    /*---------------------------------------------------------*\
    | Send the remaining packets                                |
    \*---------------------------------------------------------*/
    while(remaining)
    {
        uint16_t index      = data_size - remaining;
        if(remaining < copy_bytes)
        {
            memset(&buffer[offset2], 0, copy_bytes);
            copy_bytes      = remaining;
        }

        memcpy(&buffer[offset2], &data[index], copy_bytes);

        hid_write(dev, buffer, CORSAIR_V2_WRITE_SIZE);

        /*-----------------------------------------------------*\
        | Pace the transfer: the device does not acknowledge    |
        |   continuation packets, so a short sleep keeps the    |
        |   MCU from being flooded.                             |
        \*-----------------------------------------------------*/
        std::this_thread::sleep_for(std::chrono::milliseconds(CORSAIR_V2_WRITE_PACE));

        remaining          -= copy_bytes;
    }

    StopTransaction(0);
}

void CorsairPeripheralV2Controller::UpdateHWMode(uint16_t mode, corsair_v2_color /*color_mode*/, uint8_t /*speed*/,
                                                 uint8_t /*direction*/, uint8_t /*brightness*/, std::vector<RGBColor> /*colors*/)
{
    /*---------------------------------------------------------*\
    | If we are switching to `Direct` mode                      |
    |   set device in software mode                             |
    \*---------------------------------------------------------*/
    if(mode == CORSAIR_V2_MODE_DIRECT)
    {
        SetRenderMode(CORSAIR_V2_MODE_SW);
        return;
    }

    /*
    SetRenderMode(CORSAIR_V2_MODE_HW);

    uint8_t buffer[CORSAIR_V2_WRITE_SIZE];
    memset(buffer, 0, CORSAIR_V2_WRITE_SIZE);
    */

    /*---------------------------------------------------------*\
    | Set the data header in packet 1 with the data length      |
    |   signaling how many packets to expect to the device      |
    \*---------------------------------------------------------*/

    /*
    buffer[1]               = write_cmd;
    buffer[2]               = CORSAIR_V2_CMD_BLK_W1;
    buffer[3]               = CORSAIR_V2_MODE_HW;
    buffer[4]               = 0x30;
    buffer[8]               = mode & 0xFF;
    buffer[9]               = mode >> 8;

    buffer[10]              = CORSAIR_V2_COLOR_SPECIFIC;
    buffer[11]              = speed;

    buffer[14]              = colors.size();

    for(size_t i = 0; i < colors.size(); ++i)
    {
        uint8_t offset      = 15 + (i * 4);

        buffer[offset]      = brightness;
        buffer[offset + 1]  = RGBGetBValue(colors[i]);
        buffer[offset + 2]  = RGBGetGValue(colors[i]);
        buffer[offset + 3]  = RGBGetRValue(colors[i]);
    }
    */

}

hid_device* CorsairPeripheralV2Controller::OpenMouseEvents()
{
#ifdef _WIN32
    GUID container = {};
    if(hid_winapi_get_container_id(dev, &container) != 0 || container == GUID{})
    {
        return nullptr;
    }
#else
    wchar_t serial[HID_MAX_STR] = {};
    if(hid_get_serial_number_string(dev, serial, HID_MAX_STR) != 0 || !serial[0])
    {
        return nullptr;
    }
#endif
    hid_device* result = nullptr;
    hid_device_info* devices = hid_enumerate(0x1B1C, mouse_receiver_pid);
    for(hid_device_info* item = devices; item; item = item->next)
    {
        if(item->interface_number != 2 || item->usage_page != 0xFF42 || item->usage != 2)
        {
            continue;
        }
#ifndef _WIN32
        if(!item->serial_number || std::wcscmp(serial, item->serial_number) != 0)
        {
            continue;
        }
#endif
        hid_device* candidate = hid_open_path(item->path);
        if(!candidate)
        {
            continue;
        }
#ifdef _WIN32
        GUID candidate_container = {};
        if(hid_winapi_get_container_id(candidate, &candidate_container) != 0 || candidate_container != container)
        {
            hid_close(candidate);
            continue;
        }
#endif
        /*---------------------------------------------------------*\
        | Refuse an ambiguous sibling rather than another receiver.|
        \*---------------------------------------------------------*/
        if(result)
        {
            hid_close(candidate);
            hid_close(result);
            result = nullptr;
            break;
        }
        result = candidate;
    }
    hid_free_enumeration(devices);
    return result;
}

void CorsairPeripheralV2Controller::SetMouseConnected(bool connected)
{
    if(connected != mouse_connected)
    {
        mouse_connected = connected;
        mouse_activity_after_handoff = connected;
        mouse_inactivity_valid = false;
        mouse_next_query_ms = 0;
        mouse_idle_policy.RequireSoftwareMode();
        LOG_DEBUG("[%s] Wireless mouse %s", device_name.c_str(), connected ? "connected" : "disconnected");
    }
}

void CorsairPeripheralV2Controller::ReadMouseEvents(unsigned long long now)
{
    if(!mouse_events)
    {
        if(now < mouse_event_retry_ms)
        {
            return;
        }
        mouse_event_retry_ms = now + CORSAIR_V2_MOUSE_QUERY_PERIOD;
        mouse_events = OpenMouseEvents();
        if(!mouse_events)
        {
            return;
        }
        mouse_connection_sync = true;
        mouse_next_query_ms = 0;
        mouse_inactivity_valid = false;
        mouse_idle_policy.RequireSoftwareMode();
    }
    /*---------------------------------------------------------*\
    | Read only the sibling event endpoint. Bound each drain.  |
    | Byte four is a connected-child bitmask, not a status enum.|
    \*---------------------------------------------------------*/
    for(unsigned int i = 0; i < 8; i++)
    {
        unsigned char buffer[CORSAIR_V2_PACKET_SIZE] = {};
        int count = hid_read_timeout(mouse_events, buffer, sizeof(buffer), 0);
        if(count < 0)
        {
            hid_close(mouse_events);
            mouse_events = nullptr;
            mouse_event_retry_ms = now + CORSAIR_V2_MOUSE_QUERY_PERIOD;
            mouse_inactivity_valid = false;
            mouse_connection_sync = true;
            if(!mouse_event_failure_reported)
            {
                LOG_WARNING("[%s] Mouse event endpoint unavailable; lighting paused until recovery", device_name.c_str());
                mouse_event_failure_reported = true;
            }
            return;
        }
        if(count == 0)
        {
            return;
        }
        if(count >= 5 && buffer[0] == 0 && buffer[1] == 1 && buffer[2] == 0x36)
        {
            SetMouseConnected((buffer[4] & (1U << (write_cmd - CORSAIR_V2_WRITE_WIRED_ID))) != 0);
        }
    }
}

bool CorsairPeripheralV2Controller::ReadMouseConnection()
{
    /*---------------------------------------------------------*\
    | Query the receiver's child mask only on initialization   |
    | or event-reader recovery. This does not address the mouse.|
    \*---------------------------------------------------------*/
    ClearPacketBuffer();
    unsigned char request[CORSAIR_V2_WRITE_SIZE] = {};
    unsigned char response[CORSAIR_V2_PACKET_SIZE] = {};
    request[1] = CORSAIR_V2_WRITE_WIRED_ID;
    request[2] = CORSAIR_V2_CMD_GET;
    request[3] = 0x36;
    if(hid_write(dev, request, sizeof(request)) != sizeof(request))
    {
        return false;
    }
    int count = hid_read_timeout(dev, response, sizeof(response), CORSAIR_V2_TIMEOUT);
    if(count < 4 || response[0] != 0 || response[1] != CORSAIR_V2_CMD_GET || response[2] != 0)
    {
        return false;
    }
    SetMouseConnected((response[3] & (1U << (write_cmd - CORSAIR_V2_WRITE_WIRED_ID))) != 0);
    mouse_event_failure_reported = false;
    return true;
}

bool CorsairPeripheralV2Controller::ReadMouseInactivity()
{
    ClearPacketBuffer();
    unsigned char request[CORSAIR_V2_WRITE_SIZE] = {};
    unsigned char response[CORSAIR_V2_PACKET_SIZE] = {};
    request[1] = write_cmd;
    request[2] = CORSAIR_V2_CMD_GET;
    request[3] = 0x40;
    if(hid_write(dev, request, sizeof(request)) != sizeof(request))
    {
        return false;
    }
    int count = hid_read_timeout(dev, response, sizeof(response), CORSAIR_V2_TIMEOUT);
    if(count < 5 || response[0] != write_cmd - CORSAIR_V2_WRITE_WIRED_ID || response[1] != 2 || response[2] != 0)
    {
        return false;
    }
    mouse_inactivity_ms = (response[3] | (response[4] << 8)) * 1000ULL;
    if(mouse_idle_policy.IsHandedBack())
    {
        /*---------------------------------------------------------*\
        | Changing rendering mode resets the firmware counter.    |
        | A subsequent decrease or a later counter origin indicates|
        | input. Allow two seconds for counter quantization and I/O.|
        \*---------------------------------------------------------*/
        unsigned long long now = CorsairMouseTime();
        mouse_activity_after_handoff |= mouse_inactivity_ms < mouse_handoff_counter_ms ||
            (now >= mouse_handoff_ms && mouse_inactivity_ms + 2000 < now - mouse_handoff_ms);
        mouse_handoff_counter_ms = mouse_inactivity_ms;
    }
    return true;
}

/*---------------------------------------------------------*\
| Share the device configuration between HW and SW wrappers. |
\*---------------------------------------------------------*/
std::string CorsairPeripheralV2Controller::GetMouseIdleConfiguration()
{
    if(GetDeviceData()->pid != CORSAIR_DARK_CORE_RGB_PRO_PID || write_cmd != CORSAIR_V2_WRITE_WIRELESS_ID)
    {
        return "";
    }

    nlohmann::json config;
    config["schema"]["idle_handoff_enabled"]["title"]       = "Return mouse lighting to firmware when idle";
    config["schema"]["idle_handoff_enabled"]["type"]        = "bool";
    config["schema"]["idle_timeout_seconds"]["title"]       = "Lighting idle timeout (seconds)";
    config["schema"]["idle_timeout_seconds"]["type"]        = "integer";
    config["schema"]["idle_timeout_seconds"]["minimum"]     = 5;
    config["schema"]["idle_timeout_seconds"]["maximum"]     = 3600;
    config["configuration"]["idle_handoff_enabled"]         = false;
    config["configuration"]["idle_timeout_seconds"]         = 900;
    return config.dump();
}

void CorsairPeripheralV2Controller::SetMouseIdleConfiguration(const std::string& configuration)
{
    nlohmann::json config = nlohmann::json::parse(configuration, nullptr, false);
    if(config.is_discarded() || !config.contains("configuration"))
    {
        return;
    }
    const nlohmann::json& settings = config["configuration"];
    if(!settings.is_object() || !settings.contains("idle_handoff_enabled") ||
       !settings["idle_handoff_enabled"].is_boolean() || !settings.contains("idle_timeout_seconds") ||
       !settings["idle_timeout_seconds"].is_number_integer() ||
       settings["idle_timeout_seconds"] < 5 || settings["idle_timeout_seconds"] > 3600 ||
       GetMouseIdleConfiguration().empty())
    {
        return;
    }

    std::lock_guard<std::mutex> guard(mouse_lighting_mutex);
    bool enabled = settings["idle_handoff_enabled"].get<bool>();
    mouse_idle_timeout_ms = settings["idle_timeout_seconds"].get<unsigned int>() * 1000ULL;
    mouse_idle_handoff_enabled = enabled;
    if(enabled && !mouse_idle_enabled)
    {
        mouse_events = OpenMouseEvents();
        if(!mouse_events || !ReadMouseConnection() || (mouse_connected && !ReadMouseInactivity()))
        {
            LOG_WARNING("[%s] Firmware mouse activity unavailable; idle handoff disabled", device_name.c_str());
            if(mouse_events)
            {
                hid_close(mouse_events);
                mouse_events = nullptr;
            }
            return;
        }
        mouse_idle_enabled = true;
        mouse_inactivity_valid = false;
    }
}

bool CorsairPeripheralV2Controller::HasMouseIdleControl()
{
    std::lock_guard<std::mutex> guard(mouse_lighting_mutex);
    return mouse_idle_enabled;
}

bool CorsairPeripheralV2Controller::MouseIdleNeedsUpdate()
{
    std::lock_guard<std::mutex> guard(mouse_lighting_mutex);
    if(mouse_idle_enabled)
    {
        unsigned long long now = CorsairMouseTime();
        ReadMouseEvents(now);
        if(!mouse_events)
        {
            return false;
        }
        if(mouse_connection_sync)
        {
            return now >= mouse_next_query_ms;
        }
        if(!mouse_connected)
        {
            return false;
        }
        bool idle = MouseIsIdle();
        return now >= mouse_next_query_ms || mouse_idle_policy.NeedsUpdate(idle) ||
            (!idle && now - mouse_last_frame_ms >= CORSAIR_V2_UPDATE_PERIOD);
    }
    return false;
}

bool CorsairPeripheralV2Controller::MouseIsIdle()
{
    if(!mouse_idle_handoff_enabled)
    {
        return false;
    }
    if(mouse_idle_policy.IsHandedBack())
    {
        return !mouse_activity_after_handoff;
    }
    if(!mouse_inactivity_valid)
    {
        return mouse_idle_policy.IsHandedBack();
    }
    return mouse_inactivity_ms >= mouse_idle_timeout_ms;
}

bool CorsairPeripheralV2Controller::ChangeMouseOwnership(bool software)
{
    ClearPacketBuffer();
    if(!software)
    {
        /*---------------------------------------------------------*\
        | The command may apply even if its acknowledgement is lost.|
        \*---------------------------------------------------------*/
        mouse_handoff_ms = CorsairMouseTime();
        mouse_handoff_counter_ms = 0;
        mouse_activity_after_handoff = false;
    }
    if(!SetRenderModeChecked(software ? CORSAIR_V2_MODE_SW : CORSAIR_V2_MODE_HW) ||
       (software && !LightingControlChecked(0x5F)))
    {
        if(!mouse_transition_failed)
        {
            LOG_WARNING("[%s] Mouse idle ownership change failed", device_name.c_str());
        }
        mouse_transition_failed = true;
        return false;
    }
    mouse_transition_failed = false;
    LOG_INFO("[%s] Mouse lighting %s", device_name.c_str(), software ? "resumed on input" : "returned to firmware after inactivity");
    return true;
}
