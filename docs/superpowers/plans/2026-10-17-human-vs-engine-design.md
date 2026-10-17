# 人机对弈模式设计（2026-10-17）

## 问题

点击引擎面板「启动」后，系统只做胜率分析，从不进入人机对弈。

根因：回合状态机与引擎生命周期解耦。

1. `MainWindow` 构造时 `startGame()` 在引擎存在之前跑完 `GameController::newGame()`
   → `advanceTurn()`，此时 `m_engine->isRunning() == false`，永远落在 `HumanTurn`。
   默认 `game/blackAI`、`game/whiteAI` 又都是 false（双人类）。
2. 之后点「启动」，只有 `MainWindow::onEngineConnected` 响应：调
   `analyzePosition()` 无限流式分析，没有任何信号通知 `GameController` 重算回合。

领域层本身已支持人机对弈（`tst_gamecontroller.cpp` 全绿），坏的是接线。

## 决策（与用户确认）

| 决策点 | 结论 |
|---|---|
| 触发方式 | 引擎就绪自动续弈：`GameController` 订阅 `EngineProcess::connected`，轮到 AI 就立刻 genmove |
| 分析策略 | 对弈模式专注落子：不跑持续 kata-analyze，genmove 独占算力；引擎落子后不恢复分析 |
| 开局默认 | Quick Start 19/13/9 = 人黑 vs 引擎白，未启动引擎时自动启动；双人/AI vs AI 仍在「新局」对话框 |
| 强度控制 | 暂不加 UI，沿用 engine/moveSeconds=5s（time_settings 0 5 1） |

## 设计

### 1. GameController 订阅引擎生命周期

`attachEngine()` 增加第四条连接：`EngineProcess::connected` → 新私有槽：

    void GameController::onEngineReady() {
        if (!m_game || m_phase == Phase::GameOver || m_movePending) return;
        advanceTurn();     // 幂等：只有轮到 AI 且引擎在跑才切 EngineThinking
    }

覆盖三种迟到情况：
- AI 执黑、引擎后启动 → connected 时 nextToPlay 是黑 → 补上欠的那手
- 人执黑、引擎白 → connected 时是人类回合 → 保持 HumanTurn
- 双人类 → 永不触发

`m_movePending` 与 `Phase::EngineThinking` 双重把门，引擎重启的重复
connected 不会连发 genmove。`advanceTurn()` / `requestEngineMove()` 本体不动。

### 2. 新谓词 hasAiSide()

    bool GameController::hasAiSide() const;   // setup 中至少一方为 AI

### 3. 分析让位于对弈（MainWindow）

- `onEngineConnected()`：删掉无条件 `analyzePosition()`；只有
  `m_game && !m_controller->hasAiSide()`（非对弈局）才分析。
- `onBoardClicked` 的 review/edit 路径的 analyze 调用加同样守卫。
- `startGame()` 补 `m_boardView->setAnalysisOverlay(nullptr)`，换局清旧 overlay。
- 对弈中由 `requestEngineMove()` 既有的 `stopAnalysis()` 保证 genmove 独占；
  引擎落子后不恢复 kata-analyze。

### 4. 开局默认（AppSettings）

- `gameSetup()` 缺省 `game/whiteAI` 由 false 改 true（black 仍 Human）。
  `tst_appsettings::defaultsWhenEmpty` 同步更新（规格变更测试先行）。
- `MainWindow::newGame(size)`（Quick Start 路径）：若引擎未启动先
  `m_engine->start(m_settings.engineConfig())`；就绪信号随后自动驱动回合。
- 「新局」对话框与 GameSetup 持久化不变。

## TDD 计划（红 → 绿 → 提交）

1. **engineReadyKicksOffEngineTurn**（tst_gamecontroller）：AI 执黑、引擎后启动，
   start 后 `QTRY_COMPARE(phase, EngineThinking)`，fake 回手后回到 HumanTurn 且盘上多黑子。
2. **幂等与不惊扰**（同文件）：connected 两次只落一子；人黑 vs AI 白引擎后启动仍 HumanTurn 无 genmove。
3. **hasAiSide**（同文件）：三种组合断言。
4. **默认配置规格变更**（tst_appsettings）：defaultsWhenEmpty 改 white=AI 见红，
   再改 `AppSettings::gameSetup()` 转绿；round-trip 补 whiteAI=true。
5. **Quick Start 接线**（tests/gui offscreen 探针）：启动 → Quick Start →
   `QTRY_COMPARE(controller phase, EngineThinking)`；onEngineConnected 分支与
   overlay 清理由此覆盖。

顺序即依赖：1 → 2 → 3 → 4 → 5。

## 约束提醒

- Qt 5.11 API 天花板；core/app/gtp 层仅 QtCore；零第三方依赖。
- 测试离线：fake_engine.sh；GUI 探针走 offscreen。
- 引擎崩溃仍只回滚到 HumanTurn（spec §6），不新增对崩溃路径的改动。
