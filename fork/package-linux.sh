#!/usr/bin/env bash
set -euo pipefail

# Resources, translations and tilesets come from the SAME official stable build.
readarray -t pin < <(python3 -c 'import json; p=json.load(open("fork/stable.json")); print(p["tag"]); print(p["asset"]); print(p["sha256"])')
tag="${pin[0]}"
asset="${pin[1]}"
checksum="${pin[2]}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
curl --fail --location --retry 3 --output "$work/official.tar.gz" \
  "https://github.com/CleverRaven/Cataclysm-DDA/releases/download/$tag/$asset"
printf '%s  %s\n' "$checksum" "$work/official.tar.gz" | sha256sum --check
tar -xzf "$work/official.tar.gz" -C "$work"
mapfile -t executables < <(find "$work" -type f -name cataclysm-tiles)
test "${#executables[@]}" -eq 1
package="$(dirname "${executables[0]}")"
test -d "$package/gfx"
test -d "$package/data/json"
test -s "$package/lang/mo/zh_CN/LC_MESSAGES/cataclysm-dda.mo"

# Build with localization support; preserve the official compiled MO catalogs.
# Generate version.h first to avoid make's parallel version/object ordering race.
make COMPILER=clang++-14 TILES=1 SOUND=1 LOCALIZE=1 RELEASE=1 version
make COMPILER=clang++-14 -j"$(nproc)" CCACHE=1 TILES=1 SOUND=1 LOCALIZE=1 \
  RELEASE=1 BACKTRACE=1 PCH=0 cataclysm-tiles
cp cataclysm-tiles "$package/cataclysm-tiles"
# The official stable package bundles SDL2 beside its executable. Keep that
# layout while shipping the matching build runtime; other system dependencies
# follow upstream's Ubuntu 22.04 Linux package contract.
sdl_library="$(ldd cataclysm-tiles | awk '/libSDL2-2\.0.so.0 =>/ {print $3}')"
test -f "$sdl_library"
cp "$sdl_library" "$package/libSDL2-2.0.so.0"
patchelf --set-rpath "\$ORIGIN" "$package/cataclysm-tiles"
cp /usr/share/doc/libsdl2-2.0-0/copyright "$package/LICENSE-SDL2-Ubuntu.txt"
cp FORK.md fork/stable.json "$package/"

build_tag="catazoo-$tag-linux-${GITHUB_RUN_NUMBER:-local}-${GITHUB_RUN_ATTEMPT:-1}"
{
  echo "build type: stable Linux x86_64 with graphics and sounds"
  echo "build number: $build_tag"
  echo "upstream stable: $tag"
  echo "upstream commit: $(python3 -c 'import json; print(json.load(open("fork/stable.json"))["commit"])')"
  echo "fork commit: $(git rev-parse HEAD)"
  echo "resource asset: $asset"
  echo "resource sha256: $checksum"
  echo 'touch shortcuts: --touch-shortcuts (off by default)'
} > "$package/VERSION.txt"

env -u LD_LIBRARY_PATH ldd "$package/cataclysm-tiles" | tee "$work/deps.txt"
if grep -q 'not found' "$work/deps.txt"; then
  echo 'Packaged game has missing runtime dependencies' >&2
  exit 1
fi
grep -F "libSDL2-2.0.so.0 => $package/" "$work/deps.txt"
env -u LD_LIBRARY_PATH "$package/cataclysm-tiles" --version
env -u LD_LIBRARY_PATH "$package/cataclysm-tiles" --help | grep -- '--touch-shortcuts'
mkdir -p dist
tar -czf "dist/cdda-$tag-catazoo-linux-x86_64.tar.gz" -C "$(dirname "$package")" "$(basename "$package")"
cp "$package/VERSION.txt" dist/VERSION.txt
(cd dist && sha256sum ./*.tar.gz > SHA256SUMS)
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  printf 'tag=%s\n' "$build_tag" >> "$GITHUB_OUTPUT"
fi
ccache --show-stats
