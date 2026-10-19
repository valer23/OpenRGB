/*---------------------------------------------------------*\
| LianLiUniHubSLInfinityController.h                        |
|                                                           |
|   Driver for Lian Li SL Infinity Uni Hub                  |
|                                                           |
|   Simon McKenna                               21 Oct 2023 |
|   Will Kennedy                                17 Jan 2023 |
|   Oliver P                                    26 Apr 2022 |
|   Credit to Luca Lovisa for original work.                |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <hidapi.h>
#include "RGBController.h"

#define UNIHUB_SLINF_LED_MODE_DIRECT 0xFFFF

/*----------------------------------------------------------------------------*\
| Global definitions.                                                          |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
| Definitions related to zone Sizes                                            |
\*----------------------------------------------------------------------------*/

enum
{
    UNIHUB_SLINF_CHANNEL_COUNT              = 0x08,
    UNIHUB_SLINF_CHAN_LED_COUNT             = 0x10 * 6,
};

/*----------------------------------------------------------------------------*\
| Definitions related to LED configuration.                                    |
\*----------------------------------------------------------------------------*/

/*---------------------------------------------------------*\
| Used for sync'd mode between Fan and Edge.                |
| Palette: Static/Breathing use the full data array.        |
| Stack uses 1 colour; Color Cycle uses 3.                  |
| Staggered/Tide/Runway/Mixing/Meteor/Groove use 2.         |
| Render/Tunnel use 4. Rainbow Morph/Rainbow/Stack Multi    |
| Color/Neon/Voice do not use a palette.                    |
\*---------------------------------------------------------*/

enum
{
    UNIHUB_SLINF_LED_MODE_STATIC_COLOR   = 0x01,
    UNIHUB_SLINF_LED_MODE_BREATHING      = 0x02,
    UNIHUB_SLINF_LED_MODE_RAINBOW_MORPH  = 0x04,
    UNIHUB_SLINF_LED_MODE_RAINBOW        = 0x05,
    UNIHUB_SLINF_LED_MODE_STAGGERED      = 0x18,
    UNIHUB_SLINF_LED_MODE_TIDE           = 0x1A,
    UNIHUB_SLINF_LED_MODE_RUNWAY         = 0x1C,
    UNIHUB_SLINF_LED_MODE_MIXING         = 0x1E,
    UNIHUB_SLINF_LED_MODE_STACK          = 0x20,
    UNIHUB_SLINF_LED_MODE_STACK_MULTI_COLOR = 0x21,
    UNIHUB_SLINF_LED_MODE_NEON           = 0x22,
    UNIHUB_SLINF_LED_MODE_COLOR_CYCLE    = 0x23,
    UNIHUB_SLINF_LED_MODE_METEOR         = 0x24,
    UNIHUB_SLINF_LED_MODE_VOICE          = 0x26,
    UNIHUB_SLINF_LED_MODE_GROOVE         = 0x27,
    UNIHUB_SLINF_LED_MODE_RENDER         = 0x28,
    UNIHUB_SLINF_LED_MODE_TUNNEL         = 0x29,
    /*---------------------------------------------------------*\
    | merged modes                                              |
    \*---------------------------------------------------------*/
    UNIHUB_SLINF_LED_MODE_METEOR_MERGED  = 0x2A,
    UNIHUB_SLINF_LED_MODE_RUNWAY_MERGED  = 0x2B,
    UNIHUB_SLINF_LED_MODE_TIDE_MERGED    = 0x2C,
    UNIHUB_SLINF_LED_MODE_MIXING_MERGED  = 0x2D,
    UNIHUB_SLINF_LED_MODE_STACK_MULTI_COLOR_MERGED = 0x2E
};

enum
{
    UNIHUB_SLINF_LED_SPEED_000              = 0x02,
    UNIHUB_SLINF_LED_SPEED_025              = 0x01,
    UNIHUB_SLINF_LED_SPEED_050              = 0x00,
    UNIHUB_SLINF_LED_SPEED_075              = 0xFF,
    UNIHUB_SLINF_LED_SPEED_100              = 0xFE,
};

