# YiGo M2（最小 UI）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 主窗口 + BoardView 自绘棋盘（HiDPI）+ 落子/悔棋/虚手 + SGF 打开保存，交付可运行 GUI 程序 `yigo`。

**Architecture:** 新增 `src/ui/`（Widgets 层）、`src/app/`（可单测的点击/按键纯逻辑）与顶层 `main.cpp`。`BoardView` 是无业务状态的 QWidget：点击发 `boardClicked` 信号 → `MainWindow` 槽 → `MainWindowLogic::handleBoardClick` → `Game::play/undo` → 重绘。坐标换算（屏幕像素 ↔ 棋盘格）抽成纯函数 `BoardGeometry`，可脱离 GUI 单测。core/ 仅一处明确修改：`Game::play` 支持虚手 `(-1,-1)`（SGF 解析与 UI 虚手都需要）。

**Tech Stack:** C++17, Qt 5.11.3 (QtCore + QtWidgets + Gui), CMake 3.22, Ninja, QtTest（纯函数测试用 `QTEST_GUILESS_MAIN` 不启动 GUI）

**Spec:** `docs/superpowers/specs/2026-09-21-yigo-architecture-design.md`（§5 UI 层设计、§6 错误处理、§7 测试策略、§8 里程碑 M2）

## Global Constraints

- Qt 5.11 API 基线：只用 Qt 5.11.3 存在的 API，不使用 Qt 5.12+/Qt6 独有 API（已知坑：`QVector::swapItemsAt`、`qHash(QPoint)`、`QMouseEvent::position()` 均为 5.12+/5.14+，不可用；5.11 用 `localPos()`）
- 容器统一用 `QVector`（Qt6 迁移友好）
- `src/core/` 保持 QtCore-only；`src/app/MainWindowLogic` 亦 QtCore-only（编入 yigo_core 以便无 GUI 单测）
- UI 面板之间不直接互调；M2 只有 MainWindow + BoardView，经 MainWindow 转接
- HiDPI：所有绘制尺寸基于 `devicePixelRatioF()`，`qreal` 抗锯齿；整数倍缩放为验收基线
- 落子非法（占据/自杀/劫）不出 modal，状态栏短暂提示（spec §6）
- 快捷键：方向键前后跳转、Home/End 首末、P 虚手、Ctrl+Z 悔棋、Ctrl+O 打开、Ctrl+S 保存
- 每任务 TDD：先写失败测试→验证失败→最小实现→验证通过→提交
- 编译命令统一：`cmake --build build`；测试统一：`ctest --test-dir build --output-on-failure`
- 提交身份：仓库 local 配置 `alex <mogoweb@gmail.com>`（已配置）

## Review Focus

以下输入类别在 spec 中隐含但无自动化 UI 测试覆盖，实现者最易踩坑（每条已落在对应任务中作为测试步骤或手工验收步骤）：

1. **坐标换算边界**（点击交点半径外/棋盘外像素）：期望落点吸附到最近交叉点（吸附半径 = 半格），超出忽略 —— Task 2 步骤 1 测试 `pointToGridSnaps`/`pointToGridBounds`
2. **非法落子不弹窗**（占据点重复落子）：期望状态栏提示且盘面/手数不变 —— Task 4 步骤 1 测试 `illegalClickHints`；劫点重复落子提示列入 Task 4 手工验收
3. **HiDPI 整数倍缩放**（devicePixelRatio=2）：期望棋盘绘制不模糊、点击坐标换算正确 —— Task 4 手工验收步骤 6
4. **SGF 往返经 UI**（打开→另存→再打开）：期望子数/手数/规则元数据一致 —— Task 5 手工验收步骤 2-3
5. **空文件/畸形 SGF 经 UI 打开**：期望错误对话框而非崩溃 —— Task 5 手工验收步骤 4-5

---

### Task 1: GUI 构建骨架 + main.cpp + 空主窗口

**Files:**
- Create: `src/main.cpp`
- Create: `src/ui/CMakeLists.txt`
- Create: `src/ui/MainWindow.h`
- Create: `src/ui/MainWindow.cpp`
- Modify: `CMakeLists.txt`（find_package 增加 Widgets；追加 `add_subdirectory(src/ui)`）

**Interfaces:**
- Consumes: 无（首个 GUI 任务）
- Produces:
  ```cpp
  // src/ui/MainWindow.h
  class MainWindow : public QMainWindow {
      Q_OBJECT
  public:
      explicit MainWindow(QWidget* parent = nullptr);
  };
  ```
  可执行目标 `yigo`（链接 yigo_core + Qt5::Widgets）。

- [ ] **Step 1: 修改顶层 CMakeLists.txt**

将 `find_package(Qt5 5.11 REQUIRED COMPONENTS Core Test)` 替换为：

```cmake
find_package(Qt5 5.11 REQUIRED COMPONENTS Core Test Widgets)
```

在 `add_subdirectory(src/tools)` 之后、`enable_testing()` 之前追加：

```cmake
add_subdirectory(src/ui)
```

- [ ] **Step 2: 写 src/ui/CMakeLists.txt**

```cmake
add_executable(yigo
    ../main.cpp
    MainWindow.cpp
)
target_include_directories(yigo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(yigo PRIVATE yigo_core Qt5::Widgets)
```

