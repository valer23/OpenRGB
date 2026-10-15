#pragma once
#include <cstdint>
using RGBColor=std::uint32_t;
struct RGBControllerInterface {virtual ~RGBControllerInterface()=default;virtual void SetColor(unsigned int,RGBColor)=0;virtual void UpdateLEDs()=0;};
