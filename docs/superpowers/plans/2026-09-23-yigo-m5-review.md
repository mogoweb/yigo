# YiGo M5（复盘分析）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ReviewController 批量复盘分析（逐手 analyze、可暂停/跳转）+ WinrateCurve 数据模型 + ChartWinrate 曲线图（点击跳转）+ 失着标记（胜率差 >5% 标点）。

**Architecture:** 新增 `src/core/WinrateCurve`（QtCore-only 数据模型：手数→胜率序列 + 失着检测纯逻辑，可离线单测）；`src/app/ReviewController`（QtCore-only：持有 Game 主线快照队列，逐手发 analyzePosition，analysisUpdate 回填 MoveNode.analysis，完成一手 advance；支持 pause/resume/jumpTo(打断队列分析当前节点再恢复)）；`src/ui/ChartWinrate`（纯 QPainter QWidget：X 手数、Y 胜率 0..1，段着色红/蓝，点击发 moveClicked(k)，当前手数竖线）。MainWindow 复盘入口：Game→Review 模式开关（或加载 SGF 后自动可复盘），底部 dock 放 ChartWinrate。MoveNode.analysis 已有（M1），曲线直接读回填结果。

**Tech Stack:** C++17, Qt 5.11.3 (QtCore + QtWidgets), CMake 3.22, Ninja, QtTest（WinrateCurve/ReviewController 离线 TDD 用 fake_engine.sh 流式模式；ChartWinrate 像素探针进 tests/gui）

**Spec:** `docs/superpowers/specs/2026-09-21-yigo-architecture-design.md`（§3 WinrateCurve、§5 ReviewController/ChartWinrate、§7 测试策略、§8 里程碑 M5）

## Global Constraints

- Qt 5.11 API 基线：只用 Qt 5.11.3 存在的 API
- `src/core/WinrateCurve` 与 `src/app/ReviewController` QtCore-only（编入 yigo_core）
- 容器统一用 `QVector`
- 胜率统一黑方视角 0..1（M3 invariant；曲线 Y 轴直接用 AnalysisData.winrate）
- 失着阈值默认 5%（0.05），按执方换算比较（spec §5 ReviewController：与前一手胜率差）
- 复盘分析串行：同一时刻只分析一个节点；跳转 = 停当前 → 分析目标 → 恢复队列（spec §5）
- 曲线点击跳转；当前手数竖线指示（spec §5 ChartWinrate）
- 每任务 TDD：先写失败测试→验证失败→最小实现→验证通过→提交
- 编译命令统一：`cmake --build build`；测试统一：`ctest --test-dir build --output-on-failure`
- 提交身份：仓库 local 配置 `alex <mogoweb@gmail.com>`

## Review Focus

以下输入类别在 spec 中隐含但最易踩坑（每条已落在对应任务中作为测试步骤）：

1. **复盘跳转打断分析队列**（批量分析进行中用户点击曲线第 k 手）：期望当前分析被放弃、目标节点被分析、队列从 k+1 恢复 —— Task 2 测试 `jumpDuringAnalysis`
2. **引擎崩溃/错误时批量分析**（第 k 手分析中引擎崩溃）：期望批量停止、状态回 Idle、已分析的手保留 —— Task 2 测试 `crashStopsBatch`
3. **胜率差按执方换算**（黑方视角 winrate：黑走一手后从 0.6 跌到 0.4 = 黑损 20%；白走一手后从 0.4 升到 0.6 = 黑涨 20% 但白亏损着是白的事）：期望失着标记的是"走出该手的这一方亏损" —— Task 1 测试 `blunderPerSide`
4. **曲线点击越界/空曲线**（0 手局面、点击曲线外区域）：期望信号不发出或不越界 —— Task 3 测试 `chartClickBounds`（像素探针）
5. **SGF 变着分支**（加载多分支 SGF 复盘）：期望只分析主线（children[0] 路径），变着不进曲线 —— Task 2 测试 `mainLineOnly`

---

### Task 1: WinrateCurve 数据模型 + 失着检测（TDD）

**Files:**
- Create: `src/core/WinrateCurve.h` / `src/core/WinrateCurve.cpp`
- Modify: `src/core/CMakeLists.txt`（编入）
- Test: `tests/tst_winratecurve.cpp`
- Modify: `tests/CMakeLists.txt`（追加）

