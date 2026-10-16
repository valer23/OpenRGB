# Trial results

Status: in progress. No experimental driver has been installed or published.

## Repository verification

The known-good fork main at `6535c8ea213a55b9fe34598a1a6438b8037dd465`
matches the merged review PR tree. The previously tested production controllers,
installer source and local driver checks match the personal working branch.
The local SL-Infinity test-only detector is intentionally absent from the clean
fork review tree. It is not part of any upstream submission.

## Preparation

- Reconstruction source: upstream `4d17bac5b8f34746847e25bd42189cde8014f0cc`.
- Current contribution guidance checked at upstream
  `6a10f77c15e4be47c792b89f1a3cc4e992389684`.
- Four exported source trees have independent Git histories containing only the
  baseline and frozen guidance/task input. They have no original repository refs.
- Hashes are recorded in `input-manifest.json`.
- Fresh builder check identified the need for frozen acceptance criteria and
  separating reference behaviour from a prescribed solution. Both addressed
  before the implementation runs.
- Fork-oriented guide branch: `docs/device-contribution-guidance`.
- Clean eventual upstream proposal: `proposal/device-contribution-guidance`,
  commit `9d82c79f`, based directly on upstream/master with exactly one added
  `AGENTS.md`. The custom fork main is not its base. No upstream MR opened.
- Separate fork experiment records: `experiment/device-guidance-trial`.

## Run assignments

All implementation workers start with no conversation history. Each is told to
read AGENTS.md and TASK.md, limit reads/writes to its supplied source tree, avoid
network/hardware/other repositories/agents, leave uncommitted production changes,
and write TRIAL-RESULT.md with architecture, assumptions, actual checks and
missing validation. Compiler infrastructure is supplied by the evaluator later.
The isolation is procedural, not an OS-level sandbox.

| Task | Worker | First-pass review | Build | Hardware |
|---|---|---|---|---|
| GPU | `/root/trial_gpu` | Integration correction required; input ambiguity | Not built | Pending |
| Fans | `/root/trial_fans` | Packet assessment invalid: evaluator input error; logging gap | Not built | Pending |
| ASUS logo | `/root/trial_asus` | Source review pass | Production syntax and fake-HID checks passed | Pending |
| RAM | `/root/trial_ram` | Source review pass | Production syntax and extracted mutex/registration checks passed | Pending |

## Corrected-input repeats and feedback

- Corrected GPU input run: `/root/trial_gpu_repeat`, fresh context. It reused
  Galax v1, avoided periodic sync toggling and handled the empty NVIDIA controller.
  Source review passed but actual compilation caught protected `zones` access.
  After explicit compiler feedback it used public `GetZoneCount()`; that file
  then passed syntax compilation. Other modified production translation units
  passed. Fake-I2C execution passed for legacy colours/four modes, exact RGB/apply
  sequence, bounded sync failure, mode reselection, colour failure stopping and
  diagnostic recovery. This is a corrected result, not a first-pass build success.
- Corrected fan input run: `/root/trial_fans_repeat`, fresh context. Packet fields
  were preserved. Its lifetime log latch required explicit review feedback to
  reset per logical channel after successful colour/commit. Subsequent source
  review and syntax checks passed. Fake-HID execution passed for exact packets,
  cached setup, failed/short colour stopping, failed commit reporting, repeated
  error suppression and recovery without cross-channel resets.
- ASUS fake-HID execution compared actual generated packets with supplied
  offsets and checked failed/short transfer stopping at each setup/colour position,
  lazy initialization, reinitialization and diagnostic recovery. It did not
  execute real HID I/O or prove physical response.
- RAM checks executed extracted candidate selection/registration functions with
  stubbed Windows/PawnIO calls. Normal, abandoned, failed wait/create/selection,
  cleanup and successful interface registration were covered. This is narrower
  than full binary/hardware validation.
- Pre-hardware review found an additional fan issue: empty ports were receiving
  an unverified zero-fan setup. A second explicit feedback correction skips
  empty pairs and rejects zero before any sync/setup writes. The fake-HID check
  then confirmed zero-count calls emit nothing while prior packet/recovery
  checks still pass. Final sources are frozen in evaluated-candidate; earlier
  stages remain in first-pass, corrected-input-pass and before-final-feedback.
- Independent final judge found no remaining definite source blocker within its
  scope and recommends waiting for full-build and physical checks before a
  success-based upstream proposal.
- The untouched baseline full build failed in the pre-existing hueplusplus
  TimePattern.cpp: the Windows implementation is guarded by _MSC_VER, making
  MinGW compile calls to unavailable setenv/unsetenv. Build-Candidate.ps1 now
  generates a separate compatibility copy with a _WIN32 guard and overrides that
  source in qmake. The source tree and proposed driver diffs remain unchanged.
  The subsequent baseline link exposed an additional MinGW setup omission:
  QMAKE_TARGET.arch had not selected the x64 dependency/PawnIO blocks, and
  MSVC-default Windows system libraries were missing. The external qmake
  invocation now sets x86_64 before evaluating the project and supplies those
  libraries. No agent source was involved in either baseline failure. Baseline
  retry then linked up to the baseline's MSVC COM string conversion dependency.
  A second external source copy uses existing utf8_decode and wide BSTR
  constructors in WMI. No baseline source file was modified for these adapters.
  The baseline subsequently compiled, linked and deployed successfully; its
  clean Git status and binary hash were recorded before adding agent changes.
- The combined app with all 15 reconstructed production files now compiles,
  links and deploys successfully with the same build-only adaptations. Build
  status/hash are in candidate-built.json and logs remain local. The linked Qt
  generated-code duplicate-section warning also occurred in the baseline.
  Candidate physical testing remains pending; the known-good installed app and
  settings have not been replaced. A separate trial settings folder maps three
  copied profiles to the candidate device identities. It is private and excluded
  from fork records.

## Guidance revision

The original guidance remains in all frozen run inputs. The proposed upstream
file was subsequently refined to mention overlapping detectors, captured action
boundaries, unknown encodings, diagnostics after recovery, and compilation with
real headers. No fresh run has evaluated that revised guidance yet. Do not
attribute earlier outcomes to these later additions. There is no unguided control.

## Build environment

An evaluator-owned candidate source tree starts at the same baseline. The build
uses existing Qt 6.8.3 / MinGW 13.1 and a compatible mbedTLS archive. Qmake
overrides disable the stale precompiled-header path, remove MSVC's /utf-8 flag
for MinGW, and select that archive. These adaptations live in
Build-Candidate.ps1, outside production patches. No finished device source is
copied into the candidate.

The initial baseline build and subsequent combined candidate build must be
reported separately. A baseline/toolchain failure is not a worker code failure.

Both full-build gates have now passed after the documented local MinGW
adaptations. No Linux/macOS build or candidate hardware result is implied.
