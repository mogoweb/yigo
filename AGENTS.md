# AGENTS.md

## Project

YiGo (弈境): cross-platform Go (Weiqi) AI analysis & review tool, pure Qt C++, no Python deps. Targets UOS V20 / Kylin (Chinese Linux distros)/Windows/Mac OS. Inspired by Katrain. **All six spec milestones (M1–M6) are delivered**: core domain, minimal UI, engine integration, human-vs-engine play, review analysis, settings + UOS deb packaging.

## Build & test (exact commands)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # configure (once)
cmake --build build                                  # build
ctest --test-dir build --output-on-failure          # run all tests (~2s, 16 suites)
ctest --test-dir build -R tst_board --output-on-failure   # run a single test (by name)
cmake --build build --target deb                     # build yigo_0.5.0_amd64.deb
```

Toolchain: g++ 8.3, CMake ≥3.16, Ninja (all UOS V20 defaults). Tests use QtTest via ctest; GUI probes (tests/gui, offscreen platform) need Qt5::Widgets.

## Critical constraints

- **Qt 5.11 API ceiling**: only use APIs that exist in Qt 5.11.3. No Qt 5.12+/Qt6-only APIs. Reason: UOS V20 ships Qt 5.11.3 and Qt6 official binaries don't run on its glibc 2.28. When an API is too new, hand-write the equivalent. **Known traps already hit once**: `QVector::swapItemsAt` (5.13+), `qHash(QPoint)` (absent in 5.11 — hand-written in Board.h), `QMouseEvent::position()` (5.14+; use `localPos()`), `QVector::swap(i,j)` (5.13+).
- **Qt6 migration-friendly**: containers use `QVector` (not `QList`); encapsulate version-specific API differences in utility functions.
- **Linux uses system Qt, not bundled**: build with `apt install qtbase5-dev`; runtime depends on system `libqt5core5a`. No vcpkg on Linux. The deb packages only the binary + desktop entry + icon.
- **Layering is QtCore-pure at the bottom**: `src/core/` and `src/app/` (GameController/ReviewController/AppSettings/MainWindowLogic) may include ONLY QtCore headers — they are compiled into `yigo_core` and unit-tested without a GUI. `src/gtp/` likewise QtCore-only. Only `src/ui/` may use QtWidgets.
- **Zero third-party deps**: SGF parsing, GTP protocol, charts all self-implemented. Do not add external libraries.
- **Tests are offline**: `tests/data/fake_engine.sh` emulates a GTP engine (responses MUST end with the GTP blank-line terminator `\n\n`; `YIGO_FAKE_GENMOVE` env injects genmove replies). Never make tests depend on a real KataGo.

## Architecture (4 layers)

```
ui/ (Widgets) → app/ (Controllers) → core/ (domain, QtCore only) → gtp/ (engine via QProcess)
```

- UI panels never call each other directly; always go through controllers (`GameController` play mode, `ReviewController` batch review).
- GTP engine runs in a separate `QProcess` (async, crash-isolated); engine crash must never take down the UI — controllers roll back to HumanTurn/stop the batch.
- **Ownership is single-owner**: `GameController` owns the play `Game*`; `MainWindow::m_game` is a non-owning alias refreshed by `startGame`; `ReviewController` holds non-owning `Game*` + main-line node pointers — `MainWindow::startGame/onOpen` MUST call `m_review->stop()` before replacing the game (UAF otherwise, probe-verified).
- `core/` owns all `MoveNode`s via `GameTree`; external code holds raw pointers for read-only access only.
- EngineProcess pauses the GtpClient codec during analysis (bare `info` frames vs `\n\n`-framed responses are two different read paths); genmove must call `stopAnalysis()` first or responses get swallowed.

## Domain conventions

- **Winrate is always Black's perspective** (0.0–1.0) at every internal boundary; UI converts to the side-to-move view for display. LZ side-to-move conversion happens in EngineProcess (it knows the query color), NOT in the parser.
- Board coords: `x` = column (left→right, 0-indexed), `y` = row (top→bottom, 0-indexed). Internal grid is row-major: `idx = y * size + x`.
- **GTP display rows count from the BOTTOM**: D4@19 = (3,15). SGF pairs are top-down ('a'..'z' → 0..25); `tt` or empty = pass (only for board size ≤19). Column letter skips `I` (A–T, no I). LZ's "d4" is lowercase letter+digit, row from bottom.
- Pass is `(-1,-1)` (Game::play skips legality for it); resign parses to (-2,-2).
- Board sizes: 9 / 13 / 19 (`Board::MaxSize = 25`). Handicap: flat 9-point star sequence [ur,ul,ll,lr,tengen,L,R,T,B]; even counts (4/6/8) skip tengen; root + handicap>0 → White plays first.
- Zobrist hashing uses a fixed seed (`0x1BADB002`) for cross-process consistency.
- Blunder threshold default 5% (0.05), per-side gain: black = cur−prev, white = prev−cur.

## Build & test (exact commands)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # configure (once)
cmake --build build                                  # build
ctest --test-dir build --output-on-failure          # run all tests (16 suites, <5s)
ctest --test-dir build -R tst_board --output-on-failure   # run a single test (by name)
cmake --build build --target deb                    # yigo_0.5.0_amd64.deb
```

- Fake GTP engine for offline tests: `tests/data/fake_engine.sh` (path injected via `FAKE_ENGINE` compile definition). Env `YIGO_FAKE_GENMOVE` injects pass/resign genmove replies.
- GUI probes in `tests/gui/` (paint_probe/click_probe) run offscreen (`QT_QPA_PLATFORM=offscreen`) and pixel-verify paint/click alignment — keep them green when touching BoardView/BoardGeometry.
- `colscan` is a diagnostic tool (not in ctest): `cmake --build build --target colscan`.

## Workflow

- **TDD per task**: write failing test → verify it fails → minimal implementation → verify pass → commit.
- Commit style: conventional, short — `feat(core): ...`. Local git identity: `alex <mogoweb@gmail.com>` (already configured).
- Each milestone deliverable must be runnable and testable on its own. Milestone plans live in `docs/superpowers/plans/`, the architecture spec in `docs/superpowers/specs/`.
- Engine testing: `tests/data/fake_engine.sh` emulates GTP (`YIGO_FAKE_GENMOVE` env injects pass/resign into genmove replies).
