# Source references and dependency pins

Reviewed upstream source rather than relying on remembered SDK signatures. These are the implementation references; they do not establish that this mod was tested inside the game.

| Source | Revision / role |
|---|---|
| [Geode SDK](https://github.com/geode-sdk/geode/tree/v5.8.2) | `v5.8.2`, commit `2a5fd87433da47d6bf07221774f0cbb25535ae08`; loader APIs, Popup, TextInput, file utilities, hooks, CMake |
| [GD bindings](https://github.com/geode-sdk/bindings/tree/7f6c2a75742856de88dad354e576dcff8a28e881) | Commit `7f6c2a75742856de88dad354e576dcff8a28e881`, `bindings/2.2081/GeometryDash.bro`; Windows methods and fields |
| [Official build action](https://github.com/geode-sdk/build-geode-mod/tree/e26ece68d56a3cfa0a7f790d8120c965008c3d08) | Commit `e26ece68d56a3cfa0a7f790d8120c965008c3d08`; pinned in workflow |
| [GDR1](https://github.com/maxnut/GDReplayFormat/tree/77bd505853541a87a12895dc23de05f6585d0515) | `gdr1`, commit `77bd505853541a87a12895dc23de05f6585d0515`; legacy MessagePack / JSON fields |
| [GDR2](https://github.com/maxnut/GDReplayFormat/tree/2a0ca3cff9b1d16863037a464da5fe27c28a5823) | `gdr2`, commit `2a0ca3cff9b1d16863037a464da5fe27c28a5823`; binary writer and test fixture generation |
| [nlohmann/json](https://github.com/nlohmann/json/tree/v3.12.0) | Vendored single header v3.12.0, MIT, license included |
| [Click Between Frames](https://github.com/theyareonit/Click-Between-Frames) | Reference for input-phase and timewarp interactions; no dependency or copied implementation |
| [NaNDL calculator](https://nandl.pages.dev/) | Original task context; this mod's output is a distinct report format |

## Wire-format decisions

The GDR2 writer is authoritative for interoperability here: three-byte `GDR` magic, unsigned LEB128 version `2`, length-prefixed strings, big-endian IEEE floats, grouped player streams and delta-packed inputs. Per-input extension data is skipped according to the input tag. Some upstream README wording differs from the actual writer, so the reader was checked against independently generated writer fixtures.

No upstream GDR parser is vendored. `src/core/ReplayIO.cpp` is an independent bounded implementation. `tools/generate_fixtures.cpp` requires the pinned GDR2 upstream headers if regenerating the fixtures; building and testing this project does not fetch GDR2.

The workflow pins the Geode action, SDK version and bindings. It uses the action's default current CLI and hosted runner toolchains, so the complete build environment is not bit-for-bit locked.
