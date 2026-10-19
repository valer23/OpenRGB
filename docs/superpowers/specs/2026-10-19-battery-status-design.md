# Battery Status plugin design

Date: 2026-10-19. Status: approved by owner in chat; implementation in progress.

## Purpose and scope

Show the Corsair Dark Core RGB Pro SE and ASUS ROG Delta II battery information under OpenRGB's existing tray icon, without vendor applications. Both devices use their wireless USB receivers. Ship an optional plugin plus the minimum fork-local core support needed for safe telemetry. Windows is the first supported and tested platform.

Preserve native mouse idle handoff, offline traffic suppression, checked lighting restoration, effects and installed configuration. Fan Control must remain running. No audio, firmware, pairing, sleep-timeout, fan or unrelated service changes. No public issue, PR, MR or push is part of this design stage.

## Inspected state and base

The primary checkout is kfa2-ex-captured at 60c02a64, with untracked .agents content. Leave it alone. The clean existing fork worktree is fix/corsair-mouse-idle at cb1385facc3337b0db7f9956624dbc111a80be38. Base the isolated codex/battery-status branch on that commit, not on the primary branch or upstream master.

The installed OpenRGB-Custom.exe SHA256 was checked and matches the handoff: 1080F2E6CEEC2232C733786F458DD4C825248006ED5B9A49C60E13344BE41888. OpenRGB-Custom and FanControl were running during inspection. No hardware queries were issued.

The base exposes plugin API version 5. GetTrayMenu() is already inserted into the main tray menu by qt/OpenRGBDialog/OpenRGBDialog.cpp. There is no generic battery snapshot interface. The Corsair V2 controller owns its control handle and matched receiver event handle, with mouse_lighting_mutex and existing keepalive/update scheduling. No Delta II controller or 0x1AFA detector was found in the base Controllers tree.

## User-visible behavior

Battery Status is an optional submenu returned by the plugin's existing GetTrayMenu(). Each of the two named devices has a percentage when valid, charging only when validated, and a status/age line. Illustrative text: Dark Core RGB Pro SE: 76% (last updated 2 min ago). This is an example, not a device reading.

Represent unknown, current, stale, idle/onboard lighting, sleeping when explicitly known, disconnected, and unavailable/error honestly. Receiver attached does not prove the wireless child is awake. A missing reply is not proof of sleep; idle handoff is not proof of firmware sleep. Keep the previous valid number with an explicit last-known label when freshness or reachability changes. Never turn a timeout, malformed reply or unknown charging flag into 0% or not-charging.

Proposed defaults (builder choices for owner review): acquire at most once per 60 seconds where safe; UI cache refresh once per second; mark a sample stale after 120 seconds; notifications off initially. An optional low-battery checkbox uses a 20% threshold and 25% rearm threshold, triggers once per discharge episode on a fresh trustworthy value, and suppresses charging, stale and disconnected notifications. No forced USB refresh action. Opening the menu reads cached data only.

Plugin settings use the existing settings API under a BatteryStatus key. Battery values are runtime data, not profile data; loading an RGB profile must not reset telemetry or cause a battery transaction. In-memory values are cleared on process restart rather than presenting persisted values as fresh.

## Architecture and compatibility

Use three parts: device owners acquire data, a small core-owned cache holds snapshots, and the optional Qt plugin renders them. The plugin contains no HIDAPI or raw USB calls, no device handles, and no hardware worker thread.

Prefer a separate versioned C++ capability interface (OpenRGBBatteryStatusInterface) implemented by OpenRGBPluginAPI as an additional base. The plugin discovers it by dynamic_cast from the existing polymorphic OpenRGBPluginAPIInterface. Keep API-v5 methods, their vtable and RGBControllerInterface unchanged; do not globally bump to API 6 or rebuild all effects merely to add telemetry. Build host and new plugin with the same toolchain/runtime and RTTI enabled. Before hardware work, prove this cross-DLL cast succeeds with the new host and fails safely on the older host. Existing API-v5 plugins must still load and run. If this mechanism is incompatible with the shipped build, revisit the extension design before altering ABI.

The side interface has a capability/schema version, acquire/release monitoring subscription, and GetBatterySnapshots() returning copied JSON values using the project's existing JSON types. Values include opaque stable device ID, name, optional 0..100 percentage, charging tri-state, device availability, data quality/error, and sample age. Track sample timing with steady_clock internally. Do not expose controller pointers, handles or callbacks into unloadable plugin code. Separate availability from freshness so a disconnected last-known sample is representable. JSON is small (two devices); no general telemetry framework or SDK-network protocol extension is needed.

