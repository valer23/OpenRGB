# Battery Status progress
Plan: 2026-10-19-battery-status.md
Execution: native, following explicit approval to implement; no additional plan approval round.
Pre-flight: Tasks 1/2 share token lifecycle and copied JSON; Tasks 1/3/4 share core-only manager. Interfaces agree.
Decision: device hardware validation stays disabled by default until owner-hardware evidence exists; live hardware tests are separate from fake transport tests.

Task 1: complete. Cache tests passed for subscriptions, malformed values, stale and disconnected samples. API-v5 interface header unchanged; cross-DLL dynamic_cast validated using the actual built Qt plugin with old/new fake host shapes.
Task 2: complete for software behavior. Plugin DLL builds with existing Qt/MinGW; presentation, alerts, toggles, profile neutrality, repeated Load/Unload and host teardown-order tests pass without hardware.
Decision: keep Qt menu/page objects until plugin-root destruction because PluginManager calls Unload before GUI removal. Regression reproduced the original dangling-menu failure and passes after the fix.
Hardware validation and installed effects/full-core compatibility remain pending.
