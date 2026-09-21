# YiGo（弈境）架构设计文档

日期：2026-09-21
状态：待审阅
范围：整体架构 + 全功能里程碑规划

## 1. 需求背景与约束

YiGo 是跨平台围棋 AI 分析复盘软件，对标 Katrain，纯 Qt C++ 实现。经讨论确认的约束：

| 维度 | 决策 |
|---|---|
| 功能范围 | 完整版规划：人机对弈、复盘分析、胜率曲线、变着研究，分里程碑实施 |
| 平台 | Qt 6.2+ 基线，UOS/Deepin 为开发验证平台，Windows/macOS 兼容适配 |
| 引擎分发 | 用户自备 KataGo / Leela Zero，软件只实现 GTP 客户端 |
| 技术边界 | 纯 Qt 零第三方依赖，SGF/GTP/图表全部自研 |
| 核心场景 | 人机对弈为主（支持让子、贴目），双人本地对弈顺带支持，复盘为第二大场景 |
| 质量属性 | 快速启动、低内存、HiDPI 清晰渲染、引擎崩溃不影响主程序 |

## 2. 架构总览

选定**方案 A：单体分层 + 信号槽事件总线**。备选方案 B（Model-View 深度绑定，因规则引擎难独立测试、棋盘画布渲染不匹配而放弃）、方案 C（引擎嵌入进程内，因违背零依赖与用户自备引擎约束而放弃）。

```
┌─────────────────────────────────────────┐
│  UI 层 (Widgets)                         │
│  MainWindow / BoardView / ChartWidget…  │
├─────────────────────────────────────────┤
│  应用服务层 (Controllers)                │
│  GameController / ReviewController      │
├─────────────────────────────────────────┤
│  领域层 (纯 C++，无 Qt UI 依赖，可单测)   │
│  Board/Rules/Game  GameTree/SGF/Curve   │
├─────────────────────────────────────────┤
│  基础设施层                              │
│  GtpEngineProcess (QProcess)  Settings  │
└─────────────────────────────────────────┘
```

原则：
- 引擎在独立进程（QProcess），异步通信，UI 永不阻塞；引擎崩溃仅发信号，主程序存活
- 领域层只依赖 QtCore，单元测试不启动 GUI
- UI 之间不直接互调，全部经控制器转接，避免面板间网状依赖

### 目录结构

```
yigo/
├── CMakeLists.txt              # 顶层 CMake（Qt6, C++17）
├── src/
│   ├── main.cpp
│   ├── core/                   # 领域层：纯 C++，不依赖 QtGui
│   │   ├── Board.{h,cpp}       # 棋盘状态（19/13/9路），落子/提子/禁着点
│   │   ├── Rules.{h}           # 中国/日本规则，数目、劫
│   │   ├── Game.{h,cpp}        # 对局：走子历史、当前节点、悔棋
│   │   ├── GameTree.{h,cpp}    # SGF 变着树（多分支）
│   │   ├── SgfParser.{h,cpp}   # SGF 读写（FF[4]）
│   │   ├── WinrateCurve.{h,cpp}# 胜率曲线数据模型
│   │   └── AnalysisData.{h}    # 每手的候选点/访问量/胜率
│   ├── gtp/                    # 引擎层
│   │   ├── GtpClient.{h,cpp}   # GTP 协议编解码（QIODevice 抽象）
│   │   └── EngineProcess.{h,cpp} # QProcess 管理，异步请求/响应配对
│   ├── ui/                     # Widgets 层
│   │   ├── MainWindow.{h,cpp}
│   │   ├── BoardView.{h,cpp}   # 自绘棋盘（QPainter，HiDPI）
│   │   ├── AnalysisPanel.{h,cpp} # 胜率/候选点列表
│   │   ├── EnginePanel.{h,cpp} # 引擎配置（路径/权重/参数）
│   │   └── ChartWinrate.{h,cpp}# 纯 QPainter 胜率曲线图
│   └── app/                    # 应用服务层（控制器）
│       ├── GameController.{h,cpp}    # 对弈流程、人机轮转
│       └── ReviewController.{h,cpp}  # 复盘：分析调度、图表联动
└── tests/                      # QtTest 单元测试（core/ 全覆盖）
```

