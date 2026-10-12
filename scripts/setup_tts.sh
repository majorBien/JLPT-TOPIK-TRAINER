#!/usr/bin/env bash
# Installs Open JTalk (open-source Japanese TTS) for the Listening module.
#   - with sudo:    system-wide via apt (recommended)
#   - without sudo: downloads the Ubuntu/Debian .debs and unpacks them into
#                   third_party/open_jtalk/ (the app auto-detects that folder)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKGS=(open-jtalk open-jtalk-mecab-naist-jdic hts-voice-nitech-jp-atr503-m001 libhtsengine1)

if [[ "${1:-}" == "--system" ]]; then
  sudo apt-get install -y "${PKGS[@]}"
  exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
(cd "$TMP" && apt-get download "${PKGS[@]}")
mkdir -p "$ROOT/third_party/open_jtalk"
for deb in "$TMP"/*.deb; do dpkg -x "$deb" "$ROOT/third_party/open_jtalk"; done
echo "Open JTalk unpacked into $ROOT/third_party/open_jtalk"
