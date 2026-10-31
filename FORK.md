# CataZoo Linux build fork

This fork starts from upstream CDDA commit
`a768761b2544403c24fe3a6bc6eb604d65557ddf`. It adds Linux build infrastructure and
a desktop port of Android's contextual touch shortcut strip.

## Enable touch shortcuts

The shortcut strip is **off by default**. Opt in for one game process:

```sh
./cataclysm-tiles --touch-shortcuts --userdir /path/to/userdata
```

Launchers can choose whether to pass `--touch-shortcuts` for each installation.
The flag is advertised by `--help`; official builds without this patch do not
support it. No persistent setting or automatic touchscreen detection enables it.

The translucent strip at the bottom follows the live `input_context`, including
menu hotkeys, NPC dialogue replies, mutations, and bionics. It uses the current
keybindings, retains Shift/Ctrl/Alt modifiers, and moves recently used valid keys
to the front within each context. Recent-key history lasts for the current game
process. Additional keys are available using the strip's previous/next buttons.
Touch or click a key to activate it on release; dragging off cancels it. Pressing
a key displays its action description. Keyboard and controller input remain active.

The desktop port uses the same registered actions and manual-hotkey metadata as
Android, with a separate SDL adapter for desktop keycodes and mouse-emulated
touch (as used by some Gamescope configurations). Android JNI, soft keyboard,
vibration, joystick gestures, and nearby-world action heuristics are not enabled
on Linux. Gameplay actions come from the active context and their current bindings.

## Build a Linux package

Open **Actions → Linux x86_64 Build → Run workflow** on this repository.

- The only game target is Linux x86_64 with graphics and audio enabled.
- Builds are manual; pushing commits does not publish a release.
- Leave **publish** unchecked to download the `cdda-linux-x86_64` Actions artifact.
  It contains the game archive, `SHA256SUMS`, and `VERSION.txt`.
- Check **publish** to also publish a prerelease after package validation succeeds.
- **translations_tag** optionally pins an official `cdda-experimental-*` release.
  If empty, the build uses the newest experimental release with translation assets.
  The selected translation release is recorded in the build metadata.
- No Transifex token or Android signing credentials are needed.
- Tilesets are composed using upstream's Linux workflow. Sound playback is enabled;
  install a soundpack separately through CataZoo or the game's data directory.

The workflow checks translations using the same `msgfmt -f` flags as the game.
Broken non-Chinese catalogs are omitted with an Actions warning. Missing simplified
Chinese or invalid Chinese catalogs fail the build. This avoids the upstream
2026-10-31 Linux release failure caused by an invalid Hebrew translation.

The package includes upstream's bundled SDL runtime. CI checks its dependencies
and runs the packaged executable with `--version` outside the build library path.
This is not a Steam Deck hardware acceptance test; touch input, controllers,
audio, rendering, and SteamOS compatibility still need testing on the device.

## Enabled workflows

Only `linux-release.yml` and its Linux-only helper `compose-tilesets.yml` are
enabled in this fork's GitHub Actions settings. Upstream workflow files remain
in the repository to make future merges easier, but their workflows are disabled.
After syncing upstream, review any newly introduced workflows before enabling
them. Do not enable the upstream multi-platform release workflow for this fork.

The local `upstream` remote points to `CleverRaven/Cataclysm-DDA`; `origin` points
to this fork. Keep future touch changes separate from build infrastructure so
upstream merges remain reviewable.