The core cache mutex protects only copied state. Never hold it during USB, device-list locks or Qt calls. Workers publish values after USB transactions. UI calls never take a device I/O lock. Providers register/unregister with the owning core lifetime; controller removal invalidates availability without retaining a pointer in the plugin.

Monitoring runs only while the plugin has subscribed and monitoring is enabled. Subscribe schedules work; it never queries synchronously on the GUI thread. On unsubscribe cancel battery-only work and join the ASUS worker before closing its handle. Corsair's existing lighting/event workers and idle behavior continue unchanged. Plugin unload stops its UI timer before releasing monitoring and destroying its menu. Failed/disabled providers retain a visible explanatory state without retry storms.

## Corsair acquisition and power behavior (review revision)

The code review and actual-driver delayed-response regression established that GET0F and GET40 replies have indistinguishable headers. A delayed battery reply can be interpreted as hundreds of seconds of inactivity. A pre-read drain cannot eliminate replies that arrive later. Active battery queries have therefore been removed entirely from the core; preserving native idle behavior takes precedence over immediate battery values.

Reuse only the already-owned, matched receiver event handle. A candidate notification has child 1, notification family 1, property 0F, reserved byte 0, and a little-endian tenths-of-percent value in bytes 4..5. This comes from the pinned OpenLinkHub notification implementation and still requires owner-hardware validation. Reject wrong child, short frames, control GET replies and values above 1000. Charging remains unknown. Do not open another reader, send notification-enabling commands, or change existing polling/handoff cadence.

Only publish while the plugin is subscribed, the existing receiver connection is synchronized and the mouse is connected. A passive event does not imply wake; retain idle status after handoff. Following owner hardware validation (captured 63%, 62% and 61% notifications and confirmed tray display), BatteryStatusHardware.corsair_notifications_validated defaults true; explicit false remains an opt-out. Before that validation, frames were diagnostic candidates rather than percentages. Log at most one candidate per minute during the owner's trial. No battery GET is ever sent, even when this setting is true. If no usable notification occurs, show unknown/last-known rather than adding unsafe control requests.

This revision was selected after the independently reviewed GET approach failed a reproducing test. The shared cache/interface/UI architecture is unchanged. Passive notification framing and delivery have now been observed on the owner's receiver, and the owner confirms the mouse percentage is visible.

## Delta II acquisition and power validation gate

Add a narrowly matched core battery-only provider for 0B05:1AFA, not a fabricated RGB controller/LED zone. Reuse bundled HIDAPI and existing detection/identity helpers where applicable. Verify interface 03 collection 04, vendor usage page 0xFF00, report descriptor and report lengths; VID/PID alone is insufficient. Refuse ambiguous matches. One core owner has one handle and serialized request/reply processing. No second process or plugin reader. Report busy/unavailable rather than stealing a handle or stopping a vendor service. HID sharing may not provide exclusive ownership; document and validate the selected Windows handle/sharing behavior rather than claiming it prevents other applications opening the device.

Primary G-Helper source DeltaII.cs identifies this PID and inherits AsusHeadset. AsusHeadset.cs uses 64-byte reports beginning CC 12 07 for level at byte 6, and CC 12 08 for charging at byte 5 (including report ID). Match report ID and command/key, validate lengths and range, reject FF AA error replies at the documented positions. Route intervening unsolicited events through the single owner with a bounded report/time budget; do not blindly drain meaningful events. No audio/lighting/settings setters are allowed.

These sources establish candidate framing, not this owner's firmware behavior or a no-wake guarantee. Automatic Delta II requests remain disabled until safe acquisition is validated. Receiver enumeration, the presence of an audio endpoint, a recent sample, and a timeout do not prove the headset is awake.

First use passive receiver reports if their battery and presence meanings can be validated. An undocumented CC 12 09 event is only a research lead; do not present its payload as battery without validation. Active GETs require either verified receiver-local cached semantics that do not contact/wake the sleeping headset, or independently verified awake-state evidence valid for the transaction. If neither is available, retain passive/last-known/unknown status and stop active polling. Do not weaken the no-wake requirement to obtain a live number.

