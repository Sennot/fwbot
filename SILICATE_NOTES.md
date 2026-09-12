# Silicate study and analysis decisions

Reviewed on 2026-09-12 from the repository provided by the user:
https://git.silicate.dev/silicate/silicate/

Local checkout revision: `f001e01e10d6f996cae682d2576256a192ce9139`.

Files studied: `src/trajectory/trajectory.cpp`, `src/trajectory/trajectory.hpp`,
`src/physics/gjbasegamelayer.cpp`, `src/bot/updater.cpp`,
`src/hooks/CCScheduler.cpp`, and checkpoint interactions.

## Findings from the code

Silicate's predictor creates alternate player simulations from copied player
attributes and saved checkpoint data. Its simulation advances clocks/progress,
updates the player, performs collision checks and handles interactions through
additional physics and hook code. It tracks activated objects and restores the
saved game state after prediction. This is substantially more involved than
projecting a line from position and vertical velocity.

A predictor's observation length/input policy is also part of its result. A
short-horizon survival prediction and completion of a fixed remaining macro are
different tests even when their underlying collision model is identical.

The upstream code is GPL-3.0. No upstream implementation was copied into this
MIT project. The study informed independent changes to measurement criteria and
observability. Frame Window Lab still uses native full-start trials; this release
does not ship Silicate's predictor, checkpoint restorer or a new physics decompilation.

## Concrete evidence from the user's report

The old completed report contains input #4 (release at tick 280):

| Offset | Old result to tick 960 | Stopped at boundary | Local result to tick 324 |
|---|---|---:|---|
| -2 | fail | 293 | fail |
| -1 | fail | 571 | pass |
| 0 | pass | 960 | pass |
| +1 | fail | 325 | pass |
| +2 | fail | 323 | fail |

Thus the stored observations support a local interval `-1..+1` at endpoint 324,
while the original goal interval is `0..0`. The new regression fixture checks
exactly this distinction. Endpoint 324 is an explicit comparison example, not
an automatically detected end of a passage. Choosing endpoint 321 instead would
produce a different local result. Neither value should be selected merely to
force a desired width.

This also explains why blindly multiplying old 1-tick results by three would be
incorrect. Visual/manual verification of a specific obstacle and fresh native
runs remain necessary to establish its intended local endpoint and timing model.

## Diagnostics added because of the study

Native progress, step count, dt before/after modification, command dt, timewarp,
RNG, both players' position/vertical velocity/mode/button state, observed local
boundary and collision object/position can now be inspected together. Repeated
attempts are retained separately so an apparent width cannot hide inconsistent
outcomes. These records help separate endpoint choice, input-phase drift,
collision differences, reset problems and interference from other mods.
