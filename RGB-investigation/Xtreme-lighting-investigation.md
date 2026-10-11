# KFA2 direct lighting investigation

The ASUS initialised replay V2 changed only the motherboard logo. Fan hub
on ADD_GEN2_1 and GPU on ADD_GEN2_2 remained unaffected. This does not prove
a cable fault, nor establish that all required header commands are known.

Next hypotheses and observations that distinguish them:

1. Xtreme Tuner uses NVIDIA I2C for GPU lighting. Its captured I2C payloads
   should change with manually selected red/green/blue/red.
2. Its DeviceDll library uses HID control for this lighting path. Vendor
   LED calls and HID outputs should correlate with the colour changes.
3. It uses another private NVIDIA interface. Resolver events may identify
   that interface even if this first trace does not capture the payload.

Static inspection: both Xtreme Tuner.exe and DeviceDll.dll are x86. The
library exports SetLEDEffect, SetLEDBackColor, SetLEDInputColor and related
functions. The executable contains nvapi_QueryInterface and
nvapi_Direct_GetMethod names. These facts alone do not prove which API
controls this GPU.

`capture_xtreme.py` launches the installed vendor executable suspended,
attaches the tracing script, then resumes it. The user controls RGB only.
`xtreme_trace.js` observes vendor LED exports, NVIDIA interface resolution,
known NVIDIA I2C methods and vendor-originating HID output calls. It does
not invoke a GPU API, replace a function or alter arguments/results. Trace
hooks temporarily instrument the vendor process in memory; original app
files are unchanged. Detach removes the instrumentation on completion.
API coverage remains incomplete, especially private NVIDIA methods.

NVAPI I2C interface IDs/layout are from the local OpenRGB NVFC source.
Instrumentation API reference: https://frida.re/docs/javascript-api/
Analysis libraries pefile, capstone and frida were downloaded from PyPI
into this investigation's analysis-libs directory only.

The tracer was loaded successfully on a disposable Python process and its
marker API exercised, without opening the GPU application. This validates
the tracing runtime, not actual lighting hook coverage. Results follow.

## Captured direct control, 2026-10-11

`Xtreme-lighting-20261011-175205.jsonl` contains 664 I2C events, 51 NVIDIA
resolver events, five user action markers and no vendor LED/HID output
calls. The user confirmed all four physical manual colours matched.
Failed background probes at addresses 0x10/0xD8/0xBC are excluded from the
replay. All lighting writes below returned NVAPI status 0:

| Action | Register | Payload |
|---|---|---|
| MBSync OFF | 07 | 00 |
| RGB colour | 02 | Three RGB bytes: FF0000 / 00FF00 / 0000FF |
| Static setup | 05 | 00 |
| Static setup | 06 | 20 |
| Apply | 08 | 5A |
| MBSync ON | 07 | 01 |

Wire address is 0x46, corresponding to 7-bit address 0x23. Standard
NvAPI_I2CWrite (E812EB07) succeeds; the extended API is not needed here.
Captured I2C V3 structure: display_mask 1, is_ddc_port byte **2**, port_id 1,
is_port_id_set 1, speed 0, speed_khz 0. Preserve byte 2 even though the
field's name suggests a boolean. Register size 1. The static sequence is
RGB write, mode 05=00, mode 06=20, apply 08=5A, in that order.

The existing OpenRGB Galax v1 driver shares RGB registers but uses a
different static-mode value for register 06 and does not perform the
captured MBSync OFF/apply sequence. Simply adding a PCI detector is therefore
insufficient evidence for working support on this model.

`kfa2_nvapi.py` validates capture sequences offline and defaults to read-only
GPU access. Its x64 I2C V3 ABI is 64 bytes (x86 vendor ABI was 44 bytes).
It selects only PCI device 2208:10DE, subsystem 1535:10DE, requires one
match, and accesses only the captured 0x46 endpoint. Read-only preflight
succeeded and returned RGB FF0000, mode 00, mode2 20, MBSync 01.

`--replay` sends captured MBSync OFF and static red/green/blue/red sequences.
It snapshots colour/mode/sync first, then restores these in finally and
commits with captured apply 08=5A. It does not touch fan or OC registers.
`Start-KFA2-Direct-Test.cmd` runs the interactive test with Fan Control
running and Xtreme Tuner/OpenRGB closed. Direct replay physical result is
confirmed: user observed red/green/blue/red with Xtreme Tuner closed.
No OpenRGB support change has been made yet.

## Restoration failure and repair

`KFA2-direct-20261011-175903-result.txt`: all colour writes succeeded; the
final restoration of register 07 to 01 returned generic NVAPI status -1.
Colour/mode restoration and apply had already completed. A subsequent
read-only check confirmed RGB FF0000, modes 00/20, sync 00. This status
alone does not establish the cause (such as controller busy or contention).

The sync setter now waits 200 ms after preceding commands, retries only
this idempotent flag on status -1 up to three attempts, and requires
readback equality after a successful write. Restoration attempts the sync
step even if a preceding colour restoration fails and reports any errors.
The original sync state 01 was restored independently and read back as 01
on 2026-10-11. No full physical replay was required to confirm that repair.
Regression checks cover retry success, bounded failure, readback mismatch
and continuing sync restoration after a colour failure.

OpenRGB's current NVAPI bus uses the extended I2C API, is_ddc_port 0 and
different speed settings; the proven transport here uses standard I2C,
is_ddc_port 2, display_mask 1 and speed fields 0. A native implementation
must preserve or separately validate those differences, in addition to
the model-specific lighting sequence. Shared RGB registers are not enough
to justify reusing the existing controller unchanged.
