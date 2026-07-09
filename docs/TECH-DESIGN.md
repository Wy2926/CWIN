# CWIN 技术方案 v1.0

> 状态：已评审锁定 ｜ 日期：2026-07-09 ｜ 配套：docs/PRD.md

## 1. 技术栈与约束

- 语言：C++17/20，Win32 + COM 原生 API
- 渲染：DirectComposition + Direct2D + DirectWrite（硬件加速）
- 毛玻璃：**运行时探测** —— 22H2+ 用 DirectComposition backdrop brush；21H2 回落 DWM `SetWindowCompositionAttribute`（Accent blur behind）
- 构建：CMake + MSVC（Ninja generator），CI 友好
- 依赖：坚持零第三方运行时；JSON 用 header-only 极简库；不引入重型框架
- 内存目标：Host 常驻 < 30MB
- 兼容：Win11 21H2 老任务栏 + 22H2/23H2 新任务栏（双适配层）

## 2. 进程模型

```
CWIN.Host.exe（主进程，常驻，<30MB）
 ├─ Injector          注入 CWIN.Shell.dll 到 explorer.exe
 ├─ Renderer          DirectComposition + D2D/DWrite，Host 内置顶分层窗口
 ├─ CapsuleScheduler  数据源刷新、轮换策略
 ├─ Providers         硬件(PDH/D3DKMT)、天气(Open-Meteo/WinHTTP)、时钟、网速
 └─ IpcServer         命名管道 + JSON，服务设置页与未来插件

CWIN.Shell.dll（注入 explorer.exe）
 └─ Hook Shell_TrayWnd：测量并预留任务栏空间，回传布局坐标给 Host
    —— 不在 explorer 内做像素渲染（低风险，崩溃不拖垮 explorer）

CWIN.Settings（WebView2 宿主，按需启动，用完释放）
 └─ HTML/CSS/JS 设置页 ←IPC(命名管道/JSON)→ Host
```

### 嵌入方案（已锁定：方案 2）
- Shell.dll 只负责：挂钩任务栏、挤占/预留空间、上报坐标；
- 实际像素由 **Host 进程的 always-on-top 无边框分层窗口** 精确贴合任务栏绘制；
- 渲染进程隔离：胶囊崩溃不影响 explorer，Host 可独立重启；
- 视觉上与真注入一致，工程上低风险、可降级、易调试。

### 降级策略
- 注入失败 / Explorer 重启 / Win 更新破坏挂钩点 → 自动检测；
- 降级为**伴生窗口模式**（不预留空间，浮贴任务栏上层），用户无感知，仅融合度下降。

## 3. 核心模块

### 3.1 任务栏适配层 TaskbarAdapter（抽象接口）
- `ITaskbarLayout`：查询任务栏矩形、对齐方式（居中/左）、DPI、是否自动隐藏
- 两个实现：`Legacy21H2Adapter`、`Modern22H2Adapter`（运行时按 build 号选择）
- 为未来 Win12 变动预留扩展点

### 3.2 渲染引擎 Renderer
- DirectComposition 视觉树：每个胶囊 = 一个 IDCompositionVisual + D2D surface
- 模板系统（4 种）：纯文本 / 图标+文本 / 迷你图表(sparkline) / 进度环
- backdrop：探测系统能力选 DComp backdrop 或 DWM Accent blur

### 3.3 胶囊调度器 CapsuleScheduler
- 胶囊模型 = 数据源 + 渲染模板 + 交互行为（也是未来插件 SDK 抽象）
- 定时轮换（可配间隔）；"跟随前台应用"规则引擎排期 v2

### 3.4 数据源 Providers
| Provider | 源 | 备注 |
|---|---|---|
| CPU/内存 | PDH | 轻量，无需管理员 |
| GPU | **D3DKMT** | 轻，无需管理员 |
| 天气 | Open-Meteo / WinHTTP | 免费无 key |
| 时钟/日程 | Win32 时间 API | |
| 网速 | GetIfTable2 / IP Helper | |
- Provider 层可插拔，备用数据源可切换

### 3.5 大卡片面板
- 点击胶囊 → Host 分层窗口弹出毛玻璃大卡片（Win11 控制中心风格）
- MVP 只读展示

### 3.6 设置页（WebView2）
- 按需启动，关闭释放内存；通过命名管道 + JSON 与 Host 通信
- 管理胶囊、排序、轮换间隔、主题、自启、更新检查

## 4. 配置与数据
- 本地 JSON 配置（`%APPDATA%\CWIN\config.json`），便于手改/备份/未来云同步

## 5. 构建与工程
- CMake + MSVC + Ninja；目标产物：`CWIN.Host.exe`、`CWIN.Shell.dll`、设置页资源
- 目录建议：
```
/src/host      主进程
/src/shell     注入 DLL
/src/render     渲染引擎
/src/providers  数据源
/src/ipc        IPC 协议
/settings       WebView2 前端
/cmake, /third_party
```

## 6. 未来（v3 插件 SDK）
- 进程外插件模型：插件独立进程，崩溃隔离
- IPC 协议（JSON）稳定化，开放"数据源 / 模板 / 卡片"三层扩展点
- 任何语言可写插件