- [ ] **Step 3: 写 main.cpp**

```cpp
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("YiGo");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("YiGo");
    MainWindow w;
    w.show();
    return app.exec();
}
```

- [ ] **Step 4: 写空 MainWindow**

MainWindow.h:

```cpp
#pragma once
#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
};
```

MainWindow.cpp:

```cpp
#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("YiGo 弈境"));
    resize(800, 600);
    statusBar()->showMessage(tr("Ready"));
}
```

（M2 不强制 800x600 为最小尺寸——BoardView 需要自适应，Task 4 再定。）

- [ ] **Step 5: 构建 + 运行验证**

```bash
cmake --build build && timeout 3 ./build/src/ui/yigo; echo "exit=$?"
```
Expected: 编译通过；3 秒后 timeout 退出（exit=124 属正常），无段错误。

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/main.cpp src/ui
git commit -m "feat(ui): GUI skeleton with empty MainWindow"
```

---

### Task 2: BoardGeometry 坐标换算纯函数（TDD）

**Files:**
- Create: `src/ui/BoardGeometry.h`
- Create: `src/ui/BoardGeometry.cpp`
- Test: `tests/tst_boardgeometry.cpp`
- Modify: `tests/CMakeLists.txt`（追加 `yigo_add_test(tst_boardgeometry)`）

**Interfaces:**
- Consumes: 无外部依赖（QPointF/QChar 均 QtCore）
- Produces:
  ```cpp
  // src/ui/BoardGeometry.h
  struct BoardGeometry {
      BoardGeometry(int boardSize, qreal cellPx);
      qreal cellPx() const;          // 单格边长（逻辑像素）
      qreal boardPx() const;         // 含边距总边长 = (size+1)*cellPx
      QPointF gridToPoint(int x, int y) const;   // 交叉点 -> 绘制坐标
      int pointToGrid(QPointF p) const;          // 像素 x -> 交叉点；越界/未吸附返回 -1
      int pointToGridY(QPointF p) const;
      static int displayXToGrid(QChar letter);   // 显示字母 A..T（跳 I）-> 0..；非法返回 -1
      static QChar gridToDisplayX(int x);        // 逆向；越界返回空 QChar
  };
  ```
  注意：`BoardGeometry` 仅用 QtCore 类型，测试可用 `QTEST_GUILESS_MAIN`；头文件后续会被 `src/ui/BoardView` 与 `tests/` 共同 include，不得引入任何 Widgets 头。

- [ ] **Step 1: 写失败测试 tests/tst_boardgeometry.cpp**

```cpp
#include <QtTest>
#include <QPointF>
#include "BoardGeometry.h"

class TestBoardGeometry : public QObject {
    Q_OBJECT
private slots:
    void gridToPoint() {
        BoardGeometry g(19, 30.0);
        // (0,0) is the top-left intersection, 1-cell margin
        QPointF p = g.gridToPoint(0, 0);
        QCOMPARE(p.x(), 30.0);
        QCOMPARE(p.y(), 30.0);
        QPointF p18 = g.gridToPoint(18, 18);
        QCOMPARE(p18.x(), 30.0 + 18 * 30.0);
        QCOMPARE(p18.y(), 30.0 + 18 * 30.0);
    }
    void pointToGridSnaps() {
        BoardGeometry g(19, 30.0);
        // snap radius: within half a cell of the intersection
        QCOMPARE(g.pointToGrid(QPointF(30.0, 30.0)), 0);          // exact
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 14.0, 30.0)), 0);   // 14px off -> snap
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 16.0, 30.0)), -1);  // 16px off -> ignore
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 18 * 30.0, 30.0)), 18);
    }
    void pointToGridBounds() {
        BoardGeometry g(9, 30.0);
        QCOMPARE(g.pointToGrid(QPointF(-5.0, 30.0)), -1);           // outside
        QCOMPARE(g.pointToGrid(QPointF(30.0 + 10 * 30.0, 30.0)), -1);
        QCOMPARE(g.pointToGridY(QPointF(30.0, -5.0)), -1);
    }
    void displayLetters() {
        QCOMPARE(BoardGeometry::displayXToGrid('A'), 0);
        QCOMPARE(BoardGeometry::displayXToGrid('H'), 7);
        QCOMPARE(BoardGeometry::displayXToGrid('J'), 8);   // skips I
        QCOMPARE(BoardGeometry::displayXToGrid('T'), 18);
        QCOMPARE(BoardGeometry::displayXToGrid('a'), 0);   // lowercase ok
        QCOMPARE(BoardGeometry::displayXToGrid('I'), -1);  // no I
        QCOMPARE(BoardGeometry::displayXToGrid('U'), -1);
        QCOMPARE(BoardGeometry::gridToDisplayX(0), QChar('A'));
        QCOMPARE(BoardGeometry::gridToDisplayX(8), QChar('J'));
        QCOMPARE(BoardGeometry::gridToDisplayX(18), QChar('T'));
        QVERIFY(BoardGeometry::gridToDisplayX(-1).isNull());
        QVERIFY(BoardGeometry::gridToDisplayX(25).isNull());
    }
    void boardPx() {
        BoardGeometry g(19, 30.0);
        // 19 cells + 1-cell margin on each side = 21 cells
        QCOMPARE(g.boardPx(), 21 * 30.0);
    }
};

