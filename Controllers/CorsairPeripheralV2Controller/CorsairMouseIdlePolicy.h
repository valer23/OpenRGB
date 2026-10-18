/*---------------------------------------------------------*\
| CorsairMouseIdlePolicy.h                                  |
|                                                           |
| Corsair wireless mouse idle ownership policy              |
|                                                           |
| valer23                                     18 Oct 2026   |
|                                                           |
| This file is part of the OpenRGB project                  |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#pragma once

class CorsairMouseIdlePolicy
{
public:
    bool IsHandedBack() const
    {
        return handed_back;
    }

    void RequireSoftwareMode()
    {
        software_initialization_required = true;
        retry_after_ms = 0;
    }

    bool NeedsUpdate(bool idle) const
    {
        return idle != handed_back || (!idle && software_initialization_required);
    }

    template<class ChangeMode>
    bool AllowFrame(bool idle, unsigned long long now_ms, ChangeMode change_mode)
    {
        if(idle)
        {
            if(!handed_back)
            {
                /*---------------------------------------------------------*\
                | Retry traffic could keep the idle mouse awake.           |
                | Attempt handoff only once per idle interval.             |
                \*---------------------------------------------------------*/
                handed_back = true;
                change_mode(false);
            }
            return false;
        }
        if(handed_back || software_initialization_required)
        {
            if(now_ms < retry_after_ms)
            {
                return false;
            }
            if(!change_mode(true))
            {
                retry_after_ms = now_ms + 1000;
                return false;
            }
            handed_back = false;
            software_initialization_required = false;
            retry_after_ms = 0;
        }
        return true;
    }

private:
    bool                handed_back = false;
    bool                software_initialization_required = false;
    unsigned long long  retry_after_ms = 0;
};
