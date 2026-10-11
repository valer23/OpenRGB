#include "LianLiUniHubSLInfinityController.h"
#include "StringUtils.h"
#include <cassert>
#include <iostream>
#include <vector>
struct Report { bool feature; std::vector<unsigned char> data; };
std::vector<Report> reports;
unsigned int logged_errors=0;
bool fail_feature=false;
extern "C" int hid_write(hid_device*, const unsigned char* data, size_t size) {
    reports.push_back({false,{data,data+size}}); return static_cast<int>(size);
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
    assert(reports.size()==3 && reports[0].data[2]==0x60); // Sync is disabled once per instance.
    reports.clear();
    LianLiUniHubSLInfinityController failing(nullptr,"fake","fake");
    fail_feature=true;
    failing.SetChannelLEDs(0,green.data(),48,1.0f);
    assert(reports.size()==1 && reports[0].data[2]==0x61 && logged_errors==1);
    std::cout<<"Native SL-Infinity lighting feature/output transport and RBG colour checks passed.\n";
}