QTEST_GUILESS_MAIN(TestBoardGeometry)
#include "tst_boardgeometry.moc"
```

- [ ] **Step 2: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（BoardGeometry.h 不存在）

- [ ] **Step 3: 写最小实现**

BoardGeometry.h:

```cpp
#pragma once
#include <QChar>
#include <QPointF>

// Board <-> pixel geometry; pure QtCore, unit-testable without a GUI.
struct BoardGeometry {
    BoardGeometry(int boardSize, qreal cellPx);
    qreal cellPx() const { return m_cellPx; }
    qreal boardPx() const;                       // size cells + 1-cell margin each side
    QPointF gridToPoint(int x, int y) const;
    int pointToGrid(QPointF p) const;            // -1 if outside snap radius
    int pointToGridY(QPointF p) const;
    static int displayXToGrid(QChar letter);     // display letters skip 'I'
    static QChar gridToDisplayX(int x);

private:
    int m_size;
    qreal m_cellPx;
};
```

BoardGeometry.cpp:

```cpp
#include "BoardGeometry.h"

BoardGeometry::BoardGeometry(int boardSize, qreal cellPx)
    : m_size(boardSize), m_cellPx(cellPx) {}

qreal BoardGeometry::boardPx() const {
    return (m_size + 1) * m_cellPx;
}

QPointF BoardGeometry::gridToPoint(int x, int y) const {
    return QPointF((x + 1) * m_cellPx, (y + 1) * m_cellPx);
}

int BoardGeometry::pointToGrid(QPointF p) const {
    const qreal v = p.x() / m_cellPx - 1.0;
    const int g = qRound(v);
    if (g < 0 || g >= m_size) return -1;
    if (qAbs(v - g) > 0.5) return -1;   // snap radius: half a cell
    return g;
}

int BoardGeometry::pointToGridY(QPointF p) const {
    const qreal v = p.y() / m_cellPx - 1.0;
    const int g = qRound(v);
    if (g < 0 || g >= m_size) return -1;
    if (qAbs(v - g) > 0.5) return -1;
    return g;
}

int BoardGeometry::displayXToGrid(QChar letter) {
    const char c = letter.toUpper().toLatin1();
    if (c >= 'A' && c <= 'H') return c - 'A';
    if (c >= 'J' && c <= 'T') return c - 'A' - 1;
    return -1;
}

QChar BoardGeometry::gridToDisplayX(int x) {
    if (x < 0 || x > 24) return QChar();
    return QChar(static_cast<char>('A' + x + (x >= 8 ? 1 : 0)));
}
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_boardgeometry)
```

- [ ] **Step 4: 构建测试通过**

```bash
cmake --build build && ctest --test-dir build -R tst_boardgeometry --output-on-failure
```
Expected: 5 用例 PASS

- [ ] **Step 5: Commit**

```bash
git add src/ui/BoardGeometry.h src/ui/BoardGeometry.cpp tests
git commit -m "feat(ui): board geometry pure functions with tests"
```

---

### Task 3: BoardView 自绘（木纹/网格/星位/坐标/棋子/最后一手）

**Files:**
- Create: `src/ui/BoardView.h`
- Create: `src/ui/BoardView.cpp`
- Modify: `src/ui/CMakeLists.txt`（源列表加入 `BoardView.cpp`）

**Interfaces:**
- Consumes: Task 2 `BoardGeometry`、M1 `Game`（board()/currentNode()）
- Produces:
  ```cpp
  // src/ui/BoardView.h
  class BoardView : public QWidget {
      Q_OBJECT
  public:
      explicit BoardView(QWidget* parent = nullptr);
      void setGame(Game* game);                 // nullptr = 空 19 路盘
      void setBoardSize(int size);              // 无 game 时独立设尺寸
  signals:
      void boardClicked(QPoint pos);            // 已吸附换算的棋盘坐标
  protected:
      void paintEvent(QPaintEvent*) override;
      void mousePressEvent(QMouseEvent*) override;
      void resizeEvent(QResizeEvent*) override;
  private:
      void updateGeometry();
      void drawWood(QPainter& p);
      void drawGrid(QPainter& p);
      void drawStars(QPainter& p);
      void drawCoords(QPainter& p);
      void drawStones(QPainter& p);
      void drawLastMoveMark(QPainter& p);
      QVector<QPoint> starPoints(int size) const;
      Game* m_game = nullptr;
      BoardGeometry m_geom{19, 30.0};
  };
  ```
  绘制层次按 spec §5：木纹底色 → 网格线 → 星位 → 坐标（A-T 跳 I）→ 棋子（径向渐变高光）→ 最后一手标记。
  星位规则：19 路 9 星（{3,9,15}² 全网格）；13 路 5 星（(3,3)(3,9)(9,3)(9,9) + 天元(6,6)）；9 路 5 星（(2,2)(2,6)(6,2)(6,6) + 天元(4,4)）。

- [ ] **Step 1: 实现 BoardView**

BoardView.h:

```cpp
#pragma once
#include <QWidget>

#include "BoardGeometry.h"
#include "Game.h"

class QPainter;