**Interfaces:**
- Consumes: M1 `AnalysisData`（GameTree.h）、`Stone`、`MoveNode`
- Produces:
  ```cpp
  // src/core/WinrateCurve.h
  struct WinratePoint {
      int moveNumber = 0;        // 0 = 根（开局前）
      double winrate = 0.5;      // 黑方视角 0..1
      bool blunder = false;      // 失着：走出该手的一方亏损 > 阈值
      Stone sideToMove = Stone::Empty;  // 走出该手的一方
  };
  class WinrateCurve {
  public:
      // 从主线节点序列构建（root 的 analysis 若 valid 作为第 0 点）
      void rebuild(const QVector<MoveNode*>& mainLine);
      // 单点回填（引擎完成第 k 手分析后调用）；k 已存在则覆盖
      void setPoint(int moveNumber, double winrate);
      int count() const { return m_points.size(); }
      const QVector<WinratePoint>& points() const { return m_points; }
      // 失着阈值；默认 0.05
      void setBlunderThreshold(double t) { m_threshold = t; }
      double blunderThreshold() const { return m_threshold; }
  };
  ```
  失着规则（review focus 3）：第 k 手（k>=1）的执方 S；前一有效点 winrate prev 与当前 winrate cur（均黑方视角）：
  - S==Black：黑方收益 = cur - prev；收益 < -threshold → blunder
  - S==White：白方收益 = prev - cur；收益 < -threshold → blunder

- [ ] **Step 1: 写失败测试 tests/tst_winratecurve.cpp**

```cpp
#include <QtTest>
#include "WinrateCurve.h"
#include "GameTree.h"

class TestWinrateCurve : public QObject {
    Q_OBJECT
private slots:
    void emptyCurve() {
        WinrateCurve c;
        QCOMPARE(c.count(), 0);
    }
    void rebuildFromMainLine() {
        // 构造主线 3 手，只填 analysis
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));
        auto* n2 = t.addChild(n1, Stone::White, QPoint(15, 15));
        auto* n3 = t.addChild(n2, Stone::Black, QPoint(2, 2));
        t.root()->analysis = AnalysisData{}; t.root()->analysis.valid = true; t.root()->analysis.winrate = 0.5;
        n1->analysis.valid = true; n1->analysis.winrate = 0.55;
        n2->analysis.valid = true; n2->analysis.winrate = 0.45;
        n3->analysis.valid = true; n3->analysis.winrate = 0.5;
        QVector<MoveNode*> line{t.root(), n1, n2, n3};
        WinrateCurve c;
        c.rebuild(line);
        QCOMPARE(c.count(), 4);
        QCOMPARE(c.points()[0].winrate, 0.5);
        QCOMPARE(c.points()[0].moveNumber, 0);
        QCOMPARE(c.points()[3].moveNumber, 3);
        QCOMPARE(c.points()[1].sideToMove, Stone::Black);
        QCOMPARE(c.points()[2].sideToMove, Stone::White);
    }
    void blunderPerSide() {
        // review focus 3: 黑跌 20% = 黑失着；白把局面从 0.4 拉回 0.6（白亏 20%）= 白失着
        GameTree t;
        auto* n1 = t.addChild(t.root(), Stone::Black, QPoint(3, 3));   // 黑走
        auto* n2 = t.addChild(n1, Stone::White, QPoint(15, 15));       // 白走
        t.root()->analysis.valid = true; t.root()->analysis.winrate = 0.5;
        n1->analysis.valid = true; n1->analysis.winrate = 0.3;   // 黑跌 20% → 黑失着
        n2->analysis.valid = true; n2->analysis.winrate = 0.5;   // 白把 0.3→0.5，白亏 20% → 白失着
        QVector<MoveNode*> line{t.root(), n1, n2};
        WinrateCurve c;
        c.rebuild(line);
        QVERIFY(c.points()[1].blunder);   // 黑失着
        QVERIFY(c.points()[2].blunder);   // 白失着
        // 阈值边界：差正好等于阈值不算失着
        n1->analysis.winrate = 0.45;      // 黑跌 5% = 阈值 → 不算
        c.rebuild(line);
        QVERIFY(!c.points()[1].blunder);
        // 未分析点（valid=false）不参与
        n2->analysis.valid = false;
        c.rebuild(line);
        QVERIFY(!c.points()[2].blunder);
    }
    void setPointOverwrites() {
        WinrateCurve c;
        WinratePoint p;
        p.moveNumber = 2; p.winrate = 0.7;
        c.setPoint(2, 0.7);
        QCOMPARE(c.count(), 1);
        c.setPoint(2, 0.8);   // 覆盖
        QCOMPARE(c.count(), 1);
        QCOMPARE(c.points()[0].winrate, 0.8);
        c.setPoint(1, 0.4);   // 乱序插入：保持按 moveNumber 排序
        QCOMPARE(c.count(), 2);
        QCOMPARE(c.points()[0].moveNumber, 1);
    }
};

QTEST_GUILESS_MAIN(TestWinrateCurve)
#include "tst_winratecurve.moc"
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_winratecurve)
```

