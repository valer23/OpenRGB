# Battery Status software validation - 19 October 2026

Status: software built and reviewed; owner-hardware validation pending. Neither device has a validated live battery reading yet.

- Feature branch: codex/battery-status, isolated managed worktree, base cb1385fa.
- Built core product-source revision: 5bbeecd23f6bd5cc58029697f8a4d35ce524bcab (subsequent tests/documentation changes do not alter the product code).
- Full Windows Qt 6.8.3 / MinGW 13.1 core build: exit 0. Existing installed-build MinGW portability shims generated locally, no vendored source edits.
- Plugin DLL build: exit 0.
- Core version-only startup smoke: exit 0, before hardware initialization.
- Full software suite: cache/subscription/freshness/invalid values; tray presentation and alert thresholds; actual plugin DLL old/new host RTTI and lifecycle; installed effects DLL metadata/interface load/unload; actual Corsair controller with fake HID; strict parsers and disabled ASUS gate; both reproduced review regressions all pass.
- Both review findings fixed: no active Corsair battery GETs remain, eliminating late GET0F/GET40 mixing and the associated disconnect-state overwrite. Passive receiver notifications need separate owner validation.
- One pre-existing Qt/MinGW duplicate metatype-section warning remains, matching prior installed build logs. No new compilation error.
- Isolated trial prepared under build-battery/trial. All 11 profiles are byte-identical to the installation; non-battery settings compare equal. Both hardware validation gates remain closed. No trial was launched against hardware.
- Installed executable remains SHA256 1080F2E6CEEC2232C733786F458DD4C825248006ED5B9A49C60E13344BE41888.
- Trial executable SHA256 2DBAFE79496CE53E1D60A60695257CD917947EF5489950A636BC4EC46F222464.
- Battery plugin SHA256 8A648D52B90DC0779285816FEB427DA0287B3D22C412EA25E7A9D8DDDC4BE18F.
- Installed OpenRGB and Fan Control remained running. No installation, service stop, startup change, public post, PR/MR or push occurred.

## Important limit

A successful awake Delta II read would validate framing and the reported value only. It would not establish no-wake background polling. A Corsair notification parser passing fake-HID tests does not establish the owner's receiver emits that frame. The UI will correctly show unknown/unavailable until evidence is collected.

The next step is the pending owner-confirmed awake headset read. A later isolated full-app mouse trial requires the owner to exit normal OpenRGB through its tray so there is one receiver owner; Fan Control stays running. Never start a second hardware-detecting OpenRGB instance. The trial launcher enforces this guard. Native 15-minute firmware sleep remains pending; no repeated long wait is required for this software stage.

See plugins/BatteryStatus/README.md and the trial/probe scripts for the concrete commands and safety guards.