class BoardView : public QWidget {
    Q_OBJECT
public:
    explicit BoardView(QWidget* parent = nullptr);
    void setGame(Game* game);
    void setBoardSize(int size);

signals:
    void boardClicked(QPoint pos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void updateGeometry();
    void drawWood(QPainter& p);
    void drawGrid(QPainter& p);
    void drawStars(QPainter& p);
    void drawCoords(QPainter& p);
    void drawStones(QPainter& p);
    void drawLastMoveMark(QPainter& p);
    QVector<QPoint> starPoints(int size) const;

    Game* m_game = nullptr;
    BoardGeometry m_geom{19, 30.0};
};
```

BoardView.cpp:

```cpp
#include "BoardView.h"
#include <QMouseEvent>
#include <QPainter>

BoardView::BoardView(QWidget* parent)
    : QWidget(parent), m_geom(19, 30.0) {
    setMinimumSize(400, 400);
}

void BoardView::setGame(Game* game) {
    m_game = game;
    if (m_game) setBoardSize(m_game->boardSize());
    update();
}

void BoardView::setBoardSize(int size) {
    m_geom = BoardGeometry(size, 30.0);
    update();
}

void BoardView::updateGeometry() {
    const int n = m_game ? m_game->boardSize() : 19;
    const qreal side = qMin(width(), height());
    m_geom = BoardGeometry(n, side / (n + 1));
}

void BoardView::resizeEvent(QResizeEvent*) {
    updateGeometry();
    update();
}

void BoardView::mousePressEvent(QMouseEvent* e) {
    updateGeometry();
    // Qt 5.11: localPos(); position() is 5.14+
    const QPointF pos = e->localPos();
    const int gx = m_geom.pointToGrid(pos);
    const int gy = m_geom.pointToGridY(pos);
    if (gx >= 0 && gy >= 0)
        emit boardClicked(QPoint(gx, gy));
}

QVector<QPoint> BoardView::starPoints(int size) const {
    QVector<QPoint> stars;
    QVector<int> edges;
    if (size == 19) edges = {3, 9, 15};
    else if (size == 13) edges = {3, 9};
    else if (size == 9) edges = {2, 6};
    else return stars;
    const int c = size / 2;
    for (int ey : edges)
        for (int ex : edges) {
            // 13/9: no edge-middle stars, corners + center only
            if (size != 19 && ((ex == c) != (ey == c))) continue;
            stars.append(QPoint(ex, ey));
        }
    if (size != 19)
        stars.append(QPoint(c, c));   // tengen for 13/9
    return stars;
}

void BoardView::drawWood(QPainter& p) {
    const QRectF area(0, 0, m_geom.boardPx(), m_geom.boardPx());
    QLinearGradient grad(area.topLeft(), area.bottomRight());
    grad.setColorAt(0.0, QColor(220, 179, 122));   // light kaya
    grad.setColorAt(1.0, QColor(196, 152, 92));    // darker kaya
    p.fillRect(area, grad);
}

void BoardView::drawGrid(QPainter& p) {
    const int n = m_game ? m_game->boardSize() : 19;
    p.setPen(QPen(QColor(60, 45, 25), 1.0));
    for (int i = 0; i < n; ++i) {
        const QPointF a = m_geom.gridToPoint(0, i);
        const QPointF b = m_geom.gridToPoint(n - 1, i);
        p.drawLine(a, QPointF(b.x(), a.y()));   // horizontal
        p.drawLine(a, QPointF(a.x(), b.y()));   // vertical
    }
    // outer border slightly thicker
    p.setPen(QPen(QColor(60, 45, 25), 2.0));
    p.drawRect(QRectF(m_geom.gridToPoint(0, 0), m_geom.gridToPoint(n - 1, n - 1)));
}

void BoardView::drawStars(QPainter& p) {
    const int n = m_game ? m_game->boardSize() : 19;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(60, 45, 25));
    const qreal r = m_geom.cellPx() * 0.09;
    for (const QPoint& s : starPoints(n))
        p.drawEllipse(m_geom.gridToPoint(s.x(), s.y()), r, r);
}

void BoardView::drawCoords(QPainter& p) {
    const int n = m_game ? m_game->boardSize() : 19;
    p.setPen(QColor(60, 45, 25));
    QFont f = font();
    f.setPointSizeF(qMax(6.0, f.pointSizeF() * 0.8));
    p.setFont(f);
    for (int i = 0; i < n; ++i) {
        // top letters
        const QRectF top((i + 1) * m_geom.cellPx() - m_geom.cellPx(), 0,
                         m_geom.cellPx() * 2, m_geom.cellPx());
        p.drawText(top, Qt::AlignCenter, QString(BoardGeometry::gridToDisplayX(i)));
        // left numbers (display row 1 at top = board row 0)
        const QRectF left(0, (i + 1) * m_geom.cellPx() - m_geom.cellPx(),
                          m_geom.cellPx(), m_geom.cellPx() * 2);
        p.drawText(left, Qt::AlignCenter, QString::number(n - i));
    }
}

