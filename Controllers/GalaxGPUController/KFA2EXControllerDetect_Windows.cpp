/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "DetectionManager.h"
#include "LogManager.h"
#include "RGBController_KFA2EX_Windows.h"

DetectedControllers DetectKFA2EXControllers()
{
    DetectedControllers found;
    NV_PHYSICAL_GPU_HANDLE handles[64] = {};
    NV_S32 count = 0;
    if(NvAPI_Initialize() != 0 || NvAPI_EnumPhysicalGPUs(handles, &count) != 0)
    {
        return found;
    }
    for(NV_S32 i = 0; i < count; i++)
    {
        NV_U32 device = 0, subsystem = 0, revision = 0, extended = 0;
        if(NvAPI_GPU_GetPCIIdentifiers(handles[i], &device, &subsystem, &revision, &extended) != 0 ||
           device != 0x220810DE || subsystem != 0x153510DE || !RGBController_KFA2EX::Probe(handles[i]))
        {
            continue;
        }
        try
        {
            found.push_back(new RGBController_KFA2EX(handles[i]));
        }
        catch(const std::exception& error)
        {
            LOG_ERROR("[KFA2 EX] Detection failed: %s", error.what());
        }
    }
    return found;
}

REGISTER_DETECTOR("KFA2 RTX 3080 Ti EX (captured protocol)", DetectKFA2EXControllers);