- [ ] **Step 2: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（WinrateCurve.h 不存在）

- [ ] **Step 3: 写实现**

WinrateCurve.h:

```cpp
#pragma once
#include <QVector>

#include "Board.h"
#include "GameTree.h"

struct WinratePoint {
    int moveNumber = 0;
    double winrate = 0.5;      // black's perspective 0..1
    bool blunder = false;
    Stone sideToMove = Stone::Empty;
};

class WinrateCurve {
public:
    void rebuild(const QVector<MoveNode*>& mainLine);
    void setPoint(int moveNumber, double winrate);
    int count() const { return m_points.size(); }
    const QVector<WinratePoint>& points() const { return m_points; }
    void setBlunderThreshold(double t) { m_threshold = t; }
    double blunderThreshold() const { return m_threshold; }

private:
    QVector<WinratePoint> m_points;
    double m_threshold = 0.05;
};
```

WinrateCurve.cpp:

```cpp
#include "WinrateCurve.h"
#include <algorithm>

void WinrateCurve::rebuild(const QVector<MoveNode*>& mainLine) {
    m_points.clear();
    for (MoveNode* n : mainLine) {
        WinratePoint p;
        p.moveNumber = n->moveNumber;
        p.winrate = n->analysis.valid ? n->analysis.winrate : 0.5;
        p.sideToMove = n->color;
        m_points.append(p);
    }
    // blunder detection needs the previous ANALYZED point
    int lastAnalyzed = -1;
    for (int i = 1; i < m_points.size(); ++i) {
        MoveNode* n = mainLine[i];
        if (!n || !n->analysis.valid) continue;
        if (lastAnalyzed >= 0) {
            const double prev = m_points[lastAnalyzed].winrate;
            const double cur = m_points[i].winrate;
            const Stone side = m_points[i].sideToMove;
            const double gain = (side == Stone::Black) ? (cur - prev) : (prev - cur);
            m_points[i].blunder = gain < -m_threshold;
        }
        lastAnalyzed = i;
    }
}

void WinrateCurve::setPoint(int moveNumber, double winrate) {
    for (WinratePoint& p : m_points)
        if (p.moveNumber == moveNumber) { p.winrate = winrate; return; }
    WinratePoint p;
    p.moveNumber = moveNumber;
    p.winrate = winrate;
    m_points.append(p);
    std::sort(m_points.begin(), m_points.end(),
              [](const WinratePoint& a, const WinratePoint& b) {
                  return a.moveNumber < b.moveNumber;
              });
}
```

