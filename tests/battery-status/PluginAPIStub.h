#pragma once
#include "OpenRGBPluginInterface.h"
class PluginAPIStub : public OpenRGBPluginAPIInterface
{
public:
    nlohmann::json preferences = {{"enabled",true},{"notifications",false}};
    void LogEntry(const char* filename, int line, unsigned int level, const char* fmt, ...) override {  }
    RGBControllerInterface* CreateVirtualRGBController(RGBController_Setup* setup) override { return {}; }
    void DeleteVirtualRGBController(RGBControllerInterface* rgb_controller) override {  }
    void RegisterVirtualRGBController(RGBControllerInterface* rgb_controller) override {  }
    void RegisterVirtualRGBControllerInThread(RGBControllerInterface* rgb_controller) override {  }
    void UnregisterVirtualRGBController(RGBControllerInterface* rgb_controller) override {  }
    void UpdateVirtualRGBController(RGBControllerInterface* rgb_controller, RGBController_Setup* setup) override {  }
    void UnregisterVirtualRGBControllerInThread(RGBControllerInterface* rgb_controller) override {  }
    void ClearActiveProfile() override {  }
    std::vector<std::string> GetProfileList() override { return {}; }
    bool LoadProfile(std::string profile_name) override { return {}; }
    bool SaveProfileFromPlugin(std::string profile_name, std::string plugin_name, nlohmann::json plugin_data) override { return {}; }
    filesystem::path GetConfigurationDirectory() override { return {}; }
    bool GetDetectionEnabled() override { return {}; }
    unsigned int GetDetectionPercent() override { return {}; }
    std::string GetDetectionString() override { return {}; }
    void RescanDevices() override {  }
    void WaitForDetection() override {  }
    std::vector<RGBControllerInterface*> GetRGBControllers() override { return {}; }
    nlohmann::json GetDeviceDescriptionJSON(RGBControllerInterface* controller) override { return {}; }
    nlohmann::json GetLEDDescriptionJSON(led led) override { return {}; }
    nlohmann::json GetMatrixMapDescriptionJSON(matrix_map_type matrix_map) override { return {}; }
    nlohmann::json GetModeDescriptionJSON(mode mode) override { return {}; }
    nlohmann::json GetSegmentDescriptionJSON(segment segment) override { return {}; }
    nlohmann::json GetZoneDescriptionJSON(zone zone) override { return {}; }
    RGBControllerInterface* SetDeviceDescriptionJSON(nlohmann::json controller_json) override { return {}; }
    led SetLEDDescriptionJSON(nlohmann::json led_json) override { return {}; }
    matrix_map_type SetMatrixMapDescriptionJSON(nlohmann::json matrix_map_json) override { return {}; }
    mode SetModeDescriptionJSON(nlohmann::json mode_json) override { return {}; }
    segment SetSegmentDescriptionJSON(nlohmann::json segment_json) override { return {}; }
    zone SetZoneDescriptionJSON(nlohmann::json zone_json) override { return {}; }
    bool CompareControllers(RGBControllerInterface* controller_1, RGBControllerInterface* controller_2) override { return {}; }
    std::string DeviceTypeToString(device_type type) override { return {}; }
    bool SetModeValuesFromMode(mode& destination, mode& source) override { return {}; }
    nlohmann::json GetSettings(std::string settings_key) override { return preferences; }
    void SaveSettings() override {  }
    void SetSettings(std::string settings_key, nlohmann::json new_settings) override { preferences = new_settings; }
};
