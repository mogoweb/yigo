# YiGo M6（打磨：设置持久化 + HiDPI + UOS 打包）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** QSettings 持久化（引擎配置/对局偏好/窗口布局）+ HiDPI 启用声明 + UOS debian 打包，交付可 `dpkg -i` 安装的 `yigo_0.5.0_amd64.deb`。

**Architecture:** `AppSettings`（QtCore-only 薄封装：QSettings 读写 engine 路径/参数/引擎类型、对局 setup、窗口几何/布局状态；编入 yigo_core 可离线单测）。MainWindow 启动时恢复、关闭时保存（`closeEvent`）。HiDPI：main.cpp 在 QApplication 前设置 `AA_EnableHighDpiScaling`（Qt 5.6+ 属性，spec §1 认可 5.11 基线下的整数倍缩放）。打包：`packaging/debian/` 目录（control/rules/changelog/desktop 文件图标）+ 顶层 `cmake --build build --target deb` 自定义目标调 dpkg-deb 产出 deb。无 GUI 单测新增（AppSettings 用 QSettings 临时文件验证读写往返）。

**Tech Stack:** C++17, Qt 5.11.3, CMake 3.22 + dpkg-deb, QtTest（AppSettings 离线 TDD）

**Spec:** `docs/superpowers/specs/2026-09-21-yigo-architecture-design.md`（§5 设置持久化、§1 HiDPI 细则、§8 里程碑 M6）

## Global Constraints

- Qt 5.11 API 基线：只用 Qt 5.11.3 存在的 API（AA_EnableHighDpiScaling 是 5.6+ 属性 ✓）
- `src/app/AppSettings` QtCore-only（编入 yigo_core）
- 容器统一用 `QVector`
- Linux 依赖系统 Qt（deb 只打包 yigo 二进制 + desktop + 图标，**不**捆绑 Qt 库）
- 目标平台 UOS V20 amd64：deb Architecture: amd64，Section: games，依赖 libqt5core5a/libqt5widgets5/libqt5gui5
- 每任务 TDD：先写失败测试→验证失败→最小实现→验证通过→提交（打包任务以构建产物验收）
- 编译命令统一：`cmake --build build`；测试统一：`ctest --test-dir build --output-on-failure`
- 提交身份：仓库 local 配置 `alex <mogoweb@gmail.com>`

## Review Focus

以下输入类别最易踩坑（每条已落在对应任务中作为测试/验收步骤）：

1. **设置键缺失**（首次启动无配置文件）：期望全部走默认值不崩溃 —— Task 1 测试 `defaultsWhenEmpty`
2. **损坏的设置值**（手改配置文件为非法值，如 size=0/komi=abc）：期望 clamp/回退默认 —— Task 1 测试 `invalidValuesClamped`
3. **HiDPI 整数倍缩放**（QT_SCALE_FACTOR=2）：期望启动不崩、布局不糊（手工验收步骤，记录 1x 结果即可）
4. **deb 安装后可运行**（`dpkg -i` 后 `/usr/bin/yigo` 启动、desktop 图标出现）：期望安装卸载干净 —— Task 3 验收步骤
5. **窗口几何恢复**（保存时窗口 1000x700 → 重启恢复 1000x700）：期望精确恢复 —— Task 1 测试 `geometryRoundTrip`

---

### Task 1: AppSettings 读写封装（TDD）

**Files:**
- Create: `src/app/AppSettings.h` / `src/app/AppSettings.cpp`
- Modify: `src/core/CMakeLists.txt`（编入）
- Test: `tests/tst_appsettings.cpp`
- Modify: `tests/CMakeLists.txt`（追加）

**Interfaces:**
- Consumes: M4 `GameSetup`、`EngineConfig`
- Produces:
  ```cpp
  // src/app/AppSettings.h
  class AppSettings {
  public:
      explicit AppSettings(const QString& iniPath = QString()); // 空 = 默认 QSettings 位置
      // engine
      EngineConfig engineConfig() const;
      void setEngineConfig(const EngineConfig& cfg);
      // last game setup
      GameSetup gameSetup() const;
      void setGameSetup(const GameSetup& s);
      // window geometry (QByteArray saveState/restoreState compatible)
      QByteArray windowGeometry() const;
      void setWindowGeometry(const QByteArray& geo);
  private:
      QString m_iniPath;
  };
  ```
  键名空间：`engine/type, engine/executable, engine/args, game/size, game/komi, game/handicap, game/blackAI, game/whiteAI, window/geometry`。

