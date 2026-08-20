# Silicate Frame Window Analyzer

This fork keeps Silicate 1.0.2's replay recorder, player input execution, update loop, reset logic and Geometry Dash physics path. The Frame Window code is an analysis layer; it does **not** replace Silicate with a new bot or a separate physics implementation.

## Definition used

The canonical value follows NaN / NaNDL terminology:

> Frame Window (`N_i`) = the number of frames/ticks available to pass timing `i`.

For one recorded player action, the analyzer keeps every other replay action unchanged, shifts only that press/release by integer ticks, and lets Silicate replay the real level. Offset `0` is the verified baseline. The contiguous passing offsets around `0` are reported as the canonical Frame Window.

Example: offsets `-1, 0, +1` pass and `-2, +2` fail => `FW = 3`.

Presses and releases are separate inputs. Platformer Left/Right actions are also separate inputs.

## Spam / straight fly context

Canonical Frame Window is never replaced by the contextual metric.

When **Sequence context** is enabled, adjacent dense inputs are additionally tested in small paired perturbations (`-1/-1`, `-1/+1`, `+1/-1`, `+1/+1`). If a joint variant passes while one of those same offsets failed individually, both inputs are marked `COUPLED`.

This is intended to flag spam, straight fly, wave spam, micro-clicks and other sequences where neighboring timings can compensate for each other. It is context information only; NaNDL export still uses each input's canonical `N_i`.

## Supported gameplay states

The analyzer does not implement per-gamemode physics. It identifies the current player state for diagnostics/UI and delegates the actual outcome to Geometry Dash through Silicate playback. The UI recognizes:

- Cube
- Ship
- UFO
- Wave
- Ball
- Robot
- Spider
- Swing
- Platformer
- P1 / P2 (including dual replays)
- Platformer Jump / Left / Right press and release

Because pass/fail comes from the real replay path, portals, gravity, mini, speed changes and other level state remain part of the trial.

## How to use

1. Load or record a clean completion replay in Silicate.
2. Open the **Frame Window** tab.
3. Choose an input range. `Last input = 0` means all player inputs.
4. Leave `Sequence context` on for spam/straight-fly diagnostics if desired.
5. Press **Analyze Frame Windows**.
6. The analyzer first runs the unmodified replay as a baseline. If the baseline dies or never completes, analysis stops rather than producing fake results.
7. Each selected press/release is then tested using Silicate playback.
8. Results show input number, original frame, gamemode, P1/P2, button/state, canonical FW, early/late offsets and anomaly flags.

During analysis the fork temporarily forces settings that are required for an honest pass/fail test (Playback mode, user-input blocking, noclip off, prevent-death off, autoclicker off, input mirroring off). The previous settings are restored afterwards.

## Performance / scope

The first implementation prioritizes correctness and debuggability over maximum speed. A passing trial is validated by completing the level with all other replay inputs unchanged. Failing trials usually terminate at death. Use input ranges when analyzing very long macros.

`Search radius` is a safety bound. If the passing interval reaches the radius, the result is marked `RADIUS LIMIT`; increase the radius and re-run that range.

`Stop after failures` controls how many consecutive failed offsets terminate the scan on one side. The canonical window is always the contiguous set containing the original offset `0`.

## NaNDL JSON

The calculator currently documents JSON export/import containing frame-window rows, Game FPS, Window FPS, respawn time and the frame-number/seconds toggle. This fork exports those data and imports common field aliases. The exact private/front-end field naming used by a future NaNDL deployment can change; if an NaNDL-generated JSON does not round-trip, put it at:

`<Geode persistent dir>/framewindow.silicate/frame-window/nandl-import.json`

and use **Import NaNDL JSON**. The tolerant importer is deliberately isolated so schema fixes do not affect FW measurement.

## Diagnostics

All diagnostics are written under the mod persistent directory in `frame-window/`:

- `frame-window-debug.json` — full support dump: config, replay actions, every tested offset, pass/death/timeout/invalid result, player state, FW results, sequence context and recent events.
- `frame-window.log` — line-by-line live log, flushed continuously.
- `latest-trial.json` — small crash-friendly snapshot overwritten at each state transition.
- `nandl-export.json` — NaNDL-oriented export.
- `nandl-import.json` — default path read by the Import button.

For a normal logic bug, send `frame-window-debug.json`. For a hard crash during a trial, also send `latest-trial.json` and `frame-window.log`. GitHub Actions exports/bundles Windows PDB symbols in `RelWithDebInfo`, so keep the build artifact or build SHA with the report.

## Build-fix history

- `v1.0.2-fw.2`: make upstream `/W4` conditional on the actual MSVC compiler. The current Geode Win64 GitHub Action drives `clang++.exe`; passing `/W4` to that GNU-style frontend makes Clang treat it as a filename and abort while creating the PCH. No replay, bot, physics, or Frame Window logic changed in this fix.

## Build policy

This fork intentionally ships **one supported build path: GitHub Actions**.

Workflow: `.github/workflows/build.yml`

- Windows x64 only
- Geode SDK 5.8.2
- `RelWithDebInfo`
- PDB export enabled
- PDB bundled into the `.geode` package
- exact source uploaded as a second Actions artifact

Push the repository to GitHub and run **Actions -> Build Win64 (Geode) -> Run workflow** (or push a commit). No local build script is provided by this fork.
