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
  # Homebrew's SDL2 compatibility library dlopens SDL3. Set this inside the
  # script: macOS strips an inherited DYLD_* variable when starting /bin/bash.
  if pkg-config --exists sdl3; then
    sdl3_library_dir="$(pkg-config --variable=libdir sdl3)"
    export DYLD_LIBRARY_PATH="$sdl3_library_dir${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
  fi
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

# This small executable links the real context implementation without the
# game's ImGui screens. Clang 14's ELF ASan global metadata retains otherwise
# unused UI vtables; exclude global/vptr instrumentation for this probe only.
# Heap/stack ASan and other UBSan checks remain enabled, and the game is also
# compiled/linked in full. The model and SDL probes retain full instrumentation.
"$compiler" "${common[@]}" -mllvm -asan-globals=0 -fno-sanitize=vptr \
  -DTILES "${sdl_cflags[@]}" \
  fork/tests/context_test.cpp src/input_context.cpp src/input.cpp src/string_id.cpp \
  src/translation.cpp src/touch_shortcuts_context.cpp \
  "${sdl_libs[@]}" "${discard[@]}" -o "$work/context"
"$work/context"
