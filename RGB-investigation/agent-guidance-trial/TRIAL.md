# OpenRGB contributor-guidance trial

## Goal and proposal

Check whether a small, tool-independent AGENTS.md helps a fresh assistant add
device support while respecting OpenRGB's existing architecture and contribution
rules. The proposed upstream diff initially contains AGENTS.md only. Local trial
inputs, results, build adaptations and experimental drivers are separate.

The policy in CONTRIBUTING.md remains authoritative. This experiment does not
claim that guidance makes generated submissions automatically acceptable.

## Fixed inputs

- Reconstruction baseline: upstream commit
  `4d17bac5b8f34746847e25bd42189cde8014f0cc`, before the four submitted changes.
- Guidance cross-check: current upstream `6a10f77c15e4be47c792b89f1a3cc4e992389684`.
- Current working drivers are a private reference for the evaluator only.
- Four independent tasks: KFA2 GPU, Lian Li SL-Infinity, ASUS logo, PawnIO RAM.
- Give each worker a clean exported source tree initialized as its own repository,
  the proposed AGENTS.md, and a neutral evidence packet. Do not give it this chat,
  finished source, our patches, MR links or implementation test fixtures.
- Evidence packets contain hardware identity, observed vendor commands, known
  transport results and desired behaviour. Exclude conclusions about which class
  to extend. Document which packet statements are capture observations versus
  prior hardware observations.

## Isolation limits

Use fresh-context workers. They must read and write only their supplied task tree
and evidence packet. No network or hardware access; no inspection of the original
repository or sibling tasks. This is an instruction boundary, not an OS-enforced
sandbox. Record any breach or accidental answer exposure and mark that run invalid.

Raw private captures are not committed to the proposal. A supplied decoded packet
specification tests implementation from evidence, not independent reverse
engineering. Do not claim that this trial demonstrates capture acquisition.

## Execution

1. Review the guidance and evidence before giving them to workers. Freeze their
   hashes before the first run. Record baseline and task prompts.
2. Ask workers to inspect architecture, implement a minimal scoped change and
   report unverified behaviour. Each stops before a hardware write or publication.
3. Keep completed runs unchanged. Record initial output and any later feedback
   separately. A run corrected after evaluator feedback is not a first-pass success.
4. Evaluate architecture, diff scope, existing-device behaviour, transfer checks,
   logging, style, build integration and honesty about validation. Record findings,
   not just a pass/fail label.
   Freeze `ACCEPTANCE.md` before execution. Private reference drivers are a
   behavioural cross-check, not a required class design or exact diff. Any
   assessment requiring reference-only information must be recorded as an input
   gap, not silently counted against a worker.
5. Compile candidate source with the available Windows toolchain. Reuse a build
   environment only as infrastructure, never copy driver source from the working
   installation. Keep build-only adaptations outside candidate submission diffs.
6. If the code and builds pass review, assemble a separate candidate app. Keep the
   known-good installed app and settings backed up. Obtain a tray exit before
   replacing or running a competing RGB instance; keep Fan Control running.
   Physical results must come from the user. Restore the working build if needed.
7. Keep results in the fork's experiment material. Propose the guidance upstream
   only after human review and a credible outcome. Do not publish trial driver
   MRs or modify the existing device MRs as part of the experiment.

## Success and limitations

A successful run chooses appropriate existing abstractions, produces a small
reviewable diff, builds, preserves existing behaviour and receives physical
confirmation for its claimed functions. GPU/fan/RAM tasks should not create
duplicate device implementations without protocol evidence. ASUS may justify a
separate protocol implementation; evaluate the reasoning rather than prescribing
the answer. No Linux/macOS hardware claims without testing.

Four guided reconstructions are a feasibility study, not proof of improvement
over unguided assistants. A later comparison must use the same evidence and
baseline with separate fresh unguided workers. Do not claim that AGENTS.md alone
caused success, that any assistant will succeed, or that the maintainer will
accept the resulting code.

Start without tool-specific skills or a new automation framework. Add another
document or skill only if a recorded failure identifies a concrete gap.
