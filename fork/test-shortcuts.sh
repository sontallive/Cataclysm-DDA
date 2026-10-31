#!/usr/bin/env bash
set -euo pipefail
compiler="${CXX:-clang++}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
common=(-std=c++17 -O1 -g -Wno-unknown-warning-option '-fsanitize=address,undefined'
        -ffunction-sections -fdata-sections -Isrc -Itests -isystem src/third-party)
read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf freetype2)"
read -r -a sdl_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
if [[ "$(uname -s)" == Darwin ]]; then
  discard=('-Wl,-dead_strip')
else
  discard=('-Wl,--gc-sections')
fi

"$compiler" "${common[@]}" -DCATCH_CONFIG_MAIN \
  tests/touch_shortcuts_test.cpp src/touch_shortcuts.cpp -o "$work/model"
"$work/model" '[touch_shortcuts]'

"$compiler" "${common[@]}" -DTILES "${sdl_cflags[@]}" \
  fork/tests/sdl_input_test.cpp src/touch_shortcuts.cpp src/sdl_wrappers.cpp \
  "${sdl_libs[@]}" "${discard[@]}" -o "$work/sdl-input"
"$work/sdl-input"

"$compiler" "${common[@]}" -DTILES "${sdl_cflags[@]}" \
  fork/tests/context_test.cpp src/input_context.cpp src/input.cpp src/string_id.cpp \
  src/translation.cpp src/touch_shortcuts_context.cpp \
  "${sdl_libs[@]}" "${discard[@]}" -o "$work/context"
"$work/context"
