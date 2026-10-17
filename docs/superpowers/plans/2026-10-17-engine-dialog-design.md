# 引擎配置独立窗口设计（2026-10-17）

## 需求

配置界面（EnginePanel：引擎类型/路径/参数/强度/状态/启动停止）从主窗口右侧
停靠面板改为独立窗口。决策：文件菜单「引擎设置…」(Ctrl+E) 入口 + 非模态单例。

## 设计

### 1. EngineDialog（薄壳，src/ui/）

    class EngineDialog : public QDialog {
    public:
        explicit EngineDialog(QWidget* parent = nullptr);
        EnginePanel* panel() const;
    protected:
        void closeEvent(QCloseEvent*) override;   // hide, never delete
        void changeEvent(QEvent*) override;       // LanguageChange
    };

内部即现有 EnginePanel；单例由 MainWindow 持有，反复开关不重建，
已连接/运行状态不丢。再次打开 → show + raise + activateWindow。

### 2. MainWindow 接线

- 删除 m_engineDock；m_enginePanel = m_engineDialog->panel()
- 所有既有面板 connect / setStatus / setConfig 调用一行不改
- 状态栏新增常驻 m_engineLabel：onEngineStart/Connected/Crashed/Error/Stop
  更新，纳入 retranslateUi()
- closeEvent 的 setEngineConfig 持久化路径不变

### 3. 不改的部分

GameController / EngineProcess / AppSettings 不碰；纯 UI 装配层重构。

## TDD 任务

1. 配置窗口单例与入口（play_probe：1 次打开 = 1 个 QDialog 且承载面板；
   2 次打开同地址；close 后面板存活；重开可见）
2. 移除引擎 dock + 状态栏指示（probe：无 dock 承载 EnginePanel；
   连接后状态栏 QLabel 含引擎名）
3. i18n：lupdate + zh_CN 补译 + 真机验证
