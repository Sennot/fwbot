# Validation record

Delivery state: v1.1.0 source / testing candidate, 2026-09-12.

## v1.1.0 checks

The portable core and tests were compiled with GNU C++ 13.3.0 using undefined-behavior sanitization and fail-on-first-error. Result: **PASS: 3022 checks**, exit code 0, no UBSan diagnostics. These are assertions, not 3022 gameplay scenarios.

New checks cover local-vs-goal classification, exact death boundaries, explicit/automatic/pair endpoints, unexecuted shifted inputs, independent local instability, serialized local results, diagnostic limits/drop counters, local-boundary capture, interrupted/running trials and reset behavior. The regression fixture reproduces observations from the user's old report; it is not a fresh simulation of that level.

The Python debug inspector also reads the user's older report. Native hook/API fields were checked against the pinned SDK and bindings. The fresh Windows build, UI and gameplay have not run here. User confirmation and attached reports establish use of the earlier release only.

Next gameplay checks: build in Windows CI; import once; compare one known passage at a manually verified local endpoint; check late failure produces Local pass / Goal fail; pause/resume and export; inspect one debug file for canonical inputs, level data, mod list, both verdicts and collision/local-boundary records. Automatic Local end is a same-channel input boundary, not a geometrically detected passage boundary.

## Earlier implementation history


Build-log follow-up: corrected the mixed `target_link_libraries` signatures by
passing `LINK_TYPE PRIVATE` to `setup_geode_mod`. Geode 5.8.2 explicitly parses
this argument and uses it for both SDK and dependency linkage. The reported
Windows configuration error is addressed; a subsequent full Windows build has
not been run here.

Second user-provided Windows build log: configuration succeeded, and core,
Engine.cpp and LabPopup.cpp compiled. Hooks.cpp failed on a comparison between
the sibling pointer types FWLBase* and PlayLayer*. That comparison now explicitly
converts both operands to their common GJBaseGameLayer* base. Other hook pointer
comparisons were reviewed and compare directly related types. Linking, packaging
and runtime validation remained unconfirmed at that stage.

## Import dialog hotfix (v1.0.1)

The user subsequently confirmed that v1.0.0 built and ran, but reported that
selecting a macro repeatedly reopened the file picker. The synchronous
GetOpenFileNameW call inside the menu callback has been replaced with Geode
5.8.2's asynchronous file::pick / async::spawn API. The synchronous modal input
path is a suspected cause; the original behavior was not reproduced locally.

Only one picker can be in flight across popup instances. Popup actions are
ignored during selection, and a brief focus-return cooldown suppresses queued
import clicks. Success imports once; cancellation and errors terminate without
reopening. A retained popup is checked for being in the scene before using a
late result. The unused comdlg32 dependency was removed.

API signatures and callback delivery were checked against the pinned SDK's own
manual-install picker implementation and async.hpp. Core code is unchanged.
The hotfix has not yet been built or exercised in Windows here. Runtime checks:
select once, double-click Import, cancel then retry, select a malformed file,
select a valid Unicode path, and close the popup before a delayed result.

## Executed locally

- GNU C++ 13.3.0 on Linux compiled the portable core and test executable.
- Earlier native CMake/CTest run: 1/1 test passed.
- Earlier source revision compiled with `-std=c++23 -O0 -g -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all`.
- Earlier executable result: **`PASS: 3000 checks`**, exit code 0; no UBSan diagnostics or compiler warnings.
- Workflow YAML parsed; manifest, Windows-only job/target and pinned bindings checked.
- Geode-facing signatures and fields were inspected against the pinned SDK/bindings source. This is source review, not a compiled ABI test.

The checks cover legacy JSON and MessagePack, official GDR2-writer fixtures, big-endian metadata, player-stream interleaving, platformer buttons and extension skipping, all truncation prefixes of a fixture, malformed-input corpus, varint overflow, invalid inputs/configuration, edge and pair shifts, original and discovered endpoints, P2 filters, disconnected windows, unstable ticks, partial report semantics, and per-channel ordering/alternation for every offset in small exhaustive cases.

The number 3000 includes property assertions and corpus cases; it does not mean 3000 gameplay scenarios.

AddressSanitizer was also attempted, but its runtime could not read process/executable metadata in this sandbox and warned about invalid stack bookkeeping and possible false positives. It subsequently reported a stack-use-after-scope around exception handling. That run is **inconclusive**, not a passed memory-safety check. The independent final UBSan run above completed successfully. Windows CI runs ordinary core tests; repeat ASan on a normal Linux host if extending the importer.

## Not executed here

- Windows compiler/linker or `.geode` packaging.
- A GitHub Actions run in a user repository.
- Loading hooks in a running Geometry Dash process.
- Visual/layout and input-focus checks in the game.
- Successful replay/baseline/window checks on real level macros.

No `.dll`, `.geode`, fabricated build log or fabricated gameplay measurement is included in the source archive.

## Gameplay acceptance checks before calling the build stable

Use a clean matching game/Geode install and an unchanged local level copy. Start with a small radius and a known successful 240 TPS replay.

| Case | Required observation | Status |
|---|---|---|
| Windows workflow | Core tests, DLL link and `.geode` package all succeed | Pending |
| Pause menu / Unicode import | FW Lab opens, picker works, controls and close/resume are usable | Pending |
| Baseline frame convention | Known macro passes repeated baseline with documented offset | Pending |
| Cube / ball / UFO | Manually verified early/on-time/late presses agree with the scan | Pending |
| Ship / wave | Both press and release boundaries agree; hold-pair duration remains fixed | Pending |
| Robot / spider / swing | Long holds, releases, teleport/gravity transitions reproduce | Pending |
| Mini / inverted / mode portals | Original replay and boundaries remain repeatable | Pending |
| Dual / two-player | Same-tick P1/P2 inputs reach the correct player | Pending |
| Platformer | Jump/left/right replay; a known continuous finish passes | Pending |
| Timewarp / moving / RNG triggers | Command-tick convention and seed/reset behavior reproduce | Pending |
| Explicit endpoint / completion | Inputs beyond the goal excluded; shifted unexecuted events never pass | Pending |
| Pause / resume / cancel / quit | Interrupted trial restarts, completed probes persist, hooks restore | Pending |
| Preview followed by export | Original report status, level fingerprint and rows preserved | Pending |
| Statistics isolation | No awarded completion or persistent scan attempts/jumps/percent | Pending |
| Conflicting mods / failed baseline | Clear error and no misleading completed window | Pending |

For discrepancies retain the source replay, level copy/ID, exact game/Geode versions, active mod list, config/offset, report and crash/build log. Do not change macro metadata to force a passing baseline.
