# Battery Status Implementation Plan

> Execution skill: superpowers:executing-plans. This revision records the reviewed passive Corsair design and the owner's later finish-and-PR authorization; the ledger preserves the original GET proposal and its rejection.

**Goal:** A working optional battery submenu in OpenRGB's existing tray for Corsair Dark Core RGB Pro SE and ASUS ROG Delta II, both through their USB wireless receivers, followed by a PR in the owner's public fork valer23/OpenRGB. No upstream publication.
**Architecture:** Keep the API-v5 ABI intact through a separate RTTI capability interface. Core device owners publish copied snapshots to a bounded cache; Qt never queries hardware.
**Tech Stack:** C++17, Qt6.8.3, MinGW13.1, the existing matched Corsair HIDAPI owner and exclusive Windows HID for Delta II, nlohmann JSON.
**Spec:** ../specs/2026-10-19-battery-status-design.md. **Evidence/decisions:** battery-status-progress.md and ../../battery-status-validation.md.

## Global Constraints

- Keep Fan Control running. Run only one hardware-detecting OpenRGB instance; the owner exited installed OpenRGB for the isolated trial.
- No competing reader and no waking sleeping devices. The owner authorized exactly one separate asleep level-query experiment with observation; this does not authorize unvalidated background queries.
- Preserve native mouse idle, sleep, lighting restoration, API-v5 plugins, settings and profiles.
- Acquire at most once per60s where safe; UI reads copied cache once per1s; mark data stale after120s.
- Alerts default off, threshold20% and rearm25%; suppress stale, disconnected, unknown charging and charging samples.
- Never replace a running trial image or modify the installation. No runtime vendor-service/process termination; explicit one-shot diagnostic helpers preserve initial state and request launcher restoration.
- Both devices must meet safety and battery-value requirements before completion. A software-only gate, a standalone awake reading or a mouse-only result is insufficient.

## Review Focus

- Plugin unload and overlapping enable/disable must not leave a worker or subscription alive (Task1/2 lifecycle tests).
- A stale last-known value must never look current or generate an alert (Task1/2 snapshot tests).
- Event-reader recovery or failed software initialization must not introduce Corsair battery GETs or alter native idle and lighting (Task3 transport tests).
- Malformed/late/unrelated USB reports must never become battery values (Task3/4 parser and transport tests).
- Receiver presence must not enable headset polling or imply the child is awake (Task4 gate tests).
- A cancelled Windows request must retain its buffer, OVERLAPPED and device until completion without an infinite unload wait, a second reader or repeated resource accumulation.

### Task 1: Capability and snapshot cache

Files: OpenRGBBatteryStatusInterface.h, BatteryStatus/BatteryStatusManager.{h,cpp}, OpenRGBPluginAPI.{h,cpp}, OpenRGB.pro, cache and cross-DLL tests.
Interface: version1; AcquireBatteryMonitoring() -> uint64_t token; ReleaseBatteryMonitoring(token); GetBatterySnapshots() -> copied JSON. Two fixed device IDs; internal optional percentage/charging use -1 and serialize as null.
- [x] Test duplicate releases, independent tokens, invalid values, sample age and retained values.
- [x] Implement monotonic, mutex-protected cache without hardware I/O, external pointers or plugin callbacks.
- [x] Add an RTTI side interface without changing the existing API-v5 vtable; prove new-host cast and old-host fallback across the actual plugin DLL.
- [x] Commit and pass the software suite.

### Task 2: Optional tray plugin

Files: plugins/BatteryStatus/, presentation, notification, actual DLL lifecycle and installed Effects compatibility tests.
- [x] Implement two named device menus, explicit last-known/unknown status, age, monitoring and notification toggles, and unsupported-host message.
- [x] Use existing settings and tray object only; no hardware reader, hardware worker, profile telemetry persistence or extra tray icon.
- [x] Stop the UI timer and release monitoring on unload; retain menu/widget lifetime until host loader destruction.
- [x] Test alert thresholds, rearm and suppression; build DLL and prove real host discovery and API-v5 Effects compatibility.

