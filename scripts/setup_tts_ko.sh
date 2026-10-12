#!/usr/bin/env bash
# Korean TTS for the 듣기 (listening) module:
#   - edge-tts (Microsoft neural voices, needs internet when generating audio)
#     installed into a local venv: third_party/edge-tts/venv
#   - espeak-ng as an offline fallback
#       with --system: via apt (needs sudo)
#       otherwise:     the Ubuntu/Debian .debs unpacked into third_party/espeak-ng/
# The app auto-detects both. ffmpeg is required (sudo apt install ffmpeg).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "== edge-tts =="
python3 -m venv "$ROOT/third_party/edge-tts/venv"
"$ROOT/third_party/edge-tts/venv/bin/pip" install --quiet --upgrade edge-tts
echo "edge-tts installed: $ROOT/third_party/edge-tts/venv/bin/edge-tts"

echo "== espeak-ng (fallback) =="
PKGS=(espeak-ng espeak-ng-data libespeak-ng1 libpcaudio0 libsonic0)
if [[ "${1:-}" == "--system" ]]; then
  sudo apt-get install -y "${PKGS[@]}"
  exit 0
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
(cd "$TMP" && apt-get download "${PKGS[@]}")
mkdir -p "$ROOT/third_party/espeak-ng"
for deb in "$TMP"/*.deb; do dpkg -x "$deb" "$ROOT/third_party/espeak-ng"; done
echo "espeak-ng unpacked into $ROOT/third_party/espeak-ng"