- [ ] **Step 1: 写失败测试 tests/tst_appsettings.cpp**

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "AppSettings.h"

class TestAppSettings : public QObject {
    Q_OBJECT
private slots:
    void defaultsWhenEmpty() {
        // review focus 1: 空配置 → 默认值
        QTemporaryDir dir;
        AppSettings s(dir.path() + "/settings.ini");
        const EngineConfig e = s.engineConfig();
        QCOMPARE(e.executable, QString());
        QCOMPARE(e.type, EngineConfig::KataGo);
        const GameSetup g = s.gameSetup();
        QCOMPARE(g.boardSize, 19);
        QCOMPARE(g.komi, 7.5);
        QCOMPARE(g.handicap, 0);
        QCOMPARE(g.black.kind, PlayerConfig::Human);
        QCOMPARE(g.white.kind, PlayerConfig::Human);
        QVERIFY(s.windowGeometry().isEmpty());
    }
    void engineRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {
            AppSettings s(path);
            EngineConfig cfg;
            cfg.type = EngineConfig::LeelaZero;
            cfg.executable = "/opt/lz/leelaz";
            cfg.baseArgs = QStringList() << "--weights" << "w.gz";
            cfg.gtpCommand = "lz-analyze";
            s.setEngineConfig(cfg);
        }
        AppSettings s2(path);   // 重新打开（落盘后读回）
        const EngineConfig e = s2.engineConfig();
        QCOMPARE(e.type, EngineConfig::LeelaZero);
        QCOMPARE(e.executable, QString("/opt/lz/leelaz"));
        QCOMPARE(e.baseArgs, QStringList() << "--weights" << "w.gz");
        QCOMPARE(e.gtpCommand, QString("lz-analyze"));
    }
    void gameSetupRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {
            AppSettings s(path);
            GameSetup g;
            g.boardSize = 9; g.komi = 5.5; g.handicap = 2;
            g.black.kind = PlayerConfig::AI;
            s.setGameSetup(g);
        }
        AppSettings s2(path);
        const GameSetup g = s2.gameSetup();
        QCOMPARE(g.boardSize, 9);
        QCOMPARE(g.komi, 5.5);
        QCOMPARE(g.handicap, 2);
        QCOMPARE(g.black.kind, PlayerConfig::AI);
        QCOMPARE(g.white.kind, PlayerConfig::Human);
    }
    void invalidValuesClamped() {
        // review focus 2: 非法值 → 回退默认
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        {   // 写入垃圾
            QSettings raw(path, QSettings::IniFormat);
            raw.setValue("game/size", 0);
            raw.setValue("game/komi", "abc");
            raw.setValue("game/handicap", 99);
        }
        AppSettings s(path);
        const GameSetup g = s.gameSetup();
        QCOMPARE(g.boardSize, 19);   // 0 非法 → 默认
        QCOMPARE(g.komi, 7.5);       // 解析失败 → 默认
        QCOMPARE(g.handicap, 0);     // 99 超界 → clamp 为 0..9 语义取 0? 裁决：>9 → 0（无效让子=不让）
    }
    void geometryRoundTrip() {
        // review focus 5: 几何字节往返
        QTemporaryDir dir;
        const QString path = dir.path() + "/settings.ini";
        QByteArray geo(128, 'X');
        {
            AppSettings s(path);
            s.setWindowGeometry(geo);
        }
        AppSettings s2(path);
        QCOMPARE(s2.windowGeometry(), geo);
    }
};

QTEST_GUILESS_MAIN(TestAppSettings)
#include "tst_appsettings.moc"
```

tests/CMakeLists.txt 追加：

```cmake
yigo_add_test(tst_appsettings)
```

- [ ] **Step 2: 运行验证失败**

```bash
cmake --build build 2>&1 | tail -3
```
Expected: 编译失败（AppSettings 不存在）

- [ ] **Step 3: 写实现**

AppSettings.h:

```cpp
#pragma once
#include <QByteArray>
#include <QString>

#include "EngineConfig.h"
#include "GameController.h"

// Thin QSettings wrapper for persisted app state (engine config, last game
// setup, window geometry). QtCore-only, unit-testable with an INI path.
class AppSettings {
public:
    explicit AppSettings(const QString& iniPath = QString());

