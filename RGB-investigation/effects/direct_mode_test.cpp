#include "CompatibleDirectMode.h"
#include <cassert>
#include <iostream>

int main()
{
    RGBControllerInterface controller;
    controller.name = "Unknown firmware-backed device";
    controller.modes = {"Static"};
    controller.flags = {32};
    assert(CompatibleDirectMode(&controller) == -1);
    for(const std::string& name : {"KFA2 GeForce RTX 3080 Ti EX Gamer",
                                  "ASUS ROG STRIX B850-F GAMING WIFI7 NEO (logo)",
                                  "Lian Li SL Infinity (local static test)"})
    {
        controller.name = name;
        assert(CompatibleDirectMode(&controller) == 0);
        controller.flags[0] = 0;
        assert(CompatibleDirectMode(&controller) == -1);
        controller.flags[0] = 32;
    }
    controller.modes.push_back("Direct");
    controller.flags.push_back(32);
    assert(CompatibleDirectMode(&controller) == 1); // Native Direct always wins.
    controller.name = "Unrelated motherboard";
    assert(CompatibleDirectMode(&controller) == 1);
    controller.modes = {"Custom"};
    controller.flags = {32};
    controller.name = "Lian Li Uni Hub - SL Infinity";
    assert(CompatibleDirectMode(&controller) == 0);
    controller.flags[0] = 0;
    assert(CompatibleDirectMode(&controller) == -1);
    controller.flags[0] = 32;
    controller.name = "Unknown device";
    assert(CompatibleDirectMode(&controller) == -1);
    controller.name = "Lian Li Uni Hub - SL Infinity";
    controller.modes.push_back("Direct");
    controller.flags.push_back(32);
    assert(CompatibleDirectMode(&controller) == 1);
    std::cout << "Direct selection whitelist, per-LED requirement and native Direct preference pass\n";
}
