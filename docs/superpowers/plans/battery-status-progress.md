# Battery Status progress
Plan: 2026-10-19-battery-status.md
Execution: native, following explicit approval to implement; no additional plan approval round.
Pre-flight: Tasks 1/2 share token lifecycle and copied JSON; Tasks 1/3/4 share core-only manager. Interfaces agree.
Decision: device hardware validation stays disabled by default until owner-hardware evidence exists; live hardware tests are separate from fake transport tests.

Task 1: complete. Cache tests passed for subscriptions, malformed values, stale and disconnected samples. API-v5 interface header unchanged; cross-DLL dynamic_cast validated using the actual built Qt plugin with old/new fake host shapes.
Task 2: complete for software behavior. Plugin DLL builds with existing Qt/MinGW; presentation, alerts, toggles, profile neutrality, repeated Load/Unload and host teardown-order tests pass without hardware.
Decision: keep Qt menu/page objects until plugin-root destruction because PluginManager calls Unload before GUI removal. Regression reproduced the original dangling-menu failure and passes after the fix.
Hardware validation and installed effects/full-core compatibility remain pending.

Task 3: software tests pass. Actual controller compiles under fake HID and deterministic clock; baseline native idle/wake/offline regressions plus new battery rate/range/route/error/validation tests pass. Battery GETs require checked software restoration, valid recent native activity, subscription, exact receiver/child match and explicit hardware-validation setting. Charging is deliberately unknown until separately validated.
Task 4: strict parsers and exclusive Windows validation owner implemented. Automatic Delta II acquisition stays closed; confirmation=false test opens no interface. The manual probe uses exact vendor collection/descriptor checks, exclusive CreateFile ownership and an outer six-second process deadline. No owner-hardware evidence yet; headset automatic provider/worker is intentionally not enabled and Task 4 remains incomplete for real battery operation.
Decision: use exclusive native Windows HID access for the otherwise unowned ASUS collection; bundled HIDAPI does not expose an exclusive-open option. Reuse OS metadata/descriptor APIs and pure protocol parser; do not add another shared reader.
Task 5: full-core build in progress. Reused 901 existing object files after byte-comparing every qmake-listed source/header dependency for each target and matching compiler flags. Excluded targets using changed version metadata; preserved every newly compiled object. Reuse manifest retained in ignored build-battery output.
