/*---------------------------------------------------------*\
| AsusAuraNeoService_Windows.h                             |
|                                                         |
| ASUS Aura NEO explicit shutdown save through ASUS service |
|                                                         |
| valer23                                   23 Oct 2026    |
|                                                         |
| This file is part of the OpenRGB project                 |
| SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/
#pragma once

/* Called only by an explicit Save To Device. Never starts a service or uses HID.
 * effect_id 0 means Off; other IDs use the captured ASUS standby effect IDs. */
bool AsusAuraNeoSaveViaService(unsigned int effect_id, unsigned int rgb_rrggbb);
