# ASUS Aura NEO shutdown-effect trial

This experimental Windows extension targets the onboard logo of the ROG STRIX
B850-F GAMING WIFI7 NEO, USB 0B05:1CD2, interface 1, usage FF0A/1. It extends
the existing NEO controller; the device identity, one-logo zone and running
Static mode are unchanged.

In **Edit Device**, choose a **Shutdown effect (experimental)** and a separate
six-digit **Shutdown colour (RRGGBB)**, with a **Choose colour...** button reusing
the main-page colour wheel. The picker and hex editor stage the same value;
Cancel leaves the value unchanged. Defaults are **Leave unchanged** and
red. Accepting these settings stores them in Configuration.json. It does not
write standby settings, including when configuration is reloaded at startup.

**Shutdown writer** explicitly chooses **Direct device** (the default) or
**ASUS service**. Older configurations retain Direct device. There is no
automatic fallback between writers. Both apply only on Save To Device.
The ASUS service writer requires LightingService already running and updates
its saved Non-S0 preference through the same COM SetProfile route used by
ASUS's motherboard SDK. It opens no HID reader and starts/stops no service.
The saved profile is checked afterward, including preservation of the live
S0 profile. A successful service call alone does not prove physical shutdown
behaviour. Direct device remains useful without ASUS installed, but ASUS's
startup restoration may overwrite a direct device save.

**Save To Device** explicitly applies the staged shutdown
settings. It does not copy the running mode or colour into standby. The six
captured effects are Static, Breathing, Strobing, Color cycle, Rainbow and
Starry Night, alongside **Off**. In Direct device mode, Off sends the captured standby-disable command
followed by Static black on both units; the colour field is ignored. Enabled
effects send standby-enable first, so switching back from Off is explicit.
The enable/disable command is a motherboard standby switch without an output
selector; it may also affect motherboard-connected ARGB during shutdown. This
must be checked physically before claiming logo-only enable/disable control.
One colour is used for both onboard units. Partition is not a
seventh firmware effect: observed ASUS traffic uses shared effect group 0,
with separate colours for units 0 and 1. Independent per-output effects,
speed controls and per-header ARGB colour/effect controls are outside this trial.

The direct save path uses the existing HID handle and core SaveMode locks. It refuses
known ASUS motherboard lighting processes (LightingService.exe and
AsusMBManager.exe), or an unavailable process inventory, without stopping
anything. This is a conservative process check, not an exclusive device lock
or a guarantee against an unknown third-party controller. Keep one motherboard
lighting owner. No additional HID reader or background polling is started.

Before writes, it validates standby capabilities, group 0's exact onboard-only
mask and both onboard unit types. It preserves the existing speed/direction
bytes, uses captured 64-byte control-output reports, and checks effect/colour
readback afterward. Standby-enable delivery is checked for transfer length;
the enable state itself has no validated readback in this implementation.
Malformed/short transfers stop dependent commands. A failed
later write can leave a partial change; no automatic rollback is attempted.
Results and refusals are reported in the OpenRGB log.

Normal builds expose explicit saving. Settings and profile loads still send no
standby commands. Direct-device persistence without ASUS startup restoration
has not been independently tested across multiple boots.

## Evidence and validation

Owner-labelled USB captures on 23 October 2026 confirm effect IDs 01,02,03,04,
05,0D; shared group 0/state 1 effect writes (41 23); colour units 0/1/state 1
GRB writes (41 28); and 41 25/A5 effect readback. Earlier startup recordings
provide 41 09/89 standby capabilities and 41 29/A9 unit descriptors. No older
EC35/EC36 Aura packets are used. Sanitized Static-blue packets are retained in
the existing driver regression fixtures; raw captures and decompilation stay
outside the source patch.

An additional owner-labelled Off/On capture confirms 41 21 01 00 and
41 21 01 01 respectively, with the Off path selecting Static black and On
restoring the effect/colour. State 1 selects standby; these are distinct from
the existing live activation 41 21 00 01. No ID00-as-Off assumption is used.

Validation is reported separately for production compilation, automated
protocol/lifecycle tests, actual application/UI integration and physical
hardware behaviour. Compilation and mocked readback do not prove power-off
persistence. On 23 October 2026, the owner applied Off in trial revision
0791f856 and reported that the logo remained dark for 20 minutes while shut
down. After boot, LightingService was Running/Automatic, and Fan Control and
the usual installed OpenRGB Custom were running. On a second shutdown without
reapplying, the owner reported that the logo returned to Rainbow. Persistence
across the intervening boot therefore failed; the source of the reset is not
fully isolated. ASUS's startup log shows enabled standby lighting and the
saved Non-S0 Rainbow profile being restored. A one-time Off save through its
service subsequently changed both saved standby modes to 101 (Off), with
the live S0 profile unchanged. The owner then saved Off through the application's
ASUS service writer in revision 7759297e and reported that the logo stayed Off
during two full shutdowns, with a normal Windows/ASUS boot between them and no
reapply. Off persistence through ASUS startup is therefore physically verified
for this board and installation. Other
effects, unrelated standby headers and restoring Rainbow still need physical
verification. No automatic shutdown or reboot is part of the implementation.