    EngineConfig engineConfig() const;
    void setEngineConfig(const EngineConfig& cfg);
    GameSetup gameSetup() const;
    void setGameSetup(const GameSetup& s);
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geo);

private:
    QString m_iniPath;
};
```

AppSettings.cpp:

```cpp
#include "AppSettings.h"
#include <QSettings>

AppSettings::AppSettings(const QString& iniPath) : m_iniPath(iniPath) {}

static QSettings makeSettings(const QString& iniPath) {
    return iniPath.isEmpty() ? QSettings(QSettings::IniFormat, QSettings::UserScope,
                                         "YiGo", "YiGo")
                             : QSettings(iniPath, QSettings::IniFormat);
}

EngineConfig AppSettings::engineConfig() const {
    QSettings s = makeSettings(m_iniPath);
    EngineConfig cfg;
    cfg.type = s.value("engine/type", int(EngineConfig::KataGo)).toInt() == 1
                   ? EngineConfig::LeelaZero : EngineConfig::KataGo;
    cfg.executable = s.value("engine/executable").toString();
    cfg.baseArgs = s.value("engine/args").toStringList();
    cfg.gtpCommand = cfg.type == EngineConfig::KataGo
                         ? QStringLiteral("kata-analyze interval 50")
                         : QStringLiteral("lz-analyze");
    return cfg;
}

void AppSettings::setEngineConfig(const EngineConfig& cfg) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("engine/type", int(cfg.type));
    s.setValue("engine/executable", cfg.executable);
    s.setValue("engine/args", cfg.baseArgs);
    s.sync();
}

GameSetup AppSettings::gameSetup() const {
    QSettings s = makeSettings(m_iniPath);
    GameSetup g;
    const int size = s.value("game/size", 19).toInt();
    g.boardSize = (size == 9 || size == 13 || size == 19) ? size : 19;
    bool komiOk = false;
    const double komi = s.value("game/komi", 7.5).toDouble(&komiOk);
    g.komi = (komiOk && komi >= 0.0 && komi <= 30.0) ? komi : 7.5;
    const int hc = s.value("game/handicap", 0).toInt();
    g.handicap = (hc >= 0 && hc <= 9) ? hc : 0;
    g.black.kind = s.value("game/blackAI", false).toBool() ? PlayerConfig::AI
                                                           : PlayerConfig::Human;
    g.white.kind = s.value("game/whiteAI", false).toBool() ? PlayerConfig::AI
                                                           : PlayerConfig::Human;
    return g;
}

void AppSettings::setGameSetup(const GameSetup& g) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("game/size", g.boardSize);
    s.setValue("game/komi", g.komi);
    s.setValue("game/handicap", g.handicap);
    s.setValue("game/blackAI", g.black.kind == PlayerConfig::AI);
    s.setValue("game/whiteAI", g.white.kind == PlayerConfig::AI);
    s.sync();
}

QByteArray AppSettings::windowGeometry() const {
    QSettings s = makeSettings(m_iniPath);
    return s.value("window/geometry").toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray& geo) {
    QSettings s = makeSettings(m_iniPath);
    s.setValue("window/geometry", geo);
    s.sync();
}
```

（注意 `makeSettings` 返回值语义：QSettings 拷贝共享底层文件，栈对象析构即 flush；setXxx 中显式 `sync()` 保证测试重开立即可读。）

- [ ] **Step 4: 构建测试通过 + 全量回归**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src tests
git commit -m "feat(app): AppSettings persistence wrapper with validation"
```

---

### Task 2: MainWindow 集成 + HiDPI 声明

**Files:**
- Modify: `src/main.cpp`（AA_EnableHighDpiScaling + AppSettings 恢复）
- Modify: `src/ui/MainWindow.h` / `MainWindow.cpp`（恢复/保存设置、closeEvent、EnginePanel 预填、NewGameDialog 预填）
- Modify: `src/ui/EnginePanel.h`（追加 `setConfig(const EngineConfig&)` 预填接口）
- Modify: `src/ui/NewGameDialog.h`（追加 `void setSetup(const GameSetup&)` 预填接口）

**Interfaces:**
- Consumes: Task 1 `AppSettings`、M4 NewGameDialog、M3 EnginePanel
- Produces: 应用级设置闭环。

- [ ] **Step 1: main.cpp HiDPI + 传路径**

