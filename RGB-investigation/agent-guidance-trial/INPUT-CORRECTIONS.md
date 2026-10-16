# Input corrections after first-pass freeze

## Fan packet error

The evaluator's initial evidence-fans.md incorrectly described commit byte 3 as
packed direction/speed and moved brightness to byte 4. This contradicted the
vendor device method SetEffectSetting(port,mode,speed,direction,brightness), the
baseline controller and the known-good replay. Correct layout is E0,
10+channel, effect, speed, direction, brightness, 00. Static defaults are all
zero after effect 01. The first fan worker followed this faulty input. Its
changed commit layout must not be counted as an agent failure or evidence that
AGENTS.md helped/hurt. Retain the run but mark packet correctness invalid.

## GPU action-boundary ambiguity

The initial list described both sync-off and sync-on vendor actions without
clearly saying that sync-on was a separate later UI action, rather than part of
each static colour transaction. The first worker toggled it on after each
frame. Clarify the action boundary in the next input. Do not score this as a
fair failed protocol reconstruction from unambiguous evidence.

The empty NVIDIA controller finding is separate: the task supplied the shared
identity and zero-zone query result, but the worker explicitly deferred the
integration concern. Record it as a first-pass integration gap.

## Repeat procedure

Freeze the first output, reset only the disposable trial source to its own
baseline, replace the task input with the corrected evidence and start a new
fresh-context worker. Record the new task hash and worker identity separately.
The guidance stays unchanged for this repeat, so input clarification is the
changed variable. This is a repeat on corrected evidence, not a controlled
guided/unguided comparison and not a first-pass success on the original input.
