# Working on OpenRGB

Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing code, including its style,
submission and AI-assistance requirements. This file does not override that
policy. The contributor must understand, manually review and take responsibility
for anything submitted.

## Before changing device support

- Read [the RGBController API](Documentation/RGBControllerAPI.md), then inspect
  the current code. Examples may have changed since the documentation was written.
- Search existing detectors and controllers by manufacturer, product family,
  USB or PCI identifiers, and protocol commands. A missing identifier or a failed
  initial test does not by itself justify a new controller.
- Verify the exact product and revision from a label or invoice alongside the
  captured identifiers. Shared PCI IDs do not establish the retail model.
- Compare transport, report sizes, register layout, initialization and mode
  handling. Extend a compatible implementation when the differences are small.
  Explain the protocol differences if a separate controller is needed.
  Check related open MRs and maintainer feedback before duplicating an update path.
- Trace the relevant path through detector, device communication and
  RGBController presentation before deciding which layer to change.
- Check other detectors using the same identifiers. A working new path should
  not also publish an empty or unsupported controller through another path.

## Reuse the project architecture

- For I2C/SMBus devices, inspect `i2c_smbus/` and the existing detector macros.
  Use the shared bus interface rather than calling a platform GPU API directly
  from a device controller. Do not assume the bus from the device's name alone.
- For USB/HID devices, compare report ID, length, interface, usage and transfer
  type with the capture. Feature, control-output and interrupt-output reports
  are not interchangeable. Check the bundled HID API on each supported platform.
- Reuse the RGBController update and locking mechanisms. Check the base class
  before adding another mutex, update thread or frame scheduler.
- Keep unrelated existing device behaviour unchanged. Follow nearby code and
  CONTRIBUTING.md rather than introducing a different general coding style.
- Follow the no-throw convention: return failure and use LogManager when needed.
  Avoid incidental ownership or structural rewrites; align consecutive assignments
  on tab stops as CONTRIBUTING.md specifies.
- Follow nearby detector lifetime handling: check a constructed device's validity
  before creating its RGBController, and release resources on rejection.
- Keep protocol mode values distinct from mode-list indices. When consolidating
  colour and mode updates, verify startup readback and all existing modes.

## Evidence and validation

- Separate observed capture bytes, inferred meanings and physical confirmation.
  Successful API delivery does not prove that the lights changed.
- Distinguish initialization, user-selected mode changes and repeated colour
  updates in captures. Do not assume a list of observed commands is one frame.
  If a field is unknown, do not invent an encoding to fill the gap.
- Match detection narrowly enough to avoid unrelated devices, particularly when
  devices share identifiers. Avoid speculative writes or broad bus probing.
- Check transfer results, including short transfers, and stop dependent commands
  after a failed transfer. Keep retries bounded and grounded in observed behaviour.
- Keep routine diagnostics at an appropriate level; do not flood logs on every
  animation frame after a persistent failure. Allow a new error to be reported
  after recovery without letting another channel hide an ongoing failure.
- Limit hardware changes to the requested function. Do not change cooling,
  voltages, clocks, firmware or unrelated lighting outputs while adding RGB support.
- Follow [Compiling.md](Documentation/Compiling.md) and the relevant
  [SMBus](Documentation/SMBusAccess.md) or [USB](Documentation/USBAccess.md) access
  instructions. Distinguish compilation, mocked checks and physical testing.
  Report only the platforms and hardware actually tested.
- Compile affected production files against the real project headers. Textual
  assertions and simplified stubs do not establish that an interface is valid.
- Verify profiles and plugins against the tested build's actual device identities,
  modes and zone sizes. Check plugin loading before diagnosing missing animation.
- A fixed colour or a restored startup profile does not establish device-side
  persistence. Use MODE_FLAG_AUTOMATIC_SAVE only when automatic saving is verified;
  state the power-cycle conditions tested independently of software restoration.

## Before submitting

- Keep each MR limited to one functional area. Inspect the complete diff against
  the intended upstream base, not just the latest commit.
  Rebase on current upstream, resolve conflicts, and repeat affected checks.
- Keep personal installers, captures, temporary diagnostics and generated files
  outside the submitted diff unless requested. Use focused local checks when
  useful; do not introduce a test framework as an incidental driver change.
- For a new device, follow the [New Device MR template](.gitlab/merge_request_templates/New%20Device.md),
  including the linked issue, protocol evidence, metadata and validation checklist.
  Leave unverified requirements unchecked.
- Explain the problem, implementation and validation in the contributor's own
  words. Do not claim compatibility from a mock or promise maintainer acceptance.
