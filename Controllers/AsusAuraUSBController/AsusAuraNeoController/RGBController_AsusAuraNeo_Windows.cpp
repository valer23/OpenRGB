/*---------------------------------------------------------*\
| RGBController_AsusAuraNeo_Windows.cpp                     |
|                                                           |
| ASUS Aura NEO onboard logo controller                     |
|                                                           |
| valer23                                     12 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "RGBController_AsusAuraNeo_Windows.h"
#include "AsusAuraNeoService_Windows.h"
#include "LogManager.h"
#include <chrono>
#include <thread>

#define ASUS_NEO_STALE_REPORT_LIMIT 128

/**------------------------------------------------------------------*\
    @name ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)
    @category Motherboard
    @type USB
    @save :o:
    @direct :white_check_mark:
    @effects :o:
    @detectors DetectAsusAuraNeo
    @comment Windows only. Static onboard-logo colour streaming is supported.
             Experimental shutdown effects require explicit Save To Device.
             Shutdown persistence awaits physical verification. ARGB headers
             and independent per-output effects are unsupported.
             Avoid simultaneous ASUS LightingService lighting control.
\*-------------------------------------------------------------------*/

RGBController_AsusAuraNeo::RGBController_AsusAuraNeo(hid_device* handle, const std::string& path)
    : device(handle)
{
    name        = "ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)";
    vendor      = "ASUS";
    type        = DEVICE_TYPE_MOTHERBOARD;
    description = "Onboard logo only; captured 0B05:1CD2 protocol";
    location    = "HID: " + path;
    flags      |= CONTROLLER_FLAG_MANUALLY_CONFIGURABLE_DEVICE_SPECIFIC;
    mode static_mode;
    static_mode.name       = "Static";
    static_mode.value      = 0;
    static_mode.flags      = MODE_FLAG_HAS_PER_LED_COLOR;
#ifdef ASUS_NEO_SHUTDOWN_TRIAL
    /* Explicit trial opt-in until physical S5 persistence is verified. */
    static_mode.flags     |= MODE_FLAG_MANUAL_SAVE;
#endif
    static_mode.color_mode = MODE_COLORS_PER_LED;
    modes.push_back(static_mode);
    zone logo;
    logo.name       = "ROG Logo";
    logo.type       = ZONE_TYPE_SINGLE;
    logo.leds_min   = 1;
    logo.leds_max   = 1;
    logo.leds_count = 1;
    zones.push_back(logo);
    led logo_led;
    logo_led.name = "ROG Logo";
    leds.push_back(logo_led);
    SetupColors();

    nlohmann::json settings;
    settings["schema"]["shutdown_effect"] =
    {
        {"title", "Shutdown effect (experimental)"}, {"type", "string"}, {"order", 0},
        {"description", "Stage a shutdown effect, then use Save To Device to apply. Off mirrors ASUS's motherboard standby switch, which may also affect motherboard-connected ARGB. Leave unchanged sends no commands. No service is stopped. Persistence across boot remains experimental."},
        {"enum", {"Leave unchanged", "Off", "Static", "Breathing", "Strobing", "Color cycle", "Rainbow", "Starry Night"}}
    };
    settings["schema"]["shutdown_color"] =
    {
        {"title", "Shutdown colour (RRGGBB)"}, {"type", "string"}, {"format", "color"}, {"order", 1},
        {"description", "Choose a colour or enter six hexadecimal digits, for example FF0000 for red. Shared by both onboard outputs and independent of the running colour. Ignored for Off."}
    };
    settings["schema"]["shutdown_writer"] =
    {
        {"title", "Shutdown writer"}, {"type", "string"}, {"order", 2},
        {"description", "ASUS service updates the running LightingService's saved shutdown preference without direct USB access. Direct device requires ASUS motherboard lighting to be inactive; ASUS may overwrite it on startup. Neither writer starts/stops services or falls back to the other writer. Settings loads send no commands."},
        {"enum", {"Direct device", "ASUS service"}}
    };
    settings["configuration"]["shutdown_effect"] = "Leave unchanged";
    settings["configuration"]["shutdown_color"] = "FF0000";
    settings["configuration"]["shutdown_writer"] = "Direct device";
    configuration = settings.dump();
}