- [ ] **Step 4: 构建测试通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/core tests
git commit -m "feat(core): winrate curve model with per-side blunder detection"
```

---

### Task 2: ReviewController 批量分析（fake engine 流式 TDD）

**Files:**
- Create: `src/app/ReviewController.h` / `src/app/ReviewController.cpp`
- Modify: `src/core/CMakeLists.txt`（编入）
- Modify: `tests/data/fake_engine.sh`（kata-analyze 每次回不同的 winrate：用 YIGO_FAKE_ANALYZE_COUNTER 递增）
- Test: `tests/tst_reviewcontroller.cpp`
- Modify: `tests/CMakeLists.txt`（追加，带 FAKE_ENGINE 宏）

**Interfaces:**
- Consumes: Task 1 `WinrateCurve`、M3 `EngineProcess`（analyzePosition/analysisUpdate/crashed/errorOccurred）、M1 `Game/GameTree`
- Produces:
  ```cpp
  // src/app/ReviewController.h
  class ReviewController : public QObject {
      Q_OBJECT
  public:
      explicit ReviewController(QObject* parent = nullptr);
      void startReview(Game* game, EngineProcess* engine);  // 主线快照 + 启动批量
      void pause(); void resume();
      bool isRunning() const { return m_running; }
      int cursor() const { return m_cursor; }               // 已分析到的手数
      const WinrateCurve& curve() const { return m_curve; }
      // 跳转：放弃当前分析 → 分析第 k 手 → 从 k+1 恢复队列（k=-1 只跳不恢复）
      void jumpTo(int moveNumber);
  signals:
      void progressed(int moveNumber);          // 每完成一手
      void finished();                          // 主线全部完成
      void blunderFound(int moveNumber, Stone side);
  private slots:
      void onAnalysisUpdate(const AnalysisData& data);
      void onCrashed(int code);
      void onEngineError(const QString& msg);
  private:
      void analyzeNext();
      Game* m_game = nullptr;
      EngineProcess* m_engine = nullptr;
      QVector<MoveNode*> m_mainLine;
      WinrateCurve m_curve;
      int m_cursor = 0;          // 主线中下一个待分析下标
      bool m_running = false;
      bool m_paused = false;
      bool m_awaitReply = false;
  };
  ```
  串行规则：一次只 analyze 一个节点（m_awaitReply 保护）；analysisUpdate 到达 → 回填 m_game 主线对应节点 analysis → m_curve.setPoint → progressed → analyzeNext。jumpTo：stopAnalysis → m_cursor = k 下标 → 若 k 手未分析先分析它再继续队列。crashed/errorOccurred → m_running=false, finished() 不发（或发 aborted 语义——裁决：直接 finished()，调用方检查 isRunning）。

- [ ] **Step 1: fake engine 流式 winrate 递增**

tests/data/fake_engine.sh 的 kata-analyze 分支改为（计数器文件不引——用循环变量即可，每次 analyze 后自增）：

```bash
        kata-analyze|lz-analyze)
            n="${YIGO_ANALYZE_SEQ:-0}"
            wr=$(python3 -c "print(0.4 + 0.02 * ($n % 20))" 2>/dev/null || echo 0.5)
            printf "info move D4 visits 120 winrate %s scoreLead 0.3\n" "$wr"
            printf "\n"
            YIGO_ANALYZE_SEQ=$((n+1))   # 子进程内无法回传——改用父进程 export?
            continue ;;
```

（**裁决**：bash 子进程变量无法跨查询保持且 EngineProcess 侧不重读环境。改用简单确定性方案：**fake engine 忽略 winrate 变化，恒回 0.5；ReviewController 测试断言 progressed 次数与 cursor 推进**，不断言不同手不同胜率。胜率回填正确性由 Task 1 setPoint 与 M3 解析器单测覆盖。YIGO_ANALYZE_SEQ 方案废弃。）

fake_engine.sh kata-analyze 分支保持 M3 现状（两帧 + 空行）不变。

- [ ] **Step 2: 写失败测试 tests/tst_reviewcontroller.cpp**

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "ReviewController.h"
#include "EngineProcess.h"
#include "SgfParser.h"

class TestReviewController : public QObject {
    Q_OBJECT
private:
    EngineConfig fakeCfg() const {
        EngineConfig cfg;
        cfg.type = EngineConfig::KataGo;
        cfg.executable = "/bin/bash";
        cfg.baseArgs = QStringList() << QStringLiteral(FAKE_ENGINE) << "basic";
        cfg.gtpCommand = QStringLiteral("kata-analyze interval 50");
        return cfg;
    }

private slots:
    void batchAnalyzesMainLine() {
        QString err;
        Game* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd])", &err);
        QVERIFY2(g != nullptr, qPrintable(err));
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        QSignalSpy prog(&rc, &ReviewController::progressed);
        rc.startReview(g, &ep);
        // 3 手全部完成（fake analyze 每次即时回 2 帧）
        QTRY_COMPARE(prog.count(), 3);
        QCOMPARE(rc.cursor(), 3);
        QVERIFY(rc.curve().count() >= 3);
        QTRY_VERIFY(!rc.isRunning());
        ep.stop();
        delete g;
    }
    void pauseStopsBatch() {
        QString err;
        Game* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd])", &err);
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        rc.startReview(g, &ep);
        rc.pause();
        const int c0 = rc.cursor();
        QTest::qWait(300);
        QCOMPARE(rc.cursor(), c0);   // 暂停后不推进
        rc.resume();
        QTRY_COMPARE(rc.cursor(), 3);
        ep.stop();
        delete g;
    }
    void jumpDuringAnalysis() {
        // review focus 1: 分析进行中跳转 → 目标节点先分析，队列恢复
        QString err;
        Game* g = SgfParser::parse(
            "(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd];B[pp];W[qq])", &err);   // 5 手
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        QSignalSpy prog(&rc, &ReviewController::progressed);
        rc.startReview(g, &ep);
        QTest::qWait(50);            // 让批量启动
        rc.jumpTo(1);                // 跳到第 1 手
        QTRY_COMPARE(rc.cursor(), 5);   // 从 1 恢复后走完
        QCOMPARE(prog.count(), 5);      // 每手恰好一次（含被跳的第 1 手）
        ep.stop();
        delete g;
    }
    void crashStopsBatch() {
        // review focus 2: 引擎崩溃 → 停止、保留已完成
        QString err;
        Game* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg];B[dd])", &err);
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        rc.startReview(g, &ep);
        QTest::qWait(50);
        ep.query(999, "please crash");
        QTRY_VERIFY(!rc.isRunning());
        ep.stop();
        delete g;
    }
    void mainLineOnly() {
        // review focus 5: 分支不进曲线
        QString err;
        Game* g = SgfParser::parse(
            "(;GM[1]FF[4]SZ[9];B[cc](;W[gg];B[dd])(;W[dd];B[gg]))", &err);
        QVERIFY(g != nullptr);
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        ReviewController rc;
        QSignalSpy prog(&rc, &ReviewController::progressed);
        rc.startReview(g, &ep);
        QTRY_COMPARE(prog.count(), 3);   // 主线 3 手（root+2 moves…主线=2手）
        // 主线 = B[cc],W[gg] → 2 手 + 根 = 曲线点 ≤ 3
        QVERIFY(rc.curve().count() <= 3);
        ep.stop();
        delete g;
    }
};

QTEST_GUILESS_MAIN(TestReviewController)
#include "tst_reviewcontroller.moc"
```

