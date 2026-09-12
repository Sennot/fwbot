# Frame Window Lab

A Windows x64 Geode mod source project for measuring conditional input windows from `.gdr` and `.gdr2` replays. Targets **Geometry Dash 2.2081 / Geode 5.8.2**.

**[Полная инструкция на русском](README_RU.md)** · [Validation status](VALIDATION.md) · [Pinned sources](SOURCES.md)

The implementation replays trials through the native game engine for cube, ship, wave, ball, UFO, robot, spider, swing, mini forms, dual and platformer inputs. It supports individual press/release shifts, duration-preserving hold shifts, repeated baseline checks, disjoint passing intervals, removal tests, boundary previews and JSON/CSV export. Input is restricted to whole native command ticks at 240 TPS. CBF and correction-based replay are unsupported.

**Version 1.1.0:** separate Local and Goal windows, independently repeated local verdicts, and one `*.debug.json` containing replay/level data, environment and bounded trial traces. Local end defaults to the next same-channel input; an explicit tick can be selected. This is not automatic obstacle-boundary detection. See [changes](CHANGELOG.md) and [Silicate study](SILICATE_NOTES.md).

**Testing candidate:** the user ran the earlier release. Updated portable core tests have run locally. The Windows DLL, hooks, UI and actual gameplay have not been compiled/executed in the delivery environment. Native support for the listed modes describes the implementation, not a completed gameplay certification.

## Build with GitHub Actions

Upload this directory's contents to the repository root, including `.github/workflows/windows.yml`. Run **Actions → Build Frame Window Lab (Windows x64)**. After a successful run, download **FrameWindowLab-Windows-x64**, extract the `.geode` file into `Geometry Dash/geode/mods` and restart. The workflow builds only Windows; it also runs portable core tests before compiling the mod.

## Use

Open the matching level in normal mode without StartPos. Pause → **FW Lab** → **Import**. Start with a short input range and a small radius. End tick `0` means actual level completion; a positive tick means survival until that boundary. Scan, then inspect individual rows under **Results**. Esc pauses, Resume replays the interrupted trial, Stop preserves partial results. Completed rows autosave in the mod's Geode save directory; Export opens it.

The window is conditional on all other macro inputs staying fixed. Separate successful islands are never merged across failures. A successful search boundary is explicitly marked open. JSON uses the project's own schema, not a claimed NaNDL import format.

## Portable tests

```sh
cmake -S . -B core-build -DFWL_BUILD_MOD=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build core-build --parallel
ctest --test-dir core-build --output-on-failure
```

Project code is MIT licensed. The vendored nlohmann/json header has its own included MIT license.
