#include "RGBController_LianLiUniHubSLInfinity.h"
#include "StringUtils.h"
#include <cassert>
#include <iostream>
#include <vector>
struct Report { bool feature; std::vector<unsigned char> data; };
std::vector<Report> reports;
unsigned int logged_errors=0;
bool fail_feature=false;
bool fail_output=false;
unsigned int control_outputs=0;
extern "C" int hid_write(hid_device*, const unsigned char* data, size_t size) {
#ifdef _WIN32
    assert(false && "Windows colour reports must use the verified control transport");
#endif
    reports.push_back({false,{data,data+size}}); return static_cast<int>(size);
}
extern "C" int hid_send_output_report(hid_device*, const unsigned char* data, size_t size) {
    ++control_outputs;
    reports.push_back({false,{data,data+size}});
    return fail_output ? static_cast<int>(size-1) : static_cast<int>(size);
}
extern "C" int hid_send_feature_report(hid_device*, const unsigned char* data, size_t size) {
    reports.push_back({true,{data,data+size}}); return fail_feature ? -1 : static_cast<int>(size);
}
extern "C" void hid_close(hid_device*) {}
extern "C" int hid_get_product_string(hid_device*, wchar_t*, size_t) {return -1;}
extern "C" int hid_get_serial_number_string(hid_device*, wchar_t*, size_t) {return -1;}
std::string StringUtils::wstring_to_string(const std::wstring) {return "";}
int main() {
    LianLiUniHubSLInfinityController driver(nullptr,"fake","fake");
    assert(reports.empty());
    std::vector<RGBColor> green(48, ToRGBColor(0,255,0));
    driver.SetChannelLEDs(2,green.data(),48,1.0f);
    assert(reports.size()==4);
#ifdef _WIN32
    assert(control_outputs==1);
#endif
    assert(reports[0].feature && reports[0].data==std::vector<unsigned char>({0xe0,0x10,0x61,0,0,0,0}));
    assert(reports[1].feature && reports[1].data==std::vector<unsigned char>({0xe0,0x10,0x60,2,3,0,0}));
    assert(!reports[2].feature && reports[2].data.size()==353);
    assert(reports[2].data[0]==0xe0 && reports[2].data[1]==0x32);
    for(size_t i=2;i<146;i+=3) {
        assert(reports[2].data[i]==0 && reports[2].data[i+1]==0 && reports[2].data[i+2]==255);
    }
    for(size_t i=146;i<353;i++) assert(reports[2].data[i]==0);
    assert(reports[3].feature && reports[3].data==std::vector<unsigned char>({0xe0,0x12,1,2,0,0,0}));
    reports.clear(); driver.SetChannelLEDs(0,nullptr,0,1.0f); assert(reports.empty());
    reports.clear();
    driver.SetChannelLEDs(3,green.data(),48,1.0f);
    assert(reports.size()==2 && !reports[0].feature); // Fan count setup is cached for the shared physical port.
    reports.clear();
    // Vendor independent inner/outer animations: rainbow=5, spectrum=4, breathing=2.
    for(unsigned int effect : {5u,4u,2u}) {
        mode animation{};
        animation.value=effect; animation.speed=2; animation.brightness=4;
        animation.direction=0;
        if(effect==2) animation.colors.assign(6,ToRGBColor(128,0,255));
        for(unsigned char channel : {0,1,2,3,4,5}) {
            reports.clear();
            driver.SetChannelMode(channel,animation,channel/2==0?2:channel/2==1?1:0);
            assert(reports.size()==2 || reports.size()==3);
            if(reports.size()==3) {
                assert(reports[0].feature && reports[0].data[2]==0x60);
                assert(reports[0].data[3]==1+channel/2);
                assert(reports[0].data[4]==(channel/2==0?3:channel/2==1?2:1));
            }
            assert(!reports[reports.size()-2].feature && reports[reports.size()-2].data.size()==353);
            assert(reports.back().feature && reports.back().data==std::vector<unsigned char>({0xe0,static_cast<unsigned char>(0x10+channel),static_cast<unsigned char>(effect),0,0,0,0}));
        }
    }
    reports.clear();
    LianLiUniHubSLInfinityController failing(nullptr,"fake","fake");
    fail_feature=true;
    failing.SetChannelLEDs(0,green.data(),48,1.0f);
    assert(reports.size()==1 && reports[0].data[2]==0x61 && logged_errors==1);
    fail_feature=false;
    reports.clear();
    {
        RGBController_LianLiUniHubSLInfinity gui(new LianLiUniHubSLInfinityController(nullptr,"fake","fake"));
        assert(reports.empty());
        unsigned int cached_count=0;
        // Leave preceding channels empty to exercise sparse zone iteration.
        for(unsigned int count : {1u,15u,16u,17u,31u,32u,48u,64u,96u}) {
            for(auto& z:gui.zones) {z.flags|=ZONE_FLAG_MANUALLY_CONFIGURED_SIZE; z.leds_count=0;}
            gui.zones[4].leds_count=count;
            gui.SetupZones();
            for(auto& color:gui.colors) color=ToRGBColor(128,0,255);
            for(int selected_mode : {0,4}) {
                gui.active_mode=selected_mode;
                reports.clear(); gui.DeviceUpdateLEDs();
                unsigned int starts=0;
                for(const auto& report:reports) {
                    if(report.feature && report.data[2]==0x60) {
                        assert(report.data[3]==3);
                        assert(report.data[4]==(count+15)/16);
                        ++starts;
                    }
                }
                assert(starts==(cached_count!=(count+15)/16 ? 1u : 0u));
                cached_count=(count+15)/16;
                assert(reports.back().data[2]==(selected_mode==0?1:5));
            }
        }
        for(auto& z:gui.zones) z.leds_count=0;
        gui.SetupZones(); reports.clear(); gui.DeviceUpdateMode(); assert(reports.empty());
    }
    reports.clear();
    unsigned int errors_before=logged_errors;
    fail_output=true;
    driver.SetChannelLEDs(0,green.data(),48,1.0f);
    assert(logged_errors==errors_before+1);
    fail_output=false;
    std::cout<<"Native SL-Infinity lighting feature/output transport and RBG colour checks passed.\n";
}