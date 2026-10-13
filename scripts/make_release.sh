#!/usr/bin/env bash
# Builds a self-contained Linux x86_64 release in release/:
#   release/jp-kor-trainer-<version>-linux-x86_64/       (ready to run)
#   release/jp-kor-trainer-<version>-linux-x86_64.tar.gz
# The binary is linked statically, so it runs on Debian 11/12/13 and other distros
# regardless of their glibc / libstdc++ versions.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^project(jp_kor_trainer VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
NAME="jp-kor-trainer-${VERSION}-linux-$(uname -m)"
OUT="$ROOT/release/$NAME"

cmake -S "$ROOT" -B "$ROOT/build-release" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static" >/dev/null
cmake --build "$ROOT/build-release" -j"$(nproc)"

rm -rf "$OUT" "$OUT.tar.gz"
mkdir -p "$OUT/scripts"
install -m 755 -s "$ROOT/build-release/jp-kor-trainer" "$OUT/jp-kor-trainer"
cp -r "$ROOT/data" "$ROOT/images" "$ROOT/README.md" "$OUT/"
install -m 755 "$ROOT/scripts/setup_tts.sh" "$ROOT/scripts/setup_tts_ko.sh" "$OUT/scripts/"

# Fresh settings (no personal paths); the API key comes from $DEEPSEEK_API_KEY or the key file.
cat > "$OUT/settings.json" <<'JSON'
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

tar -C "$ROOT/release" -czf "$OUT.tar.gz" "$NAME"
echo "Release: $OUT"
echo "Archive: $OUT.tar.gz ($(du -h "$OUT.tar.gz" | cut -f1))"
