# Task: support my KFA2 GPU

Add lighting support for KFA2 GeForce RTX 3080 Ti EX Gamer to the supplied
OpenRGB baseline. PCI vendor/device 10DE:2208, subsystem 10DE:1535. Implement
the verified static-colour subset and software colour updates. Do not invent
hardware effects. Investigate existing controllers before deciding placement.

## Observed vendor traffic

Xtreme Tuner's NVIDIA I2C traffic correlated with physical red/green/blue/red.
7-bit address 0x23 (wire address 0x46). Register addresses are one byte.
The captured write sequence is:

- Disable motherboard lighting sync: register 07, byte 00.
- Register 02: three bytes R,G,B.
- Register 05: byte 00.
- Register 06: byte 20.
- Register 08: byte 5A (apply).
- Enable motherboard sync: register 07, byte 01.

Read-only checks returned signature 11 00 from register 00, three RGB bytes
from register 02, and sync from register 07. The card shares Founders Edition
PCI IDs, so identity alone is insufficient. Its standard NVIDIA illumination
query returned error -9 with zero zones.

## Prior hardware observations

Replays of these commands changed the physical GPU. An independent diagnostic
using the baseline's unmodified shared Windows I2C backend also read the device
and changed colours. Vendor-specific NVAPI ABI details are therefore not needed
as a prerequisite for that backend on this setup.

Sync writes have intermittently returned generic error -1. A 200 ms pause,
bounded retries and readback were used successfully. Colour sequence writes
used 25 ms spacing. Initial sync reads sometimes returned AC or failed despite
valid signature and RGB reads. These observations do not prove the cause.
Do not let an optional sync read hide a positively identified card or write
lighting settings during detection. Hardware sync colour following, onboard
effects, saving to the card, other GPUs and Linux are unverified.

## Deliverable

Implement the smallest appropriate production change and explain architecture,
scope, error behaviour and what still needs hardware verification. Keep local
diagnostics outside the submitted production diff. Do not access hardware,
network, other repositories or original completed solutions. Do not commit or
publish changes. You may inspect and edit only the supplied task directory.
