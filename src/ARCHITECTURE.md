## Architecture

There is one `Backend` created over the entire lifetime of the app, which holds the channel list of the loaded .vgm file, and renders when needed. `MainWindow` can tell `Backend` to load a different VGM file. `StateTransaction` is used to setup app state on startup, and manages reloading state in response to user interactions (switching files, reordering chips, currently not toggling channels). I may eventually move `StateTransaction` to backend.h and remove its `MainWindowImpl *` field, then have `MainWindow` subscribe to a new `Backend::stateChanged(StateTransaction &)` signal instead.

## Channel order

When opening a new VGM file, `Metadata::make()` loads the list of chips and channels. To enumerate the chips in a VGM file, it calls `PlayerBase::GetSongDeviceInfo()` which returns an ordered list of chip IDs. We mostly keep libvgm's chip order intact, but to improve Sega Genesis songs, we reorder SN76496 after YM2612. To map each chip to a list of channels, `Metadata::make()` calls a function located in `vgm.cpp`. This file mostly keeps libvgm's channel order intact, but in the case of YM2608, it reorders SSG before rhythm and ADPCM.

`Metadata` stores a chip list and a flat channel list. The user can reorder chips but not channels. When the user reorders chips, we reorder the flat channel list to match.

## Rendering

Rendering is managed in `Backend`. Render jobs (either a single soloed channel, or master audio) are sent to a `QThreadPool`, which spawns 1 thread per CPU core and distributes jobs among threads. For most sound chips, the master audio job takes much longer than the other jobs. If this was not the case, some renders could complete more quickly by spawning more threads than CPU cores (eg. on a 4-core CPU, rendering 5 channels simultaneously is faster than only rendering 4 channels initially, then starting the 5th channel once one channel finishes).

While a render is active, the modal `RenderDialog` shows the rendering progress, and blocks the user from interacting with `MainWindow` and editing `Backend` until the render is finished.
