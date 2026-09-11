# Validation record

Delivery state: source implementation / testing candidate, 2026-09-11.

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
and runtime validation remain unconfirmed.

## Executed locally

- GNU C++ 13.3.0 on Linux compiled the portable core and test executable.
- Earlier native CMake/CTest run: 1/1 test passed.
- Final source revision compiled with `-std=c++23 -O0 -g -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all`.
- Final executable result: **`PASS: 3000 checks`**, exit code 0; no UBSan diagnostics or compiler warnings.
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
