# Silicate Frame Window

This is a Frame Window analysis fork of **Silicate 1.0.2**. The original Silicate bot/replay logic is retained; see [`FRAME_WINDOW.md`](FRAME_WINDOW.md) for the analyzer design and usage, and [`UPSTREAM.md`](UPSTREAM.md) for attribution/license notes.


The sections below retain upstream Silicate documentation where it is still applicable.

## End of life notice

Silicate is being rewritten (version 2). This version will be maintained enough to not have breaking issues until v2 is fully ready.

## Structure

```
src/
    assist/ - Assist features, such as autoclicker or hitboxes.
    bot/ - Core bot components.
    frame_window/ - Frame Window analyzer and diagnostics.
    checkpoint/ - The practice fix.
    hooks/ - All of the hooks Silicate uses. Interacts with core game logic.
    label/ - The label system for displaying overlays.
    physics/ - Geometry Dash physics decomp for trajectory.
    render/ - The renderer and DSP recorder.
    replay/ - The replay system.
    settings/ - The bot's settings module.
    shared/ - Shared parts of the code, such as keybind logic.
    trajectory/ - Simulation/trajectory logic.
    ui/ - The interface.
    util/ - Generic utilities, such as midhooking.
lib/
    tabby/ - The UI library, based on ImGUI.
```

## Compiling

This fork is supported **only through GitHub Actions**. Push it to GitHub and run `.github/workflows/build.yml`. The workflow produces a Win64 `RelWithDebInfo` Geode package plus PDB/debug-symbol artifacts. Local build scripts from upstream were intentionally removed.

## Contributing

Please use feature branches. Use clang-format for formatting your code (unless it makes it horribly unreadable).
Currently we do not have automated testing. Please test the features you're implementing/changing and related components before releasing builds.
