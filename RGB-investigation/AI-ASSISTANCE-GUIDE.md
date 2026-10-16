# Assisted device work in this fork

This is a working guide for this personal fork. OpenRGB's CONTRIBUTING.md remains
the authority for submissions, style and assistance policy. The proposed upstream
change is a short AGENTS.md pointing to existing documentation, not this guide,
the experiment records, or a new framework. The contributor remains responsible
for understanding, reviewing and testing the code.

## Start with evidence and the existing implementation

Record the exact model, SKU and revision from the device label or invoice, then
record USB/PCI IDs separately. Our tested card is a KFA2 RTX 3080 Ti SG 1-Click OC,
SKU 38IOM5MD99DK. Its earlier EX Gamer label was wrong despite working detection.
Shared PCI IDs and a vendor utility's product label are insufficient on their own.

Search manufacturer aliases, nearby models, detector IDs and protocol registers.
Compare the transport, register/report layout, initialization, effects and colour
commit. Check related open MRs and recent maintainer comments. A first attempt
failing on another supported model is not proof that a new controller is needed.
Small differences can remain conditional in an existing controller. A genuinely
different protocol or interface can justify a separate controller; document that
comparison rather than forcing either architecture.

For GPUs, start with the shared I2C/SMBus implementations when the evidence shows
I2C traffic. Vendor NVAPI calls can be capture evidence without belonging in the
controller. HID controllers should use the existing HID transport with the correct
interface, usage, report type, ID and length. Do not infer the bus from the product
category alone.

## Keep the change in OpenRGB's style

Adam's review of GPU MR !3724 on 16 October 2026 reinforced these requirements:

- Use return values for failures and essential LogManager diagnostics. Do not add
  exceptions or catches as a new controller error-handling convention.
- Preserve nearby structure. Do not introduce smart-pointer wrappers or unrelated
  refactors simply to impose a different general C++ style.
- Align consecutive assignments on four-space tab stops.
- For a device that can be invalid after construction, check validity before
  creating the RGBController; delete the rejected controller and any separately
  owned resources. Verify ownership in the destructors rather than guessing.
- Consolidate duplicated colour/mode paths when they send the same commands.
  Check all existing modes, startup readback and recovery after that change.
  Protocol mode numbers are not necessarily zero-based UI list positions.
- Reapplying a mode after RGB writes is not alone evidence for another controller.
  Compare the related Galax work in !3712 and the actual sync/commit differences.

Retry only where justified, with a bounded limit. A successful write followed by
incorrect readback is not a verified transition. Stop dependent commands when
delivery fails. Suppress persistent per-frame errors until recovery, while allowing
a later failure to be reported. Do not remove warnings merely because lights move.

## Name modes according to verified behaviour

Distinguish host-driven colour updates, firmware effects, motherboard sync and
persistent saving. A fixed colour does not demonstrate saving. Our renamed SG
Direct mode has no MODE_FLAG_AUTOMATIC_SAVE because persistence remains unverified.

Rebooting with OpenRGB loading ValWhite only proves startup restoration. A saving
claim requires a separate test with software restoration excluded and the exact
power-cycle conditions recorded. Do not change startup, power or hardware settings
to run such a test without the user's authorization. Motherboard-sync selection
also does not prove physical colour following from an external header.

## Validate the whole path

1. Inspect the complete MR diff against current upstream, following the repository
   template. Keep local capture tools, installers and test scaffolding outside the
   upstream driver MR. Focused private tests are useful; they are not prohibited.
2. Compile production files with real headers, and build the full app. A fake bus
   can verify ordering and failure handling; a stub base class cannot establish
   API compatibility, and mocks cannot prove another operating system works.
3. Check device enumeration, duplicate/empty controllers and ownership cleanup.
   Use the current build's identities, modes and configured zones for profiles.
   Verify plugin dependencies and supported streaming modes before attributing
   missing animation to a driver. Our inherited fan Custom mode needed a narrow
   compatibility adapter in the personal Effects Plugin.
4. Test manual colours, saved profiles and animation on the physical devices.
   Inspect the logs as well as the lights. Keep Fan Control running and avoid
   cooling, voltage, clock and firmware changes. Isolate trials from the installed
   app and back up profiles before migrations.
5. Report build, local tests and physical results separately. Do not claim Linux,
   macOS, other board revisions or persistent saving without evidence. Rebase and
   rerun relevant checks after review corrections, then remove Draft when ready.

Keep MR descriptions short and in the contributor's own voice: the original
problem, the final implementation, actual validation and remaining limits.
Follow the project's authorship policy. Do not hide the use of assistance when
asked or treat omitted attribution as permission to bypass review requirements.

## What the experiment establishes

The original isolated reconstruction required corrected evidence, reviewer and
compiler feedback, deployment fixes, profile adaptation and a plugin adapter.
Physical colour and animation checks passed, but that reconstructed GPU still
logged transfer warnings. Its reliability result remains unresolved.

The later maintainer-review revision 4424545a was a separate update to the real
driver. It passed local checks, independent review, a full Windows build and the
user's hardware check without GPU lighting-transfer warnings in that session.
That result does not retroactively clear the reconstructed trial's warnings.

The guidance has since changed. No new fresh-session run has evaluated this
revision, and there was no unguided comparison or OS-enforced isolation in the
original experiment. Do not claim that AGENTS.md alone produced the result,
improved a measured success rate, or satisfies the maintainer's acceptance policy.

References: [GPU review](https://gitlab.com/CalcProgrammer1/OpenRGB/-/merge_requests/3724),
[related Galax MR](https://gitlab.com/CalcProgrammer1/OpenRGB/-/merge_requests/3712),
and [recorded trial](https://github.com/valer23/OpenRGB/blob/experiment/device-guidance-trial/RGB-investigation/agent-guidance-trial/RESULTS.md).
