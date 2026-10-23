# ASUS Aura NEO shutdown-effect trial

This experimental Windows extension targets the onboard logo of the ROG STRIX
B850-F GAMING WIFI7 NEO, USB 0B05:1CD2, interface 1, usage FF0A/1. It extends
the existing NEO controller; the device identity, one-logo zone and running
Static mode are unchanged.

In **Edit Device**, choose a **Shutdown effect (experimental)** and a separate
six-digit **Shutdown colour (RRGGBB)**. Defaults are **Leave unchanged** and
red. Accepting these settings stores them in Configuration.json. It does not
write standby settings, including when configuration is reloaded at startup.

In the trial build, **Save To Device** explicitly applies the staged shutdown
settings. It does not copy the running mode or colour into standby. The six
captured choices are Static, Breathing, Strobing, Color cycle, Rainbow and
Starry Night. One colour is used for both onboard units. Partition is not a
seventh firmware effect: observed ASUS traffic uses shared effect group 0,
with separate colours for units 0 and 1. Independent per-output effects,
Off, speed controls and ARGB headers are outside this trial.

The save path uses the existing HID handle and core SaveMode locks. It refuses
known ASUS motherboard lighting processes (LightingService.exe and
AsusMBManager.exe), or an unavailable process inventory, without stopping
anything. This is a conservative process check, not an exclusive device lock
or a guarantee against an unknown third-party controller. Keep one motherboard
lighting owner. No additional HID reader or background polling is started.

Before writes, it validates standby capabilities, group 0's exact onboard-only
mask and both onboard unit types. It preserves the existing speed/direction
bytes, uses captured 64-byte control-output reports, and checks effect/colour
readback afterward. Malformed/short transfers stop dependent commands. A failed
later write can leave a partial change; no automatic rollback is attempted.
Results and refusals are reported in the OpenRGB log.

`ASUS_NEO_SHUTDOWN_TRIAL` must be defined to enable both the Save UI flag and
the write entry point, including SDK calls. Normal builds cannot apply these
settings. Keep the gate until physical power-off persistence is verified.

## Evidence and validation

Owner-labelled USB captures on 23 October 2026 confirm effect IDs 01,02,03,04,
05,0D; shared group 0/state 1 effect writes (41 23); colour units 0/1/state 1
GRB writes (41 28); and 41 25/A5 effect readback. Earlier startup recordings
provide 41 09/89 standby capabilities and 41 29/A9 unit descriptors. No older
EC35/EC36 Aura packets are used. Sanitized Static-blue packets are retained in
the existing driver regression fixtures; raw captures and decompilation stay
outside the source patch.

Validation is reported separately for production compilation, automated
protocol/lifecycle tests, actual application/UI integration and physical
hardware behaviour. Compilation and mocked readback do not prove power-off
persistence. The first physical test must separately establish that the selected
effect survives shutdown, normal lighting is unchanged, and restoring Rainbow
works. No automatic shutdown or reboot is part of the implementation.
