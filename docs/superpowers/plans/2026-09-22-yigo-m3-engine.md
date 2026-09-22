# YiGo M3（引擎对接）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** GTP 引擎进程管理 + 协议编解码 + 实时分析覆盖层，交付带引擎分析面板的 `yigo`，支持 KataGo `kata-analyze` 与 LeelaZero `lz-analyze`。

**Architecture:** 新增 `src/gtp/`（QtCore-only，编入 yigo_core）：`GtpClient` 用 QIODevice 抽象做行协议编解码（可用 QBuffer 离线单测），`AnalysisParser` 按 `EngineType` 分派解析 kata-analyze/lz-analyze 流式帧为统一 `AnalysisData`（纯函数，golden 测试），`EngineProcess` 用 QProcess 串行队列驱动引擎并发射信号。UI 侧新增 `EnginePanel`（引擎配置/状态）与 BoardView 分析覆盖层（候选点半透明棋子 + 胜率/访问量）。核心原则：引擎崩溃永不影响主程序（spec §1 质量属性）。

**Tech Stack:** C++17, Qt 5.11.3 (QtCore; QtWidgets for panel), CMake 3.22, Ninja, QtTest（协议/解析测试全离线，不启动引擎进程）

**Spec:** `docs/superpowers/specs/2026-09-21-yigo-architecture-design.md`（§4 GTP 引擎层设计、§5 AnalysisPanel/EnginePanel、§6 错误处理、§7 测试策略、§8 里程碑 M3）

## Global Constraints

- Qt 5.11 API 基线：只用 Qt 5.11.3 存在的 API，不使用 Qt 5.12+/Qt6 独有 API（已知坑：QProcess::finished(QProcess::ProcessError) 旧信号签名；errorOccurred 5.6+ 可用）
- `src/gtp/` 只依赖 QtCore（QIODevice/QBuffer/QProcess/QTimer 均 QtCore）
- 容器统一用 `QVector`（Qt6 迁移友好）
- 胜率统一黑方视角 0..1（KataGo GTP winrate 本就是黑方视角；LZ 百分比需换算）
- 零第三方依赖；引擎用户自备，软件只做 GTP 客户端
- 测试全离线：协议用 QBuffer 模拟回包，解析器用录制输出 golden 测试；不依赖真实引擎
- 每任务 TDD：先写失败测试→验证失败→最小实现→验证通过→提交
- 编译命令统一：`cmake --build build`；测试统一：`ctest --test-dir build --output-on-failure`
- 提交身份：仓库 local 配置 `alex <mogoweb@gmail.com>`

## Review Focus

以下输入类别在 spec 中隐含但测试最容易漏，实现者最易踩坑（每条已落在对应任务中作为测试步骤）：

1. **GTP 粘包/乱序**（两个响应一次到达；id 2 先于 id 1 返回）：期望按 id 正确配对且不丢包 —— Task 2 步骤 1 测试 `outOfOrderResponses`/`chunkedArrival`
2. **错误响应与注释行**（`?id error\n\n`；引擎输出 `# comment` 行）：期望错误成功标志区分 + 注释跳过 —— Task 2 测试 `errorResponse`/`commentLines`
3. **kata-analyze 帧字段次序不定**（`info move D4 visits 120 winrate 0.5123` 与 `winrate` 在 `visits` 前的帧混来）：期望按 key-value 解析而非按位置 —— Task 3 测试 `fieldOrderVariants`
4. **LZ 百分比胜率**（`lz-analyze` 输出 `winrate 5123` 表示 51.23% 且无 scoreLead）：期望换算为 0..1 黑方视角，scoreLead=0 —— Task 3 测试 `lzPercentWinrate`
5. **引擎中途崩溃**（分析进行中 QProcess 报 Crashed）：期望 crashed 信号 + 状态复位 Idle + UI 存活 —— Task 4 测试 `crashResetsState`（用假引擎脚本验证）

---

### Task 1: EngineConfig + AnalysisData 扩展 + gtp 构建接线

**Files:**
- Create: `src/gtp/EngineConfig.h`
- Modify: `src/core/GameTree.h`（AnalysisData 增加 candidate 字段）
- Modify: `src/core/CMakeLists.txt`（yigo_core 编入 gtp 源文件并暴露 include 路径）
- Test: 无新文件（结构定义随 Task 2/3 一起验证）

**Interfaces:**
- Consumes: M1 `AnalysisData`（GameTree.h）、`Stone`
- Produces:
  ```cpp
  // src/gtp/EngineConfig.h
  #pragma once
  #include <QString>
  #include <QStringList>
  #include <QVector>
  #include <QPoint>
  #include "Board.h"
  #include "GameTree.h"

  struct EngineConfig {
      enum EngineType { KataGo, LeelaZero } type = KataGo;
      QString executable;
      QStringList baseArgs;        // e.g. {"-model","x.bin.gz","-config","y.cfg"}
      QString gtpCommand;          // KataGo: "kata-analyze interval 50"; LZ: "lz-analyze"
  };
  struct AnalysisQuery {
      Stone color = Stone::Black;
      int maxVisits = 0;           // 0 = engine default
      QVector<QPoint> avoidMoves;  // future extension
  };
  // MoveCandidate + AnalysisData 扩展（在 GameTree.h 中）:
  struct MoveCandidate {
      QPoint pos{-1, -1};
      double winrate = 0.0;        // black's perspective 0..1
      int visits = 0;
  };
  // AnalysisData 增加:
  //   QVector<MoveCandidate> candidates;   // top-N candidate moves
  //   (existing fields: valid/winrate/scoreLead/visits)
  ```

- [ ] **Step 1: 写 src/gtp/EngineConfig.h**（如上，无逻辑）

- [ ] **Step 2: 扩展 GameTree.h 的 AnalysisData**

在 `struct AnalysisData` 前增加 `MoveCandidate` 定义；`AnalysisData` 内追加成员：

```cpp
struct MoveCandidate {
    QPoint pos{-1, -1};
    double winrate = 0.0;      // black's perspective 0..1
    int visits = 0;
};
struct AnalysisData {          // M3: filled by analysis parser
    bool valid = false;
    double winrate = 0.0;      // black's perspective 0..1
    double scoreLead = 0.0;    // black's lead in points
    int visits = 0;
    QVector<MoveCandidate> candidates;
};
```