```cpp
#include <QApplication>
#include <QSettings>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // HiDPI: spec §1 整数倍缩放基线（Qt 5.6+ 属性，必须在 QApplication 前）
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName("YiGo");
    app.setApplicationVersion("0.5.0");
    app.setOrganizationName("YiGo");
    app.setOrganizationDomain("yigo");   // QSettings 默认路径稳定
    MainWindow w;
    w.show();
    return app.exec();
}
```

- [ ] **Step 2: MainWindow 恢复/保存**

MainWindow.h 追加：

```cpp
protected:
    void closeEvent(QCloseEvent* e) override;
private:
    AppSettings m_settings;
```

（构造函数初始化列表 `m_settings()`。）

MainWindow.cpp：

```cpp
// 构造函数尾部（newGame(19) 改为按设置开局）:
    const GameSetup setup = m_settings.gameSetup();
    startGame(setup);
    m_enginePanel->setConfig(m_settings.engineConfig());
    const QByteArray geo = m_settings.windowGeometry();
    if (!geo.isEmpty()) restoreGeometry(geo);

void MainWindow::closeEvent(QCloseEvent* e) {
    m_settings.setWindowGeometry(saveGeometry());
    if (m_enginePanel) m_settings.setEngineConfig(m_enginePanel->config());
    m_engine->stop();
    e->accept();
}

// onNewGameDialog 确认后追加:
    m_settings.setGameSetup(dlg.setup());
```

EnginePanel 追加：

```cpp
// .h public:
    void setConfig(const EngineConfig& cfg);
// .cpp:
void EnginePanel::setConfig(const EngineConfig& cfg) {
    m_type->setCurrentIndex(cfg.type == EngineConfig::KataGo ? 0 : 1);
    m_path->setText(cfg.executable);
    m_args->setText(cfg.baseArgs.join(' '));
}
```

NewGameDialog 追加：

```cpp
// .h public:
    void setSetup(const GameSetup& s);
// .cpp:
void NewGameDialog::setSetup(const GameSetup& s) {
    const int idx = m_size->findData(s.boardSize);
    if (idx >= 0) m_size->setCurrentIndex(idx);
    m_komi->setValue(s.komi);
    m_handicap->setValue(s.handicap);
    m_blackAI->setChecked(s.black.kind == PlayerConfig::AI);
    m_whiteAI->setChecked(s.white.kind == PlayerConfig::AI);
}
// onNewGameDialog 弹框前: dlg.setSetup(m_settings.gameSetup());
```

- [ ] **Step 3: 构建 + 全量回归 + 手工验收**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```
手工验收：
1. 启动 yigo → Engine 面板填路径 → 落一局 → 关闭 → 重启：引擎路径/参数还在，开局大小与上次一致，窗口尺寸恢复
2. `~/.config/YiGo/YiGo.ini` 出现且内容正确
3. `QT_SCALE_FACTOR=2 ./build/src/ui/yigo`：2x 下棋盘/文字清晰不糊，无崩溃（1x 机器记录 1x 通过即可）

- [ ] **Step 4: Commit**

```bash
git add src
git commit -m "feat(ui): settings persistence, HiDPI scaling, engine dialog prefill"
```

---

### Task 3: UOS debian 打包

**Files:**
- Create: `packaging/debian/control`
- Create: `packaging/debian/rules`
- Create: `packaging/debian/changelog`
- Create: `packaging/debian/yigo.desktop`
- Create: `packaging/icon.svg`
- Modify: `CMakeLists.txt`（追加 `deb` 自定义目标）

**Interfaces:**
- Consumes: Task 1-2 交付的 `build/src/ui/yigo`
- Produces: `yigo_0.5.0_amd64.deb`（`/usr/bin/yigo`、`/usr/share/applications/yigo.desktop`、`/usr/share/icons/hicolor/scalable/apps/yigo.svg`）

- [ ] **Step 1: 写 debian 文件**

packaging/debian/control:

```
Source: yigo
Section: games
Priority: optional
Maintainer: alex <mogoweb@gmail.com>
Build-Depends: debhelper (>= 10), cmake, ninja-build, qtbase5-dev (>= 5.11), g++
Standards-Version: 4.1.4
Homepage: https://github.com/mogoweb/yigo

