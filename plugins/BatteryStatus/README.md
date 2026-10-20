# Battery Status (fork-only, experimental)

This optional API-v5 plugin adds a submenu to OpenRGB's existing tray icon. It reads copied core snapshots; the plugin has no USB handles or hardware worker. It shows unknown/last-known/current data explicitly and defaults notifications off.

The matching fork core is required for telemetry. On an older API-v5 core the plugin loads but explains that battery support is unavailable. Existing API-v5 plugins remain compatible.

## Current acquisition status

- Corsair Dark Core RGB Pro SE: passive battery notification parser in the existing, matched receiver reader. No additional USB request or reader is created. Automatic control GET0F was removed after reproducing a late-reply collision with native GET40 inactivity. The exact passive frame was observed on the owner receiver (63%, then 62%) and this matched path is enabled by default; an explicit hardware-setting opt-out remains available. Native idle support must already be available; this feature never enables it itself.
- ASUS ROG Delta II: strict parsers and one exclusive Windows vendor-interface owner, scheduled by the core while monitoring is subscribed. Automatic level polling remains disabled by the compiled ReceiverLevelValidated gate until owner-firmware sleep-safe behavior is established. The automatic path queries only level; charging remains unknown until separately safe acquisition is validated. The manual probe refuses an absent, ambiguous, wrong-descriptor or busy interface. A valid awake battery reply does not establish safe background polling.

One awake Delta II exchange through the owner's receiver succeeded (89%, not charging); sleep-safe operation, coexistence with ASUS readers, and charging transitions remain unvalidated. Corsair passive receiver notifications have now been observed and decoded on the owner hardware; charging remains unknown. This is not a completed two-device release. Do not enable validation flags merely to make the menu show a percentage.

## Development and trial

From the repository root in PowerShell:

```powershell
./plugins/BatteryStatus/Build.ps1
./tests/battery-status/Run-Tests.ps1
./scripts/Build-Battery-Core.ps1
./scripts/Prepare-Battery-Trial.ps1
```

Scripts default to the user's existing Qt 6.8.3 / MinGW 13.1 toolchain and library paths. Build output is isolated under ignored build-battery/. The core build generates the same MinGW portability shims used by the prior personal build rather than modifying vendored sources. The software suite never opens a device. It covers shared monitoring subscriptions, an interruptible provider backoff, and Windows cancellation that retains a stalled operation rather than hanging unload or freeing driver-owned storage. It also checks the installed effects DLL's metadata/interface load and unload without giving it a hardware host; real effects behavior still needs a full-app trial.

Preparation copies the installed settings, profiles, effects plugin and runtime libraries into an isolated trial folder. It does not install or launch anything, overwrite the installed app, change startup tasks or touch Fan Control. Keep the trial manifest and original installation as rollback materials.

`./scripts/Start-Battery-Trial.ps1` starts with `--nodetect --noautoconnect`, for UI inspection only. `-Hardware` refuses to start while another OpenRGB instance exists. Before a hardware trial, exit installed OpenRGB through its tray; keep Fan Control running. Exit the trial through its own tray when finished and restart the normal installed launcher. No services are stopped by these scripts. Re-detection may require the owner's usual ASUS service procedure; do not infer authorization to stop services.

For Delta II validation only after owner confirmation that it is on and awake:

```powershell
./scripts/Invoke-Battery-Probe.ps1 -Mode Awake -ConfirmHeadsetOn
```

`-Mode Inspect` checks the exact interface and descriptor without sending device commands, and includes the Windows error on refusal. `-Mode Observe` sends no commands and records at most sixteen vendor input reports over about two seconds. Both modes require exclusive ownership of the exact collection, have a six-second process deadline, and never retry automatically. Logs stay under build-battery/probe/. Do not run active checks against a sleeping headset without explicit approval for that bounded experiment.

The unverified Corsair notification diagnostic is emitted at debug level at most once per minute when monitoring is subscribed. A full-app trial can collect this through the existing receiver owner without another reader or battery GET. The setting BatteryStatusHardware.corsair_notifications_validated defaults true for the validated, narrowly matched passive path; setting it false disables publication. Charging remains unknown until independently validated.

## Validation still required

Owner-device framing, notification delivery, battery values, charging, sleep/disconnect behavior, headset audio continuity, full-app effects behavior and short mouse idle/wake regressions. Native 15-minute firmware sleep remains pending; no battery-life improvement or other-platform hardware claim is made. No upstream issue, PR or MR is part of this local work.

## Manual Delta II refresh

Matching fork builds now expose a separate optional refresh capability without changing the battery-v1 or OpenRGB API-v5 vtables. Under the Delta II tray submenu, choose **Refresh headset battery while awake...**, then confirm that it is powered on and awake. Cancel sends no request. The core performs one bounded exclusive battery/charging exchange on its own thread; repeated clicks cannot queue extra reads.

This is manual-only: the last reading remains labeled **last known**, with its age. No timer, resume event, profile load, plugin reload or subscriber change authorizes another manual read. Disable monitoring or unload the final subscriber to join in-flight core work and discard a late result. System suspension also stops manual work; resuming does not repeat it.

If ASUS owns the receiver, the refresh reports unavailable and retains the previous reading. Closing the visible widget may not release background ASUS ownership. The plugin does not stop vendor services, close foreign handles or compete with another reader. There are no headset low-battery alerts based on retained manual readings. Old fork hosts retain cached display but do not show the refresh action.

The new manual path requires a live full-app trial before it can be declared owner-hardware validated. Automatic headset polling remains disabled; no no-wake claim follows from manual refresh support.
