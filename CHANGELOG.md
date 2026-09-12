# 1.1.0 — local windows and reproducible diagnostics

- Display **Local / Goal** separately. Goal retains the original fixed-tail criterion; Local has a visible observation endpoint.
- `Options → Local end`: `0` selects the next edge of the same player/button, or the goal if none exists. A positive value is an explicit absolute tick, capped by the goal. In pair mode the automatic endpoint follows the pair's release.
- Aggregate repeated Local verdicts independently from Goal verdicts. Death after the local endpoint no longer narrows the Local window. Unstable/unmeasured offsets never bridge successful islands.
- Report offsets that survive locally but fail later. Mark open bounds with `+`; retain ordering and endpoint restrictions.
- Include the current analysis status in Results. `Local early` / `Local late` select Local boundaries for preview; preview reports both verdicts.
- Add one `analysis-<timestamp>.debug.json` containing the report, complete canonical inputs, raw replay bytes up to 2 MiB (hex, with truncation metadata), serialized level, loaded mod versions, per-trial outcomes, native timing counters, player states, observed local-boundary state, collisions and desync fingerprints.
- Save debug at start, every five completed rows, on termination/level exit and on explicit Debug/Export. Ordinary report/CSV still update after each completed row. This is not a crash dump or a promise to preserve a trial interrupted by process termination.
- Bound trace memory and record dropped counts explicitly. Default: 20,000 trial summaries, 16,000 near-edge/tail snapshots, 5,000 initial baseline states and 2,000 events. Each trial can additionally retain its local boundary and terminal collision state.
- Add normal Geode log milestones for import, analysis start, baseline checks, row completion, errors and stop reason.
- Recognize the alternate `absolllute.hackmega` ID in the existing conflict check.
- Add stdlib-only `tools/inspect_debug.py` and regression tests derived from the reported 1-tick versus 3-tick example.

These changes distinguish measurement criteria. They do not automatically identify the physical exit of every obstacle or prove that every disputed input has a 3-tick window.

# 1.0.1

Use Geode's asynchronous picker; block repeated imports and queued focus-return clicks; retain the popup safely until picker completion.

# 1.0.0 build fixes

Pass `LINK_TYPE PRIVATE` to `setup_geode_mod`; compare hook pointers through their common GJBaseGameLayer base.
