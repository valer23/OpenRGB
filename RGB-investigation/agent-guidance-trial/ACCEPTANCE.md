# Frozen evaluator checks

Workers receive their source, AGENTS.md and task input only. This file is not a
worker input. Equivalent designs and error representations are acceptable.

## All tasks

- Correct changes grounded in the supplied protocol/problem and baseline source.
- Reuse existing transports/controllers where appropriate; explain meaningful
  incompatibilities. Do not score by resemblance to the private working patch.
- Complete relevant transfer checks, short-transfer handling, bounded retries
  where used, resource ownership and recovery without log flooding.
- No write during detection unless specifically justified by existing required
  behaviour; no unrelated device configuration or cooling changes.
- Follow CONTRIBUTING.md, retain existing-device behaviour, avoid new frameworks
  and generated collateral in the proposed production diff.
- Compiles against actual baseline interfaces. No fabricated test, hardware or
  platform results. Local compile is distinct from a full build and hardware test.
- No original solution/network/hardware access or reads outside supplied tree.
  Record a violation as invalid, rather than quietly repairing it.

## GPU

- Exact identity narrowed by captured signature; shared bus transport.
- RGB and static/apply order match supplied evidence; sync control and optional
  unexpected/failed initial state do not make identification depend on that state.
- Existing Galax RGB and mode sequences preserved if extended.
- No empty standard NVIDIA illumination duplicate for shared FE IDs.
- Dependent writes stop on failure; optional initial state does not cause a write.
- Windows native/backend and full profile checks are later gates, not worker claims.

## Fans

- Exact observed report types, lengths, RBG data and required commits.
- Port counts shared by centre/side channels, setup cache refreshed when changed.
- Sparse/empty zones do not block later channels; partial groups rounded correctly.
- Both static and existing hardware effect paths retain valid reports.
- Failed colour skips commit; failed commit is observable without flooding.
- Successful spacing and cooling commands unchanged; platform branches compile.

## ASUS

- Narrow HID selection and captured report bytes, lengths, GRB order and delays.
- Logo-only zone; both captured onboard outputs share colour; no header writes.
- Lazy initialization, recovery after failed setup/colour, complete transfer checks.
- Architecture choice justified by protocol comparison; respect platform API limits.
- No concurrent service-management changes hidden in the device MR.

## RAM

- Port selection serialized with the shared SMBus lock; acquired abandoned mutex
  released correctly; failed wait never releases an unowned lock.
- Only successful port selection registers a bus; failure closes the PawnIO handle.
- Preserve success paths and other backend detection; no UI deduplication shortcut.

## Reporting

For each run retain untouched first output and diff, evidence/guidance hashes,
worker ID, actual checks, findings, feedback and any corrected revision separately.
Labels: initial review pass / correction required / invalid / build pending /
hardware pending / physically verified. Avoid collapsing these into "works".
No assertion that guidance improved outcomes without a matched unguided comparison.