## 3. 领域层设计（core/）

### Board — 棋盘状态

```cpp
enum class Stone { Empty, Black, White };
class Board {
public:
    static constexpr int MaxSize = 25;
    explicit Board(int size = 19);
    Stone stoneAt(int x, int y) const;
    // 落子并返回提子列表；非法着点（占据/自杀/劫）返回失败
    bool placeStone(int x, int y, Stone color, QVector<QPoint>* captured = nullptr);
    QVector<QPoint> legalMoves(Stone color) const;
    bool isSuicide(int x, int y, Stone color) const;
private:
    QVector<Stone> m_grid;      // 一维存储，手动索引
    int m_size;
    QSet<quint64> m_zobristHistory;  // 劫检测：历史局面哈希
};
```

- 内部一维 `QVector<Stone>`，性能优先，不用容器嵌套
- 提子：落子后对相邻对方棋块 flood-fill 数气，气为 0 整块提取
- 打劫：Zobrist 哈希；简单劫（禁立即重现上一局面）默认，禁全同（超级劫）作为规则选项

### Rules — 规则引擎

```cpp
struct RulesConfig {
    enum RuleSet { Chinese, Japanese } ruleSet;
    double komi;              // 默认 7.5 (中国) / 6.5 (日本)
    int handicap;             // 让子数
    bool superko;             // 是否禁全同
};
class Rules {
public:
    double scoreBoard(const Board&, const RulesConfig&) const;
    QPair<int,int> territory(const Board&) const;  // 黑/白地域
};
```

- 中国规则数子+贴目；日本规则数目+提子计分
- 终局死活判定不自动做，依赖引擎 `final_score`；本地只做空点归属 flood-fill（只接触单色则归属该色）

### Game / GameTree — 对局与变着树

```cpp
struct MoveNode {                 // SGF 树节点
    int moveNumber;               // -1 = 根节点
    QPoint pos;                   // (-1,-1) = pass
    Stone color;
    QString comment;
    AnalysisData analysis;        // 该节点引擎分析结果
    QVector<MoveNode*> children;  // children[0] 为主变着
    MoveNode* parent;
};
class Game {
    Board m_board;
    GameTree m_tree;
    RulesConfig m_rules;
public:
    bool play(QPoint pos);        // 落子，新建节点挂当前分支
    bool undo(); bool redo();
    void goTo(MoveNode*);         // 复盘核心操作：重放到目标节点
};
```

- 跳转性能：每 50 手缓存一个棋盘快照，跳转 = 最近快照 + 增量重放
- 所有权：GameTree 独占所有 MoveNode（整树析构释放），Game/GameTree 内部迁移节点指针，对外可暴露裸指针只读访问
- AnalysisData：`winrate`、`scoreLead`、`visits`、`QVector<MoveCandidate>`（top-N 候选点）

### SgfParser — SGF FF[4]

- 手写递归下降解析器：`(` 序 `;` 节点 属性`)`，支持多分支变着、AB/AW 摆子、C 注释、KM/PB/PW/DT 等元数据
- 编码：CA 属性声明编码；默认 ISO-8859-1，对 UTF-8/GB18030 做 BOM/启发式检测
- 压缩点值（`B[ee]`=E5）与坐标转换集中在 `SgfCoord` 工具类
- 宽松解析：未知属性跳过不报错

## 4. GTP 引擎层设计（gtp/）

### GtpClient — 协议编解码

