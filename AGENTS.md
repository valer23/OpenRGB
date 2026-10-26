# Working on OpenRGB

Read [CONTRIBUTING.md](CONTRIBUTING.md) before making changes, including its
style, submission and AI guidelines. This guide does not grant an exception to
that policy. The human contributor must understand, manually review and take
responsibility for submitted code. Do not promise acceptance or conceal how
validation was performed.

## Find the existing implementation first

- Inspect the checkout, branch and local changes before editing. Identify the
  intended upstream base and read relevant issues, MRs and maintainer feedback.
- Read [RGBControllerAPI.md](Documentation/RGBControllerAPI.md), then trace the
  current detector, device controller and RGBController implementation. Current
  code may differ from older documentation examples.
- Search by manufacturer, product family, USB/PCI identifiers and protocol
  commands. Verify the exact retail model/revision separately from shared IDs.
  A missing ID or an unsuccessful first test does not justify a new controller.
- Compare transport, report/register layout, initialization and modes with
  related devices. Extend a compatible controller; explain concrete protocol
  differences when proposing another one. Check overlapping detectors so one
  device is not also published as an empty or duplicate controller. Follow nearby
  ownership conventions: validate constructed devices before publishing their
  RGBController and release resources when rejecting them.

| Area | Start here |
| --- | --- |
| Shared device API, updates, configuration and lifetime | [RGBController/](RGBController/) |
| Existing detectors and device implementations | [Controllers/](Controllers/) |
| I2C/SMBus transport and access | [i2c_smbus/](i2c_smbus/), [SMBusAccess.md](Documentation/SMBusAccess.md) |
| USB/HID access and dependencies | [USBAccess.md](Documentation/USBAccess.md), [dependencies/](dependencies/) |
| Device settings UI consumer | [OpenRGBDynamicSettingsWidget.cpp](qt/OpenRGBDynamicSettingsWidget/OpenRGBDynamicSettingsWidget.cpp) |
| Builds | [Compiling.md](Documentation/Compiling.md) |

## Use the shared mechanisms

- For I2C devices, use the shared bus interface and appropriate detector rather
  than embedding vendor I2C calls. Reuse existing vendor illumination APIs when
  the evidence matches them. Determine transport from the protocol, not the
  product category.
- For missing or duplicate RAM, trace backend availability, bus/port selection,
  registration and SPD/detector routing before changing the RAM controller.
  Distinguish transport synchronization from RGBController locks.
- For HID, match report ID, size, interface, usage and transfer type. Feature,
  control-output and interrupt-output reports are not interchangeable. Check
  dependency API/version support across the intended platforms; do not substitute
  an unverified transport or submit a personal toolchain workaround.
- Inspect the core update queue, locks and shutdown order before adding a mutex,
  scheduler or worker thread. Audit every caller before removing a lock; do not
  reacquire a nonrecursive core lock from a path that already holds it.
- Put device preferences in RGBController's device-specific configuration hooks,
  exposed through Edit Device and stored in Configuration.json. OpenRGB.json is
  for application settings. Check the actual UI consumer: its checkbox type is
  `bool`, not `boolean`. Verify rendering, callbacks, defaults/reset, live changes
  and saved reloads. A settings schema alone does not enable Edit Device: check
  the controller's configurability flags too. Keep personal migration tools
  outside controller-only MRs.
- Identify protocol modes by `modes[active_mode].value`, not presentation order.
  Keep internal streaming sentinels out of hardware commands. Check startup
  readback, reordered modes and existing modes when consolidating update paths.

## Establish the protocol and preserve device behaviour

- Separate captured bytes, inferred meanings and physical observations. Capture
  controlled changes and distinguish setup, mode selection and repeated frames,
  including setup delays, frame cadence and when cached setup becomes invalid.
  Do not invent an unknown field or assume successful USB delivery changed LEDs.
- Match detection narrowly and validate transfer lengths, routes and replies.
  Stop dependent commands on failure. Bound retries and avoid waking an idle or
  disconnected wireless device with repeated traffic. Verify sleep/wake separately
  from lighting ownership; a rendering change may reset an inactivity counter.
- Limit hardware actions to the requested function. For lighting-only tasks, do
  not change cooling, clocks, voltage, firmware or unrelated device outputs.
  Do not probe unrelated buses.
- Preserve existing device behaviour and profile identities where practical.
  A restored software profile does not prove automatic device-side saving. Set
  save capabilities only when the relevant persistence behaviour is verified.

## Preserve saved profiles across upgrades

- Treat persisted identity and capability metadata as a compatibility contract.
  Trace the actual profile loader, save editor and plugin matchers; they may use
  different fields. Check name, vendor, description, serial, version, location,
  modes and zone metadata rather than assuming the display name is sufficient.
- When a change is necessary, preserve compatibility or update the project-owned
  migration path. Cover already-renamed and partially migrated entries, including
  nested plugin assignments. Keep migrations narrow and idempotent, verify
  backups, preserve colours/effects/preferences, and reject unsupported mappings
  before writing files.
- Check packaged layout/migration data against the controller in the built
  executable. Unchanged profile-file hashes prove preservation, not compatibility.
- Test copies of existing profiles as well as fresh defaults. Check load, switching
  and Save Profile in the actual app. An Add + Keep pair can mean stale identity
  or mode/zone capabilities rather than duplicate detection. Do not hide it by
  merging display names: two identically named physical devices may be valid.
## Keep the implementation and diff small

- Follow CONTRIBUTING.md and nearby code, including its C-style conventions and
  boxed comments. Preserve attribution and avoid unrelated header, comment and
  formatting churn; update comments when their described behaviour changes.
- Justify abstractions, wrappers, locks and scheduling by distinct responsibilities.
  Reuse existing mechanisms and give each state field a distinct invariant.
  Simplify redundant state without losing recovery or confusing attempted and
  acknowledged transitions.
- Bound failure logs and avoid duplicating transition state solely for logging.
  Review the complete MR diff; line count alone does not establish quality.

## Verify and submit honestly

- Build affected production code against real project headers. Focused mocks,
  packet comparisons and lifecycle checks support specific claims; they do not
  prove UI integration, physical response or another OS's compatibility.
- Exercise the changed path and relevant failures, reconnects, mode changes and
  shutdown. For software effects, check plugin loading and the detected device
  identities, modes and zone sizes before blaming the driver.
- Report four validation categories separately: compilation with real headers and
  dependencies; automated protocol/lifecycle checks; actual application/UI
  integration; and physical hardware verification. Record the revision, platform
  and untested items in each category. Explain which uncertainty a repeated long
  hardware test resolves.
- Use a dedicated source branch and preserve upstream's linear history. Rebase
  on the intended current base and repeat affected checks after resolving conflicts.
- Keep one functional area per MR. Follow the [New Device template](.gitlab/merge_request_templates/New%20Device.md)
  when applicable. Include the required device metadata for permissions and
  supported-device documentation, and protocol evidence in the linked issue or
  developer wiki. Leave unverified items unchecked. Keep personal installers,
  capture files, generated artifacts and incidental test frameworks out of the
  code patch unless requested.
- Review architecture, necessity and project style separately from functionality.
  Apply feedback to each MR and verify its actual remote head. A working local
  build, another corrected MR or an agent's approval is not sufficient evidence.
  Describe the problem, change and validation in the contributor's own words.