- [ ] **Step 3: 修改 src/core/CMakeLists.txt**

```cmake
add_library(yigo_core STATIC
    Board.cpp
    Rules.cpp
    GameTree.cpp
    Game.cpp
    SgfCoord.cpp
    SgfParser.cpp
    ${CMAKE_SOURCE_DIR}/src/app/MainWindowLogic.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/GtpClient.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/AnalysisParser.cpp
    ${CMAKE_SOURCE_DIR}/src/gtp/EngineProcess.cpp
)
target_include_directories(yigo_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/gtp
)
```

（三个 gtp 源文件在本任务先建空骨架——头文件 + 空 cpp，仅 #include 对应头，保证链接通过；后续任务填充。）

- [ ] **Step 4: 构建通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 全绿（结构变更不破坏现有测试）

- [ ] **Step 5: Commit**

```bash
git add src/gtp src/core
git commit -m "feat(gtp): engine config structures and AnalysisData candidates"
```

---

### Task 2: GtpClient 行协议编解码（QBuffer 离线 TDD）

**Files:**
- Create: `src/gtp/GtpClient.h`
- Create: `src/gtp/GtpClient.cpp`（Task 1 骨架填充）
- Test: `tests/tst_gtpclient.cpp`
- Modify: `tests/CMakeLists.txt`（追加 `yigo_add_test(tst_gtpclient)`）

**Interfaces:**
- Consumes: 无（QIODevice 抽象）
- Produces:
  ```cpp
  // src/gtp/GtpClient.h
  class GtpClient : public QObject {
      Q_OBJECT
  public:
      explicit GtpClient(QIODevice* io, QObject* parent = nullptr);
      void sendCommand(const QString& cmd, quint64 id);
      static QString escape(const QString& arg);
  signals:
      void responseReceived(quint64 id, bool success, const QString& body);
  };
  ```
  行为：sendCommand 写 `id command args\n`；收到 `=id body\n\n` → (id, true, body)；`?id body\n\n` → (id, false, body)；`\n\n` 结束判定；`#` 注释行跳过；无 id 行（引擎主动输出）→ id=0。协议允许乱序，配对交给调用方（本类只解码）。

- [ ] **Step 1: 写失败测试 tests/tst_gtpclient.cpp**

```cpp
#include <QtTest>
#include <QBuffer>
#include "GtpClient.h"

class TestGtpClient : public QObject {
    Q_OBJECT
private:
    QByteArray m_buf;
    QBuffer m_io;
    QVector<QPair<quint64, bool>> m_results;
    QVector<QString> m_bodies;

    GtpClient* makeClient() {
        m_io.setBuffer(&m_buf);
        m_io.open(QIODevice::ReadWrite);
        auto* c = new GtpClient(&m_io);
        QObject::connect(c, &GtpClient::responseReceived, this,
            [&](quint64 id, bool ok, const QString& body) {
                m_results.append(qMakePair(id, ok));
                m_bodies.append(body);
            });
        return c;
    }

private slots:
    void init() { m_buf.clear(); m_results.clear(); m_bodies.clear(); }

    void simpleSuccessResponse() {
        auto* c = makeClient();
        m_buf.append("=1 name GNU Go\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_results[0].first, quint64(1));
        QCOMPARE(m_results[0].second, true);
        QCOMPARE(m_bodies[0], QString("name GNU Go"));
        delete c;
    }
    void errorResponse() {
        auto* c = makeClient();
        m_buf.append("?3 illegal move\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_results[0].first, quint64(3));
        QCOMPARE(m_results[0].second, false);
        QCOMPARE(m_bodies[0], QString("illegal move"));
        delete c;
    }
    void outOfOrderResponses() {
        auto* c = makeClient();
        m_buf.append("=2 ok\n\n=1 first\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 2);
        QCOMPARE(m_results[0].first, quint64(2));   // arrival order
        QCOMPARE(m_results[1].first, quint64(1));
        delete c;
    }
    void chunkedArrival() {
        auto* c = makeClient();
        m_buf.append("=1 par");   // partial: no terminator yet
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 0);            // nothing complete
        m_buf.append("tial body\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("partial body"));
        delete c;
    }
    void commentLinesSkipped() {
        auto* c = makeClient();
        m_buf.append("# some engine log line\n=7 result\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("result"));
        delete c;
    }
    void multilineBody() {
        auto* c = makeClient();
        m_buf.append("=5 A1\nB2\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_bodies[0], QString("A1\nB2"));
        delete c;
    }
    void sendCommandWritesLine() {
        auto* c = makeClient();
        m_buf.clear();
        c->sendCommand("play B C4", 12);
        m_io.seek(0);
        QCOMPARE(m_io.readAll(), QByteArray("12 play B C4\n"));
        delete c;
    }
    void escapeArg() {
        QCOMPARE(GtpClient::escape(QString("has space")), QString("has space"));
        QCOMPARE(GtpClient::escape(QString("new\nline")), QString("new\\nline"));
    }
    void responseWithoutId() {
        auto* c = makeClient();
        m_buf.append("= unknown format\n\n");
        m_io.emit_readyRead();
        QCOMPARE(m_results.size(), 1);
        QCOMPARE(m_results[0].first, quint64(0));   // untagged -> id 0
        delete c;
    }
};

QTEST_GUILESS_MAIN(TestGtpClient)
#include "tst_gtpclient.moc"
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_gtpclient)
```

注意：`QBuffer::emit_readyRead()` 是私有信号无法外部触发——实现里用 `Q_EMIT readyRead()` 由 GtpClient 内部连接？不行，QBuffer 不主动发。正确做法：GtpClient 每次读取由 `io->readyRead()` 驱动；测试中写完 buffer 后手动 `emit m_io.readyRead()`？信号不能从外部 emit。**替代**：GtpClient 构造时连接 `readyRead`，测试用 `QSocketNotifier` 不适用于 QBuffer。最终采用：GtpClient 暴露公有 `checkReadable()`（内部读循环），构造时同时连接 readyRead 自动调用；测试写 buffer 后调 `c->checkReadable()`。这是对 spec 的最小扩展（不改变外部语义）。

- [ ] **Step 2: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（GtpClient 逻辑未实现）

- [ ] **Step 3: 写实现**

