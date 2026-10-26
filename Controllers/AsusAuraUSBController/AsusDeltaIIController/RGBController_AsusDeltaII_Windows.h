/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBController.h"
#include "OpenRGBDeviceAwakeSave.h"
#include "AsusDeltaIIControl.h"
#include "AsusDeltaIIAudioSource_Windows.h"
#include <condition_variable>
#include <optional>

class RGBController_AsusDeltaII : public RGBController, public OpenRGBDeviceAwakeSave
{
public:
    RGBController_AsusDeltaII(const std::string& path,const std::string& device_serial);
    ~RGBController_AsusDeltaII() override;
    void SetupZones();
    void ResizeZone(int zone,int new_size) override;
    std::shared_ptr<Operation> PrepareAwakeSave() override;
private:
    using Time=AsusDeltaIIControl::Time;
    using Backend=AsusDeltaIIControl::Backend;
    struct Request
    {
        std::shared_ptr<Operation> operation;
        Time confirmed;
        int value;
        unsigned brightness;
        AsusDeltaIIProtocol::RGB colour;
        Backend backend;
    };
    struct Lifetime {std::mutex mutex;RGBController_AsusDeltaII* owner=nullptr;};
    std::shared_ptr<Lifetime> lifetime;
    AsusDeltaIIDirectCommands direct;
    AsusDeltaIIControl control;
    AsusDeltaIIAudioSource audio;
    Backend backend=Backend::AsusService;
    std::optional<Request> pending;
    bool stop_requested=false,closing=false,streaming=false;
    int applied_value=-1;
    unsigned applied_brightness=0;
    Time last_audio{};
    std::mutex close_mutex;
    std::condition_variable close_ready;
    bool closed=false;
    void QueueAwake(const std::shared_ptr<Operation>& operation,Time confirmed);
    void StopOnWorker();
    void CancelPending(const std::string& reason);
    void Apply(Request request);
    void DeviceUpdateLEDs() override;
    void DeviceUpdateZoneLEDs(int zone) override;
    void DeviceUpdateSingleLED(int led) override;
    void DeviceUpdateMode() override;
    void DeviceSaveMode() override;
    void DeviceUpdateDeviceSpecificConfiguration() override;
};