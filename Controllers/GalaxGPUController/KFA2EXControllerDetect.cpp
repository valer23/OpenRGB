/*---------------------------------------------------------*\
| KFA2EXControllerDetect.cpp                                |
|                                                           |
| KFA2 EX Gamer GPU detector                                |
|                                                           |
| valer23                                     13 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#include "DetectionManager.h"
#include "LogManager.h"
#include "RGBController_KFA2EX.h"

#include "pci_ids.h"

DetectedControllers DetectKFA2EXControllers(i2c_smbus_interface* bus, unsigned char address, const std::string& name)
{
    DetectedControllers found;
    if(!RGBController_KFA2EX::Probe(bus, address))
    {
        return found;
    }
    try
    {
        found.push_back(new RGBController_KFA2EX(bus, address, name));
        LOG_INFO("[%s] KFA2 EX I2C lighting controller detected", name.c_str());
    }
    catch(const std::exception& error)
    {
        LOG_ERROR("[KFA2 EX] Detection failed: %s", error.what());
    }
    return found;
}

REGISTER_I2C_PCI_DETECTOR("KFA2 GeForce RTX 3080 Ti EX Gamer", DetectKFA2EXControllers,
                         NVIDIA_VEN, NVIDIA_RTX3080TI_DEV, NVIDIA_SUB_VEN, NVIDIA_RTX3080TI_FE_SUB_DEV, 0x23);
