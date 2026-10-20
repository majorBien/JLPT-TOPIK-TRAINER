#!/usr/bin/env bash
# Cross-compiles Open JTalk 1.11 (+ hts_engine API 1.10) with Zig and lays out a
# self-contained bundle the app picks up automatically:
#
#   <out>/open_jtalk[.exe]
#   <out>/dic/                               naist-jdic, UTF-8
#   <out>/voice/nitech_jp_atr503_m001.htsvoice
#   <out>/LICENSES/
#
# Usage: scripts/build_open_jtalk.sh <platform> <out dir>
#   platform: linux-x86_64 | linux-arm64 | macos-arm64 | macos-x86_64 | windows-x86_64
# Needs Zig 0.13 (on $PATH or ZIG=/path/to/zig), curl and tar. Sources are
# downloaded once into build-release/open_jtalk-src/ and checked against SHA-256.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIG="${ZIG:-$(command -v zig || true)}"
PLATFORM="${1:?platform}"
OUT="${2:?out dir}"
SRC="$ROOT/build-release/open_jtalk-src"
[[ -n "$ZIG" ]] || { echo "build_open_jtalk.sh needs Zig (set ZIG=/path/to/zig)" >&2; exit 1; }

case "$PLATFORM" in
  linux-x86_64)   TARGET=x86_64-linux-musl ;;
  linux-arm64)    TARGET=aarch64-linux-musl ;;
  macos-arm64)    TARGET=aarch64-macos ;;
  macos-x86_64)   TARGET=x86_64-macos ;;
  windows-x86_64) TARGET=x86_64-windows-gnu ;;
  *) echo "unknown platform $PLATFORM" >&2; exit 1 ;;
esac

# ---- sources ------------------------------------------------------------------
SF=https://downloads.sourceforge.net
SOURCES=(
  "hts-engine/hts_engine_API-1.10.tar.gz e2132be5860d8fb4a460be766454cfd7c3e21cf67b509c48e1804feab14968f7"
  "open-jtalk/open_jtalk-1.11.tar.gz 20fdc6aeb6c757866034abc175820573db43e4284707c866fcd02c8ec18de71f"
  "open-jtalk/open_jtalk_dic_utf_8-1.11.tar.gz 33e9cd251bc41aa2bd7ca36f57abbf61eae3543ca25ca892ae345e394cb10549"
  "open-jtalk/hts_voice_nitech_jp_atr503_m001-1.05.tar.gz 2e555c88482267b2931c7dbc7ecc0e3df140d6f68fc913aa4822f336c9e0adfc"
)
mkdir -p "$SRC"
for entry in "${SOURCES[@]}"; do
  read -r path sum <<<"$entry"
  file="$SRC/$(basename "$path")"
  dir="${file%.tar.gz}"
  [[ -d "$dir" ]] && continue
  [[ -f "$file" ]] || curl -fsSL -o "$file" "$SF/$path"
  echo "$sum  $file" | sha256sum -c --quiet - || { rm -f "$file"; exit 1; }
  tar -C "$SRC" -xzf "$file"
done
HTS="$SRC/hts_engine_API-1.10"
OJT="$SRC/open_jtalk-1.11"
# HTS_ftell reads glibc's private fpos_t.__pos, which musl doesn't have.
sed -i 's/return (size_t) pos\.__pos;/return (size_t) ftell((FILE *) fp->pointer);/' "$HTS/lib/HTS_misc.c"

# ---- compile ------------------------------------------------------------------
OBJ="$ROOT/build-release/open_jtalk-obj/$PLATFORM"
rm -rf "$OBJ" && mkdir -p "$OBJ"

# What configure --with-charset=UTF-8 would define; mecab/config.h is replaced by -D flags.
DEFS=(-DCHARSET_UTF_8 -DMECAB_USE_UTF8_ONLY -DMECAB_DEFAULT_CHARSET='"UTF-8"' -DMECAB_DEFAULT_RC='"dummy"'
      -DMECAB_WITHOUT_MUTEX_LOCK -DDIC_VERSION=102 -DPACKAGE='"open_jtalk"' -DVERSION='"1.11"'
      -DAUDIO_PLAY_NONE -DHAVE_STDINT_H -DHAVE_STRING_H -DHAVE_STDLIB_H -DHAVE_GETENV
      -DHAVE_SYS_TYPES_H -DHAVE_SYS_STAT_H -DHAVE_FCNTL_H)
