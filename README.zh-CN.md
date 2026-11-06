[English](README.md) | **简体中文** | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md)

# gakumas-VRify

[源码构建](BUILDING.md) · [插件安装与接口](docs/GAKULOADER_PLUGIN.md) · [镜头共存记录](docs/MOTION_CAMERA_COEXIST_20261027.md) · [上游来源](UPSTREAM.md)

## 这是什么

gakumas-VRify 为《学园偶像大师》（PC DMM 版）添加 VR 支持，基于 [gkms-localify-dmm](https://git.chinosk6.cn/chinosk/gkms-localify-dmm) 开发。它通过 OpenXR 将游戏画面输出至头显，并提供头部跟踪和手柄操作。

本分支包含 **GakuLoader 插件集成版 `GakuVRify.dll`**，已实现 Localify camera/render 与 Motion begin-camera 共享桥接。源码默认构建插件模式，独立代理版（Standalone `version.dll`）保留为显式构建选项。桥接实现、构建检查与游戏中的兼容性验收各有范围，见下文说明。

## 准备工作与依赖

### 使用前注意事项
- 《学园偶像大师》原本并非面向 VR 环境开发，在某些过渡界面可能出现模型暂时缺失或画面闪烁。
- **请注意晕动症与闪烁风险**。初次使用建议采取坐姿，若感到任何眩晕或眼部不适，请立即停止使用并休息。
- 本项目为玩家自制开源工具，与游戏官方版权方及上游项目无隶属关系。使用本地游戏模组前请参阅游戏官方[用户协议（日文）](https://legal.bandainamcoent.co.jp/terms/nejp)，并自行承担账号安全风险。

### 硬件与软件环境
- 操作系统：Windows 10 / 11 64 位（x64）
- VR 硬件：支持 OpenXR 标准的 VR 头显（如 Meta Quest 3 / Quest 2、PICO 4 或 PC VR 头显）
- 串流软件：Virtual Desktop、Meta Quest Link 或 SteamVR，并确保已将默认 OpenXR 运行时正确设置为对应软件
- 宿主程序：配套的 GakuLoader，按其安装包说明单独安装

### 文件放置路径

退出游戏后，将插件与运行库放入游戏根目录中：

```text
<游戏根目录>/
├── gakumas.exe
├── openxr_loader.dll                        # OpenXR 核心运行库
├── version.dll                              # GakuLoader 核心
└── GakuLoader/
    ├── Configs/gaku.vrify/                  # VR 配置文件
    ├── PluginData/gaku.vrify/               # 布局状态数据
    ├── Logs/gaku.vrify/                     # VR 诊断日志
    └── plugins/
        ├── GakuVRify.dll                    # VR 插件核心
        └── GakuMotion.dll                   # 可选动作播放插件
```

> **注意**：
> 1. 上游官方 Releases 发布的通常是独立代理版（以 `version.dll` 形式替换）。若搭配 GakuLoader 一同使用，请使用本分支编译的 `GakuVRify.dll` 插件版，切勿在根目录混入其它独立版的 `version.dll`。
> 2. 若要升级或迁移已有配置，可查阅 [配置迁移指南](docs/CONFIG_MIGRATION_V2_20261102.md)。

全新安装请使用相互配套的完整安装包，详见[全新安装指南](docs/FRESH_INSTALL_GAKULOADER.md)。已有专用 VR 配置可以继续原位使用，变更前请备份。通用 `package-plugin.ps1`、`install-plugin.ps1` 和原生迁移工具仍以旧 `gakumas-local` 根为目标，不能直接用于新根的全新安装；通用安装脚本也无法确认外部 provider 是否配套，仍会拒绝完整 Unity VR 与 Localify/Motion 混装。

## 使用方法与快捷键

### 启动流程
1. 打开电脑上的 VR 串流平台（如 Virtual Desktop Streamer 或 Meta Quest Link），戴上头显并连接电脑。
2. 启动《学园偶像大师》PC 版。启用 VR 且运行时、设备和 Unity 初始化成功后，插件才会输出双目画面。
3. 使用配套组件时，可进入“偶像摄影”页面尝试 GakuMotion 动作播放；历史配套构建曾完成一次动作播放，不代表新 DLL 或所有场景已验收。

### Quest 手柄操作指南

![Quest 手柄操作指南（简体中文）](docs/controller-guide/exports/gakumas-vrify-quest-guide-zh-CN.png)

- **Grip 长按**：打开或关闭 VR 设置菜单；进入面板调整模式后，Grip 用于抓取和缩放面板，不再切换菜单。
- **左摇杆按住约 0.3 秒**：重置当前相机模式。
- **左 X / Y 键**：分别切换相机模式和跟随角色。摇杆、扳机及右手按键的场景行为见上方操作图。

## 常见问题与排查

1. **VR 下 GakuMotion 运镜控制失效**：
   - 这是尚未解决的排程兼容问题。历史配套构建只确认了无 VR 时 Motion 手动移动、旋转镜头恢复；VR 激活后 Motion 镜头控制仍失效，begin-camera 共享桥接尚未解决这项问题。该结果不涵盖导入镜头轨、所有场景或同时组合 Motion 运镜与头部跟踪。详情见 [镜头共存记录](docs/MOTION_CAMERA_COEXIST_20261027.md)。
2. **头显没有画面或提示找不到 OpenXR 运行时**：
   - 检查串流软件设置，确认已将 OpenXR 运行时设置为活动状态（例如在 Virtual Desktop 设置中勾选 Active OpenXR Runtime）。
   - 确认游戏根目录下存在 `openxr_loader.dll`。
   - 检查 `GakuLoader/Logs/gaku.vrify/` 目录中的诊断日志。
3. **画面卡顿或掉帧**：
   - 适当降低串流软件（如 Virtual Desktop）的编码分辨率或刷新率设置。
   - 在游戏内图形设置中调低抗锯齿与阴影等级。

## 源码构建

### 编译环境要求
- Windows 10 / 11 64 位
- PowerShell 7（`pwsh`）、Git、Python 3.11+，且 `venv` 与 `pip` 可用
- Visual Studio 2022+ 的桌面 C++ 工作负载、MSVC 14.40+ x64 工具集与 Windows SDK
- 构建脚本安装锁定的 CMake 4.4.3 和 Conan 2.32.0；项目要求 CMake 4.0+
- 首次准备依赖需要网络；离线构建须事先备齐锁定的上游源码、工具与依赖缓存，详见 [构建指南](BUILDING.md)
- 发布源码包含公共 GakuLoader SDK；开发仓库默认使用相邻的 GakuLoader SDK，无需额外依赖私有宿主实现。

### 编译命令
在 `GakuVRify` 根目录下使用 PowerShell 7：

```powershell
# 编译 GakuLoader 插件版
pwsh -NoProfile -File build.ps1 -Mode Plugin -Configuration Release
```

产物为 `build/bin/plugin/x64/Release/GakuVRify.dll`，原生配置迁移工具为 `build/tools/Release/vr-config-migration.exe`。通用插件打包脚本仍输出旧根布局；新根安装请使用[全新安装指南](docs/FRESH_INSTALL_GAKULOADER.md)所述的配套完整安装包。编译成功不代表游戏或头显验收通过；DLL 不支持热卸载。

### 架构与许可
- [插件指南与接口规范](docs/GAKULOADER_PLUGIN.md)
- [相机共享报告](docs/CAMERA_COEXIST_20261027.md)
- 项目遵循 [GPL-3.0 许可证](LICENSE)。GakuLoader SDK 遵循 MIT 许可证，第三方组件遵循各自许可，详见 [第三方通知](THIRD_PARTY_NOTICES.md)。
