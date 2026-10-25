/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
inline bool IsAsusDeltaIICollection(unsigned vendor,unsigned product,int interface_number,
                                    unsigned usage_page,unsigned usage,const char* device_path)
{
    if(vendor!=0x0B05 || product!=0x1AFA || interface_number!=3 || usage_page!=0xFF00 || usage!=1 || !device_path)return false;
    std::string path(device_path);
    std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return path.find("vid_0b05&pid_1afa&mi_03&col04")!=std::string::npos;
}