After design approval, establish awake-state framing with a bounded trial while the owner confirms the headset is on. Test receiver-only/offline and sleep behavior using passive observation first. If proving active-query sleep behavior would risk waking it, seek explicit authorization for that bounded experiment before issuing it. A successful battery reply alone does not pass the no-wake gate. Until the gate passes, headset support must remain labeled experimental/unavailable for automatic acquisition; the full two-device project is not complete merely because the mouse works.

If active acquisition is proven safe, use one cancellable core worker for this otherwise unowned device, with a 60-second deadline and finite per-attempt timeout (initial cap 250 ms per transaction and 500 ms per pair). A slower firmware response requires an evidence-based bounded adjustment. Suspend on system suspend/disconnect and require revalidation on resume/reconnect. Passive listening uses finite reads so unload never hangs. No unbounded loops, command scans or probe sweeps.

## Implementation and validation sequence after approval

1. Prove the optional capability interface and existing plugin compatibility with fake snapshots, then implement the cache and tray plugin. Exercise enable/disable/unload, profile loads, GUI responsiveness and stale/unknown rendering without hardware access.
2. Add passive Corsair telemetry in the existing owner. Use meaningful mocked transport tests for zero added battery requests, offline/handoff/recovery behavior, keyed notification framing, malformed/error reports and unchanged idle transitions. Reuse the existing mouse regression harness where available; record any harness gaps.
3. Validate Delta II framing and safe power behavior as above before enabling its provider. Record actual reports, firmware/version and observed state; do not infer owner-hardware validation from another project's results.
4. Build core and plugin with the existing Windows toolchain. Run existing effects and short mouse handoff/movement/immediate-input regressions, plus headset audio continuity, receiver disconnect/reconnect, charging where supported, sleep/resume and plugin unload checks. Tests must distinguish new failures from the known counter-resolution limitation. Keep Fan Control running throughout.
5. Prepare an isolated full-app trial and backup/rollback materials. Before replacing an active installed app/plugin, ask the owner to exit OpenRGB through its tray, per the handoff. Preserve all profiles, existing plugins, CorsairMouseIdle settings, fan layout and GPU identity migration. Do not install automatically after a successful build. No repeated 17-minute tests; native full firmware sleep remains explicitly pending unless separately observed.

Completion requires both owner devices to provide validated battery values, safe unavailable/sleep behavior, optional-plugin lifecycle correctness, preserved lighting/idle behavior, and a reviewed Windows build/trial. If Delta II no-wake acquisition cannot be established, report that blocker clearly with the partial results; do not label the full request complete.

## Source references

- Local handoff and chat: OPENRGB-BATTERY-PLUGIN-HANDOFF.md; local chat 01a12179-cc52-78d1-bef5-2240c6dab5be, Reset FanControl hardware detection.
- Core base cb1385fa: OpenRGBPluginInterface.h, OpenRGBPluginAPI.{h,cpp}, PluginManager.cpp, qt/OpenRGBDialog/OpenRGBDialog.cpp, Controllers/CorsairPeripheralV2Controller/*.
- Local pinned ckb-next 833ab50951e230674bda02e8448ef6ef365dfd81: src/daemon/device_bragi.c, bragi_get_battery_info; OpenLinkHub c8c1d2a99a56aa1d907e43137d127c2048f41ba5: src/devices/darkcorergbproseW/darkcorergbproseW.go. Stored beneath the handoff workspace's mr-corrections/corsair-notification-research directory.
- https://github.com/seerge/g-helper/blob/dd2dced63fbd6e5abb1a50d88b738f7159f5e71d/app/Peripherals/Headset/Models/DeltaII.cs
- https://github.com/seerge/g-helper/blob/dd2dced63fbd6e5abb1a50d88b738f7159f5e71d/app/Peripherals/Headset/AsusHeadset.cs (ReadBattery, ParseBattery, ParseCharging, WriteForResponse). Public reference revision pinned during design inspection: dd2dced63fbd6e5abb1a50d88b738f7159f5e71d. Do not copy unrelated device initialization or audio control.
- https://github.com/dvalfrid/rigstats/blob/main/docs/control-architecture.md documents matching framing and an unvalidated-for-us notification lead. It does not establish this owner's no-wake behavior.

## Decisions left to the builder

Names of internal helpers, exact file split, core provider registration hook, and test harness integration should follow the base code. Keep the cache/side interface small. If safe headset acquisition or ABI compatibility requires changing the architecture or user constraints, return with the evidence before building that alternative.

## Design review

A fresh independent builder check found no important unanswered planning questions and no contradiction with the no-wake or no competing reader constraints. Hardware and build behavior remain untested at this design-only stage.
