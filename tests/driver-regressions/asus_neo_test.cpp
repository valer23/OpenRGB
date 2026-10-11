/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "RGBController_AsusAuraNeo_Windows.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

unsigned int logged_errors = 0;
static std::vector<std::vector<unsigned char>> sent;
static int fail_at = -1;
static int failure_result = -1;
static unsigned int closes = 0;

extern "C" int hid_send_output_report(hid_device*, const unsigned char* data, size_t length)
{
    sent.emplace_back(data, data + length);
    if(static_cast<int>(sent.size()) == fail_at) return failure_result;
    return static_cast<int>(length);
}
extern "C" void hid_close(hid_device*) { ++closes; }

static std::vector<std::vector<unsigned char>> Fixtures()
{
    std::ifstream file("tests/driver-regressions/fixtures/asus-neo-onboard.tsv");
    assert(file.good());
    std::vector<std::vector<unsigned char>> fixtures;
    std::string line;
    while(std::getline(file, line))
    {
        assert(line.size() == 128);
        std::vector<unsigned char> bytes;
        for(size_t i = 0; i < line.size(); i += 2)
            bytes.push_back(static_cast<unsigned char>(std::stoul(line.substr(i, 2), nullptr, 16)));
        fixtures.push_back(bytes);
    }
    assert(fixtures.size() == 9);
    return fixtures;
}

int main()
{
    const auto fixtures = Fixtures();
    auto* handle = reinterpret_cast<hid_device*>(1);
    {
        RGBController_AsusAuraNeo controller(handle, "test-path");
        assert(sent.empty()); // Detection is read-only.
        assert(controller.zones.size() == 1 && controller.colors.size() == 1);
        assert(controller.modes.size() == 1 && controller.modes[0].name == "Static");
        controller.colors[0] = ToRGBColor(255, 0, 0);
        controller.DeviceUpdateMode();
        assert(sent.size() == 5);
        for(size_t i = 0; i < 5; ++i) assert(sent[i] == fixtures[i]);
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

    // Every partial initialization/colour failure stops the remaining sequence.
    // A short positive transfer is also a failure, not just a negative return.
    for(int failure = 1; failure <= 5; ++failure)
    {
        sent.clear();
        fail_at = failure;
        failure_result = failure % 2 ? -1 : 63;
        RGBController_AsusAuraNeo controller(handle, "test-path");
        controller.DeviceUpdateLEDs();
        assert(static_cast<int>(sent.size()) == failure);
        assert(logged_errors == static_cast<unsigned int>(failure));
        sent.clear();
        fail_at = 1;
        controller.DeviceUpdateLEDs();
        assert(sent.size() == 1 && sent[0] == fixtures[0]); // Retry re-initializes.
        --logged_errors;
    }
    // The fixtures and all generated writes target onboard selector1 or the
    // captured activation command. No selector0 ARGB setup/colour is emitted.
    for(const auto& packet : fixtures)
        assert(packet.size() == 64 && (packet[1] == 0x21 || (packet[1] == 0x05 && packet[2] == 1)));
    std::cout << "ASUS NEO captured packets, lazy setup and failure handling passed\n";
}
