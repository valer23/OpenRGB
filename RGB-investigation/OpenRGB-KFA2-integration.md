# Local OpenRGB KFA2 integration

Source checkout: repository root, base commit `4d17bac`.
This is a local experimental build; nothing has been submitted upstream.

## Behaviour

The Windows detector matches PCI device 2208:10DE and subsystem 1535:10DE,
then requires the observed lighting controller identifier 11 00 at I2C
registers 00-01. Detection and construction read state without writing.
The actual GPU identifier was read successfully during this integration.

The native driver uses the standard NVIDIA I2C interfaces and preserves
the transport fields validated by `kfa2_nvapi.py`: wire address 46, port 1,
display_mask 1, is_ddc_port 2, speed fields 0. It exposes a single GPU zone
and two modes: Static and Motherboard Sync. Static updates use the captured
RGB/mode/apply sequence; switching modes sets and verifies sync register 07
with bounded retries on generic status -1. Untested animated effects and
hardware persistence are not exposed. Successful MBSync flag selection
does not establish a working external ARGB signal path.

The generic NVIDIA Illumination detector avoids claiming this GPU as a
Founders Edition card only when both PCI IDs and this controller ID match.
Existing Galax drivers and their transport are unchanged. Shared NVFC
wrappers add the standard I2C interfaces alongside existing extended ones.

## Build and validation

`CONFIG+=kfa2_capture_test` builds `OpenRGB-KFA2-test.exe` with only this
GPU detector registered. A Nanoleaf helper implementation is retained to
satisfy the existing GUI, without including its detector. The test build
uses its own settings under `OpenRGB-KFA2-settings` and no automatic profile
loading. The installed OpenRGB executable, settings, profiles and startup
tasks are unchanged.

Portable Qt 6.8.3 and matching MinGW 13.1 are in `toolchain`. An initial
Qt 5.15.2/MinGW 8.1 attempt failed on current filesystem code; it is not the
active toolchain. Build script: `Build-OpenRGB-KFA2.ps1`. Compiler portability
fixes scope /utf-8 to MSVC, use QT_ARCH for MinGW, link required Windows
libraries, use Windows _mkgmtime and avoid narrow _bstr_t conversion helpers.

Hardware-free native checks compile the actual new driver and real NVAPI
structure header with a presentation-layer stub. They validate read-only
construction, controller ID rejection, exact transport fields, captured
green sequence, motherboard-sync switching, retry success and bounded
failure without applying colours after a failed sync switch.

The real application build compiles against OpenRGB's actual base class
and Qt GUI. The GPU-only executable successfully linked and its Qt runtime
was deployed on 2026-10-11. Bundled MSVC mbedTLS libraries were incompatible
with MinGW; `build_mbedtls.py` builds all 93 library C sources from the
official Mbed-TLS/mbedtls v3.2.1 source archive into one native archive.
The qmake test configuration accepts its path as KFA2_MBEDTLS_ARCHIVE.
Source download: https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.2.1.zip

Read-only application detection exited successfully and registered
`KFA2 GeForce RTX 3080 Ti EX Gamer`; see test settings logs at
`logs/OpenRGB_20261011_182935.log`. The Windows startup attaches CONOUT,
so redirected CLI output files are empty; detection is verified by the
application log, not inferred from an empty output file. A subsequent
independent read confirmed RGB FF0000, mode 00, mode2 20 and sync 01
unchanged. The normal-build NVIDIA detector exclusion also passed a
syntax check against the real headers. Native transport regression checks
passed with MinGW 13.1.

Original hardware evidence: the user confirmed direct Python replay
red/green/blue/red with Xtreme Tuner closed. Its restoration of MBSync
failed once; MBSync was repaired and read back as 01. Native GUI validation
was subsequently confirmed by the user on 2026-10-11.

## Physical OpenRGB GUI verification

On 2026-10-11 the user reported that the native OpenRGB test worked and
provided a screenshot showing the correctly named KFA2 GPU, GPU zone, Static
mode and colour 0000FF. This confirms working native GUI colour control;
it does not establish motherboard-sync operation, hardware persistence,
other GPU models, or fan lighting support.
