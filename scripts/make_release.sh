#!/usr/bin/env bash
# Builds release archives in release/, one per platform, each with the binary,
# data/, images/, README.md, settings.json, a ready-to-use Open JTalk in open_jtalk/
# (furigana + Japanese voice, see scripts/build_open_jtalk.sh) and the voice setup scripts:
#
#   jp-kor-trainer-<ver>-linux-x86_64.tar.gz     static (musl)
#   jp-kor-trainer-<ver>-linux-arm64.tar.gz      static (musl)
#   jp-kor-trainer-<ver>-macos-arm64.tar.gz      Apple Silicon
#   jp-kor-trainer-<ver>-macos-x86_64.tar.gz     Intel Mac
#   jp-kor-trainer-<ver>-windows-x86_64.zip
#
# Cross-compiling needs Zig (https://ziglang.org, 0.13): put it on $PATH or set ZIG=/path/to/zig.
# Without Zig only the native Linux build is made (static, via CMake), without Open JTalk.
# Usage: scripts/make_release.sh [target ...]   e.g.  scripts/make_release.sh windows-x86_64
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^project(jp_kor_trainer VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
ZIG="${ZIG:-$(command -v zig || true)}"
ALL=(linux-x86_64 linux-arm64 macos-arm64 macos-x86_64 windows-x86_64)
TARGETS=("${@:-${ALL[@]}}")
BUILD="$ROOT/build-release"
mkdir -p "$ROOT/release" "$BUILD"

zig_target() {
  case "$1" in
    linux-x86_64)   echo x86_64-linux-musl ;;
    linux-arm64)    echo aarch64-linux-musl ;;
    macos-arm64)    echo aarch64-macos ;;
    macos-x86_64)   echo x86_64-macos ;;
    windows-x86_64) echo x86_64-windows-gnu ;;
    *) echo "unknown target $1" >&2; exit 1 ;;
  esac
}

build_binary() {  # $1 = platform, $2 = output file
  local platform="$1" out="$2"
  if [[ -n "$ZIG" ]]; then
    local extra=()
    case "$platform" in
      linux-*)   extra=(-static) ;;
      windows-*) extra=(-lwinmm) ;;
    esac
    "$ZIG" c++ -std=c++17 -O2 -s -target "$(zig_target "$platform")" -I"$ROOT/include" \
      -DTRAINER_SOURCE_DIR='"."' "$ROOT"/src/*.cpp "$ROOT"/src/modules/*.cpp "${extra[@]}" -o "$out"
  elif [[ "$platform" == "linux-$(uname -m)" ]]; then
    cmake -S "$ROOT" -B "$BUILD/cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static" >/dev/null
    cmake --build "$BUILD/cmake" -j"$(nproc)" >/dev/null
    install -m 755 -s "$BUILD/cmake/jp-kor-trainer" "$out"
  else
    echo "skipping $platform: cross-compiling needs Zig (set ZIG=/path/to/zig)" >&2
    return 1
  fi
}

write_settings() {
  cat > "$1" <<'JSON'
{
  "language": "ja",
  "default_level": "N4",
  "default_level_ko": "A1",
  "session_size": 5,
  "srs_new_per_session": 5,
  "srs_max_reviews": 20,
  "deepseek": {
    "key_file": "~/.config/jp-kor-trainer/deepseek.key",
    "model": "deepseek-chat",
    "explanation_language": "English",
    "timeout_seconds": 60
  },
  "tts": {
    "open_jtalk_bin": "",
    "dictionary": "",
    "voice_male": "",
    "voice_female": "",
    "speed": 0.95,
    "player": ""
  }
}
JSON
}

for platform in "${TARGETS[@]}"; do
  name="jp-kor-trainer-${VERSION}-${platform}"
  out="$ROOT/release/$name"
  exe="jp-kor-trainer"; [[ "$platform" == windows-* ]] && exe="jp-kor-trainer.exe"
  echo "== $platform"
  rm -rf "$out" "$out.tar.gz" "$out.zip"
  mkdir -p "$out"
  if ! build_binary "$platform" "$out/$exe"; then rm -rf "$out"; continue; fi
  chmod 755 "$out/$exe"
  rm -f "$out"/*.lib "$out"/*.pdb  # Zig's Windows import-library leftovers
  cp -r "$ROOT/data" "$ROOT/images" "$ROOT/README.md" "$out/"
  write_settings "$out/settings.json"
  if [[ -n "$ZIG" ]]; then
    ZIG="$ZIG" "$ROOT/scripts/build_open_jtalk.sh" "$platform" "$out/open_jtalk"
  fi
  if [[ "$platform" == linux-* ]]; then
    mkdir -p "$out/scripts"
    install -m 755 "$ROOT/scripts/setup_tts.sh" "$ROOT/scripts/setup_tts_ko.sh" "$out/scripts/"
  fi
  if [[ "$platform" == windows-* ]]; then
    (cd "$ROOT/release" && rm -f "$name.zip" && python3 - "$name" <<'PY'
import os, sys, zipfile
name = sys.argv[1]
with zipfile.ZipFile(name + ".zip", "w", zipfile.ZIP_DEFLATED) as z:
    for base, _, files in os.walk(name):
        for f in files:
            z.write(os.path.join(base, f))
PY
    )
    echo "   release/$name.zip ($(du -h "$out.zip" | cut -f1))"
  else
    tar -C "$ROOT/release" -czf "$out.tar.gz" "$name"
    echo "   release/$name.tar.gz ($(du -h "$out.tar.gz" | cut -f1))"
  fi
done
