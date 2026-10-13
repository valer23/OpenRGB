# Driver regression checks

On Windows with MinGW g++ on PATH, run `./tests/driver-regressions/Run-Tests.ps1`.
Pass `-Compiler` to select a compiler executable. The matching test is included
on each review branch; the combined branch includes both. Tests use no hardware
and create temporary executable files outside the repository.

The actual GPU driver and actual fan hardware/presentation controllers are
compiled. HID/NVAPI calls and the core RGBController base are mocked. These
checks do not replace a full application build, real ProfileManager integration,
or physical device tests. GPU transport ABI assertions target Windows x64.
The fan tests cover sparse zones and partial fan counts in Static and Rainbow.
The GPU tests cover stable distinct PCI identities and failed identity reads.

The separate NVAPI wrapper test compiles the real shared implementation and
checks structure initialization, unavailable functions, official query IDs,
argument forwarding and error propagation without loading the NVIDIA DLL.

The ASUS NEO test compiles the real Windows logo controller and mocks only
HID transport and the RGBController base. Its fixtures are exact 64-byte
reports from Aura startup frames 369, 371, 407 and the first two onboard
colour reports of each red/green/blue cycle in Aura-payloads.tsv. It checks
GRB bytes, lazy setup order, no writes during construction, short/failed
transfers at every setup/colour stage, retry setup, and callback bounds.
The Windows hotplug HIDAPI include supplies the control output-report API.
No tests access lighting devices.

The separate `pawnio_port_test.py` takes the production PawnIO source path,
MinGW compiler path and temporary output directory as its three arguments.
It compiles the actual PIIX4 port selection and registration blocks against
mock mutex/PawnIO APIs. It checks mutex ownership, abandoned/failed waits,
selection failures, handle cleanup and registration only after successful
selection. No PawnIO module is loaded and no hardware is accessed.

The KFA2 check now compiles the platform-independent controller with the real i2c_smbus interface and a fake bus. It covers exact captured commits, read-only probing, truncated reads, sync changes, bounded failures, constructor shutdown and callback indices. These diagnostics remain in the personal fork and are deliberately excluded from upstream MR diffs.
