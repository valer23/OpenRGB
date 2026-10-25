#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
inline std::atomic<unsigned> recovery_shadow_events{0};
inline bool recovery_shadow_accept=true;
class DetectionManager
{
public:
 static DetectionManager* get(){static DetectionManager manager;return &manager;}
 bool TryDetectCorsairFromInput(std::string,std::chrono::steady_clock::time_point,
                               std::function<bool()> current,std::function<void(bool)> finished)
 {
  if(!recovery_shadow_accept || !current())return false;
  ++recovery_shadow_events;
  finished(true); // Test consumer only: never enumerate/open/query a HID device.
  return true;
 }
};