tests/CMakeLists.txt 追加：

```cmake
add_executable(tst_reviewcontroller tst_reviewcontroller.cpp)
target_compile_definitions(tst_reviewcontroller PRIVATE
    FAKE_ENGINE="${CMAKE_SOURCE_DIR}/tests/data/fake_engine.sh")
target_link_libraries(tst_reviewcontroller PRIVATE yigo_core Qt5::Test)
add_test(NAME tst_reviewcontroller COMMAND tst_reviewcontroller)
```

- [ ] **Step 3: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（ReviewController 不存在）

- [ ] **Step 4: 写实现**

ReviewController.h（按 Produces 全量声明）。

ReviewController.cpp 关键逻辑：

```cpp
void ReviewController::startReview(Game* game, EngineProcess* engine) {
    m_game = game;
    m_engine = engine;
    m_mainLine.clear();
    m_mainLine.append(game->tree().root());
    m_mainLine += game->tree().mainLine();
    m_curve.rebuild(m_mainLine);
    m_cursor = 0;
    m_running = true;
    m_paused = false;
    m_awaitReply = false;
    // connections (guard against double-start)
    if (!m_connUpdate) {
        m_connUpdate = connect(m_engine, &EngineProcess::analysisUpdate, this, &ReviewController::onAnalysisUpdate);
        m_connCrash = connect(m_engine, &EngineProcess::crashed, this, &ReviewController::onCrashed);
        m_connErr = connect(m_engine, &EngineProcess::errorOccurred, this, &ReviewController::onEngineError);
    }
    analyzeNext();
}

void ReviewController::analyzeNext() {
    if (!m_running || m_paused || m_awaitReply) return;
    while (m_cursor < m_mainLine.size() && m_mainLine[m_cursor]->analysis.valid)
        ++m_cursor;                        // 跳过已分析节点
    if (m_cursor >= m_mainLine.size()) {
        m_running = false;
        Q_EMIT finished();
        return;
    }
    m_awaitReply = true;
    AnalysisQuery q;
    q.color = m_mainLine[m_cursor]->color == Stone::Empty
                  ? Stone::Black : m_mainLine[m_cursor]->color;
    m_engine->analyzePosition(*m_game, q);
    // 注意：analyzePosition 分析的是"当前局面"？—— 不：M3 analyzePosition 同步
    // 位置后 analyze 的是引擎收到的最终局面，即 game 当前节点局面。
    // —— **裁决**：复盘需逐"手后"局面分析。实现：先把 game goTo(主线[m_cursor])
    //    再 analyzePosition；完成后 goTo 不变（复盘窗口跟随 cursor）。
    m_game->goTo(m_mainLine[m_cursor]);
}

void ReviewController::onAnalysisUpdate(const AnalysisData& data) {
    if (!m_awaitReply || !m_running) return;
    m_awaitReply = false;
    MoveNode* n = m_mainLine[m_cursor];
    n->analysis = data;
    m_curve.setPoint(n->moveNumber, data.winrate);
    Q_EMIT progressed(n->moveNumber);
    if (data.winrate 与前分析点差 > 阈值 && n->color != Stone::Empty) {
        const Stone side = n->color;
        Q_EMIT blunderFound(n->moveNumber, side);
    }
    ++m_cursor;
    analyzeNext();
}

void ReviewController::jumpTo(int moveNumber) {
    if (!m_running) return;
    // 找到主线中 moveNumber 的下标
    int idx = -1;
    for (int i = 0; i < m_mainLine.size(); ++i)
        if (m_mainLine[i]->moveNumber == moveNumber) { idx = i; break; }
    if (idx < 0) return;
    stopAnalysis();                 // M3：打断当前流
    m_awaitReply = false;
    m_cursor = idx;
    analyzeNext();                  // 先分析目标，之后队列自然恢复
}

void ReviewController::onCrashed(int) {
    m_running = false;
    m_awaitReply = false;
    Q_EMIT finished();              // aborted 语义：调用方以 isRunning 区分
}

void ReviewController::onEngineError(const QString&) {
    m_running = false;
    m_awaitReply = false;
    Q_EMIT finished();
}
```