void BoardView::drawStones(QPainter& p) {
    if (!m_game) return;
    const Board& b = m_game->board();
    const qreal r = m_geom.cellPx() * 0.47;
    for (int y = 0; y < b.size(); ++y) {
        for (int x = 0; x < b.size(); ++x) {
            const Stone s = b.stoneAt(x, y);
            if (s == Stone::Empty) continue;
            const QPointF c = m_geom.gridToPoint(x, y);
            QRadialGradient grad(c - QPointF(r * 0.3, r * 0.3), r * 1.4);
            if (s == Stone::Black) {
                grad.setColorAt(0.0, QColor(90, 90, 90));
                grad.setColorAt(0.4, QColor(25, 25, 25));
                grad.setColorAt(1.0, QColor(0, 0, 0));
            } else {
                grad.setColorAt(0.0, QColor(255, 255, 255));
                grad.setColorAt(0.5, QColor(235, 235, 230));
                grad.setColorAt(1.0, QColor(190, 190, 185));
            }
            p.setPen(QPen(QColor(40, 30, 20, 120), 1.0));
            p.setBrush(grad);
            p.drawEllipse(c, r, r);
        }
    }
}

void BoardView::drawLastMoveMark(QPainter& p) {
    if (!m_game) return;
    const MoveNode* cur = m_game->currentNode();
    if (!cur || !cur->parent || cur->pos.x() < 0) return;
    const QPointF c = m_geom.gridToPoint(cur->pos.x(), cur->pos.y());
    p.setPen(QPen(cur->color == Stone::Black ? QColor(255, 255, 255)
                                             : QColor(30, 30, 30), 2.0));
    p.setBrush(Qt::NoBrush);
    const qreal r = m_geom.cellPx() * 0.22;
    p.drawEllipse(c, r, r);
}

void BoardView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // HiDPI: QPainter works in logical (device-independent) coords; Qt scales
    // by devicePixelRatioF() automatically. Keep all math in qreal.
    updateGeometry();
    p.translate((width() - m_geom.boardPx()) / 2.0,
                (height() - m_geom.boardPx()) / 2.0);
    drawWood(p);
    drawGrid(p);
    drawStars(p);
    drawCoords(p);
    drawStones(p);
    drawLastMoveMark(p);
}
```

`src/ui/CMakeLists.txt` 源列表加入 `BoardView.cpp`。

- [ ] **Step 2: 构建 + 冒烟**

```bash
cmake --build build && timeout 3 ./build/src/ui/yigo; echo "exit=$?"
```
Expected: 编译通过，无运行报错（此时 MainWindow 尚未接入 BoardView，视觉验收在 Task 4）。

- [ ] **Step 3: Commit**

```bash
git add src/ui
git commit -m "feat(ui): BoardView with wood, grid, stars, coords, stones, last-move mark"
```

---

### Task 4: 虚手支持 + MainWindowLogic + MainWindow 集成

**Files:**
- Modify: `src/core/Game.h`（`play` 的注释更新：支持虚手）
- Modify: `src/core/Game.cpp`（`play` 支持 `(-1,-1)` 虚手）
- Create: `src/app/MainWindowLogic.h`
- Create: `src/app/MainWindowLogic.cpp`
- Modify: `src/core/CMakeLists.txt`（yigo_core 编入 MainWindowLogic.cpp 并暴露 include 路径）
- Modify: `tests/tst_game.cpp`（追加虚手用例）
- Test: `tests/tst_mainwindowlogic.cpp`（新）
- Modify: `tests/CMakeLists.txt`（追加 `yigo_add_test(tst_mainwindowlogic)`）
- Modify: `src/ui/MainWindow.h` / `src/ui/MainWindow.cpp`（BoardView 接线、菜单、快捷键、状态栏）

**Interfaces:**
- Consumes: Task 2 `BoardGeometry`、Task 3 `BoardView`、M1 `Game::play/undo/redo/goTo/nextToPlay`
- Produces:
  ```cpp
  // src/app/MainWindowLogic.h —— QtCore-only，编入 yigo_core
  struct MainWindowLogic {
      // 点击处理：合法返回 true；非法时 *hint 填状态栏文案，盘面不变
      static bool handleBoardClick(Game& game, QPoint pos, QString* hint = nullptr);
      enum class Action { PrevMove, NextMove, FirstMove, LastMove, Pass, Undo };
      static bool handleAction(Game& game, Action a, QString* hint = nullptr);
  };
  // Game::play 语义变化（M1 扩展）：pos=(-1,-1) 表示虚手——跳过合法性检查、
  // 不改盘面、新建节点。SGF 解析（B[]/W[]/tt）与 UI 虚手都依赖它。
  ```

- [ ] **Step 1: 写失败测试（两处）**

tests/tst_game.cpp 类内追加：

```cpp
    void passMove() {
        Game g(9);
        auto* n = g.play(QPoint(-1, -1), Stone::Black);
        QVERIFY(n != nullptr);
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QCOMPARE(g.currentNode()->pos, QPoint(-1, -1));
        QCOMPARE(g.nextToPlay(), Stone::White);
        QCOMPARE(g.board().stoneAt(4, 4), Stone::Empty);   // board unchanged
        QVERIFY(g.play(QPoint(-1, -1), Stone::White));     // two passes in a row ok
        QCOMPARE(g.currentNode()->moveNumber, 2);
    }
```

新文件 tests/tst_mainwindowlogic.cpp：

```cpp
#include <QtTest>
#include "MainWindowLogic.h"
#include "SgfParser.h"

