# Wii Menu Recomp

Wii Menu consumer for the pinned [wiillr engine](https://github.com/mstan/wiillr).
Original system files, console keys, installed channels and generated guest code
are external inputs and are never included in this repository.

This development branch adds an optional **Adaptive channel grid** mod. It keeps
three rows and changes the column count as the Fit-to-screen window is resized:
two columns in portrait, six or seven in wider desktop windows. It reuses the
original artwork and GUI objects, preserves native slot identities and NAND
ordering, and keeps stock presentation available. Channel rearrangement is
disabled while the experiment is active. Other Menu screens retain their original
layouts; Android support is not implied. Owner visual review is pending.

Neighboring pages peek through at the edges. Paging uses the original Menu's
sliding animation, while the bottom controls and clock keep their proportions
as the window changes shape.

Initialize the engine submodule, then use the local platform toolchain:

```sh
git submodule update --init --recursive
python tools/build.py --version 0.1.0-adaptive --inputs-dir /path/to/qualified-inputs --build-dir /path/to/build --output-dir /path/to/dist --jobs 2
```

The command selects this consumer's provider and the Wii Menu product. The pinned
engine's [build guide](https://github.com/mstan/wiillr/blob/b187ae905556df9412f0dfcd2e159d463473e9a5/docs/building.md) describes Windows, Linux and macOS
dependencies and the private qualified input bundle. Builds run locally.

The provider is original MIT-licensed code. Reverse-engineering references and
version identities are documented in the engine's
[Menu annotation guide](https://github.com/mstan/wiillr/blob/b187ae905556df9412f0dfcd2e159d463473e9a5/docs/wii-menu-references.md). Native saves and
explicit player preferences must be preserved when updating.