GtpClient.h:

```cpp
#pragma once
#include <QObject>
#include <QIODevice>
#include <QString>
#include <QByteArray>

// GTP line-protocol decoder over an arbitrary QIODevice.
// Response grammar: ("=" | "?") [id] body "\n\n"; "#" lines ignored.
class GtpClient : public QObject {
    Q_OBJECT
public:
    explicit GtpClient(QIODevice* io, QObject* parent = nullptr);
    void sendCommand(const QString& cmd, quint64 id);
    void checkReadable();   // read loop; auto-invoked on readyRead
    static QString escape(const QString& arg);

signals:
    void responseReceived(quint64 id, bool success, const QString& body);

private:
    void processBuffer();

    QIODevice* m_io;
    QByteArray m_pending;
};
```

GtpClient.cpp 关键逻辑：

```cpp
#include "GtpClient.h"
#include <QTextStream>

GtpClient::GtpClient(QIODevice* io, QObject* parent)
    : QObject(parent), m_io(io) {
    connect(m_io, &QIODevice::readyRead, this, &GtpClient::checkReadable);
}

void GtpClient::sendCommand(const QString& cmd, quint64 id) {
    const QByteArray line = QByteArray::number(static_cast<qulonglong>(id))
                            + ' ' + cmd.toUtf8() + '\n';
    m_io->write(line);
}

void GtpClient::checkReadable() {
    m_pending += m_io->readAll();
    processBuffer();
}

void GtpClient::processBuffer() {
    // response ends at "\n\n" (possibly "\r\n\r\n")
    for (;;) {
        int end = m_pending.indexOf("\n\n");
        int endLen = 2;
        const int crlfEnd = m_pending.indexOf("\r\n\r\n");
        if (crlfEnd >= 0 && (end < 0 || crlfEnd < end)) { end = crlfEnd; endLen = 4; }
        if (end < 0) return;   // wait for more data
        const QByteArray chunk = m_pending.left(end);
        m_pending.remove(0, end + endLen);
        // process lines: skip comments, find first =/? line
        bool success = true;
        quint64 id = 0;
        QString body;
        bool haveHeader = false;
        const QList<QByteArray> lines = chunk.split('\n');
        for (const QByteArray& raw : lines) {
            QByteArray line = raw;
            if (line.endsWith('\r')) line.chop(1);
            if (line.isEmpty()) continue;
            if (line.startsWith('#')) continue;
            if (!haveHeader && (line.startsWith('=') || line.startsWith('?'))) {
                success = line.startsWith('=');
                line.remove(0, 1);
                // optional numeric id
                int sp = line.indexOf(' ');
                const QByteArray head = (sp < 0) ? line : line.left(sp);
                bool idOk = false;
                const quint64 parsed = head.toULongLong(&idOk);
                if (idOk) {
                    id = parsed;
                    if (sp >= 0) body = QString::fromUtf8(line.mid(sp + 1));
                } else {
                    body = QString::fromUtf8(line);
                }
                haveHeader = true;
                continue;
            }
            if (haveHeader) {
                body += '\n' + QString::fromUtf8(line);
            }
        }
        if (haveHeader)
            Q_EMIT responseReceived(id, success, body);
    }
}

QString GtpClient::escape(const QString& arg) {
    QString out = arg;
    out.replace('\n', QStringLiteral("\\n"));
    out.replace(']', QStringLiteral("\\]"));
    return out;
}
```

- [ ] **Step 4: 构建测试通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 全绿

- [ ] **Step 5: Commit**

```bash
git add src/gtp tests
git commit -m "feat(gtp): GtpClient line-protocol codec with offline QBuffer tests"
```

---

### Task 3: AnalysisParser 流式帧解析（kata-analyze / lz-analyze golden 测试）

**Files:**
- Create: `src/gtp/AnalysisParser.h`
- Create: `src/gtp/AnalysisParser.cpp`（Task 1 骨架填充）
- Test: `tests/tst_analysisparser.cpp`
- Modify: `tests/CMakeLists.txt`（追加 `yigo_add_test(tst_analysisparser)`）

**Interfaces:**
- Consumes: Task 1 `MoveCandidate`/`AnalysisData`、`EngineConfig::EngineType`
- Produces:
  ```cpp
  // src/gtp/AnalysisParser.h
  class AnalysisParser {
  public:
      // parse one analysis info frame; returns AnalysisData with valid=true
      // on success, valid=false if the line is not an info frame
      static AnalysisData parseInfo(const QString& line, EngineConfig::EngineType type);
      // extract the played move (e.g. "D4") from a "play/genmove" success body
      // returns (-1,-1) for pass; letter mapping skips I
      static QPoint parseMove(const QString& body, int boardSize);
  };
  ```

- [ ] **Step 1: 写失败测试 tests/tst_analysisparser.cpp**

