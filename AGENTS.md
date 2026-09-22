# AGENTS.md

## Project

YiGo (弈境): cross-platform Go (Weiqi) AI analysis & review tool, pure Qt C++, no Python deps. Targets UOS V20 / Kylin (Chinese Linux distros)/Windows/Mac OS. Inspired by Katrain.

## Build & test (exact commands)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # configure (once)
cmake --build build                                  # build
ctest --test-dir build --output-on-failure          # run all tests
ctest --test-dir build -R tst_board --output-on-failure   # run a single test (by name)
```

Toolchain: g++ 8.3, CMake ≥3.16, Ninja (all UOS V20 defaults). Tests use QtTest via ctest.

## Critical constraints

- **Qt 5.11 API ceiling**: only use APIs that exist in Qt 5.11.3. No Qt 5.12+/Qt6-only APIs. Reason: UOS V20 ships Qt 5.11.3 and Qt6 official binaries don't run on its glibc 2.28. When an API is too new, hand-write the equivalent.
- **Qt6 migration-friendly**: containers use `QVector` (not `QList`); encapsulate version-specific API differences (e.g. `QString::split` enum names) in utility functions for easy conditional-compile later.
- **Linux uses system Qt, not bundled**: build with `apt install qtbase5-dev`; runtime depends on system `libqt5core5a`. No vcpkg on Linux. (vcpkg is gitignored for Windows/macOS.)
- **`src/core/` is QtCore-only**: no `#include` of any QtGui/QtWidgets header. This keeps the domain layer unit-testable without a GUI. Tests in `tests/` use QtTest and never start a GUI.
- **Zero third-party deps**: SGF parsing, GTP protocol, and charts are all self-implemented. Do not add external libraries.

## Architecture (4 layers)

```
ui/ (Widgets) → app/ (Controllers) → core/ (domain, QtCore only) → gtp/ (engine via QProcess)
```

- UI panels never call each other directly; always go through controllers (`GameController`, `ReviewController`).
- GTP engine runs in a separate `QProcess` (async, crash-isolated); engine crash must never take down the UI.
- `core/` owns all `MoveNode`s via `GameTree`; external code holds raw pointers for read-only access only.

## Domain conventions

- **Winrate is always Black's perspective** (0.0–1.0); UI converts to the side-to-move view for display.
- Board coords: `x` = column (left→right, 0-indexed), `y` = row (top→bottom, 0-indexed). Internal grid is row-major: `idx = y * size + x`.
- SGF coords: `'a'..'z'` → 0..25; `tt` or empty value = pass (only for board size ≤19). Column letter skips `I` in display (A–T, no I).
- Board sizes: 9 / 13 / 19 (`Board::MaxSize = 25`).
- Zobrist hashing uses a fixed seed (`0x1BADB002`) for cross-process consistency.

## Workflow (M1)

- **TDD per task**: write failing test → verify it fails → minimal implementation → verify pass → commit. The M1 plan defines 7 tasks each ending in a commit.
- Commit style: conventional, short — `feat(core): ...`.
- Each milestone deliverable must be runnable and testable on its own.
- 每个任务完成后，等我确认，再进行下一个任务。
