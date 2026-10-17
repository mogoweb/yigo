# 引擎强度档位设计（2026-10-17）

## 需求

三档引擎强度：快(2s) / 标准(5s) / 强(15s)，落到 EngineConfig::moveSeconds，
引擎面板下拉选择，运行中切换即时生效（time_settings）。

决策：EnginePanel + 即时生效；持久化真相仍是 engine/moveSeconds（已有 round-trip）。

## 设计

### 1. 档位映射（EngineConfig.h 纯函数）

    enum StrengthTier { TierFast = 0, TierStandard = 1, TierStrong = 2 };
    static int secondsForTier(int tier);   // 2 / 5 / 15，非法回退 5
    static int tierForSeconds(int secs);   // 精确命中 2/5/15，否则 Standard

UI 不私藏数字；旧配置值（如 10）显示标准档但不静默改写 moveSeconds。

### 2. EngineProcess::setMoveBudget(int secs)

运行中发送 time_settings 0 N 1 并更新 m_cfg.moveSeconds；
secs<=0 或未运行为 no-op。当前 genmove 不中断，下一手生效。

### 3. EnginePanel + MainWindow

- 强度下拉 currentIndex == tier；config()/setConfig() 经映射函数换算
- 新信号 strengthChanged(int seconds)，值不变不触发
- MainWindow::onStrengthChanged: setMoveBudget + setEngineConfig 持久化
- retranslateUi 补三档文案（tr）

## TDD 任务

1. 映射纯函数（tst_appsettings.cpp）——红 = 编译错误
2. setMoveBudget（tst_engineprocess.cpp，YIGO_FAKE_LOG 断言 time_settings 到达）
3. UI 接线（play_probe：切档 config 值 / 运行中切档日志 / 重跑持久化恢复）

## 边界情况

- 切档时 genmove 挂起：time_settings 入队按序应答，当前手不受影响
- time_settings 是标准 GTP，KataGo/LZ 通用
- QComboBox currentIndexChanged 值不变不触发，无需去重
- Qt 5.11 API；moveSeconds=0（引擎自管）语义不变