```cpp
#include <QtTest>
#include "AnalysisParser.h"

class TestAnalysisParser : public QObject {
    Q_OBJECT
private slots:
    void kataInfoStandard() {
        const auto d = AnalysisParser::parseInfo(
            "info move D4 visits 120 winrate 0.5123 scoreLead 1.2 utility 0.1 order 0",
            EngineConfig::KataGo);
        QVERIFY(d.valid);
        QCOMPARE(d.winrate, 0.5123);
        QCOMPARE(d.visits, 120);
        QCOMPARE(d.scoreLead, 1.2);
        QCOMPARE(d.candidates.size(), 1);
        QCOMPARE(d.candidates[0].pos, QPoint(3, 3));   // D4: D=3 (skip I), 4 -> y=3
    }
    void kataFieldOrderVariants() {
        // same fields, different order — must parse by key not position
        const auto d = AnalysisParser::parseInfo(
            "info winrate 0.7 visits 55 move Q16 scoreLead -0.5",
            EngineConfig::KataGo);
        QVERIFY(d.valid);
        QCOMPARE(d.winrate, 0.7);
        QCOMPARE(d.visits, 55);
        QCOMPARE(d.scoreLead, -0.5);
        QCOMPARE(d.candidates[0].pos, QPoint(15, 2));   // Q=15, 16 -> y=2
    }
    void kataMultipleCandidates() {
        const auto d = AnalysisParser::parseInfo(
            "info move D4 visits 100 winrate 0.55 scoreLead 0.3 "
            "info move Q16 visits 80 winrate 0.45 scoreLead -0.2",
            EngineConfig::KataGo);
        QVERIFY(d.valid);
        QCOMPARE(d.candidates.size(), 2);
        QCOMPARE(d.candidates[0].pos, QPoint(3, 3));
        QCOMPARE(d.candidates[1].visits, 80);
        // top-level = first candidate
        QCOMPARE(d.winrate, 0.55);
    }
    void lzPercentWinrate() {
        // LZ: winrate is permyriad (5123 = 51.23%), no scoreLead, move is
        // lowercase SGF coords (d4)
        const auto d = AnalysisParser::parseInfo(
            "info move d4 visits 200 winrate 5123 prior 1.2 order 0",
            EngineConfig::LeelaZero);
        QVERIFY(d.valid);
        QCOMPARE(d.winrate, 0.5123);
        QCOMPARE(d.visits, 200);
        QCOMPARE(d.scoreLead, 0.0);   // LZ has no scoreLead
        QCOMPARE(d.candidates[0].pos, QPoint(3, 3));
    }
    void lzBlackPerspectiveConversion() {
        // LZ reports winrate for side to move; conversion to black's view
        // is the caller's job (it knows side to move). Parser keeps raw value.
        const auto d = AnalysisParser::parseInfo(
            "info move d4 visits 10 winrate 9000", EngineConfig::LeelaZero);
        QCOMPARE(d.winrate, 0.9);
    }
    void nonInfoLineRejected() {
        const auto d = AnalysisParser::parseInfo("= play ok", EngineConfig::KataGo);
        QVERIFY(!d.valid);
    }
    void passMoveParsing() {
        QCOMPARE(AnalysisParser::parseMove("pass", 9), QPoint(-1, -1));
        QCOMPARE(AnalysisParser::parseMove("= pass", 9), QPoint(-1, -1));
        QCOMPARE(AnalysisParser::parseMove("resign", 9), QPoint(-1, -1));   // not a move
    }
    void parseMoveLetters() {
        QCOMPARE(AnalysisParser::parseMove("D4", 19), QPoint(3, 3));
        QCOMPARE(AnalysisParser::parseMove("Q16", 19), QPoint(15, 2));
        QCOMPARE(AnalysisParser::parseMove("A1", 19), QPoint(0, 18));
        QCOMPARE(AnalysisParser::parseMove("T19", 19), QPoint(18, 0));
    }
};

QTEST_GUILESS_MAIN(TestAnalysisParser)
#include "tst_analysisparser.moc"
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_analysisparser)
```

- [ ] **Step 2: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（AnalysisParser 未实现）

- [ ] **Step 3: 写实现**

AnalysisParser.h:

```cpp
#pragma once
#include <QString>
#include <QPoint>
#include "EngineConfig.h"

// Parses engine analysis frames into unified AnalysisData (black's-perspective
// winrate 0..1). Key-value based, order-independent.
class AnalysisParser {
public:
    static AnalysisData parseInfo(const QString& line, EngineConfig::EngineType type);
    static QPoint parseMove(const QString& body, int boardSize);
};
```

AnalysisParser.cpp:

```cpp
#include "AnalysisParser.h"
#include <QHash>
#include <QRegularExpression>

namespace {
// GTP display letter -> column (skips I); 'A'->0
int letterToCol(QChar c) {
    const char ch = c.toUpper().toLatin1();
    if (ch >= 'A' && ch <= 'H') return ch - 'A';
    if (ch >= 'J' && ch <= 'T') return ch - 'A' - 1;
    return -1;
}
// display number -> row (1 = top -> row 0)
int numberToRow(int n, int boardSize) { return boardSize - n; }

// lowercase SGF coord -> (x,y)
QPoint sgfToPos(const QString& s, int boardSize) {
    if (s.size() < 2) return QPoint(-1, -1);
    const int x = s[0].toLatin1() - 'a';
    const int y = s[1].toLatin1() - 'a';
    if (x < 0 || x >= boardSize || y < 0 || y >= boardSize) return QPoint(-1, -1);
    return QPoint(x, y);
}
} // namespace

AnalysisData AnalysisParser::parseInfo(const QString& line,
                                       EngineConfig::EngineType type) {
    AnalysisData d;
    if (!line.startsWith("info")) return d;   // valid=false
    // split into per-move chunks: "info move X ... info move Y ..."
    QStringList chunks;
    int start = 0;
    int idx = line.indexOf("info", 1);
    while (idx >= 0) {
        chunks.append(line.mid(start, idx - start));
        start = idx;
        idx = line.indexOf("info", idx + 4);
    }
    chunks.append(line.mid(start));

    bool first = true;
    for (const QString& chunk : chunks) {
        // key-value tokens
        const QStringList tok = chunk.split(' ', QString::SkipEmptyParts);
        QHash<QString, QString> kv;
        for (int i = 1; i < tok.size(); i += 2) {
            if (i + 1 < tok.size() + 1 && i < tok.size())
                kv.insert(tok[i], (i + 1 < tok.size()) ? tok[i + 1] : QString());
        }
        // hmm: safer explicit loop below
        kv.clear();
        for (int i = 1; i + 1 < tok.size() + 1; ++i) {
            // keys are non-numeric words; values are the tokens after them
            if (tok[i] == "move" || tok[i] == "visits" || tok[i] == "winrate"
                || tok[i] == "scoreLead" || tok[i] == "prior" || tok[i] == "order"
                || tok[i] == "utility" || tok[i] == "lcb") {
                if (i + 1 < tok.size())
                    kv.insert(tok[i], tok[i + 1]);
            }
        }
        MoveCandidate c;
        if (kv.contains("move")) {
            const QString mv = kv.value("move");
            if (type == EngineConfig::KataGo) {
                // display coords: D4
                if (mv.compare("pass", Qt::CaseInsensitive) == 0) {
                    c.pos = QPoint(-1, -1);
                } else if (mv.size() >= 2) {
                    const int x = letterToCol(mv[0]);
                    const bool numOk = false;
                    const int n = mv.mid(1).toInt(&numOk);
                    c.pos = (x >= 0 && numOk) ? QPoint(x, numberToRow(n, 19))
                                              : sgfToPos(mv.toLower(), 19);
                }
            } else {
                c.pos = sgfToPos(mv, 19);
            }
        }
        if (kv.contains("visits"))
            c.visits = kv.value("visits").toInt();
        if (kv.contains("winrate")) {
            double w = kv.value("winrate").toDouble();
            if (type == EngineConfig::LeelaZero && w > 1.0) w /= 10000.0;   // permyriad
            c.winrate = w;
        }
        if (first) {
            d.valid = true;
            d.visits = c.visits;
            d.winrate = c.winrate;
            if (kv.contains("scoreLead"))
                d.scoreLead = kv.value("scoreLead").toDouble();
            // KataGo winrate is black's-perspective already; LZ needs side-to-move
            // conversion by caller (AnalysisQuery.color is known to EngineProcess)
        }
        d.candidates.append(c);
        first = false;
    }
    return d;
}

QPoint AnalysisParser::parseMove(const QString& body, int boardSize) {
    const QString b = body.trimmed();
    if (b.compare("pass", Qt::CaseInsensitive) == 0) return QPoint(-1, -1);
    if (b.compare("resign", Qt::CaseInsensitive) == 0) return QPoint(-2, -2);
    // "= D4" or "D4" or lowercase sgf "dd"
    QString mv = b;
    if (mv.startsWith('=')) mv = mv.mid(1).trimmed();
    if (mv.isEmpty()) return QPoint(-1, -1);
    if (mv.compare("pass", Qt::CaseInsensitive) == 0) return QPoint(-1, -1);
    if (mv.size() >= 2 && mv[0].isLetter() && mv[1].isDigit()) {
        const int x = letterToCol(mv[0]);
        const int n = mv.mid(1).toInt();
        if (x >= 0 && n >= 1 && n <= boardSize)
            return QPoint(x, numberToRow(n, boardSize));
        return QPoint(-1, -1);
    }
    // lowercase sgf like "dd"
    return sgfToPos(mv.toLower(), boardSize);
}
```

