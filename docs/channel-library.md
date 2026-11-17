# Expanded channel library

**Expanded channel library (experimental)** is an opt-in Mods feature. Stock
mode keeps 48 positions. Its capacity choices are 2x (96), 4x (192), 8x (384),
16x (768), 32x (1,536), 64x (3,072) and 128x (6,144).
Restart Wii Menu after changing this feature or its capacity.
The enabled state and selected capacity persist in `channel-library.cfg` in
the application's per-user preferences folder.

The original three rows remain. With Adaptive channel grid and Fit to screen,
the window determines the column count independently of library capacity.
Adaptive Fit to screen currently caps the Menu viewport at 21:9 (six columns).
Wider windows and fullscreen displays keep their size and show side margins;
the tiles, pointer, clock and footer use the same capped viewport. This avoids
unsupported layouts at extreme widths. Portrait and ordinary widescreen layouts
continue to adapt within that limit.
Without adaptive layout, expanded pages use four columns. The original sliding
animation and neighboring column peeks remain. Rearranging channels inside the
Menu is disabled while either grid experiment is active.
The main grid's **X / Y** page counter uses the original Menu font and follows
the live layout. Resizing recalculates the page total while keeping the current
channel position nearby. The counter stays hidden on other screens and with
stock capacity; page changes follow the original sliding transition.

## Installed channels and ordering

The first 48 positions follow the copied NAND's stock channel order. Additional
installed channels with tickets, TMDs and their content appear after those
positions. The library saves overflow identities and the last viewed position in
`.wiillr-channel-library`, beside the copied NAND marker. This host sidecar is
separate from `iplsave.bin`, installed content and native progress. Reducing the
capacity hides positions outside the selected limit; it retains their stored
identities for a later increase. Turning the feature off restores stock mode
after restarting.

Keep the sidecar when moving or backing up the managed NAND. An invalid sidecar
is preserved instead of overwritten. Its error appears on the expanded-library
feature in Mods. Repair or move aside only this sidecar after backing it up. Native saves are
independent of it.

## Optional installed-title shortcuts

Advanced setups can place shortcuts in `channels.cfg` in the managed NAND
folder. Each line contains a zero-based position of 48 or greater, followed by
an installed 16-digit hexadecimal Wii title ID. For example, position 48 is the
first position beyond stock; position 95 is the last in the 2x library. Blank
lines and lines that do not describe an installed eligible title are ignored.

This file is optional. Ordinary installed overflow channels need no file.
Shortcuts can point to an already installed title more than once. They do not
copy the title, allocate a second save, or create a WAD/forwarder executable.
To remove a shortcut while preserving its empty position, add a line such as
`48 -`. This clears only that library position. It never deletes its target's
content, disc image or progress. Merely removing a line leaves the last stored
placement in the host sidecar; the explicit `-` clears that placement.

## Data Management and limits

Save Data shows actual native saves. A shortcut is not another saved game.
Channel management continues to operate on installed titles through the stock
worker; deleting an installed title has its ordinary effects. An acquired stock
channel refresh invalidates the residency map before it is rebuilt.

This feature expands the Menu's positions. It does not supply channel content,
compile new game code, make arbitrary homebrew executable, change original NAND
quotas, or implement a complete disc-image forwarder system. Each launched title
still needs a supported native build and its original installed data. A title
that is visible in the library is not thereby qualified for execution. Public
firmware/content distribution is not part of this feature.

The native title runtime has a separate known limitation: HOME during Mega Man
9's early startup can stop rendering on an unsupported modal fade. The
title-screen HOME return is qualified. Expanded positions do not remove that
startup limitation.

Metadata and overflow ordering use host memory. The guest retains 48 resident
channel records and at most the stock tile-object pool. New banners use the
original asynchronous loader. Offscreen records are recycled only after their
requests and active module workers release ownership. No virtual page/index is
stored in a stock guest array.