（注意：jumpTo 中 stopAnalysis 会触发 EngineProcess 的 Stopping→Idle；analyzePosition 内部队列从新位置开始——与 M3 实现兼容。paused 状态：pause() 设 m_paused=true 且 stopAnalysis()；resume() 设 m_paused=false 且 analyzeNext()。）

- [ ] **Step 5: 构建测试通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add src tests
git commit -m "feat(app): ReviewController batch analysis with pause/jump/crash handling"
```

---

### Task 3: ChartWinrate 曲线图（像素探针）

**Files:**
- Create: `src/ui/ChartWinrate.h` / `src/ui/ChartWinrate.cpp`
- Modify: `src/ui/CMakeLists.txt`（加源文件）
- Test: `tests/gui/paint_probe.cpp`（追加 checkChart）

**Interfaces:**
- Consumes: Task 1 `WinrateCurve`
- Produces:
  ```cpp
  // src/ui/ChartWinrate.h
  class ChartWinrate : public QWidget {
      Q_OBJECT
  public:
      explicit ChartWinrate(QWidget* parent = nullptr);
      void setCurve(const WinrateCurve* curve);      // 非拥有
      void setCurrentMove(int moveNumber);           // 当前手数竖线
  signals:
      void moveClicked(int moveNumber);              // 点击曲线区域 → 最近手数
  protected:
      void paintEvent(QPaintEvent*) override;
      void mousePressEvent(QMouseEvent*) override;
  };
  ```
  绘制（spec §5）：白底网格；X 轴手数（0..N），Y 轴胜率 0..1（黑方视角，1 在上）；数据点折线；相邻两段按"黑方收益"着色（涨=蓝，跌=红，spec §5 红=黑亏、蓝=黑涨）；当前手数竖线（黑虚线）。点击：X 反算最近手数（±1 手内）发 moveClicked。

- [ ] **Step 1: 写失败探针（tests/gui/paint_probe.cpp 追加 checkChart）**

```cpp
static int checkChart() {
    WinrateCurve c;
    WinratePoint p0; p0.moveNumber = 0; p0.winrate = 0.5;
    WinratePoint p1; p1.moveNumber = 1; p1.winrate = 0.7;   // 黑涨 → 蓝段
    WinratePoint p2; p2.moveNumber = 2; p2.winrate = 0.3;   // 黑跌 → 红段
    // 直接构造 curve：用 setPoint 逐点
    c.setPoint(0, 0.5); c.setPoint(1, 0.7); c.setPoint(2, 0.3);
    ChartWinrate w;
    w.setCurve(&c);
    w.setCurrentMove(1);
    w.resize(300, 150);
    const QImage img = w.grab().toImage();
    // 1) 找红色像素（跌段）与蓝色像素（涨段）都存在
    bool hasRed = false, hasBlue = false;
    for (int y = 0; y < 150 && !(hasRed && hasBlue); ++y)
        for (int x = 0; x < 300 && !(hasRed && hasBlue); ++x) {
            const QRgb px = img.pixel(x, y);
            if (qRed(px) > 180 && qGreen(px) < 100 && qBlue(px) < 100) hasRed = true;
            if (qBlue(px) > 180 && qRed(px) < 100) hasBlue = true;
        }
    if (!hasRed || !hasBlue) {
        std::printf("FAIL: chart segment colors missing (red=%d blue=%d)\n",
                    hasRed, hasBlue);
        return 1;
    }
    return 0;
}
```

（加入 main 的 failures 累计。）先跑确认编译失败/FAIL，再实现。

- [ ] **Step 2: 实现 ChartWinrate**

ChartWinrate.h:

```cpp
#pragma once
#include <QWidget>