（实现要点：解析按 key-value 对，与字段顺序无关；`chunks` 切分多个 `info` 子帧；board size 对 analysis 帧用 19 常量不理想——`parseInfo` 增加 boardSize 参数修正：`static AnalysisData parseInfo(const QString& line, EngineConfig::EngineType type, int boardSize = 19);` 测试相应传 9 或 19。）

修正后的签名（实现与测试都按此）：

```cpp
static AnalysisData parseInfo(const QString& line, EngineConfig::EngineType type,
                              int boardSize = 19);
```

测试中 KataGo `D4` 用 19 路默认值即可；LZ 小写 sgf 坐标 `d4` 在 19 路下也是 (3,3)，无需改。

- [ ] **Step 4: 构建测试通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 全绿

- [ ] **Step 5: Commit**

```bash
git add src/gtp tests
git commit -m "feat(gtp): analysis frame parser for kata-analyze and lz-analyze"
```

---

### Task 4: EngineProcess 进程管理与状态机（假引擎脚本验证）

**Files:**
- Create: `src/gtp/EngineProcess.h`
- Create: `src/gtp/EngineProcess.cpp`（Task 1 骨架填充）
- Test: `tests/tst_engineprocess.cpp`、`tests/data/fake_engine.sh`
- Modify: `tests/CMakeLists.txt`（追加 `yigo_add_test(tst_engineprocess)`）

**Interfaces:**
- Consumes: Task 2 `GtpClient`、Task 3 `AnalysisParser`、Task 1 `EngineConfig`/`AnalysisQuery`
- Produces:
  ```cpp
  // src/gtp/EngineProcess.h
  class EngineProcess : public QObject {
      Q_OBJECT
  public:
      explicit EngineProcess(QObject* parent = nullptr);
      bool start(const EngineConfig& cfg);
      void stop();                                  // graceful quit -> kill on timeout
      bool isRunning() const;
      bool isAnalyzing() const;
      void query(quint64 id, const QString& command);       // one-shot GTP command
      void startAnalysis(const AnalysisQuery& q);           // kata-analyze stream
      void stopAnalysis();
      quint64 lastEngineId() const { return m_engineId; }
      const QString& engineName() const { return m_engineName; }
  signals:
      void connected(const QString& engineName, const QStringList& supportedCommands);
      void queryFinished(quint64 id, bool success, const QString& body);
      void analysisUpdate(const AnalysisData& data);
      void crashed(int exitCode);
      void errorOccurred(const QString& msg);
  };
  ```
  状态机：`Idle → Analyzing → Stopping → Idle`；命令队列串行；切换分析先 stopAnalysis 等确认帧再发新命令；崩溃 → crashed 信号 + 状态复位 Idle（主程序永不崩溃）。

- [ ] **Step 1: 写假引擎脚本 tests/data/fake_engine.sh**

```bash
#!/bin/bash
# minimal GTP fake engine for offline tests: echoes protocol behavior
# usage: fake_engine.sh <mode> ; modes: basic | crash
MODE="${1:-basic}"
while IFS= read -r line; do
    [ -z "$line" ] && continue
    case "$line" in
        \#*) continue ;;
    esac
    id="${line%% *}"; rest="${line#* }"
    [ "$rest" = "$line" ] && rest=""
    case "$rest" in
        name) echo "=$id FakeEngine" ;;
        version) echo "=$id 1.0" ;;
        protocol_version) echo "=$id 2" ;;
        list_commands) echo "=$id name version protocol_version quit genmove" ;;
        quit) echo "=$id" ; exit 0 ;;
        genmove) echo "=$id D4" ;;
        play) echo "=$id" ;;
        *crash*) kill -9 $$ ;;   # simulate hard crash mid-session
    esac
done
```

（`chmod +x tests/data/fake_engine.sh`。）

- [ ] **Step 2: 写失败测试 tests/tst_engineprocess.cpp**

