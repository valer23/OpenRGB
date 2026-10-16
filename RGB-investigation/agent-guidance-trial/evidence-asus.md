# Task: support my motherboard's ROG logo

Add onboard-logo lighting support for ASUS ROG STRIX B850-F GAMING WIFI7 NEO.
USB 0B05:1CD2, interface 1, usage page FF0A, usage 0001. Limit this task to
the logo. No ARGB-header configuration or colours. Compare existing ASUS
protocol implementations before choosing the architecture.

## Captured reports

All output reports are 64 bytes including report ID 41. Captured Windows
transfer: control SET_REPORT, bmRequestType 21, bRequest 09, wValue 0241,
wIndex 0001, wLength 0040. hid_write uses a different transfer path here.
Unspecified bytes below are zero. Offsets are zero-based; hex values:

- Onboard initialization: [0]=41, [1]=05, [2]=01, [6]=output (00 then 01),
  [8]=01. Send each output once.
- After a 2.2 second pause, activation: [0]=41, [1]=21, [3]=01. Then wait
  one second. This packet restored response in replay; its wider meaning is unknown.
- Onboard colour: [0]=41, [1]=05, [2]=01, [6]=output (00 then 01), [16]=01,
  [18]=green, [19]=red, [20]=blue. The remaining bytes are zero.
- 30 ms spacing after each report worked in prior replay.

## Physical observations and limits

The two captured onboard outputs received the same RGB colour. The ROG logo
followed red/green/blue/white. Independent physical mapping of those outputs
has not been established, so only one logo zone is justified.
ASUS LightingService must not control lighting concurrently. A separate local
launcher paused it during testing and restored it after exit; service/startup
management is outside this driver change. Detection should not alter lighting.
ARGB headers, firmware persistence, hardware effects and Linux/macOS are not
verified. Existing Windows HID control-output API is available; inspect bundled
headers before assuming availability on other platforms.

## Deliverable

Minimal production support, keeping observed bytes distinct from inferred
semantics. Explain why reuse or a separate protocol is appropriate. Handle
short/failed reports without dependent writes or repetitive frame errors, and
allow recovery. Use the existing update/locking architecture. Do not access
hardware, network, other repositories or finished solutions. Do not commit or
publish. Read and edit only the supplied task directory. Keep diagnostics out
of the production submission diff.