```cpp
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

- 行协议：`id command args\n` → `=id result\n\n` 或 `?id error\n\n`
- 空行（`\n\n`）为响应结束标志；按 id 配对，协议上允许乱序
- 只做文本解析，语义解释交调用方

### EngineProcess — 进程与生命周期

```cpp
class EngineProcess : public QObject {
    Q_OBJECT
public:
    bool start(const EngineConfig& cfg);
    void stop();                            // 优雅 quit → 超时 kill
    bool isAnalyzing() const;
    void query(quint64 id, const QString& command);
    void startAnalysis(const AnalysisQuery& q);   // kata-analyze 流式
    void stopAnalysis();
signals:
    void connected(quint64 id, const QString& engineName, const QStringList& supportedCommands);
    void queryFinished(quint64 id, const QString& result);
    void analysisUpdate(const AnalysisData& data);
    void crashed(int exitCode);
    void errorOccurred(const QString& msg);
};
```

- 命令队列串行：GTP 引擎同一时刻只处理一个分析流；状态机 `Idle → Analyzing → Stopping → Idle`
- 切换分析局面：先 `stop`，等回包后再发新 `kata-analyze`（KataGo 中途打断行为不保证）
- 复盘跳转去抖：合并 200ms 内跳转，只分析最终节点
- kata-analyze 输出 `info move D4 visits 120 winrate 0.5123 scoreLead 1.2 ...` 周期帧，节流 ~10Hz 更新 UI
- 崩溃处理：`QProcess::Crashed` → `crashed` 信号 → UI 提示，可选自动重启并重放当前局面

### 配置结构

```cpp
struct EngineConfig {
    QString executable;
    QStringList baseArgs;        // -model xxx.bin.gz -config xxx.cfg
    QString gtpCommand;          // KataGo: "kata-analyze interval 50"; LZ: "lz-analyze"
    enum EngineType { KataGo, LeelaZero } type;
};
struct AnalysisQuery {
    Stone color;
    int maxVisits;
    QVector<QPoint> avoidMoves;  // 未来扩展
};
```

- LeelaZero 兼容：`lz-analyze` 输出格式不同（winrate 为百分比、无 scoreLead），按 `EngineType` 分派解析器，对上层统一输出 `AnalysisData`
- 视角统一：`AnalysisData.winrate` 一律存黑方视角概率，UI 按执方换算显示

## 5. UI 层设计（ui/ + app/）

### BoardView — 棋盘自绘

普通 QWidget + QPainter（不用 QGraphicsView：棋盘是单一画布，场景管理是多余开销）。

```cpp
class BoardView : public QWidget {
    Q_OBJECT
public:
    void setGame(Game* game);
    void setAnalysisOverlay(const AnalysisData* data);
    void setViewMode(ViewMode mode);          // Play / Review
signals:
    void boardClicked(QPoint pos);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
};
```

- 绘制层次：木纹底色 → 网格线 → 星位 → 坐标（A-T 跳 I）→ 棋子（径向渐变高光）→ 最后一手标记 → 分析覆盖层
- 分析覆盖层：候选点半透明棋子 + 胜率百分比 + 访问量；胜率→颜色用 QColor HSL 插值
- HiDPI：所有尺寸基于 `devicePixelRatioF()`，`qreal` 抗锯齿绘制
- 交互：View 无业务状态，点击发信号 → 控制器判定 → Game 更新 → `boardChanged` → 重绘

### MainWindow 布局

```
┌────────────────────────────────────────────────┐
│ 菜单栏：文件/对局/引擎/视图/帮助                  │
├──────────────────────────────┬─────────────────┤
│                              │ QDockWidget:     │
│        BoardView             │ AnalysisPanel    │
│        (中央)                 │ ChartWinrate     │
│                              │ GameInfoPanel    │
├──────────────────────────────┴─────────────────┤
│ 状态栏：引擎状态 | 胜率 | 手数 | 计时             │
└────────────────────────────────────────────────┘
```

- 快捷键：方向键前后跳转、Home/End 首末、P 虚手、Ctrl+Z 悔棋

### ChartWinrate — 胜率曲线

- 纯 QPainter：X 轴手数、Y 轴胜率，胜率差着色（红=黑亏、蓝=黑涨）
- 点击曲线跳转对应手数（与 ReviewController 联动）；当前手数竖线指示

### GameController — 对弈模式

```cpp
enum class Phase { Idle, HumanTurn, EngineThinking, GameOver };
// 职责：黑白执方配置(人/AI)、handicap/komi、genmove 调度、
//       连续两次虚手终局、终局 final_score
```

### ReviewController — 复盘模式

- 进入复盘：加载 SGF → 后台顺序逐手批量分析，每完成一手更新曲线/面板，可暂停/跳过
- 跳转节点：暂停队列 → 分析当前节点 → 恢复队列
- 失着检测：与前一手胜率差 > 阈值（默认 5%）标橙/红点

### 复盘典型数据流

```
用户点击胜率曲线第 k 手
  → ReviewController::goTo(k) → Game::goTo(node) → boardChanged → BoardView 重绘
  → EngineProcess::analyze(node) → analysisUpdate → AnalysisPanel + BoardView 覆盖层
