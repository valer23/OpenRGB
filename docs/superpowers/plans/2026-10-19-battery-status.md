# Battery Status Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** An optional battery tray plugin with safe core-owned telemetry for the two approved devices.
**Architecture:** Keep the API-v5 ABI intact through a separate RTTI capability interface. Device owners publish copied snapshots to a bounded cache. Qt renders the cache and never queries hardware.
**Tech Stack:** Existing C++17, Qt 6.8.3, MinGW 13.1, bundled HIDAPI, nlohmann JSON.
**Spec:** ../specs/2026-10-19-battery-status-design.md (approved in chat).

## Global Constraints

- Fan Control and installed OpenRGB remain running during development.
- No competing reader; no query intended solely to wake a device.
- Preserve native mouse idle and lighting behavior, API-v5 plugins, settings and profiles.
- Acquire at most once per 60 seconds where safe; UI once per second; stale after 120 seconds.
- Low-battery alerts default off, 20% trigger, 25% rearm; never alert from stale, unknown or charging data.
- Device protocols and safe power behavior require owner-hardware validation before automatic acquisition is enabled.
- No installation or active-app replacement without tray exit; no public posting or push.

## Review Focus

- Plugin unload and overlapping enable/disable must not leave a worker or subscription alive (Task 1/2 lifecycle tests).
- A stale last-known value must never look current or generate an alert (Task 1/2 snapshot tests).
- Event-reader recovery or failed software initialization must block all battery GETs (Task 3 transport tests).
- Malformed/late/unrelated USB reports must never become battery values (Task 3/4 parser and transport tests).
- Receiver presence must not enable headset polling or imply the child is awake (Task 4 gate tests).

### Task 1: Capability and snapshot cache

Files: create OpenRGBBatteryStatusInterface.h, BatteryStatus/BatteryStatusManager.{h,cpp}, tests/battery-status/cache_test.cpp and tests/battery-status/Run-Tests.ps1; modify OpenRGBPluginAPI.{h,cpp}, OpenRGB.pro.
Interfaces: capability version 1; AcquireBatteryMonitoring() -> uint64_t token, ReleaseBatteryMonitoring(uint64_t), GetBatterySnapshots() -> nlohmann::json; manager Get(), Acquire(), Release(token), Enabled(), Publish(id,name,percent,charging,state,detail), SetState(id,name,state,detail), Snapshots(). Use -1 for optional percentage/charging at the internal boundary and JSON null in copied output.
- [ ] Write and run failing cache tests for duplicate release, invalid values, retained sample age, state changes and token isolation.
- [ ] Implement a mutex-protected, core-only singleton cache with two configured slots, monotonic ages, no external pointers/callbacks and no I/O.
- [ ] Add the optional interface as a second API base without changing existing vtables; write real Qt plugin cross-DLL tests with new and old host shapes before hardware work.
- [ ] Run tests and commit the self-contained deliverable.

### Task 2: Optional tray plugin

Files: create plugins/BatteryStatus/BatteryStatusPlugin.{h,cpp}, BatteryStatusPresentation.h, BatteryStatusPlugin.json, BatteryStatus.pro, Build.ps1; tests/battery-status/plugin_test.cpp, abi_host.cpp.
Consumes Task 1 interface. Produces GetTrayMenu() and GetWidget() through the existing plugin interface, with no profile effects or SDK commands.
- [ ] Write and run failing presentation/notification and plugin lifecycle tests using a fake host (zero USB).
- [ ] Implement two device rows with explicit last-known states, monitoring toggle, notification toggle and host-unsupported message. Use existing settings under BatteryStatus; timers refresh only copied values. Stop timer before releasing token on unload; no callback into unloaded code.
- [ ] Implement optional notifications via the existing system tray object if safely discoverable; do not create another tray icon. Unknown charging suppresses alerts. Test discharge threshold/rearm and fresh sample handling.
- [ ] Build DLL and execute offscreen Qt lifecycle/ABI tests, including the installed effects DLL metadata/loading compatibility; commit.

### Task 3: Corsair telemetry in existing owner

Files: modify Controllers/CorsairPeripheralV2Controller/CorsairPeripheralV2Controller.{h,cpp}; create BatteryStatus/BatteryProtocol.h and tests/battery-status/corsair_driver_test.cpp, reuse existing private driver harness/stubs.
Consumes manager publication and subscriptions. Produces guarded GET0F battery values only for receiver 1BA6, mouse PID1B7E, route09, with hardware validation explicitly off by default.
- [ ] Write actual-driver fake HID regressions showing no battery traffic with no subscription, validation disabled, idle handoff, offline, failed native activity, recovery, failed lighting initialization, or stale activity.
- [ ] Add sample scheduling only after existing AllowFrame succeeds and recent valid activity exists. Check subscription/validation/eligibility immediately before the serialized GET. No retry burst or changes to inactivity/handoff scheduling.
- [ ] Validate response route, command, status, minimum length, range and optional charging codes. Initially leave charging unknown until separately validated. Publish transport errors without changing lighting state. Limit attempts to one per 60000 ms.
- [ ] Run existing mouse regressions and new tests; commit.

### Task 4: Delta II guarded provider and bounded validation tools

Files: create BatteryStatus/AsusDeltaIIBattery.{h,cpp}; extend manager ownership/lifecycle only as needed; tests/battery-status/protocol_test.cpp; scripts/documentation for awake validation.
Consumes cache and core lifecycle. Produces a narrowly matched battery-only owner with automatic active queries disabled until no-wake behavior is proven. First inspect bundled HID capabilities and exclusive ownership options; never assume a shared handle prevents other readers.
- [ ] Write failing parsers for CC1207 level at byte6 and CC1208 charging at byte5, short reports, error forms, out-of-range values and unknown charging.
- [ ] Implement pure parsers and a default-closed safety gate. Any passive listening must refuse ambiguous identity or competing ownership; no interpretation of undocumented CC1209 as battery.
- [ ] Implement a bounded, manually invoked awake validation path only after owner confirms the headset is on and only with a single exclusive owner. Do not use this path for background polling. Keep queries disabled if safety cannot be proven.
- [ ] Record unresolved no-wake evidence honestly. Run software tests, commit.

### Task 5: Windows build, review and trial preparation

Files: build script under scripts/, progress/validation notes under docs/.
- [ ] Build full core and plugin with the existing toolchain and compatibility libraries; logs remain in ignored build output. No hardware app launch during compilation.
- [ ] Run software suite and independent whole-branch review. Fix important findings with reproducing tests.
- [ ] Prepare isolated settings/plugin trial without modifying installed files. Ask owner for awake-state confirmation and any required tray exit only when the trial is concrete and ready.
- [ ] Record outstanding owner hardware checks. Project remains incomplete until both devices satisfy the approved safety and battery-value requirements.
