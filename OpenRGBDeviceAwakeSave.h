/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

// Optional local-UI action interface. This does not alter RGBController's ABI.
// Preparing an operation must not acquire a device or send commands.
class OpenRGBDeviceAwakeSave
{
public:
    class Operation
    {
    public:
        struct Result {bool done=false,success=false;std::string message;};
        explicit Operation(std::string prompt={}) : prompt(std::move(prompt)) {}
        const std::string& Prompt()const{return prompt;}
        void SetConfirmation(std::function<void()> action)
        {std::lock_guard<std::mutex> lock(mutex);confirmation=std::move(action);}
        void Confirm()
        {
            std::function<void()> action;
            {std::lock_guard<std::mutex> lock(mutex);if(confirmed || result.done)return;confirmed=true;action=std::move(confirmation);}
            if(action)action();
        }
        void Complete(bool success,const std::string& message)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if(result.done)return;
            result={true,success,message};confirmation={};
        }
        Result Read(){std::lock_guard<std::mutex> lock(mutex);return result;}
        void Cancel(){cancelled=true;Complete(false,"Headset lighting operation cancelled");}
        bool Cancelled()const{return cancelled.load();}
    private:
        const std::string prompt;
        std::mutex mutex;
        std::function<void()> confirmation;
        Result result;
        bool confirmed=false;
        std::atomic<bool> cancelled{false};
    };
    virtual ~OpenRGBDeviceAwakeSave()=default;
    virtual std::shared_ptr<Operation> PrepareAwakeSave()=0;
};