RGBController_AsusAuraNeo::~RGBController_AsusAuraNeo()
{
    Shutdown();
    hid_close(device);
}

RGBController_AsusAuraNeo::Report RGBController_AsusAuraNeo::OnboardReport(
    unsigned char output, bool initialize, RGBColor color)
{
    Report report = {};
    report[0] = 0x41;
    report[1] = 0x05;
    report[2] = 0x01;
    report[6] = output;
    if(initialize)
    {
        report[8] = 0x01;
    }
    else
    {
        report[16] = 0x01;
        report[18] = RGBGetGValue(color);
        report[19] = RGBGetRValue(color);
        report[20] = RGBGetBValue(color);
    }
    return report;
}

bool RGBController_AsusAuraNeo::Send(const Report& report)
{
    // The capture uses control SET_REPORT. hid_write uses a different endpoint.
    if(hid_send_output_report(device, report.data(), report.size()) != static_cast<int>(report.size()))
    {
        if(!write_failed)
        {
            LOG_ERROR("[ASUS Aura NEO] Output report 0x%02X failed", report[1]);
        }
        write_failed = true;
        initialized = false;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    return true;
}

bool RGBController_AsusAuraNeo::Initialize()
{
    // Exact captured onboard setup packets, frames 369 and 371. Header setup
    // packets are deliberately excluded; only the onboard logo is supported.
    if(!Send(OnboardReport(0, true, 0)) || !Send(OnboardReport(1, true, 0)))
    {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
    Report activate = {};
    activate[0] = 0x41;
    activate[1] = 0x21;
    activate[3] = 0x01; // Exact captured frame 407; activation semantics inferred.
    if(!Send(activate))
    {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    initialized = true;
    return true;
}

void RGBController_AsusAuraNeo::DeviceUpdateLEDs()
{
    if(colors.empty() || active_mode != 0 || (!initialized && !Initialize()))
    {
        return;
    }
    // Both captured onboard outputs share the chosen logo colour. Their separate
    // physical mapping is unknown, so do not expose unverified independent zones.
    if(!Send(OnboardReport(0, false, colors[0])))
    {
        return;
    }
    if(Send(OnboardReport(1, false, colors[0])))
    {
        write_failed = false;
    }
}

void RGBController_AsusAuraNeo::DeviceUpdateZoneLEDs(int zone)
{
    if(zone == 0)
    {
        DeviceUpdateLEDs();
    }
}

void RGBController_AsusAuraNeo::DeviceUpdateSingleLED(int led)
{
    if(led == 0)
    {
        DeviceUpdateLEDs();
    }
}

void RGBController_AsusAuraNeo::DeviceUpdateMode()
{
    DeviceUpdateLEDs();
}

bool RGBController_AsusAuraNeo::Query(const Report& request, Report& response)
{
    /*-----------------------------------------------------*\
    | Use this controller's handle under the core's lock.   |
    | Drain stale input with a bound; never start a reader. |
    \*-----------------------------------------------------*/
    for(unsigned int i = 0; i < ASUS_NEO_STALE_REPORT_LIMIT; i++)
    {
        int length = hid_read_timeout(device, response.data(), response.size(), 0);
        if(length < 0)
        {
            LOG_ERROR("[ASUS Aura NEO] Input drain failed before query 0x%02X", request[1]);
            return false;
        }
        if(length == 0)
        {
            if(i != 0)
            {
                LOG_INFO("[ASUS Aura NEO] Drained %u queued input reports before query 0x%02X", i, request[1]);
            }
            break;
        }
        if(i == ASUS_NEO_STALE_REPORT_LIMIT - 1)
        {
            LOG_ERROR("[ASUS Aura NEO] Input queue still busy after %u reports; query 0x%02X not sent", ASUS_NEO_STALE_REPORT_LIMIT, request[1]);
            return false;
        }
    }
    if(!Send(request))
    {
        return false;
    }
    unsigned int received = 0;
    for(unsigned int i = 0; i < 16; i++)
    {
        int length = hid_read_timeout(device, response.data(), response.size(), 40);
        if(length < 0)
        {
            LOG_ERROR("[ASUS Aura NEO] Input read failed for query 0x%02X", request[1]);
            return false;
        }
        if(length == 0)
        {
            continue;
        }
        if(length != (int)response.size())
        {
            LOG_ERROR("[ASUS Aura NEO] Query 0x%02X reply length %d; expected %u", request[1], length, (unsigned int)response.size());
            return false;
        }
        received++;
        if(response[0] == 0x41 && response[1] == (request[1] | 0x80) && response[2] == request[2])
        {
            if(request[1] != 0x09 && response[3] != request[3])
            {
                LOG_ERROR("[ASUS Aura NEO] Query 0x%02X reply state %u; expected %u", request[1], response[3], request[3]);
                return false;
            }
            return true;
        }
    }
    if(received == 0)
    {
        LOG_ERROR("[ASUS Aura NEO] No input reply for query 0x%02X after bounded wait", request[1]);
    }
    else
    {
        LOG_ERROR("[ASUS Aura NEO] No matching reply for query 0x%02X (%u reports; last prefix %02X %02X %02X %02X)",
                  request[1], received, response[0], response[1], response[2], response[3]);
    }
    return false;
}

void RGBController_AsusAuraNeo::DeviceSaveMode()
{
#ifndef ASUS_NEO_SHUTDOWN_TRIAL
    LOG_WARNING("[ASUS Aura NEO] Shutdown apply is disabled outside the experimental trial build");
    return;
#endif
    /*-----------------------------------------------------*\
    | Settings/profile loads only stage JSON. Standby I/O   |
    | belongs exclusively to this explicit, core-locked    |
    | save path. Never initialize or change running mode.  |
    \*-----------------------------------------------------*/
    static const char* effect_names[] = {"Static", "Breathing", "Strobing", "Color cycle", "Rainbow", "Starry Night", "Off"};
    static const unsigned char effect_ids[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x0D, 0x01};
    std::string effect_name;
    std::string color_text;
    std::string writer;
    try
    {
        nlohmann::json settings = nlohmann::json::parse(configuration).at("configuration");
        effect_name = settings.at("shutdown_effect").get<std::string>();
        if(effect_name == "Leave unchanged")
        {
            return;
        }
        color_text = effect_name == "Off" ? "000000" : settings.at("shutdown_color").get<std::string>();
        writer = settings.value("shutdown_writer", std::string("Direct device"));
    }
    catch(...)
    {
        LOG_ERROR("[ASUS Aura NEO] Invalid shutdown settings; no commands sent");
        return;
    }
    unsigned int effect = 0;
    for(; effect < 7; effect++)
    {
        if(effect_name == effect_names[effect])
        {
            break;
        }
    }
    unsigned int rgb = 0;
    if(effect == 7 || color_text.size() != 6)
    {
        LOG_ERROR("[ASUS Aura NEO] Invalid shutdown effect or RRGGBB colour; no commands sent");
        return;
    }
    for(unsigned int i = 0; i < 6; i++)
    {
        char c = color_text[i];
        int digit = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if(digit < 0)
        {
            LOG_ERROR("[ASUS Aura NEO] Invalid RRGGBB colour; no commands sent");
            return;
        }
        rgb = (rgb << 4) | digit;
    }
    if(writer == "ASUS service")
    {
        AsusAuraNeoSaveViaService(effect_name == "Off" ? 0 : effect_ids[effect], rgb);
        return;
    }
    if(writer != "Direct device")
    {
        LOG_ERROR("[ASUS Aura NEO] Unknown shutdown writer; no commands sent");
        return;
    }
    if(AsusAuraNeoHasCompetingLighting())
    {
        LOG_ERROR("[ASUS Aura NEO] Shutdown apply refused: ASUS motherboard lighting is active or process inventory is unavailable; no commands sent");
        return;
    }

    Report query = {};
    Report response = {};
    query[0] = 0x41;
    query[1] = 0x09;
    query[2] = 0x01;
    if(!Query(query, response) || response[3] > 60)
    {
        LOG_ERROR("[ASUS Aura NEO] Standby capability query failed; no settings written");
        return;
    }
    bool supported = false;
    for(unsigned int i = 0; i < response[3]; i++)
    {
        if(response[4 + i] == effect_ids[effect])
        {
            supported = true;
        }
    }
    if(!supported)
    {
        LOG_ERROR("[ASUS Aura NEO] Selected standby effect is not advertised; no settings written");
        return;
    }
    query = {};
    query[0] = 0x41;
    query[1] = 0x25;
    query[3] = 0x01;
    Report previous_effect = {};
    if(!Query(query, previous_effect) || previous_effect[8] != 0x03 ||
       previous_effect[9] != 0 || previous_effect[10] != 0 || previous_effect[11] != 0)
    {
        LOG_ERROR("[ASUS Aura NEO] Standby group does not match the captured onboard route; no settings written");
        return;
    }
    for(unsigned int unit = 0; unit < 2; unit++)
    {
        query[1] = 0x29;
        query[2] = unit;
        if(!Query(query, response) || response[8] != 0x10 || response[10] != unit)
        {
            LOG_ERROR("[ASUS Aura NEO] Standby unit does not match the captured onboard route; no settings written");
            return;
        }
    }

    int r = (rgb >> 16) & 0xFF;
    int g = (rgb >> 8) & 0xFF;
    int b = rgb & 0xFF;
    int maximum = r > g ? r : g;
    maximum = maximum > b ? maximum : b;
    int minimum = r < g ? r : g;
    minimum = minimum < b ? minimum : b;
    int hue = 0;
    if(maximum != minimum)
    {
        double degrees = maximum == r ? 60.0 * (g - b) / (maximum - minimum) :
                         maximum == g ? 120.0 + 60.0 * (b - r) / (maximum - minimum) :
                                        240.0 + 60.0 * (r - g) / (maximum - minimum);
        if(degrees < 0)
        {
            degrees += 360;
        }
        hue = (int)(degrees * 255 / 360);
    }
    Report effect_report = {};
    effect_report[0] = 0x41;
    effect_report[1] = 0x23;
    effect_report[3] = 0x01;
    effect_report[4] = effect_ids[effect];
    effect_report[5] = previous_effect[5];
    effect_report[6] = previous_effect[6];
    effect_report[7] = hue;
    if(AsusAuraNeoHasCompetingLighting())
    {
        LOG_ERROR("[ASUS Aura NEO] ASUS lighting became active; no settings written");
        return;
    }
    /*-----------------------------------------------------*\
    | Captured Off/On toggle: standby state 1, enable 0/1.  |
    | Off then selects Static black on both onboard units. |
    | Never use state 0 here: that controls running lights.|
    \*-----------------------------------------------------*/
    Report enable_report = {};
    enable_report[0] = 0x41;
    enable_report[1] = 0x21;
    enable_report[2] = 0x01;
    enable_report[3] = effect_name == "Off" ? 0 : 1;
    if(!Send(enable_report))
    {
        return;
    }
    if(!Send(effect_report))
    {
        LOG_ERROR("[ASUS Aura NEO] Shutdown apply incomplete; standby enable state may have changed");
        return;
    }
    for(unsigned int unit = 0; unit < 2; unit++)
    {
        Report color_report = {};
        color_report[0] = 0x41;
        color_report[1] = 0x28;
        color_report[2] = unit;
        color_report[3] = 0x01;
        color_report[4] = g;
        color_report[5] = r;
        color_report[6] = b;
        if(!Send(color_report))
        {
            LOG_ERROR("[ASUS Aura NEO] Shutdown apply incomplete; an earlier write may have taken effect");
            return;
        }
    }

    query[1] = 0x25;
    query[2] = 0;
    if(!Query(query, response) || response[4] != effect_report[4] ||
       response[5] != effect_report[5] || response[6] != effect_report[6] || response[7] != effect_report[7])
    {
        LOG_ERROR("[ASUS Aura NEO] Shutdown effect readback failed; settings may have changed");
        return;
    }
    for(unsigned int unit = 0; unit < 2; unit++)
    {
        query[1] = 0x29;
        query[2] = unit;
        if(!Query(query, response) || response[4] != g || response[5] != r || response[6] != b)
        {
            LOG_ERROR("[ASUS Aura NEO] Shutdown colour readback failed; settings may have changed");
            return;
        }
    }
    LOG_INFO("[ASUS Aura NEO] Shutdown %s command delivered; effect/colour readback matched. Standby enable state and power-off persistence require physical verification", effect_name.c_str());
}