class TestMainWindowLogic : public QObject {
    Q_OBJECT
private slots:
    void legalClickPlays() {
        Game g(9);
        QString hint;
        QVERIFY(MainWindowLogic::handleBoardClick(g, QPoint(2, 2), &hint));
        QVERIFY(hint.isEmpty());
        QCOMPARE(g.board().stoneAt(2, 2), Stone::Black);
        QCOMPARE(g.currentNode()->moveNumber, 1);
    }
    void illegalClickHints() {
        Game g(9);
        QVERIFY(g.play(QPoint(2, 2), Stone::Black));
        QString hint;
        QVERIFY(!MainWindowLogic::handleBoardClick(g, QPoint(2, 2), &hint));  // occupied
        QVERIFY2(!hint.isEmpty(), "hint text required for illegal move");
        QCOMPARE(g.currentNode()->moveNumber, 1);   // board unchanged
    }
    void navigationActions() {
        Game g(9);
        QVERIFY(g.play(QPoint(2, 2), Stone::Black));
        QVERIFY(g.play(QPoint(5, 5), Stone::White));
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::PrevMove));
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::Undo));
        QCOMPARE(g.currentNode()->moveNumber, 0);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::NextMove));
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::LastMove));
        QCOMPARE(g.currentNode()->moveNumber, 2);
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::FirstMove));
        QCOMPARE(g.currentNode()->moveNumber, 0);
    }
    void passAppendsNode() {
        Game g(9);
        QString hint;
        QVERIFY(MainWindowLogic::handleAction(g, MainWindowLogic::Action::Pass, &hint));
        QCOMPARE(g.currentNode()->moveNumber, 1);
        QCOMPARE(g.currentNode()->pos, QPoint(-1, -1));
    }
    void loadSgfAndNavigate() {
        QString err;
        auto* g = SgfParser::parse("(;GM[1]FF[4]SZ[9];B[cc];W[gg])", &err);
        QVERIFY2(g != nullptr, qPrintable(err));
        QVERIFY(MainWindowLogic::handleAction(*g, MainWindowLogic::Action::LastMove));
        QCOMPARE(g->currentNode()->moveNumber, 2);
        QVERIFY(MainWindowLogic::handleAction(*g, MainWindowLogic::Action::FirstMove));
        QCOMPARE(g->currentNode()->moveNumber, 0);
        delete g;
    }
};

QTEST_GUILESS_MAIN(TestMainWindowLogic)
#include "tst_mainwindowlogic.moc"
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_mainwindowlogic)
```

- [ ] **Step 2: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（MainWindowLogic.h 不存在、Game::play 无虚手支持 → tst_game passMove FAIL）

- [ ] **Step 3: 写最小实现**

src/app/MainWindowLogic.h:

```cpp
#pragma once
#include <QString>
#include <QPoint>

#include "Game.h"

// Pure QtCore logic behind MainWindow: click/key handling on a Game.
struct MainWindowLogic {
    enum class Action { PrevMove, NextMove, FirstMove, LastMove, Pass, Undo };
    static bool handleBoardClick(Game& game, QPoint pos, QString* hint = nullptr);
    static bool handleAction(Game& game, Action a, QString* hint = nullptr);
};
```

src/app/MainWindowLogic.cpp:

```cpp
#include "MainWindowLogic.h"

bool MainWindowLogic::handleBoardClick(Game& game, QPoint pos, QString* hint) {
    const Stone color = game.nextToPlay();
    if (!game.play(pos, color)) {
        if (hint) *hint = MainWindowLogic::tr ? QString("Illegal move") : QString();
        return false;
    }
    return true;
}
```

（`tr` 非 QObject 成员——不用它。hint 文案为静态英文常量即可，MainWindow 端负责展示；最终实现：）

```cpp
#include "MainWindowLogic.h"

bool MainWindowLogic::handleBoardClick(Game& game, QPoint pos, QString* hint) {
    const Stone color = game.nextToPlay();
    if (!game.play(pos, color)) {
        if (hint) *hint = QStringLiteral("Illegal move");
        return false;
    }
    return true;
}

bool MainWindowLogic::handleAction(Game& game, Action a, QString* hint) {
    Q_UNUSED(hint);
    switch (a) {
    case Action::PrevMove: return game.undo();
    case Action::NextMove: return game.redo();
    case Action::FirstMove:
        game.goTo(game.tree().root());
        return true;
    case Action::LastMove: {
        MoveNode* n = game.currentNode();
        while (!n->children.isEmpty())
            n = n->children[0];
        game.goTo(n);
        return true;
    }
    case Action::Pass:
        return game.play(QPoint(-1, -1), game.nextToPlay()) != nullptr;
    case Action::Undo: return game.undo();
    }
    return false;
}
```

src/core/Game.cpp `play()` 替换为：

```cpp
MoveNode* Game::play(QPoint pos, Stone color) {
    const bool isPass = pos.x() < 0 || pos.y() < 0;
    if (!isPass && !m_board.isLegal(pos.x(), pos.y(), color)) return nullptr;
    MoveNode* node = m_tree.addChild(m_current, color, pos);
    if (!isPass) {
        QVector<QPoint> captured;
        m_board.placeStone(pos.x(), pos.y(), color, &captured);
        if (color == Stone::Black) m_blackCaptures += captured.size();
        else if (color == Stone::White) m_whiteCaptures += captured.size();
    }
    m_current = node;
    maybeSnapshot();
    return node;
}
```

src/core/CMakeLists.txt 修改为：

```cmake
add_library(yigo_core STATIC
    Board.cpp
    Rules.cpp
    GameTree.cpp
    Game.cpp
    SgfCoord.cpp
    SgfParser.cpp
    ${CMAKE_SOURCE_DIR}/src/app/MainWindowLogic.cpp
)
target_include_directories(yigo_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/app
)
target_link_libraries(yigo_core PUBLIC Qt5::Core)
```

src/core/Game.h `play` 注释同步更新：

```cpp
    // play: if legal, create MoveNode under current node and advance,
    // returns the new node; returns nullptr if illegal.
    // pos=(-1,-1) is a pass: no legality check, board unchanged.