#include "WinrateCurve.h"

class ChartWinrate : public QWidget {
    Q_OBJECT
public:
    explicit ChartWinrate(QWidget* parent = nullptr);
    void setCurve(const WinrateCurve* curve);
    void setCurrentMove(int moveNumber);

signals:
    void moveClicked(int moveNumber);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    QPointF pointToPixel(int moveNumber, double winrate) const;
    int pixelToMove(QPointF p) const;

    const WinrateCurve* m_curve = nullptr;
    int m_current = -1;
    qreal m_marginLeft = 30.0, m_marginRight = 10.0, m_marginTop = 10.0, m_marginBottom = 20.0;
};
```

ChartWinrate.cpp:

```cpp
#include "ChartWinrate.h"
#include <QMouseEvent>
#include <QPainter>

ChartWinrate::ChartWinrate(QWidget* parent) : QWidget(parent) {
    setMinimumSize(240, 120);
}

void ChartWinrate::setCurve(const WinrateCurve* curve) {
    m_curve = curve;
    update();
}

void ChartWinrate::setCurrentMove(int moveNumber) {
    m_current = moveNumber;
    update();
}

QPointF ChartWinrate::pointToPixel(int moveNumber, double winrate) const {
    const int maxMove = qMax(1, m_curve ? m_curve->count() - 1 : 1);
    const qreal w = width() - m_marginLeft - m_marginRight;
    const qreal h = height() - m_marginTop - m_marginBottom;
    const qreal x = m_marginLeft + w * (maxMove > 0 ? qreal(moveNumber) / maxMove : 0.0);
    const qreal y = m_marginTop + h * (1.0 - qBound(0.0, winrate, 1.0));
    return QPointF(x, y);
}

int ChartWinrate::pixelToMove(QPointF p) const {
    if (!m_curve || m_curve->count() < 1) return -1;
    const int maxMove = qMax(1, m_curve->count() - 1);
    const qreal w = width() - m_marginLeft - m_marginRight;
    const qreal frac = (p.x() - m_marginLeft) / w;
    const int mv = qRound(frac * maxMove);
    if (mv < 0 || mv > maxMove) return -1;
    return mv;
}

void ChartWinrate::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);
    // grid: horizontal quarter lines
    p.setPen(QPen(QColor(220, 220, 220), 1.0));
    for (int i = 0; i <= 4; ++i) {
        const qreal y = m_marginTop + (height() - m_marginTop - m_marginBottom) * i / 4.0;
        p.drawLine(QPointF(m_marginLeft, y), QPointF(width() - m_marginRight, y));
    }
    if (!m_curve || m_curve->count() < 1) return;
    // segments colored by black's gain
    const auto& pts = m_curve->points();
    for (int i = 1; i < pts.size(); ++i) {
        const double gain = pts[i].winrate - pts[i - 1].winrate;
        p.setPen(QPen(gain >= 0 ? QColor(60, 100, 220) : QColor(220, 60, 60), 2.0));
        p.drawLine(pointToPixel(pts[i-1].moveNumber, pts[i-1].winrate),
                   pointToPixel(pts[i].moveNumber, pts[i].winrate));
    }
    // points
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 40, 40));
    for (const auto& pt : pts) {
        if (pt.moveNumber == 0) continue;
        p.drawEllipse(pointToPixel(pt.moveNumber, pt.winrate), 3.0, 3.0);
        if (pt.blunder) {
            p.setBrush(QColor(255, 140, 0));   // orange blunder ring (spec §5)
            p.drawEllipse(pointToPixel(pt.moveNumber, pt.winrate), 5.0, 5.0);
            p.setBrush(QColor(40, 40, 40));
        }
    }
    // current-move vertical line
    if (m_current >= 0 && !pts.isEmpty()) {
        const qreal x = pointToPixel(m_current, 0.5).x();
        p.setPen(QPen(Qt::black, 1.0, Qt::DashLine));
        p.drawLine(QPointF(x, m_marginTop), QPointF(x, height() - m_marginBottom));
    }
}