if [[ "$PLATFORM" == windows-* ]]; then
  DEFS+=(-DHAVE_WINDOWS_H -DHAVE_IO_H)
else
  DEFS+=(-DHAVE_UNISTD_H -DHAVE_SYS_MMAN_H -DHAVE_MMAP -DHAVE_DIRENT_H)
fi
INCS=(-I"$HTS/include")
for d in text2mecab mecab/src mecab2njd njd njd_set_pronunciation njd_set_digit njd_set_accent_phrase \
         njd_set_accent_type njd_set_unvoiced_vowel njd_set_long_vowel njd2jpcommon jpcommon; do
  INCS+=(-I"$OJT/$d")
done
# 2015-era code: keep it on the language standards it was written for, quietly.
CFLAGS=(-target "$TARGET" -O2 -std=gnu99 -w "${DEFS[@]}" "${INCS[@]}")
CXXFLAGS=(-target "$TARGET" -O2 -std=gnu++03 -w "${DEFS[@]}" "${INCS[@]}")

C_SOURCES=("$HTS"/lib/*.c "$OJT"/bin/open_jtalk.c)
for d in text2mecab mecab2njd njd njd_set_pronunciation njd_set_digit njd_set_accent_phrase njd_set_accent_type \
         njd_set_unvoiced_vowel njd_set_long_vowel njd2jpcommon jpcommon; do
  C_SOURCES+=("$OJT/$d"/*.c)
done
CXX_SOURCES=()
for f in "$OJT"/mecab/src/*.cpp; do
  [[ "$(basename "$f")" == mecab-dict-index.cpp ]] || CXX_SOURCES+=("$f")
done

objs=()
for f in "${C_SOURCES[@]}" "${CXX_SOURCES[@]}"; do
  o="$OBJ/$(basename "$(dirname "$f")")_$(basename "${f%.*}").o"
  objs+=("$o")
  if [[ "$f" == *.cpp ]]; then "$ZIG" c++ "${CXXFLAGS[@]}" -c "$f" -o "$o" &
  else "$ZIG" cc "${CFLAGS[@]}" -c "$f" -o "$o" &
  fi
  while (( $(jobs -rp | wc -l) >= $(nproc) )); do wait -n; done
done
wait

exe="open_jtalk"; [[ "$PLATFORM" == windows-* ]] && exe="open_jtalk.exe"
link=(); [[ "$PLATFORM" == linux-* ]] && link=(-static)
mkdir -p "$OUT"
"$ZIG" c++ -target "$TARGET" -s "${objs[@]}" "${link[@]}" -o "$OUT/$exe"
rm -f "$OUT"/*.lib "$OUT"/*.pdb  # Zig's Windows import-library leftovers

# ---- data ---------------------------------------------------------------------
rm -rf "$OUT/dic" "$OUT/voice" "$OUT/LICENSES"
cp -r "$SRC/open_jtalk_dic_utf_8-1.11" "$OUT/dic"
mkdir -p "$OUT/voice" "$OUT/LICENSES"
cp "$SRC/hts_voice_nitech_jp_atr503_m001-1.05/nitech_jp_atr503_m001.htsvoice" "$OUT/voice/"
cp "$OJT/COPYING" "$OUT/LICENSES/open_jtalk.txt"
cp "$OJT/mecab/COPYING" "$OUT/LICENSES/mecab.txt"
cp "$OJT/mecab-naist-jdic/COPYING" "$OUT/LICENSES/naist-jdic.txt"
cp "$HTS/COPYING" "$OUT/LICENSES/hts_engine_API.txt"
cp "$SRC/hts_voice_nitech_jp_atr503_m001-1.05/COPYING" "$OUT/LICENSES/nitech_jp_atr503_m001.txt"
echo "   open_jtalk → $OUT"
