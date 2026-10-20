# Delta II battery cache investigation — 19 October 2026

## Result

Static inspection establishes a battery cache inside the installed ASUS device service, but does not establish a supported external reader for OpenRGB. Do not enable automatic headset queries on this evidence. No device command, IPC connection, service stop, process termination, or target-code execution was performed during this pass.

## Inspected artifacts and reproducibility

- Installed model6906 service: `C:\Program Files (x86)\ASUS\ArmouryDevice\View\6906\service.js`, SHA256 `ec2433623faa9ca8d3e1226b2c3038302156f4c128741a436dd21243057b3aa9`.
- Installed model6906 `index.js`, and `.webpack/main/index.js` read directly from installed `resources/app.asar` without running the archive.
- Babel AST used to locate the service class function at UTF-16 offsets524794..548630 (end exclusive). An unchanged slice wrapped in parentheses was analyzed as inert JavaScript. The original bundle and external/native bindings were not executed.
- REA retained application evidence `ev_9b4a6fa6a6ad63b2536148ca02fd782b78ada1f52dd37d4eda532126578a9680`; backward property trace `ev_a5010db6d2347b34568753eee32d70c44b101cb6ea776606d8e239dfdd3faa94`.
- Ignored `build-battery/research/cache-static/` retains provenance, the derivative scope and the exported12300927-byte REA evidence bundle. Vendor source is not committed.

## Established paths

1. `getPowerInfo` at original service offsets536970..537510 calls `getState`, `Array.isArray`, array `map`, and `parseInt`. It reads `power.powerStatus`, `power.chargingStatus`, and `settings.isDeviceAlive`; this function contains no hardware request. It maps charging string1 to true and computes the low-battery threshold.
2. `handleSDKEvent` accepts HEADSET_GENERAL_PURPOSE407 / DEVICE_STATUS8. It copies `af_value_0` to device-alive state, and when alive copies `af_value_2` to battery and `af_value_1` to charging for a single device; `af_value_7` selects the multi-device layout. It dispatches the cache update and broadcasts formatted cached power to the tray. These are SDK event fields, not USB report offsets.
3. The framework POWER_INFO20101 handler is a second cache input. POWER_TRAY_ICON20102 calls `checkBroadcastPower`; its battery values come from `getPowerInfo`. The tray broadcast uses SEND_POWER_EVENT10200 and target role `trayIcon`.
4. Both HTTP `checkRequestAccepable` and WebSocket `checkConnectionAccepable` in the installed main bundle obtain the connecting process from its port, check creation time twice and require `isRelatedProcess` or `isASUSTrusted`. Setting a tray role does not skip the upgrade check. An independent OpenRGB subscription is not established as supported by this code. No trust-check bypass, injection or vendor modification is proposed.
5. Profile serialization explicitly removes `powerStatus` and `chargingStatus` before writing the power object to `sp_<profile>_config.xml`. Those saved settings cannot provide the live battery cache. This conclusion concerns the inspected serialization paths, not every possible file on the computer.
6. In `getPowerInfo`, `isFiveStages` selects a `levels` value5 versus4 and a low-battery threshold20 versus25; the battery value itself is parsed without a stage-to-percent conversion. This resolves the earlier concern that this particular method necessarily returns a0..5 stage index. It does not independently validate SDK event values against the physical battery.

## Limits and decision

REA parsed the focused23840-byte derivative with7370 AST nodes and no parse failure or omitted semantic nodes. Its backward property trace is ambiguous:3 seed matches,0 traversed relations,3 unresolved receiver identities. External minified bindings/native SDK behavior remain unknown. The concrete paths above were checked against the original artifact text and AST; they are not claimed as resolved REA cross-module data flow or observed runtime behavior.

A vendor SDK device-alive event, a cached percentage, blue button light, logos switching off, or eight idle hours does not establish RF sleep state or prove a battery query cannot wake the headset. The native SDK may itself query hardware; this static JavaScript pass does not prove how events are generated. The original strict no-wake requirement therefore remains unsatisfied for automatic Delta II acquisition, and the compiled gate stays false.

A manual awake-confirmed refresh is a possible explicit scope revision. It should use the existing core-owned bounded exclusive transport once, preserve last-known data between refreshes, refuse busy ownership, and never stop ASUS components at runtime. It is not implemented or approved by this investigation. Automatic queries and passive-listener coexistence are not enabled.

FanControl PID7284 and installed OpenRGB-Custom PID50048 were observed running at the end of this static pass. No trial was launched or installed file changed. The two-device project remains incomplete; no PR was opened.

## 20 October continuation

Following the proposed manual alternative and the owner's resume, a manual awake-confirmed tray path is now implemented in the working tree. The negative passive-source findings and closed automatic gate above remain applicable. A live full-app trial has not yet validated this new UI/core path.
