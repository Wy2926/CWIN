# CWIN

Windows 11 任务栏增强软件：信息胶囊 + 毛玻璃大卡片面板，对标 macOS 菜单栏/灵动岛体验，Win11 Fluent 风格（大卡片、毛玻璃、圆角）。低内存占用（常驻目标 < 30MB）。

- 产品需求：[docs/PRD.md](docs/PRD.md)
- 技术方案：[docs/TECH-DESIGN.md](docs/TECH-DESIGN.md)

## 构建

要求：Windows 11、Visual Studio 2022 Build Tools（VC++ 工作负载）、CMake ≥ 3.21、Ninja。

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物：

- `build/src/host/CWIN.Host.exe` — 主进程（渲染、调度、IPC）
- `build/src/shell/CWIN.Shell.dll` — 注入 explorer.exe 的测量/预留模块

## 目录结构

```
src/common      配置与公共类型
src/render      DirectComposition + Direct2D 渲染引擎、毛玻璃 backdrop
src/providers   数据源（硬件/天气/时钟/网速）
src/ipc         命名管道 JSON IPC（设置页 / 未来插件 SDK）
src/shell       注入 DLL 与任务栏双代适配层
src/host        主进程入口与胶囊窗口
docs            PRD 与技术方案
```
