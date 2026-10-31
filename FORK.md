# CataZoo stable Linux fork

This fork supports **official CDDA stable releases only**, starting with
[Ito-1 / 0.I-1](https://github.com/CleverRaven/Cataclysm-DDA/releases/tag/0.I-1),
commit `7b2efa5cea38e4d4d97dd0e63b28b9148623da59`. Its only packaged target is
Linux x86_64, including Steam Deck. macOS is a development preview.

`stable` is the maintained/default branch. The earlier SDL3 experimental
prototype remains in `master` as history; it is not a release source. Do not
merge upstream `master` or experimental tags into the maintained branch.

## Opt-in touch shortcuts

```sh
./cataclysm-tiles --touch-shortcuts --userdir /path/to/userdata
```

The strip is **off by default**, enabled only for this process by the command
line flag. Launchers can decide whether to pass it per installation. Official
unpatched executables do not support the flag; patched builds advertise it in
`--help`. The CataZoo launcher toggle is not implemented by this repository.

The translucent bottom strip follows the active input context and current
keybindings, including manual menu/dialogue/mutation/bionic hotkeys. Modifiers
are preserved. Recently used valid keys move to the front within each context;
history lasts for the process. Overflow keys are paged. Releasing on the pressed
button activates it; releasing outside, changing context, resizing or losing
focus cancels the press. Transparent unused slots pass input through.

This ports Android's contextual shortcut concept and metadata. It does not
enable Android JNI, its software keyboard, vibration, joystick gestures or
nearby-world heuristics. Hardware keyboard/controller input remains available.

## Module boundaries

| Layer | Files | Responsibility |
| --- | --- | --- |
| State model | `src/touch_shortcuts.{h,cpp}` | Contextual recent-key history, paging, geometry and press cancellation; no SDL dependency |
| CDDA bridge | `src/touch_shortcuts_context.{h,cpp}` | Registered actions, manual hotkeys, key types/modifiers and text-entry contexts |
| Platform adapter | `src/sdl_touch_shortcuts.{h,cpp}` | SDL2 drawing, logical/pixel coordinates, pointer ownership and ImGui event forwarding |
| Stable build policy | `fork/`, `linux-release.yml` | Pinned upstream source/resources, validation, Linux packaging |

Upstream integration is deliberately limited to these locations:

- `main.cpp`: opt-in CLI flag.
- `sdltiles.cpp`: draw after the game buffer; intercept captured pointers before
  ImGui/map hit testing; return selected input through the normal input path.
- `input_context.{h,cpp}`: expose existing Android metadata to tiles builds,
  register copied contexts, scope activation during input, and remember valid keys.
- `input.cpp`, `debug.cpp`, `uilist.cpp`, `npctalk.cpp`, `mutation_ui.cpp`,
  `bionics_ui.cpp`: expose existing manual-hotkey metadata to desktop.

This is a small source patch, not a binary plugin or a promise of conflict-free
upgrades. Ordinary data/content changes should not affect the model. Changes to
input-context ownership, key representation, ImGui, font drawing, or SDL require
reviewing the corresponding bridge/adapter. A future SDL2→SDL3 stable transition
will specifically require a platform adapter update; the historical prototype
contains a starting point, not a replacement stable base.

## Linux build

Run **Actions → Stable Linux x86_64 Build** on branch **stable**. Only this branch
can build. Leave `publish` unchecked for a downloadable Actions artifact. Checking
it publishes the validated custom build as a prerelease: the upstream base is
stable, but our new touch patch still needs device acceptance.

`fork/stable.json` pins the official tag, commit, Linux resource asset and SHA-256.
Before compilation, CI verifies the official release is neither a prerelease nor
a draft, verifies tag/commit/ancestry, and rejects changes outside the documented
patch surface. It downloads the pinned official stable archive, verifies its
checksum, preserves its game data, tilesets and compiled translations, and replaces
the executable with our build. It never downloads experimental translations or
moving tileset branches. Provenance is recorded in `VERSION.txt`.

The Linux build uses Ubuntu 22.04 and SDL2, following this stable release's
dependency baseline. SDL2 is bundled beside the executable with a relative runtime
path; other system dependencies follow the official package contract. CI checks
resolved libraries and the packaged executable's version/help. Only
`linux-release.yml` is enabled; upstream workflow files are retained but disabled.

## Updating to the next stable release

1. Verify the new tag is an official stable release (including stable point
   releases). Fetch that exact tag; do not use the upstream default branch.
2. Save the current stable branch as a versioned maintenance branch. Start an
   upgrade branch from the new tag and apply the **net fork diff** from the old
   pinned base, preferably keeping feature code and build policy in separate
   commits. `git diff --binary <old-base> stable` produces that patch; applying it
   with `git apply --3way` exposes changed integration points for review.
3. Update `fork/stable.json` from the new official release's exact commit, asset
   and SHA-256. Review SDL/compiler requirements and the integration list above.
   Do not resolve conflicts by copying whole experimental upstream files.
4. Run `python3 fork/check_stable.py`, `bash fork/test-shortcuts.sh`, a full tiles
   and sound build, `actionlint .github/workflows/linux-release.yml`, and
   `shellcheck fork/*.sh`. Inspect the net diff against the new tag: all gameplay
   and resource changes should come from that official stable release.
5. On the upgrade branch, complete desktop preview/input checks. Integrate the
   reviewed result into `stable` without discarding published history, then run
   the Linux workflow with `publish=false`. Verify the resulting package on Deck
   before publishing. CI checks out 64 commits; deepen that history if the pinned
   stable ancestor falls outside the checkout (never bypass the ancestry check).

Acceptance covers flag off/on, menus and nested popups, gameplay actions,
inventory/text entry, custom modified bindings, recent-key ordering, paging,
touch/mouse deduplication, drag-out cancellation, resize/focus loss and HiDPI.
Automated SDL event tests and macOS previews do not count as physical Steam Deck
touch/controller verification.
