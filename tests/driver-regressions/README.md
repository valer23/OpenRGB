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