```

（`SgfParser` 的 pass 处理随之简化可不再走 `[illegal]` 兜底，但 M1 逻辑已兼容两种路径，本任务不动 SgfParser。）

- [ ] **Step 4: 构建测试通过**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 全部 PASS（含 M1 全部回归——特别确认 tst_sgf 的 passMove 用例仍绿）

- [ ] **Step 5: MainWindow 接线**

完整替换 src/ui/MainWindow.h：

```cpp
#pragma once
#include <QMainWindow>

#include "Game.h"

class BoardView;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onBoardClicked(QPoint pos);
    void onNewGame19();
    void onNewGame13();
    void onNewGame9();
    void onOpen();
    void onSave();
    void onUndo();
    void onPass();
    void onPrev();
    void onNext();
    void onFirst();
    void onLast();

private:
    void setupMenus();
    void setupCentral();
    void refreshStatus();
    void newGame(int size);
    bool confirmDiscard();

    Game* m_game = nullptr;
    BoardView* m_boardView = nullptr;
    QLabel* m_moveLabel = nullptr;
    QLabel* m_turnLabel = nullptr;
    QString m_currentFile;
};
```

完整替换 src/ui/MainWindow.cpp：

```cpp
#include "MainWindow.h"

#include "BoardView.h"
#include "MainWindowLogic.h"
#include "SgfParser.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("YiGo 弈境"));
    resize(800, 600);
    setupCentral();
    setupMenus();
    newGame(19);
}

void MainWindow::setupCentral() {
    m_boardView = new BoardView(this);
    setCentralWidget(m_boardView);
    connect(m_boardView, &BoardView::boardClicked, this, &MainWindow::onBoardClicked);
    m_moveLabel = new QLabel(this);
    m_turnLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_moveLabel);
    statusBar()->addPermanentWidget(m_turnLabel);
    statusBar()->showMessage(tr("Ready"), 2000);
}

void MainWindow::setupMenus() {
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* openAct = file->addAction(tr("&Open..."), this, &MainWindow::onOpen);
    openAct->setShortcut(QKeySequence::Open);
    QAction* saveAct = file->addAction(tr("&Save As..."), this, &MainWindow::onSave);
    saveAct->setShortcut(QKeySequence::Save);
    file->addSeparator();
    QAction* quitAct = file->addAction(tr("E&xit"), this, &QWidget::close);
    quitAct->setShortcut(QKeySequence::Quit);

    QMenu* game = menuBar()->addMenu(tr("&Game"));
    QMenu* newMenu = game->addMenu(tr("&New"));
    connect(newMenu->addAction(tr("19 x 19")), &QAction::triggered,
            this, &MainWindow::onNewGame19);
    connect(newMenu->addAction(tr("13 x 13")), &QAction::triggered,
            this, &MainWindow::onNewGame13);
    connect(newMenu->addAction(tr("9 x 9")), &QAction::triggered,
            this, &MainWindow::onNewGame9);
    QAction* undoAct = game->addAction(tr("&Undo"), this, &MainWindow::onUndo);
    undoAct->setShortcut(QKeySequence::Undo);
    QAction* passAct = game->addAction(tr("&Pass"), this, &MainWindow::onPass);
    passAct->setShortcut(tr("P"));
}

void MainWindow::newGame(int size) {
    if (!confirmDiscard()) return;
    delete m_game;
    m_game = new Game(size);
    m_currentFile.clear();
    m_boardView->setGame(m_game);
    setWindowTitle(tr("YiGo 弈境"));
    refreshStatus();
}

void MainWindow::onNewGame19() { newGame(19); }
void MainWindow::onNewGame13() { newGame(13); }
void MainWindow::onNewGame9()  { newGame(9); }

void MainWindow::onBoardClicked(QPoint pos) {
    if (!m_game) return;
    QString hint;
    if (!MainWindowLogic::handleBoardClick(*m_game, pos, &hint)) {
        statusBar()->showMessage(hint, 2000);   // no modal per spec §6
        return;
    }
    m_boardView->update();
    refreshStatus();
}

void MainWindow::refreshStatus() {
    m_moveLabel->setText(tr("Move %1").arg(m_game->currentNode()->moveNumber));
    const Stone who = m_game->nextToPlay();
    m_turnLabel->setText(who == Stone::Black ? tr("Black to play")
                                             : tr("White to play"));
}