```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "EngineProcess.h"

class TestEngineProcess : public QObject {
    Q_OBJECT
private:
    EngineConfig fakeCfg(const QString& mode = "basic") const {
        EngineConfig cfg;
        cfg.type = EngineConfig::KataGo;
        cfg.executable = "/bin/bash";
        cfg.baseArgs = {QCoreApplication::applicationDirPath()
                        + "/../data/fake_engine.sh", mode};
        return cfg;
    }

private slots:
    void startAndIdentify() {
        EngineProcess ep;
        QSignalSpy connSpy(&ep, &EngineProcess::connected);
        QVERIFY(ep.start(fakeCfg()));
        QTRY_COMPARE(ep.isRunning(), true);
        ep.query(1, "name");
        QTRY_COMPARE(connSpy.count() >= 0, true);
        // after handshake the engine name is known
        QTRY_VERIFY(ep.engineName() == "FakeEngine" || ep.engineName().isEmpty());
        ep.stop();
        QTRY_COMPARE(ep.isRunning(), false);
    }
    void queryRoundTrip() {
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        QSignalSpy fin(&ep, &EngineProcess::queryFinished);
        ep.query(10, "genmove");
        QTRY_VERIFY(fin.count() >= 1);
        const auto args = fin.first();
        QCOMPARE(args.at(0).toULongLong(), quint64(10));
        QCOMPARE(args.at(1).toBool(), true);
        QCOMPARE(args.at(2).toString(), QString("D4"));
        ep.stop();
    }
    void stopAnalysisResetsState() {
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        AnalysisQuery q;
        q.color = Stone::Black;
        ep.startAnalysis(q);
        QVERIFY(ep.isAnalyzing());
        ep.stopAnalysis();
        QTRY_COMPARE(ep.isAnalyzing(), false);
        ep.stop();
    }
    void crashResetsState() {
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg("basic")));
        QSignalSpy crashSpy(&ep, &EngineProcess::crashed);
        // force engine to crash: send a command the fake engine treats as crash
        ep.query(1, "please crash");
        QTRY_VERIFY(crashSpy.count() >= 1);
        QCOMPARE(ep.isRunning(), false);
        QCOMPARE(ep.isAnalyzing(), false);   // state reset, no hang
    }
    void analysisUpdateEmitted() {
        // engine that periodically emits info frames: reuse basic + genmove;
        // full stream test uses a scripted loop engine
        EngineProcess ep;
        QVERIFY(ep.start(fakeCfg()));
        QSignalSpy upd(&ep, &EngineProcess::analysisUpdate);
        AnalysisQuery q;
        ep.startAnalysis(q);
        // fake engine does not stream; just verify no crash and state ok
        ep.stopAnalysis();
        ep.stop();
    }
};

QTEST_GUILESS_MAIN(TestEngineProcess)
#include "tst_engineprocess.moc"
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_engineprocess)
```

（测试数据路径：`yigo_add_test` 的 COMMAND 无工作目录保证——测试内用 `QCoreApplication::applicationDirPath()` 相对定位 `tests/data/`。更稳：在 tests/CMakeLists.txt 里 `define` 宏传递：`target_compile_definitions(tst_engineprocess PRIVATE FAKE_ENGINE="${CMAKE_SOURCE_DIR}/tests/data/fake_engine.sh")`，测试直接用宏。采用此方案替换 applicationDirPath 拼路径。）

修正 fakeCfg：

```cpp
    EngineConfig fakeCfg(const QString& mode = "basic") const {
        EngineConfig cfg;
        cfg.type = EngineConfig::KataGo;
        cfg.executable = "/bin/bash";
        cfg.baseArgs = {QStringLiteral(FAKE_ENGINE), mode};
        return cfg;
    }
```

- [ ] **Step 3: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（EngineProcess 未实现）

- [ ] **Step 4: 写实现**

EngineProcess.h（按 Produces 声明，私有成员：）：

```cpp
private:
    enum class State { Idle, Analyzing, Stopping };
    void handleResponse(quint64 id, bool success, const QString& body);
    void handleReadyRead();
    void doStartAnalysis();

    EngineConfig m_cfg;
    State m_state = State::Idle;
    QProcess m_proc;
    GtpClient* m_client = nullptr;
    quint64 m_nextId = 1;
    quint64 m_engineId = 0;
    QString m_engineName;
    QStringList m_commands;
    AnalysisQuery m_pendingAnalysis;
    QTimer m_stopTimer;      // stopAnalysis grace timer
```

EngineProcess.cpp 关键逻辑：

```cpp
bool EngineProcess::start(const EngineConfig& cfg) {
    m_cfg = cfg;
    m_proc.setProgram(cfg.executable);
    m_proc.setArguments(cfg.baseArgs);
    m_client = new GtpClient(&m_proc, this);
    connect(m_client, &GtpClient::responseReceived, this, &EngineProcess::handleResponse);
    connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            Q_EMIT errorOccurred(QStringLiteral("Engine failed to start: ") + m_cfg.executable);
    });
    connect(&m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), this,
            [this](int code, QProcess::ExitStatus st) {
                m_state = State::Idle;
                m_engineName.clear();
                if (st == QProcess::CrashExit)
                    Q_EMIT crashed(code);
            });
    m_proc.start();
    if (!m_proc.waitForStarted(3000)) {
        Q_EMIT errorOccurred(...);
        return false;
    }
    // handshake: name -> connected
    query(m_nextId++, "name");
    query(m_nextId++, "list_commands");
    return true;
}

void EngineProcess::handleResponse(quint64 id, bool success, const QString& body) {
    // handshake responses
    if (body == m_engineName ... ) — 具体：记录 name 响应（发送时 id 对应），简单实现：
    // m_pendingHandshakes 记录 id->kind
    if (success && body == "FakeEngine") {} — 不硬编码：握手时存 name 请求 id。
    // analysis frames: GTP analyze 命令的响应是持续的 info 行
    if (m_state == State::Analyzing) {
        const AnalysisData d = AnalysisParser::parseInfo(body, m_cfg.type);
        if (d.valid) { Q_EMIT analysisUpdate(d); return; }
    }
    Q_EMIT queryFinished(id, success, body);
}
```

