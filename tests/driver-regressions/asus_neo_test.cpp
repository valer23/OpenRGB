/*---------------------------------------------------------*\
| asus_neo_test.cpp                                         |
|                                                           |
| ASUS Aura NEO onboard logo regression tests               |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "RGBController_AsusAuraNeo_Windows.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

unsigned int logged_errors = 0;
static std::vector<std::vector<unsigned char>> sent;
static int fail_at = -1;
static int failure_result = -1;
static unsigned int closes = 0;
static bool competing = false;
static bool reply_pending = false;
static int fault = 0;
static unsigned int stale_remaining = 0;
static unsigned char reply[64] = {};
static unsigned char effect_state[64] = {0x41, 0xA5, 0, 1, 5, 0, 0, 0, 3};
static unsigned char unit_state[2][64] =
{
    {0x41, 0xA9, 0, 1, 0, 0xFF, 0, 0, 0x10, 5, 0},
    {0x41, 0xA9, 1, 1, 0, 0xFF, 0, 0, 0x10, 5, 1}
};

bool AsusAuraNeoHasCompetingLighting()
{
    return competing;
}

extern "C" int hid_send_output_report(hid_device*, const unsigned char* data, size_t length)
{
    sent.emplace_back(data, data + length);
    if(static_cast<int>(sent.size()) == fail_at)
    {
        return failure_result;
    }
    if(data[1] == 0x09 || data[1] == 0x25 || data[1] == 0x29)
    {
        std::fill(reply, reply + 64, 0);
        if(data[1] == 0x09)
        {
            unsigned char caps[] = {0x41, 0x89, 1, 9, 0, 1, 2, 3, 4, 5, 6, 0x0D, 0xFF};
            std::copy(caps, caps + sizeof(caps), reply);
            if(fault == 1) { reply[3] = 0; }
            if(fault == 2) { reply[3] = 61; }
        }
        else if(data[1] == 0x25)
        {
            std::copy(effect_state, effect_state + 64, reply);
            if(fault == 3) { reply[8] = 0x0B; } // Includes an ARGB unit.
            if(fault == 9 && sent.size() > 7) { reply[4] = 0xFF; }
        }
        else
        {
            assert(data[2] < 2);
            std::copy(unit_state[data[2]], unit_state[data[2]] + 64, reply);
            if(fault == 4) { reply[8] = 0x20; }
            if(fault == 5) { reply[2] ^= 1; }
        }
        if(fault == 6) { reply[0] = 0xEC; }
        if(fault == 7 && data[1] != 9) { reply[3] = 0; }
        reply_pending = true;
    }
    else if(data[1] == 0x23)
    {
        std::copy(data + 4, data + 8, effect_state + 4);
    }
    else if(data[1] == 0x28)
    {
        assert(data[2] < 2);
        std::copy(data + 4, data + 7, unit_state[data[2]] + 4);
    }
    return static_cast<int>(length);
}
extern "C" int hid_read_timeout(hid_device*, unsigned char* data, size_t length, int milliseconds)
{
    assert(length == 64);
    if(milliseconds == 0 && stale_remaining)
    {
        --stale_remaining;
        std::fill(data, data + length, 0);
        return 64;
    }
    if(milliseconds == 0 || !reply_pending)
    {
        return 0;
    }
    reply_pending = false;
    std::copy(reply, reply + 64, data);
    return fault == 8 ? 12 : fault == 10 ? -1 : 64;
}
extern "C" void hid_close(hid_device*)
{
    ++closes;
}

static std::vector<std::vector<unsigned char>> Fixtures(const char* path, unsigned int count)
{
    std::ifstream file(path);
    assert(file.good());
    std::vector<std::vector<unsigned char>> fixtures;
    std::string line;
    while(std::getline(file, line))
    {
        assert(line.size() == 128);
        std::vector<unsigned char> bytes;
        for(size_t i = 0; i < line.size(); i += 2)
        {
            bytes.push_back(static_cast<unsigned char>(std::stoul(line.substr(i, 2), nullptr, 16)));
        }
        fixtures.push_back(bytes);
    }
    assert(fixtures.size() == count);
    return fixtures;
}

int main()
{
    const std::vector<std::vector<unsigned char>> fixtures = Fixtures("tests/driver-regressions/fixtures/asus-neo-onboard.tsv", 9);
    const std::vector<std::vector<unsigned char>> shutdown_blue = Fixtures("tests/driver-regressions/fixtures/asus-neo-shutdown-blue.tsv", 3);
    hid_device* handle = reinterpret_cast<hid_device*>(1);
    {
        RGBController_AsusAuraNeo controller(handle, "test-path");
        assert(sent.empty()); // Detection is read-only.
        assert(controller.zones.size() == 1 && controller.colors.size() == 1);
        assert(controller.modes.size() == 1 && controller.modes[0].name == "Static");
        assert(controller.flags & CONTROLLER_FLAG_MANUALLY_CONFIGURABLE_DEVICE_SPECIFIC);
        assert(nlohmann::json::parse(controller.configuration)["schema"]["shutdown_effect"]["type"] == "string");
        controller.colors[0] = ToRGBColor(255, 0, 0);
        controller.DeviceUpdateMode();
        assert(sent.size() == 5);
        for(size_t i = 0; i < 5; ++i)
        {
            assert(sent[i] == fixtures[i]);
        }
        sent.clear();
        controller.colors[0] = ToRGBColor(0, 255, 0);
        controller.DeviceUpdateZoneLEDs(0);
        assert(sent.size() == 2 && sent[0] == fixtures[5] && sent[1] == fixtures[6]);
        sent.clear();
        controller.colors[0] = ToRGBColor(0, 0, 255);
        controller.DeviceUpdateSingleLED(0);
        assert(sent.size() == 2 && sent[0] == fixtures[7] && sent[1] == fixtures[8]);
        sent.clear();
        controller.DeviceUpdateZoneLEDs(1);
        controller.DeviceUpdateSingleLED(-1);
        controller.active_mode = 1;
        controller.DeviceUpdateLEDs();
        assert(sent.empty());
    }
    assert(closes == 1);

#ifdef ASUS_NEO_SHUTDOWN_TRIAL
    // Staging/default/reloading settings never touches hardware. Save is the
    // only standby path and refuses invalid input or an active ASUS reader.
    fail_at = -1;
    sent.clear();
    {
        RGBController_AsusAuraNeo controller(handle, "test-path");
        assert(sent.empty());
        controller.DeviceSaveMode();
        assert(sent.empty());
        nlohmann::json config = nlohmann::json::parse(controller.configuration);
        config["configuration"]["shutdown_effect"] = "Static";
        config["configuration"]["shutdown_color"] = "0000FF";
        controller.configuration = config.dump();
        assert(sent.empty());
        competing = true;
        controller.DeviceSaveMode();
        assert(sent.empty());
        competing = false;
        config["configuration"]["shutdown_color"] = "00GGFF";
        controller.configuration = config.dump();
        controller.DeviceSaveMode();
        assert(sent.empty());
        config["configuration"]["shutdown_effect"] = "Partition";
        controller.configuration = config.dump();
        controller.DeviceSaveMode();
        assert(sent.empty());
        config["configuration"]["shutdown_effect"] = "Static";
        config["configuration"]["shutdown_color"] = "0000FF";
        controller.configuration = config.dump();
        stale_remaining = 16;
        controller.DeviceSaveMode();
        assert(sent.empty());
        controller.DeviceSaveMode();
        assert(sent.size() == 10);
        const unsigned char commands[] = {9, 0x25, 0x29, 0x29, 0x23, 0x28, 0x28, 0x25, 0x29, 0x29};
        for(unsigned int i = 0; i < 10; i++)
        {
            assert(sent[i][0] == 0x41 && sent[i][1] == commands[i]);
            assert(sent[i][1] == 9 || sent[i][3] == 1);
        }
        assert(sent[4][4] == 1 && sent[4][7] == 0xAA);
        assert(sent[5][2] == 0 && sent[6][2] == 1);
        assert(sent[5][4] == 0 && sent[5][5] == 0 && sent[5][6] == 0xFF);
        for(unsigned int i = 0; i < 3; i++)
        {
            assert(sent[4 + i] == shutdown_blue[i]);
        }
        assert(!sent.empty() && sent[5] != fixtures[7]); // Standby, not live stream.
        for(int failure = 1; failure <= 8; failure++)
        {
            fault = failure;
            sent.clear();
            controller.DeviceSaveMode();
            for(const std::vector<unsigned char>& packet : sent)
            {
                assert(packet[1] != 0x23 && packet[1] != 0x28);
            }
        }
        fault = 0;
        const char* names[] = {"Static", "Breathing", "Strobing", "Color cycle", "Rainbow", "Starry Night"};
        const unsigned char ids[] = {1, 2, 3, 4, 5, 0x0D};
        effect_state[5] = 7; effect_state[6] = 1;
        for(unsigned int i = 0; i < 6; i++)
        {
            config["configuration"]["shutdown_effect"] = names[i];
            controller.configuration = config.dump();
            sent.clear();
            controller.DeviceSaveMode();
            assert(sent.size() == 10 && sent[4][4] == ids[i]);
            assert(sent[4][5] == 7 && sent[4][6] == 1);
        }
        effect_state[5] = 0; effect_state[6] = 0;
        for(int failure = 5; failure <= 7; failure++)
        {
            sent.clear(); fail_at = failure; failure_result = 63;
            controller.DeviceSaveMode();
            assert((int)sent.size() == failure);
        }
        fail_at = -1;
        fault = 9;
        sent.clear();
        controller.DeviceSaveMode();
        assert(sent.size() == 8); // Failed readback stops dependent reads.
        fault = 0;
    }
    logged_errors = 0;
#else
    sent.clear();
    {
        RGBController_AsusAuraNeo controller(handle, "test-path");
        nlohmann::json config = nlohmann::json::parse(controller.configuration);
        config["configuration"]["shutdown_effect"] = "Static";
        controller.configuration = config.dump();
        controller.DeviceSaveMode();
        assert(sent.empty()); // Includes explicit SDK SaveMode in a normal build.
        assert((controller.modes[0].flags & MODE_FLAG_MANUAL_SAVE) == 0);
    }
#endif

    // Every partial initialization/colour failure stops the remaining sequence.
    // A short positive transfer is also a failure, not just a negative return.
    for(int failure = 1; failure <= 5; ++failure)
    {
        sent.clear();
        fail_at        = failure;
        failure_result = failure % 2 ? -1 : 63;
        RGBController_AsusAuraNeo controller(handle, "test-path");
        controller.DeviceUpdateLEDs();
        assert(static_cast<int>(sent.size()) == failure);
        assert(logged_errors == static_cast<unsigned int>(failure));
        sent.clear();
        fail_at = 1;
        controller.DeviceUpdateLEDs();
        assert(sent.size() == 1 && sent[0] == fixtures[0]); // Retry re-initializes.
        assert(logged_errors == static_cast<unsigned int>(failure));
    }
    // A complete update re-enables reporting for a new failure period.
    {
        sent.clear();fail_at=1;
        RGBController_AsusAuraNeo controller(handle, "test-path");
        unsigned int before=logged_errors;
        controller.DeviceUpdateLEDs();assert(logged_errors==before+1);
        sent.clear();fail_at=-1;controller.DeviceUpdateLEDs();assert(sent.size()==5);
        sent.clear();fail_at=1;controller.DeviceUpdateLEDs();assert(logged_errors==before+2);
    }
    // The fixtures and all generated writes target onboard selector1 or the
    // captured activation command. No selector0 ARGB setup/colour is emitted.
    for(const std::vector<unsigned char>& packet : fixtures)
    {
        assert(packet.size() == 64 && (packet[1] == 0x21 || (packet[1] == 0x05 && packet[2] == 1)));
    }
    std::cout << "ASUS NEO captured packets, lazy setup and failure handling passed\n";
}
