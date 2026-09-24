# YiGo (弈境)

English | [简体中文](README_zh-CN.md)

## Introduction

**YiGo** is a cross-platform Go (Weiqi/Baduk) AI analysis and review software built entirely with native Qt C++. Inspired by Katrain, it abandons Python dependencies for faster startup, lower memory usage and more consistent cross-platform experience. Designed for Go players, learners and professionals, providing stable, lightweight and high-performance AI analysis and game record management.

**Current status (v0.5.0)**: M1–M6 milestones complete — playable human-vs-engine games, live analysis overlay, batch review with winrate chart and blunder detection, SGF round-trip, settings persistence, and a UOS debian package.

## Core Features

- **Pure Qt Native**: Built with C++17 / Qt 5.11, lightweight and script-free
- **Human vs Engine Play**: Choose board size (9/13/19), komi, handicap and sides (human/AI); engine moves via GTP `genmove`; two-pass game end with local scoring
- **AI Engine Integration**: Native GTP protocol support for KataGo / Leela Zero, with live candidate-move overlay and real-time winrate in the status bar
- **Professional Review**: One-click batch analysis of the main line, winrate chart with colored segments (blue = black gains / red = black loses), blunder markers (default 5% threshold), click chart to jump to any move
- **Full SGF Support**: Open, save, variations, handicap stones (AB/AW), compressed point ranges, multi-encoding (UTF-8 / GB18030 detection), round-trip safe
- **Engine Crash Isolation**: Engine runs in a separate QProcess; a crash never takes down the app — offline play and review continue
- **Settings Persistence**: Engine path/args, last game setup and window geometry restored across restarts
- **High DPI Aware**: `AA_EnableHighDpiScaling`, sharp rendering on high-resolution displays
- **Handicap & Komi**: Standard star-point handicap placement (9/13/19 boards), Chinese/Japanese scoring, two-pass endgame with local scoring

## Tech Stack

- **Framework**: Qt 5.11 (Core / Widgets / Gui — system Qt on Linux)
- **Language**: C++17
- **Build System**: CMake ≥ 3.16 + Ninja
- **AI Protocol**: GTP (Go Text Protocol)
- **Supported Engines**: KataGo (`kata-analyze` / `genmove`), Leela Zero (`lz-analyze`)
- **Tests**: QtTest, 16 suites, fully offline (fake GTP engine; no real engine needed)

## Build from Source

```bash
# dependencies (UOS V20 / Debian-based)
sudo apt install cmake ninja-build qtbase5-dev g++

# configure + build + test
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

# run
./build/src/ui/yigo
```

### Debian package (UOS V20 / deepin)

```bash
cmake --build build --target deb
sudo dpkg -i build/yigo_0.5.0_amd64.deb   # installs /usr/bin/yigo + desktop entry
```

## Using an Engine

YiGo does not bundle an engine. Install [KataGo](https://github.com/lightvector/KataGo) or Leela Zero yourself, then in the app:

1. **Engine panel (right dock)**: pick engine type, set the executable path and arguments, press **Start**
2. Play against it (**Game → New…**, choose sides/handicap/komi), or open an SGF and press **Game → Analyze Game (Ctrl+R)** for batch review
3. Recommended KataGo launch: `katago gtp -model <weights>.bin.gz -config gtp.cfg` (set path + args in the Engine panel; the model is user-supplied)

## Use Cases

- Human-vs-engine play with handicap & komi; two-pass scoring
- Game review: batch analysis, winrate chart, blunder detection (default 5% threshold)
- SGF management, teaching demonstration & variation research

## License

This project is released under an open source license. Feel free to Star, Fork, submit PRs and Issues.