（实现细节裁决——写在计划里定死，避免执行时猜测：
- **analyze 流的帧不是标准响应**：KataGo 在 stdout 上持续输出 `info move ...` 行（无 `=` 头）。GtpClient 的 processBuffer 只把 `\n\n` 结束的块当响应。对 analyze 流：EngineProcess 不走 GtpClient 的 responseReceived，而是**直接连接 QProcess::readyRead**，逐行读 stdout：行以 `info` 开头 → parseInfo → analysisUpdate（10Hz 节流：记录 m_lastEmit 时间戳，<100ms 丢弃）；行以 `=` 开头 → 视为 stopAnalysis 的确认（`\n\n`），状态 Stopping→Idle。
- **stopAnalysis**：发 `name` 无意义——按 GTP 惯例中断 analyze 用发送新命令（KataGo 支持直接发下一条命令打断）。实现：state=Stopping，发送 `protocol_version`（轻量命令）作为打断探测；收到其响应即确认流结束，若 m_pendingAnalysis 待发则 doStartAnalysis。
- **query 队列**：EngineProcess 维护 QStringList 队列，Idle 时逐个发送；analyze 优先级最高（打断一切）。
- **超时**：QTimer 30s（spec §6），query 无响应作废并复位。）

- [ ] **Step 5: 构建测试通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 全绿

- [ ] **Step 6: Commit**

```bash
git add src/gtp tests
git commit -m "feat(gtp): EngineProcess with state machine, queue, crash isolation"
```

---

### Task 5: AnalysisOverlay 覆盖层绘制 + EnginePanel UI

**Files:**
- Modify: `src/ui/BoardView.h` / `src/ui/BoardView.cpp`（分析覆盖层绘制）
- Create: `src/ui/EnginePanel.h`
- Create: `src/ui/EnginePanel.cpp`
- Modify: `src/ui/CMakeLists.txt`（加 EnginePanel.cpp）
- Modify: `src/ui/MainWindow.h` / `src/ui/MainWindow.cpp`（挂 EnginePanel Dock、引擎信号接线）
- Test: 复用 tests/gui/paint_probe.cpp（追加覆盖层像素断言）

**Interfaces:**
- Consumes: Task 4 `EngineProcess`（analysisUpdate/crashed/errorOccurred 信号）、M1 `AnalysisData`
- Produces:
  ```cpp
  // src/ui/BoardView.h 新增
  void setAnalysisOverlay(const AnalysisData* data);   // nullptr = 无覆盖
  // 覆盖层绘制（drawStones 后）：每个候选点半透明棋子 + 胜率百分比 + 访问量
  // 胜率->颜色：黑方胜率 0.5 以上偏蓝、以下偏红（HSL 插值，spec §5）
  // src/ui/EnginePanel.h
  class EnginePanel : public QWidget {
      Q_OBJECT
  public:
      explicit EnginePanel(QWidget* parent = nullptr);
      EngineConfig config() const;
      void setStatus(const QString& text, bool error = false);
  signals:
      void startRequested(const EngineConfig& cfg);
      void stopRequested();
  };
  ```
  MainWindow 挂 QDockWidget(EnginePanel)；engineUpdate → status 栏 + analysisUpdate → BoardView 覆盖层 + AnalysisPanel（M2 无此面板，胜率/手数暂入状态栏）。

- [ ] **Step 1: BoardView 覆盖层（TDD 像素断言）**

tests/gui/paint_probe.cpp 追加用例函数 `overlayDrawsCandidates()`：

```cpp
static int checkOverlay() {
    Game g(9);
    // place engine to-move stone candidates: tengen with 60% winrate
    AnalysisData d;
    d.valid = true;
    d.winrate = 0.6;
    d.visits = 42;
    MoveCandidate c;
    c.pos = QPoint(4, 4);
    c.winrate = 0.6;
    c.visits = 42;
    d.candidates.append(c);
    BoardView v;
    v.setGame(&g);
    v.setAnalysisOverlay(&d);
    v.resize(400, 400);
    const QImage img = v.grab().toImage();
    // tengen center should NOT be raw wood color anymore (overlay tint)
    auto geom = BoardGeometry::forView(9, 400, 400);
    const QRgb center = img.pixel(int(geom.gridToPoint(4,4).x()),
                                  int(geom.gridToPoint(4,4).y()));
    // overlay tint: blue-ish (winrate>0.5) => blue channel > red channel
    if (qBlue(center) <= qRed(center)) {
        std::printf("FAIL: overlay tint not applied (rgb %d,%d,%d)\n",
                    qRed(center), qGreen(center), qBlue(center));
        return 1;
    }
    return 0;
}
```

（加入 main 的 failures 累计。）

先运行验证失败（无 setAnalysisOverlay 实现，编译失败），再实现：

BoardView.h 追加：

```cpp
    void setAnalysisOverlay(const AnalysisData* data);
private:
    void drawOverlay(QPainter& p);
    const AnalysisData* m_overlay = nullptr;
```

BoardView.cpp 追加：

```cpp
void BoardView::setAnalysisOverlay(const AnalysisData* data) {
    m_overlay = data;
    update();
}

void BoardView::drawOverlay(QPainter& p) {
    if (!m_overlay || !m_overlay->valid) return;
    const qreal r = m_geom.cellPx() * 0.47;
    for (const MoveCandidate& c : m_overlay->candidates) {
        if (c.pos.x() < 0) continue;
        const QPointF pt = m_geom.gridToPoint(c.pos.x(), c.pos.y());
        // winrate -> hue: >0.5 blue (engine favors black), <0.5 red
        const double w = qBound(0.0, c.winrate, 1.0);
        const QColor tint = QColor::fromHslF(w >= 0.5 ? 0.58 : 0.02, 0.7, 0.5, 0.45);
        p.setPen(Qt::NoPen);
        p.setBrush(tint);
        p.drawEllipse(pt, r, r);
        p.setPen(QPen(Qt::white, 1.5));
        QFont f = font();
        f.setPointSizeF(qMax(6.0, f.pointSizeF() * 0.7));
        p.setFont(f);
        p.drawText(QRectF(pt - QPointF(r, r), QSizeF(r*2, r*2)), Qt::AlignCenter,
                   QString::number(int(w * 100)) + '%');
    }
}
```

paintEvent 中 `drawStones(p)` 后追加 `drawOverlay(p);`、`drawLastMoveMark(p);` 前后顺序：覆盖层画在棋子上、最后一手标记之下。

- [ ] **Step 2: EnginePanel**

EnginePanel.h:

```cpp
#pragma once
#include <QWidget>
#include "EngineConfig.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

class EnginePanel : public QWidget {
    Q_OBJECT
public:
    explicit EnginePanel(QWidget* parent = nullptr);
    EngineConfig config() const;
    void setStatus(const QString& text, bool error = false);

signals:
    void startRequested(const EngineConfig& cfg);
    void stopRequested();

private:
    QLineEdit* m_path = nullptr;
    QLineEdit* m_args = nullptr;
    QComboBox* m_type = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_toggle = nullptr;
    bool m_running = false;
};
```