void MainWindow::onUndo() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::Undo)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onPass() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::Pass)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onPrev() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::PrevMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onNext() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::NextMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onFirst() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::FirstMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::onLast() {
    if (!m_game) return;
    if (MainWindowLogic::handleAction(*m_game, MainWindowLogic::Action::LastMove)) {
        m_boardView->update();
        refreshStatus();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Left:  onPrev();  return;
    case Qt::Key_Right: onNext();  return;
    case Qt::Key_Home:  onFirst(); return;
    case Qt::Key_End:   onLast();  return;
    case Qt::Key_P:     onPass();  return;
    case Qt::Key_Z:
        if (e->modifiers() & Qt::ControlModifier) { onUndo(); return; }
        break;
    default: break;
    }
    QMainWindow::keyPressEvent(e);
}

void MainWindow::onOpen() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open SGF"), QString(),
                                                      tr("SGF files (*.sgf)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open failed"), tr("Cannot read %1").arg(path));
        return;
    }
    QString err;
    Game* g = SgfParser::parse(QString::fromUtf8(f.readAll()), &err);
    if (!g) {
        QMessageBox::warning(this, tr("Open failed"), err);
        return;
    }
    delete m_game;
    m_game = g;
    m_currentFile = path;
    m_boardView->setGame(m_game);
    setWindowTitle(tr("%1 - YiGo 弈境").arg(QFileInfo(path).fileName()));
    refreshStatus();
}

void MainWindow::onSave() {
    if (!m_game) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save SGF"), m_currentFile,
                                                      tr("SGF files (*.sgf)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Cannot write %1").arg(path));
        return;
    }
    f.write(SgfParser::serialize(*m_game).toUtf8());
    m_currentFile = path;
    setWindowTitle(tr("%1 - YiGo 弈境").arg(QFileInfo(path).fileName()));
}

bool MainWindow::confirmDiscard() {
    if (!m_game || m_game->tree().nodeCount() <= 1) return true;   // empty game
    const QMessageBox::StandardButton r = QMessageBox::question(
        this, tr("Discard current game?"),
        tr("The current game is not saved. Discard it?"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return r == QMessageBox::Discard;
}
```

（`#include <QFile>` 由 QFileDialog/SgfParser 传递包含；若编译报错显式补 `#include <QFile>`。）

- [ ] **Step 6: 构建 + 全量测试 + 手工验收**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
手工验收（本机桌面运行 `./build/src/ui/yigo`）：

1. 点击空盘任意交叉点 → 黑子出现，状态栏 "Move 1 / White to play"
2. 再点同一点 → 状态栏 2 秒内显示 "Illegal move"，无弹窗，手数不变
3. 劫点重复提子 → 状态栏提示，盘面不变（用 CLI 生成劫形 SGF 后打开测试）
4. Ctrl+Z 悔棋 → 子消失；P 虚手 → 手数 +1 盘面不变
5. 方向键/Home/End 跳转正常
6. 新建 9 路对局 → 棋盘变小、5 星位
7. HiDPI（若本机为 1x 缩放则记录"1x 通过"）：棋盘清晰不模糊、点击位置准确

- [ ] **Step 7: Commit**

```bash
git add src tests
git commit -m "feat(ui): main window with play/undo/pass, sgf io, status feedback"
```

---

### Task 5: SGF 往返 + 畸形输入防护（收尾验收）

**Files:**
- Modify: `src/ui/MainWindow.cpp`（Task 4 已含 onOpen/onSave/confirmDiscard；本任务只做验收与发现问题的修补）

**Interfaces:**
- Consumes: Task 4 的 onOpen/onSave/confirmDiscard、`SgfParser::parse/serialize`
- Produces: 完整 M2 交付物。

- [ ] **Step 1: 构建 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 全绿（M1 5 个 + M2 tst_boardgeometry/tst_mainwindowlogic）

- [ ] **Step 2: 手工验收清单**

```bash
echo '(;GM[1]FF[4]SZ[9]KM[7.5];B[cc];W[gg];B[dd])' > /tmp/opencode/m2.sgf
: > /tmp/opencode/empty.sgf
printf '(;GM[1]' > /tmp/opencode/trunc.sgf
./build/src/ui/yigo
```
1. Ctrl+O 打开 /tmp/opencode/m2.sgf → 盘面显示 3 子，标题含文件名，状态栏 Move 3
2. 方向键导航 → 重放到每手正确
3. 落 1 子后 Ctrl+S 另存 → 重新打开 → 子数/手数一致
4. 打开 /tmp/opencode/empty.sgf → 错误对话框（"no root node" 或 byte 错误信息），不崩溃
5. 打开 /tmp/opencode/trunc.sgf → 错误对话框，不崩溃
6. 对局中途 Ctrl+O → 出现"丢弃当前对局?"确认框；取消则不丢失

- [ ] **Step 3: 发现问题则修复并补测试，然后 Commit**

```bash
git add src/ui
git commit -m "feat(ui): m2 acceptance pass - sgf roundtrip and malformed input guards"
```

---

## 完成定义（M2 DoD）

- `ctest --test-dir build` 全绿（M1 5 个 + M2 tst_boardgeometry/tst_mainwindowlogic）
- `./build/src/ui/yigo` 可运行：自绘棋盘（木纹/网格/星位/坐标/棋子/最后一手）、点击落子、非法提示（无 modal）、悔棋/虚手/方向键跳转、9/13/19 新建、SGF 打开保存往返一致、畸形 SGF 不崩溃、丢弃确认
- core/ 唯一改动：`Game::play` 虚手支持（含新测试）
- 每个 Task 独立提交，共 5 个提交
