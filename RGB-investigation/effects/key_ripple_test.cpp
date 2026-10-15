#include "LightMountKeys.h"
#include "RipplePattern.h"
#include "RawKeyboardInput.h"
#include <iostream>
#define CHECK(condition) do {if(!(condition)){std::cerr << "Failed: " << #condition << '\n';return 1;}}while(false)
int main()
{
    CHECK(LightMountLamp(0x14)==46); // T physical position
    CHECK(LightMountLamp(0x1E)==63); // A
    CHECK(LightMountLamp(0x39)==98); // Space
    CHECK(LightMountLamp(0x148)==90); // Up arrow
    CHECK(LightMountLamp(0x48)==59); // same scan without E0 is keypad 8
    CHECK(LightMountLamp(0x153)==55); // Delete
    CHECK(LightMountLamp(0x53)==107); // decimal
    CHECK(LightMountLamp(500)==-1);
    CHECK(RippleIntensity(0,0,16,0.8,1.8)==1);
    CHECK(RippleIntensity(0,5,16,0.8,1.8)==0);
    CHECK(RippleIntensity(0.25,5,16,0.8,1.8)==1);
    CHECK(RippleIntensity(0.8,16,16,0.8,1.8)==0);
    CHECK(RippleIntensity(-1,0,16,0.8,1.8)==0);
    CHECK(std::abs(RippleIntensity(0.25,6.79999,16,0.8,1.8)-RippleIntensity(0.25,6.80001,16,0.8,1.8))<0.001);
    for(int repeat=0;repeat<3;repeat++)
    {
        RawKeyboardInput input;
        CHECK(input.Start());
        RawKeyboardInput duplicate;
        CHECK(!duplicate.Start());
        input.Stop();
        CHECK(input.Drain().empty());
        CHECK(duplicate.Start());
        duplicate.Stop();
    }
    std::cout << "Key mapping, ripple timing and raw input start/stop checks passed\n";
}