```

### 设置持久化

- QSettings：引擎路径、权重参数、执方偏好、分析 visits、主题
- EnginePanel 引擎探测：启动 → `list_commands` → 显示引擎名/版本

## 6. 错误处理

| 场景 | 处理 |
|---|---|
| 引擎启动失败（路径错/依赖缺） | EnginePanel 红色状态 + 错误详情弹窗；不阻塞手动打谱 |
| 引擎运行中崩溃 | 状态栏警示 + 弹窗"重启引擎/继续离线打谱"；重启后重放当前局面 |
| GTP 响应超时（默认 30s） | 请求作废、状态复位 Idle，允许重试 |
| SGF 解析失败/非围棋 SGF | 指出字节偏移+上下文；未知属性宽松跳过 |
| 非法落子 | 状态栏抖动提示 + 短暂高亮非法点，不出 modal |
| 快照缓存损坏 | 校验失败丢弃缓存全量重放（正确性兜底） |

## 7. 测试策略（QtTest）

- core/ 全覆盖：
  - Board：提子、自杀、打劫、超级劫、让子摆子
  - GameTree：分支插入/删除/跳转、快照一致性（随机操作序列 vs 全量重放对比）
  - SgfParser：往返测试（parse→save→parse 语义等价）、多分支、特殊编码、畸形输入不崩溃
  - Rules：终局数目（构造已知局面断言分数）
- gtp/：GtpClient 用 QBuffer 模拟引擎回包（乱序、错误包、粘包）；解析器用录制的 kata-analyze/lz-analyze 真实输出做 golden 测试
- ui/：不写自动化 UI 测试；BoardView 坐标换算抽纯函数测试
- CI：`cmake --build build && ctest --test-dir build`

## 8. 里程碑

每步可运行、可发布：

| # | 内容 | 交付物 |
|---|---|---|
| M1 | core 领域层 + 单测 | Board/Rules/GameTree/SgfParser + ctest 全绿 + CLI 冒烟程序 |
| M2 | 最小 UI | 主窗口 + BoardView + 落子/悔棋 + SGF 打开保存 |
| M3 | 引擎对接 | GtpClient/EngineProcess + EnginePanel + 实时分析覆盖层 |
| M4 | 人机对弈 | GameController 状态机 + genmove + 让子/贴目/虚手/终局数子 |
| M5 | 复盘分析 | ReviewController + 批量分析 + 胜率曲线 + 失着标记 |
| M6 | 打磨 | 设置持久化、HiDPI 细节、UOS 打包（debian） |

## 9. 范围外（明确不做）

- 引擎内嵌（编译 KataGo 为库）
- 自动死活/终局死活判定（依赖引擎 final_score）
- 内置引擎下载器（用户自备）
- 网络对弈、棋谱云同步
