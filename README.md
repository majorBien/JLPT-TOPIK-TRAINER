# jp-kor-trainer - JLPT / TOPIK Trainer

A fast, colourful terminal trainer for the **Japanese JLPT (N5–N3)** and the **Korean TOPIK I (A1–A2)** exams.
Exam-style multiple-choice questions, real listening practice with synthesized voices, spaced repetition,
a mistakes notebook and AI explanations - all in your terminal.

<p align="center">
  <img src="images/main-menu-jp.png" width="49%" alt="Main menu – Japanese (JLPT)">
  <img src="images/main-menu-kor.png" width="49%" alt="Main menu – Korean (TOPIK)">
</p>

## Features

- **Two languages, one app** - switch between Japanese (JLPT) and Korean (TOPIK I) in *Settings*.
- **Exam-format questions** - every closed question type of the real tests: vocabulary, grammar, sentence order (★), reading, paraphrase/usage.
- **Listening** - dialogues are spoken by male/female voices, exactly like the exam parts (課題理解, 概要理解, 即時応答 / 듣기 1–30).
- **Study tools after every answer** - `w` explain why (even when you guessed right), `t` translate, `f` furigana (Japanese).
- **SRS vocabulary trainer**, **My Mistakes** notebook and a **Progress** screen.

| Level | Words | Grammar & reading | Listening |
|---|---|---|---|
| JLPT N5 | 498 | 140 | 100 |
| JLPT N4 | 546 | 175 | 101 |
| JLPT N3 | 547 | 152 | 100 |
| TOPIK I · 1급 (A1) | 589 | 152 | 100 |
| TOPIK I · 2급 (A2) | 554 | 153 | 100 |

## Download

Grab the archive for your system from the [Releases](../../releases) page:

| System | File |
|---|---|
| Linux x86_64 (any distro, static) | `jp-kor-trainer-1.0-linux-x86_64.tar.gz` |
| Linux ARM64 (Raspberry Pi 4/5, …) | `jp-kor-trainer-1.0-linux-arm64.tar.gz` |
| macOS Apple Silicon (M1–M4) | `jp-kor-trainer-1.0-macos-arm64.tar.gz` |
| macOS Intel | `jp-kor-trainer-1.0-macos-x86_64.tar.gz` |
| Windows 10/11 x64 | `jp-kor-trainer-1.0-windows-x86_64.zip` |

## Quick start

**Linux / macOS**

```bash
tar xzf jp-kor-trainer-1.0-<system>.tar.gz
cd jp-kor-trainer-1.0-<system>
./jp-kor-trainer                 # Japanese, N4
./jp-kor-trainer --lang ko       # Korean, TOPIK I A1
./jp-kor-trainer --level N5      # pick a level
```

> **macOS:** the binaries are not notarized. If macOS blocks them, run
> `xattr -dr com.apple.quarantine .` once inside the folder. This also unblocks the bundled `open_jtalk/open_jtalk`.

**Windows** — unzip, open **Windows Terminal** in the folder and run:

```powershell
.\jp-kor-trainer.exe
.\jp-kor-trainer.exe --lang ko
```

Use Windows Terminal (default on Windows 11) for colours and Japanese/Korean characters.

That's it — vocabulary, grammar, reading, SRS, progress, furigana and the Japanese listening voice work
out of the box. No libraries to install. Your progress is saved in `userdata/` next to the binary.

### Optional: Korean voices for listening

Japanese needs nothing: Open JTalk is bundled (see below).

| | Korean |
|---|---|
| **Linux** | `sudo apt install ffmpeg python3-venv`<br>`./scripts/setup_tts_ko.sh` |
| **macOS** | built in — voice *Yuna* |
| **Windows** | built in — add *Korean* under Settings › Time & language › Speech |

