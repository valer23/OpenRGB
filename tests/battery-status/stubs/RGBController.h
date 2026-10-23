#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
using RGBColor=uint32_t;
#define RGBGetRValue(rgb) (rgb & 0x000000FF)
#define RGBGetGValue(rgb) ((rgb >> 8) & 0x000000FF)
#define RGBGetBValue(rgb) ((rgb >> 16) & 0x000000FF)
enum device_type {DEVICE_TYPE_KEYBOARD,DEVICE_TYPE_MOUSE};
enum zone_type {ZONE_TYPE_SINGLE,ZONE_TYPE_LINEAR};
