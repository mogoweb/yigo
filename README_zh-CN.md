# 弈境 (YiGo)

[English](README.md) | 简体中文

## 项目简介

**弈境（YiGo）** 是一款基于纯 Qt C++ 开发的跨平台围棋 AI 对弈与复盘工具，功能对标 Katrain。全程采用原生 Qt 开发，无 Python 依赖，启动更快、占用更低、跨平台体验更统一。面向围棋爱好者、复盘学习者与专业棋手，提供稳定、轻量、高性能的 AI 分析与棋谱管理能力。

**当前状态（v0.5.0）**：M1–M6 里程碑全部完成——人机对弈、实时分析覆盖层、批量复盘与胜率曲线/失着检测、SGF 往返、设置持久化、UOS deb 安装包。

## 核心特性

- **纯 Qt 原生开发**：C++17 / Qt 5.11 构建，轻量、无脚本依赖
- **人机对弈**：棋盘大小（9/13/19 路）、贴目、让子、执方可选；引擎走子走 GTP `genmove`；双虚手终局本地数子
- **标准 AI 引擎对接**：原生 GTP 协议支持 KataGo / Leela Zero，落子候选点覆盖层 + 状态栏实时胜率
- **专业复盘**：一键批量分析主线，胜率曲线分段着色（蓝=黑涨 / 红=黑亏）、失着标记（默认 5% 阈值）、点击曲线跳转任意手
- **完整 SGF 支持**：打开、保存、变着、让子摆子（AB/AW）、压缩点值、多编码（UTF-8 / GB18030 检测）、往返无损
- **引擎崩溃隔离**：引擎运行于独立 QProcess，崩溃不影响主程序——离线打谱与复盘照常继续
- **设置持久化**：引擎路径/参数、上次对局设置、窗口几何，重启自动恢复
- **高 DPI 适配**：`AA_EnableHighDpiScaling`，高分屏清晰渲染
- **让子与贴目**：9/13/19 路标准星位让子摆位，中国/日本规则数子

## 技术栈

- **框架**：Qt 5.11（Core / Widgets / Gui——Linux 使用系统 Qt）
- **语言**：C++17
- **构建系统**：CMake ≥ 3.16 + Ninja
- **AI 协议**：GTP（Go Text Protocol）
- **支持的引擎**：KataGo（`kata-analyze` / `genmove`）、Leela Zero（`lz-analyze`）
- **测试**：QtTest，16 个测试套件，全离线（假 GTP 引擎，无需真实引擎）

## 从源码构建

```bash
# 依赖（UOS V20 / Debian 系）
sudo apt install cmake ninja-build qtbase5-dev g++

# 配置 + 构建 + 测试
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

# 运行
./build/src/ui/yigo
```

### deb 安装包（UOS V20 / deepin）

```bash
cmake --build build --target deb
sudo dpkg -i build/yigo_0.5.0_amd64.deb   # 安装 /usr/bin/yigo + 桌面入口
```

## 引擎使用

弈境不捆绑引擎，请自行安装 [KataGo](https://github.com/lightvector/KataGo) 或 Leela Zero，然后在应用内：

1. **Engine 面板（右侧 Dock）**：选择引擎类型、填可执行文件路径与参数，点 **Start**
2. 人机对弈（**对局 → 新对局…**，选执方/让子/贴目），或打开 SGF 后按 **对局 → 分析整局（Ctrl+R）** 批量复盘
3. KataGo 推荐启动方式：`katago gtp -model <权重>.bin.gz -config gtp.cfg`（路径与参数在 Engine 面板填写；权重模型用户自备）

## 适用场景

- 人机对弈（让子/贴目）、双虚手终局数子
- 对局复盘：批量分析、胜率曲线、失着检测（默认 5% 阈值）
- 棋谱整理、教学演示、变化研究

## 开源协议

本项目基于开源协议开放，欢迎 Star、Fork、PR 与 Issue。
