# Wii Menu Recomp

The Wii Menu as a native desktop application, with its own startup launcher,
system-data import, controller assignments and peripheral settings. This
repository owns the Menu's presentation and release configuration and pins the
[wiillr engine](https://github.com/mstan/wiillr) as a Git submodule.

Provide your own supported Wii Menu 4.3U NAND backup or extracted system folder.
Firmware, console keys, installed channel content and generated guest code are
external inputs. Preserve your managed NAND and per-user settings when updating.

![Wii Menu launcher](docs/images/wii-menu-launcher.png)

![Original Wii Menu channel grid](docs/images/wii-menu-running.png)

## Adaptive Fit to screen

Enable **Adaptive channel grid (experimental)** in **Mods** and select **Fit to
screen**, then resize the window. The original three rows remain, with two
columns in portrait and up to six in wider windows. The Menu viewport is capped
at 21:9; wider windows retain their size with side margins. Neighboring columns peek
through at the sides, and paging uses the original sliding animation. Channels
keep their original identities and NAND order.

The pointer, clock and bottom controls retain their proportions. Channel frames
blend with the original backdrop. Stock presentation remains available; channel
rearrangement is disabled while the adaptive mod is active. Other Menu screens
retain their original layouts. Android support is not implied.

![Fit to screen in an ultrawide window](docs/images/wii-menu-fit-wide.png)

<img src="docs/images/wii-menu-fit-portrait.png" width="320" alt="Fit to screen with two columns and three rows in a portrait window">

[Setup, controls and peripherals](https://github.com/mstan/wiillr/blob/4e18ac9dcaee0c3817e73544827a314f943808bc/docs/wii-menu.md)
include four Wii Remote assignments, two USB ports and an SD Card slot. Keyboard
typing uses the USB keyboard peripheral independently of Wii Remote buttons.

## Expanded channel library

**Expanded channel library (experimental)** is available in **Mods**, disabled
by default. Choose 2x through 128x the stock 48 positions and
restart Wii Menu. Installed overflow channels appear after the stock positions;
the three rows, adaptive columns and original page transitions remain.
The main grid shows a Wii-style **X / Y** page counter that recalculates when
resizing changes the column count.
[Library setup, ordering, shortcuts and limits](docs/channel-library.md) explains
how this stays separate from native saves and stock channel order. Execution
support for each launched title remains a separate requirement.

## Run the Windows preview

Download the Windows archive from [Releases](https://github.com/mstan/wii-menu-recomp/releases),
extract the complete archive and open **Wii Menu**. Import your supported NAND
backup or extracted system folder in the launcher. The source-build arguments
below are not needed to run the downloaded application.

## Local builds

```sh
git clone https://github.com/mstan/wii-menu-recomp.git
cd wii-menu-recomp
git submodule update --init wiillr
```

These are maintainer source-build commands. The generated-code input bundle is
private; a public clone and a NAND backup alone do not currently supply it.
Use the matching native toolchain and qualified build inputs described in the
engine's [build guide](https://github.com/mstan/wiillr/blob/4e18ac9dcaee0c3817e73544827a314f943808bc/docs/building.md).

Windows PowerShell:

```powershell
./tools/build-windows.ps1 --inputs "C:/wii-build-inputs" --jobs 2
```

Linux:

```sh
bash tools/build-linux.sh --inputs /path/to/wii-build-inputs --jobs 2
```

macOS:

```sh
bash tools/build-macos.sh --inputs /path/to/wii-build-inputs --jobs 2
```

The consumer selects the product automatically. Its version defaults to the
value in `product.json`; `--version 0.1.1-local.1` is an optional package-label
override, unrelated to Wii Menu 4.3U. `--inputs` (or `--inputs-dir`) accepts a verified developer bundle directory
or checksum-pinned ZIP containing `inputs.json`, sources and the qualified catalog.
It does not point to a NAND or game folder.

Build working files default to `build/HOST-ARCH` and archives to `dist`.
Use `--build-dir` and `--output-dir` to override them. Add `--check` to validate
the input bundle and basic prerequisites without compiling or launching a game. The engine's
[argument reference](https://github.com/mstan/wiillr/blob/4e18ac9dcaee0c3817e73544827a314f943808bc/docs/building.md#build-arguments)
defines valid products, inputs, versions and common errors.

`product.json` owns the product identity and default version. `tools/build.py`
selects this consumer's provider and delegates compilation to the pinned engine;
`tools/package.py` packages explicit outputs with the consumer's documentation
and required source/license notices. Builds run locally. The adaptive preview
archive targets Windows x86_64. These wrappers have passed a fresh Windows
source build with the matching input bundle. Linux and macOS scripts are
provided; native consumer builds and runtime qualification on those hosts
remain pending.

The provider and consumer build scripts use [GPLv3 or later](LICENSE).
[Licensing details](LICENSING.md) preserve upstream terms and earlier grants. The engine's
[Menu annotation guide](https://github.com/mstan/wiillr/blob/4e18ac9dcaee0c3817e73544827a314f943808bc/docs/wii-menu-references.md)
records the original-version identities and reverse-engineering references.