### Task 3: Passive Corsair telemetry in the existing owner

Files: Controllers/CorsairPeripheralV2Controller/, BatteryStatus/BatteryProtocol.h, actual-controller fake-HID/native regression tests and captured passive fixture.
Ruling: remove all active GET0F acquisition because a delayed reply is indistinguishable from GET40 inactivity and corrupts native sleep timing. Do not reintroduce a drain, active refresh, notification-enabling command or extra reader.
- [x] Accept only the exact existing receiver1BA6, wireless mouse1B7E, route09 and notification01010F00 with valid tenths-of-percent range.
- [x] Publish only while subscribed and connected through the synchronized matched event owner; preserve idle/onboard state. Charging remains unknown.
- [x] Reproduce and fix late-reply and disconnect-state regressions; preserve native handoff, offline suppression, child isolation and checked lighting recovery.
- [x] Validate real passive63%,62%,61% frames and owner-confirmed tray percentage; enable the narrowly matched passive path by default with explicit opt-out.
- [ ] Finish current full-app short idle/movement/lighting confirmation. Native15-minute firmware sleep remains a separately recorded hardware check; do not repeat long waits merely to rerun software tests.

### Task 4: Delta II provider and bounded validation tools

Files: BatteryStatus/AsusDeltaIIBattery, AsusBatteryMonitor, BatteryMonitoringService, WindowsHidTransfer, protocol/service/Win32 tests and explicit diagnostic scripts.
- [x] Validate report identity/length/range and exact vendor collection:0B05:1AFA, MI03/COL04, usageFF00/1,64-byte input/output.
- [x] Use one exclusive in-process owner; serialize requests, report busy access, reject ambiguous/wrong collections and never steal another reader.
- [x] Implement bounded native I/O with retained storage if cancellation remains pending; no infinite wait or freed driver-owned buffer.
- [x] Wire shared core subscriptions to one worker, interruptible60s backoff and final-release join. Keep compiled ReceiverLevelValidated=false until hardware evidence.
- [x] Validate one owner-confirmed awake level/charging exchange:89%,charging=false. Do not inject it as current plugin telemetry or infer no-wake from it.
- [x] Prepare exactly-one-level-query asleep experiment with explicit owner confirmation and observed-wake answer before vendor restoration; no charging/settings queries.
- [ ] Obtain sleep-test evidence and owner wake observation. If inconclusive or it wakes the headset, find a safe acquisition source; do not weaken the no-wake constraint.
- [ ] Enable only acquisition established safe by evidence, and verify real headset battery values through the core cache/tray, audio continuity, hotplug and charging semantics where supported.

### Task 5: Build, independent review and isolated trial

- [x] Build full core and plugin with existing toolchain/compatibility shims, without vendored edits. Integrated core revision7db6db7c builds and passes version-only startup.
- [x] Run the hardware-free suite, including actual driver, DLL, protocol, subscription, bounded I/O, simulated service restoration and packaging tests.
- [x] Prepare isolated trial, preserve11 profiles and non-battery settings; owner confirms visible tray menu and usual lighting. Fix missing Effects migration source in future packaging.
- [x] Complete fresh whole-branch review (e1662f81); reproduce/fix the one Important shutdown race and pass the full suite. Later explicit suspend/resume requirement coverage is separately recorded and tested.
- [ ] Update the isolated app only after normal tray exit, complete both-device hardware checks and record evidence. Installed executable/profile state and Fan Control must remain intact.

### Task 6: Completion and fork PR

- [ ] Audit every requirement against current evidence; two-device working state and no-wake behavior remain required.
- [ ] Commit final changes, push only to valer23/OpenRGB on codex/battery-status, and open the requested fork PR against the appropriate existing mouse-fix base. Do not post upstream.
- [ ] Attach and verify the PR URL/state and report remaining material limitations honestly. Mark the goal complete only when the working plugin and PR are both proven.
