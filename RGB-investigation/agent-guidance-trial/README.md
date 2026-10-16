# Device-contribution guidance experiment

This is a local feasibility trial, not a new driver submission or proof that
generated code meets OpenRGB's acceptance policy.

The proposed upstream change is only `AGENTS.md` on the fork's
`proposal/device-contribution-guidance` branch. These experiment records belong
on `experiment/device-guidance-trial`, separately from that proposal and main.

Read `RESULTS.md` for current gates and limitations, `TRIAL.md` for the procedure,
and `ACCEPTANCE.md` for criteria frozen before the runs. `INPUT-CORRECTIONS.md`
records mistakes in the evaluator's supplied evidence. First-pass and repeat
reviews must not be conflated with successful compilation or hardware tests.

`AGENTS.trial-v1.md` is the guidance actually supplied to workers. The proposed
guide was refined afterward; no run has tested those later additions yet.
Original and corrected evidence packets and their hashes are retained.

Full baseline exports, Git directories, compiled binaries, raw private captures
and temporary build trees are intentionally excluded from the fork records.
They remain in the local workspace. The developer's Windows toolchain setup is
infrastructure rather than part of the upstream proposal.

There is no unguided control group or OS-enforced isolation. Workers were given
decoded protocol evidence; they did not independently collect vendor captures.
Human code review, platform validation and ownership of submissions still apply.