Better voices everywhere (optional): `pip install edge-tts` plus `ffmpeg` on the PATH gives natural
Microsoft neural voices (`brew install ffmpeg` / `winget install ffmpeg`). They need internet the first time
a recording is generated; audio is cached afterwards. On Linux an audio player is needed too
(`pipewire-bin`, `pulseaudio-utils` or `alsa-utils`); macOS and Windows play audio natively.

### Furigana (`f`) and the Japanese voice

Both come from [Open JTalk](https://open-jtalk.sourceforge.net/), which ships with every release archive in the
`open_jtalk/` folder (program, UTF-8 `naist-jdic` dictionary and the *nitech* voice). Nothing to install:
answer a question, press `f`, and kanji get their readings, e.g. `日本語(にほんご)を勉強(べんきょう)しています`.

**Using your own Open JTalk instead:** fill in the `tts` block of `settings.json`. Any path you set wins over
the bundled copy; an empty value means "auto-detect":

```json
"tts": {
  "open_jtalk_bin": "/usr/bin/open_jtalk",
  "dictionary":     "/var/lib/mecab/dic/open-jtalk/naist-jdic",
  "voice_male":     "/usr/share/hts-voice/nitech-jp-atr503-m001/nitech_jp_atr503_m001.htsvoice",
  "voice_female":   ""
}
```

The dictionary must be the UTF-8 build. On Windows write paths with forward slashes (`C:/open_jtalk/dic`).
If *f* says *"Furigana needs Open JTalk: …"* or *"Could not analyse the sentence"* after you changed these,
clear all three paths so the bundled copy is used again.

### Optional: AI explanations (`w`) and translations (`t`)

Uses the [DeepSeek API](https://platform.deepseek.com). Provide a key in one of two ways:

```bash
export DEEPSEEK_API_KEY=sk-...                          # environment variable
# or
mkdir -p ~/.config/jp-kor-trainer && echo 'sk-...' > ~/.config/jp-kor-trainer/deepseek.key
```

On Windows `~` is your user folder (`%USERPROFILE%`). `curl` is built into Windows 10/11 and macOS.

Or point the app to any key file in *Settings → 3 DeepSeek API key file*.
The explanation language (Polish, English, Japanese, Korean, …) is chosen in *Settings* too.

## How it looks

<p align="center">
  <img src="images/example-session-jp-2.png" width="49%" alt="Grammar question with translation and furigana">
  <img src="images/example-session-kor-1.png" width="49%" alt="TOPIK reading question with an AI explanation">
</p>
<p align="center">
  <img src="images/example-session-jp-1.png" width="49%" alt="JLPT reading passage">
  <img src="images/example-session-kor-2.png" width="49%" alt="TOPIK listening question">
</p>

## Controls

| Where | Keys |
|---|---|
| Main menu | `1`–`6` modules · `7` level · `8` settings · `9` exit |
| Question | `A`–`D` (or `1`–`4`) answer · `r` replay audio · `q` quit |
| After answering | `Enter` next · `w` why · `t` translate · `f` furigana · `q` quit |
| SRS | `h` / `g` / `e` hard · good · easy |

## Configuration

`settings.json` (next to the binary): default language and levels, session size, SRS limits,
DeepSeek key file and explanation language, voice options (`tts`, `tts_ko`).
Environment: `DEEPSEEK_API_KEY`, `JP_KOR_TRAINER_HOME` (data root), `NO_COLOR`.

## Build from source

```bash
sudo apt install build-essential cmake
cmake -S . -B build && cmake --build build -j
./build/jp-kor-trainer

./scripts/make_release.sh      # release archives for all systems → release/ (cross-compiles with Zig)
```

`make_release.sh` also cross-compiles Open JTalk into each archive via `scripts/build_open_jtalk.sh`
(sources are downloaded once from SourceForge and checked by SHA-256). For a local source build on Linux,
`./scripts/setup_tts.sh` provides Open JTalk instead.

Question banks live in `data/<ja|ko>/levels/<LEVEL>/` as plain JSON - add a level by adding a folder.
All questions, texts and dialogues are original, written for this app in the official exam formats.