Package: yigo
Architecture: amd64
Depends: libqt5core5a (>= 5.11), libqt5gui5 (>= 5.11), libqt5widgets5 (>= 5.11), libqt5test5
Description: Cross-platform Go (Weiqi) AI analysis and review tool
 YiGo is a native Qt C++ Go analysis and review application inspired by
 Katrain. It integrates GTP engines such as KataGo and Leela Zero for
 winrate analysis, blunder detection and human-vs-engine play.
```

packaging/debian/rules（二进制复用 CMake 构建，简单 install 方式）：

```makefile
#!/usr/bin/make -f
%:
	dh $@

override_dh_auto_build:
	cmake --build build

override_dh_auto_install:
	install -Dm755 build/src/ui/yigo debian/yigo/usr/bin/yigo
	install -Dm644 packaging/debian/yigo.desktop debian/yigo/usr/share/applications/yigo.desktop
	install -Dm644 packaging/icon.svg debian/yigo/usr/share/icons/hicolor/scalable/apps/yigo.svg
```

packaging/debian/changelog:

```
yigo (0.5.0) unstable; urgency=medium

  * Initial packaging for UOS V20 (M6)
  * Engine integration (KataGo / Leela Zero via GTP)
  * Play, review analysis, winrate chart, handicap

 -- alex <mogoweb@gmail.com>  Wed, 24 Sep 2026 12:00:00 +0800
```

packaging/debian/yigo.desktop:

```ini
[Desktop Entry]
Type=Application
Name=YiGo 弈境
Name[zh_CN]=弈境
Comment=Go AI analysis and review
Comment[zh_CN]=围棋 AI 复盘分析
Exec=yigo
Icon=yigo
Terminal=false
Categories=Game;BoardGame;
Keywords=go;baduk;weiqi;katago;
```

（简 SVG 图标：黑白双子 + 木纹底色圆。）

- [ ] **Step 2: CMake deb 目标**

CMakeLists.txt 追加：

```cmake
# Debian packaging: builds a .deb from the already-built binary
find_program(DPKG_DEB_EXECUTABLE dpkg-deb)
if(DPKG_DEB_EXECUTABLE)
    add_custom_target(deb
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/DEBIAN
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/applications
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/ui/yigo ${CMAKE_BINARY_DIR}/debroot/usr/bin/yigo
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/yigo.desktop ${CMAKE_BINARY_DIR}/debroot/usr/share/applications/yigo.desktop
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/icon.svg ${CMAKE_BINARY_DIR}/debroot/usr/share/icons/hicolor/scalable/apps/yigo.svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/packaging/debian/control ${CMAKE_BINARY_DIR}/debroot/DEBIAN/control
        COMMAND sed -i "s/^Installed-Size:.*/Installed-Size: 20480/" ${CMAKE_BINARY_DIR}/debroot/DEBIAN/control || true
        COMMAND ${DPKG_DEB_EXECUTABLE} --build --root-owner-group
                ${CMAKE_BINARY_DIR}/debroot ${CMAKE_BINARY_DIR}/yigo_0.5.0_amd64.deb
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS yigo
        COMMENT "Building yigo_0.5.0_amd64.deb"
        VERBATIM
    )
endif()
```

（control 顶部追加 `Installed-Size: 20480`（约 20MB 二进制的 KB 数）避免 dpkg 警告。）

- [ ] **Step 3: 构建产物验收**

```bash
cmake --build build && cmake --build build --target deb
ls -la build/yigo_0.5.0_amd64.deb
dpkg-deb --info build/yigo_0.5.0_amd64.deb | head -20
```
手工验收（可选，需 sudo）：
1. `sudo dpkg -i build/yigo_0.5.0_amd64.deb`
2. `/usr/bin/yigo` 启动正常；启动器出现"弈境"图标
3. `sudo dpkg -r yigo` 卸载干净

- [ ] **Step 4: Commit**

```bash
git add packaging CMakeLists.txt
git commit -m "build: UOS debian packaging with desktop entry and icon"
```

---

## 完成定义（M6 DoD）

- `ctest --test-dir build` 全绿（M1-M5 + tst_appsettings）
- 设置闭环：引擎配置/对局偏好/窗口几何 保存-恢复一致；首次启动默认值；非法值回退
- HiDPI：AA_EnableHighDpiScaling 已声明（1x/2x 无崩溃）
- `build/yigo_0.5.0_amd64.deb` 可 dpkg-deb 校验通过，含二进制/desktop/图标
- 每个 Task 独立提交，共 3 个提交