enum
{
    UNIHUB_SLINF_LED_DIRECTION_LTR          = 0x00,
    UNIHUB_SLINF_LED_DIRECTION_RTL          = 0x01,
};

enum
{
    UNIHUB_SLINF_LED_BRIGHTNESS_000         = 0x08,
    UNIHUB_SLINF_LED_BRIGHTNESS_025         = 0x03,
    UNIHUB_SLINF_LED_BRIGHTNESS_050         = 0x02,
    UNIHUB_SLINF_LED_BRIGHTNESS_075         = 0x01,
    UNIHUB_SLINF_LED_BRIGHTNESS_100         = 0x00,
};

enum
{
    /*---------------------------------------------------------*\
    | Limit the color white to 999999 as per manufacturer       |
    | limits                                                    |
    \*---------------------------------------------------------*/
    UNIHUB_SLINF_LED_LIMITER                = 0x01
};

/*----------------------------------------------------------------------------*\
| Definitions related to packet configuration.                                 |
\*----------------------------------------------------------------------------*/

enum
{
    UNIHUB_SLINF_TRANSACTION_ID            = 0xE0,
};

/*----------------------------------------------------------------------------*\
| Uni Hub SL Infinity controller.                                              |
\*----------------------------------------------------------------------------*/

class LianLiUniHubSLInfinityController
{

public:
    LianLiUniHubSLInfinityController(hid_device* dev_handle, const char* path, std::string dev_name);
    ~LianLiUniHubSLInfinityController();

    std::string                 GetDeviceLocation();
    std::string                 GetFirmwareVersionString();
    std::string                 GetName();
    std::string                 GetSerialString();

    void                SetChannelMode
                                (
                                unsigned char   channel,
                                const mode active_mode,
                                unsigned int    num_fans
                                );

    void                SetChannelLEDs
                                (
                                unsigned char   channel,
                                RGBColor *      colors,
                                unsigned int    num_colors,
                                float           brightness
                                );

    bool                SendStartAction
                                (
                                unsigned char   channel,
                                unsigned int    num_fans
                                );

    bool                SendColorData
                                (
                                unsigned char   channel,
                                unsigned int    num_leds,
                                unsigned char*  led_data
                                );

    void                SendCommitAction
                                (
                                unsigned char   channel,
                                unsigned char   effect,
                                unsigned char   speed,
                                unsigned int    direction,
                                unsigned int    brightness
                                );

private:
    /*---------------------------------------------------------*\
    | The Uni Hub requires colors in RBG order                  |
    \*---------------------------------------------------------*/
    struct Color
    {
        uint8_t r;
        uint8_t b;
        uint8_t g;
    };

    struct Channel
    {
        uint8_t  index;

        uint8_t  anyFanCountOffset;
        uint8_t  anyFanCount;

        uint16_t ledActionAddress;
        uint16_t ledCommitAddress;
        uint16_t ledModeAddress;
        uint16_t ledSpeedAddress;
        uint16_t ledDirectionAddress;
        uint16_t ledBrightnessAddress;

        Color    colors[UNIHUB_SLINF_CHAN_LED_COUNT];

        uint8_t  ledMode;
        uint8_t  ledSpeed;
        uint8_t  ledDirection;
        uint8_t  ledBrightness;

        uint16_t fanHubActionAddress;
        uint16_t fanHubCommitAddress;

        uint16_t fanPwmActionAddress;
        uint16_t fanPwmCommitAddress;
        uint16_t fanRpmActionAddress;

        uint16_t fanSpeed;
    };

private:
    void                    LogLightingFailure(unsigned char channel, const char* message);
    bool                    lighting_failed[8] = {};
    bool                    direct_lighting_enabled = false;
    unsigned int            configured_fan_counts[4] = {};
    hid_device*             dev;

    /*---------------------------------------------------------*\
    | Device information strings                                |
    \*---------------------------------------------------------*/
    std::string             firmware_version;
    std::string             location;
    std::string             name;
};