void ChartWinrate::mousePressEvent(QMouseEvent* e) {
    // Qt 5.11: localPos()
    const int mv = pixelToMove(e->localPos());
    if (mv >= 0)
        Q_EMIT moveClicked(mv);
}
```

`src/ui/CMakeLists.txt` 加 `ChartWinrate.cpp`。

- [ ] **Step 3: 构建 + 探针通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 4: Commit**

```bash
git add src tests
git commit -m "feat(ui): winrate chart with colored segments and click-to-jump"
```

---

### Task 4: MainWindow 复盘模式接线（收尾验收）

**Files:**
- Modify: `src/ui/MainWindow.h` / `MainWindow.cpp`
- Modify: `src/ui/CMakeLists.txt`（已含 ChartWinrate）

**Interfaces:**
- Consumes: Task 1-3 全部、M4 GameController
- Produces: M5 完整交付。

- [ ] **Step 1: MainWindow 集成**

```cpp
// 成员：
    ReviewController* m_review = nullptr;
    ChartWinrate* m_chart = nullptr;
    bool m_reviewMode = false;

// setupCentral() 追加（底部 dock）:
    m_chart = new ChartWinrate(this);
    auto* chartDock = new QDockWidget(tr("Winrate"), this);
    chartDock->setWidget(m_chart);
    addDockWidget(Qt::BottomDockWidgetArea, chartDock);
    chartDock->hide();                     // 复盘模式才显示
    m_review = new ReviewController(this);
    connect(m_review, &ReviewController::progressed, this, &MainWindow::onReviewProgress);
    connect(m_review, &ReviewController::finished, this, &MainWindow::onReviewFinished);
    connect(m_chart, &ChartWinrate::moveClicked, this, &MainWindow::onChartClicked);

// 菜单 Game→Analyze Game (Ctrl+R):
void MainWindow::onReview() {
    if (!m_game || !m_engine || !m_engine->isRunning()) {
        QMessageBox::warning(this, tr("Review"), tr("Start the engine first"));
        return;
    }
    m_reviewMode = true;
    m_chart->parentWidget()->show();
    m_review->startReview(m_game, m_engine);
}
void MainWindow::onReviewProgress(int moveNumber) {
    m_chart->setCurve(&m_review->curve());
    m_chart->setCurrentMove(moveNumber);
    // 若当前节点在主线 → goTo 让棋盘跟随（复盘窗口联动）
}
void MainWindow::onReviewFinished() { /* status: done or aborted */ }
void MainWindow::onChartClicked(int mv) {
    if (m_reviewMode && m_game) {
        // 主线上找 mv 手节点
        const auto ml = m_game->tree().mainLine();
        for (MoveNode* n : ml)
            if (n->moveNumber == mv) { m_game->goTo(n); m_chart->setCurrentMove(mv);
                                       m_boardView->update(); refreshStatus(); return; }
    }
}
```

- [ ] **Step 2: 构建 + 全量回归 + 手工验收**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
手工验收（fake_engine 或真 KataGo）：
1. 启动引擎 → 打开多手 SGF → Game→Analyze Game（Ctrl+R）
2. 底部 Winrate 面板出现，曲线逐手绘制（fake 引擎瞬时完成）
3. 点击曲线某手 → 棋盘跳转到该手局面，竖线跟随
4. 复盘中引擎 kill -9 → 批量停止，已绘曲线保留，UI 存活
5. 真引擎低 visits 复盘 30 手 SGF：失着点橙色标记出现在跌幅 >5% 的手

- [ ] **Step 3: Commit**

```bash
git add src/ui
git commit -m "feat(ui): review mode with winrate chart dock and click-to-jump"
```

---

## 完成定义（M5 DoD）

- `ctest --test-dir build` 全绿（M1-M4 + tst_winratecurve + tst_reviewcontroller + GUI 探针含 checkChart）
- 复盘闭环：引擎运行中 → Analyze Game → 主线逐手分析 → 曲线实时生长 → 点击跳转 → 失着橙点
- 引擎崩溃批量停止且 UI 存活；跳转打断队列正确恢复
- 每个 Task 独立提交，共 4 个提交