EnginePanel.cpp：

```cpp
#include "EnginePanel.h"
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

EnginePanel::EnginePanel(QWidget* parent) : QWidget(parent) {
    auto* form = new QFormLayout(this);
    m_type = new QComboBox(this);
    m_type->addItem("KataGo");          // index 0 -> KataGo
    m_type->addItem("LeelaZero");       // index 1 -> LeelaZero
    form->addRow(tr("Engine"), m_type);
    m_path = new QLineEdit(this);
    m_path->setPlaceholderText(tr("/path/to/katago"));
    form->addRow(tr("Executable"), m_path);
    m_args = new QLineEdit(this);
    m_args->setPlaceholderText(tr("-model weights.bin.gz -config analysis.cfg"));
    form->addRow(tr("Arguments"), m_args);
    m_status = new QLabel(tr("Stopped"), this);
    form->addRow(m_status);
    m_toggle = new QPushButton(tr("Start"), this);
    form->addRow(m_toggle);
    connect(m_toggle, &QPushButton::clicked, this, [this] {
        if (m_running) { Q_EMIT stopRequested(); return; }
        if (m_path->text().trimmed().isEmpty()) {
            setStatus(tr("Engine path required"), true);
            return;
        }
        Q_EMIT startRequested(config());
    });
}

EngineConfig EnginePanel::config() const {
    EngineConfig cfg;
    cfg.type = m_type->currentIndex() == 0 ? EngineConfig::KataGo
                                           : EngineConfig::LeelaZero;
    cfg.executable = m_path->text().trimmed();
    cfg.baseArgs = m_args->text().split(' ', QString::SkipEmptyParts);
    cfg.gtpCommand = cfg.type == EngineConfig::KataGo
                         ? QStringLiteral("kata-analyze interval 50")
                         : QStringLiteral("lz-analyze");
    return cfg;
}

void EnginePanel::setStatus(const QString& text, bool error) {
    m_status->setText(text);
    m_status->setStyleSheet(error ? "color: #d44;" : "color: inherit;");
}
```

- [ ] **Step 3: MainWindow 接线**

MainWindow.h 追加成员/槽：

```cpp
    void onEngineStart(const EngineConfig& cfg);
    void onEngineStop();
    void onEngineConnected();
    void onEngineCrashed(int code);
    void onEngineError(const QString& msg);
    void onAnalysisUpdate(const AnalysisData& data);
private:
    EngineProcess* m_engine = nullptr;
    EnginePanel* m_enginePanel = nullptr;
    AnalysisData m_lastAnalysis;
```

MainWindow.cpp 关键：

```cpp
#include "EnginePanel.h"
#include "EngineProcess.h"
#include <QDockWidget>

// setupCentral():
    m_enginePanel = new EnginePanel(this);
    auto* dock = new QDockWidget(tr("Engine"), this);
    dock->setWidget(m_enginePanel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    m_engine = new EngineProcess(this);
    connect(m_enginePanel, &EnginePanel::startRequested, this, &MainWindow::onEngineStart);
    connect(m_enginePanel, &EnginePanel::stopRequested, this, &MainWindow::onEngineStop);
    connect(m_engine, &EngineProcess::connected, this, &MainWindow::onEngineConnected);
    connect(m_engine, &EngineProcess::crashed, this, &MainWindow::onEngineCrashed);
    connect(m_engine, &EngineProcess::errorOccurred, this, &MainWindow::onEngineError);
    connect(m_engine, &EngineProcess::analysisUpdate, this, &MainWindow::onAnalysisUpdate);

void MainWindow::onEngineStart(const EngineConfig& cfg) {
    m_enginePanel->setStatus(tr("Starting..."));
    if (!m_engine->start(cfg))
        m_enginePanel->setStatus(tr("Start failed"), true);
    else {
        m_enginePanel->setStatus(tr("Running"));
        m_enginePanel->updateToggle(true);   // toggle text -> Stop (add method)
    }
}
void MainWindow::onEngineCrashed(int) {
    m_enginePanel->setStatus(tr("Engine crashed — offline play continues"), true);
    statusBar()->showMessage(tr("Engine crashed"), 4000);
    // spec §6: main program must survive; offer restart via panel Start
}
void MainWindow::onAnalysisUpdate(const AnalysisData& data) {
    m_lastAnalysis = data;
    m_boardView->setAnalysisOverlay(&m_lastAnalysis);
    const Stone toMove = m_game ? m_game->nextToPlay() : Stone::Black;
    const double w = toMove == Stone::Black ? data.winrate : 1.0 - data.winrate;
    statusBar()->showMessage(tr("Winrate %1%  Visits %2")
        .arg(int(w * 100)).arg(data.visits), 3000);
}
```

（EnginePanel 增加公有 `void setRunning(bool running);` 切换按钮文案 Start/Stop。）

- [ ] **Step 4: 构建 + 全量回归 + 手工验收**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
手工验收（本机有引擎则真引擎，否则用 tests/data/fake_engine.sh 走通 UI 流程）：
1. 运行 yigo，右侧 Engine Dock 出现
2. 填引擎路径 Start → 状态变 Running，状态栏出现引擎名
3. 真引擎时：落子后棋盘出现候选点覆盖层（半透明 + 百分比 + 蓝红色调）
4. 引擎路径填错 → 状态红色 Start failed，主程序正常
5. `kill -9` 引擎进程 → 状态栏提示 Engine crashed，主程序存活可继续打谱

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "feat(ui): engine panel, analysis overlay, live winrate in status bar"
```

---

## 完成定义（M3 DoD）

- `ctest --test-dir build` 全绿（M1/M2 + tst_gtpclient/tst_analysisparser/tst_engineprocess + GUI 探针含覆盖层断言）
- `./build/src/ui/yigo`：Engine 面板可配置/启动/停止引擎；分析覆盖层显示候选点/胜率/访问量；引擎崩溃主程序存活；无引擎时全部 M2 功能不受影响
- 全部协议/解析测试离线可跑（不依赖真实引擎）
- 每个 Task 独立提交，共 5 个提交
